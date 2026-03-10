/**
 * @file webui.c
 * @brief Embedded HTTP server Implementation.
 *
 * Serves a simple dashboard and provides REST API for:
 *   GET /             – Dashboard SPA
 *   GET /api/status   – uptime_ms, free_heap, version
 *   GET /api/tasks    – task cycle-time stats array
 *   GET /api/gpio     – current LED/GPIO state
 *   POST /api/gpio    – {"pin":<n>,"value":<0|1>}
 *   GET /api/config   – SSID, TCP port (read-only)
 */
#include "webui.h"
#include "utils.h"
#include "monitor.h"
#include "hw_hal.h"
#include "protocol.h"

#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include <cJSON.h>

#ifdef CONFIG_PROJECT_WEBUI_PORT
#  define WEBUI_PORT CONFIG_PROJECT_WEBUI_PORT
#else
#  define WEBUI_PORT 80
#endif

#ifdef CONFIG_PROJECT_WIFI_SSID
#  define PROJ_SSID CONFIG_PROJECT_WIFI_SSID
#else
#  define PROJ_SSID "unknown"
#endif

#ifdef CONFIG_PROJECT_TCP_PORT
#  define PROJ_TCP_PORT CONFIG_PROJECT_TCP_PORT
#else
#  define PROJ_TCP_PORT 7070
#endif

#ifdef CONFIG_PROJECT_LED_GPIO
#  define LED_GPIO CONFIG_PROJECT_LED_GPIO
#else
#  define LED_GPIO 2
#endif

static const char *TAG = "WEBUI";
static httpd_handle_t s_server  = NULL;
static QueueHandle_t  s_cmd_queue = NULL;

/* ---- helpers ---- */
static void send_json(httpd_req_t *req, cJSON *root) {
    char *out = cJSON_PrintUnformatted(root);
    if (!out) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON Print failed");
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_sendstr(req, out);
        free(out);
    }
    cJSON_Delete(root);
}

/* --- Pin Capabilities --- */
#define CAP_GPIO_IN  (1 << 1)
#define CAP_GPIO_OUT (1 << 2)
#define CAP_PWM      (1 << 3)
#define CAP_ADC      (1 << 4)
#define CAP_DAC      (1 << 5)
#define CAP_I2C      (1 << 6)
#define CAP_UART     (1 << 7)

static uint32_t get_pin_caps(uint8_t pin) {
    uint32_t caps = CAP_GPIO_IN;
    /* Basic WROOM-32 Pinout logic */
    if (pin < 34) caps |= CAP_GPIO_OUT | CAP_PWM;
    if (pin == 32 || pin == 33 || pin == 34 || pin == 35 || pin == 36 || pin == 39 || 
        pin == 0 || pin == 2 || pin == 4 || pin == 12 || pin == 13 || pin == 14 || pin == 15 || pin == 25 || pin == 26 || pin == 27) {
        caps |= CAP_ADC;
    }
    if (pin == 25 || pin == 26) caps |= CAP_DAC;
    if (pin == 21 || pin == 22) caps |= CAP_I2C;
    if (pin == 16 || pin == 17) caps |= CAP_UART;
    return caps;
}

/* ---- GET / ---- */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

/* ---- GET / ---- */
static esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

/* ---- GET /api/status ---- */
static esp_err_t status_get_handler(httpd_req_t *req) {
    extern uint32_t app_get_cycle_ms(void);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "uptime_ms",  (double)monitor_get_uptime_ms());
    cJSON_AddNumberToObject(root, "free_heap",  (double)monitor_get_free_heap());
    cJSON_AddStringToObject(root, "version",    "1.1.0");
    cJSON_AddNumberToObject(root, "cycle_ms",   (double)app_get_cycle_ms());
    send_json(req, root);
    return ESP_OK;
}

/* ---- GET /api/tasks ---- */
static esp_err_t tasks_get_handler(httpd_req_t *req) {
    size_t count = monitor_get_task_count();
    cJSON *arr = cJSON_CreateArray();
    
    for (size_t i = 0; i < count; i++) {
        char name[MONITOR_NAME_LEN];
        if (monitor_get_task_name(i, name)) {
            uint32_t mn, mx, av;
            if (monitor_get_stats(name, &mn, &mx, &av)) {
                cJSON *t = cJSON_CreateObject();
                cJSON_AddStringToObject(t, "name",   name);
                cJSON_AddNumberToObject(t, "min_us", (double)mn);
                cJSON_AddNumberToObject(t, "max_us", (double)mx);
                cJSON_AddNumberToObject(t, "avg_us", (double)av);
                cJSON_AddItemToArray(arr, t);
            }
        }
    }
    send_json(req, arr);
    return ESP_OK;
}


