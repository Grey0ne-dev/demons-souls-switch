#include "ds_entry.h"
#include "ds_reference.h"
#include <cstdio>
#include <cstring>
static unsigned checks,failures;
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    const bool normal=std::strcmp(argv[2],"--explore-first-facet")==0;
    DsImage image={};DsEntryResult r={};char error[256]={};
    CHECK(ds_image_load_file(&image,argv[1],48u*1024u*1024u,error,sizeof(error)),"verified image loads");
    const bool ok=ds_entry_run_first_facet(&image,&r,error,sizeof(error));
    std::printf("FACET ok=%d stop=%d nid=%08x callee=%08x functions=%u accesses=%u completed=%u r3=%llx r4=%llx r5=%llx r6=%llx lr=%llx sp=%llx toc=%llx mutexes=%u thread_ids=%u locks=%u/%u held=%u descriptor=%08x code=%08x error=%s\n",
        ok,r.stop,r.boundary_nid,r.callee,r.function_count,r.trace_count,r.constructors_completed,
        (unsigned long long)r.r3,(unsigned long long)r.r4,(unsigned long long)r.r5,(unsigned long long)r.r6,
        (unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.toc,
        r.mutex_count,r.thread_id_calls,r.lock_calls,r.unlock_calls,r.native_held_at_stop,r.indirect_descriptor,r.indirect_code,error);
    for(unsigned i=0;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i)std::printf("%u: %08x\n",i,r.functions[i]);
    if(normal){
        CHECK(ok && r.stop==DS_ENTRY_FIRST_FACET_READY && !error[0] && r.function_count==799 && r.trace_count==13238 &&
              r.constructors_completed==12,"facet construction and cleanup registration reach the exact checkpoint");
        CHECK(r.callee==0x9E96A0 && r.r3==DS_HEAP_BASE+208 && r.r4==32 && r.r5==1 &&
              r.r6==UINT64_C(0x7F7F7F7F7F7F7F7F) && r.lr==0x1688280 && r.stack_pointer==0xFEFFA90 &&
              r.toc==0x19B5040 && r.indirect_descriptor==0x1970348 && r.indirect_code==0x9E96A0,
              "typed virtual boundary checks the original opcode, descriptor and actual main TOC without entering its target");
        bool target_entered=false;for(unsigned i=0;i<r.function_count;++i)target_entered|=r.functions[i]==0x9E96A0;
        CHECK(!target_entered,"virtual method is not executed or counted");
        CHECK(r.runtime_heap.allocation_calls==8 && !r.runtime_heap.free_calls && !r.runtime_heap.failed_allocations &&
              r.runtime_heap.live_allocations==8 && r.runtime_heap.requested_bytes==176 && r.runtime_heap.live_bytes==256 &&
              r.runtime_heap.peak_live_bytes==256 && r.runtime_heap.backed_bytes==65536 && r.runtime_heap.arena_count==1 &&
              r.runtime_last_malloc_address==DS_HEAP_BASE+240 && r.runtime_last_malloc_size==8,"facet and cleanup node share the existing budgeted arena");
        CHECK(image.allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000+65536 && image.segment_count==7 &&
              ds_image_address(&image,DS_HEAP_BASE,65536,DS_PF_R|DS_PF_W) &&
              !ds_image_address(&image,DS_HEAP_BASE,1,DS_PF_X),"backing and executable permissions remain bounded");
        static constexpr uint32_t tail[]={0x1872C50,1,0x176E82A,0x176EFF4,0x176F1F6,0,0,0,0,DS_HEAP_BASE+208,0,0};
        for(unsigned offset=0;offset<256;offset+=4){
            const uint32_t expected=offset>=208?tail[(offset-208)/4]:offset==24 || offset==152?2u:
                offset==48 || offset==176 || offset==192?DS_HEAP_BASE+64:
                offset==64?0x18C7DE0u:offset==68?5u:offset==80?0x3Fu:offset==92?0x43000000u:
                offset==108?1u:offset==112?15u:0u;
            uint32_t v=0;CHECK(ds_image_read_be32(&image,DS_HEAP_BASE+offset,&v) && v==expected,
                "complete heap bytes include the facet tables, cleanup link, existing locale and rounded padding");
        }
        static constexpr uint32_t fields[][2]={{0x1B50430,DS_HEAP_BASE+208},{0x1CE6720,DS_HEAP_BASE+240},
            {0x1AD5630,78},{0x1CE58E4,0x19AC6D0},{0x1CE58E8,0x1967920},
            {0xE000430,DS_HEAP_BASE+64},{0xFEFFB04,0},{0xFEFFB70,0},{0xFEFFB74,15},{0xFEFFB8C,0},{0xFEFFB90,15}};
        for(const auto &f:fields){uint32_t v=0;CHECK(ds_image_read_be32(&image,f[0],&v) && v==f[1],"original locale guard teardown, cache and cleanup publication are preserved");}
        CHECK(r.mutex_count==14 && r.thread_id_calls==14 && r.lock_calls==25 && r.unlock_calls==25 && !r.native_held_at_stop &&
              r.registry_entries==10 && r.callback_entries==40 && r.kernel_mutex_calls==5,"all original guards release normally alongside earlier metadata");
        DsEntryResult replay={};char message[256]={};
        CHECK(!ds_entry_run_first_facet(&image,&replay,message,sizeof(message)) && replay.stop==DS_ENTRY_BAD_INPUT,"in-place replay is refused");
        for(unsigned attempt=0;attempt<2;++attempt){DsImage fresh={};DsEntryResult again={};
            CHECK(ds_image_load_file(&fresh,argv[1],48u*1024u*1024u,message,sizeof(message)),"fresh image loads");
            CHECK(ds_entry_run_first_facet(&fresh,&again,message,sizeof(message)) && again.stop==DS_ENTRY_FIRST_FACET_READY &&
                  again.function_count==799 && again.trace_count==13238 && !again.native_live_after_cleanup &&
                  !again.runtime_heap_metadata_after_cleanup,"fresh execution recreates facet ownership and locale state");
            ds_image_destroy(&fresh);CHECK(!fresh.allocated_bytes,"fresh backing is released");}
        struct Prior{bool (*run)(DsImage*,DsEntryResult*,char*,size_t);uint32_t functions,accesses;DsEntryStop stop;};
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
            {ds_entry_run_locale_object,669,12314,DS_ENTRY_LOCALE_OBJECT_READY},
            {ds_entry_run_locale_published,697,12492,DS_ENTRY_LOCALE_PUBLISHED},
            {ds_entry_run_stream_locale,747,12849,DS_ENTRY_STREAM_LOCALE_READY}};
        for(const auto &p:prior){DsImage old={};DsEntryResult result={};
            CHECK(ds_image_load_file(&old,argv[1],48u*1024u*1024u,message,sizeof(message)),"prior checkpoint image loads");
            CHECK(p.run(&old,&result,message,sizeof(message)) && result.stop==p.stop && result.function_count==p.functions &&
                  result.trace_count==p.accesses && !result.native_live_after_cleanup && !result.runtime_heap_metadata_after_cleanup,
                  "all twelve prior metadata/runtime checkpoint APIs keep exact counts and cleanup");
            ds_image_destroy(&old);CHECK(!old.allocated_bytes,"prior backing is released");}
    }else{
        CHECK(!ok && r.constructors_completed==12,"fault cannot claim first-facet success");
        if(std::strcmp(argv[2],"--explore-first-facet-state-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(error,"0x600000D4") && !r.native_held_at_stop,"corrupt facet references are identified after guards release");
        else if(std::strcmp(argv[2],"--explore-first-facet-node-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(error,"0x600000F0") && !r.native_held_at_stop,"corrupt cleanup linkage is identified");
        else if(std::strcmp(argv[2],"--explore-first-facet-snapshot-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(error,"0x0FEFFB08") && !r.native_held_at_stop,"corrupt saved locale snapshot is identified");
        else if(std::strcmp(argv[2],"--explore-first-facet-seventh-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.r3==25 && r.runtime_heap.allocation_calls==6 && r.native_held_at_stop==1,"wrong facet size is rejected before allocation");
        else if(std::strcmp(argv[2],"--explore-first-facet-eighth-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.r3==16 && r.runtime_heap.allocation_calls==7 && r.native_held_at_stop==1,"wrong cleanup-node size is rejected before allocation");
        else if(std::strcmp(argv[2],"--explore-first-facet-descriptor-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.runtime_heap.allocation_calls==8 && !r.native_held_at_stop,"corrupt virtual descriptor TOC is rejected");
        else if(std::strcmp(argv[2],"--explore-first-facet-target-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.callee==0x9E9648 && r.runtime_heap.allocation_calls==8 && !r.native_held_at_stop,"different executable virtual target is refused before dispatch");
        else if(std::strcmp(argv[2],"--explore-first-facet-compare-fault")==0)
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && !r.fault_write && r.fault_address==0x70000000 && r.runtime_heap.allocation_calls==7 && r.native_held_at_stop==1,"original comparison checks source backing with a locale guard held");
        else if(std::strcmp(argv[2],"--explore-first-facet-lock-fault")==0)
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && r.fault_write && !r.fault_address && r.runtime_heap.allocation_calls==7 && r.native_held_at_stop==1,"checked fault under recursive wrapper ownership cleans the actual native lock");
        else CHECK(false,"known test mode");
    }
    CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup &&
          !r.runtime_heap_metadata_after_cleanup,"all native registries and heap metadata clear on stop and failure");
    ds_image_destroy(&image);CHECK(!image.allocated_bytes && !image.segment_count,"image destruction releases all backing");
    std::printf("First facet tests: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
