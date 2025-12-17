/**
 * @file http_server.cpp
 * @brief HTTP server implementation for Cronotermostato
 */

#include "http_server.h"
#include "wifi.h"
#include "comune.h"
#include "storage_manager.h"

#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static const char *TAG = "HTTP_SERVER";
static httpd_handle_t server = NULL;

// ============================================================================
// Helper Functions
// ============================================================================

static inline const char* mime_from_path(const char* path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".json")) return "application/json";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".jpg"))  return "image/jpeg";
    if (strstr(path, ".ico"))  return "image/x-icon";
    if (strstr(path, ".svg"))  return "image/svg+xml";
    return "application/octet-stream";
}

// ============================================================================
// HTTP Handlers
// ============================================================================

/**
 * @brief Serve file from SPIFFS
 */
static esp_err_t serve_spiffs_file(httpd_req_t *req, const char* filepath) {
    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Get file size
    fseek(fd, 0, SEEK_END);
    long file_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    // Set content type
    const char* mime_type = mime_from_path(filepath);
    httpd_resp_set_type(req, mime_type);

    // Read and send file in chunks
    char *chunk = (char*)malloc(1024);
    if (!chunk) {
        ESP_LOGE(TAG, "Failed to allocate memory for file chunk");
        fclose(fd);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t read_bytes;
    do {
        read_bytes = fread(chunk, 1, 1024, fd);
        if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send chunk");
                free(chunk);
                fclose(fd);
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);

    // Send empty chunk to signal end
    httpd_resp_send_chunk(req, NULL, 0);

    free(chunk);
    fclose(fd);

    ESP_LOGI(TAG, "Served %s (%ld bytes, %s)", filepath, file_size, mime_type);
    return ESP_OK;
}

/**
 * @brief Root handler - Serve index.html from SPIFFS
 */
static esp_err_t root_handler(httpd_req_t *req) {
    if (!storage_is_ready()) {
        ESP_LOGE(TAG, "SPIFFS not ready");
        const char* error_msg = "SPIFFS not initialized. Please run 'pio run -t uploadfs'";
        httpd_resp_send_500(req);
        httpd_resp_send(req, error_msg, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    return serve_spiffs_file(req, "/spiffs/index.html");
}

// ============================================================================
// Server Management
// ============================================================================

bool is_webserver_running(void) {
    return (server != NULL);
}

void start_webserver_if_not_running(void) {
    if (server != NULL) {
        ESP_LOGI(TAG, "Web server already running");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.core_id = 0;  // Run on core 0
    config.task_priority = 5;
    config.stack_size = 8192;
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.max_open_sockets = 7;
    config.max_uri_handlers = 16;
    config.max_resp_headers = 8;
    config.backlog_conn = 5;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 60;
    config.send_wait_timeout = 60;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started successfully");

        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        // Register WebSocket handler (from wifi.cpp)
        httpd_uri_t ws_uri = {
            .uri       = "/ws",
            .method    = HTTP_GET,
            .handler   = ws_handler,
            .user_ctx  = NULL,
            .is_websocket = true
        };
        httpd_register_uri_handler(server, &ws_uri);

        ESP_LOGI(TAG, "Registered handlers: / (GET), /ws (WebSocket)");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}

void stop_webserver(void) {
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping HTTP server");
        httpd_stop(server);
        server = NULL;
    }
}
