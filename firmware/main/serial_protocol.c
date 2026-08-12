//
// Serial Protocol Implementation
// JSON-based bidirectional communication over USB-Serial-JTAG
//

#include "serial_protocol.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "led.h"
#include "button.h"
#include "scd41.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "serial_protocol";

#define RX_BUF_SIZE 256
#define JSON_OUTPUT_BUF_SIZE 512

// External function to get readout period (defined in main.c)
extern uint32_t get_sensor_readout_period_ms(void);
extern void set_sensor_readout_period_ms(uint32_t period);

void serial_protocol_init(void)
{
    // ESP32-H2 uses USB-Serial-JTAG for console (not UART 0)
    // Install the USB-Serial-JTAG driver for RX capability
    usb_serial_jtag_driver_config_t usb_serial_config = {
        .rx_buffer_size = RX_BUF_SIZE * 2,
        .tx_buffer_size = RX_BUF_SIZE,
    };

    esp_err_t ret = usb_serial_jtag_driver_install(&usb_serial_config);
    if (ret == ESP_OK) {
        // Switch VFS to use the driver so RX data goes into the driver's buffer
        usb_serial_jtag_vfs_use_driver();
        ESP_LOGI(TAG, "USB-Serial-JTAG driver installed");
    } else {
        ESP_LOGW(TAG, "USB-Serial-JTAG driver install failed: %s (RX commands may not work)",
                 esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Serial protocol initialized (USB-Serial-JTAG)");
}

void serial_send_sensor_data(uint8_t ens210_status, float temperature_c, float humidity,
                             const char* model, int co2_ppm)
{
    char json_buffer[JSON_OUTPUT_BUF_SIZE];
    
    // Get timestamp (milliseconds since boot)
    uint32_t timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Convert Celsius to Fahrenheit
    float temperature_f = temperature_c * 9.0f / 5.0f + 32.0f;
    
    // Format JSON output. Note: "aqi" is the canonical AirCube VOC Level
    // (TVOC-derived, 0-500). "aqi_s" is deprecated and always 0.
    // Temperature/humidity come from the SCD41 (the "ens210" keys are kept
    // for backward compatibility), "scd41" carries the true CO2.
    int len = snprintf(json_buffer, sizeof(json_buffer),
        "{\"model\":\"%s\","
        "\"ens210\":{\"status\":%u,\"temperature_c\":%.2f,\"temperature_f\":%.2f,\"humidity\":%.2f},"
        "\"scd4x\":{\"co2\":%d,\"connected\":%s},"
        "\"calibration\":{\"temp_trim_c\":%.3f,\"pressure_hpa\":%u},"
        "\"timestamp\":%lu}\n",
        model, ens210_status, temperature_c, temperature_f, humidity,
        co2_ppm, co2_ppm > 0 ? "true" : "false",
        scd41_get_temp_trim(), scd41_get_pressure(),
        (unsigned long)timestamp);
    
    if (len > 0 && len < sizeof(json_buffer)) {
        // Write to console UART using printf (goes to UART 0)
        printf("%s", json_buffer);
        fflush(stdout); // Ensure immediate output
    } else {
        ESP_LOGW(TAG, "JSON buffer too small or formatting error");
    }
}

static void send_response(const char* status, const char* cmd, float value)
{
    char response[128];
    int len = snprintf(response, sizeof(response),
        "{\"status\":\"%s\",\"cmd\":\"%s\",\"value\":%.2f}\n",
        status, cmd ? cmd : "", value);
    
    if (len > 0 && len < sizeof(response)) {
        printf("%s", response);
        fflush(stdout);
    }
}

static void send_error(const char* msg)
{
    char response[128];
    int len = snprintf(response, sizeof(response),
        "{\"status\":\"error\",\"msg\":\"%s\"}\n", msg);
    
    if (len > 0 && len < sizeof(response)) {
        printf("%s", response);
        fflush(stdout);
    }
}

static void send_config_response(void)
{
    char response[128];
    int len = snprintf(response, sizeof(response),
        "{\"config\":{\"intensity\":%.2f,\"readout_period\":%lu}}\n",
        (float)button_get_brightness_percent() / 100.0f,
        (unsigned long)get_sensor_readout_period_ms());

    if (len > 0 && len < (int)sizeof(response)) {
        printf("%s", response);
        fflush(stdout);
    }
}

static bool parse_command(const char* buffer, size_t len)
{
    // Strip trailing \r if present (monitor sends \r\n)
    size_t slen = len;
    while (slen > 0 && (buffer[slen - 1] == '\r' || buffer[slen - 1] == '\n' || buffer[slen - 1] == ' ')) {
        slen--;
    }

    // Simple JSON parsing for commands
    // Expected format: {"cmd":"command_name","value":number}
    // or {"cmd":"get_config"}
    
    if (len < 10) return false; // Minimum valid command size
    
    // Check if it starts with {"cmd":
    if (strncmp(buffer, "{\"cmd\":", 7) != 0) {
        return false;
    }
    
    // Find command name
    const char* cmd_start = strstr(buffer, "\"cmd\":\"");
    if (!cmd_start) return false;
    cmd_start += 7; // Skip "cmd":"
    
    const char* cmd_end = strchr(cmd_start, '"');
    if (!cmd_end) return false;
    
    size_t cmd_len = cmd_end - cmd_start;
    char cmd_name[32];
    if (cmd_len >= sizeof(cmd_name)) return false;
    strncpy(cmd_name, cmd_start, cmd_len);
    cmd_name[cmd_len] = '\0';
    
    // Handle get_config command (no value field needed)
    if (strcmp(cmd_name, "get_config") == 0) {
        send_config_response();
        return true;
    }
    
    // For set commands, find value field
    const char* value_start = strstr(buffer, "\"value\":");
    if (!value_start) {
        send_error("missing value field");
        return false;
    }
    value_start += 8; // Skip "value":
    
    // Parse float value
    float value = strtof(value_start, NULL);
    
    // Handle set_intensity command
    if (strcmp(cmd_name, "set_intensity") == 0) {
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;

        int percent = (int)(value * 100.0f + 0.5f);
        button_set_brightness_percent(percent);
        send_response("ok", "set_intensity", value);
        ESP_LOGI(TAG, "LED intensity set to %.2f (%d%%)", value, percent);
        return true;
    }
    
    // Handle set_readout_period command
    if (strcmp(cmd_name, "set_readout_period") == 0) {
        // Clamp value to valid range (100ms to 10000ms)
        uint32_t period = (uint32_t)value;
        if (period < 100) period = 100;
        if (period > 10000) period = 10000;
        
        set_sensor_readout_period_ms(period);
        send_response("ok", "set_readout_period", (float)period);
        ESP_LOGI(TAG, "Sensor readout period set to %lu ms", (unsigned long)period);
        return true;
    }
    
    // Handle scd4x_frc command (forced recalibration, target ppm in value)
    if (strcmp(cmd_name, "scd4x_frc") == 0) {
        uint16_t target = (uint16_t)value;
        if (target < 400 || target > 2000) {
            send_error("FRC target out of range (400-2000 ppm)");
            return false;
        }
        bool ok = scd41_perform_frc(target);
        if (ok) {
            send_response("ok", "scd4x_frc", (float)target);
        } else {
            send_error("FRC failed (sensor present and running >3 min?)");
        }
        return true;
    }

    // Handle set_temp_trim command
    if (strcmp(cmd_name, "set_temp_trim") == 0) {
        scd41_set_temp_trim(value);
        send_response("ok", "set_temp_trim", value);
        ESP_LOGI(TAG, "Temperature trim set to %.3f C", value);
        return true;
    }

    // Handle set_pressure command
    if (strcmp(cmd_name, "set_pressure") == 0) {
        uint16_t pres = (uint16_t)value;
        if (scd41_set_pressure(pres)) {
            send_response("ok", "set_pressure", (float)pres);
            ESP_LOGI(TAG, "Pressure set to %u hPa", pres);
        } else {
            send_error("Pressure out of range (700-1100 hPa)");
        }
        return true;
    }

    // Unknown command
    send_error("unknown command");
    return false;
}

void serial_process_commands(void)
{
    static uint8_t rx_buffer[RX_BUF_SIZE];
    static size_t buffer_pos = 0;
    
    // Read available data from USB-Serial-JTAG (non-blocking, 0 tick timeout)
    int len = usb_serial_jtag_read_bytes(rx_buffer + buffer_pos,
                                          RX_BUF_SIZE - buffer_pos - 1, 0);
    
    if (len > 0) {
        buffer_pos += len;
        rx_buffer[buffer_pos] = '\0'; // Null terminate
        
        // Look for complete JSON commands (ending with \n or })
        char* newline = strchr((char*)rx_buffer, '\n');
        char* brace_end = strrchr((char*)rx_buffer, '}');
        
        if (newline || (brace_end && buffer_pos > 0)) {
            // Process command
            size_t cmd_len = newline ? (newline - (char*)rx_buffer) : 
                            (brace_end ? (brace_end - (char*)rx_buffer + 1) : buffer_pos);
            
            if (cmd_len > 0 && cmd_len < RX_BUF_SIZE) {
                rx_buffer[cmd_len] = '\0';
                parse_command((char*)rx_buffer, cmd_len);
            }
            
            // Shift remaining data to start of buffer
            size_t remaining = buffer_pos - cmd_len - (newline ? 1 : 0);
            if (remaining > 0 && remaining < RX_BUF_SIZE) {
                memmove(rx_buffer, rx_buffer + cmd_len + (newline ? 1 : 0), remaining);
                buffer_pos = remaining;
            } else {
                buffer_pos = 0;
            }
        }
        
        // Prevent buffer overflow
        if (buffer_pos >= RX_BUF_SIZE - 1) {
            ESP_LOGW(TAG, "Command buffer overflow, resetting");
            buffer_pos = 0;
        }
    }
}
