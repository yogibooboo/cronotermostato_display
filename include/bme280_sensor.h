/**
 * @file bme280_sensor.h
 * @brief Driver BME280 per temperatura, umidità e pressione
 */

#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Dati letti dal sensore BME280
 */
typedef struct {
    float temperature;      // Temperatura in °C
    float humidity;         // Umidità relativa in %
    float pressure;         // Pressione in hPa
    bool valid;             // true se lettura valida
} bme280_data_t;

/**
 * @brief Inizializza il sensore BME280
 *
 * Configura I2C e verifica presenza sensore.
 *
 * @return ESP_OK se successo, ESP_FAIL se sensore non trovato
 */
esp_err_t bme280_init(void);

/**
 * @brief Legge temperatura, umidità e pressione
 *
 * @param data Puntatore alla struttura dove salvare i dati
 * @return ESP_OK se lettura valida
 */
esp_err_t bme280_read(bme280_data_t* data);

/**
 * @brief Verifica se il sensore è inizializzato e funzionante
 *
 * @return true se sensore OK
 */
bool bme280_is_ready(void);

/**
 * @brief Deinizializza il sensore e libera risorse I2C
 */
void bme280_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // BME280_SENSOR_H
