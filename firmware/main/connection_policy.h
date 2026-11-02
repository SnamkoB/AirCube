#ifndef AIRCUBE_CONNECTION_POLICY_H
#define AIRCUBE_CONNECTION_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool connected;
    bool ever_connected;
    int64_t lost_at_us;
} connection_led_t;

typedef struct {
    bool show_blue;
    bool blink_join;
    int64_t recovered_after_us;
} connection_led_result_t;

connection_led_result_t connection_led_update(connection_led_t *state,
                                              int64_t now_us, bool connected);

typedef struct {
    bool connected;
    bool ever_connected;
    bool waiting;
    bool fresh_during_loss;
    bool fresh_for_recovery;
    int64_t ready_at_us;
} recovery_report_t;

// Call every sensor-task iteration with freshness of the primary sensor.
// Returns true exactly once when a delayed, fresh recovery report is due.
bool recovery_report_update(recovery_report_t *state, int64_t now_us,
                            bool connected, bool fresh_primary);

static inline bool recovery_report_waiting(const recovery_report_t *state)
{
    return state->waiting;
}

#endif
