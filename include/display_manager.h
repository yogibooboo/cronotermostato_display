/**
 * @file display_manager.h
 * @brief Display and LVGL management for JC3248W535EN module
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize display and LVGL
 *
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t display_init(void);

/**
 * @brief Update display with current thermostat status
 *
 * @param temperature Current temperature in °C
 * @param hour Current hour (0-23)
 * @param minute Current minute (0-59)
 * @param second Current second (0-59)
 * @param heater_on Heater state (true = ON, false = OFF)
 */
void display_update_status(float temperature, uint8_t hour, uint8_t minute, uint8_t second, bool heater_on);

/**
 * @brief Set the daily program to display on chart
 *
 * @param setpoints Array of 48 temperature setpoints (one per half-hour)
 * @param count Number of setpoints (should be 48)
 */
void display_set_program(const float* setpoints, int count);

/**
 * @brief Load temperature history from today's buffer into the chart
 *
 * Should be called after history_init() to display historical data
 */
void display_load_history_temperatures(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H
