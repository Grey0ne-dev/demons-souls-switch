#include "ppu_recomp.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void func_01668E50(ppu_context *);
static constexpr uint64_t base=0x30000,toc=0x19D3BAC;
static std::array<uint8_t,8192> memory;
static unsigned entries,checks,failures;
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
static uint8_t *address(uint64_t a,unsigned n)
{
    if(a<base || a-base>memory.size() || n>memory.size()-(a-base))std::abort();
    return memory.data()+(a-base);
}
extern "C" void ds_entry_trace_function(unsigned int a){if(a!=0x1668E50)std::abort();++entries;}
extern "C" uint8_t vm_read8(uint64_t a){return *address(a,1);}
extern "C" uint32_t vm_read32(uint64_t a){auto p=address(a,4);return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
extern "C" uint64_t vm_read64(uint64_t a){if(a==toc+0x47AC)return UINT64_C(0x7F7F7F7F7F7F7F7F);return uint64_t(vm_read32(a))<<32|vm_read32(a+4);}
int main()
{
    static constexpr unsigned lengths[]={0,1,2,3,4,7,8,9,15,16,31,32,63,128,255,256};
    for(unsigned n:lengths)for(unsigned left=0;left<8;++left)for(unsigned right=0;right<8;++right)for(unsigned pattern=0;pattern<8;++pattern){
        memory.fill(0xA5);const unsigned a=128+left,b=4096+right;
        for(unsigned i=0;i<n;++i)memory[a+i]=memory[b+i]=uint8_t(1+(i*37u)%255u);
        memory[a+n]=memory[b+n]=0;
        if(n){
            if(pattern==1){memory[a]=0x7F;memory[b]=0x80;}
            if(pattern==2){memory[a]=0xFF;memory[b]=0x80;}
            if(pattern==3){memory[a+n-1]=1;memory[b+n-1]=2;}
            if(pattern==4)memory[a+n-1]=0;
            if(pattern==5)memory[b+n/2]=0;
            if(pattern==6)memory[a+n/2]=memory[b+n/2]=0x80;
            if(pattern==7){memory[a+n-1]=0xFF;memory[b+n-1]=0x7F;}
        }
        const auto original_memory=memory;
        const int oracle=std::strcmp(reinterpret_cast<const char*>(memory.data()+a),reinterpret_cast<const char*>(memory.data()+b));
        const int expected=(oracle>0)-(oracle<0);
        ppu_context ctx={};for(unsigned i=0;i<32;++i)ctx.gpr[i]=0x9876000000000000ull+i;
        ctx.gpr[2]=toc;ctx.gpr[3]=base+a;ctx.gpr[4]=base+b;ctx.lr=0x123456;
        const ppu_context original=ctx;const unsigned before=entries;
        func_01668E50(&ctx);
        if(ctx.gpr[3]!=uint64_t(int64_t(expected)))std::printf("MISMATCH length=%u left=%u right=%u pattern=%u expected=%d actual=%llx\n",n,left,right,pattern,expected,(unsigned long long)ctx.gpr[3]);
        CHECK(ctx.gpr[3]==uint64_t(int64_t(expected)),"original comparator agrees with unsigned-byte host ordering and sign-extends its return");
        CHECK(memory==original_memory && ctx.lr==original.lr && ctx.gpr[1]==original.gpr[1] && ctx.gpr[2]==toc &&
              std::memcmp(ctx.gpr+14,original.gpr+14,18*sizeof(uint64_t))==0 && entries==before+1,
              "comparison preserves all guest bytes, caller stack, TOC, LR and nonvolatile registers");
    }
    std::printf("Original comparison tests: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
