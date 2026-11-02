#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "led_color_lib.h"

#include "led.h"
#include "ens210.h"
#include "ens16x_driver.h"
#include "scd41.h"
#include "i2c_driver.h"
#include "serial_protocol.h"
#include "sensor_ema.h"
#include "button.h"
#include "esp_timer.h"

#if CONFIG_AIRCUBE_NETWORK_ZIGBEE
#include "zigbee.h"
#define net_init                zigbee_init
#define net_update_sensors      zigbee_update_sensors
#define net_is_connected        zigbee_is_connected
#define net_is_commissioning    zigbee_is_pairing
#elif CONFIG_AIRCUBE_NETWORK_THREAD
#include "thread_network.h"
#define net_init                thread_network_init
#define net_update_sensors      thread_network_update_sensors
#define net_is_connected        thread_network_is_connected
#define net_is_commissioning    thread_network_is_commissioning
#endif

static const char *TAG = "main";

#define SENSOR_TASK_STACK_SIZE 4096
#define SENSOR_TASK_PRIORITY 5
#define COMMAND_TASK_STACK_SIZE 4096
#define COMMAND_TASK_PRIORITY 4

// Configurable sensor readout period (default 1000ms)
static uint32_t sensor_readout_period_ms = 1000;
static SemaphoreHandle_t readout_period_mutex = NULL;

// LED color mapping: continuous green->red gradient from whichever of
// VOC Level / CO2 Level is currently worse.
#define AQI_MIN 0
#define AQI_MAX 200
#define AQI_GREEN_THRESHOLD 10  // Values 0-10 are pure green

#define HUE_GREEN  21845  // 120 deg - pure green
#define MODEL_NAME  "aircube"

// Shared 0-500 severity checkpoints. Both the VOC and CO2 threshold tables
// below map their raw units onto these same bands, so the two resulting
// levels are directly comparable (see co2_calculate()/aqi_calculate()).
// Shared severity bands (ENS16X disabled — unused until re-enabled)
// static const uint16_t AQI_LEVEL_BANDS[6] = {
//     0,
//     15,
//     50,
//     100,
//     200,
//     500
// };

// VOC Level values at each TVOC band edge (matches published VOC table):
//   0-65 ppb      -> VOC Level 0-15   (Excellent)
//   65-220 ppb    -> VOC Level 15-50  (Good)
//   220-650 ppb   -> VOC Level 50-100 (Moderate)
//   650-2200 ppb  -> VOC Level 100-200 (Poor)
//   2200-5500 ppb -> VOC Level 200-500 (Unhealthy)
// Driven by TVOC alone; eCO2 is reported separately as raw ppm.
// /* ENS16X disabled */
// static const int AQI_TVOC_THRESHOLDS_PPB[5] = { 65, 220, 650, 2200, 5500 };

// Canonical VOC Level (TVOC-derived) and CO2 Level (SCD4x-derived)
// for LED color mapping. The LED loop picks whichever is worse each tick.
static int current_voc_level = 0;
static int current_co2_level = 0;

// Static variables for smooth LED color transitions
static float current_hue = 21845.0f;
static uint16_t target_hue = 21845;
#define TRANSITION_SPEED 0.02f

// Getter and setter for sensor readout period (for serial_protocol.c)
uint32_t get_sensor_readout_period_ms(void)
{
    uint32_t period = 1000;
    if (readout_period_mutex != NULL) {
        if (xSemaphoreTake(readout_period_mutex, portMAX_DELAY) == pdTRUE) {
            period = sensor_readout_period_ms;
            xSemaphoreGive(readout_period_mutex);
        }
    }
    return period;
}

void set_sensor_readout_period_ms(uint32_t period)
{
    if (readout_period_mutex != NULL) {
        if (xSemaphoreTake(readout_period_mutex, portMAX_DELAY) == pdTRUE) {
            sensor_readout_period_ms = period;
            xSemaphoreGive(readout_period_mutex);
        }
    }
}

/**
 * @brief Map VOC Level value to hue
 *
 * Maps canonical VOC Level (TVOC-derived, 0-500) to LED hue:
 * - VOC Level 0-10: pure green (no color change)
 * - VOC Level 10-200: smooth gradient from green to red
 * - VOC Level 200+: full red (handled by caller)
 *
 * @param aqi VOC Level value
 * @return 16-bit hue value (21845 = green, 0 = red)
 */
static uint16_t aqi_to_hue(int aqi)
{
    if (aqi < AQI_MIN) aqi = AQI_MIN;
    if (aqi > AQI_MAX) aqi = AQI_MAX;

    if (aqi <= AQI_GREEN_THRESHOLD) {
        return HUE_GREEN;
    }

    float ratio = (float)(aqi - AQI_GREEN_THRESHOLD) / (float)(AQI_MAX - AQI_GREEN_THRESHOLD);
    uint16_t hue = HUE_GREEN - (uint16_t)(ratio * HUE_GREEN);

    return hue;
}

