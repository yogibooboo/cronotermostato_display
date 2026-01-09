/**
 * @file display_manager.cpp
 * @brief Display and LVGL management for JC3248W535EN module
 */

#include "display_manager.h"
#include <lvgl.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "esp_log.h"
#include "comune.h"
#include "history_manager.h"

static const char *TAG = "DISPLAY";

// LVGL rotation: 90 degrees for landscape mode
#define LVGL_PORT_ROTATION_DEGREE (90)

// Display dimensions in landscape
#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320

// Chart area - lascia spazio per etichette Y a sinistra
#define CHART_TOP      70
#define CHART_HEIGHT   220
#define CHART_WIDTH    430
#define CHART_LEFT     40   // Spazio per etichette Y
#define CHART_BOTTOM   25   // Spazio per etichette X

// Linea verticale a puntini
#define TIME_LINE_DOTS      15   // Numero di puntini
#define TIME_LINE_DOT_SIZE  3    // Dimensione puntino

// Scala temperatura grafico (da rendere adattativa in futuro)
#define CHART_TEMP_MIN      10.0f   // Temperatura minima scala
#define CHART_TEMP_MAX      25.0f   // Temperatura massima scala

// Global UI objects
static lv_obj_t *main_screen = NULL;

// Status labels
static lv_obj_t *label_temp = NULL;
static lv_obj_t *label_time = NULL;
static lv_obj_t *heater_indicator = NULL;  // Pallino stato caldaia

// Chart objects
static lv_obj_t *chart = NULL;
static lv_chart_series_t *ser_program = NULL;   // Programma giornaliero (blu)
static lv_chart_series_t *ser_temp = NULL;      // Temperatura attuale (rosso)

// Program data cache (48 slots)
static float program_data[SLOTS_PER_DAY];
static bool program_loaded = false;

// Linea verticale a puntini per ora corrente
static lv_obj_t *time_line_dots[TIME_LINE_DOTS];

// Per aggiornamento grafico solo ogni minuto
static int last_chart_minute = -1;

// Stato caldaia corrente
static bool current_heater_state = false;

// ============================================================================
// Helper: converti temperatura in valore chart
// ============================================================================
static lv_coord_t temp_to_chart_val(float temp)
{
    // Scala: CHART_TEMP_MIN -> 100, CHART_TEMP_MAX -> 250
    // Range interno LVGL: 100-250 (150 unità)
    float range = CHART_TEMP_MAX - CHART_TEMP_MIN;
    lv_coord_t val = (lv_coord_t)((temp - CHART_TEMP_MIN) / range * 150.0f + 100.0f);
    if (val < 100) val = 100;
    if (val > 250) val = 250;
    return val;
}

// ============================================================================
// UI Creation
// ============================================================================

