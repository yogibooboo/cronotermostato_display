/**
 * @file storage_manager.cpp
 * @brief SPIFFS filesystem and configuration storage management
 */

#include "storage_manager.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <sys/stat.h>

static const char *TAG = "STORAGE";
static bool spiffs_mounted = false;

/**
 * @brief Initialize SPIFFS filesystem
 */
esp_err_t storage_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,  // Use first partition with type=data, subtype=spiffs
        .max_files = 10,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    // Check SPIFFS info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPIFFS mounted successfully");
    ESP_LOGI(TAG, "Partition size: total: %d bytes, used: %d bytes (%.1f%%)",
             total, used, 100.0 * used / total);

    // Verify index.html exists
    struct stat st;
    if (stat("/spiffs/index.html", &st) == 0) {
        ESP_LOGI(TAG, "Found /spiffs/index.html (%ld bytes)", st.st_size);
    } else {
        ESP_LOGW(TAG, "File /spiffs/index.html not found - run 'pio run -t uploadfs'");
    }

    spiffs_mounted = true;
    return ESP_OK;
}

/**
 * @brief Get SPIFFS usage statistics
 */
esp_err_t storage_get_info(size_t *total_bytes, size_t *used_bytes)
{
    if (!spiffs_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_spiffs_info(NULL, total_bytes, used_bytes);
}

/**
 * @brief Check if SPIFFS is mounted and healthy
 */
bool storage_is_ready(void)
{
    return spiffs_mounted;
}

/**
 * @brief Unmount SPIFFS filesystem
 */
esp_err_t storage_unmount(void)
{
    if (!spiffs_mounted) {
        ESP_LOGW(TAG, "SPIFFS already unmounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Unmounting SPIFFS...");
    esp_err_t ret = esp_vfs_spiffs_unregister(NULL);
    if (ret == ESP_OK) {
        spiffs_mounted = false;
        ESP_LOGI(TAG, "SPIFFS unmounted successfully");
    } else {
        ESP_LOGE(TAG, "Failed to unmount SPIFFS: %s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief Remount SPIFFS filesystem
 */
esp_err_t storage_remount(void)
{
    if (spiffs_mounted) {
        ESP_LOGW(TAG, "SPIFFS already mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Remounting SPIFFS...");
    return storage_init();
}
