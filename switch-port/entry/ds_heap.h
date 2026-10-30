#ifndef DS_HEAP_H
#define DS_HEAP_H
#include "ds_image.h"
#ifdef __cplusplus
extern "C" {
#endif

#define DS_HEAP_BASE UINT32_C(0x60000000)
#define DS_HEAP_WINDOW UINT32_C(0x04000000)
#define DS_HEAP_DEFAULT_LIMIT UINT32_C(0x800000)
#define DS_HEAP_MAX_ARENAS 8u
#define DS_HEAP_MAX_BLOCKS 1024u
#define DS_HEAP_PAGE 4096u

typedef enum { DS_HEAP_OK, DS_HEAP_BAD_REQUEST, DS_HEAP_OUT_OF_MEMORY,
               DS_HEAP_INVALID_FREE, DS_HEAP_CORRUPT } DsHeapStatus;
typedef struct { uint32_t address,size; unsigned char *backing; } DsHeapArena;
typedef struct { uint32_t address,size,requested,alignment,arena; bool used; } DsHeapBlock;
typedef struct {
    uint32_t allocation_calls,free_calls,failed_allocations,live_allocations;
    size_t live_bytes,requested_bytes,peak_live_bytes,backed_bytes;
    uint32_t arena_count;
} DsHeapStats;
typedef struct {
    DsImage *image;
    size_t limit;
    uint32_t block_count;
    DsHeapStats stats;
    DsHeapArena arenas[DS_HEAP_MAX_ARENAS];
    DsHeapBlock blocks[DS_HEAP_MAX_BLOCKS];
} DsHeap;

/* Single guest thread. Metadata is host-side; returned pointers are guest offsets.
 * A zero-byte malloc owns a minimum16-byte block. Alignment is a power of two,
 * 16..65536. Backing remains charged after free until destroy/image destruction. */
bool ds_heap_init(DsHeap *heap,DsImage *image,size_t limit);
bool ds_heap_validate(const DsHeap *heap);
DsHeapStatus ds_heap_alloc(DsHeap *heap,uint64_t size,uint32_t alignment,uint32_t *guest);
DsHeapStatus ds_heap_free(DsHeap *heap,uint64_t guest);
/* Destroy releases owned arenas and budget. Detach drops metadata only; backing
 * then belongs solely to the image and is released by ds_image_destroy(). */
bool ds_heap_destroy(DsHeap *heap);
void ds_heap_detach(DsHeap *heap);
#ifdef __cplusplus
}
#endif
#endif
