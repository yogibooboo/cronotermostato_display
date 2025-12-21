/**
 * @file log_reader.h
 * @brief Binary log file reader for thermostat history
 */

#ifndef LOG_READER_H
#define LOG_READER_H

#include <esp_err.h>
#include <esp_http_server.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Log file format structures
#define LOG_MAGIC 0x474F4C54  // "TLOG"

typedef struct __attribute__((packed)) {
    uint32_t magic;           // "TLOG" (0x544C4F47)
    uint8_t version;          // Format version (1)
    uint16_t year;            // Year
    uint8_t month;            // Month (1-12)
    uint8_t day;              // Day (1-31)
    uint16_t num_samples;     // Number of samples
    uint8_t reserved;         // Reserved
} log_header_t;

// Note: Using history_sample_t from comune.h (10 bytes per record)
// This header just defines the log file header structure

/**
 * @brief Register log data HTTP handlers
 *
 * @param server HTTP server handle
 * @return ESP_OK on success
 */
esp_err_t register_log_handlers(httpd_handle_t server);

/**
 * @brief Read log file and return JSON data
 *
 * @param req HTTP request
 * @return ESP_OK on success
 */
esp_err_t log_data_handler(httpd_req_t *req);

/**
 * @brief Read log file and return raw binary data
 *
 * @param req HTTP request
 * @return ESP_OK on success
 */
esp_err_t log_raw_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // LOG_READER_H
