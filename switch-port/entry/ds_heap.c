#include "ds_heap.h"
#include <string.h>

static bool owns_arena(const DsHeap *heap,const DsHeapArena *arena)
{
    for (size_t i=0;i<heap->image->segment_count;++i) {
        const DsSegment *s=&heap->image->segments[i];
        if (!s->from_elf && s->file_offset==UINT64_MAX && !s->file_size &&
            s->guest_address==arena->address && s->memory_size==arena->size &&
            s->data==arena->backing && (s->flags&(DS_PF_R|DS_PF_W))==(DS_PF_R|DS_PF_W)) return true;
    }
    return false;
}

bool ds_heap_init(DsHeap *heap,DsImage *image,size_t limit)
{
    if (!heap || heap->image || !image || !limit || limit>DS_HEAP_DEFAULT_LIMIT ||
        image->segment_count>DS_MAX_SEGMENTS || image->allocated_bytes>image->memory_limit) return false;
    memset(heap,0,sizeof(*heap));
    heap->image=image;
    heap->limit=limit;
    return true;
}

bool ds_heap_validate(const DsHeap *heap)
{
    if (!heap || !heap->image || !heap->limit || heap->limit>DS_HEAP_DEFAULT_LIMIT ||
        heap->image->segment_count>DS_MAX_SEGMENTS || heap->image->allocated_bytes>heap->image->memory_limit ||
        heap->stats.arena_count>DS_HEAP_MAX_ARENAS || heap->block_count>DS_HEAP_MAX_BLOCKS) return false;
    size_t backed=0,live=0,requested=0;
    uint32_t blocks=0,used=0;
    uint64_t previous_end=DS_HEAP_BASE;
    for (uint32_t a=0;a<heap->stats.arena_count;++a) {
        const DsHeapArena *arena=&heap->arenas[a];
        if (!arena->size || (arena->address|arena->size)&(DS_HEAP_PAGE-1) ||
            arena->address<previous_end || (uint64_t)arena->address+arena->size>(uint64_t)DS_HEAP_BASE+DS_HEAP_WINDOW ||
            !owns_arena(heap,arena)) return false;
        uint64_t cursor=arena->address;
        bool previous_free=false;
        while (blocks<heap->block_count && heap->blocks[blocks].arena==a) {
            const DsHeapBlock *b=&heap->blocks[blocks++];
            if (!b->size || (b->size&15u) || b->address!=cursor ||
                cursor+b->size>(uint64_t)arena->address+arena->size) return false;
            if (b->used) {
                if (b->alignment<16 || b->alignment>65536 || (b->alignment&(b->alignment-1)) ||
                    (b->address&(b->alignment-1)) || b->requested>b->size) return false;
                ++used;live+=b->size;requested+=b->requested;
            } else if (b->requested || b->alignment || previous_free) return false;
            previous_free=!b->used;
            cursor+=b->size;
        }
        if (cursor!=(uint64_t)arena->address+arena->size) return false;
        previous_end=cursor;
        backed+=arena->size;
    }
    return blocks==heap->block_count && backed==heap->stats.backed_bytes && backed<=heap->limit &&
        backed<=heap->image->allocated_bytes && live==heap->stats.live_bytes &&
        requested==heap->stats.requested_bytes && used==heap->stats.live_allocations &&
        live<=heap->stats.peak_live_bytes && heap->stats.peak_live_bytes<=backed;
}

static uint64_t align_up(uint64_t value,uint32_t alignment)
{
    return (value+alignment-1)&~((uint64_t)alignment-1);
}

static bool grow(DsHeap *heap,uint32_t need,uint32_t alignment)
{
    if (heap->stats.arena_count==DS_HEAP_MAX_ARENAS || heap->block_count==DS_HEAP_MAX_BLOCKS) return false;
    uint32_t address=DS_HEAP_BASE;
    uint64_t target=65536;
    if (heap->stats.arena_count) {
        const DsHeapArena *last=&heap->arenas[heap->stats.arena_count-1];
        const uint64_t next=(uint64_t)last->address+last->size+65536;
        if (next>UINT32_MAX) return false;
        address=(uint32_t)next;
        target=last->size<0x100000 ? (uint64_t)last->size*2 : 0x100000;
    }
    const uint64_t required=align_up(align_up(address,alignment)-address+need,DS_HEAP_PAGE);
    if (target<required) target=required;
    size_t available=heap->limit-heap->stats.backed_bytes;
    const size_t image_available=heap->image->memory_limit-heap->image->allocated_bytes;
    if (available>image_available) available=image_available;
    available&=~(size_t)(DS_HEAP_PAGE-1);
    if (target>available) target=available;
    if (target<required || target>UINT32_MAX ||
        (uint64_t)address+target>(uint64_t)DS_HEAP_BASE+DS_HEAP_WINDOW) return false;
    const uint64_t start=align_up(address,alignment);
    const uint32_t descriptors=1u+(start!=address)+(start+need!=(uint64_t)address+target);
    if (descriptors>DS_HEAP_MAX_BLOCKS-heap->block_count) return false;
    if (!ds_image_add_zero_region(heap->image,address,(size_t)target,DS_PF_R|DS_PF_W,NULL,0)) return false;
    unsigned char *backing=ds_image_address(heap->image,address,(size_t)target,DS_PF_R|DS_PF_W);
    const uint32_t arena=heap->stats.arena_count++;
    heap->arenas[arena]=(DsHeapArena){address,(uint32_t)target,backing};
    heap->blocks[heap->block_count++]=(DsHeapBlock){address,(uint32_t)target,0,0,arena,false};
    heap->stats.backed_bytes+=(size_t)target;
    return true;
}

