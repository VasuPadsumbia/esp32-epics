#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "driver/usb_serial_jtag.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"

#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"

#include "sdkconfig.h"
#include "esp_check.h"

// Protocol / Behaviour constants
#define SOFTWARE_VERSION "2025-12-18"
#define SOFTWARE_ID "ESP32-EPICS Streamline"

#define USB_BAUD              115200
#define BUFFER_LENGTH         40
#define COMMAND_LENGTH        16
#define EOS_TERMINATOR_CHAR   '\n'
#define UNDEFINED             (-1)

#define PWM_MIN_VALUE         0
#define PWM_MAX_VALUE         255

#define PERIOD_DEFAULT_US     20000  // 20ms default period for servo PWM
#define PERIOD_MIN_US         5000   // 5ms minimum period for servo PWM
#define PERIOD_MAX_US         3600000  // 1 hour maximum period for servo PWM

#define MULTIPLIER_DEFAULT    1000
#define MULTIPLIER_MIN        1
#define MULTIPLIER_MAX        1000000

// ADC mapping
// cmd_response protocol expects: ?ai <index>
// ON EPS32-C6 ADC channels are mapped as follows:
// Index 0 -> ADC1_CHANNEL_0 (GPIO1)
// Index 1 -> ADC1_CHANNEL_1 (GPIO2)
// Index 2 -> ADC1_CHANNEL_2 (GPIO3)
// Index 3 -> ADC1_CHANNEL_3 (GPIO4)

typedef struct {
    adc_channel_t channel; // ADC channel
    adc_unit_t unit;      // ADC unit
    gpio_num_t gpio;    // GPIO number
} adc_channel_map_t;

static const adc_channel_map_t adc_channel_map[] = {
    {ADC_CHANNEL_0, ADC_UNIT_1, GPIO_NUM_1},
    {ADC_CHANNEL_1, ADC_UNIT_1, GPIO_NUM_2},
    {ADC_CHANNEL_2, ADC_UNIT_1, GPIO_NUM_3},
    {ADC_CHANNEL_3, ADC_UNIT_1, GPIO_NUM_4},
};

#define NUM_AI ((int)(sizeof(adc_channel_map) / sizeof(adc_channel_map[0])))

// Digital pins : we will accept any GPIO number in a board range,
// but we should avoid invalid / strapping / USB pins as per documentation.
// For ESP32-C6, we will allow GPIO0 to GPIO21, excluding GPIO18 and GPIO19 (USB D+/D-)
#define NUM_DIGITAL_PINS 22
static const gpio_num_t invalid_digital_pins[] = {GPIO_NUM_18, GPIO_NUM_19};
#define NUM_INVALID_DIGITAL_PINS (sizeof(invalid_digital_pins) / sizeof(invalid_digital_pins[0]))
static bool is_valid_digital_pin(gpio_num_t gpio)
{
    if (gpio < GPIO_NUM_0 || gpio >= GPIO_NUM_22) {
        return false;
    }
    for (size_t i = 0; i < NUM_INVALID_DIGITAL_PINS; i++) {
        if (gpio == invalid_digital_pins[i]) {
            ESP_LOGW(SOFTWARE_ID, "GPIO %d is used for USB D+/D-", gpio);
            return false;
        }
    }
    return true;
}

static char inputString[BUFFER_LENGTH + 1]; // +1 for null terminator
static int  strPtr = 0;
static bool stringComplete = false;

static long arg1 = UNDEFINED;
static long arg2 = UNDEFINED;
static char baseCmd[COMMAND_LENGTH + 1]; // +1 for null terminator

static long period_us = PERIOD_DEFAULT_US;
static long multiplier = MULTIPLIER_DEFAULT;

static int      ai_watched[NUM_AI];
static int16_t  ai_sums[NUM_AI];
static float    ai_mean[NUM_AI];
static int64_t  loop_count = 0;
static int64_t  loop_rate = 0;

static int64_t nextUpdate_us = 0;

static SemaphoreHandle_t ai_lock;

// ADC oneshot handle (unit per mapping; simplest: assume all units are same)
static adc_oneshot_unit_handle_t adc_handle = NULL;

// LEDC channel configuration for PWM output bookkeeping : one channel per pin (simple case)
typedef struct {
    bool configured;
    gpio_num_t gpio;
    ledc_channel_t ledc_channel;
} pwm_channel_t;

