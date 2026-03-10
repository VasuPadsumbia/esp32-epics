/**
 * @file utils.c
 * @brief Common utilities — implementation.
 */
#include "utils.h"
#include <string.h>
#include <stdio.h>

/* Global debug buffer for troubleshooting */
static char s_debug_last_cmd[128] = "None";

void utils_debug_set_last_cmd(const char *cmd) {
    if (!cmd) return;
    char hex[64] = {0};
    int len = strlen(cmd);
    if (len > 20) len = 20; // truncate hex view
    for(int i=0; i<len; i++) sprintf(hex+i*3, "%02X ", (uint8_t)cmd[i]);
    
    snprintf(s_debug_last_cmd, sizeof(s_debug_last_cmd) - 1, "'%s' (hex: %s)", cmd, hex);
    s_debug_last_cmd[sizeof(s_debug_last_cmd) - 1] = '\0';
}

void utils_debug_get_last_cmd(char *buf, size_t buf_len) {
    if (!buf || buf_len == 0) return;
    strncpy(buf, s_debug_last_cmd, buf_len - 1);
    buf[buf_len - 1] = '\0';
}
