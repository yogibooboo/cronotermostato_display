/**
 * @file comune.h
 * @brief Strutture dati comuni e definizioni condivise per il cronotermostato
 */

#ifndef COMUNE_H
#define COMUNE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PIN DEFINITIONS - JC3248W535EN Module
// ============================================================================

// Display QSPI
#define QSPI_CS     45
#define QSPI_CLK    47
#define QSPI_D0     21
#define QSPI_D1     48
#define QSPI_D2     40
#define QSPI_D3     39

// Touch I2C
#define TOUCH_SDA   4
#define TOUCH_SCL   8
#define TOUCH_RST   12
#define TOUCH_INT   11
#define TOUCH_ADDR  0x3B

// Backlight PWM
#define GFX_BL      1

// Sensore BME280 I2C (bus dedicato)
#define SENSOR_SDA  15
#define SENSOR_SCL  16
#define BME280_ADDR 0x76    // Indirizzo BME280 (SDO a GND), 0x77 se SDO a VCC

// Alimentazione sensore BME280 via GPIO
#define SENSOR_GND  46      // GPIO per GND sensore (output LOW)
#define SENSOR_VCC  9       // GPIO per VCC sensore (output HIGH)

// Relè caldaia
#define RELAY_GPIO  17

// LED WiFi status (active LOW - acceso quando output = 0)
#define LED_WIFI    GPIO_NUM_6

// ============================================================================
// CONSTANTS
// ============================================================================

#define PROGRAMS_COUNT          4      // Numero di programmi orari
#define SLOTS_PER_DAY           48     // Slot da mezz'ora
#define DAYS_PER_WEEK           7
#define PROGRAM_NAME_LEN        16     // Lunghezza nome programma
#define SAMPLES_PER_DAY         1440   // 1 sample/minuto per 24h

#define MIN_TEMP                5.0f   // Temperatura minima (°C)
#define MAX_TEMP                30.0f  // Temperatura massima (°C)
#define DEFAULT_HYSTERESIS      0.3f   // Isteresi default (°C)

// ============================================================================
// HISTORY SAMPLE - 12 byte
// ============================================================================

/**
 * @brief Sample singolo dello storico temperatura/umidità/pressione
 *
 * Ogni file giornaliero contiene 1440 di questi record (1 per minuto).
 * La data è implicita nel nome file (YYYY-MM-DD.bin).
 */
typedef struct {
    uint16_t minute_of_day;    // Minuti dalla mezzanotte (0-1439)
    int16_t temperature;       // Temperatura × 100 (es. 2350 = 23.50°C)
    uint8_t humidity;          // Umidità relativa (0-100%)
    uint8_t flags;             // Bit flags:
                               //   bit 0: relay_on (caldaia accesa)
                               //   bit 1: manual_mode
                               //   bit 2: exception_active
                               //   bit 3-7: riservati
    int16_t setpoint;          // Soglia attiva × 100 (°C)
    uint8_t active_bank;       // Banco programma attivo (0-3)
    uint8_t reserved;          // Riservato per allineamento/futuro
    uint16_t pressure;         // Pressione atmosferica in decimi di hPa (es. 10132 = 1013.2 hPa)
} __attribute__((packed)) history_sample_t;

// Flag bits per history_sample_t.flags
#define HISTORY_FLAG_RELAY_ON       0x01
#define HISTORY_FLAG_MANUAL_MODE    0x02
#define HISTORY_FLAG_EXCEPTION      0x04

// ============================================================================
// THERMOSTAT CONFIGURATION
// ============================================================================

/**
 * @brief Configurazione completa del termostato
 *
 * Salvata in SPIFFS come /spiffs/config.json (o binario)
 */
typedef struct {
    // ===== Stato sistema =====
    bool system_enabled;           // ON/OFF globale
    bool manual_mode;              // true=manuale, false=auto
    float manual_setpoint;         // Soglia manuale (°C)
    float temp_correction;         // Offset calibrazione sensore (°C)
    float hysteresis;              // Isteresi (°C)

    // ===== Modalità programmazione =====
    uint8_t program_mode;          // 0=programma unico, 1=settimanale
    uint8_t single_program_bank;   // Banco per programma unico (0-3)
    uint8_t weekly_banks[DAYS_PER_WEEK];  // Banco per ogni giorno (dom=0, sab=6)

    // ===== Eccezione (vacanze) =====
    bool exception_enabled;
    uint8_t exception_start_day;
    uint8_t exception_start_month;
    uint8_t exception_end_day;
    uint8_t exception_end_month;
    uint8_t exception_bank;        // Banco da usare durante eccezione (0-3)

    // ===== Programmi orari =====
    // 4 banchi × 48 slot (mezz'ora), temperature in °C
    float programs[PROGRAMS_COUNT][SLOTS_PER_DAY];

    // Nomi programmi (es. "Casa", "Lavoro", "Weekend", "Vacanza")
    char program_names[PROGRAMS_COUNT][PROGRAM_NAME_LEN];

    // ===== Hardware GPIO (opzionale, se configurabili) =====
    uint8_t relay_gpio;
    uint8_t sensor_sda;
    uint8_t sensor_scl;

} thermostat_config_t;

// ============================================================================
// RUNTIME STATE (non salvato, solo in RAM)
// ============================================================================

/**
 * @brief Stato runtime del termostato (non persistente)
 */
typedef struct {
    float current_temperature;     // Lettura corrente sensore (con correzione applicata)
    uint8_t current_humidity;      // Umidità corrente
    uint16_t current_pressure;     // Pressione atmosferica corrente (decimi di hPa)
    bool relay_state;              // Stato attuale relè
    uint8_t active_bank;           // Banco programma correntemente attivo
    float active_setpoint;         // Soglia correntemente attiva
    bool in_exception;             // true se siamo in periodo eccezione
    uint32_t last_sample_time;     // Ultimo timestamp sample salvato
    uint16_t reconnection_count;   // Contatore riconnessioni WiFi
} thermostat_state_t;

// ============================================================================
// FUNCTION PROTOTYPES (da implementare nei vari moduli)
// ============================================================================

// Storage
esp_err_t storage_init(void);
esp_err_t config_load(thermostat_config_t* config);
esp_err_t config_save(const thermostat_config_t* config);

// History
esp_err_t history_init(void);
esp_err_t history_add_sample(const history_sample_t* sample);
esp_err_t history_save_day(void);  // Chiamato a mezzanotte
esp_err_t history_load_day(const char* date, history_sample_t* buffer);
esp_err_t history_cleanup_old(uint16_t keep_days);

// Thermostat logic (da implementare)
void thermostat_init(const thermostat_config_t* config);
void thermostat_update(float temperature, uint8_t humidity, thermostat_state_t* state);
bool thermostat_is_exception_active(const thermostat_config_t* config, const struct tm* now);
float thermostat_get_setpoint(const thermostat_config_t* config, const thermostat_state_t* state, const struct tm* now);

#ifdef __cplusplus
}
#endif

#endif // COMUNE_H
