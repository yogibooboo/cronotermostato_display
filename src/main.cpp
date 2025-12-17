/**
 * @file main.cpp
 * @brief Cronotermostato ESP32-S3 - Main application entry point
 *
 * Hardware: JC3248W535EN (ESP32-S3 + Display 3.5" Touch)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include "comune.h"
#include "wifi.h"
#include "storage_manager.h"

static const char *TAG = "MAIN";

// ============================================================================
// Global state
// ============================================================================

static thermostat_config_t g_config;
static thermostat_state_t g_state;

// ============================================================================
// Tasks
// ============================================================================

/**
 * @brief Task principale del termostato
 *
 * Legge sensore, calcola soglia, controlla relè
 */
static void thermostat_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Thermostat task started on core %d", xPortGetCoreID());

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(60000);  // 1 minuto

    while (1) {
        // TODO: Lettura sensore temperatura/umidità
        // TODO: Calcolo setpoint attivo
        // TODO: Controllo relè con isteresi
        // TODO: Salvataggio sample nello storico

        ESP_LOGI(TAG, "Thermostat tick - Free heap: %lu bytes", esp_get_free_heap_size());

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

/**
 * @brief Task di diagnostica (opzionale)
 */
static void diag_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Diagnostics task started on core %d", xPortGetCoreID());

    while (1) {
        // Sistema info periodico
        ESP_LOGI(TAG, "--- System Status ---");
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Free PSRAM: %lu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI(TAG, "Uptime: %llu seconds", esp_timer_get_time() / 1000000ULL);

        vTaskDelay(pdMS_TO_TICKS(30000));  // Ogni 30 secondi
    }
}

// ============================================================================
// Initialization functions
// ============================================================================

/**
 * @brief Inizializzazione NVS flash
 */
static esp_err_t init_nvs(void)
{
    ESP_LOGI(TAG, "Initializing NVS...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS initialized successfully");
    }

    return ret;
}

/**
 * @brief Stampa informazioni sistema
 */
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   CRONOTERMOSTATO ESP32-S3 - JC3248W535EN Module      ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Chip: %s", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "Features: %s%s%s%s",
             chip_info.features & CHIP_FEATURE_WIFI_BGN ? "WiFi/" : "",
             chip_info.features & CHIP_FEATURE_BT ? "BT/" : "",
             chip_info.features & CHIP_FEATURE_BLE ? "BLE/" : "",
             chip_info.features & CHIP_FEATURE_EMB_FLASH ? "Embedded-Flash" : "External-Flash");
    ESP_LOGI(TAG, "Flash: %lu MB %s", flash_size / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    ESP_LOGI(TAG, "PSRAM: %lu bytes", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "");
}

/**
 * @brief Inizializza configurazione di default
 */
static void init_default_config(void)
{
    ESP_LOGI(TAG, "Initializing default configuration...");

    memset(&g_config, 0, sizeof(thermostat_config_t));

    // Stato sistema
    g_config.system_enabled = true;
    g_config.manual_mode = false;
    g_config.manual_setpoint = 20.0f;
    g_config.temp_correction = 0.0f;
    g_config.hysteresis = DEFAULT_HYSTERESIS;

    // Modalità programmazione
    g_config.program_mode = 0;  // Programma unico
    g_config.single_program_bank = 0;

    // Eccezione disabilitata
    g_config.exception_enabled = false;

    // GPIO
    g_config.relay_gpio = RELAY_GPIO;
    g_config.sensor_sda = SENSOR_SDA;
    g_config.sensor_scl = SENSOR_SCL;

    // Programmi di default (18°C tutto il giorno per tutti i banchi)
    for (int bank = 0; bank < PROGRAMS_COUNT; bank++) {
        snprintf(g_config.program_names[bank], PROGRAM_NAME_LEN,
                 "Programma %d", bank + 1);

        for (int slot = 0; slot < SLOTS_PER_DAY; slot++) {
            g_config.programs[bank][slot] = 18.0f;
        }
    }

    // Inizializza stato runtime
    memset(&g_state, 0, sizeof(thermostat_state_t));
    g_state.current_temperature = 20.0f;
    g_state.current_humidity = 50;
    g_state.relay_state = false;

    ESP_LOGI(TAG, "Default configuration initialized");
}

// ============================================================================
// Main application entry point
// ============================================================================

extern "C" void app_main(void)
{
    // Stampa info sistema
    print_system_info();

    // Inizializza NVS
    ESP_ERROR_CHECK(init_nvs());

    // Inizializza configurazione di default
    init_default_config();

    // Inizializza SPIFFS
    ESP_LOGI(TAG, "Initializing SPIFFS...");
    ESP_ERROR_CHECK(storage_init());

    // TODO: Carica configurazione salvata
    // config_load(&g_config);

    // TODO: Inizializza history manager
    // ESP_ERROR_CHECK(history_init());

    // Inizializza WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    setup_wifi();

    // TODO: Inizializza sincronizzazione tempo
    // time_sync_setup_timezone();
    // time_sync_start();

    // TODO: Inizializza console/telnet
    // console_start();
    // telnet_start();

    // TODO: Inizializza web server
    // http_server_start();

    // TODO: Inizializza sensore temperatura
    // sensor_init();

    // TODO: Inizializza display + touch
    // display_init();
    // touch_init();

    ESP_LOGI(TAG, "Starting tasks...");

    // Crea task termostato (CORE 0)
    xTaskCreatePinnedToCore(
        thermostat_task,
        "thermostat",
        4096,
        NULL,
        5,  // Priorità alta
        NULL,
        0   // Core 0
    );

    // Crea task diagnostica (CORE 1)
    xTaskCreatePinnedToCore(
        diag_task,
        "diagnostics",
        3072,
        NULL,
        1,  // Priorità bassa
        NULL,
        1   // Core 1
    );

    ESP_LOGI(TAG, "Initialization complete. System running.");
}
