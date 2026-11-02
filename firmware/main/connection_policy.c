#include "connection_policy.h"

#define BLUE_DELAY_US   (5LL * 60 * 1000000)
#define REPORT_DELAY_US (3LL * 1000000)

connection_led_result_t connection_led_update(connection_led_t *state,
                                              int64_t now_us, bool connected)
{
    connection_led_result_t result = {0};
    if (connected) {
        if (!state->connected) {
            result.blink_join = !state->ever_connected ||
                                now_us - state->lost_at_us >= BLUE_DELAY_US;
            if (state->ever_connected) {
                result.recovered_after_us = now_us - state->lost_at_us;
            }
        }
        state->connected = true;
        state->ever_connected = true;
    } else {
        if (state->connected) {
            state->lost_at_us = now_us;
        }
        state->connected = false;
        // Before the first join retain the established startup blue indication.
        result.show_blue = !state->ever_connected ||
                           now_us - state->lost_at_us >= BLUE_DELAY_US;
    }
    return result;
}

bool recovery_report_update(recovery_report_t *state, int64_t now_us,
                            bool connected, bool fresh_primary)
{
    if (!connected) {
        if (state->connected) {
            state->waiting = false;
            state->fresh_during_loss = false;
            state->fresh_for_recovery = false;
        }
        state->connected = false;
        if (state->ever_connected && fresh_primary) state->fresh_during_loss = true;
        return false;
    }

    if (!state->connected) {
        state->waiting = state->ever_connected;
        state->ready_at_us = now_us + REPORT_DELAY_US;
        state->fresh_for_recovery = state->fresh_during_loss;
        state->fresh_during_loss = false;
    }
    state->connected = true;
    state->ever_connected = true;
    if (state->waiting && fresh_primary) state->fresh_for_recovery = true;
    if (state->waiting && state->fresh_for_recovery && now_us >= state->ready_at_us) {
        state->waiting = false;
        state->fresh_for_recovery = false;
        return true;
    }
    return false;
}
