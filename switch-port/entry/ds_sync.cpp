#include "ds_sync.h"
#ifdef __SWITCH__
extern "C" {
#include <switch/kernel/mutex.h>
}
#else
#include <pthread.h>
#endif

namespace {
struct Slot {
    bool initialized;
    uint32_t owner;
#ifdef __SWITCH__
    Mutex native;
#else
    pthread_mutex_t native;
#endif
};
Slot slots[DS_CRT_MUTEX_CAPACITY];
}

extern "C" bool ds_sync_create(uint32_t index)
{
    if (index >= DS_CRT_MUTEX_CAPACITY || slots[index].initialized) return false;
    Slot &slot = slots[index];
#ifdef __SWITCH__
    slot.native = 0;
#else
    if (pthread_mutex_init(&slot.native, nullptr) != 0) return false;
#endif
    slot.owner = 0;
    slot.initialized = true;
    return true;
}

extern "C" bool ds_sync_acquire(uint32_t index, uint32_t owner)
{
    if (index >= DS_CRT_MUTEX_CAPACITY || !owner || !slots[index].initialized || slots[index].owner) return false;
    Slot &slot = slots[index];
#ifdef __SWITCH__
    if (!mutexTryLock(&slot.native)) return false;
#else
    if (pthread_mutex_trylock(&slot.native) != 0) return false;
#endif
    slot.owner = owner;
    return true;
}

extern "C" bool ds_sync_release(uint32_t index, uint32_t owner)
{
    if (index >= DS_CRT_MUTEX_CAPACITY || !owner || !slots[index].initialized || slots[index].owner != owner) return false;
    Slot &slot = slots[index];
#ifdef __SWITCH__
    mutexUnlock(&slot.native);
#else
    if (pthread_mutex_unlock(&slot.native) != 0) return false;
#endif
    slot.owner = 0;
    return true;
}

extern "C" uint32_t ds_sync_live(void)
{
    uint32_t count = 0;
    for (const Slot &slot : slots) count += slot.initialized;
    return count;
}

extern "C" uint32_t ds_sync_held(void)
{
    uint32_t count = 0;
    for (const Slot &slot : slots) count += slot.owner != 0;
    return count;
}

extern "C" bool ds_sync_reset(void)
{
    bool ok = true;
    for (uint32_t i = 0; i < DS_CRT_MUTEX_CAPACITY; ++i) {
        Slot &slot = slots[i];
        if (!slot.initialized) continue;
        if (slot.owner && !ds_sync_release(i, slot.owner)) { ok = false; continue; }
#ifndef __SWITCH__
        if (pthread_mutex_destroy(&slot.native) != 0) { ok = false; continue; }
#endif
        slot.initialized = false;
    }
    return ok;
}
