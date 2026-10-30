#include "ds_atomic.h"

#include <string.h>

/* Interpret big-endian bytes in the host's native word representation. This
 * handles either host byte order without aliasing a byte buffer as uint32_t.
 */
static uint32_t raw_be_word(uint32_t value)
{
    const unsigned char bytes[4] = {
        (unsigned char)(value >> 24), (unsigned char)(value >> 16),
        (unsigned char)(value >> 8), (unsigned char)value
    };
    uint32_t raw;
    memcpy(&raw, bytes, sizeof(raw));
    return raw;
}

bool ds_image_compare_exchange_be32(DsImage *image, uint64_t address,
                                   uint32_t expected, uint32_t desired,
                                   bool *exchanged)
{
    if (!image || !exchanged || address > UINT32_MAX || (address & 3u)) return false;
    void *host = ds_image_address(image, (uint32_t)address, sizeof(uint32_t), DS_PF_R | DS_PF_W);
    if (!host || (uintptr_t)host % _Alignof(uint32_t)) return false;

    uint32_t raw_expected = raw_be_word(expected);
    const uint32_t raw_desired = raw_be_word(desired);
    const bool stored = __atomic_compare_exchange_n((uint32_t *)host, &raw_expected, raw_desired,
                                                   false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    *exchanged = stored;
    return true;
}
