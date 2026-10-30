#include "ds_kernel_mutex.h"
#ifdef __SWITCH__
extern "C" {
#include <switch/kernel/mutex.h>
}
#else
#include <pthread.h>
#endif
namespace {
struct Slot {
    bool active;
#ifdef __SWITCH__
    RMutex native;
#else
    pthread_mutex_t native;
#endif
};
Slot slots[8];
}
extern "C" bool ds_kernel_mutex_create(uint32_t *handle)
{
    if (!handle) return false;
    for (uint32_t i = 0; i < 8; ++i) {
        if (slots[i].active) continue;
#ifdef __SWITCH__
        rmutexInit(&slots[i].native);
#else
        pthread_mutexattr_t attr;
        if (pthread_mutexattr_init(&attr) != 0) return false;
        const int type_rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        const int init_rc = type_rc ? type_rc : pthread_mutex_init(&slots[i].native, &attr);
        const int attr_rc = pthread_mutexattr_destroy(&attr);
        if (init_rc != 0) return false;
        if (attr_rc != 0) {
            if (pthread_mutex_destroy(&slots[i].native) != 0) slots[i].active = true;
            return false;
        }
#endif
        slots[i].active = true;
        *handle = 0x1001 + i;
        return true;
    }
    return false;
}
extern "C" bool ds_kernel_mutex_contains(uint32_t handle)
{
    return handle >= 0x1001 && handle <= 0x1008 && slots[handle - 0x1001].active;
}
extern "C" uint32_t ds_kernel_mutex_live(void)
{
    uint32_t count = 0;
    for (const Slot &slot : slots) count += slot.active;
    return count;
}
extern "C" bool ds_kernel_mutex_reset(void)
{
    bool ok = true;
    for (Slot &slot : slots) {
        if (!slot.active) continue;
#ifndef __SWITCH__
        if (pthread_mutex_destroy(&slot.native) != 0) { ok = false; continue; }
#endif
        slot.active = false;
    }
    return ok;
}
