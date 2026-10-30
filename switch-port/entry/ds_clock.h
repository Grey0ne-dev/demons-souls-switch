#ifndef DS_CLOCK_H
#define DS_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t ticks;
    uint64_t frequency;
    uint64_t microseconds;
} DsClockSample;

/* Exact truncation to signed-64-compatible microseconds, with overflow checks. */
bool ds_clock_ticks_to_us(uint64_t ticks, uint64_t frequency, uint64_t *microseconds);
/* Hardware system counter on Switch; CLOCK_MONOTONIC on Linux. No fake ticks,
 * wall clock, sleeps, per-call increments or per-checkpoint reset of the epoch.
 */
bool ds_clock_now(DsClockSample *sample);

#ifdef __cplusplus
}
#endif
#endif
