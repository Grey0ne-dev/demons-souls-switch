#include "ds_entry.h"
#include "ds_reference.h"
#include <cstdio>
#include <cstring>

static unsigned checks,failures;
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    const bool normal=std::strcmp(argv[2],"--explore-locale")==0;
    DsImage image={};DsEntryResult r={};char error[256]={};
    CHECK(ds_image_load_file(&image,argv[1],48u*1024u*1024u,error,sizeof(error)),"verified image loads");
    const bool ok=ds_entry_run_locale_object(&image,&r,error,sizeof(error));
    std::printf("LOCALE ok=%d stop=%d callee=%08x functions=%u accesses=%u completed=%u r3=%llx r4=%llx r5=%llx r6=%llx lr=%llx sp=%llx toc=%llx mutexes=%u thread_ids=%u locks=%u/%u held=%u error=%s\n",
        ok,r.stop,r.callee,r.function_count,r.trace_count,r.constructors_completed,
        (unsigned long long)r.r3,(unsigned long long)r.r4,(unsigned long long)r.r5,(unsigned long long)r.r6,
        (unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.toc,
        r.mutex_count,r.thread_id_calls,r.lock_calls,r.unlock_calls,r.native_held_at_stop,error);
    for(unsigned i=137;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i)std::printf("%u: %08x\n",i,r.functions[i]);
    if(normal) {
        CHECK(ok && r.stop==DS_ENTRY_LOCALE_OBJECT_READY && !error[0] && r.constructors_completed==12 &&
              r.function_count==669 && r.trace_count==12314,"locale scalar initialization is validated");
        CHECK(r.runtime_heap.allocation_calls==3 && !r.runtime_heap.free_calls && !r.runtime_heap.failed_allocations &&
              r.runtime_heap.live_allocations==3 && r.runtime_heap.live_bytes==128 && r.runtime_heap.requested_bytes==96 &&
              r.runtime_heap.peak_live_bytes==128 && r.runtime_heap.backed_bytes==65536 && r.runtime_heap.arena_count==1,
              "third real allocation uses existing backing with exact requested and rounded accounting");
        CHECK(r.runtime_last_malloc_address==DS_HEAP_BASE+64 && r.runtime_last_malloc_size==52 &&
              r.runtime_malloc_address==DS_HEAP_BASE && r.runtime_malloc_size==40 && r.runtime_heap_status==DS_HEAP_OK,
              "reviewed 52-byte operator-new request returns the next aligned guest offset");
        CHECK(image.allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000+65536 && image.segment_count==7 &&
              ds_image_address(&image,DS_HEAP_BASE,65536,DS_PF_R|DS_PF_W) &&
              !ds_image_address(&image,DS_HEAP_BASE,1,DS_PF_X) && !ds_image_address(&image,DS_HEAP_BASE+65536,1,DS_PF_R),
              "physical backing stays budgeted RW and cannot expose the adjacent hole");
        uint32_t value=0;
        for(unsigned offset=0;offset<128;offset+=4) {
            const uint32_t expected=offset==24?2u:offset==64?0x18C7DE0u:offset==68?1u:offset==112?15u:0u;
            CHECK(ds_image_read_be32(&image,DS_HEAP_BASE+offset,&value) && value==expected,
                  "all original object fields and rounded padding match the initialized guest bytes");
        }
        CHECK(ds_image_read_be32(&image,0xE000430,&value) && !value &&
              ds_image_read_be32(&image,DS_HEAP_BASE+48,&value) && !value,
              "real TLS locale slot and public handle stay unpublished before string construction");
        CHECK(r.thread_id_calls==3 && r.lock_calls==18 && r.unlock_calls==17 && r.native_held_at_stop==1 &&
              r.mutex_count==13 && r.kernel_mutex_calls==5 && r.registry_entries==10 && r.callback_entries==40,
              "locale guard is actually held with all previous metadata intact");
        DsEntryResult replay={};char message[256]={};
        CHECK(!ds_entry_run_locale_object(&image,&replay,message,sizeof(message)) && replay.stop==DS_ENTRY_BAD_INPUT &&
              image.allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000+65536,
              "in-place replay cannot rerun constructors or allocate another arena");
        for(unsigned attempt=0;attempt<2;++attempt) {
            DsImage fresh={};DsEntryResult again={};
            CHECK(ds_image_load_file(&fresh,argv[1],48u*1024u*1024u,message,sizeof(message)),"fresh image loads");
            CHECK(ds_entry_run_locale_object(&fresh,&again,message,sizeof(message)) &&
                  again.stop==DS_ENTRY_LOCALE_OBJECT_READY && again.function_count==669 && again.trace_count==12314 &&
                  !again.native_live_after_cleanup && !again.runtime_heap_metadata_after_cleanup,
                  "fresh repeated execution recreates locale ownership after held-lock cleanup");
            ds_image_destroy(&fresh);
            CHECK(!fresh.allocated_bytes,"fresh image backing is released");
        }
        DsImage prior={};DsEntryResult old={};
        CHECK(ds_image_load_file(&prior,argv[1],48u*1024u*1024u,message,sizeof(message)),"old checkpoint image loads");
        CHECK(ds_entry_run_runtime_allocated(&prior,&old,message,sizeof(message)) && old.stop==DS_ENTRY_RUNTIME_ALLOCATED &&
              old.function_count==658 && old.trace_count==12247 && old.runtime_heap.allocation_calls==2,
              "v0.20 checkpoint still stops before the selected consumer is counted or entered");
        ds_image_destroy(&prior);
        CHECK(!prior.allocated_bytes,"old checkpoint backing is released");
    } else {
        CHECK(!ok && r.constructors_completed==12,"fault cannot claim locale checkpoint success");
        if(std::strcmp(argv[2],"--explore-locale-state-fault")==0) {
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(error,"0x60000044") &&
                  r.runtime_heap.live_allocations==3 && r.native_held_at_stop==1,
                  "final validation identifies the corrupted locale reference count");
        } else if(std::strcmp(argv[2],"--explore-locale-argument-fault")==0) {
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.callee==0x168ABD0 && r.r3==53 &&
                  r.runtime_heap.allocation_calls==2 && r.runtime_heap.live_bytes==64 && r.native_held_at_stop==1,
                  "wrong third allocation ABI is rejected before allocating the locale object");
        } else if(std::strcmp(argv[2],"--explore-locale-lock-fault")==0) {
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && r.fault_write && !r.fault_address &&
                  r.runtime_heap.allocation_calls==2 && r.native_held_at_stop==1,
                  "checked fault immediately after locale lock acquisition cleans a held native lock");
        } else if(std::strcmp(argv[2],"--explore-locale-tls-fault")==0) {
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && !r.fault_write && r.fault_address==0x70000004 &&
                  r.runtime_heap.allocation_calls==2 && r.native_held_at_stop==1,
                  "existing TLS locale pointer is consumed by original code and faults on unmapped backing");
        } else CHECK(false,"known test mode");
    }
    CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup &&
          !r.runtime_heap_metadata_after_cleanup,"all native resources and heap metadata clear even with a held locale guard");
    ds_image_destroy(&image);
    CHECK(!image.allocated_bytes && !image.segment_count,"image destruction releases all arenas");
    std::printf("Locale object tests: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
