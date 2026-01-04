/**
 * @file sensor_simulator.h
 * @brief Simulatore sensore temperatura/umidità/pressione per testing
 *
 * Simula il comportamento di un sensore reale con:
 * - Temperatura che varia in base allo stato caldaia (+/- 0.01°C/sec)
 * - Logica termostato con isteresi
 * - Umidità oscillante 40-60%
 * - Pressione oscillante 980-1020 hPa
 */

#ifndef SENSOR_SIMULATOR_H
#define SENSOR_SIMULATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Stato interno del simulatore
 */
typedef struct {
    // Valori simulati (alta precisione interna)
    int32_t temp_centesimi;      // Temperatura in centesimi di grado (es. 2050 = 20.50°C)
    int32_t humidity_centesimi;  // Umidità in centesimi (es. 5000 = 50.00%)
    int32_t pressure_centesimi;  // Pressione in centesimi (es. 101300 = 1013.00 hPa)

    // Direzione oscillazione
    int8_t humidity_direction;   // +1 o -1
    int8_t pressure_direction;   // +1 o -1

    // Stato caldaia simulata
    bool heater_on;

    // Setpoint corrente (da programma)
    float current_setpoint;

    // Configurazione
    float hysteresis;
} sensor_sim_state_t;

/**
 * @brief Inizializza il simulatore
 *
 * @param initial_temp Temperatura iniziale in °C
 * @param initial_humidity Umidità iniziale in %
 * @param initial_pressure Pressione iniziale in hPa
 * @param hysteresis Isteresi termostato in °C
 */
void sensor_sim_init(float initial_temp, uint8_t initial_humidity,
                     uint16_t initial_pressure, float hysteresis);

/**
 * @brief Aggiorna la simulazione (chiamare ogni secondo)
 *
 * Aggiorna temperatura, umidità e pressione secondo le regole di simulazione.
 * La temperatura varia di +0.01°C/sec se caldaia accesa, -0.01°C/sec se spenta.
 * La caldaia si accende sotto setpoint, si spegne sopra setpoint+isteresi.
 */
void sensor_sim_update(void);

/**
 * @brief Imposta il setpoint corrente (da programma orario)
 *
 * @param setpoint Temperatura target in °C
 */
void sensor_sim_set_setpoint(float setpoint);

/**
 * @brief Ottiene la temperatura simulata
 *
 * @return Temperatura in °C (precisione 0.1°C per visualizzazione)
 */
float sensor_sim_get_temperature(void);

/**
 * @brief Ottiene l'umidità simulata
 *
 * @return Umidità in % (0-100)
 */
uint8_t sensor_sim_get_humidity(void);

/**
 * @brief Ottiene la pressione simulata
 *
 * @return Pressione in hPa
 */
uint16_t sensor_sim_get_pressure(void);

/**
 * @brief Ottiene lo stato caldaia
 *
 * @return true se caldaia accesa
 */
bool sensor_sim_get_heater_state(void);

/**
 * @brief Ottiene il setpoint corrente
 *
 * @return Setpoint in °C
 */
float sensor_sim_get_setpoint(void);

/**
 * @brief Ottiene lo stato completo del simulatore (per debug)
 *
 * @return Puntatore allo stato interno (read-only)
 */
const sensor_sim_state_t* sensor_sim_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_SIMULATOR_H