/* ---- POST /api/gpio ---- */
static esp_err_t gpio_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    cJSON *body = cJSON_Parse(buf);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }
    cJSON *jpin = cJSON_GetObjectItem(body, "pin");
    cJSON *jval = cJSON_GetObjectItem(body, "value");
    if (jpin && jval) {
        int pin = jpin->valueint;
        int val = jval->valueint;
        hw_hal_gpio_set(pin, val ? true : false);
    }
    cJSON_Delete(body);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    send_json(req, resp);
    return ESP_OK;
}

/* ---- GET /api/gpio/schema ---- */
static esp_err_t gpio_schema_get_handler(httpd_req_t *req) {
    cJSON *arr = cJSON_CreateArray();
    const char *role_names[] = {"UNUSED","GPIO_IN","GPIO_OUT","PWM","ADC","DAC","I2C","UART"};
    
    for (int i = 0; i < 40; i++) {
        /* Skip SPI flash pins (6-11) */
        if (i >= 6 && i <= 11) continue;
        
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "pin", i);
        uint32_t caps = get_pin_caps(i);
        cJSON_AddNumberToObject(o, "caps", caps);
        
        pin_role_t role = hw_hal_get_pin_role(i);
        cJSON_AddNumberToObject(o, "role", (int)role); 
        cJSON_AddStringToObject(o, "role_name", role_names[role]);
        
        if (role == PIN_ROLE_PWM || role == PIN_ROLE_DAC) {
            cJSON_AddNumberToObject(o, "val", hw_hal_get_pin_value(i));
        } else {
            cJSON_AddNumberToObject(o, "val", hw_hal_gpio_get(i));
        }
        
        if (caps & CAP_ADC) cJSON_AddNumberToObject(o, "mv", hw_hal_adc_read_mv(i));
        
        cJSON_AddItemToArray(arr, o);
    }
    send_json(req, arr);
    return ESP_OK;
}

/* ---- GET /api/config ---- */
static esp_err_t config_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid",     PROJ_SSID);
    cJSON_AddNumberToObject(root, "tcp_port", PROJ_TCP_PORT);
    cJSON_AddNumberToObject(root, "led_pin",  LED_GPIO);
    send_json(req, root);
    return ESP_OK;
}

/* ---- POST /api/pin/cfg ---- */
static esp_err_t pin_cfg_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    if (httpd_req_recv(req, buf, req->content_len) <= 0) return ESP_FAIL;
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    int pin = cJSON_GetObjectItem(body, "pin")->valueint;
    int role = cJSON_GetObjectItem(body, "role")->valueint;
    hw_hal_pin_cfg((uint8_t)pin, (pin_role_t)role);
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ---- POST /api/dac ---- */
static esp_err_t dac_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    if (httpd_req_recv(req, buf, req->content_len) <= 0) return ESP_FAIL;
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    int pin = cJSON_GetObjectItem(body, "pin")->valueint;
    int val = cJSON_GetObjectItem(body, "value")->valueint;
    hw_hal_dac_set_voltage((uint8_t)pin, (uint8_t)val);
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}


