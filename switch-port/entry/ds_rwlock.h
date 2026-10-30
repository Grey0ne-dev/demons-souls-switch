#ifndef DS_RWLOCK_H
#define DS_RWLOCK_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Small guest-handle registry for the single active host/guest thread.
 * Uncontended, nonrecursive write ownership is supported. Read acquisition,
 * blocking waits, IPC and waiter scheduling remain unsupported.
 */
bool ds_rwlock_create(uint32_t *handle);
bool ds_rwlock_contains(uint32_t handle);
uint32_t ds_rwlock_live(void);
bool ds_rwlock_write_acquire(uint32_t handle, uint32_t owner);
bool ds_rwlock_write_release(uint32_t handle, uint32_t owner);
uint32_t ds_rwlock_held(void);
bool ds_rwlock_reset(void);
#ifdef __cplusplus
}
#endif
#endif
