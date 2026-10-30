#include "ds_clock.h"

#ifdef __SWITCH__
#include <switch/arm/counter.h>
#else
#include <time.h>
#endif

extern "C" bool ds_clock_ticks_to_us(uint64_t ticks, uint64_t frequency, uint64_t *microseconds)
{
    if (!frequency || !microseconds) return false;
    const __uint128_t scaled = static_cast<__uint128_t>(ticks) * 1000000u / frequency;
    if (scaled > INT64_MAX) return false;
    *microseconds = static_cast<uint64_t>(scaled);
    return true;
}

extern "C" bool ds_clock_now(DsClockSample *sample)
{
    if (!sample) return false;
    DsClockSample next = {};
#ifdef __SWITCH__
    next.frequency = armGetSystemTickFreq();
    next.ticks = armGetSystemTick();
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
        now.tv_nsec < 0 || now.tv_nsec >= 1000000000L) return false;
    const __uint128_t ticks = static_cast<__uint128_t>(now.tv_sec) * 1000000000u + now.tv_nsec;
    if (ticks > UINT64_MAX) return false;
    next.frequency = 1000000000u;
    next.ticks = static_cast<uint64_t>(ticks);
#endif
    if (!ds_clock_ticks_to_us(next.ticks, next.frequency, &next.microseconds)) return false;
    *sample = next;
    return true;
}
