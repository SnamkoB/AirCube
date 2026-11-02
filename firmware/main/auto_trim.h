#ifndef AIRCUBE_AUTO_TRIM_H
#define AIRCUBE_AUTO_TRIM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float target_ema_c;
    int64_t warmup_start_us;
    int64_t last_sample_us;
    int64_t last_adjust_us;
    bool initialized;
} auto_trim_t;

void auto_trim_init(auto_trim_t *state, int64_t now_us, float current_trim_c);

// Call only after both physical sensors have produced fresh, valid samples.
// Returns true only when a new runtime trim should be applied.
bool auto_trim_sample(auto_trim_t *state, int64_t now_us, float ens_temp_c,
                      float scd_corrected_temp_c, float current_trim_c,
                      float *new_trim_c);

#endif
