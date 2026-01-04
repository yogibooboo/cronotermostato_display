/**
 * @file sensor_simulator.cpp
 * @brief Implementazione simulatore sensore
 */

#include "sensor_simulator.h"
#include "esp_log.h"

static const char* TAG = "SENSOR_SIM";

// Stato globale simulatore
static sensor_sim_state_t s_state = {0};

// Limiti simulazione
#define TEMP_CHANGE_PER_SEC     1       // centesimi di grado per secondo (0.01°C)
#define HUMIDITY_MIN            4000    // 40.00%
#define HUMIDITY_MAX            6000    // 60.00%
#define HUMIDITY_CHANGE_PER_SEC 5       // centesimi per secondo (0.05%)
#define PRESSURE_MIN            99000   // 980.00 hPa
#define PRESSURE_MAX            101000  // 1020.00 hPa
#define PRESSURE_CHANGE_PER_SEC 5       // centesimi per secondo (0.05 hPa)

void sensor_sim_init(float initial_temp, uint8_t initial_humidity,
                     uint16_t initial_pressure, float hysteresis)
{
    s_state.temp_centesimi = (int32_t)(initial_temp * 100);
    s_state.humidity_centesimi = initial_humidity * 100;
    s_state.pressure_centesimi = initial_pressure * 100;

    s_state.humidity_direction = 1;   // Inizia salendo
    s_state.pressure_direction = 1;

    s_state.heater_on = false;
    s_state.current_setpoint = 20.0f;  // Default
    s_state.hysteresis = hysteresis;

    ESP_LOGI(TAG, "Simulator initialized: T=%.2f°C H=%d%% P=%d hPa hyst=%.1f°C",
             initial_temp, initial_humidity, initial_pressure, hysteresis);
}

void sensor_sim_update(void)
{
    // === TEMPERATURA ===
    // Varia di +0.01°C/sec se caldaia accesa, -0.01°C/sec se spenta
    if (s_state.heater_on) {
        s_state.temp_centesimi += TEMP_CHANGE_PER_SEC;
    } else {
        s_state.temp_centesimi -= TEMP_CHANGE_PER_SEC;
    }

    // Limita temperatura a range ragionevole (5-35°C)
    if (s_state.temp_centesimi < 500) s_state.temp_centesimi = 500;
    if (s_state.temp_centesimi > 3500) s_state.temp_centesimi = 3500;

    // === LOGICA TERMOSTATO ===
    float temp_celsius = s_state.temp_centesimi / 100.0f;
    float setpoint = s_state.current_setpoint;
    float hysteresis = s_state.hysteresis;

    if (s_state.heater_on) {
        // Caldaia accesa: spegni quando temperatura > setpoint + isteresi
        if (temp_celsius >= (setpoint + hysteresis)) {
            s_state.heater_on = false;
            ESP_LOGI(TAG, "Heater OFF: T=%.2f°C >= %.2f°C (setpoint %.1f + hyst %.1f)",
                     temp_celsius, setpoint + hysteresis, setpoint, hysteresis);
        }
    } else {
        // Caldaia spenta: accendi quando temperatura < setpoint
        if (temp_celsius < setpoint) {
            s_state.heater_on = true;
            ESP_LOGI(TAG, "Heater ON: T=%.2f°C < %.2f°C (setpoint)",
                     temp_celsius, setpoint);
        }
    }

    // === UMIDITA ===
    // Oscilla tra 40% e 60% con variazione di 0.05%/sec
    s_state.humidity_centesimi += s_state.humidity_direction * HUMIDITY_CHANGE_PER_SEC;

    if (s_state.humidity_centesimi >= HUMIDITY_MAX) {
        s_state.humidity_centesimi = HUMIDITY_MAX;
        s_state.humidity_direction = -1;
    } else if (s_state.humidity_centesimi <= HUMIDITY_MIN) {
        s_state.humidity_centesimi = HUMIDITY_MIN;
        s_state.humidity_direction = 1;
    }

    // === PRESSIONE ===
    // Oscilla tra 980 e 1020 hPa con variazione di 0.05 hPa/sec
    s_state.pressure_centesimi += s_state.pressure_direction * PRESSURE_CHANGE_PER_SEC;

    if (s_state.pressure_centesimi >= PRESSURE_MAX) {
        s_state.pressure_centesimi = PRESSURE_MAX;
        s_state.pressure_direction = -1;
    } else if (s_state.pressure_centesimi <= PRESSURE_MIN) {
        s_state.pressure_centesimi = PRESSURE_MIN;
        s_state.pressure_direction = 1;
    }
}

void sensor_sim_set_setpoint(float setpoint)
{
    if (setpoint != s_state.current_setpoint) {
        ESP_LOGI(TAG, "Setpoint changed: %.1f -> %.1f°C", s_state.current_setpoint, setpoint);
        s_state.current_setpoint = setpoint;
    }
}

float sensor_sim_get_temperature(void)
{
    // Restituisce in decimi di grado (arrotondato)
    return s_state.temp_centesimi / 100.0f;
}

uint8_t sensor_sim_get_humidity(void)
{
    // Restituisce intero (arrotondato)
    return (uint8_t)((s_state.humidity_centesimi + 50) / 100);
}

uint16_t sensor_sim_get_pressure(void)
{
    // Restituisce intero hPa (arrotondato)
    return (uint16_t)((s_state.pressure_centesimi + 50) / 100);
}

bool sensor_sim_get_heater_state(void)
{
    return s_state.heater_on;
}

float sensor_sim_get_setpoint(void)
{
    return s_state.current_setpoint;
}

const sensor_sim_state_t* sensor_sim_get_state(void)
{
    return &s_state;
}
