#include "ds_rwlock.h"
#ifdef __SWITCH__
extern "C" {
#include <switch/kernel/rwlock.h>
}
#else
#include <pthread.h>
#endif
namespace {
struct Slot {
    bool active;
    uint32_t write_owner;
#ifdef __SWITCH__
    RwLock native;
#else
    pthread_rwlock_t native;
#endif
};
Slot slots[8];
}
extern "C" bool ds_rwlock_create(uint32_t *handle)
{
    if (!handle) return false;
    for (uint32_t i = 0; i < 8; ++i) {
        if (slots[i].active) continue;
#ifdef __SWITCH__
        rwlockInit(&slots[i].native);
#else
        if (pthread_rwlock_init(&slots[i].native, nullptr) != 0) return false;
#endif
        slots[i].write_owner = 0;
        slots[i].active = true;
        *handle = i + 1;
        return true;
    }
    return false;
}
extern "C" bool ds_rwlock_contains(uint32_t handle)
{
    return handle >= 1 && handle <= 8 && slots[handle - 1].active;
}
extern "C" uint32_t ds_rwlock_live(void)
{
    uint32_t count = 0;
    for (const Slot &slot : slots) count += slot.active;
    return count;
}

extern "C" bool ds_rwlock_write_acquire(uint32_t handle, uint32_t owner)
{
    if (!owner || !ds_rwlock_contains(handle)) return false;
    Slot &slot = slots[handle - 1];
    if (slot.write_owner) return false;
#ifdef __SWITCH__
    if (!rwlockTryWriteLock(&slot.native)) return false;
#else
    if (pthread_rwlock_trywrlock(&slot.native) != 0) return false;
#endif
    slot.write_owner = owner;
    return true;
}

extern "C" bool ds_rwlock_write_release(uint32_t handle, uint32_t owner)
{
    if (!owner || !ds_rwlock_contains(handle)) return false;
    Slot &slot = slots[handle - 1];
    if (slot.write_owner != owner) return false;
#ifdef __SWITCH__
    rwlockWriteUnlock(&slot.native);
#else
    if (pthread_rwlock_unlock(&slot.native) != 0) return false;
#endif
    slot.write_owner = 0;
    return true;
}

extern "C" uint32_t ds_rwlock_held(void)
{
    uint32_t count = 0;
    for (const Slot &slot : slots) count += slot.active && slot.write_owner != 0;
    return count;
}

extern "C" bool ds_rwlock_reset(void)
{
    bool ok = true;
    for (uint32_t i = 0; i < 8; ++i) {
        Slot &slot = slots[i];
        if (!slot.active) continue;
        if (slot.write_owner && !ds_rwlock_write_release(i + 1, slot.write_owner)) {
            ok = false;
            continue;
        }
#ifndef __SWITCH__
        if (pthread_rwlock_destroy(&slot.native) != 0) { ok = false; continue; }
#endif
        slot.active = false;
    }
    return ok;
}
