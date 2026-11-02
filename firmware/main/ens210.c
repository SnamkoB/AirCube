
//
// Driver for the ScioSense ENS210 temperature / humidity sensor.
// Uses the datasheet-recommended single-shot run mode (SENS_RUN = 0):
// each sample is triggered via SENS_START, the conversion is awaited via
// SENS_STAT, and the result is CRC-7 validated. Automatic low power keeps
// the device in standby between conversions, minimizing self-heating.
// Verified against the ENS210 datasheet SC-000897-DS-12 / Version 6
// (2025-11-18).
//

#include "ens210.h"
#include "i2c_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define ENS210_I2C_ADDRESS 0x43
#define ENS210_PART_ID     0x0210

#define ENS210_REG_PART_ID   0x00
#define ENS210_REG_SYS_CTRL  0x10
#define ENS210_REG_SYS_STAT  0x11
#define ENS210_REG_SENS_RUN  0x21
#define ENS210_REG_SENS_START 0x22
#define ENS210_REG_SENS_STAT 0x24
#define ENS210_REG_T_VAL     0x30
#define ENS210_REG_H_VAL     0x33

// CRC-7 per datasheet §Computing CRC-7: polynomial x^7+x^3+1 (0x89),
// initial vector 0x7F, 17-bit payload (data + valid flag).
#define ENS210_CRC7_POLY   0x89
#define ENS210_CRC7_IVEC   0x7F

// Sample cadence: match the SCD4x ~5 s natural readout. A single-shot
// conversion is only triggered when a fresh sample is due; in between,
// the last validated values are returned.
#define ENS210_SAMPLE_INTERVAL_S  5

// T+RH single-shot conversion: 122-130 ms typical (datasheet Figure 9).
// Allow generous margin for the completion poll.
#define ENS210_CONV_TIMEOUT_MS    250

uint8_t ens210_t[2];
uint8_t ens210_h[2];

float temperature_K;
float temperature_C;
float temperature_F;
float humidity_percentage;

static bool ens210_present = false;
static int64_t ens210_last_sample_us = 0;

bool ens210_is_present(void){
    return ens210_present;
}

void ens210_get_envir(uint8_t * t, uint8_t * h){
    t[0] = ens210_t[0];
    t[1] = ens210_t[1];

    h[0] = ens210_h[0];
    h[1] = ens210_h[1];
}

void ens210_set_mode(void){

}

// 0 = F
// 1 = C
// 2 = K
float ens210_get_temperature(uint8_t type){
    float temperature = 0;

    switch (type) {
        case 0:
            temperature = temperature_F;
            break;
        case 1:
            temperature = temperature_C;
            break;
        case 2:
            temperature = temperature_K;
            break;
        default:
            temperature = temperature_F;
            break;
    }

    return temperature;
}

float ens210_get_humidity(void){
    return humidity_percentage;
}

uint8_t ens210_get_status(void){
    uint8_t i2c_data[1];
    uint8_t i2c_byte_address[1];

    i2c_byte_address[0] = ENS210_REG_SYS_STAT;
    i2c_data[0] = 0;
    i2c_driver_read(ENS210_I2C_ADDRESS, i2c_byte_address, 1, i2c_data, 1);

    return i2c_data[0];
}

void ens210_deinit(void){
    uint8_t i2c_data[2];
    i2c_data[0] = ENS210_REG_SYS_CTRL;
    i2c_data[1] = 0x01;

    i2c_driver_write(ENS210_I2C_ADDRESS, i2c_data, 2);
}

// CRC-7 over the 17-bit payload (16 data bits + valid bit), exactly as the
// datasheet example code (returns the 7-bit CRC).
static uint8_t ens210_crc7(uint32_t val)
{
    uint32_t pol = (uint32_t)ENS210_CRC7_POLY << 9;   /* align with 17-bit payload */
    uint32_t bit = 1UL << 16;

    val = (val << 7) | ENS210_CRC7_IVEC;
    bit = bit << 7;
    pol = pol << 7;

    while (bit & (0x1FFFFUL << 7)) {
        if (bit & val) {
            val ^= pol;
        }
        bit >>= 1;
        pol >>= 1;
    }
    return (uint8_t)(val & 0x7F);
}

