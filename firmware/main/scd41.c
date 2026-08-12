//
// Driver for the Sensirion SCD4x CO2 / temperature / humidity sensor.
// Implements periodic measurement mode with ASC (Automatic Self-Calibration).
// Verified against the SCD4x datasheet v1.7 (April 2025).
// Compatible with SCD40, SCD41, and SCD43.
//

#include "scd41.h"
#include "i2c_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "scd41";

#define SCD41_I2C_ADDRESS 0x62

// 16-bit command words (most significant byte transmitted first)
#define SCD41_CMD_START_PERIODIC_MEASUREMENT       0x21B1
#define SCD41_CMD_STOP_PERIODIC_MEASUREMENT        0x3F86
#define SCD41_CMD_READ_MEASUREMENT                 0xEC05
#define SCD41_CMD_GET_SERIAL_NUMBER                0x3682
#define SCD41_CMD_GET_SENSOR_VARIANT               0x202F
#define SCD41_CMD_SET_TEMPERATURE_OFFSET           0x241D
#define SCD41_CMD_GET_TEMPERATURE_OFFSET           0x2318
#define SCD41_CMD_SET_AMBIENT_PRESSURE             0xE000
#define SCD41_CMD_SET_AUTOMATIC_SELF_CALIBRATION   0x2416
#define SCD41_CMD_GET_AUTOMATIC_SELF_CALIBRATION   0x2313
#define SCD41_CMD_PERFORM_FORCED_RECALIBRATION     0x362F
#define SCD41_CMD_PERSIST_SETTINGS                 0x3615

// CRC-8: polynomial 0x31, init 0xFF, no reflection, final XOR 0x00
#define SCD41_CRC8_POLYNOMIAL 0x31
#define SCD41_CRC8_INIT       0xFF

// Periodic measurement read interval. The sensor produces one measurement every
// ~5 s. We poll at 1 s — on the first poll after a new measurement is ready, the
// sensor returns fresh data; on subsequent polls during the ~5 s window it
// returns the same data (which we ignore).
#define SCD41_READ_INTERVAL_S  5

// On-chip temperature offset (deg C). The SCD4x subtracts this from the raw
// reading and also uses it to compensate the reported humidity. Kept at the 4.0 C
// factory default; temperature is trimmed separately in software below.
#define SCD41_TEMPERATURE_OFFSET_C  4.0f

// Runtime calibration defaults
#define SCD41_DEFAULT_TEMP_TRIM_C   0.0f
#define SCD41_DEFAULT_PRESSURE_HPA  1013

// NVS keys for persisting calibration settings
#define NVS_NAMESPACE          "aircube"
#define NVS_KEY_TEMP_TRIM      "scd4x_trim"
#define NVS_KEY_PRESSURE       "scd4x_pres"
#define NVS_KEY_TEMP_TRIM_I32  "scd4x_tm32"  // stored as int32 millidegrees
#define NVS_KEY_PRESSURE_U16   "scd4x_pu16"

static bool scd41_present = false;
static bool scd41_data_valid = false;

static uint16_t scd41_co2 = 0;
static float scd41_temperature_c = 0.0f;
static float scd41_humidity = 0.0f;

// Runtime calibration (persisted to NVS)
static float    scd41_temp_trim_c = SCD41_DEFAULT_TEMP_TRIM_C;
static uint16_t scd41_ambient_pressure_hpa = SCD41_DEFAULT_PRESSURE_HPA;

/* ── Forward declarations ───────────────────────────────────────────── */
static esp_err_t scd41_send_command_arg(uint16_t command, uint16_t arg);

static void scd41_send_pressure_to_sensor(void)
{
    if (!scd41_present) return;
    uint16_t word = scd41_ambient_pressure_hpa;
    scd41_send_command_arg(SCD41_CMD_SET_AMBIENT_PRESSURE, word);
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_LOGI(TAG, "Ambient pressure set to %u hPa", scd41_ambient_pressure_hpa);
}

