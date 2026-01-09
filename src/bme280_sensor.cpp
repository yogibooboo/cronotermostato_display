/**
 * @file bme280_sensor.cpp
 * @brief Driver BME280 per ESP-IDF
 *
 * Implementazione basata su datasheet Bosch BME280.
 * Supporta lettura temperatura, umidità e pressione.
 */

#include "bme280_sensor.h"
#include "comune.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char* TAG = "BME280";

// I2C configuration
#define I2C_MASTER_NUM          I2C_NUM_1
#define I2C_MASTER_FREQ_HZ      100000
#define I2C_MASTER_TIMEOUT_MS   100

// BME280 Registers
#define BME280_REG_CHIP_ID      0xD0
#define BME280_REG_RESET        0xE0
#define BME280_REG_CTRL_HUM     0xF2
#define BME280_REG_STATUS       0xF3
#define BME280_REG_CTRL_MEAS    0xF4
#define BME280_REG_CONFIG       0xF5
#define BME280_REG_DATA         0xF7    // 8 bytes: press[2:0], temp[2:0], hum[1:0]

// Calibration data registers
#define BME280_REG_CALIB_T1     0x88    // T1-T3, P1-P9 (26 bytes)
#define BME280_REG_CALIB_H1     0xA1    // H1 (1 byte)
#define BME280_REG_CALIB_H2     0xE1    // H2-H6 (7 bytes)

// Chip ID
#define BME280_CHIP_ID          0x60
#define BMP280_CHIP_ID          0x58    // BMP280 (no humidity)

// Reset command
#define BME280_RESET_CMD        0xB6

// Oversampling settings
#define BME280_OSRS_T_X1        (0x01 << 5)
#define BME280_OSRS_P_X1        (0x01 << 2)
#define BME280_OSRS_H_X1        0x01
#define BME280_MODE_NORMAL      0x03
#define BME280_MODE_FORCED      0x01

// Calibration data structure
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
} bme280_calib_t;

// Static variables
static bool s_initialized = false;
static bool s_has_humidity = false;     // true for BME280, false for BMP280
static bme280_calib_t s_calib;
static int32_t t_fine;                  // Fine temperature for compensation

// ============================================================================
// I2C Helper Functions
// ============================================================================

static esp_err_t i2c_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, BME280_ADDR, buf, 2,
                                       pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

static esp_err_t i2c_read_bytes(uint8_t reg, uint8_t* data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, BME280_ADDR,
                                         &reg, 1, data, len,
                                         pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

// ============================================================================
// Calibration Data
// ============================================================================

static esp_err_t read_calibration_data(void)
{
    uint8_t buf[26];
    esp_err_t ret;

    // Read T1-T3, P1-P9 (26 bytes starting at 0x88)
    ret = i2c_read_bytes(BME280_REG_CALIB_T1, buf, 26);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read calibration data (T/P)");
        return ret;
    }

    s_calib.dig_T1 = (uint16_t)(buf[1] << 8 | buf[0]);
    s_calib.dig_T2 = (int16_t)(buf[3] << 8 | buf[2]);
    s_calib.dig_T3 = (int16_t)(buf[5] << 8 | buf[4]);

    s_calib.dig_P1 = (uint16_t)(buf[7] << 8 | buf[6]);
    s_calib.dig_P2 = (int16_t)(buf[9] << 8 | buf[8]);
    s_calib.dig_P3 = (int16_t)(buf[11] << 8 | buf[10]);
    s_calib.dig_P4 = (int16_t)(buf[13] << 8 | buf[12]);
    s_calib.dig_P5 = (int16_t)(buf[15] << 8 | buf[14]);
    s_calib.dig_P6 = (int16_t)(buf[17] << 8 | buf[16]);
    s_calib.dig_P7 = (int16_t)(buf[19] << 8 | buf[18]);
    s_calib.dig_P8 = (int16_t)(buf[21] << 8 | buf[20]);
    s_calib.dig_P9 = (int16_t)(buf[23] << 8 | buf[22]);

    if (s_has_humidity) {
        // Read H1 (1 byte at 0xA1)
        ret = i2c_read_bytes(BME280_REG_CALIB_H1, &s_calib.dig_H1, 1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read H1");
            return ret;
        }

        // Read H2-H6 (7 bytes at 0xE1)
        uint8_t hbuf[7];
        ret = i2c_read_bytes(BME280_REG_CALIB_H2, hbuf, 7);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read H2-H6");
            return ret;
        }

        s_calib.dig_H2 = (int16_t)(hbuf[1] << 8 | hbuf[0]);
        s_calib.dig_H3 = hbuf[2];
        s_calib.dig_H4 = (int16_t)((hbuf[3] << 4) | (hbuf[4] & 0x0F));
        s_calib.dig_H5 = (int16_t)((hbuf[5] << 4) | (hbuf[4] >> 4));
        s_calib.dig_H6 = (int8_t)hbuf[6];
    }

    ESP_LOGI(TAG, "Calibration data loaded");
    return ESP_OK;
}

