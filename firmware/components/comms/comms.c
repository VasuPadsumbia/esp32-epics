/**
 * @file comms.c
 * @brief UART and WiFi TCP transport implementations.
 *
 * UART task: perpetually reads bytes from UART0, assembles lines, calls
 *            protocol_parse_and_enqueue() with fd=-1 (UART sentinel).
 *
 * WiFi task: connects to AP, opens a TCP server on COMMS_TCP_PORT.
 *            For each connected client it assembles lines and calls
 *            protocol_parse_and_enqueue() with the client's socket fd.
 */
#include "comms.h"
#include "protocol.h"
#include "monitor.h"
#include "utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

/* WiFi / LwIP / socket headers */
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include <string.h>
#include <errno.h>

#define TAG_UART "COMMS_UART"
#define TAG_WIFI "COMMS_WIFI"

/* ---- Shared protocol queue handle ---- */
static QueueHandle_t s_cmd_queue = NULL;

/* ======================================================================
 * UART Transport
 * ====================================================================== */

static void uart_listener_task(void *pvParameters) {
    uint8_t *buf = (uint8_t *)malloc(COMMS_UART_BUF_SIZE);
    char     line[128];
    int      line_len = 0;

    while (1) {
        monitor_task_begin("UART");
        int rx = uart_read_bytes(UART_NUM_0, buf, COMMS_UART_BUF_SIZE,
                                  10 / portTICK_PERIOD_MS);
        for (int i = 0; i < rx; i++) {
            if (buf[i] == '\n' || buf[i] == '\r') {
                if (line_len > 0) {
                    line[line_len] = '\0';
                    UTILS_LOGD(TAG_UART, "RX line: '%s'", line);
                    protocol_parse_and_enqueue(line, -1);
                    line_len = 0;
                }
            } else if (line_len < (int)sizeof(line) - 1) {
                line[line_len++] = (char)buf[i];
            }
        }
        monitor_task_end("UART");
    }
    free(buf);
}

void comms_uart_init(QueueHandle_t cmd_queue) {
    s_cmd_queue = cmd_queue;

    const uart_config_t cfg = {
        .baud_rate  = COMMS_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &cfg);
    uart_driver_install(UART_NUM_0, COMMS_UART_BUF_SIZE * 2, 0, 0, NULL, 0);

    xTaskCreate(uart_listener_task, "uart_rx", 4096, NULL, 5, NULL);
    UTILS_LOGI(TAG_UART, "UART transport started at %d baud", COMMS_UART_BAUD);
}

void comms_uart_send(const char *response) {
    uart_write_bytes(UART_NUM_0, response, strlen(response));
}

/* ======================================================================
 * WiFi TCP Transport
 * ====================================================================== */

static EventGroupHandle_t s_wifi_event_group = NULL;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        UTILS_LOGW(TAG_WIFI, "Disconnected — reconnecting...");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        UTILS_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void tcp_client_task(void *pvParameters) {
    int client_fd = (int)(intptr_t)pvParameters;
    char buf[128];
    char line[128];
    int  line_len = 0;

    while (1) {
        monitor_task_begin("TCP");
        int rx = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (rx <= 0) {
            monitor_task_end("TCP");
            break; /* client disconnected or error */
        }

        for (int i = 0; i < rx; i++) {
            if (buf[i] == '\n' || buf[i] == '\r') {
                if (line_len > 0) {
                    line[line_len] = '\0';
                    UTILS_LOGD(TAG_WIFI, "TCP RX: '%s' fd=%d", line, client_fd);
                    protocol_parse_and_enqueue(line, client_fd);
                    line_len = 0;
                }
            } else if (line_len < (int)sizeof(line) - 1) {
                line[line_len++] = buf[i];
            }
        }
        monitor_task_end("TCP");
    }
    UTILS_LOGI(TAG_WIFI, "Client fd=%d disconnected", client_fd);
    close(client_fd);
    vTaskDelete(NULL);
}

static void tcp_server_task(void *pvParameters) {
    struct sockaddr_in server_addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(COMMS_TCP_PORT)
    };

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        UTILS_LOGE(TAG_WIFI, "socket() failed: %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        UTILS_LOGE(TAG_WIFI, "bind() failed: %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    listen(sock, 4);
    UTILS_LOGI(TAG_WIFI, "TCP server listening on port %d", COMMS_TCP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(sock, (struct sockaddr *)&client_addr, &len);
        if (client_fd < 0) {
            UTILS_LOGW(TAG_WIFI, "accept() failed: %d", errno);
            continue;
        }
        UTILS_LOGI(TAG_WIFI, "Client connected fd=%d", client_fd);
        /* Spawn a short-lived task per client */
        xTaskCreate(tcp_client_task, "tcp_client", 4096,
                    (void *)(intptr_t)client_fd, 5, NULL);
    }
    close(sock);
    vTaskDelete(NULL);
}

void comms_wifi_init(QueueHandle_t cmd_queue, const comms_wifi_cfg_t *cfg) {
    s_cmd_queue       = cmd_queue;
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &wifi_event_handler, NULL, NULL);

    wifi_config_t sta_cfg = {};
    strncpy((char *)sta_cfg.sta.ssid,     cfg->ssid,     sizeof(sta_cfg.sta.ssid));
    strncpy((char *)sta_cfg.sta.password, cfg->password, sizeof(sta_cfg.sta.password));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();

    /* Wait for connection before starting server */
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    xTaskCreate(tcp_server_task, "tcp_srv", 6144, NULL, 5, NULL);
}

void comms_wifi_send(int fd, const char *response) {
    send(fd, response, strlen(response), 0);
}
