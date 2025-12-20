/**
 * @file storage_manager.h
 * @brief SPIFFS filesystem and configuration storage management
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SPIFFS filesystem
 *
 * Mounts the SPIFFS partition and checks filesystem health.
 *
 * @return ESP_OK on success
 */
esp_err_t storage_init(void);

/**
 * @brief Get SPIFFS usage statistics
 *
 * @param[out] total_bytes Total SPIFFS size
 * @param[out] used_bytes Used space
 * @return ESP_OK on success
 */
esp_err_t storage_get_info(size_t *total_bytes, size_t *used_bytes);

/**
 * @brief Check if SPIFFS is mounted and healthy
 *
 * @return true if SPIFFS is ready
 */
bool storage_is_ready(void);

/**
 * @brief Unmount SPIFFS filesystem
 *
 * Used before OTA updates to prevent concurrent access
 *
 * @return ESP_OK on success
 */
esp_err_t storage_unmount(void);

/**
 * @brief Remount SPIFFS filesystem
 *
 * Used after OTA updates or when unmount was called
 *
 * @return ESP_OK on success
 */
esp_err_t storage_remount(void);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_MANAGER_H
