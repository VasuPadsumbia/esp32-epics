#include "hw_hal.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/dac_oneshot.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include <string.h>
#include <stdint.h>

#include <nvs_flash.h>
#include <nvs.h>

/* Shadow register: one bit per GPIO pin (GPIO 0-39) */
static uint64_t s_output_shadow = 0ULL;
static pin_role_t s_pin_roles[40] = {0};
static uint32_t s_pin_values[40] = {0};

static void save_role(uint8_t pin, pin_role_t role) {
    nvs_handle_t h;
    if (nvs_open("periph", NVS_READWRITE, &h) == ESP_OK) {
        char key[16];
        snprintf(key, sizeof(key), "p%d", pin);
        nvs_set_u8(h, key, (uint8_t)role);
        nvs_commit(h);
        nvs_close(h);
    }
}

void hw_hal_init_all(void) {
    nvs_handle_t h;
    if (nvs_open("periph", NVS_READONLY, &h) == ESP_OK) {
        for (int i = 0; i < 40; i++) {
            char key[16];
            snprintf(key, sizeof(key), "p%d", i);
            uint8_t r = 0;
            if (nvs_get_u8(h, key, &r) == ESP_OK) {
                s_pin_roles[i] = (pin_role_t)r;
                if (r != PIN_ROLE_UNUSED) {
                    hw_hal_pin_cfg(i, (pin_role_t)r);
                }
            }
        }
        nvs_close(h);
    }
}
static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
static adc_oneshot_unit_handle_t s_adc2_handle = NULL;
static dac_oneshot_handle_t s_dac_handles[2] = {NULL, NULL}; /* for pin 25, 26 */

void hw_hal_pin_cfg(uint8_t pin, pin_role_t role) {
    if (pin >= 40) return;
    
    gpio_reset_pin(pin);
    s_pin_roles[pin] = role;
    save_role(pin, role);
    
    switch (role) {
        case PIN_ROLE_GPIO_OUT:
            hw_hal_gpio_init_output(pin);
            break;
        case PIN_ROLE_GPIO_IN:
            hw_hal_gpio_init_input(pin);
            break;
        case PIN_ROLE_ADC:
            hw_hal_adc_init(pin);
            break;
        case PIN_ROLE_DAC:
            hw_hal_dac_init(pin);
            break;
        case PIN_ROLE_PWM:
            hw_hal_pwm_init(pin, pin % 8, 5000); 
            break;
        default:
            break;
    }
}

void hw_hal_gpio_init_output(uint8_t pin) {
    if (pin >= 40) return;
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(pin, 0);
    s_output_shadow &= ~(1ULL << pin);
    s_pin_roles[pin] = PIN_ROLE_GPIO_OUT;
}

void hw_hal_gpio_init_input(uint8_t pin) {
    if (pin >= 40) return;
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_pullup_en(pin);
    s_pin_roles[pin] = PIN_ROLE_GPIO_IN;
}

void hw_hal_gpio_set(uint8_t pin, bool level) {
    gpio_set_level(pin, (uint32_t)level);
    if (level) s_output_shadow |= (1ULL << pin);
    else s_output_shadow &= ~(1ULL << pin);
}

bool hw_hal_gpio_get(uint8_t pin) {
    if (pin >= 40) return false;
    /* For output pins, read from shadow to ensure consistency */
    if (s_pin_roles[pin] == PIN_ROLE_GPIO_OUT) {
        return (s_output_shadow & (1ULL << pin)) != 0;
    }
    return (bool)gpio_get_level((gpio_num_t)pin);
}

pin_role_t hw_hal_get_pin_role(uint8_t pin) {
    if (pin >= 40) return PIN_ROLE_UNUSED;
    return s_pin_roles[pin];
}

uint32_t hw_hal_get_pin_value(uint8_t pin) {
    if (pin >= 40) return 0;
    return s_pin_values[pin];
}

