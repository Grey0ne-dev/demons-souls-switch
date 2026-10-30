#include "ds_entry.h"
#include "ds_reference.h"
#include "ppu_recomp.h"
#include <cstdio>
#include <cstring>
static unsigned checks,failures,allocation_calls,fault_allocation;
static char fault_kind[48]={};
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
extern void func_0168ABD0(ppu_context*);
void ds_test_wide_fault(ppu_context *ctx,unsigned stage)
{
    if(stage==0){
        ++allocation_calls;if(allocation_calls!=fault_allocation)return;
        if(!std::strcmp(fault_kind,"size"))ctx->gpr[3]+=1;
        if(!std::strcmp(fault_kind,"caller"))ctx->lr+=4;
        if(!std::strcmp(fault_kind,"stack"))ctx->gpr[1]-=16;
        if(!std::strcmp(fault_kind,"toc"))ctx->gpr[2]=0x19B5040;
        if(!std::strcmp(fault_kind,"reorder")){ctx->gpr[3]=4;ctx->lr=0x168BD6C;ctx->gpr[1]=0xFEFFCA0;}
    }else if(stage==1 && allocation_calls==25 && ctx->ctr==0x745248){
        if(!std::strcmp(fault_kind,"registry"))vm_write32(0x1CE64B4,0);
        if(!std::strcmp(fault_kind,"references"))vm_write32(0x1CE6490,0);
        if(!std::strcmp(fault_kind,"fill"))vm_write32(0x1CE6980,0);
        if(!std::strcmp(fault_kind,"handle"))vm_write32(0x1CE6974,0x60000280);
        if(!std::strcmp(fault_kind,"buffer"))vm_write32(0x1CE6834,0x600001B0);
        if(!std::strcmp(fault_kind,"cleanup"))vm_write32(0x60000260,0);
        if(!std::strcmp(fault_kind,"counter"))vm_write32(0x1AD61D4,0);
        if(!std::strcmp(fault_kind,"callback"))vm_write32(0x1AD5630,77);
        if(!std::strcmp(fault_kind,"next-target"))ctx->ctr=0x3F46B0;
        if(!std::strcmp(fault_kind,"extra")){ctx->gpr[2]=0x19D3BAC;ctx->gpr[3]=4;ctx->lr=0x1685FEC;ctx->gpr[1]=0xFEFFAB0;func_0168ABD0(ctx);}
    }else if(stage==2 && allocation_calls==22 && !std::strcmp(fault_kind,"lock"))vm_write32(0,1);
    else if(stage==3){
        if(!std::strcmp(fault_kind,"virtual-target")){ctx->ctr=0x9E96A0;ctx->gpr[10]=0x1970348;}
        if(!std::strcmp(fault_kind,"virtual-argument"))ctx->gpr[4]=33;
        if(!std::strcmp(fault_kind,"virtual-stack"))ctx->gpr[1]-=16;
        if(!std::strcmp(fault_kind,"virtual-toc"))vm_write32(0x1967FD4,0x19D3BAC);
    }else if(stage==4 && !std::strcmp(fault_kind,"conversion-pointer"))ctx->gpr[7]=0x70000000;
    else if(stage==5 && allocation_calls==18 && !std::strcmp(fault_kind,"native-last"))vm_write32(0,1);
    else if(stage==6 && !std::strcmp(fault_kind,"conversion-result"))ctx->gpr[3]=0;
}
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    unsigned parsed=0;char kind[48]={};
    if(std::sscanf(argv[2],"--explore-wide-streams-alloc-%u-%47s",&parsed,kind)==2){
        fault_allocation=parsed;
    }else if(std::sscanf(argv[2],"--explore-wide-streams-%47s",kind)==1){
        if(!std::strcmp(kind,"reorder-fault"))fault_allocation=14;
    }
    char *suffix=std::strstr(kind,"-fault");
    if(suffix && !suffix[6]){*suffix='\0';std::snprintf(fault_kind,sizeof(fault_kind),"%s",kind);}
    const bool normal=!std::strcmp(argv[2],"--explore-wide-streams");
    const bool probe=!std::strcmp(argv[2],"--explore-wide-streams-probe");
    DsImage image={};DsEntryResult r={};char error[256]={};
    CHECK(ds_image_load_file(&image,argv[1],48u*1024u*1024u,error,sizeof(error)),"verified image loads");
    const bool ok=ds_entry_run_wide_streams(&image,&r,error,sizeof(error));
    std::printf("WIDE ok=%d stop=%d callee=%08x functions=%u accesses=%u completed=%u r3=%llx r4=%llx r5=%llx r6=%llx lr=%llx sp=%llx toc=%llx mutexes=%u thread_ids=%u locks=%u/%u held=%u allocations=%u error=%s\n",ok,r.stop,r.callee,r.function_count,r.trace_count,r.constructors_completed,(unsigned long long)r.r3,(unsigned long long)r.r4,(unsigned long long)r.r5,(unsigned long long)r.r6,(unsigned long long)r.lr,(unsigned long long)r.stack_pointer,(unsigned long long)r.toc,r.mutex_count,r.thread_id_calls,r.lock_calls,r.unlock_calls,r.native_held_at_stop,r.runtime_heap.allocation_calls,error);
    for(unsigned i=0;i<r.function_count && i<DS_ENTRY_FUNCTION_CAPACITY;++i)std::printf("%u: %08x\n",i,r.functions[i]);
    if(normal || probe){
        CHECK(ok && !error[0] && r.stop==DS_ENTRY_WIDE_STREAMS_READY && r.function_count==1265 && r.trace_count==16590 && r.constructors_completed==13,"wide initialization and constructor thirteen reach the exact pre-fourteenth checkpoint");
        CHECK(r.callee==0x745248 && !r.r3 && !r.r4 && r.r5==1 && r.r6==0xFEFFB24 && r.lr==0x16B84F4 && r.stack_pointer==0xFEFFE00 && r.toc==0x19B5040,"exact boundary ABI");
        CHECK(r.runtime_heap.allocation_calls==25 && r.runtime_heap.live_allocations==25 && !r.runtime_heap.free_calls && !r.runtime_heap.failed_allocations && r.runtime_heap.requested_bytes==420 && r.runtime_heap.live_bytes==672 && r.runtime_heap.peak_live_bytes==672 && r.runtime_heap.backed_bytes==65536 && r.runtime_heap.arena_count==1 && r.runtime_last_malloc_address==0x60000290 && r.runtime_last_malloc_size==4,"all twenty-five allocations retain bounded arena ownership");
        CHECK(r.mutex_count==18 && r.mutex_addresses[17]==0x60000200 && r.thread_id_calls==58 && r.lock_calls==66 && r.unlock_calls==66 && !r.native_held_at_stop,"all six runtime mutexes and normal unlocks are real");
        unsigned virtuals=0,registrations=0,mallocs=0,wide=0;
        for(unsigned i=0;i<r.function_count;++i){virtuals+=r.functions[i]==0xB17510;registrations+=r.functions[i]==0x1684D70;mallocs+=r.functions[i]==0x167AC88;wide+=r.functions[i]==0x745248;}
        CHECK(virtuals==4 && registrations==8 && mallocs==25 && !wide,"four wide virtual calls and eight registrations; constructor fourteen never enters");
        CHECK(image.allocated_bytes==DS_REF_ALLOCATED_BYTES+0x11A000+65536 && image.segment_count==7 && !ds_image_address(&image,DS_HEAP_BASE,1,DS_PF_X),"backing budget and non-executable guest heap preserved");
        // Dump for the independent source/ELF state model, including padding.
        for(uint32_t a=0x1CE6488;a<0x1CE6738;a+=4){uint32_t v=0;CHECK(ds_image_read_be32(&image,a,&v),"state readable");std::printf("NARROW %08x %08x\n",a,v);}
        for(uint32_t a=0x60000000;a<0x600002A0;a+=4){uint32_t v=0;CHECK(ds_image_read_be32(&image,a,&v),"heap readable");std::printf("HEAP %08x %08x\n",a,v);}
        for(uint32_t a=0x1CE6750;a<0x1CE69A0;a+=4){uint32_t v=0;CHECK(ds_image_read_be32(&image,a,&v),"wide state readable");std::printf("WSTATE %08x %08x\n",a,v);}
        for(uint32_t a=0x1AD61B0;a<0x1AD6220;a+=4){uint32_t v=0;ds_image_read_be32(&image,a,&v);std::printf("GLOBAL %08x %08x\n",a,v);}
        for(uint32_t a=0xE000300;a<0xE0003E0;a+=4){uint32_t v=0;ds_image_read_be32(&image,a,&v);std::printf("TLS %08x %08x\n",a,v);}
        static constexpr uint32_t meta[]={0x1AD5630u,0x1AD6088u,0x1B93E84u,0x1CE6720u,0x1CE6734u,0x1CE58E4u,0x1CE58E0u,0x1CE6994u};
        for(uint32_t a:meta){uint32_t v=0;ds_image_read_be32(&image,a,&v);std::printf("META %08x %08x\n",a,v);}
        DsEntryResult replay={};char message[256]={};
        CHECK(!ds_entry_run_wide_streams(&image,&replay,message,sizeof(message)) && replay.stop==DS_ENTRY_BAD_INPUT,"in-place replay rejected");
        for(unsigned attempt=0;attempt<2;++attempt){DsImage fresh={};DsEntryResult again={};
            CHECK(ds_image_load_file(&fresh,argv[1],48u*1024u*1024u,message,sizeof(message)),"fresh image loads");
            CHECK(ds_entry_run_wide_streams(&fresh,&again,message,sizeof(message)) && again.function_count==1265 && again.trace_count==16590 && !again.native_live_after_cleanup && !again.runtime_heap_metadata_after_cleanup,"fresh execution recreates all ownership");
            ds_image_destroy(&fresh);CHECK(!fresh.allocated_bytes,"fresh backing released");}
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
            {ds_entry_run_stream_locale,747,12849,DS_ENTRY_STREAM_LOCALE_READY},
            {ds_entry_run_first_facet,799,13238,DS_ENTRY_FIRST_FACET_READY},
            {ds_entry_run_first_stream,810,13312,DS_ENTRY_FIRST_STREAM_READY},
            {ds_entry_run_narrow_streams,966,14378,DS_ENTRY_NARROW_STREAMS_READY}};
        for(const auto &p:prior){DsImage old={};DsEntryResult result={};
            CHECK(ds_image_load_file(&old,argv[1],48u*1024u*1024u,message,sizeof(message)),"prior image loads");
            CHECK(p.run(&old,&result,message,sizeof(message)) && result.stop==p.stop && result.function_count==p.functions && result.trace_count==p.accesses && !result.native_live_after_cleanup && !result.runtime_heap_metadata_after_cleanup,"fifteen prior checkpoint APIs preserve exact stops and cleanup");
            ds_image_destroy(&old);CHECK(!old.allocated_bytes,"prior backing released");}
    }else{
        CHECK(!ok && r.constructors_completed<=13,"fault cannot claim checkpoint success");
        const bool lock=!std::strcmp(fault_kind,"lock");
        const bool memory=lock || !std::strcmp(fault_kind,"conversion-pointer") || !std::strcmp(fault_kind,"native-last");
        CHECK(r.stop==(memory?DS_ENTRY_MEMORY_FAULT:DS_ENTRY_ABI_MISMATCH),"fault rejected explicitly");
        const bool virtual_fault=!std::strncmp(fault_kind,"virtual-",8);
        const unsigned expected=fault_allocation?fault_allocation-1:lock || virtual_fault || !std::strcmp(fault_kind,"conversion-pointer")?22:!std::strcmp(fault_kind,"native-last")?18:25;
        CHECK(r.runtime_heap.allocation_calls==expected && r.runtime_heap.live_allocations==expected && (!lock || (r.fault_write && !r.fault_address && r.native_held_at_stop==1)),"invalid allocation never executes and native ownership is cleaned");
    }
    CHECK(!r.native_live_after_cleanup && !r.native_rwlocks_after_cleanup && !r.native_kernel_mutexes_after_cleanup && !r.runtime_heap_metadata_after_cleanup,"native resources and heap metadata clear on all exits");
    ds_image_destroy(&image);CHECK(!image.allocated_bytes && !image.segment_count,"all backing released");
    std::printf("Wide stream tests: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
