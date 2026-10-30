#ifndef DS_KERNEL_MUTEX_H
#define DS_KERNEL_MUTEX_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Creation-only private recursive mutex registry, one active host thread. */
bool ds_kernel_mutex_create(uint32_t *handle);
bool ds_kernel_mutex_contains(uint32_t handle);
uint32_t ds_kernel_mutex_live(void);
bool ds_kernel_mutex_reset(void);
#ifdef __cplusplus
}
#endif
#endif
