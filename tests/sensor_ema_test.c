#include "sensor_ema.h"

#include <assert.h>
#include <math.h>

static void near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.002f);
}

int main(void)
{
    sensor_ema_t scd4x = {0};
    sensor_ema_t ens210 = {0};

    sensor_ema_update(&scd4x, 1000000, 20.0f, 40.0f, 800.0f);
    assert(scd4x.initialized);
    near(scd4x.temperature_c, 20.0f); // first valid sample, not startup zero
    near(scd4x.co2_ppm, 800.0f);

    sensor_ema_update(&scd4x, 6000000, 30.0f, 50.0f, 1000.0f);
    float alpha5 = 1.0f - expf(-5.0f / 30.0f);
    near(scd4x.temperature_c, 20.0f + 10.0f * alpha5);
    near(scd4x.humidity, 40.0f + 10.0f * alpha5);
    near(scd4x.co2_ppm, 800.0f + 200.0f * alpha5);

    sensor_ema_update(&scd4x, 6000000, 100.0f, 100.0f, 2000.0f);
    near(scd4x.co2_ppm, 800.0f + 200.0f * alpha5); // no duplicate sample
    sensor_ema_update(&scd4x, 16000000, 30.0f, 50.0f, 1000.0f);
    float alpha10 = 1.0f - expf(-10.0f / 30.0f);
    near(scd4x.co2_ppm, 800.0f + 200.0f * alpha5
                         + alpha10 * (1000.0f - (800.0f + 200.0f * alpha5)));

    sensor_ema_update(&scd4x, 17000000, NAN, 30.0f, 900.0f);
    assert(scd4x.last_sample_us == 16000000); // invalid sample is ignored

    sensor_ema_update(&ens210, 7000000, 25.0f, 55.0f, 0.0f);
    near(ens210.temperature_c, 25.0f); // independent fallback/comparison state
    near(scd4x.temperature_c, 20.0f + 10.0f * alpha5
                               + alpha10 * (30.0f - (20.0f + 10.0f * alpha5)));
    return 0;
}
