#include "connection_policy.h"

#include <assert.h>

#define S 1000000LL

int main(void)
{
    connection_led_t led = {0};
    connection_led_result_t r = connection_led_update(&led, 0, false);
    assert(r.show_blue);
    r = connection_led_update(&led, 5*S, true);
    assert(r.blink_join);
    r = connection_led_update(&led, 10*S, false);
    assert(!r.show_blue);
    r = connection_led_update(&led, 20*S, true);
    assert(!r.blink_join && r.recovered_after_us == 10*S);
    connection_led_update(&led, 25*S, false);
    r = connection_led_update(&led, 324*S, false);
    assert(!r.show_blue);
    r = connection_led_update(&led, 325*S, false);
    assert(r.show_blue);
    r = connection_led_update(&led, 326*S, true);
    assert(r.blink_join);

    recovery_report_t report = {0};
    assert(!recovery_report_update(&report, 0, false, false));
    assert(!recovery_report_update(&report, 5*S, true, true));
    assert(!recovery_report_waiting(&report)); // ordinary first join
    assert(!recovery_report_update(&report, 10*S, false, true));
    assert(!recovery_report_update(&report, 12*S, true, false));
    assert(recovery_report_waiting(&report));
    assert(!recovery_report_update(&report, 14*S, true, false));
    assert(recovery_report_update(&report, 15*S, true, false));
    assert(!recovery_report_update(&report, 16*S, true, false)); // exactly once
    assert(!recovery_report_update(&report, 20*S, false, false));
    assert(!recovery_report_update(&report, 21*S, true, false));
    assert(!recovery_report_update(&report, 24*S, true, false)); // no stale replay
    assert(recovery_report_update(&report, 25*S, true, true));
    assert(!recovery_report_update(&report, 30*S, false, true));
    assert(!recovery_report_update(&report, 31*S, true, false));
    assert(!recovery_report_update(&report, 32*S, false, false)); // interrupted delay
    assert(!recovery_report_update(&report, 33*S, true, false));
    assert(!recovery_report_update(&report, 36*S, true, false));
    assert(recovery_report_update(&report, 37*S, true, true));
    return 0;
}
