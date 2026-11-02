//
// Driver for the Sensirion SCD4x CO2 / temperature / humidity sensor.
// Implements periodic measurement mode with ASC.
// Compatible with SCD40, SCD41, and SCD43.
//

#ifndef AIRCUBE_SCD41_H
#define AIRCUBE_SCD41_H

#include <stdint.h>
#include <stdbool.h>

void scd41_init(void);
bool scd41_is_present(void);
bool scd41_poll(void);

uint16_t scd41_get_co2(void);
float    scd41_get_temperature_c(void);
float    scd41_get_humidity(void);
bool     scd41_has_data(void);

bool scd41_perform_frc(uint16_t target_ppm);

// Runtime calibration settings (persisted to NVS)
float    scd41_get_temp_trim(void);
void     scd41_set_temp_trim(float trim_c);
// Automatic adjustments are RAM-only; NVS writes are separately rate-limited.
void     scd41_set_temp_trim_runtime(float trim_c);
bool     scd41_save_temp_trim_if_changed(float min_delta_c);
uint32_t scd41_get_manual_trim_generation(void);
uint16_t scd41_get_pressure(void);
bool     scd41_set_pressure(uint16_t pressure_hpa);

#endif // AIRCUBE_SCD41_H