#define PWM_SLOTS 8
static pwm_channel_t pwm_channels[PWM_SLOTS];

// Helpers
static int freeRamBytes(void){
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_8BIT);
    return info.total_free_bytes;
}

// Write lines to USB SERIAL JTAG
static void uart_write_lines(const char *lines)
{
    if (lines == NULL) {
        return;
    }

    char buf[BUFFER_LENGTH + 2];
    size_t len = strnlen(lines, BUFFER_LENGTH);
    memcpy(buf, lines, len);
    buf[len] = '\n';
    buf[len + 1] = '\0';
    usb_serial_jtag_write_bytes(buf, len + 1, 20 / portTICK_PERIOD_MS);
}

// Reset input buffer and parsing state
static void resetBuffer(void)
{
    inputString[0] = '\0';
    strPtr = 0;
    stringComplete = false;

    baseCmd[0] = '\0';
    arg1 = UNDEFINED;
    arg2 = UNDEFINED;
}

static void finalizeError(const char *prefix, const char *input)
{
    char buf[BUFFER_LENGTH + 64];
    size_t pos = 0;

    if (prefix != NULL) {
        size_t plen = strnlen(prefix, sizeof(buf) - 2);
        memcpy(buf + pos, prefix, plen);
        pos += plen;
    }
    if (input != NULL && pos < sizeof(buf) - 2) {
        size_t ilen = strnlen(input, sizeof(buf) - 2 - pos);
        memcpy(buf + pos, input, ilen);
        pos += ilen;
    }

    buf[pos++] = '\n';
    usb_serial_jtag_write_bytes(buf, pos, 20 / portTICK_PERIOD_MS);
}

// very small tokenizer for command parsing : baseCmd [arg1] [arg2]
static void dissectCommand(const char *source){
  char temp[BUFFER_LENGTH + 1];
  strncpy(temp, source, BUFFER_LENGTH);
  temp[BUFFER_LENGTH] = '\0';

  char *saveptr = NULL;
  char *token = strtok_r(temp, " ", &saveptr);
  if (!token) {
      baseCmd[0] = '\0';
      return;
  }
  strncpy(baseCmd, token, COMMAND_LENGTH);
  baseCmd[COMMAND_LENGTH] = '\0';

  token = strtok_r(NULL, " ", &saveptr);
  if (token) {
      arg1 = strtol(token, NULL, 0);
      token = strtok_r(NULL, " ", &saveptr);
      if (token) {
          arg2 = strtol(token, NULL, 0);
          token = strtok_r(NULL, " ", &saveptr);
          // Ignore any further tokens
          if (token) {
              finalizeError("ERROR_TOO_MANY_ARGUMENTS: ", source);
              resetBuffer();
          }
      }
  }
}

static bool gpio_is_reasonable(gpio_num_t gpio)
{
    if (!is_valid_digital_pin(gpio)) {
        return false;
    }
    return true;
}

// Command handlers
static void cmd_get_num_ai(const char *input){
    char response[64];
    snprintf(response, sizeof(response), "NUM_AI %d", NUM_AI);
    uart_write_lines(response);
}

static void cmd_get_num_bin(const char *input){
    char response[64];
    snprintf(response, sizeof(response), "NUM_BIN %d", NUM_DIGITAL_PINS);
    uart_write_lines(response);
}

static void cmd_get_version(const char *input){
    char response[64];
    snprintf(response, sizeof(response), "VERSION %s", SOFTWARE_VERSION);
    uart_write_lines(response);
}

static void cmd_get_id(const char *input){
    char response[128];
    snprintf(response, sizeof(response), "ID %s", SOFTWARE_ID);
    uart_write_lines(response);
}

static void cmd_get_rate(const char *input){
    char response[64];
    snprintf(response, sizeof(response), "RATE %lld", (long long)loop_rate);
    uart_write_lines(response);
}

static void cmd_set_period(const char *input){
    if (arg1 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        return;
    }
    if (arg1 < PERIOD_MIN_US || arg1 > PERIOD_MAX_US) {
        finalizeError("ERROR_INVALID_ARGUMENT: ", inputString);
        return;
    }
    period_us = arg1;
    uart_write_lines("Ok");
    nextUpdate_us = esp_timer_get_time(); // Reset update timer
}