static void create_main_screen(void)
{
    main_screen = lv_obj_create(NULL);

    // Sfondo nero
    lv_obj_set_style_bg_color(main_screen, lv_color_black(), 0);

    // Time label - in alto a sinistra (bianco)
    label_time = lv_label_create(main_screen);
    lv_label_set_text(label_time, "--:--:--");
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
    lv_obj_align(label_time, LV_ALIGN_TOP_LEFT, 10, 10);

    // Temperature label - in alto a destra (rosso)
    label_temp = lv_label_create(main_screen);
    lv_label_set_text(label_temp, "--.-°C");
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_temp, lv_color_hex(0xFF0000), 0);
    lv_obj_align(label_temp, LV_ALIGN_TOP_RIGHT, -50, 10);  // Spazio per pallino

    // Pallino stato caldaia (a destra della temperatura)
    heater_indicator = lv_obj_create(main_screen);
    lv_obj_set_size(heater_indicator, 30, 30);
    lv_obj_set_style_radius(heater_indicator, 15, 0);  // Cerchio
    lv_obj_set_style_bg_color(heater_indicator, lv_color_hex(0x444444), 0);  // Grigio default
    lv_obj_set_style_border_width(heater_indicator, 0, 0);
    lv_obj_set_style_pad_all(heater_indicator, 0, 0);  // Rimuovi padding
    lv_obj_set_scrollbar_mode(heater_indicator, LV_SCROLLBAR_MODE_OFF);  // Rimuovi scrollbar
    lv_obj_align(heater_indicator, LV_ALIGN_TOP_RIGHT, -10, 20);

    // =========================================================================
    // Grafico programma giornaliero
    // =========================================================================

    chart = lv_chart_create(main_screen);
    lv_obj_set_size(chart, CHART_WIDTH, CHART_HEIGHT);
    lv_obj_set_pos(chart, CHART_LEFT, CHART_TOP);

    // Stile grafico - sfondo scuro
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(chart, 1, 0);

    // Griglia molto scura
    lv_obj_set_style_line_color(chart, lv_color_hex(0x1a1a1a), LV_PART_MAIN);

    // Tipo grafico a linee
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);

    // 480 punti (10 per slot di 30 min = 1 punto ogni 3 minuti)
    // Questo permette gradini quasi verticali
    lv_chart_set_point_count(chart, 480);

    // Range Y: 10-25°C (mappato a 100-250 per precisione interna)
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 100, 250);

    // Linee divisorie: 3 orizzontali (ogni 5°C per scala 10-25), 24 verticali (ogni ora)
    lv_chart_set_div_line_count(chart, 3, 24);

    // Nascondi i punti, mostra solo linee
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);

    // Serie programma (blu scuro) - STEP CHART (a gradini)
    ser_program = lv_chart_add_series(chart, lv_color_hex(0x0D47A1), LV_CHART_AXIS_PRIMARY_Y);

    // Serie temperatura attuale (rosso)
    ser_temp = lv_chart_add_series(chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);

    // Inizializza serie con valori di default
    // 480 punti totali (10 per ogni slot di 30 minuti)
    for (int i = 0; i < 480; i++) {
        lv_chart_set_value_by_id(chart, ser_program, i, 200);  // 20°C default
        lv_chart_set_value_by_id(chart, ser_temp, i, LV_CHART_POINT_NONE);
    }

    // =========================================================================
    // Linea verticale a puntini per ora corrente
    // =========================================================================
    int dot_spacing = CHART_HEIGHT / (TIME_LINE_DOTS + 1);
    for (int i = 0; i < TIME_LINE_DOTS; i++) {
        time_line_dots[i] = lv_obj_create(main_screen);
        lv_obj_set_size(time_line_dots[i], TIME_LINE_DOT_SIZE, TIME_LINE_DOT_SIZE);
        lv_obj_set_style_radius(time_line_dots[i], TIME_LINE_DOT_SIZE / 2, 0);  // Cerchio
        lv_obj_set_style_bg_color(time_line_dots[i], lv_color_hex(0x606000), 0);  // Giallo scuro
        lv_obj_set_style_border_width(time_line_dots[i], 0, 0);
        // Posizione iniziale (sarà aggiornata)
        lv_obj_set_pos(time_line_dots[i], CHART_LEFT, CHART_TOP + dot_spacing * (i + 1));
    }

    // =========================================================================
    // Etichette asse Y (temperature) - a sinistra del grafico
    // Scala 10-25°C con 4 etichette (ogni 5°C)
    // =========================================================================
    const char* temps[] = {"25", "20", "15", "10"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* lbl = lv_label_create(main_screen);
        lv_label_set_text(lbl, temps[i]);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        // Posiziona a sinistra del grafico, distribuiti verticalmente
        // 4 etichette: 0%, 33%, 66%, 100% dell'altezza
        int y_pos = CHART_TOP + (i * CHART_HEIGHT / 3) - 6;
        lv_obj_set_pos(lbl, 5, y_pos);
    }

    // =========================================================================
    // Etichette asse X (ore) - sotto il grafico
    // =========================================================================
    const char* hours[] = {"0", "6", "12", "18", "24"};
    for (int i = 0; i < 5; i++) {
        lv_obj_t* lbl = lv_label_create(main_screen);
        lv_label_set_text(lbl, hours[i]);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        // Posiziona sotto il grafico
        int x_pos = CHART_LEFT + (i * CHART_WIDTH / 4) - 5;
        lv_obj_set_pos(lbl, x_pos, CHART_TOP + CHART_HEIGHT + 5);
    }
}

// ============================================================================
// Public API
// ============================================================================

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing display...");

    // Configure display with LVGL
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
#if LVGL_PORT_ROTATION_DEGREE == 90
        .rotate = LV_DISP_ROT_90,
#elif LVGL_PORT_ROTATION_DEGREE == 270
        .rotate = LV_DISP_ROT_270,
#elif LVGL_PORT_ROTATION_DEGREE == 180
        .rotate = LV_DISP_ROT_180,
#elif LVGL_PORT_ROTATION_DEGREE == 0
        .rotate = LV_DISP_ROT_NONE,
#endif
    };

    lv_disp_t *disp = bsp_display_start_with_config(&cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to start display");
        return ESP_FAIL;
    }

    // Turn on backlight
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Display backlight ON");

    // Create UI (must be within lock/unlock)
    bsp_display_lock(0);
    create_main_screen();
    lv_scr_load(main_screen);
    bsp_display_unlock();

    ESP_LOGI(TAG, "Display initialized successfully");
    return ESP_OK;
}

