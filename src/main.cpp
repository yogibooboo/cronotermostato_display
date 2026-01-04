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
#include "time_sync.h"
#include "history_manager.h"
#include "sensor_simulator.h"
#include "esp_wifi.h"
#include "display_manager.h"

static const char *TAG = "MAIN";

// ============================================================================
// Global state
// ============================================================================

thermostat_config_t g_config;   // Non-static per accesso da status_api.cpp
thermostat_state_t g_state;     // Non-static per accesso da status_api.cpp

// ============================================================================
// Tasks
// ============================================================================

/**
 * @brief Calcola il setpoint corrente dal programma 0 (Standard)
 *
 * Usa slot da 30 minuti (48 slot/giorno).
 * I valori sono in decimi di grado (es. 210 = 21.0°C).
 */
static float get_current_setpoint(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Calcola slot corrente (0-47)
    int minute_of_day = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int slot = minute_of_day / 30;  // 30 minuti per slot

    if (slot >= SLOTS_PER_DAY) slot = SLOTS_PER_DAY - 1;

    // Leggi setpoint dal programma 0 (Standard)
    float setpoint = g_config.programs[0][slot];

    return setpoint;
}

/**
 * @brief Task di status (ogni secondo)
 *
 * Aggiorna simulazione sensore, display e registra sample allo scoccare del minuto.
 */
