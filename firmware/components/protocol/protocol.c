/**
 * @file protocol.c
 * @brief ASCII protocol parser — implementation.
 */
#include "protocol.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "PROTOCOL";
static QueueHandle_t s_cmd_queue = NULL;

QueueHandle_t protocol_init(void) {
    s_cmd_queue = xQueueCreate(PROTOCOL_QUEUE_DEPTH, sizeof(protocol_cmd_t));
    if (!s_cmd_queue) {
        UTILS_LOGE(TAG, "Failed to create command queue");
    }
    UTILS_LOGI(TAG, "Protocol subsystem initialised");
    return s_cmd_queue;
}

/* ---------- Parser ---------- */

void protocol_parse_and_enqueue(const char *line, int response_fd) {
    if (!line || !s_cmd_queue) return;

    /* Diagnostic hook */
    utils_debug_set_last_cmd(line);

    protocol_cmd_t cmd = {
        .device      = DEVICE_UNKNOWN,
        .action      = ACTION_UNKNOWN,
        .value       = 0,
        .value2      = 0,
        .value3      = 0,
        .str_arg     = {0},
        .response_fd = response_fd
    };

    char buf[64];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Strip trailing CR, LF, and SPACE */
    char *end = buf + strlen(buf) - 1;
    while (end >= buf && (*end == '\r' || *end == '\n' || *end == ' ')) {
        *end-- = '\0';
    }

    /* Basic splitting: DEVICE:ACTION [ARGS...] */
    char *colon = strchr(buf, ':');
    if (!colon) return;
    *colon = '\0';
    char *dev_s = buf;
    char *rest  = colon + 1;

    char *space = strchr(rest, ' ');
    char *act_s = rest;
    char *args  = NULL;
    if (space) {
        *space = '\0';
        args = space + 1;
    }

    /* Map Device */
    if (strcmp(dev_s, "SYS") == 0) {
        cmd.device = DEVICE_SYS;
    } else if (strcmp(dev_s, "LED") == 0) {
        cmd.device = DEVICE_LED;
    } else if (strcmp(dev_s, "PWM") == 0) {
        cmd.device = DEVICE_PWM;
    } else if (strcmp(dev_s, "AI") == 0) {
        cmd.device = DEVICE_AI;
    } else if (strcmp(dev_s, "DAC") == 0) {
        cmd.device = DEVICE_DAC;
    } else if (strcmp(dev_s, "I2C") == 0) {
        cmd.device = DEVICE_I2C;
    } else if (strcmp(dev_s, "UART2") == 0) {
        cmd.device = DEVICE_UART2;
    } else if (strcmp(dev_s, "PIN") == 0) {
        cmd.device = DEVICE_PIN;
    } else if (strcmp(dev_s, "TASK") == 0) {
        cmd.device = DEVICE_TASK;
    } else if (strcmp(dev_s, "GPIO") == 0) { // Re-added GPIO
        cmd.device = DEVICE_GPIO;
    } else {
        UTILS_LOGE(TAG, "Unknown device: %s", dev_s);
        return;
    }

    /* Map Action */
    if      (strcmp(act_s, "SET")     == 0) cmd.action = ACTION_SET;
    else if (strcmp(act_s, "GET")     == 0) cmd.action = ACTION_GET;
    else if (strcmp(act_s, "GETALL")  == 0) cmd.action = ACTION_GETALL;
    else if (strcmp(act_s, "LIST")    == 0) cmd.action = ACTION_LIST;
    else if (strcmp(act_s, "RESET")   == 0) cmd.action = ACTION_RESET;
    else if (strcmp(act_s, "PING")    == 0) cmd.action = ACTION_PING;
    else if (strcmp(act_s, "STATUS")  == 0) cmd.action = ACTION_STATUS;
    else if (strcmp(act_s, "VERSION") == 0) cmd.action = ACTION_VERSION;
    else if (strcmp(act_s, "HEAP")    == 0) cmd.action = ACTION_HEAP;
    else if (strcmp(act_s, "DIR")     == 0) cmd.action = ACTION_DIR;
    else if (strcmp(act_s, "CFG")     == 0) cmd.action = ACTION_CFG;
    else if (strcmp(act_s, "START")   == 0) cmd.action = ACTION_START;
    else if (strcmp(act_s, "STOP")    == 0) cmd.action = ACTION_STOP;
    else if (strcmp(act_s, "CYCLE")   == 0) cmd.action = ACTION_CYCLE;

    /* Parse Arguments based on context */
    if (args) {
        if (cmd.device == DEVICE_TASK) {
            strncpy(cmd.str_arg, args, PROTOCOL_FIELD_LEN - 1);
        } else {
            /* Most other devices take 1-3 numeric args */
            sscanf(args, "%d %d %d", (int *)&cmd.value, (int *)&cmd.value2, (int *)&cmd.value3);
        }
    }

    if (cmd.device == DEVICE_UNKNOWN || cmd.action == ACTION_UNKNOWN) {
        UTILS_LOGW(TAG, "Unknown command parsing failure: '%s' (Dev:%d Act:%d)", line, cmd.device, cmd.action);
        return;
    }

    if (xQueueSend(s_cmd_queue, &cmd, 0) != pdTRUE) {
        // UTILS_LOGW(TAG, "Queue full — dropping command");
    }
}

void protocol_format_response(char *buf, size_t len, bool success, int value) {
    if (!success) {
        snprintf(buf, len, "ERR\n");
        return;
    }
    if (value >= 0) {
        snprintf(buf, len, "%d\n", value);
    } else {
        snprintf(buf, len, "OK\n");
    }
}

