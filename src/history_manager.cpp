/**
 * @file history_manager.cpp
 * @brief Implementazione gestione log giornaliero in PSRAM
 */

#include "history_manager.h"
#include "storage_manager.h"
#include "time_sync.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

static const char* TAG = "HISTORY";

// ============================================================================
// VARIABILI STATICHE
// ============================================================================

static history_buffer_t s_buffer = {0};

// ============================================================================
// FUNZIONI PRIVATE
// ============================================================================

/**
 * @brief Ottiene la data corrente dal sistema
 */
static void get_current_date(uint16_t* year, uint8_t* month, uint8_t* day)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    *year = timeinfo.tm_year + 1900;
    *month = timeinfo.tm_mon + 1;
    *day = timeinfo.tm_mday;
}

/**
 * @brief Ottiene il minuto corrente del giorno (0-1439)
 */
static uint16_t get_current_minute_of_day(void)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}

/**
 * @brief Inizializza header per una nuova giornata
 */
static void init_header_for_date(uint16_t year, uint8_t month, uint8_t day)
{
    memcpy(s_buffer.header.magic, HISTORY_MAGIC, 4);
    s_buffer.header.version = HISTORY_VERSION;
    s_buffer.header.year = year;
    s_buffer.header.month = month;
    s_buffer.header.day = day;
    s_buffer.header.num_samples = 0;
    s_buffer.header.reserved = 0;
}

/**
 * @brief Inizializza tutti i sample con valori invalidi
 */
static void clear_samples(void)
{
    for (int i = 0; i < HISTORY_SAMPLES_PER_DAY; i++) {
        s_buffer.samples[i].minute_of_day = i;
        s_buffer.samples[i].temperature = -32768;  // Valore invalido
        s_buffer.samples[i].humidity = 255;        // Valore invalido
        s_buffer.samples[i].flags = 0;
        s_buffer.samples[i].setpoint = -32768;
        s_buffer.samples[i].active_bank = 0;
        s_buffer.samples[i].reserved = 0;
        s_buffer.samples[i].pressure = 0;          // Valore invalido
    }
    s_buffer.current_minute = 0;
    s_buffer.dirty = false;
}

/**
 * @brief Verifica se la data nel buffer corrisponde a oggi
 */
static bool is_buffer_for_today(void)
{
    uint16_t year;
    uint8_t month, day;
    get_current_date(&year, &month, &day);

    return (s_buffer.header.year == year &&
            s_buffer.header.month == month &&
            s_buffer.header.day == day);
}

// ============================================================================
// API PUBBLICHE
// ============================================================================

esp_err_t history_init(void)
{
    ESP_LOGI(TAG, "Initializing history manager...");

    // Verifica PSRAM disponibile
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t required = HISTORY_SAMPLES_PER_DAY * sizeof(history_sample_t);

    ESP_LOGI(TAG, "PSRAM free: %u bytes, required: %u bytes", psram_free, required);

    if (psram_free < required) {
        ESP_LOGE(TAG, "Insufficient PSRAM! Need %u bytes, have %u", required, psram_free);
        return ESP_ERR_NO_MEM;
    }

    // Alloca buffer in PSRAM
    s_buffer.samples = (history_sample_t*)heap_caps_malloc(
        required, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_buffer.samples == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffer!");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Allocated %u bytes in PSRAM at %p", required, s_buffer.samples);

    // Inizializza per la data corrente
    uint16_t year;
    uint8_t month, day;
    get_current_date(&year, &month, &day);

    // Prova a caricare dati esistenti per oggi
    esp_err_t ret = history_load_from_file(year, month, day);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded existing data for %04d-%02d-%02d (%d samples)",
                 year, month, day, s_buffer.header.num_samples);
    } else {
        // Nessun file esistente, inizializza vuoto
        ESP_LOGI(TAG, "No existing data, initializing new buffer for %04d-%02d-%02d",
                 year, month, day);
        init_header_for_date(year, month, day);
        clear_samples();
    }

    s_buffer.initialized = true;

    ESP_LOGI(TAG, "History manager initialized successfully");
    return ESP_OK;
}

void history_deinit(void)
{
    if (s_buffer.samples != NULL) {
        // Salva eventuali dati non salvati
        if (s_buffer.dirty) {
            history_save_to_file();
        }

        heap_caps_free(s_buffer.samples);
        s_buffer.samples = NULL;
    }

    s_buffer.initialized = false;
    ESP_LOGI(TAG, "History manager deinitialized");
}