static void status_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Status task started on core %d", xPortGetCoreID());

    // Wait for WiFi to be ready
    vTaskDelay(pdMS_TO_TICKS(2000));

    static int last_minute = -1;  // Per rilevare cambio minuto
    static int save_counter = 0;  // Contatore per salvataggio su flash

    while (1) {
        // Ottieni ora corrente
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

        // Calcola setpoint dal programma orario
        float setpoint = get_current_setpoint();
        sensor_sim_set_setpoint(setpoint);

        // Aggiorna simulazione sensore (ogni secondo)
        sensor_sim_update();

        // Leggi valori simulati
        float temperature = sensor_sim_get_temperature();
        uint8_t humidity = sensor_sim_get_humidity();
        uint16_t pressure = sensor_sim_get_pressure();
        bool heater_on = sensor_sim_get_heater_state();

        // Aggiorna stato globale
        g_state.current_temperature = temperature;
        g_state.current_humidity = humidity;
        g_state.current_pressure = pressure;
        g_state.relay_state = heater_on;
        g_state.active_setpoint = setpoint;
        g_state.active_bank = 0;  // Programma 0 (Standard)

        // Rileva cambio minuto - registra sample esattamente allo scoccare
        int current_minute = timeinfo.tm_min;
        if (current_minute != last_minute && last_minute >= 0) {
            // Nuovo minuto! Registra sample
            esp_err_t ret = history_record(temperature, humidity, pressure,
                                           setpoint, heater_on, g_config.manual_mode, 0);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Sample recorded at %02d:%02d - T=%.1f°C H=%d%% P=%d hPa",
                         timeinfo.tm_hour, timeinfo.tm_min,
                         temperature, humidity, pressure);
            }

            // Salva su flash ogni ora (60 minuti)
            if (++save_counter >= 60) {
                save_counter = 0;
                history_save_to_file();
            }
        }
        last_minute = current_minute;

        // Get WiFi RSSI
        wifi_ap_record_t ap_info;
        int8_t rssi = 0;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            rssi = ap_info.rssi;
        }

        // Free heap
        uint32_t free_heap = esp_get_free_heap_size();

        // Update display with current status
        display_update_status(temperature, humidity, pressure, heater_on);

        // Print status line
        printf("[%s] WiFi:%ddBm T:%.2f°C H:%d%% P:%dhPa Set:%.1f°C %s Heap:%lu\n",
               time_str, rssi, temperature, humidity, pressure, setpoint,
               heater_on ? "ON" : "OFF", free_heap);

        vTaskDelay(pdMS_TO_TICKS(1000));  // Ogni secondo
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
    ESP_LOGI(TAG, "Flash: %u MB %s", (unsigned int)(flash_size / (1024 * 1024)),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    ESP_LOGI(TAG, "PSRAM: %u bytes", (unsigned int)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Free heap: %u bytes", (unsigned int)esp_get_free_heap_size());
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

    // Programma 0: Standard (da config.json)
    // Slot 0-13 (00:00-06:59): 16°C - Notte
    // Slot 14-15 (07:00-07:59): 19°C - Risveglio
    // Slot 16-33 (08:00-16:59): 21°C - Giorno
    // Slot 34-35 (17:00-17:59): 20°C - Sera
    // Slot 36-39 (18:00-19:59): 19°C
    // Slot 40-43 (20:00-21:59): 18°C
    // Slot 44-47 (22:00-23:59): 16°C - Notte
    snprintf(g_config.program_names[0], PROGRAM_NAME_LEN, "Standard");
    const float prog0[] = {
        16.0f, 16.0f, 16.0f, 16.0f, 16.0f, 16.0f, 16.0f, 16.0f,  // 00:00-03:59
        16.0f, 16.0f, 16.0f, 16.0f, 16.0f, 16.0f,                // 04:00-06:59
        19.0f, 19.0f,                                             // 07:00-07:59
        21.0f, 21.0f, 21.0f, 21.0f, 21.0f, 21.0f, 21.0f, 21.0f,  // 08:00-11:59
        21.0f, 21.0f, 21.0f, 21.0f, 21.0f, 21.0f, 21.0f, 21.0f,  // 12:00-15:59
        21.0f, 21.0f,                                             // 16:00-16:59
        20.0f, 20.0f,                                             // 17:00-17:59
        19.0f, 19.0f, 19.0f, 19.0f,                               // 18:00-19:59
        18.0f, 18.0f, 17.0f, 17.0f,                               // 20:00-21:59
        16.0f, 16.0f, 16.0f, 16.0f                                // 22:00-23:59
    };
    for (int slot = 0; slot < SLOTS_PER_DAY; slot++) {
        g_config.programs[0][slot] = prog0[slot];
    }

    // Altri programmi: default 18°C
    for (int bank = 1; bank < PROGRAMS_COUNT; bank++) {
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
    g_state.current_pressure = 1013;  // hPa standard
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

    // Inizializza WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    setup_wifi();

    // Inizializza sincronizzazione tempo NTP
    ESP_LOGI(TAG, "Starting NTP time sync...");
    time_sync_start();

    // Aspetta sincronizzazione NTP prima di inizializzare history
    // (necessario per avere la data corretta)
    ESP_LOGI(TAG, "Waiting for NTP sync...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Inizializza history manager (buffer in PSRAM)
    ESP_LOGI(TAG, "Initializing history manager...");
    esp_err_t hist_ret = history_init();
    if (hist_ret != ESP_OK) {
        ESP_LOGW(TAG, "History manager init failed: %s", esp_err_to_name(hist_ret));
    }

    // TODO: Inizializza console/telnet
    // console_start();
    // telnet_start();

    // TODO: Inizializza web server
    // http_server_start();

    // Inizializza simulatore sensore (per testing senza hardware)
    ESP_LOGI(TAG, "Initializing sensor simulator...");
    sensor_sim_init(
        18.0f,                  // Temperatura iniziale (sotto setpoint tipico)
        50,                     // Umidità iniziale 50%
        1000,                   // Pressione iniziale 1000 hPa
        g_config.hysteresis     // Isteresi da configurazione
    );

    // Inizializza display + touch
    ESP_LOGI(TAG, "Initializing display...");
    ESP_ERROR_CHECK(display_init());

    ESP_LOGI(TAG, "Starting tasks...");

    // Crea task status (CORE 0) - gestisce anche registrazione sample ogni minuto
    xTaskCreatePinnedToCore(
        status_task,
        "status",
        4096,
        NULL,
        5,  // Priorità alta
        NULL,
        0   // Core 0
    );

    ESP_LOGI(TAG, "Initialization complete. System running.");
}