/* ---- POST /api/pwm ---- */
static esp_err_t pwm_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) return ESP_FAIL;
    
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    
    cJSON *jpin = cJSON_GetObjectItem(body, "pin");
    cJSON *jduty = cJSON_GetObjectItem(body, "duty");
    if (jpin && jduty) {
        hw_hal_pwm_set_duty((uint8_t)jpin->valueint, (uint32_t)jduty->valueint);
    }
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ---- POST /api/config/hw ---- */
static esp_err_t hw_config_post_handler(httpd_req_t *req) {
    char buf[256] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) return ESP_FAIL;
    
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    
    cJSON *jtype = cJSON_GetObjectItem(body, "type");
    if (jtype && strcmp(jtype->valuestring, "i2c") == 0) {
        int sda = cJSON_GetObjectItem(body, "sda")->valueint;
        int scl = cJSON_GetObjectItem(body, "scl")->valueint;
        hw_hal_i2c_init(sda, scl, 100000);
        UTILS_LOGI(TAG, "WebUI Reconf I2C: SDA=%d SCL=%d", sda, scl);
    } else if (jtype && strcmp(jtype->valuestring, "uart") == 0) {
        int tx = cJSON_GetObjectItem(body, "tx")->valueint;
        int rx = cJSON_GetObjectItem(body, "rx")->valueint;
        hw_hal_uart2_init(tx, rx, 9600);
        UTILS_LOGI(TAG, "WebUI Reconf UART2: TX=%d RX=%d", tx, rx);
    }
    
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ---- POST /api/config/sys ---- */
static esp_err_t sys_config_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) return ESP_FAIL;
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    cJSON *jcycle = cJSON_GetObjectItem(body, "cycle_ms");
    if (jcycle) {
        protocol_cmd_t cmd = {
            .device = DEVICE_SYS,
            .action = ACTION_CYCLE,
            .value  = jcycle->valueint,
            .response_fd = -1
        };
        xQueueSend(s_cmd_queue, &cmd, 0);
    }
    cJSON_Delete(body);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ---- POST /api/cycle ---- */
static esp_err_t cycle_post_handler(httpd_req_t *req) {
    char buf[64] = {0};
    int  len = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    if (httpd_req_recv(req, buf, len) <= 0) return ESP_FAIL;
    cJSON *body = cJSON_Parse(buf);
    if (!body) return ESP_FAIL;
    cJSON *jms = cJSON_GetObjectItem(body, "ms");
    if (jms && jms->valueint > 0) {
        protocol_cmd_t cmd = {
            .device = DEVICE_SYS,
            .action = ACTION_CYCLE,
            .value  = jms->valueint,
            .response_fd = -1
        };
        xQueueSend(s_cmd_queue, &cmd, 0);
        UTILS_LOGI(TAG, "WebUI set cycle: %d ms", jms->valueint);
    }
    cJSON_Delete(body);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    send_json(req, resp);
    return ESP_OK;
}


/* ---- URI table ---- */
static const httpd_uri_t uris[] = {
    { .uri = "/",            .method = HTTP_GET,  .handler = index_get_handler,  .user_ctx = NULL },
    { .uri = "/api/status",  .method = HTTP_GET,  .handler = status_get_handler, .user_ctx = NULL },
    { .uri = "/api/tasks",   .method = HTTP_GET,  .handler = tasks_get_handler,  .user_ctx = NULL },
    { .uri = "/api/gpio/schema", .method = HTTP_GET, .handler = gpio_schema_get_handler, .user_ctx = NULL },
    { .uri = "/api/pin/cfg", .method = HTTP_POST, .handler = pin_cfg_post_handler, .user_ctx = NULL },
    { .uri = "/api/dac",     .method = HTTP_POST, .handler = dac_post_handler,     .user_ctx = NULL },
    { .uri = "/api/pwm",     .method = HTTP_POST, .handler = pwm_post_handler,   .user_ctx = NULL },
    { .uri = "/api/config/hw", .method = HTTP_POST, .handler = hw_config_post_handler, .user_ctx = NULL },
    { .uri = "/api/config/sys",.method = HTTP_POST, .handler = sys_config_post_handler,.user_ctx = NULL },
    { .uri = "/api/gpio",    .method = HTTP_POST, .handler = gpio_post_handler,  .user_ctx = NULL },
    { .uri = "/api/config",  .method = HTTP_GET,  .handler = config_get_handler, .user_ctx = NULL },
    { .uri = "/api/cycle",   .method = HTTP_POST, .handler = cycle_post_handler, .user_ctx = NULL },
};


void webui_init(QueueHandle_t cmd_queue) {
    s_cmd_queue = cmd_queue;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port      = WEBUI_PORT;
    config.max_uri_handlers = 16;
    config.stack_size       = 8192; // Increased for complex JSON processing
    config.max_open_sockets = 7;    // Increased to handle simultaneous polling

    UTILS_LOGI(TAG, "Starting server on port %d", config.server_port);
    if (httpd_start(&s_server, &config) == ESP_OK) {
        for (size_t i = 0; i < sizeof(uris)/sizeof(uris[0]); i++) {
            httpd_register_uri_handler(s_server, &uris[i]);
        }
        UTILS_LOGI(TAG, "Registered %d URI handlers", (int)(sizeof(uris)/sizeof(uris[0])));
    } else {
        UTILS_LOGE(TAG, "Failed to start HTTP server");
    }
}

void webui_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
