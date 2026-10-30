#ifndef DS_ATOMIC_H
#define DS_ATOMIC_H

#include "ds_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compare/exchange one aligned big-endian guest word using a real native
 * sequentially consistent CAS. Invalid ranges, permissions, alignment or
 * arguments return false without changing memory or *exchanged. A valid call
 * returns true and reports whether the expected value matched in *exchanged.
 * Guest reservation ownership/invalidation belongs to the calling PPU adapter.
 */
bool ds_image_compare_exchange_be32(DsImage *image, uint64_t address,
                                   uint32_t expected, uint32_t desired,
                                   bool *exchanged);

#ifdef __cplusplus
}
#endif
#endif