#if 0 // ENS16X disabled — VOC mapping helpers not currently used

static float value_to_level_pos(int value, const int thresholds[5])
{
    if (value <= 0) {
        return 0.0f;
    }
    if (value >= thresholds[4]) {
        return 5.0f;
    }
    if (value < thresholds[0]) {
        return (float)value / (float)thresholds[0];
    }
    for (int i = 0; i < 4; i++) {
        if (value < thresholds[i + 1]) {
            float span = (float)(thresholds[i + 1] - thresholds[i]);
            float frac = (float)(value - thresholds[i]) / span;
            return (float)(i + 1) + frac;
        }
    }
    return 5.0f;
}

static uint16_t level_from_thresholds(int value, const int thresholds[5], const uint16_t bands[6])
{
    float pos = value_to_level_pos(value, thresholds);
    if (pos <= 0.0f) {
        return bands[0];
    }
    if (pos >= 5.0f) {
        return bands[5];
    }

    int low = (int)pos;
    if (low > 4) {
        low = 4;
    }
    float frac = pos - (float)low;

    int32_t a_low  = (int32_t)bands[low];
    int32_t a_high = (int32_t)bands[low + 1];
    int32_t level = a_low + (int32_t)((a_high - a_low) * frac);

    if (level < 0) {
        level = 0;
    }
    if (level > 500) {
        level = 500;
    }
    return (uint16_t)level;
}

#endif // ENS16X disabled

/**
 * @brief Compute the canonical AirCube VOC Level from TVOC.
 *
 * Maps TVOC ppb to VOC Level 0-500 using fixed indoor-air bands (see AQI_LEVEL_BANDS).
 * TVOC-only; eCO2 is reported separately. LED color follows this VOC Level via
 * aqi_to_hue() (green at 0-10, gradient to red at 200+).
 *
 * @param etvoc Equivalent TVOC in ppb
 * @return VOC Level value in [0, 500]
 */
// /* ENS16X disabled — aqi_calculate not currently used */
// static uint16_t aqi_calculate(int etvoc)
// {
//     return level_from_thresholds(etvoc, AQI_TVOC_THRESHOLDS_PPB, AQI_LEVEL_BANDS);
// }

// CO2 ppm → CO2 Level: linear gradient from 500 ppm (pure green) to
// 1500 ppm (full red, AQI_MAX). Below 500 = green, above 1500 = red.
static uint16_t co2_calculate(int co2_ppm)
{
    if (co2_ppm <= 500)   return 0;
    if (co2_ppm >= 1500)  return AQI_MAX;
    return (uint16_t)((co2_ppm - 500) * (uint32_t)AQI_MAX / 1000);
}

/**
 * @brief Build ENS16X temperature/humidity compensation bytes from floats.
 *
 * The ENS16X TEMP_IN/RH_IN registers (written by ens16x_write_ens210_data())
 * use the same encoding as the ENS210 raw output: temperature in 1/64 K and
 * relative humidity in 1/512 %, each a little-endian uint16.
 */
// /* ENS16X disabled — compose_ens16x_compensation not currently used */
// static void compose_ens16x_compensation(float temp_c, float humidity,
//                                         uint8_t t[2], uint8_t h[2])
// {
//     uint16_t t_raw = (uint16_t)((temp_c + 273.15f) * 64.0f);
//     uint16_t h_raw = (uint16_t)(humidity * 512.0f);
//     t[0] = (uint8_t)(t_raw & 0xFF);
//     t[1] = (uint8_t)(t_raw >> 8);
//     h[0] = (uint8_t)(h_raw & 0xFF);
//     h[1] = (uint8_t)(h_raw >> 8);
// }

