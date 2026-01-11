/**
 * @file status_api.cpp
 * @brief API endpoint per stato corrente del termostato
 */

#include "status_api.h"
#include "comune.h"
#include "history_manager.h"
#include <esp_log.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "STATUS_API";

// Riferimento a stato e config globali definiti in main.cpp
extern thermostat_state_t g_state;
extern thermostat_config_t g_config;

esp_err_t status_handler(httpd_req_t *req)
{
    // Ottieni contatore sample dal buffer history
    uint16_t sample_count = history_get_sample_count();

    // Ottieni data/ora corrente
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Costruisci risposta JSON con data/ora
    char json[192];
    int len = snprintf(json, sizeof(json),
        "{\"temp\":%.2f,\"hum\":%d,\"press\":%.1f,\"setpoint\":%.1f,\"heat\":%d,\"samples\":%d,"
        "\"year\":%d,\"month\":%d,\"day\":%d,\"hour\":%d,\"min\":%d,\"sec\":%d,\"wday\":%d}",
        g_state.current_temperature,
        g_state.current_humidity,
        g_state.current_pressure / 10.0f,  // Converti da decimi a hPa
        g_state.active_setpoint,
        g_state.relay_state ? 1 : 0,
        sample_count,
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec,
        timeinfo.tm_wday
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, len);

    return ESP_OK;
}

/**
 * @brief Handler per /api/program - restituisce i 48 slot del programma attivo
 */
esp_err_t program_handler(httpd_req_t *req)
{
    // Per ora usa sempre il programma 0 (Standard)
    // TODO: in futuro considerare program_mode, weekly_banks, eccezioni
    int bank = 0;

    // Costruisci JSON array con 48 valori
    // Formato: {"bank":0,"slots":[21.0,21.0,20.5,...]}
    httpd_resp_set_type(req, "application/json");

    char buf[32];
    snprintf(buf, sizeof(buf), "{\"bank\":%d,\"slots\":[", bank);
    httpd_resp_sendstr_chunk(req, buf);

    for (int i = 0; i < SLOTS_PER_DAY; i++) {
        snprintf(buf, sizeof(buf), "%s%.1f",
                 i > 0 ? "," : "",
                 g_config.programs[bank][i]);
        httpd_resp_sendstr_chunk(req, buf);
    }

    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_sendstr_chunk(req, NULL);  // Fine chunked

    return ESP_OK;
}

esp_err_t register_status_handler(httpd_handle_t server)
{
    if (!server) {
        ESP_LOGE(TAG, "Server handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Handler /api/status
    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = NULL
    };

    esp_err_t ret = httpd_register_uri_handler(server, &status_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/status: %s", esp_err_to_name(ret));
        return ret;
    }

    // Handler /api/program
    httpd_uri_t program_uri = {
        .uri = "/api/program",
        .method = HTTP_GET,
        .handler = program_handler,
        .user_ctx = NULL
    };

    ret = httpd_register_uri_handler(server, &program_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/program: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Registered /api/status and /api/program handlers");
    return ESP_OK;
}
