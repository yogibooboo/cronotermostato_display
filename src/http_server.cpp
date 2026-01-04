/**
 * @file http_server.cpp
 * @brief HTTP server implementation for Cronotermostato
 */

#include "http_server.h"
#include "wifi.h"
#include "comune.h"
#include "storage_manager.h"
#include "ota_handlers.h"
#include "log_reader.h"
#include "status_api.h"

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

/**
 * @brief Generic file handler - Serve any file from SPIFFS
 */
static esp_err_t file_handler(httpd_req_t *req) {
    if (!storage_is_ready()) {
        ESP_LOGE(TAG, "SPIFFS not ready");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    const char *uri = req->uri;

    // Protezione base contro path traversal
    if (strstr(uri, "..")) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad path");
        return ESP_FAIL;
    }

    char filepath[520];

    // Mappa route speciali a file HTML
    if (strcmp(uri, "/update") == 0) {
        snprintf(filepath, sizeof(filepath), "/spiffs/update.html");
    } else {
        snprintf(filepath, sizeof(filepath), "/spiffs%s", uri);
    }

    return serve_spiffs_file(req, filepath);
}

// Handler per upload di singoli file
static esp_err_t file_upload_handler(httpd_req_t *req)
{
    char filepath[128];
    char filename[64] = {0};

    // Estrai nome file dalla query string (?file=index.html)
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "file", filename, sizeof(filename)) != ESP_OK) {
                free(buf);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'file' parameter");
                return ESP_FAIL;
            }
        }
        free(buf);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'file' parameter");
        return ESP_FAIL;
    }

    // Verifica che il nome file sia valido (no path traversal)
    if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }

    // Costruisci path completo
    snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);

    ESP_LOGI(TAG, "Uploading file: %s (size: %d bytes)", filepath, req->content_len);

    // Apri file per scrittura
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create file");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    // Buffer per ricevere dati
    char buffer[1024];
    int remaining = req->content_len;
    int received;

    while (remaining > 0) {
        int to_read = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;

        received = httpd_req_recv(req, buffer, to_read);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(f);
            ESP_LOGE(TAG, "File reception failed");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        if (fwrite(buffer, 1, received, f) != received) {
            fclose(f);
            ESP_LOGE(TAG, "Failed to write file");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    fclose(f);

    ESP_LOGI(TAG, "File uploaded successfully: %s", filename);

    // Invia risposta di successo
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "File uploaded successfully");

    return ESP_OK;
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
    config.uri_match_fn = httpd_uri_match_wildcard;  // Abilita wildcard matching

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started successfully");

        // Register URI handlers (ordine importante!)

        // 1. Handler specifici prima
        httpd_uri_t root_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        // 2. WebSocket handler
        httpd_uri_t ws_uri = {
            .uri       = "/ws",
            .method    = HTTP_GET,
            .handler   = ws_handler,
            .user_ctx  = NULL,
            .is_websocket = true
        };
        httpd_register_uri_handler(server, &ws_uri);

        // 3. Handler /update
        httpd_uri_t update_uri = {
            .uri       = "/update",
            .method    = HTTP_GET,
            .handler   = file_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &update_uri);

        // 4. OTA handlers (POST)
        register_ota_handlers(server);

        // 5. File upload handler (POST)
        httpd_uri_t upload_file_uri = {
            .uri       = "/api/upload",
            .method    = HTTP_POST,
            .handler   = file_upload_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &upload_file_uri);

        // 6. Log API handlers (GET)
        register_log_handlers(server);

        // 7. Status API handler (GET)
        register_status_handler(server);

        // 8. Wildcard handler per file statici (DEVE essere ultimo!)
        httpd_uri_t file_uri = {
            .uri       = "/*",
            .method    = HTTP_GET,
            .handler   = file_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &file_uri);

        ESP_LOGI(TAG, "Registered handlers: / /ws /update /ota_* /api/upload /api/log /api/status /* (wildcard)");
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