static void scd41_load_calibration_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;

    int32_t trim_milli = 0;
    if (nvs_get_i32(h, NVS_KEY_TEMP_TRIM_I32, &trim_milli) == ESP_OK) {
        scd41_temp_trim_c = (float)trim_milli / 1000.0f;
        ESP_LOGI(TAG, "Loaded temp trim from NVS: %.3f C", scd41_temp_trim_c);
    }

    int32_t pres = 0;
    if (nvs_get_i32(h, NVS_KEY_PRESSURE_U16, &pres) == ESP_OK && pres >= 700 && pres <= 1100) {
        scd41_ambient_pressure_hpa = (uint16_t)pres;
        ESP_LOGI(TAG, "Loaded pressure from NVS: %u hPa", scd41_ambient_pressure_hpa);
    }

    nvs_close(h);
}

static int64_t scd41_last_read_us = 0;
static uint16_t scd41_last_co2_word = 0;
static uint16_t scd41_last_t_word  = 0;
static uint16_t scd41_last_rh_word = 0;

static uint8_t scd41_crc8(const uint8_t *data, int len)
{
    uint8_t crc = SCD41_CRC8_INIT;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (uint8_t)((crc << 1) ^ SCD41_CRC8_POLYNOMIAL);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

static esp_err_t scd41_send_command(uint16_t command)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(command >> 8);
    buf[1] = (uint8_t)(command & 0xFF);
    return i2c_driver_write(SCD41_I2C_ADDRESS, buf, 2);
}

static esp_err_t scd41_send_command_arg(uint16_t command, uint16_t arg)
{
    uint8_t buf[5];
    buf[0] = (uint8_t)(command >> 8);
    buf[1] = (uint8_t)(command & 0xFF);
    buf[2] = (uint8_t)(arg >> 8);
    buf[3] = (uint8_t)(arg & 0xFF);
    buf[4] = scd41_crc8(&buf[2], 2);
    return i2c_driver_write(SCD41_I2C_ADDRESS, buf, 5);
}

// Apply the static temperature offset. Not persisted — re-applied every boot.
// The offset word is offset_c * 65535 / 175 per the datasheet.
static void scd41_apply_temperature_offset(float offset_c)
{
    uint16_t word = (uint16_t)((offset_c * 65535.0f) / 175.0f + 0.5f);
    // Must be called while sensor is idle (before start_periodic_measurement)
    if (scd41_send_command_arg(SCD41_CMD_SET_TEMPERATURE_OFFSET, word) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1));
        ESP_LOGI(TAG, "Temperature offset set to %.2f C", offset_c);
    } else {
        ESP_LOGW(TAG, "Failed to set temperature offset");
    }
}

// Send a command, wait execution time, read 'count' words with CRC per word.
static esp_err_t scd41_read_words(uint16_t command, uint16_t *words, int count, uint32_t delay_ms)
{
    esp_err_t ret = scd41_send_command(command);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    uint8_t buf[3 * count];
    int bytes = count * 3;
    ret = i2c_driver_read_raw(SCD41_I2C_ADDRESS, buf, bytes);
    if (ret != ESP_OK) return ret;

    for (int i = 0; i < count; i++) {
        const uint8_t *word = &buf[i * 3];
        if (scd41_crc8(word, 2) != word[2]) {
            ESP_LOGW(TAG, "CRC mismatch on word %d (cmd 0x%04X)", i, command);
            return ESP_ERR_INVALID_CRC;
        }
        words[i] = (uint16_t)((word[0] << 8) | word[1]);
    }
    return ESP_OK;
}

bool scd41_is_present(void)          { return scd41_present; }
uint16_t scd41_get_co2(void)         { return scd41_co2; }
float    scd41_get_temperature_c(void) { return scd41_temperature_c; }
float    scd41_get_humidity(void)    { return scd41_humidity; }
bool     scd41_has_data(void)        { return scd41_data_valid; }

float    scd41_get_temp_trim(void)   { return scd41_temp_trim_c; }
uint16_t scd41_get_pressure(void)    { return scd41_ambient_pressure_hpa; }

