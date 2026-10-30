#include "ds_entry.h"
#include "ds_reference.h"
#include <cstdio>
#include <cstring>

static unsigned checks,failures;
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    const bool normal=std::strcmp(argv[2],"--explore-stream-locale")==0;
    DsImage image={};DsEntryResult r={};char error[256]={};
    CHECK(ds_image_load_file(&image,argv[1],48u*1024u*1024u,error,sizeof(error)),"verified image loads");
    const bool ok=ds_entry_run_stream_locale(&image,&r,error,sizeof(error));
    std::printf("STREAM ok=%d stop=%d callee=%08x functions=%u accesses=%u completed=%u r3=%llx r4=%llx r5=%llx r6=%llx lr=%llx sp=%llx toc=%llx mutexes=%u thread_ids=%u locks=%u/%u held=%u error=%s\n",
        ok,r.stop,r.callee,r.function_count,r.trace_count,r.constructors_completed,
        (unsigned long long)r.r3,(unsigned long long)r.r4,(unsigned long long)r.r5,(unsigned long long)r.r6,
        (unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.toc,
        r.mutex_count,r.thread_id_calls,r.lock_calls,r.unlock_calls,r.native_held_at_stop,error);
    std::printf("BOUNDARY NID: %08x\n",r.boundary_nid);
    for(unsigned i=0;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i)std::printf("%u: %08x\n",i,r.functions[i]);
    if(normal) {
        CHECK(ok && r.stop==DS_ENTRY_STREAM_LOCALE_READY && !error[0] && r.constructors_completed==12 &&
              r.function_count==747 && r.trace_count==12849,"stream locale checkpoint is validated without claiming constructor return");
        CHECK(r.callee==0x168ABD0 && r.r3==24 && r.r4==1 && r.r5==0 && r.r6==0x1CE6564 &&
              r.lr==0x1688380 && r.stack_pointer==0xFEFFA90 && r.toc==0x19D3BAC,
              "facet request stops before operator-new entry with the original ABI");
        CHECK(r.runtime_heap.allocation_calls==6 && !r.runtime_heap.free_calls && !r.runtime_heap.failed_allocations &&
              r.runtime_heap.live_allocations==6 && r.runtime_heap.live_bytes==208 && r.runtime_heap.requested_bytes==144 &&
              r.runtime_heap.peak_live_bytes==208 && r.runtime_heap.backed_bytes==65536 && r.runtime_heap.arena_count==1 &&
              r.runtime_last_malloc_address==DS_HEAP_BASE+192 && r.runtime_last_malloc_size==4,
              "three additional actual allocations share existing backed memory");
        CHECK(image.allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000+65536 && image.segment_count==7 &&
              ds_image_address(&image,DS_HEAP_BASE,65536,DS_PF_R|DS_PF_W) &&
              !ds_image_address(&image,DS_HEAP_BASE,1,DS_PF_X),"heap backing remains budgeted RW and non-executable");
        for(unsigned offset=0;offset<208;offset+=4) {
            const uint32_t expected=offset==24 || offset==152?2u:
                offset==48 || offset==176 || offset==192?DS_HEAP_BASE+64:
                offset==64?0x18C7DE0u:offset==68?6u:offset==80?0x3Fu:
                offset==92?0x43000000u:offset==108?1u:offset==112?15u:0u;
            uint32_t v=0;
            CHECK(ds_image_read_be32(&image,DS_HEAP_BASE+offset,&v) && v==expected,
                  "complete heap contents include both real mutexes, three locale handles, C string and rounded padding");
        }
        static constexpr uint32_t fields[][2]={
            {0xE000430,DS_HEAP_BASE+64},{0x1CE6558,0x18C1B58},{0x1CE655C,DS_HEAP_BASE+128},
            {0x1CE6590,DS_HEAP_BASE+176},{0x1CE65D0,0x18C1CB0},{0x1CE65D8,0},{0x1CE65DC,0},
            {0x1CE65E0,0x201},{0x1CE65E4,6},{0x1CE65F4,DS_HEAP_BASE+192},{0x1CE65F8,0x1CE64E8},
            {0x1CE6724,1},{0x1CE6728,0},{0x1CE6734,1}
        };
        for(const auto &field:fields){uint32_t v=0;CHECK(ds_image_read_be32(&image,field[0],&v) && v==field[1],"original stream/base and facet lookup state matches its checked object");}
        CHECK(r.thread_id_calls==10 && r.lock_calls==23 && r.unlock_calls==22 && r.native_held_at_stop==1 &&
              r.mutex_count==14 && r.mutex_addresses[13]==DS_HEAP_BASE+144 &&
              r.kernel_mutex_calls==5 && r.registry_entries==10 && r.callback_entries==40,
              "fourteenth CRT mutex and held locale ownership are accounted alongside existing metadata");
        DsEntryResult replay={};char message[256]={};
        CHECK(!ds_entry_run_stream_locale(&image,&replay,message,sizeof(message)) && replay.stop==DS_ENTRY_BAD_INPUT,
              "in-place replay is refused");
        for(unsigned attempt=0;attempt<2;++attempt) {
            DsImage fresh={};DsEntryResult again={};
            CHECK(ds_image_load_file(&fresh,argv[1],48u*1024u*1024u,message,sizeof(message)),"fresh image loads");
            CHECK(ds_entry_run_stream_locale(&fresh,&again,message,sizeof(message)) && again.stop==DS_ENTRY_STREAM_LOCALE_READY &&
                  again.function_count==747 && again.trace_count==12849 && !again.native_live_after_cleanup &&
                  !again.runtime_heap_metadata_after_cleanup,"fresh repetition recreates native ownership after held-lock cleanup");
            ds_image_destroy(&fresh);CHECK(!fresh.allocated_bytes,"fresh backing is released");
        }
        DsImage prior={};DsEntryResult before={};
        CHECK(ds_image_load_file(&prior,argv[1],48u*1024u*1024u,message,sizeof(message)),"v0.22 image loads");
        CHECK(ds_entry_run_locale_published(&prior,&before,message,sizeof(message)) && before.stop==DS_ENTRY_LOCALE_PUBLISHED &&
              before.function_count==697 && before.trace_count==12492 && before.runtime_heap.allocation_calls==3 &&
              !before.native_held_at_stop,"v0.22 stops before the fourth allocation without changed counts");
        ds_image_destroy(&prior);CHECK(!prior.allocated_bytes,"v0.22 image backing releases");
    } else {
        CHECK(!ok && r.constructors_completed==12,"fault cannot claim stream locale success");
        if(std::strcmp(argv[2],"--explore-stream-locale-state-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(error,"0x60000044") && r.native_held_at_stop==1,
                  "corrupted final locale refcount is identified with guard held");
        else if(std::strcmp(argv[2],"--explore-stream-locale-handle-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && std::strstr(error,"0x600000B0") && r.native_held_at_stop==1,
                  "corrupted stream handle is identified with guard held");
        else if(std::strcmp(argv[2],"--explore-stream-locale-fourth-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.callee==0x168ABD0 && r.r3==41 &&
                  r.runtime_heap.allocation_calls==3 && !r.native_held_at_stop,"wrong fourth request is refused before allocation");
        else if(std::strcmp(argv[2],"--explore-stream-locale-fifth-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.callee==0x168ABD0 && r.r3==8 &&
                  r.runtime_heap.allocation_calls==4 && r.mutex_count==14 && !r.native_held_at_stop,"wrong fifth request is refused after real mutex creation");
        else if(std::strcmp(argv[2],"--explore-stream-locale-sixth-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.callee==0x168ABD0 && r.r3==8 &&
                  r.runtime_heap.allocation_calls==5 && !r.native_held_at_stop,"wrong sixth request is refused without another handle");
        else if(std::strcmp(argv[2],"--explore-stream-locale-stack-fault")==0)
            CHECK(r.stop==DS_ENTRY_ABI_MISMATCH && r.callee==0x168ABD0 && r.stack_pointer==0xFEFFB50 &&
                  r.runtime_heap.allocation_calls==3 && !r.native_held_at_stop,"wrong caller stack is refused before allocation");
        else if(std::strcmp(argv[2],"--explore-stream-locale-native-fault")==0)
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && r.fault_write && !r.fault_address &&
                  r.runtime_heap.allocation_calls==4 && r.mutex_count==14 && !r.native_held_at_stop,"fault after actual fourteenth mutex creation releases every native resource");
        else if(std::strcmp(argv[2],"--explore-stream-locale-tls-fault")==0)
            CHECK(r.stop==DS_ENTRY_MEMORY_FAULT && !r.fault_write && r.fault_address==0x70000004 &&
                  r.runtime_heap.allocation_calls==5 && r.native_held_at_stop==1,"stream consumer follows real TLS pointer and rejects missing backing");
        else CHECK(false,"known test mode");
    }
    CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup &&
          !r.runtime_heap_metadata_after_cleanup,"all native registries and heap metadata clear on stop and failure");
    ds_image_destroy(&image);CHECK(!image.allocated_bytes && !image.segment_count,"image destruction releases all backing");
    std::printf("Stream locale tests: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
