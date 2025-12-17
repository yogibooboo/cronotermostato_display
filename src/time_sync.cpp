/**
 * @file time_sync.cpp
 * @brief NTP time synchronization implementation
 */

#include "time_sync.h"
#include <esp_log.h>
#include <esp_sntp.h>
#include <string.h>
#include <sys/time.h>

static const char *TAG = "TIME_SYNC";
static bool time_synced = false;

/**
 * @brief Callback when time is synchronized
 */
static void time_sync_notification_cb(struct timeval *tv)
{
    time_synced = true;

    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    ESP_LOGI(TAG, "Time synchronized: %s", strftime_buf);
}

/**
 * @brief Initialize and start NTP synchronization
 */
bool time_sync_start(void)
{
    ESP_LOGI(TAG, "Initializing SNTP...");

    // Set timezone for Italy (Europe/Rome)
    // Format: STD offset DST [offset],start[/time],end[/time]
    // CET-1CEST,M3.5.0,M10.5.0/3 = Central European Time
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");

    // Set notification callback
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // Set sync interval (default 1 hour)
    esp_sntp_set_sync_interval(3600000);  // 1 hour in ms

    esp_sntp_init();

    ESP_LOGI(TAG, "Waiting for NTP sync...");

    // Wait for time to be set (max 10 seconds)
    int retry = 0;
    const int retry_count = 50;  // 50 * 200ms = 10 seconds

    while (!time_synced && ++retry < retry_count) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (time_synced) {
        ESP_LOGI(TAG, "NTP synchronization successful");
        return true;
    } else {
        ESP_LOGW(TAG, "NTP sync timeout - will retry in background");
        return false;
    }
}

/**
 * @brief Check if time is synchronized
 */
bool time_is_synced(void)
{
    return time_synced;
}

/**
 * @brief Get current time as struct tm
 */
bool time_get_local(struct tm *timeinfo)
{
    if (!timeinfo) {
        return false;
    }

    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);

    // Check if time is valid (year > 2024)
    return (timeinfo->tm_year + 1900) > 2024;
}

/**
 * @brief Get formatted time string
 */
bool time_get_string(char *buffer, size_t buf_size, const char *format)
{
    if (!buffer || buf_size == 0) {
        return false;
    }

    struct tm timeinfo;
    if (!time_get_local(&timeinfo)) {
        snprintf(buffer, buf_size, "----/--/-- --:--:--");
        return false;
    }

    // Use default format if not specified
    if (!format) {
        format = "%Y-%m-%d %H:%M:%S";
    }

    strftime(buffer, buf_size, format, &timeinfo);
    return true;
}