void scd41_set_temp_trim(float trim_c)
{
    scd41_temp_trim_c = trim_c;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        int32_t millideg = (int32_t)(trim_c * 1000.0f);
        nvs_set_i32(h, NVS_KEY_TEMP_TRIM_I32, millideg);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Temp trim set to %.3f C", trim_c);
}

bool scd41_set_pressure(uint16_t pressure_hpa)
{
    if (pressure_hpa < 700 || pressure_hpa > 1100) return false;
    scd41_ambient_pressure_hpa = pressure_hpa;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, NVS_KEY_PRESSURE_U16, (int32_t)pressure_hpa);
        nvs_commit(h);
        nvs_close(h);
    }
    scd41_send_pressure_to_sensor();
    return true;
}

void scd41_init(void)
{
    scd41_present = false;
    scd41_data_valid = false;

    // Load persisted calibration before probing the sensor
    scd41_load_calibration_from_nvs();
    scd41_data_valid = false;

    // Check if sensor is on the I2C bus
    if (!i2c_driver_probe(SCD41_I2C_ADDRESS)) {
        ESP_LOGI(TAG, "SCD4x not present");
        return;
    }

    // Power-on wait: sensor needs up to 30 ms after VDD stabilizes.
    // Per datasheet §3.1, the sensor is ready after this delay.
    vTaskDelay(pdMS_TO_TICKS(30));

    // Send stop to guarantee idle state (valid from any state)
    scd41_send_command(SCD41_CMD_STOP_PERIODIC_MEASUREMENT);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Verify sensor identity by reading serial number
    uint16_t serial[3] = {0};
    esp_err_t ret = scd41_read_words(SCD41_CMD_GET_SERIAL_NUMBER, serial, 3, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SCD4x not detected (serial read failed)");
        return;
    }

    // Read sensor variant (SCD40 = 0b0000, SCD41 = 0b0001, SCD43 = 0b0101)
    uint16_t variant = 0;
    ret = scd41_read_words(SCD41_CMD_GET_SENSOR_VARIANT, &variant, 1, 1);
    if (ret == ESP_OK) {
        uint8_t vc = (variant >> 12) & 0x0F;
        const char *name = (vc == 0x0) ? "SCD40" : (vc == 0x1) ? "SCD41" :
                           (vc == 0x5) ? "SCD43" : "unknown SCD4x";
        ESP_LOGI(TAG, "%s detected (serial %04X%04X%04X)", name,
                 serial[0], serial[1], serial[2]);
    } else {
        ESP_LOGI(TAG, "SCD4x detected (serial %04X%04X%04X)",
                 serial[0], serial[1], serial[2]);
    }

    // Configure sensor — all commands below must be sent while SENSOR IS IDLE
    // (i.e., before start_periodic_measurement), per datasheet §3.5.3.

    // 1. Temperature offset: compensate for self-heating from periodic operation
    //    and enclosure thermal effects. Factory default is 4.0 C.
    scd41_apply_temperature_offset(SCD41_TEMPERATURE_OFFSET_C);

    // 2. Ambient pressure compensation (altitude correction).
    //    Per datasheet §3.5.8, set the local average pressure for accurate CO2.
    scd41_send_pressure_to_sensor();

    // 3. ASC (Automatic Self-Calibration) — enabled by factory default.
    //    Explicitly enable it to be sure. Requires periodic measurement mode
    //    and ~7 days of continuous operation with periodic fresh air exposure.
    //    Per datasheet §3.5.6: ASC continuously corrects the CO2 baseline
    //    using the lowest reading observed over a rolling ~1-week window.
    {
        uint16_t asc_enable = 1;
        scd41_send_command_arg(SCD41_CMD_SET_AUTOMATIC_SELF_CALIBRATION, asc_enable);
        vTaskDelay(pdMS_TO_TICKS(1));
        ESP_LOGI(TAG, "ASC (automatic self-calibration) enabled");
    }

    // 4. Persist settings to EEPROM so they survive power cycles.
    //    Per datasheet §3.5: persist_settings (0x3615) saves temperature offset,
    //    ASC state, and ambient pressure. Execute command should not exceed 800 ms.
    //    Note: EEPROM has a limited write endurance; the datasheet allows
    //    persisting on every boot because the expected lifetime far exceeds
    //    the product lifecycle at typical boot frequencies.
    scd41_send_command(SCD41_CMD_PERSIST_SETTINGS);
    vTaskDelay(pdMS_TO_TICKS(800));

    // 5. Start periodic measurement mode.
    //    Per datasheet §3.5.1: the sensor automatically measures at ~5 s intervals.
    //    ASC only functions in this mode (§3.5.6). The first measurement is
    //    available after ~5 s; subsequent measurements update every ~5 s.
    ret = scd41_send_command(SCD41_CMD_START_PERIODIC_MEASUREMENT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start periodic measurement");
        return;
    }

    scd41_present = true;
    ESP_LOGI(TAG, "Periodic measurement started (read every %d s, ASC enabled)",
             SCD41_READ_INTERVAL_S);
}