/* --- PWM (LEDC) --- */
void hw_hal_pwm_init(uint8_t pin, uint8_t channel, uint32_t freq_hz) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .freq_hz          = freq_hz,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = (ledc_channel_t)channel,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = pin,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void hw_hal_pwm_set_duty(uint8_t pin, uint32_t duty) {
    if (pin >= 40) return;
    ESP_LOGI("HAL", "PWM Set Pin %d Duty %d", pin, (int)duty);
    s_pin_values[pin] = duty;
    uint8_t channel = pin % 8; /* Simplified mapping */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

void hw_hal_pwm_set_freq(uint8_t pin, uint32_t freq_hz) {
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
}

/* --- ADC --- */
void hw_hal_adc_init(uint8_t pin) {
    adc_channel_t chan;
    adc_unit_t    unit;
    if (adc_oneshot_io_to_channel(pin, &unit, &chan) != ESP_OK) return;

    if (unit == ADC_UNIT_1 && s_adc1_handle == NULL) {
        adc_oneshot_unit_init_cfg_t cfg = { .unit_id = ADC_UNIT_1 };
        adc_oneshot_new_unit(&cfg, &s_adc1_handle);
    } else if (unit == ADC_UNIT_2 && s_adc2_handle == NULL) {
        adc_oneshot_unit_init_cfg_t cfg = { .unit_id = ADC_UNIT_2 };
        adc_oneshot_new_unit(&cfg, &s_adc2_handle);
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (unit == ADC_UNIT_1) adc_oneshot_config_channel(s_adc1_handle, chan, &config);
    else if (unit == ADC_UNIT_2) adc_oneshot_config_channel(s_adc2_handle, chan, &config);
}

uint32_t hw_hal_adc_read(uint8_t pin) {
    adc_channel_t chan;
    adc_unit_t    unit;
    if (adc_oneshot_io_to_channel(pin, &unit, &chan) != ESP_OK) return 0;
    int raw = 0;
    if (unit == ADC_UNIT_1 && s_adc1_handle) adc_oneshot_read(s_adc1_handle, chan, &raw);
    else if (unit == ADC_UNIT_2 && s_adc2_handle) adc_oneshot_read(s_adc2_handle, chan, &raw);
    return (uint32_t)raw;
}

uint32_t hw_hal_adc_read_mv(uint8_t pin) {
    return (hw_hal_adc_read(pin) * 3300) / 4095;
}

/* --- DAC --- */
void hw_hal_dac_init(uint8_t pin) {
    int dac_chan = (pin == 25) ? 0 : (pin == 26 ? 1 : -1);
    if (dac_chan == -1) {
        ESP_LOGE("HAL", "Pin %d does not support DAC", pin);
        return;
    }
    if (s_dac_handles[dac_chan] == NULL) {
        dac_oneshot_config_t cfg = { .chan_id = (pin == 25 ? DAC_CHAN_0 : DAC_CHAN_1) };
        esp_err_t err = dac_oneshot_new_channel(&cfg, &s_dac_handles[dac_chan]);
        if (err != ESP_OK) {
            ESP_LOGE("HAL", "DAC init failed for pin %d: %s", pin, esp_err_to_name(err));
        } else {
            ESP_LOGI("HAL", "DAC init OK for pin %d", pin);
        }
    }

}

void hw_hal_dac_set_voltage(uint8_t pin, uint8_t value) {
    int dac_chan = (pin == 25) ? 0 : (pin == 26 ? 1 : -1);
    ESP_LOGI("HAL", "DAC Set Pin %d Value %d (Chan %d)", pin, value, dac_chan);
    if (dac_chan != -1 && s_dac_handles[dac_chan]) {
        s_pin_values[pin] = value;
        dac_oneshot_output_voltage(s_dac_handles[dac_chan], value);
    }
}

void hw_hal_i2c_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t clk_speed) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = clk_speed,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

void hw_hal_uart2_init(uint8_t tx_pin, uint8_t rx_pin, uint32_t baud) {
    uart_config_t cfg = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_1, &cfg);
    uart_set_pin(UART_NUM_1, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0);
}