// Trigger one combined T+RH single-shot conversion, wait for completion
// (SENS_STAT idle), then read and CRC-validate both results.
static bool ens210_single_shot(void)
{
    uint8_t i2c_data[2];
    uint8_t i2c_byte_address[1];

    // Start both sensors: write 0x03 to SENS_START (0x22).
    i2c_data[0] = ENS210_REG_SENS_START;
    i2c_data[1] = 0x03;
    if (i2c_driver_write(ENS210_I2C_ADDRESS, i2c_data, 2) != ESP_OK) {
        ESP_LOGW("ens210", "Failed to start single-shot measurement");
        return false;
    }

    // Wait for the sensors to finish: SENS_STAT (0x24) T_STAT/H_STAT return
    // to 0 when idle. Typical T+RH single-shot conversion: 122-130 ms.
    int waited_ms = 0;
    uint8_t sens_stat = 0xFF;
    do {
        vTaskDelay(pdMS_TO_TICKS(10));
        waited_ms += 10;

        i2c_byte_address[0] = ENS210_REG_SENS_STAT;
        if (i2c_driver_read(ENS210_I2C_ADDRESS, i2c_byte_address, 1,
                            &sens_stat, 1) != ESP_OK) {
            ESP_LOGW("ens210", "SENS_STAT read failed");
            return false;
        }
    } while ((sens_stat & 0x03) != 0 && waited_ms < ENS210_CONV_TIMEOUT_MS);

    if ((sens_stat & 0x03) != 0) {
        ESP_LOGW("ens210", "Single-shot conversion timed out");
        return false;
    }

    // Read T_VAL + H_VAL in one 6-byte transaction (datasheet example).
    uint8_t buf[6];
    i2c_byte_address[0] = ENS210_REG_T_VAL;
    if (i2c_driver_read(ENS210_I2C_ADDRESS, i2c_byte_address, 1, buf, 6) != ESP_OK) {
        ESP_LOGW("ens210", "Readout failed");
        return false;
    }

    // 24-bit registers (little endian): [data LSB, data MSB, VALID+CRC].
    uint32_t t_val = ((uint32_t)buf[2] << 16) | ((uint32_t)buf[1] << 8) | buf[0];
    uint32_t h_val = ((uint32_t)buf[5] << 16) | ((uint32_t)buf[4] << 8) | buf[3];

    uint32_t t_payl = t_val & 0x1FFFF;
    uint8_t  t_crc  = (uint8_t)((t_val >> 17) & 0x7F);
    uint32_t h_payl = h_val & 0x1FFFF;
    uint8_t  h_crc  = (uint8_t)((h_val >> 17) & 0x7F);

    if ((t_val & 0x10000) == 0 || ens210_crc7(t_payl) != t_crc) {
        ESP_LOGW("ens210", "Temperature data invalid (valid flag or CRC-7)");
        return false;
    }
    if ((h_val & 0x10000) == 0 || ens210_crc7(h_payl) != h_crc) {
        ESP_LOGW("ens210", "Humidity data invalid (valid flag or CRC-7)");
        return false;
    }

    ens210_t[0] = buf[0];
    ens210_t[1] = buf[1];
    ens210_h[0] = buf[3];
    ens210_h[1] = buf[4];

    float TinK = (float)(t_payl & 0xFFFF) / 64.0f;   // 1/64 K per LSB
    float TinC = TinK - 273.15f;                     // Celsius
    float TinF = TinC * 1.8f + 32.0f;                // Fahrenheit

    temperature_K = TinK;
    temperature_C = TinC;
    temperature_F = TinF;
    humidity_percentage = (float)(h_payl & 0xFFFF) / 512.0f;  // 1/512 %RH

    ESP_LOGI("ens210", "%5.1fK %4.1fC %4.1fF", TinK, TinC, TinF);
    return true;
}

