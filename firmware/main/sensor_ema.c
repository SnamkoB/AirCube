#include "sensor_ema.h"

#include <math.h>

#define SENSOR_EMA_TAU_US 30000000.0f  // 30-second time constant

void sensor_ema_update(sensor_ema_t *filter, int64_t sample_us,
                       float temperature_c, float humidity, float co2_ppm)
{
    if (!isfinite(temperature_c) || !isfinite(humidity) || !isfinite(co2_ppm)) {
        return;
    }

    // Do not blend a real first measurement with the sensors' startup zeros.
    if (!filter->initialized) {
        filter->temperature_c = temperature_c;
        filter->humidity = humidity;
        filter->co2_ppm = co2_ppm;
        filter->last_sample_us = sample_us;
        filter->initialized = true;
        return;
    }

    if (sample_us <= filter->last_sample_us) {
        return;
    }

    // Actual sample spacing matters: the sensor cadence can drift or be changed.
    float alpha = 1.0f - expf(-(float)(sample_us - filter->last_sample_us)
                              / SENSOR_EMA_TAU_US);
    filter->temperature_c += alpha * (temperature_c - filter->temperature_c);
    filter->humidity += alpha * (humidity - filter->humidity);
    filter->co2_ppm += alpha * (co2_ppm - filter->co2_ppm);
    filter->last_sample_us = sample_us;
}
