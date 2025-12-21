/**
 * @file log_reader.cpp
 * @brief Binary log file reader implementation
 */

#include "log_reader.h"
#include "storage_manager.h"
#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "LOG_READER";

esp_err_t log_data_handler(httpd_req_t *req)
{
    // Estrai parametro data dalla query string (?date=20231221)
    char date_str[16] = {0};
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;

    if (buf_len > 1) {
        char *buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "date", date_str, sizeof(date_str)) != ESP_OK) {
                strcpy(date_str, "20251221");  // Default today
            }
        }
        free(buf);
    } else {
        strcpy(date_str, "20251221");  // Default
    }

    // Costruisci percorso file
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/spiffs/log_%s.bin", date_str);

    ESP_LOGI(TAG, "Reading log file: %s", filepath);

    // Verifica esistenza file
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGW(TAG, "Log file not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Apri file
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open log file");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open log file");
        return ESP_FAIL;
    }

    // Leggi header
    log_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        ESP_LOGE(TAG, "Failed to read header");
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid log file");
        return ESP_FAIL;
    }

    // Verifica magic number
    if (header.magic != LOG_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic number: 0x%08X", (unsigned int)header.magic);
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid log format");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Log file: %04d-%02d-%02d, %d samples",
             header.year, header.month, header.day, header.num_samples);

    // Invia header HTTP JSON
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{");

    // Metadata
    char json_buf[256];
    snprintf(json_buf, sizeof(json_buf),
             "\"date\":\"%04d-%02d-%02d\",\"samples\":%d,\"data\":[",
             header.year, header.month, header.day, header.num_samples);
    httpd_resp_sendstr_chunk(req, json_buf);

    // Leggi e invia records
    // Per ridurre dimensione, campiona ogni N minuti (es. ogni 5 minuti = 288 punti invece di 1440)
    const int downsample = 5;  // Invia 1 punto ogni 5 minuti
    log_record_t record;

    for (int i = 0; i < header.num_samples; i++) {
        if (fread(&record, sizeof(record), 1, f) != 1) {
            ESP_LOGW(TAG, "Failed to read record %d", i);
            break;
        }

        // Campiona dati
        if (i % downsample == 0) {
            // Calcola ora/minuto
            int hour = i / 60;
            int minute = i % 60;

            // Converti temperatura da decimi a float
            float temp = record.temperature / 10.0f;

            snprintf(json_buf, sizeof(json_buf),
                     "%s{\"t\":\"%02d:%02d\",\"temp\":%.1f,\"hum\":%d,\"heat\":%d}",
                     (i == 0) ? "" : ",",
                     hour, minute, temp, record.humidity, record.heater_state);

            httpd_resp_sendstr_chunk(req, json_buf);
        }
    }

    fclose(f);

    // Chiudi JSON
    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_sendstr_chunk(req, NULL);  // Fine chunked response

    return ESP_OK;
}

esp_err_t log_raw_handler(httpd_req_t *req)
{
    // Estrai parametro data dalla query string (?date=20231221)
    char date_str[16] = {0};
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;

    if (buf_len > 1) {
        char *buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "date", date_str, sizeof(date_str)) != ESP_OK) {
                strcpy(date_str, "20251221");  // Default today
            }
        }
        free(buf);
    } else {
        strcpy(date_str, "20251221");  // Default
    }

    // Costruisci percorso file
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/spiffs/log_%s.bin", date_str);

    ESP_LOGI(TAG, "Reading raw log file: %s", filepath);

    // Verifica esistenza file
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGW(TAG, "Log file not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Apri file
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open log file");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open log file");
        return ESP_FAIL;
    }

    // Imposta header HTTP per binario
    httpd_resp_set_type(req, "application/octet-stream");

    // Aggiungi header per nome file
    char content_disp[128];
    snprintf(content_disp, sizeof(content_disp), "inline; filename=\"log_%s.bin\"", date_str);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disp);

    // Stream diretto del file in chunk da 1KB
    uint8_t buffer[1024];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (httpd_resp_send_chunk(req, (const char*)buffer, bytes_read) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send chunk");
            fclose(f);
            return ESP_FAIL;
        }
    }

    fclose(f);

    // Fine chunked response
    httpd_resp_send_chunk(req, NULL, 0);

    ESP_LOGI(TAG, "Sent raw log file successfully");
    return ESP_OK;
}

esp_err_t register_log_handlers(httpd_handle_t server)
{
    if (!server) {
        ESP_LOGE(TAG, "Server handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Registra handler JSON
    httpd_uri_t log_data_uri = {
        .uri = "/api/log",
        .method = HTTP_GET,
        .handler = log_data_handler,
        .user_ctx = NULL
    };

    esp_err_t ret = httpd_register_uri_handler(server, &log_data_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/log handler: %s", esp_err_to_name(ret));
        return ret;
    }

    // Registra handler binario
    httpd_uri_t log_raw_uri = {
        .uri = "/api/log/raw",
        .method = HTTP_GET,
        .handler = log_raw_handler,
        .user_ctx = NULL
    };

    ret = httpd_register_uri_handler(server, &log_raw_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/log/raw handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Log API handlers registered (JSON + Binary)");
    return ESP_OK;
}
