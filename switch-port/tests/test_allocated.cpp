#include "ds_entry.h"
#include "ds_reference.h"
#include <cstdio>
#include <cstring>

static unsigned checks,failures;
static const char *image_path;
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
static bool load(DsImage &image)
{
    char error[256]={};
    const bool ok=ds_image_load_file(&image,image_path,48u*1024u*1024u,error,sizeof(error));
    CHECK(ok,"verified game image loads");
    return ok;
}
static void cleanup(DsImage &image)
{
    ds_image_destroy(&image);
    CHECK(!image.segment_count && !image.allocated_bytes,"image destruction releases heap and all startup arenas");
}
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    image_path=argv[1];const char *mode=argv[2];
    const bool normal=std::strcmp(mode,"--explore-allocated")==0;
    const bool state=std::strcmp(mode,"--explore-allocated-state-fault")==0;
    const bool native=std::strcmp(mode,"--explore-allocated-native-fault")==0;
    const bool provider=std::strcmp(mode,"--explore-allocated-provider-fault")==0;
    DsImage image={};if(!load(image))return 1;
    DsEntryResult r={};char e[256]={};
    const bool ok=ds_entry_run_runtime_allocated(&image,&r,e,sizeof(e));
    std::printf("ALLOCATED ok=%d stop=%d callee=%08x functions=%u accesses=%u completed=%u r3=%llx r4=%llx r5=%llx r6=%llx lr=%llx sp=%llx toc=%llx mutexes=%u thread_ids=%u error=%s\n",
        ok,r.stop,r.callee,r.function_count,r.trace_count,r.constructors_completed,
        (unsigned long long)r.r3,(unsigned long long)r.r4,(unsigned long long)r.r5,(unsigned long long)r.r6,
        (unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.toc,r.mutex_count,r.thread_id_calls,e);
    std::printf("HEAP calls=%u free=%u failed=%u live=%u bytes=%zu requested=%zu peak=%zu backed=%zu arenas=%u first=%08x/%u last=%08x/%u status=%d image=%zu metadata=%u\n",
        r.runtime_heap.allocation_calls,r.runtime_heap.free_calls,r.runtime_heap.failed_allocations,
        r.runtime_heap.live_allocations,r.runtime_heap.live_bytes,r.runtime_heap.requested_bytes,
        r.runtime_heap.peak_live_bytes,r.runtime_heap.backed_bytes,r.runtime_heap.arena_count,
        r.runtime_malloc_address,r.runtime_malloc_size,r.runtime_last_malloc_address,r.runtime_last_malloc_size,
        r.runtime_heap_status,image.allocated_bytes,r.runtime_heap_metadata_after_cleanup);
    for(unsigned i=137;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i)std::printf("%u: %08x\n",i,r.functions[i]);
    if(normal) {
        CHECK(ok && r.stop==DS_ENTRY_RUNTIME_ALLOCATED && !e[0] && r.constructors_completed==12,
              "real allocations and caller initialization reach a validated later dependency");
        CHECK(r.runtime_malloc_address==DS_HEAP_BASE && r.runtime_malloc_size==40 &&
              r.runtime_last_malloc_address==DS_HEAP_BASE+48 && r.runtime_last_malloc_size==4,
              "real operator-new requests return aligned guest offsets from the same backed arena");
        CHECK(r.runtime_heap.allocation_calls==2 && !r.runtime_heap.failed_allocations &&
              !r.runtime_heap.free_calls && r.runtime_heap.live_allocations==2 && r.runtime_heap.live_bytes==64 &&
              r.runtime_heap.requested_bytes==44 && r.runtime_heap.peak_live_bytes==64 &&
              r.runtime_heap.backed_bytes==65536 && r.runtime_heap.arena_count==1,
              "physical backing and logical allocation accounting are independently exact");
        CHECK(image.allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000+65536 && image.segment_count==7,
              "new backing is charged to the same guest-image cap");
        CHECK(r.mutex_count==13 && r.mutex_addresses[12]==DS_HEAP_BASE+16 &&
              r.thread_id_calls==2 && r.lock_calls==17 && r.unlock_calls==17 && r.kernel_mutex_calls==5,
              "original runtime consumes the first allocation to create its thirteenth CRT mutex");
        uint32_t v=0;
        CHECK(ds_image_read_be32(&image,0x1CE64EC,&v) && v==DS_HEAP_BASE &&
              ds_image_read_be32(&image,DS_HEAP_BASE+24,&v) && v==2,
              "actual caller publishes the malloc address and initializes the embedded mutex protocol");
        CHECK(ds_image_address(&image,DS_HEAP_BASE,65536,DS_PF_R|DS_PF_W) &&
              !ds_image_address(&image,DS_HEAP_BASE,1,DS_PF_X) &&
              !ds_image_address(&image,DS_HEAP_BASE+65536,1,DS_PF_R),
              "allocated arena is fully backed RW, non-executable, and bounded by an unmapped neighbor");
        CHECK(r.registry_entries==10 && r.callback_entries==40 && r.allocation_calls==14 && !r.bootstrap_fallback_calls,
              "runtime heap does not consume or replace the existing game pool");
        DsEntryResult reused={};
        CHECK(!ds_entry_run_runtime_allocated(&image,&reused,e,sizeof(e)) && reused.stop==DS_ENTRY_BAD_INPUT,
              "heap stage cannot replay into an initialized image");
        for(unsigned attempt=0;attempt<2;++attempt) {
            DsImage fresh={};if(!load(fresh))return 1;
            DsEntryResult again={};char message[256]={};
            CHECK(ds_entry_run_runtime_allocated(&fresh,&again,message,sizeof(message)) &&
                  again.runtime_malloc_address==DS_HEAP_BASE && again.runtime_heap.backed_bytes==65536 &&
                  !again.runtime_heap_metadata_after_cleanup,
                  "fresh reload repeats original guest addresses without stale native metadata");
            cleanup(fresh);
        }
        DsImage old={};if(!load(old))return 1;
        DsEntryResult prior={};char message[256]={};
        CHECK(ds_entry_run_runtime_heap(&old,&prior,message,sizeof(message)) && prior.function_count==652 &&
              prior.trace_count==12206 && !prior.runtime_heap.allocation_calls && !prior.runtime_heap.backed_bytes &&
              old.allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000,
              "v0.19 checkpoint still stops before allocating any backing");
        cleanup(old);
    } else {
        CHECK(!ok && r.constructors_completed==12,"fault fixture cannot claim allocation-stage success");
        if(state) {
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(e,"0x60000018")!=nullptr &&
                  r.runtime_heap.allocation_calls==2,"validator identifies a corrupted real heap mutex word");
        } else if(native) {
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && r.fault_write && r.fault_address==0 && r.mutex_count==13 &&
                  r.runtime_heap.live_allocations==1 && r.runtime_heap.backed_bytes==65536,
                  "failure after actual native mutex creation retains owned backing and cleans native objects");
        } else if(provider) {
            CHECK(r.stop==DS_ENTRY_HEAP_FAILURE && r.r3==0 && r.runtime_heap_status==DS_HEAP_OUT_OF_MEMORY &&
                  r.runtime_heap.allocation_calls==1 && r.runtime_heap.failed_allocations==1 &&
                  !r.runtime_heap.backed_bytes && !r.runtime_heap.live_allocations && r.mutex_count==12,
                  "backing-provider failure returns null with explicit failure and no uncharged or leaked arena");
        } else {
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && !r.runtime_heap.backed_bytes,
                  "wrong allocator ABI is rejected before guest pages are allocated");
        }
    }
    CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup &&
          !r.runtime_heap_metadata_after_cleanup,"all native synchronization and heap metadata clear after stop or failure");
    cleanup(image);
    std::printf("Backed runtime allocation tests: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
