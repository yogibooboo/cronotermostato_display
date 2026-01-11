/**
 * @file log_reader.cpp
 * @brief Binary log file reader implementation
 */

#include "log_reader.h"
#include "storage_manager.h"
#include "history_manager.h"
#include "comune.h"
#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

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

    // Leggi e invia tutti i records (formato history_sample_t - 12 bytes con pressione)
    uint8_t record[12];
    bool first_record = true;

    for (int i = 0; i < header.num_samples; i++) {
        if (fread(record, 12, 1, f) != 1) {
            ESP_LOGW(TAG, "Failed to read record %d", i);
            break;
        }

        // Parse record (history_sample_t format - 12 bytes)
        uint16_t minute_of_day = record[0] | (record[1] << 8);
        int16_t temp = (int16_t)(record[2] | (record[3] << 8));
        uint8_t hum = record[4];
        uint8_t flags = record[5];
        int16_t setpoint = (int16_t)(record[6] | (record[7] << 8));
        uint16_t pressure = record[10] | (record[11] << 8);

        // Calcola ora/minuto
        int hour = minute_of_day / 60;
        int minute = minute_of_day % 60;

        // Estrai stato caldaia (bit 0 dei flags)
        uint8_t heat = flags & 0x01;

        // Gestisci sentinel values per dati mancanti
        char temp_str[16], setpoint_str[16], hum_str[8], press_str[8];

        if (temp == -32768) {
            strcpy(temp_str, "null");
        } else {
            snprintf(temp_str, sizeof(temp_str), "%.2f", temp / 100.0f);
        }

        if (setpoint == -32768) {
            strcpy(setpoint_str, "null");
        } else {
            snprintf(setpoint_str, sizeof(setpoint_str), "%.2f", setpoint / 100.0f);
        }

        if (hum == 255) {
            strcpy(hum_str, "null");
        } else {
            snprintf(hum_str, sizeof(hum_str), "%d", hum);
        }

        if (pressure == 0) {
            strcpy(press_str, "null");
        } else {
            snprintf(press_str, sizeof(press_str), "%d", pressure);
        }

        snprintf(json_buf, sizeof(json_buf),
                 "%s{\"t\":\"%02d:%02d\",\"temp\":%s,\"hum\":%s,\"heat\":%d,\"setpoint\":%s,\"press\":%s}",
                 first_record ? "" : ",",
                 hour, minute, temp_str, hum_str, heat, setpoint_str, press_str);

        httpd_resp_sendstr_chunk(req, json_buf);
        first_record = false;
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

esp_err_t log_current_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving current day log from PSRAM");

    const history_buffer_t* buffer = history_get_buffer();
    if (buffer == NULL || !buffer->initialized || buffer->samples == NULL) {
        ESP_LOGW(TAG, "History buffer not initialized");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "History not initialized");
        return ESP_FAIL;
    }

    // Imposta header HTTP per binario
    httpd_resp_set_type(req, "application/octet-stream");

    // Costruisci nome file dalla data nel buffer
    char content_disp[128];
    snprintf(content_disp, sizeof(content_disp), "inline; filename=\"log_%04d%02d%02d.bin\"",
             buffer->header.year, buffer->header.month, buffer->header.day);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disp);

    // Invia header (12 bytes)
    if (httpd_resp_send_chunk(req, (const char*)&buffer->header, sizeof(history_header_t)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send header");
        return ESP_FAIL;
    }

    // Invia tutti i sample in chunk (per evitare timeout)
    const size_t chunk_samples = 100;  // 100 sample × 12 bytes = 1200 bytes per chunk
    const history_sample_t* samples = buffer->samples;

    for (size_t i = 0; i < HISTORY_SAMPLES_PER_DAY; i += chunk_samples) {
        size_t remaining = HISTORY_SAMPLES_PER_DAY - i;
        size_t count = (remaining < chunk_samples) ? remaining : chunk_samples;

        if (httpd_resp_send_chunk(req, (const char*)&samples[i],
                                  count * sizeof(history_sample_t)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send samples chunk at %d", i);
            return ESP_FAIL;
        }
    }

    // Fine chunked response
    httpd_resp_send_chunk(req, NULL, 0);

    ESP_LOGI(TAG, "Sent current log: %04d-%02d-%02d (%d valid samples)",
             buffer->header.year, buffer->header.month, buffer->header.day,
             buffer->header.num_samples);

    return ESP_OK;
}

esp_err_t log_list_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Listing available log files");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");

    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open /spiffs directory");
        httpd_resp_sendstr_chunk(req, "]");
        httpd_resp_sendstr_chunk(req, NULL);
        return ESP_OK;
    }

    struct dirent *entry;
    bool first = true;
    char json_buf[64];

    while ((entry = readdir(dir)) != NULL) {
        // Cerca file con pattern log_YYYYMMDD.bin
        if (strncmp(entry->d_name, "log_", 4) == 0 &&
            strstr(entry->d_name, ".bin") != NULL) {

            // Estrai la data dal nome file
            char date_str[9] = {0};
            strncpy(date_str, entry->d_name + 4, 8);

            // Formatta come JSON entry
            snprintf(json_buf, sizeof(json_buf), "%s\"%s\"",
                     first ? "" : ",", date_str);
            httpd_resp_sendstr_chunk(req, json_buf);
            first = false;
        }
    }

    closedir(dir);

    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);

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

    // Registra handler binario (file da SPIFFS)
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

    // Registra handler log corrente (da PSRAM)
    httpd_uri_t log_current_uri = {
        .uri = "/api/log/current",
        .method = HTTP_GET,
        .handler = log_current_handler,
        .user_ctx = NULL
    };

    ret = httpd_register_uri_handler(server, &log_current_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/log/current handler: %s", esp_err_to_name(ret));
        return ret;
    }

    // Registra handler lista log
    httpd_uri_t log_list_uri = {
        .uri = "/api/log/list",
        .method = HTTP_GET,
        .handler = log_list_handler,
        .user_ctx = NULL
    };

    ret = httpd_register_uri_handler(server, &log_list_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/log/list handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Log API handlers registered (JSON + Binary + Current + List)");
    return ESP_OK;
}
