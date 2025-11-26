/**
 * @file wifi.cpp
 * @brief WiFi management and WebSocket handling for Cronotermostato
 */

#include "wifi.h"
#include "http_server.h"
#include "comune.h"
#include "credentials.h"

#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <driver/gpio.h>

static const char *TAG = "WIFI";

// Forward declarations for functions not yet implemented
// void telnet_start(int port);      // TODO: implement when we add telnet
// void telnet_stop(void);           // TODO
// void time_sync_start(void);       // TODO: implement when we add time_sync
// void time_sync_stop(void);        // TODO

// ============================================================================
// WebSocket Client Management
// ============================================================================

#define MAX_WS_CLIENTS 4
static int g_ws_client_fds[MAX_WS_CLIENTS] = {-1, -1, -1, -1};
static int g_ws_client_count = 0;

void register_ws_client(int fd) {
    if (g_ws_client_count < MAX_WS_CLIENTS) {
        g_ws_client_fds[g_ws_client_count] = fd;
        g_ws_client_count++;
        ESP_LOGI(TAG, "WebSocket client registered, total: %d", g_ws_client_count);
    } else {
        ESP_LOGW(TAG, "Max WebSocket clients reached, cannot register fd=%d", fd);
    }
}

static int find_ws_client_by_fd(int fd) {
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (g_ws_client_fds[i] == fd) {
            return i;
        }
    }
    return -1;
}

void unregister_ws_client(int fd) {
    int i = find_ws_client_by_fd(fd);
    if (i != -1) {
        ESP_LOGI(TAG, "WebSocket client unregistered, fd=%d, total: %d", fd, g_ws_client_count - 1);
        g_ws_client_fds[i] = -1;
        g_ws_client_count--;
    }
}

// ============================================================================
// Socket Validation
// ============================================================================

static bool is_socket_valid(int fd) {
    if (fd < 0) return false;

    int socket_error = 0;
    socklen_t len = sizeof(socket_error);

    // Check if socket has errors
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &len) != 0) {
        return false;
    }

    if (socket_error != 0) {
        return false;
    }

    // Additional test: try non-blocking send of 0 bytes
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        ssize_t result = send(fd, NULL, 0, MSG_NOSIGNAL);
        fcntl(fd, F_SETFL, flags); // Restore original flags

        if (result < 0 && (errno == ECONNRESET || errno == EPIPE || errno == ENOTCONN)) {
            return false;
        }
    }

    return true;
}

void websocket_cleanup_dead_connections(void) {
    for (int i = 0; i < g_ws_client_count; i++) {
        if (g_ws_client_fds[i] >= 0 && !is_socket_valid(g_ws_client_fds[i])) {
            ESP_LOGW(TAG, "Detected invalid WebSocket, fd=%d", g_ws_client_fds[i]);

            struct linger linger = { .l_onoff = 1, .l_linger = 0 };
            setsockopt(g_ws_client_fds[i], SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
            shutdown(g_ws_client_fds[i], SHUT_RDWR);
            close(g_ws_client_fds[i]);

            g_ws_client_fds[i] = -1;

            // Compact array
            for (int j = i; j < g_ws_client_count - 1; j++) {
                g_ws_client_fds[j] = g_ws_client_fds[j + 1];
            }
            g_ws_client_count--;
            i--;
        }
    }
}

// Cleanup task - runs every 30 minutes
static void websocket_cleanup_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1800000)); // 30 minutes
        ESP_LOGI(TAG, "Running periodic WebSocket cleanup...");
        websocket_cleanup_dead_connections();
    }
}

// ============================================================================
// WebSocket Handler (to be called by http_server)
// ============================================================================

esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake initiated");
        return ESP_OK;
    }

    // TODO: Implement WebSocket message handling here
    // For now, just a placeholder

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // First, receive the frame metadata
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %d", ret);
        return ret;
    }

    // Allocate buffer for payload
    if (ws_pkt.len > 0) {
        uint8_t *buf = (uint8_t *)malloc(ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for WebSocket payload");
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame payload failed: %d", ret);
            free(buf);
            return ret;
        }

        buf[ws_pkt.len] = '\0'; // Null-terminate

        ESP_LOGI(TAG, "Received WebSocket message: %s", buf);

        // TODO: Process message here (config updates, commands, etc.)

        free(buf);
    }

    return ESP_OK;
}

// ============================================================================
// WiFi Tracking and Diagnostics
// ============================================================================

static bool wifi_connected = false;
static int8_t wifi_rssi = 0;
static uint32_t wifi_reconnect_count = 0;
static uint8_t last_disconnect_reason = 0; // WIFI_REASON_UNSPECIFIED
static time_t last_disconnect_time = 0;
static time_t connection_start_time = 0;
static uint32_t total_disconnect_count = 0;

static void wifi_track_connection_start(void) {
    time(&connection_start_time);
    ESP_LOGI(TAG, "Connection tracking started");
}

static void wifi_track_disconnect(uint8_t reason) {
    last_disconnect_reason = reason;
    time(&last_disconnect_time);
    total_disconnect_count++;
    ESP_LOGI(TAG, "Disconnect tracking #%lu, reason: %d", total_disconnect_count, reason);
}

