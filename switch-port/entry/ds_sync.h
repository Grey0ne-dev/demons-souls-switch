#ifndef DS_SYNC_H
#define DS_SYNC_H
#include <stdbool.h>
#include <stdint.h>
#define DS_CRT_MUTEX_CAPACITY 32
#ifdef __cplusplus
extern "C" {
#endif
/* Bounded native mutex pool for the single host thread executing this slice.
 * No blocking, recursion, guest scheduling or cross-thread ownership transfer.
 * Refused operations leave ownership unchanged; callers stop unsupported paths.
 */
bool ds_sync_create(uint32_t slot);
bool ds_sync_acquire(uint32_t slot, uint32_t owner);
bool ds_sync_release(uint32_t slot, uint32_t owner);
bool ds_sync_reset(void);
uint32_t ds_sync_live(void);
uint32_t ds_sync_held(void);
#ifdef __cplusplus
}
#endif
#endif
