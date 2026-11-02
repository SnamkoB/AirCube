#include "auto_trim.h"

#include <assert.h>
#include <math.h>

#define S 1000000LL

int main(void)
{
    auto_trim_t state;
    float next = 0.0f, current = 0.0f;
    auto_trim_init(&state, 0, current);
    for (int t = 5; t < 600; t += 5) {
        assert(!auto_trim_sample(&state, t * S, 21.0f, 24.0f, current, &next));
    }
    assert(!auto_trim_sample(&state, 600*S, 21.0f, 24.0f, current, &next));
    assert(state.initialized);
    for (int t = 605; t < 660; t += 5) {
        assert(!auto_trim_sample(&state, t*S, 21.0f, 24.0f + current, current, &next));
    }
    assert(auto_trim_sample(&state, 660*S, 21.0f, 24.0f + current, current, &next));
    assert(next < -0.5f && next > -0.7f); // 60 s / 300 s time constant
    current = next;
    assert(!auto_trim_sample(&state, 660*S, 21.0f, 24.0f + current, current, &next));
    for (int t = 665; t < 720; t += 5) {
        assert(!auto_trim_sample(&state, t*S, 21.0f, 24.0f + current, current, &next));
    }
    assert(auto_trim_sample(&state, 720*S, 21.0f, 24.0f + current, current, &next));
    assert(next < current); // converges to -3, never repeatedly adds the same error
    current = next;
    assert(!auto_trim_sample(&state, 725*S, NAN, 24.0f, current, &next));
    assert(!auto_trim_sample(&state, 730*S, 40.0f, 24.0f, current, &next));
    assert(!auto_trim_sample(&state, 850*S, 21.0f, 24.0f + current, current, &next));
    assert(!state.initialized); // outage restarts warm-up instead of a giant EMA step
    auto_trim_init(&state, 850*S, 1.0f); // manual trim restarts warm-up
    assert(!auto_trim_sample(&state, 900*S, 21.0f, 24.0f, 1.0f, &next));
    return 0;
}