void display_update_status(float temperature, uint8_t hour, uint8_t minute, uint8_t second, bool heater_on)
{
    if (label_temp == NULL || label_time == NULL) {
        return; // Display not initialized yet
    }

    // Usa timeout di 100ms invece di 0 (che blocca indefinitamente)
    if (!bsp_display_lock(100)) {
        ESP_LOGW("DISPLAY", "Failed to acquire display lock, skipping update");
        return;
    }

    // Update temperature label
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f°C", temperature);
    lv_label_set_text(label_temp, buf);

    // Update time label (hh:mm:ss)
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
    lv_label_set_text(label_time, buf);

    // Aggiorna pallino stato caldaia
    if (heater_indicator != NULL) {
        if (heater_on) {
            lv_obj_set_style_bg_color(heater_indicator, lv_color_hex(0xFF0000), 0);  // Rosso
        } else {
            lv_obj_set_style_bg_color(heater_indicator, lv_color_hex(0x444444), 0);  // Grigio
        }
        current_heater_state = heater_on;
    }

    // Calcola minuto corrente per decidere se aggiornare il grafico
    int current_minute = hour * 60 + minute;

    // Aggiorna grafico solo se è cambiato il minuto
    if (current_minute != last_chart_minute && chart != NULL && ser_temp != NULL) {
        last_chart_minute = current_minute;

        // Calcola indice nel grafico (480 punti = 1 ogni 3 minuti)
        // current_minute va da 0 a 1439, chart_idx va da 0 a 479
        int chart_idx = current_minute / 3;
        if (chart_idx >= 480) chart_idx = 479;

        // Converti temperatura in valore chart
        lv_coord_t temp_val = temp_to_chart_val(temperature);

        // Imposta il punto corrente
        lv_chart_set_value_by_id(chart, ser_temp, chart_idx, temp_val);

        ESP_LOGI(TAG, "Chart updated: idx=%d, temp=%.1f°C, val=%d", chart_idx, temperature, temp_val);

        lv_chart_refresh(chart);
    }

    // Aggiorna posizione linea verticale a puntini (sempre, ad ogni secondo)
    int total_min = hour * 60 + minute;
    int x_pos = CHART_LEFT + (total_min * CHART_WIDTH / 1440);
    int dot_spacing = CHART_HEIGHT / (TIME_LINE_DOTS + 1);

    for (int i = 0; i < TIME_LINE_DOTS; i++) {
        if (time_line_dots[i] != NULL) {
            lv_obj_set_pos(time_line_dots[i], x_pos, CHART_TOP + dot_spacing * (i + 1));
        }
    }

    bsp_display_unlock();
}

void display_set_program(const float* setpoints, int count)
{
    if (chart == NULL || ser_program == NULL || setpoints == NULL) {
        ESP_LOGW(TAG, "display_set_program: invalid params");
        return;
    }

    if (!bsp_display_lock(100)) {
        ESP_LOGW("DISPLAY", "Failed to acquire display lock for program update");
        return;
    }

    // Grafico a gradini verticali con 480 punti:
    // Ogni slot di 30 minuti occupa 10 punti (480/48 = 10)
    // Tutti i 10 punti di uno slot hanno lo stesso valore -> gradini orizzontali
    // Al cambio slot il valore cambia -> transizione quasi verticale (1 punto)

    int slots = (count > SLOTS_PER_DAY) ? SLOTS_PER_DAY : count;
    for (int i = 0; i < slots; i++) {
        program_data[i] = setpoints[i];
        lv_coord_t val = temp_to_chart_val(setpoints[i]);

        // Ogni slot occupa 10 punti nel grafico (480 / 48 = 10)
        int base_idx = i * 10;
        for (int j = 0; j < 10; j++) {
            lv_chart_set_value_by_id(chart, ser_program, base_idx + j, val);
        }
    }

    program_loaded = true;
    lv_chart_refresh(chart);

    bsp_display_unlock();

    ESP_LOGI(TAG, "Program loaded: %d slots, first=%.1f°C, last=%.1f°C",
             slots, setpoints[0], setpoints[slots-1]);
}

void display_load_history_temperatures(void)
{
    const history_buffer_t* hist = history_get_buffer();
    if (hist == NULL || !hist->initialized || hist->samples == NULL) {
        ESP_LOGW(TAG, "History buffer not available");
        return;
    }

    if (!bsp_display_lock(100)) {
        ESP_LOGW("DISPLAY", "Failed to acquire display lock for history load");
        return;
    }

    int loaded = 0;

    // Carica temperature dallo storico
    // 480 punti = 1 ogni 3 minuti
    for (int minute = 0; minute < HISTORY_SAMPLES_PER_DAY; minute++) {
        const history_sample_t* sample = &hist->samples[minute];

        // Verifica se il sample è valido (temperatura != -32768 che è il marker invalido)
        if (sample->temperature != -32768) {
            float temp = sample->temperature / 100.0f;

            // Verifica range ragionevole (accetta anche fuori scala, clamp dopo)
            if (temp >= 0.0f && temp <= 50.0f) {
                // Calcola indice nel grafico (480 punti = 1 ogni 3 minuti)
                int chart_idx = minute / 3;
                if (chart_idx >= 480) chart_idx = 479;

                lv_coord_t temp_val = temp_to_chart_val(temp);

                // Imposta il punto
                lv_chart_set_value_by_id(chart, ser_temp, chart_idx, temp_val);

                loaded++;
            }
        }
    }

    if (loaded > 0) {
        lv_chart_refresh(chart);
        ESP_LOGI(TAG, "Loaded %d temperature samples from history", loaded);
    }

    bsp_display_unlock();
}