DsHeapStatus ds_heap_alloc(DsHeap *heap,uint64_t size,uint32_t alignment,uint32_t *guest)
{
    if (!guest) return DS_HEAP_BAD_REQUEST;
    *guest=0;
    if (!ds_heap_validate(heap)) return DS_HEAP_CORRUPT;
    ++heap->stats.allocation_calls;
    if (alignment<16 || alignment>65536 || (alignment&(alignment-1)) || size>UINT32_MAX-15u) {
        ++heap->stats.failed_allocations;
        return DS_HEAP_BAD_REQUEST;
    }
    const uint32_t need=(uint32_t)align_up(size?size:16,16);
    if (need>heap->limit) { ++heap->stats.failed_allocations;return DS_HEAP_OUT_OF_MEMORY; }
    for (unsigned pass=0;pass<2;++pass) {
        for (uint32_t i=0;i<heap->block_count;++i) {
            const DsHeapBlock old=heap->blocks[i];
            if (old.used) continue;
            const uint64_t start=align_up(old.address,alignment),end=start+need;
            if (end>(uint64_t)old.address+old.size) continue;
            const uint32_t prefix=(uint32_t)(start-old.address),suffix=(uint32_t)(old.address+old.size-end);
            const uint32_t extra=(prefix!=0)+(suffix!=0);
            if (extra>DS_HEAP_MAX_BLOCKS-heap->block_count) continue;
            void *host=ds_image_address(heap->image,(uint32_t)start,need,DS_PF_R|DS_PF_W);
            if (!host) return DS_HEAP_CORRUPT;
            memmove(&heap->blocks[i+extra+1],&heap->blocks[i+1],(heap->block_count-i-1)*sizeof(old));
            heap->block_count+=extra;
            uint32_t slot=i;
            if (prefix) heap->blocks[slot++]=(DsHeapBlock){old.address,prefix,0,0,old.arena,false};
            heap->blocks[slot++]=(DsHeapBlock){(uint32_t)start,need,(uint32_t)size,alignment,old.arena,true};
            if (suffix) heap->blocks[slot]=(DsHeapBlock){(uint32_t)end,suffix,0,0,old.arena,false};
            /* Deterministic bytes on fresh/reused backed storage; no guest holes. */
            memset(host,0,need);
            ++heap->stats.live_allocations;
            heap->stats.live_bytes+=need;
            heap->stats.requested_bytes+=(size_t)size;
            if (heap->stats.live_bytes>heap->stats.peak_live_bytes) heap->stats.peak_live_bytes=heap->stats.live_bytes;
            *guest=(uint32_t)start;
            return DS_HEAP_OK;
        }
        if (pass || !grow(heap,need,alignment)) break;
    }
    ++heap->stats.failed_allocations;
    return DS_HEAP_OUT_OF_MEMORY;
}

DsHeapStatus ds_heap_free(DsHeap *heap,uint64_t guest)
{
    if (!ds_heap_validate(heap)) return DS_HEAP_CORRUPT;
    if (!guest) return DS_HEAP_OK;
    if (guest>UINT32_MAX) return DS_HEAP_INVALID_FREE;
    for (uint32_t i=0;i<heap->block_count;++i) {
        DsHeapBlock *b=&heap->blocks[i];
        if (b->address!=guest || !b->used) continue;
        --heap->stats.live_allocations;
        heap->stats.live_bytes-=b->size;
        heap->stats.requested_bytes-=b->requested;
        ++heap->stats.free_calls;
        b->used=false;b->requested=0;b->alignment=0;
        if (i+1<heap->block_count && !b[1].used && b->arena==b[1].arena) {
            b->size+=b[1].size;
            memmove(b+1,b+2,(heap->block_count-i-2)*sizeof(*b));
            --heap->block_count;
        }
        if (i && !b[-1].used && b[-1].arena==b->arena) {
            b[-1].size+=b->size;
            memmove(b,b+1,(heap->block_count-i-1)*sizeof(*b));
            --heap->block_count;
        }
        if (heap->block_count<DS_HEAP_MAX_BLOCKS)
            memset(&heap->blocks[heap->block_count],0,sizeof(*b));
        return DS_HEAP_OK;
    }
    return DS_HEAP_INVALID_FREE;
}

bool ds_heap_destroy(DsHeap *heap)
{
    if (!ds_heap_validate(heap)) return false;
    for (uint32_t i=0;i<heap->stats.arena_count;++i) {
        const DsHeapArena *a=&heap->arenas[i];
        if (!ds_image_remove_zero_region(heap->image,a->address,a->size,a->backing,NULL,0)) return false;
    }
    memset(heap,0,sizeof(*heap));
    return true;
}

void ds_heap_detach(DsHeap *heap)
{
    if (heap) memset(heap,0,sizeof(*heap));
}