static void cmd_get_period(const char *input){
    char response[64];
    snprintf(response, sizeof(response), "PERIOD %ld", period_us);
    uart_write_lines(response);
}

static void cmd_get_period_min(const char *input){
    char response[32];
    snprintf(response, sizeof(response), "%d", PERIOD_MIN_US);
    uart_write_lines(response);
}
static void cmd_get_period_max(const char *input){
    char response[32];
    snprintf(response, sizeof(response), "%d", PERIOD_MAX_US);
    uart_write_lines(response);
}

static void cmd_set_multiplier(const char *input){
    if (arg1 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    if (arg1 < MULTIPLIER_MIN || arg1 > MULTIPLIER_MAX) {
        finalizeError("ERROR_MULTIPLIER_RANGE: ", inputString);
        resetBuffer();
        return;
    }
    multiplier = arg1;
    uart_write_lines("Ok");
}

static void cmd_get_multiplier(const char *input){
    char response[64];
    snprintf(response, sizeof(response), "MULTIPLIER %ld", multiplier);
    uart_write_lines(response);
}

static void cmd_get_multiplier_min(const char *input){
    char response[32];
    snprintf(response, sizeof(response), "%d", MULTIPLIER_MIN);
    uart_write_lines(response);
}
static void cmd_get_multiplier_max(const char *input)
{
    char response[32];
    snprintf(response, sizeof(response), "%d", MULTIPLIER_MAX);
    uart_write_lines(response);
}
static void cmd_read_bi(const char *input){
    if (arg1 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    gpio_num_t gpio = (gpio_num_t)arg1;
    if (!gpio_is_reasonable(gpio)) {
        finalizeError("ERROR_BI_PIN_NOT_AVAILABLE: ", inputString);
        resetBuffer();
        return;
    }
    int level = gpio_get_level(gpio);
    char response[64];
    snprintf(response, sizeof(response), "BI %ld %d", (long)arg1, level);
    uart_write_lines(response);
}

static void cmd_write_bo(const char *input){
    if (arg1 == UNDEFINED || arg2 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    gpio_num_t gpio = (gpio_num_t)arg1;
    if (!gpio_is_reasonable(gpio)) {
        finalizeError("ERROR_BO_PIN_NOT_AVAILABLE: ", inputString);
        resetBuffer();
        return;
    }
    if (arg2 != 0 && arg2 != 1) {
        finalizeError("ERROR_INVALID_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }

    // Arduino-like behavior: ensure the pin is configured as an output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t cfg_err = gpio_config(&io_conf);
    if (cfg_err != ESP_OK) {
        finalizeError("ERROR_SETTING_PINMODE: ", inputString);
        resetBuffer();
        return;
    }

    esp_err_t err = gpio_set_level(gpio, (uint32_t)arg2);
    if (err != ESP_OK) {
        finalizeError("ERROR_SETTING_BO_LEVEL: ", inputString);
        resetBuffer();
        return;
    }
    uart_write_lines("Ok");
}

static void cmd_set_pinmode(const char *input){
    if (arg1 == UNDEFINED || arg2 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    gpio_num_t gpio = (gpio_num_t)arg1;
    if (!gpio_is_reasonable(gpio)) {
        finalizeError("ERROR_PIN_NOT_AVAILABLE: ", inputString);
        resetBuffer();
        return;
    }
    if (arg2 != 0 && arg2 != 1) {
        finalizeError("ERROR_INVALID_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (arg2 == 0) {
        io_conf.mode = GPIO_MODE_INPUT;
    } else {
        io_conf.mode = GPIO_MODE_OUTPUT;
    }
    esp_err_t res2 = gpio_config(&io_conf);
    if (res2 != ESP_OK) {
        finalizeError("ERROR_SETTING_PINMODE: ", inputString);
        resetBuffer();
        return;
    }
    uart_write_lines("Ok");
}

// ... PWM : use LEDC channels
static pwm_channel_t* pwm_get_or_alloc(gpio_num_t gpio){
    // Check if already allocated
    for (int i = 0; i < PWM_SLOTS; i++) {
        if (pwm_channels[i].configured && pwm_channels[i].gpio == gpio) {
            return &pwm_channels[i];
        }
    }
    // Allocate new slot
    for (int i = 0; i < PWM_SLOTS; i++) {
        if (!pwm_channels[i].configured) {
            pwm_channels[i].configured = true;
            pwm_channels[i].gpio = gpio;
            pwm_channels[i].ledc_channel = (ledc_channel_t)i;
            return &pwm_channels[i];
        }
    }
    return NULL; // No available slot
}

static void cmd_write_pwm(const char *input){
    if (arg1 == UNDEFINED || arg2 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    gpio_num_t gpio = (gpio_num_t)arg1;
    if (!gpio_is_reasonable(gpio)) {
        finalizeError("ERROR_PWM_PIN_NOT_AVAILABLE: ", inputString);
        resetBuffer();
        return;
    }
    if (arg2 < PWM_MIN_VALUE || arg2 > PWM_MAX_VALUE) {
        finalizeError("ERROR_PWM_VALUE_OUT_OF_RANGE: ", inputString);
        resetBuffer();
        return;
    }
    pwm_channel_t *pwm_chan = pwm_get_or_alloc(gpio);
    if (pwm_chan == NULL) {
        finalizeError("ERROR_NO_PWM_SLOTS_AVAILABLE: ", inputString);
        resetBuffer();
        return;
    }
    // Configure LEDC for this pin if not already done
    static bool ledc_initialized = false;
    if (!ledc_initialized) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_LOW_SPEED_MODE,
            .timer_num        = LEDC_TIMER_0,
            .duty_resolution  = LEDC_TIMER_8_BIT,
            .freq_hz          = 1000000 / period_us, // Frequency in Hz
            .clk_cfg          = LEDC_AUTO_CLK,
        };
        esp_err_t res1 = ledc_timer_config(&ledc_timer);
        if (res1 != ESP_OK) {
            finalizeError("ERROR_CONFIGURING_LEDC_TIMER: ", inputString);
            resetBuffer();
            return;
        }
        ledc_initialized = true;
    }
    // Configure channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = pwm_chan->ledc_channel,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio,
        .duty           = 0, // will set later
        .hpoint         = 0,
    };
    esp_err_t res2 = ledc_channel_config(&ledc_channel);
    if (res2 != ESP_OK) {
        finalizeError("ERROR_CONFIGURING_LEDC_CHANNEL: ", inputString);
        resetBuffer();
        return;
    }
    // Set duty
    uint32_t duty = (uint32_t)arg2; // 0-255
    esp_err_t res3 = ledc_set_duty(LEDC_LOW_SPEED_MODE, pwm_chan->ledc_channel, duty);
    if (res3 != ESP_OK) {
        finalizeError("ERROR_SETTING_PWM_DUTY: ", inputString);
        resetBuffer();
        return;
    }
    esp_err_t res4 = ledc_update_duty(LEDC_LOW_SPEED_MODE, pwm_chan->ledc_channel);
    if (res4 != ESP_OK) {
        finalizeError("ERROR_UPDATING_PWM_DUTY: ", inputString);
        resetBuffer();
        return;
    }
    uart_write_lines("Ok");
}

static void cmd_read_ai(const char *input){
    if (arg1 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    if (NUM_AI <= 0) {
        finalizeError("ERROR_NO_ADC_CHANNELS_AVAILABLE: ", inputString);
        resetBuffer();
        return;
    }
    int ai_index = (int)arg1;
    if (ai_index < 0 || ai_index >= NUM_AI) {
        finalizeError("ERROR_AI_INDEX_OUT_OF_RANGE: ", inputString);
        resetBuffer();
        return;
    }
    int raw;
    esp_err_t res = adc_oneshot_read(adc_handle, adc_channel_map[ai_index].channel, &raw);
    if (res != ESP_OK) {
        finalizeError("ERROR_READING_ADC: ", inputString);
        resetBuffer();
        return;
    }
    char response[64];
    snprintf(response, sizeof(response), "AI %d %d", ai_index, raw);
    uart_write_lines(response);
}

static void cmd_watch_ai(const char *input){
    if (arg1 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    if (NUM_AI <= 0) {
        finalizeError("ERROR_NO_ADC_CHANNELS_AVAILABLE: ", inputString);
        resetBuffer();
        return;
    }
    int watch = (arg2 == 0) ? 0 : 1; // Default to 1 (enable) if arg2 is undefined
    xSemaphoreTake(ai_lock, portMAX_DELAY);
    ai_watched[(int)arg1] = watch;
    ai_sums[(int)arg1] = 0;
    ai_mean[(int)arg1] = 0.0f;
    xSemaphoreGive(ai_lock);
    uart_write_lines("Ok");
}

static void cmd_read_ai_mean(const char *input){
    if (arg1 == UNDEFINED) {
        finalizeError("ERROR_MISSING_ARGUMENT: ", inputString);
        resetBuffer();
        return;
    }
    if (NUM_AI <= 0) {
        finalizeError("ERROR_NO_ADC_CHANNELS_AVAILABLE: ", inputString);
        resetBuffer();
        return;
    }
    xSemaphoreTake(ai_lock, portMAX_DELAY);
    if (!ai_watched[(int)arg1]) {
        xSemaphoreGive(ai_lock);
        finalizeError("ERROR_AI_NOT_WATCHED: ", inputString);
        resetBuffer();
        return;
    }
    float mean_value = ai_mean[(int)arg1] * (float)multiplier;
    xSemaphoreGive(ai_lock);
    char response[64];
    snprintf(response, sizeof(response), "AI_MEAN %d %.2f", (int)arg1, mean_value);
    uart_write_lines(response);
}

// --- Dispatcher ---
static void executeCommandLine(const char *line){
  dissectCommand(line);
  if (baseCmd[0] == '\0') {
      finalizeError("ERROR_INVALID_COMMAND: ", line);
      resetBuffer();
      return;
  }
  if (strcmp(baseCmd, "?ai") == 0) cmd_read_ai(line);
  else if (strcmp(baseCmd, "?#ai") == 0) cmd_get_num_ai(line);
  else if (strcmp(baseCmd, "!ai:watch") == 0) cmd_watch_ai(line);
  else if (strcmp(baseCmd, "?ai:mean") == 0) cmd_read_ai_mean(line);

  else if (strcmp(baseCmd, "!bo") == 0) cmd_write_bo(line);
  else if (strcmp(baseCmd, "!pin") == 0) cmd_set_pinmode(line);
  else if (strcmp(baseCmd, "!pwm") == 0) cmd_write_pwm(line);

  else if (strcmp(baseCmd, "?#bi") == 0) cmd_get_num_bin(line);
  else if (strcmp(baseCmd, "?bi") == 0) cmd_read_bi(line);

  else if (strcmp(baseCmd, "?v") == 0) cmd_get_version(line);
  else if (strcmp(baseCmd, "?id") == 0) cmd_get_id(line);
  else if (strcmp(baseCmd, "?rate") == 0) cmd_get_rate(line);

  else if (strcmp(baseCmd, "!t") == 0) cmd_set_period(line);
  else if (strcmp(baseCmd, "?t") == 0) cmd_get_period(line);
  else if (strcmp(baseCmd, "?t:min") == 0) cmd_get_period_min(line);
  else if (strcmp(baseCmd, "?t:max") == 0) cmd_get_period_max(line);

  else if (strcmp(baseCmd, "!k") == 0) cmd_set_multiplier(line);
  else if (strcmp(baseCmd, "?k") == 0) cmd_get_multiplier(line);
  else if (strcmp(baseCmd, "?k:min") == 0) cmd_get_multiplier_min(line);
  else if (strcmp(baseCmd, "?k:max") == 0) cmd_get_multiplier_max(line);

  else {
      finalizeError("ERROR_UNKNOWN_COMMAND: ", line);
      resetBuffer();
  }
}

// --- Tasks ---
static void uart_cmd_task(void *arg)
{
    ESP_LOGI(SOFTWARE_ID, "UART command task starting");

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUFFER_LENGTH + 1);
    if (data == NULL) {
        ESP_LOGE(SOFTWARE_ID, "no memory for data");
        return;
    }

    resetBuffer();

    while (1) {

        int len = usb_serial_jtag_read_bytes(data, BUFFER_LENGTH, 20 / portTICK_PERIOD_MS);

        // Process incoming data
        for (int i = 0; i < len; i++) {
            char c = (char)data[i];
            if (c == '\r') {
                continue;
            }
            if (c == EOS_TERMINATOR_CHAR) {
                inputString[strPtr] = '\0'; // Null-terminate the string
                stringComplete = true;
                executeCommandLine(inputString);
                resetBuffer();
            } else {
                if (strPtr < BUFFER_LENGTH) {
                    inputString[strPtr++] = c;
                } else {
                    // Buffer overflow
                    finalizeError("ERROR_INPUT_BUFFER_OVERFLOW: ", inputString);
                    resetBuffer();
                }
            }
        }
    }
}

static void ai_sampling_task(void *arg)
{
    ESP_LOGI(SOFTWARE_ID, "AI sampling task starting");
    int64_t last_time = esp_timer_get_time();
    int64_t window_samples = 0;
    int64_t nextUpdate_us = last_time + period_us;
    while (1) {
        int any = 0;
        if (NUM_AI > 0) {
            xSemaphoreTake(ai_lock, portMAX_DELAY);
            for (int i = 0; i < NUM_AI; i++) {
                if (ai_watched[i]) {
                    any = 1;
                    int raw;
                    esp_err_t res = adc_oneshot_read(adc_handle, adc_channel_map[i].channel, &raw);
                    if (res == ESP_OK) {
                        ai_sums[i] += raw;
                    }
                }
            }
            xSemaphoreGive(ai_lock);
        }
        window_samples++;
        int64_t current_time = esp_timer_get_time();
        if (current_time >= nextUpdate_us) {
            // Update loop rate
            int64_t elapsed_us = current_time - last_time;
            if (elapsed_us > 0) {
                loop_rate = (window_samples * 1000000) / elapsed_us;
            } else {
                loop_rate = 0;
            }
            // Update mean values
            if (NUM_AI > 0) {
                xSemaphoreTake(ai_lock, portMAX_DELAY);
                for (int i = 0; i < NUM_AI; i++) {
                    if (ai_watched[i]) {
                        int64_t denom = (window_samples > 0) ? window_samples : 1;
                        ai_mean[i] = (float)ai_sums[i] / (float)denom;
                        ai_sums[i] = 0;
                    }
                }
                xSemaphoreGive(ai_lock);
            }
            last_time = current_time;
            window_samples = 0;
            nextUpdate_us += period_us;
        }

        // Yield time to the rest of the system (and avoid WDT)
        vTaskDelay(any ? 1 : 10);
    }
}

// --- Main application entry point ---
void app_main(void) {
  ai_lock = xSemaphoreCreateMutex();
  ESP_ERROR_CHECK(ai_lock != NULL ? ESP_OK : ESP_FAIL);

    // Configure USB SERIAL JTAG early so logging/output works before tasks start
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
            .rx_buffer_size = 2048,
            .tx_buffer_size = 2048,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    ESP_LOGI(SOFTWARE_ID, "USB_SERIAL_JTAG init done");

  // Initialize ADC oneshot
  if (NUM_AI > 0) {
      adc_oneshot_unit_init_cfg_t init_config = {
          .unit_id = adc_channel_map[0].unit,
          .ulp_mode = ADC_ULP_MODE_DISABLE,
      };
      ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

      for (int i = 0; i < NUM_AI; i++) {
          adc_oneshot_chan_cfg_t chan_config = {
              .bitwidth = ADC_BITWIDTH_DEFAULT,
              .atten = ADC_ATTEN_DB_11,
          };
          ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel_map[i].channel, &chan_config));
          ai_watched[i] = 0;
          ai_sums[i] = 0;
          ai_mean[i] = 0.0f;
      }
  }

  // Print startup message
  char startup_msg[128];
  snprintf(startup_msg, sizeof(startup_msg), "%s starting. Version: %s. Free memory: %d bytes", SOFTWARE_ID, SOFTWARE_VERSION, freeRamBytes());
  uart_write_lines(startup_msg);

  // Create tasks
  BaseType_t res1 = xTaskCreate(uart_cmd_task, "UART_cmd_task", 8192, NULL, 10, NULL);
  ESP_ERROR_CHECK(res1 == pdTRUE ? ESP_OK : ESP_FAIL);
  BaseType_t res2 = xTaskCreate(ai_sampling_task, "AI_sampling_task", 8192, NULL, 10, NULL);
  ESP_ERROR_CHECK(res2 == pdTRUE ? ESP_OK : ESP_FAIL);
}