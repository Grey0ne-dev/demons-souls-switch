#include "ds_heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,failures;
static bool fail_backing;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t count,size_t size)
{
    if (fail_backing) { fail_backing=false;return NULL; }
    return __real_calloc(count,size);
}
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)

static void finish(DsHeap *heap,DsImage *image)
{
    CHECK(ds_heap_validate(heap),"heap invariants hold before teardown");
    CHECK(ds_heap_destroy(heap),"owned heap arenas released");
    CHECK(image->allocated_bytes==0 && image->segment_count==0,"heap teardown restores exact image budget");
    ds_image_destroy(image);
}

static void basic(void)
{
    DsImage image={0};image.memory_limit=8u*1024*1024;
    DsHeap heap={0};uint32_t p[8]={0};
    CHECK(ds_heap_init(&heap,&image,DS_HEAP_DEFAULT_LIMIT),"empty heap attaches without backing");
    CHECK(image.allocated_bytes==0 && heap.stats.backed_bytes==0,"no physical pages committed at init");
    CHECK(ds_heap_alloc(&heap,40,16,&p[0])==DS_HEAP_OK && p[0]==DS_HEAP_BASE,"first runtime request returns a guest address");
    CHECK(heap.stats.backed_bytes==65536 && image.allocated_bytes==65536 && heap.stats.live_bytes==48 &&
          heap.stats.requested_bytes==40,"full physical arena and rounded allocation charged independently");
    unsigned char *host=ds_image_address(&image,p[0],48,DS_PF_R|DS_PF_W);
    CHECK(host && (uintptr_t)host!=p[0],"guest offset is translated to separately allocated host backing");
    CHECK(!ds_image_address(&image,DS_HEAP_BASE-1,1,DS_PF_R) &&
          !ds_image_address(&image,DS_HEAP_BASE+65536,1,DS_PF_R),"neighboring guest holes stay inaccessible");
    CHECK(!ds_image_address(&image,p[0],1,DS_PF_X),"heap backing is never executable");
    memset(host,0xA5,40);
    CHECK(ds_heap_alloc(&heap,17,4096,&p[1])==DS_HEAP_OK && !(p[1]&4095),"aligned allocation splits leading free padding");
    CHECK(ds_heap_alloc(&heap,33,65536,&p[2])==DS_HEAP_OK && !(p[2]&65535),"large alignment obtains owned backed storage");
    CHECK(ds_heap_alloc(&heap,0,16,&p[3])==DS_HEAP_OK && p[3]!=p[0],"zero-byte allocation owns a distinct freeable minimum block");
    CHECK(ds_heap_validate(&heap),"splitting and multiple arenas preserve metadata invariants");
    CHECK(ds_heap_free(&heap,p[0]+1)==DS_HEAP_INVALID_FREE,"interior pointers rejected");
    CHECK(ds_heap_free(&heap,(uint64_t)p[0]+UINT64_C(0x100000000))==DS_HEAP_INVALID_FREE,"wide addresses cannot truncate into owned pointers");
    CHECK(ds_heap_free(&heap,0)==DS_HEAP_OK,"free null is a no-op");
    CHECK(ds_heap_free(&heap,p[0])==DS_HEAP_OK,"owned block can be freed");
    CHECK(ds_heap_free(&heap,p[0])==DS_HEAP_INVALID_FREE,"double free rejected without metadata changes");
    CHECK(ds_heap_alloc(&heap,40,16,&p[4])==DS_HEAP_OK && p[4]==p[0],"freed low block is reused");
    host=ds_image_address(&image,p[4],48,DS_PF_R);
    bool zero=true;for(unsigned i=0;i<48;++i)zero&=host[i]==0;
    CHECK(zero,"reused allocated bytes are cleared without touching neighboring live blocks");
    for(unsigned i=1;i<5;++i) CHECK(ds_heap_free(&heap,p[i])==DS_HEAP_OK,"remaining live allocation freed");
    CHECK(heap.stats.live_bytes==0 && heap.stats.requested_bytes==0 && heap.stats.live_allocations==0 &&
          heap.block_count==heap.stats.arena_count,"adjacent free pieces coalesce within each arena");
    CHECK(image.allocated_bytes==heap.stats.backed_bytes && image.allocated_bytes>0,"free retains physical backing charge until arena teardown");
    finish(&heap,&image);
}

