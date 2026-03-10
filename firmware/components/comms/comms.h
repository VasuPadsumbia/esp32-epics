/**
 * @file comms.h
 * @brief Communications layer: UART and WiFi TCP transports.
 *
 * This component provides two FreeRTOS tasks:
 *   - comms_uart_task: reads lines from UART0 and passes them to the protocol queue.
 *   - comms_wifi_task: connects to WiFi and opens a TCP server; reads lines from
 *                      connected clients and passes them to the protocol queue.
 *
 * Both transports use the same protocol queue and reply via their own I/O path,
 * keeping the application (app_task) transport-agnostic.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief UART baud rate for the EPICS StreamDevice connection. */
#define COMMS_UART_BAUD     115200

/** @brief TCP port on which the WiFi server listens. */
#define COMMS_TCP_PORT      7070

/** @brief Maximum bytes in the UART receive buffer. */
#define COMMS_UART_BUF_SIZE 1024

/**
 * @brief Configuration for the WiFi transport.
 *
 * Populate this in your app config or sdkconfig (via Kconfig) and pass it to
 * comms_wifi_init() before starting the WiFi task.
 */
typedef struct {
    char ssid[32];
    char password[64];
} comms_wifi_cfg_t;

/**
 * @brief Initialise the UART transport and start the listener task.
 * @param cmd_queue  The protocol queue to push parsed commands into.
 */
void comms_uart_init(QueueHandle_t cmd_queue);

/**
 * @brief Initialise the WiFi transport, connect to AP, and start TCP server task.
 * @param cmd_queue  The protocol queue to push parsed commands into.
 * @param cfg        WiFi SSID and password.
 */
void comms_wifi_init(QueueHandle_t cmd_queue, const comms_wifi_cfg_t *cfg);

/**
 * @brief Send a response back via UART0.
 * @param response  Null-terminated reply string (e.g. "OK\n").
 */
void comms_uart_send(const char *response);

/**
 * @brief Send a response via WiFi to a specific socket fd.
 * @param fd        Client socket descriptor.
 * @param response  Null-terminated reply string.
 */
void comms_wifi_send(int fd, const char *response);

#ifdef __cplusplus
}
#endif
