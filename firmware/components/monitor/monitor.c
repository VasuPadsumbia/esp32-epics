/**
 * @file monitor.c
 * @brief FreeRTOS task cycle-time instrumentation — implementation.
 *
 * Uses esp_timer_get_time() (64-bit microsecond counter) to measure the wall
 * clock time between monitor_task_begin() and monitor_task_end() calls.
 *
 * A FreeRTOS mutex protects the stats table so any task can read stats safely.
 * A rolling average is maintained using exponential moving average (alpha=1/8).
 */
#include "monitor.h"
#include "utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static const char *TAG = "MONITOR";

/* ---- Per-task stats record ---- */
typedef struct {
    char     name[MONITOR_NAME_LEN]; /**< Task name (NUL-terminated) */
    int64_t  t_begin;                /**< esp_timer timestamp at loop begin */
    uint32_t min_us;                 /**< Minimum cycle time observed */
    uint32_t max_us;                 /**< Maximum cycle time observed */
    uint32_t avg_us;                 /**< Exponential rolling average */
    uint32_t count;                  /**< Number of samples */
    bool     active;                 /**< Slot is in use */
} monitor_entry_t;

static monitor_entry_t s_table[MONITOR_MAX_TASKS];
static SemaphoreHandle_t s_mutex = NULL;
static size_t s_count = 0;

/* ---- Internal helpers ---- */

static monitor_entry_t *find_entry(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < MONITOR_MAX_TASKS; i++) {
        if (s_table[i].active && strncmp(s_table[i].name, name, MONITOR_NAME_LEN - 1) == 0) {
            return &s_table[i];
        }
    }
    return NULL;
}

static monitor_entry_t *alloc_entry(const char *name) {
    for (size_t i = 0; i < MONITOR_MAX_TASKS; i++) {
        if (!s_table[i].active) {
            memset(&s_table[i], 0, sizeof(monitor_entry_t));
            strncpy(s_table[i].name, name, MONITOR_NAME_LEN - 1);
            s_table[i].name[MONITOR_NAME_LEN - 1] = '\0';
            s_table[i].min_us = UINT32_MAX;
            s_table[i].active = true;
            s_count++;
            return &s_table[i];
        }
    }
    return NULL;
}

/* ---- Public API ---- */

void monitor_init(void) {
    memset(s_table, 0, sizeof(s_table));
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        UTILS_LOGE(TAG, "Failed to create monitor mutex");
    }
    s_count = 0;
    UTILS_LOGI(TAG, "Monitor subsystem initialised (max %d tasks)", MONITOR_MAX_TASKS);
}

void monitor_task_begin(const char *name) {
    if (!s_mutex || !name) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    monitor_entry_t *e = find_entry(name);
    if (!e) {
        e = alloc_entry(name);
        if (!e) {
            UTILS_LOGW(TAG, "monitor table full — cannot track '%s'", name);
            xSemaphoreGive(s_mutex);
            return;
        }
        UTILS_LOGI(TAG, "Tracking new task '%s'", name);
    }
    e->t_begin = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void monitor_task_end(const char *name) {
    if (!s_mutex || !name) return;
    int64_t t_end = esp_timer_get_time();

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    monitor_entry_t *e = find_entry(name);
    if (e && e->t_begin > 0) {
        uint32_t elapsed = (uint32_t)(t_end - e->t_begin);
        if (elapsed < e->min_us) e->min_us = elapsed;
        if (elapsed > e->max_us) e->max_us = elapsed;
        /* Exponential moving average: avg = (7*avg + new) / 8 */
        if (e->count == 0) {
            e->avg_us = elapsed;
        } else {
            e->avg_us = (e->avg_us * 7 + elapsed) / 8;
        }
        e->count++;
    }
    xSemaphoreGive(s_mutex);
}

bool monitor_get_stats(const char *name,
                       uint32_t *min_us,
                       uint32_t *max_us,
                       uint32_t *avg_us) {
    if (!s_mutex || !name) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    monitor_entry_t *e = find_entry(name);
    bool found = false;
    if (e && e->count > 0) {
        if (min_us) *min_us = e->min_us;
        if (max_us) *max_us = e->max_us;
        if (avg_us) *avg_us = e->avg_us;
        found = true;
    }
    xSemaphoreGive(s_mutex);
    return found;
}

void monitor_reset_stats(const char *name) {
    if (!s_mutex) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (name) {
        monitor_entry_t *e = find_entry(name);
        if (e) {
            e->min_us = UINT32_MAX;
            e->max_us = 0;
            e->avg_us = 0;
            e->count  = 0;
        }
    } else {
        for (size_t i = 0; i < MONITOR_MAX_TASKS; i++) {
            if (s_table[i].active) {
                s_table[i].min_us = UINT32_MAX;
                s_table[i].max_us = 0;
                s_table[i].avg_us = 0;
                s_table[i].count  = 0;
            }
        }
    }
    xSemaphoreGive(s_mutex);
}

size_t monitor_get_task_count(void) {
    return s_count;
}

bool monitor_get_task_name(size_t index, char *name_buf) {
    size_t found = 0;
    for (size_t i = 0; i < MONITOR_MAX_TASKS; i++) {
        if (s_table[i].active) {
            if (found == index) {
                strncpy(name_buf, s_table[i].name, MONITOR_NAME_LEN);
                return true;
            }
            found++;
        }
    }
    return false;
}

uint32_t monitor_get_free_heap(void) {
    return (uint32_t)esp_get_free_heap_size();
}

uint64_t monitor_get_uptime_ms(void) {
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}