static void wifi_track_reconnect(void) {
    wifi_reconnect_count++;
    wifi_track_connection_start();
    ESP_LOGI(TAG, "Reconnect tracking #%lu", wifi_reconnect_count);
}

// ============================================================================
// WiFi Event Handler
// ============================================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi STA started, connecting...");

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        wifi_rssi = 0;

        // Reset WebSocket client count
        g_ws_client_count = 0;

        // Turn off WiFi LED (active LOW - set to 1)
        #ifdef LED_WIFI
        gpio_set_level(LED_WIFI, 1);
        #endif

        // Track disconnection
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        wifi_track_disconnect(disconnected->reason);

        esp_wifi_connect();
        ESP_LOGW(TAG, "WiFi connection lost, reconnecting...");

        // Track reconnection attempt
        wifi_track_reconnect();

        // Stop services
        stop_webserver();
        // telnet_stop();      // TODO
        // time_sync_stop();   // TODO

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;

        // Track connection established
        wifi_track_connection_start();

        // Start telnet server
        // telnet_start(2323);  // TODO

        // Get WiFi RSSI
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            wifi_rssi = ap_info.rssi;
            ESP_LOGI(TAG, "WiFi RSSI: %d dBm", wifi_rssi);
        }

        // Turn on WiFi LED (active LOW - set to 0)
        #ifdef LED_WIFI
        gpio_set_level(LED_WIFI, 0);
        #endif

        // Start NTP time sync
        // time_sync_start();  // TODO

        // Start web server (if not already running)
        start_webserver_if_not_running();
    }
}

// ============================================================================
// WiFi Setup
// ============================================================================

void setup_wifi(void) {
    // Initialize WiFi LED GPIO (if defined)
    #ifdef LED_WIFI
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << LED_WIFI);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(LED_WIFI, 1);  // Start with LED off (active LOW)
    ESP_LOGI(TAG, "WiFi LED initialized on GPIO %d (active LOW)", LED_WIFI);
    #endif

    // NVS initialization (may have been done in main, but safe to call again)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS error, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Configure WiFi
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Reset tracking variables
    connection_start_time = 0;
    last_disconnect_time = 0;
    total_disconnect_count = 0;
    wifi_reconnect_count = 0;

    ESP_LOGI(TAG, "WiFi initialization complete. SSID: %s", WIFI_SSID);

    // Start WebSocket cleanup task
    xTaskCreate(websocket_cleanup_task, "ws_cleanup", 3072, NULL, 1, NULL);
    ESP_LOGI(TAG, "WebSocket cleanup task started (runs every 30 minutes)");
}

// ============================================================================
// WiFi Status Diagnostics
// ============================================================================

extern "C" void get_wifi_status(char* buffer, size_t buffer_size, const char* subcommand) {
    if (!subcommand || strlen(subcommand) == 0) {
        time_t now;
        time(&now);
        struct tm *tm_now = localtime(&now);
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_now);

        // Basic configuration info
        char mac_str[18] = "N/A";
        uint8_t mac[6];
        if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }

        char ip_str[16] = "N/A";
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            }
        }

        // RSSI percentage and bar graph
        int rssi_percent = 0;
        if (wifi_rssi >= -50) rssi_percent = 100;
        else if (wifi_rssi >= -60) rssi_percent = 80;
        else if (wifi_rssi >= -70) rssi_percent = 60;
        else if (wifi_rssi >= -80) rssi_percent = 40;
        else if (wifi_rssi >= -90) rssi_percent = 20;
        else rssi_percent = 10;

        char rssi_bar[11];
        int bars = rssi_percent / 10;
        for (int i = 0; i < 10; i++) {
            rssi_bar[i] = (i < bars) ? '|' : '.';
        }
        rssi_bar[10] = '\0';

        // Connection duration
        uint32_t uptime_sec = 0;
        if (connection_start_time > 0) {
            uptime_sec = (uint32_t)(now - connection_start_time);
        }
        uint32_t hours = uptime_sec / 3600;
        uint32_t minutes = (uptime_sec % 3600) / 60;
        uint32_t seconds = uptime_sec % 60;

        // Format output
        snprintf(buffer, buffer_size,
                 "=== WiFi Status (%s) ===\n"
                 "Estado: %s\n"
                 "SSID: %s\n"
                 "IP: %s\n"
                 "MAC: %s\n"
                 "RSSI: %d dBm (%d%%) [%s]\n"
                 "Uptime: %luh %lum %lus\n"
                 "Reconnects: %lu\n"
                 "Total disconnects: %lu\n"
                 "WebSocket clients: %d\n",
                 time_str,
                 wifi_connected ? "Connected" : "Disconnected",
                 WIFI_SSID,
                 ip_str,
                 mac_str,
                 wifi_rssi, rssi_percent, rssi_bar,
                 hours, minutes, seconds,
                 wifi_reconnect_count,
                 total_disconnect_count,
                 g_ws_client_count);
    } else {
        snprintf(buffer, buffer_size, "Unknown WiFi subcommand: %s\n", subcommand);
    }
}