bool scd41_poll(void)
{
    if (!scd41_present) return false;

    int64_t now = esp_timer_get_time();

    // Don't poll faster than the sensor produces new data (~5 s).
    // read_measurement returns the same buffered result between updates.
    if ((now - scd41_last_read_us) < (int64_t)SCD41_READ_INTERVAL_S * 1000000LL) {
        return false;
    }

    // Read measurement buffers from the sensor (non-blocking — data is ready).
    // Per datasheet §3.5.2: read_measurement returns the latest buffered
    // result; the sensor continues measuring in the background.
    uint16_t words[3] = {0};
    esp_err_t ret = scd41_read_words(SCD41_CMD_READ_MEASUREMENT, words, 3, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Read measurement failed: %s", esp_err_to_name(ret));
        return false;
    }

    scd41_last_co2_word = words[0];
    scd41_last_t_word  = words[1];
    scd41_last_rh_word = words[2];
    scd41_last_read_us = now;

    // Convert raw values to engineering units (per datasheet §3.5.2):
    //   CO2  [ppm] = word[0]
    //   T    [°C]  = -45 + 175 * word[1] / 65535
    //   RH   [%]   = 100 * word[2] / 65535
    scd41_co2 = words[0];
    scd41_temperature_c = -45.0f + 175.0f * ((float)words[1] / 65535.0f)
                          + scd41_temp_trim_c;
    scd41_humidity = 100.0f * ((float)words[2] / 65535.0f);
    scd41_data_valid = true;

    return true;
}

bool scd41_perform_frc(uint16_t target_ppm)
{
    if (!scd41_present) {
        ESP_LOGW(TAG, "FRC failed: sensor not present");
        return false;
    }
    if (target_ppm < 400 || target_ppm > 2000) {
        ESP_LOGW(TAG, "FRC failed: target %u ppm out of range (400-2000)", target_ppm);
        return false;
    }

    ESP_LOGI(TAG, "FRC: calibrating to %u ppm CO2...", target_ppm);

    // Per datasheet §3.5.7: send perform_forced_recalibration with the
    // reference CO2 value as argument. The sensor must be in periodic
    // measurement mode for at least 3 minutes. The command returns the
    // FRC correction value (0xFFFF on failure).
    esp_err_t ret = scd41_send_command_arg(SCD41_CMD_PERFORM_FORCED_RECALIBRATION,
                                           target_ppm);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FRC failed: I2C error");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(400));

    uint8_t buf[3] = {0};
    ret = i2c_driver_read_raw(SCD41_I2C_ADDRESS, buf, 3);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FRC failed: read error");
        return false;
    }
    uint16_t frc_response = (uint16_t)((buf[0] << 8) | buf[1]);

    if (frc_response == 0xFFFF) {
        ESP_LOGW(TAG, "FRC failed: sensor rejected (run >3 min in periodic mode first)");
        return false;
    }

    ESP_LOGI(TAG, "FRC successful: correction = %d ppm (target %u ppm)",
             (int16_t)frc_response, target_ppm);
    return true;
}