static void failures_and_budget(void)
{
    DsImage image={0};image.memory_limit=65536;
    DsHeap heap={0};uint32_t guest=123,other=0;
    CHECK(ds_heap_init(&heap,&image,65536),"bounded test heap initialized");
    fail_backing=true;
    CHECK(ds_heap_alloc(&heap,40,16,&guest)==DS_HEAP_OUT_OF_MEMORY && guest==0 &&
          !image.allocated_bytes && !image.segment_count && !heap.stats.arena_count,"host allocation failure is transactional and returns null");
    CHECK(ds_heap_alloc(&heap,65536,16,&guest)==DS_HEAP_OK && image.allocated_bytes==65536,"exact remaining budget can be committed");
    unsigned char *host=ds_image_address(&image,guest,65536,DS_PF_W);memset(host,0x5A,65536);
    CHECK(ds_heap_alloc(&heap,1,16,&other)==DS_HEAP_OUT_OF_MEMORY && other==0 &&
          image.allocated_bytes==65536 && heap.stats.live_allocations==1,"exhaustion does not exceed the configured image budget");
    CHECK(host[0]==0x5A && host[65535]==0x5A,"OOM preserves the existing allocation");
    CHECK(ds_heap_alloc(&heap,UINT64_MAX,16,&other)==DS_HEAP_BAD_REQUEST && other==0,"size overflow rejected");
    CHECK(ds_heap_alloc(&heap,1,24,&other)==DS_HEAP_BAD_REQUEST,"non-power-of-two alignment rejected");
    CHECK(ds_heap_alloc(&heap,1,8,&other)==DS_HEAP_BAD_REQUEST,"under-alignment rejected");
    CHECK(ds_heap_alloc(&heap,1,131072,&other)==DS_HEAP_BAD_REQUEST,"unbounded alignment rejected");
    CHECK(ds_heap_validate(&heap),"failure cases preserve the heap");
    CHECK(ds_heap_free(&heap,guest)==DS_HEAP_OK,"full arena allocation freed");
    CHECK(ds_heap_alloc(&heap,65536,16,&other)==DS_HEAP_OK && other==guest,"full coalesced arena reused without a second budget charge");
    finish(&heap,&image);

    DsImage charged={0};charged.memory_limit=65536;
    DsHeap tight={0};char error[128]={0};
    CHECK(ds_image_add_zero_region(&charged,0x10000,4096,DS_PF_R|DS_PF_W,error,sizeof(error)),"unrelated arena consumes shared budget");
    CHECK(ds_heap_init(&tight,&charged,65536),"heap uses the same charged image");
    CHECK(ds_heap_alloc(&tight,60000,16,&guest)==DS_HEAP_OK && charged.allocated_bytes==65536,"arena growth clamps to actual remaining physical allowance");
    CHECK(tight.stats.backed_bytes==61440,"only available page-aligned backing committed");
    const void *foreign=charged.segments[0].data;
    CHECK(!ds_image_remove_zero_region(&charged,0x10000,4096,tight.arenas[0].backing,error,sizeof(error)) &&
          charged.segments[0].data==foreign,"release requires the exact original backing token");
    CHECK(ds_heap_destroy(&tight) && charged.allocated_bytes==4096 && charged.segment_count==1,"heap teardown leaves unrelated image regions intact");
    ds_image_destroy(&charged);
}

static void coalescing_and_limits(void)
{
    DsImage image={0};image.memory_limit=65536;
    DsHeap heap={0};uint32_t p[DS_HEAP_MAX_BLOCKS+2]={0},q=0;
    CHECK(ds_heap_init(&heap,&image,65536),"coalescing heap initialized");
    for(unsigned i=0;i<4;++i) CHECK(ds_heap_alloc(&heap,16384,16,&p[i])==DS_HEAP_OK,"four exact quarter-arena blocks allocated");
    CHECK(ds_heap_free(&heap,p[1])==DS_HEAP_OK && ds_heap_free(&heap,p[2])==DS_HEAP_OK,"free adjacent middle blocks");
    CHECK(ds_heap_alloc(&heap,32768,16,&q)==DS_HEAP_OK && q==p[1],"coalesced middle span satisfies larger request");
    CHECK(ds_heap_free(&heap,q)==DS_HEAP_OK && ds_heap_free(&heap,p[0])==DS_HEAP_OK &&
          ds_heap_free(&heap,p[3])==DS_HEAP_OK && heap.block_count==1,"both coalescing directions recover the whole arena");
    unsigned count=0;
    while(count<DS_HEAP_MAX_BLOCKS+2 && ds_heap_alloc(&heap,16,16,&p[count])==DS_HEAP_OK)++count;
    CHECK(count==DS_HEAP_MAX_BLOCKS-1 && heap.block_count==DS_HEAP_MAX_BLOCKS,"host metadata capacity is enforced before a split");
    CHECK(heap.stats.backed_bytes==65536 && ds_heap_validate(&heap),"metadata exhaustion keeps backing and existing ownership valid");
    for(unsigned i=0;i<count;++i) CHECK(ds_heap_free(&heap,p[i])==DS_HEAP_OK,"metadata-capacity allocation reclaimed");
    CHECK(heap.block_count==1,"reclamation recovers metadata slots");
    CHECK(ds_heap_alloc(&heap,65536,16,&q)==DS_HEAP_OK,"entire recovered arena remains usable");
    finish(&heap,&image);
}

