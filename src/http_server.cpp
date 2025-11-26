/**
 * @file http_server.cpp
 * @brief HTTP server implementation for Cronotermostato
 */

#include "http_server.h"
#include "wifi.h"
#include "comune.h"

#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>

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
 * @brief Root handler - Hello World page
 */
static esp_err_t root_handler(httpd_req_t *req) {
    const char* html =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset='UTF-8'>\n"
        "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
        "    <title>Cronotermostato ESP32-S3</title>\n"
        "    <style>\n"
        "        body {\n"
        "            font-family: Arial, sans-serif;\n"
        "            max-width: 800px;\n"
        "            margin: 50px auto;\n"
        "            padding: 20px;\n"
        "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n"
        "            color: white;\n"
        "        }\n"
        "        .container {\n"
        "            background: rgba(255, 255, 255, 0.1);\n"
        "            border-radius: 15px;\n"
        "            padding: 30px;\n"
        "            backdrop-filter: blur(10px);\n"
        "            box-shadow: 0 8px 32px 0 rgba(31, 38, 135, 0.37);\n"
        "        }\n"
        "        h1 {\n"
        "            text-align: center;\n"
        "            font-size: 2.5em;\n"
        "            margin-bottom: 10px;\n"
        "        }\n"
        "        .subtitle {\n"
        "            text-align: center;\n"
        "            font-size: 1.2em;\n"
        "            opacity: 0.9;\n"
        "            margin-bottom: 30px;\n"
        "        }\n"
        "        .info {\n"
        "            background: rgba(255, 255, 255, 0.15);\n"
        "            border-radius: 10px;\n"
        "            padding: 20px;\n"
        "            margin: 20px 0;\n"
        "        }\n"
        "        .info h3 {\n"
        "            margin-top: 0;\n"
        "            border-bottom: 2px solid rgba(255, 255, 255, 0.3);\n"
        "            padding-bottom: 10px;\n"
        "        }\n"
        "        .status {\n"
        "            display: inline-block;\n"
        "            padding: 5px 15px;\n"
        "            background: #4CAF50;\n"
        "            border-radius: 20px;\n"
        "            font-weight: bold;\n"
        "        }\n"
        "        ul {\n"
        "            list-style: none;\n"
        "            padding: 0;\n"
        "        }\n"
        "        li {\n"
        "            padding: 8px 0;\n"
        "            border-bottom: 1px solid rgba(255, 255, 255, 0.2);\n"
        "        }\n"
        "        li:last-child {\n"
        "            border-bottom: none;\n"
        "        }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class='container'>\n"
        "        <h1>🌡️ Cronotermostato</h1>\n"
        "        <div class='subtitle'>ESP32-S3 + Display Touch 3.5\"</div>\n"
        "        \n"
        "        <div class='info'>\n"
        "            <h3>Sistema</h3>\n"
        "            <ul>\n"
        "                <li><strong>Status:</strong> <span class='status'>Online</span></li>\n"
        "                <li><strong>Hardware:</strong> JC3248W535EN Module</li>\n"
        "                <li><strong>MCU:</strong> ESP32-S3 @ 240MHz (Dual Core)</li>\n"
        "                <li><strong>Flash:</strong> 16 MB</li>\n"
        "                <li><strong>PSRAM:</strong> 8 MB</li>\n"
        "            </ul>\n"
        "        </div>\n"
        "\n"
        "        <div class='info'>\n"
        "            <h3>Funzionalità</h3>\n"
        "            <ul>\n"
        "                <li>✅ WiFi connesso</li>\n"
        "                <li>✅ WebSocket server</li>\n"
        "                <li>✅ HTTP server</li>\n"
        "                <li>🚧 Display touch (in sviluppo)</li>\n"
        "                <li>🚧 Controllo temperatura (in sviluppo)</li>\n"
        "                <li>🚧 Programmazione oraria (in sviluppo)</li>\n"
        "            </ul>\n"
        "        </div>\n"
        "\n"
        "        <div class='info'>\n"
        "            <h3>WebSocket Endpoint</h3>\n"
        "            <ul>\n"
        "                <li><strong>URL:</strong> ws://\" + window.location.hostname + \"/ws</li>\n"
        "                <li><strong>Clients connessi:</strong> <span id='ws-clients'>0</span></li>\n"
        "            </ul>\n"
        "        </div>\n"
        "    </div>\n"
        "\n"
        "    <script>\n"
        "        // Test WebSocket connection\n"
        "        const ws = new WebSocket('ws://' + window.location.hostname + '/ws');\n"
        "        \n"
        "        ws.onopen = function() {\n"
        "            console.log('WebSocket connected');\n"
        "        };\n"
        "        \n"
        "        ws.onmessage = function(event) {\n"
        "            console.log('WebSocket message:', event.data);\n"
        "        };\n"
        "        \n"
        "        ws.onerror = function(error) {\n"
        "            console.error('WebSocket error:', error);\n"
        "        };\n"
        "    </script>\n"
        "</body>\n"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
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