esp_err_t history_add_sample(const history_sample_t* sample)
{
    if (!s_buffer.initialized || s_buffer.samples == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (sample->minute_of_day >= HISTORY_SAMPLES_PER_DAY) {
        return ESP_ERR_INVALID_ARG;
    }

    // Copia sample nel buffer
    memcpy(&s_buffer.samples[sample->minute_of_day], sample, sizeof(history_sample_t));

    // Aggiorna contatore sample
    if (sample->minute_of_day >= s_buffer.header.num_samples) {
        s_buffer.header.num_samples = sample->minute_of_day + 1;
    }

    s_buffer.current_minute = sample->minute_of_day;
    s_buffer.dirty = true;

    return ESP_OK;
}

esp_err_t history_record(float temperature, uint8_t humidity, uint16_t pressure,
                         float setpoint, bool relay_on, bool manual_mode,
                         uint8_t active_bank)
{
    // Controlla cambio giorno prima di registrare
    history_check_day_change();

    uint16_t minute = get_current_minute_of_day();

    history_sample_t sample;
    sample.minute_of_day = minute;
    sample.temperature = (int16_t)(temperature * 100);
    sample.humidity = humidity;
    sample.pressure = pressure;
    sample.setpoint = (int16_t)(setpoint * 100);
    sample.active_bank = active_bank;
    sample.reserved = 0;

    // Costruisci flags
    sample.flags = 0;
    if (relay_on) sample.flags |= HISTORY_FLAG_RELAY_ON;
    if (manual_mode) sample.flags |= HISTORY_FLAG_MANUAL_MODE;

    return history_add_sample(&sample);
}

bool history_check_day_change(void)
{
    if (!s_buffer.initialized) {
        return false;
    }

    if (is_buffer_for_today()) {
        return false;
    }

    // Il giorno è cambiato!
    ESP_LOGI(TAG, "Day change detected!");

    // Salva buffer corrente
    if (s_buffer.dirty && s_buffer.header.num_samples > 0) {
        ESP_LOGI(TAG, "Saving previous day data (%d samples)...",
                 s_buffer.header.num_samples);
        history_save_to_file();
    }

    // Inizializza per il nuovo giorno
    uint16_t year;
    uint8_t month, day;
    get_current_date(&year, &month, &day);

    ESP_LOGI(TAG, "Initializing buffer for new day: %04d-%02d-%02d",
             year, month, day);

    init_header_for_date(year, month, day);
    clear_samples();

    return true;
}

esp_err_t history_save_to_file(void)
{
    if (!s_buffer.initialized || s_buffer.samples == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char filename[32];
    history_get_filename(s_buffer.header.year, s_buffer.header.month,
                         s_buffer.header.day, filename, sizeof(filename));

    ESP_LOGI(TAG, "Saving to %s (%d samples)...", filename, s_buffer.header.num_samples);

    FILE* f = fopen(filename, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open %s for writing", filename);
        return ESP_FAIL;
    }

    // Scrivi header
    size_t written = fwrite(&s_buffer.header, 1, sizeof(history_header_t), f);
    if (written != sizeof(history_header_t)) {
        ESP_LOGE(TAG, "Failed to write header");
        fclose(f);
        return ESP_FAIL;
    }

    // Scrivi tutti i sample (anche quelli vuoti, per mantenere offset fissi)
    written = fwrite(s_buffer.samples, sizeof(history_sample_t),
                     HISTORY_SAMPLES_PER_DAY, f);
    if (written != HISTORY_SAMPLES_PER_DAY) {
        ESP_LOGE(TAG, "Failed to write samples (wrote %d of %d)",
                 written, HISTORY_SAMPLES_PER_DAY);
        fclose(f);
        return ESP_FAIL;
    }

    fclose(f);
    s_buffer.dirty = false;

    ESP_LOGI(TAG, "Saved %d bytes to %s", HISTORY_FILE_SIZE, filename);
    return ESP_OK;
}

esp_err_t history_load_from_file(uint16_t year, uint8_t month, uint8_t day)
{
    if (s_buffer.samples == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char filename[32];
    history_get_filename(year, month, day, filename, sizeof(filename));

    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        ESP_LOGD(TAG, "File %s not found", filename);
        return ESP_ERR_NOT_FOUND;
    }

    // Leggi header
    size_t read = fread(&s_buffer.header, 1, sizeof(history_header_t), f);
    if (read != sizeof(history_header_t)) {
        ESP_LOGE(TAG, "Failed to read header");
        fclose(f);
        return ESP_FAIL;
    }

    // Verifica magic
    if (memcmp(s_buffer.header.magic, HISTORY_MAGIC, 4) != 0) {
        ESP_LOGE(TAG, "Invalid magic number in %s", filename);
        fclose(f);
        return ESP_ERR_INVALID_VERSION;
    }

    // Verifica versione
    if (s_buffer.header.version != HISTORY_VERSION) {
        ESP_LOGW(TAG, "Version mismatch: file=%d, expected=%d",
                 s_buffer.header.version, HISTORY_VERSION);
    }

    // Leggi sample
    read = fread(s_buffer.samples, sizeof(history_sample_t),
                 HISTORY_SAMPLES_PER_DAY, f);

    fclose(f);

    if (read != HISTORY_SAMPLES_PER_DAY) {
        ESP_LOGW(TAG, "Partial file: read %d of %d samples", read, HISTORY_SAMPLES_PER_DAY);
        // Non è un errore fatale, potrebbe essere un file parziale
    }

    s_buffer.dirty = false;

    // Ottieni minuto corrente del giorno
    uint16_t current_minute = get_current_minute_of_day();

    // Invalida sample futuri (dopo l'ora corrente) per sicurezza
    // Questo evita di mostrare dati "dal futuro" dopo un riavvio
    uint16_t invalidated = 0;
    for (int i = current_minute + 1; i < HISTORY_SAMPLES_PER_DAY; i++) {
        if (s_buffer.samples[i].temperature != -32768) {
            s_buffer.samples[i].temperature = -32768;
            s_buffer.samples[i].humidity = 255;
            s_buffer.samples[i].setpoint = -32768;
            s_buffer.samples[i].pressure = 0;
            s_buffer.samples[i].flags = 0;
            invalidated++;
        }
    }

    if (invalidated > 0) {
        ESP_LOGW(TAG, "Invalidated %d future samples (after minute %d)",
                 invalidated, current_minute);
        s_buffer.dirty = true;  // Marcato per salvataggio
    }

    // Ricalcola num_samples basandosi sui dati validi fino a current_minute
    s_buffer.header.num_samples = 0;
    for (int i = 0; i <= current_minute; i++) {
        if (s_buffer.samples[i].temperature != -32768) {
            s_buffer.header.num_samples = i + 1;
        }
    }

    // Trova l'ultimo minuto valido (entro l'ora corrente)
    s_buffer.current_minute = 0;
    for (int i = current_minute; i >= 0; i--) {
        if (s_buffer.samples[i].temperature != -32768) {
            s_buffer.current_minute = i;
            break;
        }
    }

    ESP_LOGI(TAG, "Loaded %s: %d valid samples, last minute=%d, current minute=%d",
             filename, s_buffer.header.num_samples, s_buffer.current_minute, current_minute);

    return ESP_OK;
}

const history_buffer_t* history_get_buffer(void)
{
    if (!s_buffer.initialized) {
        return NULL;
    }
    return &s_buffer;
}

esp_err_t history_get_sample(uint16_t minute_of_day, history_sample_t* sample)
{
    if (!s_buffer.initialized || s_buffer.samples == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (minute_of_day >= HISTORY_SAMPLES_PER_DAY) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(sample, &s_buffer.samples[minute_of_day], sizeof(history_sample_t));

    // Verifica se il sample è valido
    if (sample->temperature == -32768) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

uint16_t history_get_sample_count(void)
{
    if (!s_buffer.initialized) {
        return 0;
    }
    return s_buffer.header.num_samples;
}

void history_get_filename(uint16_t year, uint8_t month, uint8_t day,
                          char* buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "/spiffs/log_%04d%02d%02d.bin",
             year, month, day);
}

bool history_file_exists(uint16_t year, uint8_t month, uint8_t day)
{
    char filename[32];
    history_get_filename(year, month, day, filename, sizeof(filename));

    struct stat st;
    return (stat(filename, &st) == 0);
}

void history_get_stats(float* min_temp, float* max_temp, float* avg_temp,
                       uint16_t* heater_on_minutes)
{
    if (!s_buffer.initialized || s_buffer.samples == NULL) {
        return;
    }

    float t_min = 100.0f;
    float t_max = -100.0f;
    float t_sum = 0.0f;
    int t_count = 0;
    uint16_t heater_count = 0;

    for (int i = 0; i < HISTORY_SAMPLES_PER_DAY; i++) {
        const history_sample_t* s = &s_buffer.samples[i];

        if (s->temperature != -32768) {
            float temp = s->temperature / 100.0f;

            if (temp < t_min) t_min = temp;
            if (temp > t_max) t_max = temp;
            t_sum += temp;
            t_count++;

            if (s->flags & HISTORY_FLAG_RELAY_ON) {
                heater_count++;
            }
        }
    }

    if (min_temp) *min_temp = (t_count > 0) ? t_min : 0.0f;
    if (max_temp) *max_temp = (t_count > 0) ? t_max : 0.0f;
    if (avg_temp) *avg_temp = (t_count > 0) ? (t_sum / t_count) : 0.0f;
    if (heater_on_minutes) *heater_on_minutes = heater_count;
}
