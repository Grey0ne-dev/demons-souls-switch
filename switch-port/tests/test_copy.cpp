#include "ppu_recomp.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void func_01666594(ppu_context *);
static constexpr uint64_t base=0x20000;
static std::array<uint8_t,16384> memory;
static unsigned entries,checks,failures;
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
static uint8_t *address(uint64_t a,unsigned n)
{
    if(a<base || a-base>memory.size() || n>memory.size()-(a-base))std::abort();
    return memory.data()+(a-base);
}
extern "C" void ds_entry_trace_function(unsigned int a)
{
    if(a!=0x1666594)std::abort();
    ++entries;
}
extern "C" uint8_t vm_read8(uint64_t a){return *address(a,1);}
extern "C" uint16_t vm_read16(uint64_t a){auto p=address(a,2);return uint16_t(p[0])<<8|p[1];}
extern "C" uint32_t vm_read32(uint64_t a){auto p=address(a,4);return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
extern "C" uint64_t vm_read64(uint64_t a){return uint64_t(vm_read32(a))<<32|vm_read32(a+4);}
extern "C" void vm_write8(uint64_t a,uint8_t v){*address(a,1)=v;}
extern "C" void vm_write16(uint64_t a,uint16_t v){auto p=address(a,2);p[0]=v>>8;p[1]=v;}
extern "C" void vm_write32(uint64_t a,uint32_t v){auto p=address(a,4);p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
extern "C" void vm_write64(uint64_t a,uint64_t v){vm_write32(a,v>>32);vm_write32(a+4,v);}
int main()
{
    static constexpr unsigned lengths[]={0,1,2,3,4,5,6,7,8,9,15,16,17,23,24,25,31,32,47,48,49,63,64,127,128,255,256,257,4095,4096};
    for(unsigned n:lengths)for(unsigned so=0;so<8;++so)for(unsigned dest=0;dest<8;++dest)for(unsigned reverse=0;reverse<2;++reverse){
        for(unsigned i=0;i<memory.size();++i)memory[i]=uint8_t(i*37u+(i>>3)*11u+19u);
        auto expected=memory;
        const unsigned source=(reverse?8192u:256u)+so;
        const unsigned target=(reverse?256u:8192u)+dest;
        std::memcpy(expected.data()+target,expected.data()+source,n);
        ppu_context ctx={};
        for(unsigned i=0;i<32;++i)ctx.gpr[i]=0x1234000000000000ull+i;
        ctx.gpr[3]=base+target;ctx.gpr[4]=base+source;ctx.gpr[5]=n;ctx.lr=0xABCDEF;
        const ppu_context original=ctx;
        const unsigned before=entries;
        func_01666594(&ctx);
        CHECK(memory==expected,"original copy matches host oracle and preserves every byte outside destination");
        CHECK(ctx.gpr[3]==base+target && ctx.lr==original.lr && ctx.gpr[1]==original.gpr[1] &&
              std::memcmp(ctx.gpr+14,original.gpr+14,18*sizeof(uint64_t))==0 && entries==before+1,
              "copy returns destination and preserves caller stack, LR and nonvolatile registers");
    }
    const auto original=memory;
    ppu_context empty={};empty.lr=0x1234;
    func_01666594(&empty);
    CHECK(memory==original && empty.gpr[3]==0 && empty.lr==0x1234,"zero-byte copy never dereferences null inputs");
    std::printf("Original copy tests: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
