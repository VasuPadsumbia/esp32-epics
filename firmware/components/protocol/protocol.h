/**
 * @file protocol.h
 * @brief ASCII command protocol parser for the ESP32 EPICS firmware.
 *
 * Protocol wire format: "<DEVICE>:<ACTION> [VALUE]\r\n"
 *
 * Supported commands:
 *   SYS:PING           -> PONG
 *   SYS:STATUS         -> OK <uptime_ms>
 *   SYS:VERSION        -> <major>.<minor>.<patch>
 *   SYS:HEAP           -> <free_bytes>
 *   SYS:RESET          -> OK  (then reboots)
 *
 *   LED:SET <0/1>      -> OK
 *   LED:GET            -> <0/1>
 *
 *   GPIO:SET <pin> <0/1>       -> OK
 *   GPIO:GET <pin>             -> <0/1>
 *   GPIO:DIR <pin> <IN|OUT>    -> OK
 *
 *   TASK:LIST          -> <N>
 *   TASK:GET <name>    -> <min_us> <max_us> <avg_us>
 *   TASK:GETALL        -> multi-line: <name> <min> <max> <avg>\n (one per task)
 *   TASK:RESET <name>  -> OK
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_FIELD_LEN  16
#define PROTOCOL_QUEUE_DEPTH 32

/** @brief Known device identifiers. */
typedef enum {
    DEVICE_SYS,
    DEVICE_LED,
    DEVICE_GPIO,
    DEVICE_PWM,
    DEVICE_AI,
    DEVICE_DAC,
    DEVICE_I2C,
    DEVICE_UART2,
    DEVICE_PIN,
    DEVICE_TASK,
    DEVICE_UNKNOWN
} protocol_device_t;

/** @brief Known actions. */
typedef enum {
    ACTION_SET    = 0,
    ACTION_GET,
    ACTION_GETALL,
    ACTION_LIST,
    ACTION_RESET,
    ACTION_PING,
    ACTION_STATUS,
    ACTION_VERSION,
    ACTION_HEAP,
    ACTION_DIR,
    ACTION_CFG,
    ACTION_START,
    ACTION_STOP,
    ACTION_CYCLE,
    ACTION_UNKNOWN
} protocol_action_t;



/**
 * @brief A fully-parsed command, ready for the application task to dispatch.
 *
 * @note response_fd == -1 means UART; otherwise it is a WiFi socket descriptor.
 */
typedef struct {
    protocol_device_t device;
    protocol_action_t action;
    int32_t           value;        /**< Numeric argument 1 (pin, channel, etc.) */
    int32_t           value2;       /**< Numeric argument 2 (level, freq, sda, tx, etc.) */
    int32_t           value3;       /**< Numeric argument 3 (scl, rx, baud, etc.) */
    char              str_arg[PROTOCOL_FIELD_LEN]; /**< String argument (task name) */
    int               response_fd;  /**< -1 = UART, else WiFi socket fd */
} protocol_cmd_t;


/** @brief Initialise the protocol module and create the shared command queue. */
QueueHandle_t protocol_init(void);

/**
 * @brief Parse a raw ASCII line into a command and enqueue it.
 *
 * Non-blocking: if the queue is full the command is silently dropped with a warning.
 * @param line        Null-terminated ASCII line (stripped of CR/LF).
 * @param response_fd Socket fd for WiFi, or -1 for UART.
 */
void protocol_parse_and_enqueue(const char *line, int response_fd);

/**
 * @brief Format a reply string.
 * @param buf     Output buffer.
 * @param len     Buffer length.
 * @param success true → "OK\n" (or "<value>\n"); false → "ERR\n".
 * @param value   If >= 0, emit the numeric value instead of "OK".
 */
void protocol_format_response(char *buf, size_t len, bool success, int value);

#ifdef __cplusplus
}
#endif
