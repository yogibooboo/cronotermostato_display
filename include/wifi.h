/**
 * @file wifi.h
 * @brief WiFi management and WebSocket client handling
 */

#ifndef WIFI_H
#define WIFI_H

#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// WiFi Setup
// ============================================================================

/**
 * @brief Initialize and start WiFi connection
 */
void setup_wifi(void);

// ============================================================================
// WebSocket Client Management
// ============================================================================

/**
 * @brief Register a new WebSocket client for broadcast
 * @param fd File descriptor of the client socket
 */
void register_ws_client(int fd);

/**
 * @brief Unregister a WebSocket client
 * @param fd File descriptor of the client socket
 */
void unregister_ws_client(int fd);

/**
 * @brief WebSocket handler (called by http_server)
 * @param req HTTP request
 * @return ESP_OK on success
 */
esp_err_t ws_handler(httpd_req_t *req);

/**
 * @brief Cleanup dead WebSocket connections
 */
void websocket_cleanup_dead_connections(void);

// ============================================================================
// Diagnostics
// ============================================================================

/**
 * @brief Get WiFi status information
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @param subcommand Optional subcommand (NULL for full status)
 */
void get_wifi_status(char* buffer, size_t buffer_size, const char* subcommand);

// ============================================================================
// Constants
// ============================================================================

#ifndef WS_FRAME_SIZE
#define WS_FRAME_SIZE 1024
#endif

#ifndef POST_BODY_SIZE
#define POST_BODY_SIZE (16 * 1024)
#endif

#ifndef FILE_CHUNK_SIZE
#define FILE_CHUNK_SIZE (8 * 1024)
#endif

#ifdef __cplusplus
}
#endif

#endif // WIFI_H