bool ens210_read_envir(void){
    if (!ens210_present) {
        return false;
    }

    // Sample at the same ~5 s cadence as the SCD4x. The single-shot
    // conversion is triggered only when a fresh sample is due. A zero
    // timestamp means "never sampled" - sample immediately.
    int64_t now = esp_timer_get_time();
    if (ens210_last_sample_us != 0 &&
        (now - ens210_last_sample_us) < (int64_t)ENS210_SAMPLE_INTERVAL_S * 1000000LL) {
        return false;
    }
    ens210_last_sample_us = now;

    if (!ens210_single_shot()) {
        ESP_LOGW("ens210", "Sample failed - keeping previous values");
        return false;
    }
    return true;
}

void ens210_init(void){
    uint8_t i2c_data[2];
    uint8_t i2c_byte_address[1];

    ens210_present = false;
    ens210_last_sample_us = 0;

    // Quietly check for an ACK at the ENS210 address first. On Pro hardware the
    // ENS210 may be absent, so a no-ACK is a normal "not present" result, not a
    // fault - skip cleanly without emitting I2C error logs.
    if (!i2c_driver_probe(ENS210_I2C_ADDRESS)) {
        ESP_LOGI("ens210", "ENS210 not present");
        return;
    }

    // Wake the device out of power-on standby (disable low power) so the
    // PART_ID register is readable, then wait tbooting (~1 ms, plus margin).
    i2c_data[0] = ENS210_REG_SYS_CTRL;
    i2c_data[1] = 0x00; // Disable low power mode (device stays in active state)
    i2c_driver_write(ENS210_I2C_ADDRESS, i2c_data, 2);

    vTaskDelay(10 / portTICK_PERIOD_MS);

    // Probe for the ENS210 via its PART_ID register (0x00 -> 0x0210, little endian).
    // On Pro hardware the ENS210 may be absent; skip configuration if not found so
    // we don't spam the I2C bus with failed transactions.
    uint8_t part[2] = {0};
    i2c_byte_address[0] = ENS210_REG_PART_ID;
    if (i2c_driver_read(ENS210_I2C_ADDRESS, i2c_byte_address, 1, part, 2) == ESP_OK) {
        uint16_t part_id = (uint16_t)part[0] | ((uint16_t)part[1] << 8);
        // Match only the low 12 bits (0x210). On our hardware the top nibble reads
        // back as 0xA (PART_ID 0xA210) rather than the datasheet's 0x0; treat that
        // as a benign variant/revision marker so the ENS210 is still recognized.
        ens210_present = ((part_id & 0x0FFF) == (ENS210_PART_ID & 0x0FFF));
        if (!ens210_present) {
            ESP_LOGW("ens210", "Unexpected PART_ID 0x%04X (expected low 12 bits 0x%03X)",
                     part_id, ENS210_PART_ID & 0x0FFF);
        } else if (part_id != ENS210_PART_ID) {
            ESP_LOGI("ens210", "ENS210 PART_ID 0x%04X (accepted via low-12-bit match)", part_id);
        }
    } else {
        ens210_present = false;
    }

    if (!ens210_present) {
        ESP_LOGI("ens210", "ENS210 not present");
        return;
    }

    // Configure per the datasheet recommendation: single-shot run mode for
    // both sensors (SENS_RUN = 0x00, also the power-on default) and automatic
    // low power enabled (SYS_CTRL LOW_POWER = 1). The device then goes back
    // to standby after each triggered conversion, which minimizes self-heating
    // and improves temperature accuracy.
    i2c_data[0] = ENS210_REG_SENS_RUN;
    i2c_data[1] = 0x00; // T and RH in single-shot mode
    i2c_driver_write(ENS210_I2C_ADDRESS, i2c_data, 2);

    i2c_data[0] = ENS210_REG_SYS_CTRL;
    i2c_data[1] = 0x01; // LOW_POWER = 1 (standby between conversions)
    i2c_driver_write(ENS210_I2C_ADDRESS, i2c_data, 2);

    ESP_LOGI("ens210", "ENS210 initialized in single-shot mode (samples every %d s)",
             ENS210_SAMPLE_INTERVAL_S);
}