// ============================================================================
// Compensation Formulas (from Bosch datasheet)
// ============================================================================

static float compensate_temperature(int32_t adc_T)
{
    int32_t var1, var2;

    var1 = ((((adc_T >> 3) - ((int32_t)s_calib.dig_T1 << 1))) * ((int32_t)s_calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)s_calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)s_calib.dig_T1))) >> 12) *
            ((int32_t)s_calib.dig_T3)) >> 14;

    t_fine = var1 + var2;
    float T = (t_fine * 5 + 128) >> 8;
    return T / 100.0f;
}

static float compensate_pressure(int32_t adc_P)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)s_calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)s_calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)s_calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)s_calib.dig_P3) >> 8) + ((var1 * (int64_t)s_calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)s_calib.dig_P1) >> 33;

    if (var1 == 0) {
        return 0;  // Avoid division by zero
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)s_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)s_calib.dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)s_calib.dig_P7) << 4);
    return (float)p / 25600.0f;  // Return hPa
}

static float compensate_humidity(int32_t adc_H)
{
    int32_t v_x1_u32r;

    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)s_calib.dig_H4) << 20) -
                   (((int32_t)s_calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
                 (((((((v_x1_u32r * ((int32_t)s_calib.dig_H6)) >> 10) *
                      (((v_x1_u32r * ((int32_t)s_calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
                    ((int32_t)2097152)) * ((int32_t)s_calib.dig_H2) + 8192) >> 14));

    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                               ((int32_t)s_calib.dig_H1)) >> 4));

    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;

    float h = (v_x1_u32r >> 12);
    return h / 1024.0f;
}

// ============================================================================
// Public API
// ============================================================================

esp_err_t bme280_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing BME280 on SDA=%d, SCL=%d, addr=0x%02X",
             SENSOR_SDA, SENSOR_SCL, BME280_ADDR);

    // Configure power pins for sensor (GPIO powered)
    ESP_LOGI(TAG, "Configuring sensor power: GND=IO%d, VCC=IO%d", SENSOR_GND, SENSOR_VCC);

    // Configure GND pin (output LOW)
    gpio_config_t gnd_conf = {};
    gnd_conf.pin_bit_mask = (1ULL << SENSOR_GND);
    gnd_conf.mode = GPIO_MODE_OUTPUT;
    gnd_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gnd_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gnd_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&gnd_conf);
    gpio_set_level((gpio_num_t)SENSOR_GND, 0);

    // Configure VCC pin (output HIGH)
    gpio_config_t vcc_conf = {};
    vcc_conf.pin_bit_mask = (1ULL << SENSOR_VCC);
    vcc_conf.mode = GPIO_MODE_OUTPUT;
    vcc_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    vcc_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    vcc_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&vcc_conf);
    gpio_set_level((gpio_num_t)SENSOR_VCC, 1);

    // Wait for sensor to power up (BME280 startup time ~2ms, add margin)
    vTaskDelay(pdMS_TO_TICKS(50));

    // Configure I2C
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SENSOR_SDA;
    conf.scl_io_num = SENSOR_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read chip ID
    uint8_t chip_id;
    ret = i2c_read_bytes(BME280_REG_CHIP_ID, &chip_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read chip ID: %s", esp_err_to_name(ret));
        i2c_driver_delete(I2C_MASTER_NUM);
        return ret;
    }

    if (chip_id == BME280_CHIP_ID) {
        ESP_LOGI(TAG, "BME280 detected (chip ID: 0x%02X)", chip_id);
        s_has_humidity = true;
    } else if (chip_id == BMP280_CHIP_ID) {
        ESP_LOGW(TAG, "BMP280 detected (no humidity), chip ID: 0x%02X", chip_id);
        s_has_humidity = false;
    } else {
        ESP_LOGE(TAG, "Unknown chip ID: 0x%02X (expected 0x60 or 0x58)", chip_id);
        i2c_driver_delete(I2C_MASTER_NUM);
        return ESP_FAIL;
    }

    // Soft reset
    ret = i2c_write_byte(BME280_REG_RESET, BME280_RESET_CMD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Reset command failed (may be OK)");
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    // Read calibration data
    ret = read_calibration_data();
    if (ret != ESP_OK) {
        i2c_driver_delete(I2C_MASTER_NUM);
        return ret;
    }

    // Configure sensor: normal mode, oversampling x1 for all
    if (s_has_humidity) {
        // Must write ctrl_hum before ctrl_meas
        ret = i2c_write_byte(BME280_REG_CTRL_HUM, BME280_OSRS_H_X1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure humidity");
            i2c_driver_delete(I2C_MASTER_NUM);
            return ret;
        }
    }

    // Config: standby 1000ms, filter off
    ret = i2c_write_byte(BME280_REG_CONFIG, 0x05 << 5);  // t_sb = 1000ms
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set config register");
    }

    // ctrl_meas: temp x1, press x1, normal mode
    ret = i2c_write_byte(BME280_REG_CTRL_MEAS, BME280_OSRS_T_X1 | BME280_OSRS_P_X1 | BME280_MODE_NORMAL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure measurement");
        i2c_driver_delete(I2C_MASTER_NUM);
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "BME280 initialized successfully");

    return ESP_OK;
}