// Command processing task
void command_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Command task started");

    while (1) {
        serial_process_commands();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// Sensor reading task
void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started");

    esp_task_wdt_add(NULL);

#if CONFIG_AIRCUBE_NETWORK_ZIGBEE
    sensor_ema_t scd4x_ema = {0};
    sensor_ema_t ens210_ema = {0};
#endif

    while (1) {
        esp_task_wdt_reset();

        // SCD4x provides temp/RH/CO2; ENS210 for comparison (or fallback).
        float temp_c = 0.0f;
        float humidity = 0.0f;
        uint8_t ens210_status = 0;
        uint16_t co2_ppm = 0;
        float lux = 0.0f;
        bool scd4x_connected = scd41_is_present();

        // ENS210 comparison readout (if fitted alongside SCD4x).
        bool ens210_compare = false;
        float ens210_temp_c = 0.0f;
        float ens210_humidity = 0.0f;

        if (scd4x_connected) {
            bool fresh_scd4x = scd41_poll();
            temp_c   = scd41_get_temperature_c();
            humidity = scd41_get_humidity();
            co2_ppm  = scd41_get_co2();
            ens210_status = 0x01;
#if CONFIG_AIRCUBE_NETWORK_ZIGBEE
            if (fresh_scd4x) {
                sensor_ema_update(&scd4x_ema, esp_timer_get_time(),
                                  temp_c, humidity, (float)co2_ppm);
            }
#else
            (void)fresh_scd4x;
#endif
        }

        // If an ENS210 is fitted, read it for comparison (or as fallback)
        if (ens210_is_present()) {
            bool fresh_ens210 = ens210_read_envir();
            ens210_temp_c = ens210_get_temperature(1) - 2;
            ens210_humidity = ens210_get_humidity();
            ens210_compare = true;
#if CONFIG_AIRCUBE_NETWORK_ZIGBEE
            if (fresh_ens210) {
                sensor_ema_update(&ens210_ema, esp_timer_get_time(),
                                  ens210_temp_c, ens210_humidity, 0.0f);
            }
#else
            (void)fresh_ens210;
#endif

            // Fallback: use ENS210 if SCD4x is absent
            if (!scd4x_connected) {
                temp_c   = ens210_temp_c;
                humidity = ens210_humidity;
                ens210_status = ens210_get_status();
            }
        }

        if (!scd4x_connected && !ens210_is_present()) {
            ESP_LOGW(TAG, "No sensors detected on I2C bus");
        }

        // Only calculate CO2 level once SCD4x has produced a valid measurement
        if (scd4x_connected && scd41_has_data()) {
            current_co2_level = co2_calculate((int)co2_ppm);
        }

        // /* ENS16X disabled */
        int etvoc = 0, eco2 = 0, aqi = 0;
        current_voc_level = 0;

        ESP_LOGI(TAG, "=== Sensor Data ===");
        if (scd4x_connected) {
            ESP_LOGI(TAG, "SCD4x - Temperature: %.2f°C / %.2f°F, Humidity: %.2f%%, CO2: %u ppm",
                     temp_c, temp_c * 1.8f + 32.0f, humidity, co2_ppm);
        } else {
            ESP_LOGI(TAG, "SCD4x - not connected");
            if (ens210_compare) {
                ESP_LOGI(TAG, "ENS210 (fallback) - Temperature: %.2f°C / %.2f°F, Humidity: %.2f%%",
                         temp_c, temp_c * 1.8f + 32.0f, humidity);
            }
        }
        if (ens210_compare) {
            float dT_c = ens210_temp_c - temp_c;
            float dRh = ens210_humidity - humidity;
            if (scd4x_connected) {
                ESP_LOGI(TAG, "ENS210 - Temperature: %.2f°C / %.2f°F, Humidity: %.2f%% (compare: dT=%+.2f°C, dRH=%+.2f%%)",
                         ens210_temp_c, ens210_temp_c * 1.8f + 32.0f, ens210_humidity,
                         dT_c, dT_c * 1.8f, dRh);
            } else {
                ESP_LOGI(TAG, "ENS210 - Temperature: %.2f°C / %.2f°F, Humidity: %.2f%%",
                         ens210_temp_c, ens210_temp_c * 1.8f + 32.0f, ens210_humidity);
            }
        }

        // Send sensor data as JSON over serial
        serial_send_sensor_data(ens210_status, temp_c, humidity,
                               MODEL_NAME, (int)co2_ppm);

        // Push sensor data to Zigbee every 10 seconds
        {
            static TickType_t last_zb_update = 0;
            TickType_t now = xTaskGetTickCount();
            if ((now - last_zb_update) >= pdMS_TO_TICKS(10000)) {
                last_zb_update = now;
#if CONFIG_AIRCUBE_NETWORK_ZIGBEE
                // Don't publish startup zeros. Keep the two sensor filters
                // independent so ENS210 fallback never inherits SCD4x state.
                const sensor_ema_t *primary = scd4x_connected ? &scd4x_ema : &ens210_ema;
                if (primary->initialized) {
                    float compare_temp = ens210_ema.initialized ? ens210_ema.temperature_c : 0.0f;
                    float compare_hum = ens210_ema.initialized ? ens210_ema.humidity : 0.0f;
                    ESP_LOGI(TAG, "Zigbee EMA (tau=30s): CO2 %.0f ppm, T %.2f C, RH %.2f%%; ENS210 T %.2f C, RH %.2f%%",
                             scd4x_ema.initialized ? scd4x_ema.co2_ppm : 0.0f,
                             primary->temperature_c, primary->humidity,
                             compare_temp, compare_hum);
                    net_update_sensors(primary->temperature_c, primary->humidity,
                                       eco2, etvoc, aqi,
                                       scd4x_ema.initialized ? scd4x_ema.co2_ppm : 0.0f,
                                       lux, compare_temp, compare_hum);
                }
#else
                net_update_sensors(temp_c, humidity, eco2, etvoc, aqi,
                                   (float)co2_ppm, lux,
                                   ens210_temp_c, ens210_humidity);
#endif
            }
        }

        // Wait for configurable period before next reading
        uint32_t period = get_sensor_readout_period_ms();
        vTaskDelay(period / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "AirCube");

    ESP_LOGI(TAG, "LED color: continuous green->red from canonical VOC Level (TVOC-derived)");

    // Configure power management
    // ESP32-H2 valid max frequencies: 96, 64, or 48 MHz. Min = XTAL = 32 MHz.
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 48,
        .min_freq_mhz = 32,
        .light_sleep_enable = false
    };

    esp_err_t ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure power management: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Power management configured");
    }

    // Initialize NVS (Non-Volatile Storage) for saving settings
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize I2C driver (must be done before initializing sensors)
    if (i2c_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C driver");
        return;
    }

    // Create mutex for readout period
    readout_period_mutex = xSemaphoreCreateMutex();
    if (readout_period_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create readout period mutex");
        return;
    }

    // Initialize serial protocol
    serial_protocol_init();

    // Initialize LED control system
    led_init();

    // Set initial LED color to blue (idle until Zigbee connection)
    led_set_color(LED_COLOR_BLUE);

    // Initialize button for brightness control
    button_init();

    // Initialize ENS210 temperature and humidity sensor
    ens210_init();
    ESP_LOGI(TAG, "ENS210 %s", ens210_is_present() ? "present" : "not present");

    // /* ENS16X disabled */
    // ens16x_init();
    // ESP_LOGI(TAG, "ENS16X initialized");

    // Initialize SCD4x (works for both SCD40 and SCD41)
    scd41_init();
    ESP_LOGI(TAG, "SCD4x %s", scd41_is_present() ? "present" : "not present");

    // Start the network stack (Zigbee or Thread).
    net_init();

    // Create command processing task
    xTaskCreate(command_task, "command_task", COMMAND_TASK_STACK_SIZE, NULL,
                COMMAND_TASK_PRIORITY, NULL);
    ESP_LOGI(TAG, "Command task created");

    // Create sensor reading task
    xTaskCreate(sensor_task, "sensor_task", SENSOR_TASK_STACK_SIZE, NULL,
                SENSOR_TASK_PRIORITY, NULL);
    ESP_LOGI(TAG, "Sensor task created");

    // Main loop for LED color based on VOC/CO2 Level (with commissioning override)
    bool was_connected = net_is_connected();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));

        bool connected = net_is_connected();
        bool commissioning = net_is_commissioning();

        // Mode cue: three quick green blinks when network joins
        if (connected && !was_connected) {
            for (int i = 0; i < 3; i++) {
                led_set_color(LED_COLOR_GREEN);
                vTaskDelay(pdMS_TO_TICKS(120));
                led_set_color(LED_COLOR_OFF);
                vTaskDelay(pdMS_TO_TICKS(120));
            }
        }
        was_connected = connected;

        // Commissioning mode: flash blue at 2 Hz
        if (commissioning) {
            uint32_t tick_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            bool on = ((tick_ms / 250) % 2) == 0;
            led_set_color(on ? LED_COLOR_BLUE : LED_COLOR_OFF);
            continue;
        }

        // Idle (not connected): solid blue until network joined
        if (!connected) {
            led_set_color(LED_COLOR_BLUE);
            continue;
        }

        // Normal mode: continuous green->red from whichever is worse
        int display_level = current_voc_level;
        bool co2_is_worse = false;
        if (current_co2_level > display_level) {
            display_level = current_co2_level;
            co2_is_worse = true;
        }

        static bool was_co2_worse = false;
        if (co2_is_worse != was_co2_worse) {
            ESP_LOGI(TAG, "LED now driven by %s Level (VOC %d, CO2 %d)",
                     co2_is_worse ? "CO2" : "VOC", current_voc_level, current_co2_level);
            was_co2_worse = co2_is_worse;
        }

        if (display_level >= AQI_MAX) {
            target_hue = 0;
        } else {
            target_hue = aqi_to_hue(display_level);
        }

        // Smoothly transition current_hue towards target_hue
        float hue_diff = (float)target_hue - current_hue;
        current_hue += hue_diff * TRANSITION_SPEED;

        // Convert current hue to color
        uint32_t color = get_color_from_hue((uint16_t)current_hue);

        // Update LED with the smoothly transitioning color
        led_set_color(color);
    }
}
