/**
 * @file monitor.h
 * @brief FreeRTOS task cycle-time instrumentation.
 *
 * Each FreeRTOS task that needs cycle-time monitoring calls:
 *   - monitor_task_begin(name)  at the top of its main loop
 *   - monitor_task_end(name)    at the bottom of its main loop
 *
 * Statistics (min/max/avg in microseconds) are accumulated in a mutex-
 * protected table and can be queried at any time from any task via
 * monitor_get_stats() or through the protocol TASK:GET command.
 *
 * Usage example:
 * @code
 *   void my_task(void *arg) {
 *       while (1) {
 *           monitor_task_begin("MY_TASK");
 *           // ... task work ...
 *           monitor_task_end("MY_TASK");
 *       }
 *   }
 * @endcode
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of simultaneously tracked tasks. */
#define MONITOR_MAX_TASKS  8

/** @brief Maximum length of a task name string (including NUL). */
#define MONITOR_NAME_LEN  16

/**
 * @brief Initialise the monitor subsystem.
 *
 * Must be called once before any task calls monitor_task_begin().
 */
void monitor_init(void);

/**
 * @brief Mark the start of a task loop iteration.
 * @param name  Short ASCII task name (max MONITOR_NAME_LEN-1 chars).
 *              The name is registered on first call.
 */
void monitor_task_begin(const char *name);

/**
 * @brief Mark the end of a task loop iteration and update stats.
 * @param name  Same name passed to the corresponding monitor_task_begin().
 */
void monitor_task_end(const char *name);

/**
 * @brief Query accumulated cycle-time statistics for a named task.
 * @param[in]  name    Task name.
 * @param[out] min_us  Minimum cycle time in microseconds.
 * @param[out] max_us  Maximum cycle time in microseconds.
 * @param[out] avg_us  Rolling average cycle time in microseconds.
 * @return true if the task was found; false otherwise.
 */
bool monitor_get_stats(const char *name,
                       uint32_t *min_us,
                       uint32_t *max_us,
                       uint32_t *avg_us);

/**
 * @brief Reset the min/max/avg accumulators for a named task.
 * @param name  Task name, or NULL to reset ALL tasks.
 */
void monitor_reset_stats(const char *name);

/**
 * @brief Get the number of currently tracked tasks.
 */
size_t monitor_get_task_count(void);

/**
 * @brief Get the name of a task by index (for iterating all tasks).
 * @param index  Zero-based index (0 .. monitor_get_task_count()-1).
 * @param[out] name_buf  Buffer to receive the name (size >= MONITOR_NAME_LEN).
 * @return true if index is valid.
 */
bool monitor_get_task_name(size_t index, char *name_buf);

/**
 * @brief Return the current free heap in bytes.
 */
uint32_t monitor_get_free_heap(void);

/**
 * @brief Return the system uptime in milliseconds.
 */
uint64_t monitor_get_uptime_ms(void);

#ifdef __cplusplus
}
#endif
