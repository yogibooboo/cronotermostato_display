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

static const char *TAG = "DISPLAY";

// LVGL rotation: 90 degrees for landscape mode
#define LVGL_PORT_ROTATION_DEGREE (90)

// Global UI objects
static lv_obj_t *main_screen = NULL;
static lv_obj_t *page1_screen = NULL;
static lv_obj_t *page2_screen = NULL;
static lv_obj_t *page3_screen = NULL;

// Status labels (will be updated from thermostat)
static lv_obj_t *label_temp = NULL;
static lv_obj_t *label_humidity = NULL;
static lv_obj_t *label_heater = NULL;

// ============================================================================
// UI Creation Functions (from NorthernMan54 demo)
// ============================================================================

static void btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        int page_num = (int)(intptr_t)lv_event_get_user_data(e);

        switch(page_num) {
            case 1:
                lv_scr_load(page1_screen);
                ESP_LOGI(TAG, "Navigated to Page 1");
                break;
            case 2:
                lv_scr_load(page2_screen);
                ESP_LOGI(TAG, "Navigated to Page 2");
                break;
            case 3:
                lv_scr_load(page3_screen);
                ESP_LOGI(TAG, "Navigated to Page 3");
                break;
            case 0:
                lv_scr_load(main_screen);
                ESP_LOGI(TAG, "Navigated to Main Screen");
                break;
        }
    }
}

static void create_main_screen(void)
{
    main_screen = lv_obj_create(NULL);

    // Title label
    lv_obj_t * label = lv_label_create(main_screen);
    lv_label_set_text(label, "Cronotermostato");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    // Temperature label
    label_temp = lv_label_create(main_screen);
    lv_label_set_text(label_temp, "T: --.-°C");
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, -40);

    // Humidity label
    label_humidity = lv_label_create(main_screen);
    lv_label_set_text(label_humidity, "H: --%");
    lv_obj_align(label_humidity, LV_ALIGN_CENTER, 0, 0);

    // Heater status label
    label_heater = lv_label_create(main_screen);
    lv_label_set_text(label_heater, "Caldaia: OFF");
    lv_obj_align(label_heater, LV_ALIGN_CENTER, 0, 40);

    // Button 1
    lv_obj_t * btn1 = lv_btn_create(main_screen);
    lv_obj_set_size(btn1, 150, 50);
    lv_obj_align(btn1, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(btn1, btn_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    label = lv_label_create(btn1);
    lv_label_set_text(label, "Programmi");
    lv_obj_center(label);

    // Button 2
    lv_obj_t * btn2 = lv_btn_create(main_screen);
    lv_obj_set_size(btn2, 150, 50);
    lv_obj_align(btn2, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btn2, btn_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)2);

    label = lv_label_create(btn2);
    lv_label_set_text(label, "Storico");
    lv_obj_center(label);

    // Button 3
    lv_obj_t * btn3 = lv_btn_create(main_screen);
    lv_obj_set_size(btn3, 150, 50);
    lv_obj_align(btn3, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(btn3, btn_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)3);

    label = lv_label_create(btn3);
    lv_label_set_text(label, "Impostazioni");
    lv_obj_center(label);
}

static void create_page1(void)
{
    page1_screen = lv_obj_create(NULL);

    lv_obj_t * label = lv_label_create(page1_screen);
    lv_label_set_text(label, "Programmi");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    // Back button
    lv_obj_t * btn_back = lv_btn_create(page1_screen);
    lv_obj_set_size(btn_back, 150, 50);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(btn_back, btn_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)0);

    label = lv_label_create(btn_back);
    lv_label_set_text(label, "Indietro");
    lv_obj_center(label);

    // Content
    label = lv_label_create(page1_screen);
    lv_label_set_text(label, "Qui verranno visualizzati\ni programmi orari");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

static void create_page2(void)
{
    page2_screen = lv_obj_create(NULL);

    lv_obj_t * label = lv_label_create(page2_screen);
    lv_label_set_text(label, "Storico");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    // Back button
    lv_obj_t * btn_back = lv_btn_create(page2_screen);
    lv_obj_set_size(btn_back, 150, 50);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(btn_back, btn_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)0);

    label = lv_label_create(btn_back);
    lv_label_set_text(label, "Indietro");
    lv_obj_center(label);

    // Content
    label = lv_label_create(page2_screen);
    lv_label_set_text(label, "Qui verrà visualizzato\nlo storico dei valori");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

static void create_page3(void)
{
    page3_screen = lv_obj_create(NULL);

    lv_obj_t * label = lv_label_create(page3_screen);
    lv_label_set_text(label, "Impostazioni");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    // Back button
    lv_obj_t * btn_back = lv_btn_create(page3_screen);
    lv_obj_set_size(btn_back, 150, 50);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(btn_back, btn_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)0);

    label = lv_label_create(btn_back);
    lv_label_set_text(label, "Indietro");
    lv_obj_center(label);

    // Content
    label = lv_label_create(page3_screen);
    lv_label_set_text(label, "Qui verranno visualizzate\nle impostazioni");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

static void create_simple_ui(void)
{
    // Create all screens
    create_main_screen();
    create_page1();
    create_page2();
    create_page3();

    // Load main screen
    lv_scr_load(main_screen);
    ESP_LOGI(TAG, "Simple UI created with navigation");
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
    create_simple_ui();
    bsp_display_unlock();

    ESP_LOGI(TAG, "Display initialized successfully");
    return ESP_OK;
}

void display_update_status(float temperature, uint8_t humidity, bool heater_on)
{
    if (label_temp == NULL || label_humidity == NULL || label_heater == NULL) {
        return; // Display not initialized yet
    }

    bsp_display_lock(0);

    // Update temperature
    char buf[32];
    snprintf(buf, sizeof(buf), "T: %.1f°C", temperature);
    lv_label_set_text(label_temp, buf);

    // Update humidity
    snprintf(buf, sizeof(buf), "H: %d%%", humidity);
    lv_label_set_text(label_humidity, buf);

    // Update heater status
    lv_label_set_text(label_heater, heater_on ? "Caldaia: ON" : "Caldaia: OFF");

    bsp_display_unlock();
}