static void random_sequences(void)
{
    struct Live {uint32_t guest,size;unsigned char pattern;} live[64]={{0}};
    DsImage image={0};image.memory_limit=DS_HEAP_DEFAULT_LIMIT;
    DsHeap heap={0};uint32_t seed=0x3517A19u;
    CHECK(ds_heap_init(&heap,&image,DS_HEAP_DEFAULT_LIMIT),"randomized heap initialized");
    for(unsigned step=0;step<3000;++step) {
        seed=seed*1664525u+1013904223u;unsigned slot=(seed>>16)%64;
        if(live[slot].guest) {
            unsigned char *host=ds_image_address(&image,live[slot].guest,live[slot].size,DS_PF_R);
            bool intact=host!=NULL;
            for(uint32_t i=0;host && i<live[slot].size;++i)intact&=host[i]==live[slot].pattern;
            CHECK(intact,"randomized live payload survives unrelated split/free/grow");
            CHECK(ds_heap_free(&heap,live[slot].guest)==DS_HEAP_OK,"randomized exact-owner free succeeds");
            live[slot].guest=0;
        } else {
            uint32_t size=1+(seed%2048),alignment=16u<<((seed>>24)%7),guest=0;
            CHECK(ds_heap_alloc(&heap,size,alignment,&guest)==DS_HEAP_OK && !(guest&(alignment-1)),"randomized aligned allocation succeeds within the bounded working set");
            unsigned char *host=ds_image_address(&image,guest,size,DS_PF_W);
            CHECK(host!=NULL,"randomized allocation is entirely writable backed memory");
            if(host)memset(host,(unsigned char)step,size);
            live[slot]=(struct Live){guest,size,(unsigned char)step};
        }
        CHECK(ds_heap_validate(&heap) && image.allocated_bytes<=image.memory_limit,"every randomized transition preserves allocator and budget invariants");
    }
    finish(&heap,&image);
}

static void ownership(void)
{
    DsImage image={0};image.memory_limit=131072;
    DsHeap heap={0};uint32_t p=0;char error[128]={0};
    CHECK(ds_heap_init(&heap,&image,65536),"ownership heap initialized");
    CHECK(ds_heap_alloc(&heap,40,16,&p)==DS_HEAP_OK,"ownership arena allocated");
    CHECK(!ds_heap_init(&heap,&image,65536),"live metadata cannot be reinitialized");
    DsSegment original=image.segments[0];
    image.segments[0].from_elf=true;
    CHECK(!ds_image_remove_zero_region(&image,DS_HEAP_BASE,65536,original.data,error,sizeof(error)),
          "owned-region removal cannot release ELF-marked data even with an exact pointer");
    CHECK(!ds_heap_validate(&heap) && !ds_heap_destroy(&heap),"heap cannot release an ELF-marked region");
    image.segments[0]=original;
    heap.blocks[0].size+=16;
    CHECK(!ds_heap_validate(&heap) && ds_heap_free(&heap,p)==DS_HEAP_CORRUPT,"corrupt metadata refuses pointer operations");
    heap.blocks[0].size-=16;
    CHECK(ds_heap_validate(&heap),"test-only metadata corruption restored");
    ds_heap_detach(&heap);
    CHECK(!heap.image && image.allocated_bytes==65536,"detach drops metadata while image retains physical ownership");
    ds_image_destroy(&image);
    CHECK(!image.allocated_bytes && !image.segment_count,"image destruction releases detached backing");
    image.memory_limit=131072;
    CHECK(ds_image_add_zero_region(&image,DS_HEAP_BASE,65536,DS_PF_R|DS_PF_W,error,sizeof(error)),"foreign region occupies heap's proposed guest base");
    CHECK(ds_heap_init(&heap,&image,65536),"heap attaches before collision check");
    CHECK(ds_heap_alloc(&heap,40,16,&p)==DS_HEAP_OUT_OF_MEMORY && !p && image.allocated_bytes==65536,
          "guest address collision cannot claim or overwrite foreign backing");
    CHECK(ds_heap_destroy(&heap) && image.allocated_bytes==65536,"empty failed heap does not release foreign backing");
    ds_image_destroy(&image);
}

static void arena_limits(void)
{
    DsImage image={0};image.memory_limit=16u*1024*1024;
    DsHeap heap={0};uint32_t guest=0;
    CHECK(ds_heap_init(&heap,&image,65536),"heap cap may be lower than the image allowance");
    CHECK(ds_heap_alloc(&heap,65537,16,&guest)==DS_HEAP_OUT_OF_MEMORY && !guest && !image.allocated_bytes,
          "separate heap cap rejects an oversized request before committing pages");
    finish(&heap,&image);
    image.memory_limit=DS_HEAP_DEFAULT_LIMIT;
    CHECK(ds_heap_init(&heap,&image,DS_HEAP_DEFAULT_LIMIT),"arena-capacity heap initialized");
    unsigned allocations=0;
    while(allocations<128 && ds_heap_alloc(&heap,65536,16,&guest)==DS_HEAP_OK)++allocations;
    CHECK(allocations==79 && heap.stats.arena_count==DS_HEAP_MAX_ARENAS,
          "bounded geometric growth reaches the exact independent arena-table limit");
    CHECK(ds_heap_validate(&heap) && image.allocated_bytes==heap.stats.backed_bytes &&
          image.allocated_bytes==79u*65536u && image.allocated_bytes<image.memory_limit,
          "arena-table exhaustion retains exact physical accounting below the cap");
    finish(&heap,&image);
}

int main(void)
{
    basic();failures_and_budget();coalescing_and_limits();random_sequences();ownership();arena_limits();
    printf("Guest heap tests: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
