#ifndef AIRCUBE_SENSOR_EMA_H
#define AIRCUBE_SENSOR_EMA_H

#include <stdbool.h>
#include <stdint.h>

// One state per physical sensor. Call only after a fresh, valid measurement.
typedef struct {
    bool initialized;
    int64_t last_sample_us;
    float temperature_c;
    float humidity;
    float co2_ppm;
} sensor_ema_t;

void sensor_ema_update(sensor_ema_t *filter, int64_t sample_us,
                       float temperature_c, float humidity, float co2_ppm);

#endif
