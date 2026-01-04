/**
 * @file status_api.h
 * @brief API endpoint per stato corrente del termostato
 */

#ifndef STATUS_API_H
#define STATUS_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>
#include <esp_http_server.h>

/**
 * @brief Handler per /api/status
 *
 * Restituisce JSON con valori correnti:
 * - temp: temperatura attuale (°C)
 * - hum: umidità (%)
 * - press: pressione (hPa)
 * - setpoint: soglia attiva (°C)
 * - heat: stato caldaia (0/1)
 * - samples: numero sample nel buffer (per trigger aggiornamento grafico)
 *
 * @param req HTTP request
 * @return ESP_OK on success
 */
esp_err_t status_handler(httpd_req_t *req);

/**
 * @brief Registra handler /api/status
 *
 * @param server HTTP server handle
 * @return ESP_OK on success
 */
esp_err_t register_status_handler(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif // STATUS_API_H
