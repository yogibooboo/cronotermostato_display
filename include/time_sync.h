/**
 * @file time_sync.h
 * @brief NTP time synchronization for ESP32-S3
 */

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start NTP synchronization
 *
 * Configures timezone for Europe/Rome and starts SNTP client.
 * Blocks until first sync is complete or timeout (10s).
 *
 * @return true if sync successful
 */
bool time_sync_start(void);

/**
 * @brief Check if time is synchronized
 *
 * @return true if time has been synced via NTP
 */
bool time_is_synced(void);

/**
 * @brief Get current time as struct tm
 *
 * @param[out] timeinfo Pointer to tm structure to fill
 * @return true if time is valid (synced)
 */
bool time_get_local(struct tm *timeinfo);

/**
 * @brief Get formatted time string
 *
 * @param[out] buffer Buffer to write string (min 64 bytes)
 * @param format strftime format string (NULL = default "YYYY-MM-DD HH:MM:SS")
 * @return true if successful
 */
bool time_get_string(char *buffer, size_t buf_size, const char *format);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_H