esp_err_t bme280_read(bme280_data_t* data)
{
    if (!s_initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    data->valid = false;

    // Read all data registers (8 bytes: press[2:0], temp[2:0], hum[1:0])
    uint8_t buf[8];
    esp_err_t ret = i2c_read_bytes(BME280_REG_DATA, buf, 8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sensor data");
        return ret;
    }

    // Parse raw values (20-bit pressure, 20-bit temperature, 16-bit humidity)
    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | ((int32_t)buf[2] >> 4);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | ((int32_t)buf[5] >> 4);
    int32_t adc_H = ((int32_t)buf[6] << 8) | (int32_t)buf[7];

    // Check for invalid readings
    if (adc_T == 0x80000 || adc_P == 0x80000) {
        ESP_LOGW(TAG, "Sensor returned invalid data");
        return ESP_FAIL;
    }

    // Compensate values (temperature must be first, sets t_fine)
    data->temperature = compensate_temperature(adc_T);
    data->pressure = compensate_pressure(adc_P);

    if (s_has_humidity) {
        data->humidity = compensate_humidity(adc_H);
    } else {
        data->humidity = 0.0f;
    }

    data->valid = true;

    ESP_LOGD(TAG, "T=%.2f°C, H=%.1f%%, P=%.1f hPa",
             data->temperature, data->humidity, data->pressure);

    return ESP_OK;
}

bool bme280_is_ready(void)
{
    return s_initialized;
}

void bme280_deinit(void)
{
    if (s_initialized) {
        // Put sensor in sleep mode
        i2c_write_byte(BME280_REG_CTRL_MEAS, 0x00);

        i2c_driver_delete(I2C_MASTER_NUM);
        s_initialized = false;

        ESP_LOGI(TAG, "BME280 deinitialized");
    }
}
