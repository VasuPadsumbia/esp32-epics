/**
 * @file webui.h
 * @brief Embedded HTTP server for the ESP32 EPICS Integration.
 *
 * Provides a dashboard for live status monitoring and field configuration.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Web UI component.
 *
 * Starts the HTTP server on the configured WEBUI_PORT.
 */
void webui_init(QueueHandle_t cmd_queue);

/**
 * @brief Update the last received command for the /api/debug endpoint.
 */
void webui_debug_update(const char *cmd);


/**
 * @brief Stop the Web UI component.
 */
void webui_stop(void);

#ifdef __cplusplus
}
#endif
