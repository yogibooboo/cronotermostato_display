/**
 * @file history_manager.h
 * @brief Gestione log giornaliero temperatura/umidità/pressione in PSRAM
 *
 * Il buffer circolare in PSRAM contiene 1440 sample (1 per minuto per 24h).
 * All'avvio o al cambio giornata il buffer viene reinizializzato.
 * I dati possono essere salvati su SPIFFS e serviti via HTTP.
 */

#ifndef HISTORY_MANAGER_H
#define HISTORY_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "comune.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// COSTANTI
// ============================================================================

#define HISTORY_SAMPLES_PER_DAY     1440    // 1 sample/minuto
#define HISTORY_HEADER_SIZE         12      // Dimensione header file
#define HISTORY_SAMPLE_SIZE         12      // sizeof(history_sample_t)
#define HISTORY_FILE_SIZE           (HISTORY_HEADER_SIZE + HISTORY_SAMPLES_PER_DAY * HISTORY_SAMPLE_SIZE)

#define HISTORY_MAGIC               "TLOG"  // Magic number per file
#define HISTORY_VERSION             1       // Versione formato

// ============================================================================
// STRUTTURE
// ============================================================================

/**
 * @brief Header del file log giornaliero (12 bytes)
 */
typedef struct {
    char magic[4];          // "TLOG"
    uint8_t version;        // Versione formato (1)
    uint16_t year;          // Anno (little endian)
    uint8_t month;          // Mese (1-12)
    uint8_t day;            // Giorno (1-31)
    uint16_t num_samples;   // Numero sample validi
    uint8_t reserved;       // Riservato
} __attribute__((packed)) history_header_t;

/**
 * @brief Stato del buffer log in memoria
 */
typedef struct {
    history_header_t header;                        // Header corrente
    history_sample_t* samples;                      // Puntatore al buffer PSRAM
    uint16_t current_minute;                        // Ultimo minuto scritto
    bool initialized;                               // Buffer inizializzato
    bool dirty;                                     // Dati modificati da salvare
} history_buffer_t;

// ============================================================================
// API PUBBLICHE
// ============================================================================

/**
 * @brief Inizializza il gestore storico
 *
 * Alloca il buffer in PSRAM e carica eventuali dati esistenti per oggi.
 *
 * @return ESP_OK se successo, ESP_ERR_NO_MEM se PSRAM non disponibile
 */
esp_err_t history_init(void);

/**
 * @brief Deinizializza e libera memoria
 */
void history_deinit(void);

/**
 * @brief Aggiunge un nuovo sample al buffer
 *
 * Chiamare ogni minuto dal task principale.
 * Se il giorno è cambiato, salva il buffer precedente e lo reinizializza.
 *
 * @param sample Puntatore al sample da aggiungere
 * @return ESP_OK se successo
 */
esp_err_t history_add_sample(const history_sample_t* sample);

/**
 * @brief Aggiunge sample con i valori correnti
 *
 * Convenience function che crea il sample dai parametri.
 *
 * @param temperature Temperatura in °C
 * @param humidity Umidità %
 * @param pressure Pressione hPa
 * @param setpoint Setpoint attivo °C
 * @param relay_on Stato relè
 * @param manual_mode Modalità manuale attiva
 * @param active_bank Banco programma attivo
 * @return ESP_OK se successo
 */
esp_err_t history_record(float temperature, uint8_t humidity, uint16_t pressure,
                         float setpoint, bool relay_on, bool manual_mode,
                         uint8_t active_bank);

/**
 * @brief Controlla se è necessario cambiare giorno e gestisce la rotazione
 *
 * Da chiamare periodicamente (es. ogni minuto).
 * Se il giorno è cambiato:
 * 1. Salva il buffer corrente su file
 * 2. Reinizializza il buffer per il nuovo giorno
 *
 * @return true se è avvenuto un cambio giorno
 */
bool history_check_day_change(void);

/**
 * @brief Salva il buffer corrente su SPIFFS
 *
 * @return ESP_OK se successo, ESP_FAIL se errore I/O
 */
esp_err_t history_save_to_file(void);

/**
 * @brief Carica dati da file SPIFFS nel buffer
 *
 * @param year Anno
 * @param month Mese
 * @param day Giorno
 * @return ESP_OK se caricato, ESP_ERR_NOT_FOUND se file non esiste
 */
esp_err_t history_load_from_file(uint16_t year, uint8_t month, uint8_t day);

/**
 * @brief Ottiene puntatore al buffer corrente (read-only)
 *
 * @return Puntatore al buffer o NULL se non inizializzato
 */
const history_buffer_t* history_get_buffer(void);

/**
 * @brief Ottiene un sample specifico dal buffer corrente
 *
 * @param minute_of_day Minuto del giorno (0-1439)
 * @param sample Puntatore dove copiare il sample
 * @return ESP_OK se trovato, ESP_ERR_NOT_FOUND se non presente
 */
esp_err_t history_get_sample(uint16_t minute_of_day, history_sample_t* sample);

/**
 * @brief Ottiene il numero di sample validi nel buffer
 *
 * @return Numero di sample (0-1440)
 */
uint16_t history_get_sample_count(void);

/**
 * @brief Genera il nome file per una data specifica
 *
 * @param year Anno
 * @param month Mese
 * @param day Giorno
 * @param buffer Buffer di destinazione (almeno 32 caratteri)
 * @param buffer_size Dimensione buffer
 */
void history_get_filename(uint16_t year, uint8_t month, uint8_t day,
                          char* buffer, size_t buffer_size);

/**
 * @brief Verifica se esiste un file log per una data
 *
 * @param year Anno
 * @param month Mese
 * @param day Giorno
 * @return true se il file esiste
 */
bool history_file_exists(uint16_t year, uint8_t month, uint8_t day);

/**
 * @brief Ottiene statistiche sul buffer corrente
 *
 * @param min_temp Puntatore per temperatura minima (può essere NULL)
 * @param max_temp Puntatore per temperatura massima (può essere NULL)
 * @param avg_temp Puntatore per temperatura media (può essere NULL)
 * @param heater_on_minutes Puntatore per minuti caldaia accesa (può essere NULL)
 */
void history_get_stats(float* min_temp, float* max_temp, float* avg_temp,
                       uint16_t* heater_on_minutes);

#ifdef __cplusplus
}
#endif

#endif // HISTORY_MANAGER_H
