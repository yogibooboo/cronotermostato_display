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
 * @param humidity Current humidity in %
 * @param heater_on Heater state (true = ON, false = OFF)
 */
void display_update_status(float temperature, uint8_t humidity, bool heater_on);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H
