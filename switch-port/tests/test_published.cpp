#include "ds_entry.h"
#include "ds_reference.h"
#include <cstdio>
#include <cstring>

static unsigned checks,failures;
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    const bool normal=std::strcmp(argv[2],"--explore-published")==0;
    DsImage image={};DsEntryResult r={};char error[256]={};
    CHECK(ds_image_load_file(&image,argv[1],48u*1024u*1024u,error,sizeof(error)),"verified image loads");
    const bool ok=ds_entry_run_locale_published(&image,&r,error,sizeof(error));
    std::printf("PUBLISHED ok=%d stop=%d callee=%08x functions=%u accesses=%u completed=%u r3=%llx r4=%llx r5=%llx r6=%llx lr=%llx sp=%llx toc=%llx mutexes=%u thread_ids=%u locks=%u/%u held=%u error=%s\n",
        ok,r.stop,r.callee,r.function_count,r.trace_count,r.constructors_completed,
        (unsigned long long)r.r3,(unsigned long long)r.r4,(unsigned long long)r.r5,(unsigned long long)r.r6,
        (unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.toc,
        r.mutex_count,r.thread_id_calls,r.lock_calls,r.unlock_calls,r.native_held_at_stop,error);
    for(unsigned i=0;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i)std::printf("%u: %08x\n",i,r.functions[i]);
    if(normal) {
        CHECK(ok && r.stop==DS_ENTRY_LOCALE_PUBLISHED && !error[0] && r.constructors_completed==12 &&
              r.function_count==697 && r.trace_count==12492,"original locale publication reaches its exact checkpoint");
        CHECK(r.callee==0x168ABD0 && r.r3==40 && r.r4==0x1AD58D0 && r.r5==0x1CE6510 && r.r6==0x1CE6500 &&
              r.lr==0x16A051C && r.stack_pointer==0xFEFFB60 && r.toc==0x19D3BAC,
              "next mutex allocation is stopped before operator-new entry");
        CHECK(r.runtime_heap.allocation_calls==3 && !r.runtime_heap.free_calls && !r.runtime_heap.failed_allocations &&
              r.runtime_heap.live_allocations==3 && r.runtime_heap.live_bytes==128 && r.runtime_heap.requested_bytes==96 &&
              r.runtime_heap.peak_live_bytes==128 && r.runtime_heap.backed_bytes==65536 && r.runtime_heap.arena_count==1 &&
              r.runtime_last_malloc_address==DS_HEAP_BASE+64 && r.runtime_last_malloc_size==52,
              "string initialization requires no further guest allocation");
        CHECK(image.allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000+65536 && image.segment_count==7 &&
              ds_image_address(&image,DS_HEAP_BASE,65536,DS_PF_R|DS_PF_W) &&
              !ds_image_address(&image,DS_HEAP_BASE,1,DS_PF_X),"all heap backing remains budgeted RW");
        for(unsigned offset=0;offset<128;offset+=4) {
            const uint32_t expected=offset==24?2u:offset==48?DS_HEAP_BASE+64:offset==64?0x18C7DE0u:
                offset==68?3u:offset==80?0x3Fu:offset==92?0x43000000u:offset==108?1u:offset==112?15u:0u;
            uint32_t v=0;
            CHECK(ds_image_read_be32(&image,DS_HEAP_BASE+offset,&v) && v==expected,
                  "complete heap contents include C terminator, capacity, flags, refcount, handle and padding");
        }
        uint32_t v=0;
        CHECK(ds_image_read_be32(&image,0xE000430,&v) && v==DS_HEAP_BASE+64 &&
              ds_image_read_be32(&image,0x1CE672C,&v) && v==DS_HEAP_BASE+64 &&
              ds_image_read_be32(&image,0x1CE6730,&v) && v==DS_HEAP_BASE+64,
              "original TLS setter and both globals publish the allocated locale object");
        CHECK(r.thread_id_calls==5 && r.lock_calls==19 && r.unlock_calls==19 && !r.native_held_at_stop &&
              r.mutex_count==13 && r.kernel_mutex_calls==5 && r.registry_entries==10 && r.callback_entries==40,
              "locale recursion balances and all previous metadata survives");
        DsEntryResult replay={};char message[256]={};
        CHECK(!ds_entry_run_locale_published(&image,&replay,message,sizeof(message)) && replay.stop==DS_ENTRY_BAD_INPUT,
              "in-place replay is refused");
        for(unsigned attempt=0;attempt<2;++attempt) {
            DsImage fresh={};DsEntryResult again={};
            CHECK(ds_image_load_file(&fresh,argv[1],48u*1024u*1024u,message,sizeof(message)),"fresh image loads");
            CHECK(ds_entry_run_locale_published(&fresh,&again,message,sizeof(message)) &&
                  again.stop==DS_ENTRY_LOCALE_PUBLISHED && again.function_count==697 && again.trace_count==12492 &&
                  !again.native_live_after_cleanup && !again.runtime_heap_metadata_after_cleanup,
                  "fresh repeated execution recreates ownership and publication");
            ds_image_destroy(&fresh);CHECK(!fresh.allocated_bytes,"fresh image backing is released");
        }
        struct Prior {bool (*run)(DsImage*,DsEntryResult*,char*,size_t);uint32_t functions,accesses;DsEntryStop stop;};
        static constexpr Prior prior[]={
            {ds_entry_run_three_constructors,213,4141,DS_ENTRY_THIRD_CONSTRUCTOR_READY},
            {ds_entry_run_four_constructors,354,5937,DS_ENTRY_FOURTH_CONSTRUCTOR_READY},
            {ds_entry_run_six_constructors,424,7407,DS_ENTRY_SIXTH_CONSTRUCTOR_READY},
            {ds_entry_run_nine_constructors,561,10052,DS_ENTRY_NINTH_CONSTRUCTOR_READY},
            {ds_entry_run_ten_constructors,600,11007,DS_ENTRY_TENTH_CONSTRUCTOR_READY},
            {ds_entry_run_eleven_constructors,603,11044,DS_ENTRY_ELEVENTH_CONSTRUCTOR_READY},
            {ds_entry_run_twelve_constructors,641,12134,DS_ENTRY_TWELFTH_CONSTRUCTOR_READY},
            {ds_entry_run_runtime_heap,652,12206,DS_ENTRY_RUNTIME_HEAP_BOUNDARY},
            {ds_entry_run_runtime_allocated,658,12247,DS_ENTRY_RUNTIME_ALLOCATED},
            {ds_entry_run_locale_object,669,12314,DS_ENTRY_LOCALE_OBJECT_READY}
        };
        for(const auto &p:prior) {
            DsImage old_image={};DsEntryResult old={};
            CHECK(ds_image_load_file(&old_image,argv[1],48u*1024u*1024u,message,sizeof(message)),"prior checkpoint image loads");
            CHECK(p.run(&old_image,&old,message,sizeof(message)) && old.stop==p.stop &&
                  old.function_count==p.functions && old.trace_count==p.accesses &&
                  !old.native_live_after_cleanup && !old.runtime_heap_metadata_after_cleanup,
                  "older APIs retain their original boundaries, counts and cleanup");
            ds_image_destroy(&old_image);CHECK(!old_image.allocated_bytes,"prior image backing is released");
        }
    } else {
        CHECK(!ok && r.constructors_completed==12,"fault cannot claim locale publication success");
        if(std::strcmp(argv[2],"--explore-published-state-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(error,"0x6000005C") && !r.native_held_at_stop,
                  "final validation identifies a corrupted locale name after all locks release");
        else if(std::strcmp(argv[2],"--explore-published-tls-state-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(error,"0x0E000430") && !r.native_held_at_stop,
                  "final validation rejects lost TLS publication");
        else if(std::strcmp(argv[2],"--explore-published-copy-input-fault")==0)
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && !r.fault_write && r.fault_address==0x70000000 &&
                  r.native_held_at_stop==1,"original copy rejects an unmapped source with the guard held");
        else if(std::strcmp(argv[2],"--explore-published-copy-output-fault")==0)
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && r.fault_write && r.fault_address==0x70000000 &&
                  r.native_held_at_stop==1,"original copy rejects an unmapped destination with the guard held");
        else if(std::strcmp(argv[2],"--explore-published-tls-consumer-fault")==0)
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && !r.fault_write && r.fault_address==0x70000004 &&
                  r.native_held_at_stop==1,"consumer follows actual TLS pointer and rejects missing backing");
        else if(std::strcmp(argv[2],"--explore-published-capacity-fault")==0)
            CHECK(r.stop==DS_ENTRY_UNSUPPORTED_CALL && r.callee==0x12470 && r.runtime_heap.allocation_calls==3 &&
                  r.native_held_at_stop==1,"unaudited string growth remains an explicit boundary");
        else if(std::strcmp(argv[2],"--explore-published-argument-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.callee==0x168ABD0 && r.r3==41 &&
                  r.runtime_heap.allocation_calls==3 && !r.native_held_at_stop,"next allocation size is checked without allocating");
        else CHECK(false,"known test mode");
    }
    CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup &&
          !r.runtime_heap_metadata_after_cleanup,"all native resources and heap metadata clear on stop and failure");
    ds_image_destroy(&image);CHECK(!image.allocated_bytes && !image.segment_count,"all image backing is released");
    std::printf("Locale publication tests: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
