/**
 * @file utils.h
 * @brief Logging utilities for the ESP32 EPICS firmware.
 *
 * Wraps ESP_LOG macros and provides a consistent TAG-based logging strategy.
 * To enable verbose debug output for a component, set LOG_LEVEL to DEBUG
 * in sdkconfig or call esp_log_level_set() in your component's init function.
 */
#pragma once

#include "esp_log.h"

/** @brief Log an informational message. Usage: UTILS_LOGI(TAG, "msg %d", val) */
#define UTILS_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)

/** @brief Log a warning message. */
#define UTILS_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)

/** @brief Log an error message. */
#define UTILS_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)

/**
 * @brief Thread-safe hook to record a debug string (e.g. last command).
 */
void utils_debug_set_last_cmd(const char *cmd);

/**
 * @brief Thread-safe hook to retrieve the recorded debug string.
 */
void utils_debug_get_last_cmd(char *buf, size_t buf_len);


/** @brief Log a debug message (only visible if log level set to DEBUG). */
#define UTILS_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
