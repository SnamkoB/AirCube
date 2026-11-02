#include "auto_trim.h"

#include <math.h>

#define WARMUP_US         (10LL * 60 * 1000000)
#define ADJUST_PERIOD_US  (60LL * 1000000)
#define STALE_PERIOD_US   (2LL * 60 * 1000000)
#define TAU_US            (5.0f * 60.0f * 1000000.0f)
#define MAX_ABS_TRIM_C    5.0f

void auto_trim_init(auto_trim_t *state, int64_t now_us, float current_trim_c)
{
    state->target_ema_c = current_trim_c;
    state->warmup_start_us = now_us;
    state->last_sample_us = 0;
    state->last_adjust_us = now_us;
    state->initialized = false;
}

bool auto_trim_sample(auto_trim_t *state, int64_t now_us, float ens_temp_c,
                      float scd_corrected_temp_c, float current_trim_c,
                      float *new_trim_c)
{
    if (!isfinite(ens_temp_c) || !isfinite(scd_corrected_temp_c) ||
        !isfinite(current_trim_c) || now_us < state->warmup_start_us) {
        return false;
    }

    // A long sensor outage must not be integrated as a huge elapsed-time step.
    if (state->last_sample_us != 0 &&
        now_us - state->last_sample_us > STALE_PERIOD_US) {
        auto_trim_init(state, now_us, current_trim_c);
        return false;
    }

    if (now_us - state->warmup_start_us < WARMUP_US ||
        (state->last_sample_us != 0 && now_us <= state->last_sample_us)) {
        return false;
    }

    // The measured SCD temperature already includes the applied software trim.
    // Estimate the *absolute* trim needed relative to the untrimmed SCD sample,
    // not an error to add repeatedly (which would cause integral wind-up).
    float target_c = ens_temp_c - (scd_corrected_temp_c - current_trim_c);
    if (!isfinite(target_c) || fabsf(target_c) > MAX_ABS_TRIM_C) {
        return false;
    }

    if (!state->initialized) {
        // Begin from the current trim, so there is no step after warm-up.
        state->target_ema_c = current_trim_c;
        state->last_sample_us = now_us;
        state->last_adjust_us = now_us;
        state->initialized = true;
        return false;
    }

    float alpha = 1.0f - expf(-(float)(now_us - state->last_sample_us) / TAU_US);
    state->target_ema_c += alpha * (target_c - state->target_ema_c);
    state->last_sample_us = now_us;

    if (now_us - state->last_adjust_us < ADJUST_PERIOD_US) {
        return false;
    }
    state->last_adjust_us = now_us;
    *new_trim_c = state->target_ema_c;
    return fabsf(*new_trim_c - current_trim_c) >= 0.001f;
}
