#ifndef DS_IMAGE_H
#define DS_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS_MAX_SEGMENTS 16
#define DS_PF_X 1u
#define DS_PF_W 2u
#define DS_PF_R 4u

typedef struct {
    uint32_t guest_address;
    uint32_t flags;
    uint64_t file_offset;
    size_t file_size;
    size_t memory_size;
    unsigned char *data;
    bool from_elf;
} DsSegment;

typedef struct {
    DsSegment segments[DS_MAX_SEGMENTS];
    size_t segment_count;
    size_t allocated_bytes;
    size_t memory_limit;
    uint32_t entry_descriptor;
    uint32_t entry_code;
    uint32_t toc;
    uint32_t tls_address;
    size_t tls_file_size;
    size_t tls_memory_size;
    bool constructor_execution_attempted; /* Restart this stage only after image reload. */
} DsImage;

/* Initialize an empty output; all failure paths free partial allocations. */
bool ds_image_load_file(DsImage *image, const char *path, size_t memory_limit,
                        char *error, size_t error_capacity);
void ds_image_destroy(DsImage *image);

/* Separate backed arenas; no fixed-address host mappings or pointer truncation. */
void *ds_image_address(const DsImage *image, uint32_t guest_address,
                       size_t size, uint32_t required_flags);
bool ds_image_add_zero_region(DsImage *image, uint32_t guest_address, size_t size,
                              uint32_t flags, char *error, size_t error_capacity);
/* Release only an exact anonymous region whose original backing the caller owns.
 * ELF regions and mismatched pointers/extents are never removed. */
bool ds_image_remove_zero_region(DsImage *image, uint32_t guest_address, size_t size,
                                 const void *expected_backing, char *error, size_t error_capacity);
bool ds_image_read_be32(const DsImage *image, uint32_t address, uint32_t *value);
bool ds_image_read_be64(const DsImage *image, uint32_t address, uint64_t *value);
bool ds_image_write_be32(DsImage *image, uint32_t address, uint32_t value);

/* Conservative 1 MiB-granular guest allowance, charged for ALL image arenas.
 * This is the configured cap, not a claim about console RAM or guaranteed
 * future host allocation success. Invalid accounting is rejected.
 */
bool ds_image_memory_info(const DsImage *image, uint32_t *total, uint32_t *available);

#ifdef __cplusplus
}
#endif
#endif
