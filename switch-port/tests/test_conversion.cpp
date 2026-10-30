#include "ppu_recomp.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static constexpr uint64_t base=0x30000,toc=0x19D3BAC,src=base+0x800,out=base+0xA00,state=base+0xC00,table=base+0x310;
static std::array<uint8_t,0x4000> mem;
static unsigned checks,failures,entries;
#define CHECK(c,what) do { ++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %s (%d)\n",what,__LINE__);} } while(0)
void func_0166C0E8(ppu_context*);void func_00B17510(ppu_context*);
extern "C" { PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*)=nullptr; }
static uint8_t *address(uint64_t a,unsigned n){if(a<base || a-base>mem.size() || n>mem.size()-(a-base))std::abort();return mem.data()+a-base;}
extern "C" void ds_entry_trace_function(unsigned int a){if(a!=0x166C0E8 && a!=0x1664E60 && a!=0xB17510 && a!=0x10C50)std::abort();++entries;}
extern "C" uint8_t vm_read8(uint64_t a){return *address(a,1);}
extern "C" uint16_t vm_read16(uint64_t a){return uint16_t(vm_read8(a))<<8|vm_read8(a+1);}
extern "C" uint32_t vm_read32(uint64_t a){if(a==toc+0x4828)return base+0x1800;return uint32_t(vm_read16(a))<<16|vm_read16(a+2);}
extern "C" uint64_t vm_read64(uint64_t a){return uint64_t(vm_read32(a))<<32|vm_read32(a+4);}
extern "C" void vm_write8(uint64_t a,uint8_t v){*address(a,1)=v;}
extern "C" void vm_write16(uint64_t a,uint16_t v){vm_write8(a,v>>8);vm_write8(a+1,v);}
extern "C" void vm_write32(uint64_t a,uint32_t v){vm_write16(a,v>>16);vm_write16(a+2,v);}
extern "C" void vm_write64(uint64_t a,uint64_t v){vm_write32(a,v>>32);vm_write32(a+4,v);}
static void reset(){mem.fill(0);vm_write32(out-4,0xA5A5BEEF);vm_write32(out,0xBEEFA5A5);}
static ppu_context context(uint64_t input,uint64_t length,uint64_t output=out){ppu_context c={};c.gpr[1]=base+0x3000;c.gpr[2]=toc;c.gpr[3]=output;c.gpr[4]=input;c.gpr[5]=length;c.gpr[6]=state;c.gpr[7]=table;c.gpr[13]=base+0x7000;c.lr=0x123456;c.gpr[28]=28;c.gpr[29]=29;c.gpr[30]=30;c.gpr[31]=31;return c;}
static void abi(const ppu_context &c){CHECK(c.gpr[1]==base+0x3000 && c.gpr[2]==toc && c.lr==0x123456 && c.gpr[28]==28 && c.gpr[29]==29 && c.gpr[30]==30 && c.gpr[31]==31,"original conversion preserves caller ABI");CHECK(vm_read16(out-2)==0xBEEF && vm_read16(out+2)==0xA5A5,"output canaries survive");}
static std::vector<uint8_t> encode(unsigned cp){if(cp<0x80)return {uint8_t(cp)};if(cp<0x800)return {uint8_t(0xC0|(cp>>6)),uint8_t(0x80|(cp&63))};return {uint8_t(0xE0|(cp>>12)),uint8_t(0x80|((cp>>6)&63)),uint8_t(0x80|(cp&63))};}
int main(){
 std::vector<unsigned> cps;for(unsigned cp=0;cp<128;++cp)cps.push_back(cp);
 for(unsigned cp:{0x80u,0x7FFu,0x800u,0x1234u,0x20ACu,0xD7FFu,0xE000u,0xFFFDu,0xFFFFu})cps.push_back(cp);
 uint32_t random=0x12345678;for(unsigned i=0;i<64;++i){random=random*1664525+1013904223;unsigned cp=random&0xFFFF;if(cp>=0xD800 && cp<=0xDFFF)cp=0xE000;cps.push_back(cp);}
 for(unsigned cp:cps)for(unsigned alignment=0;alignment<4;++alignment){
  const auto bytes=encode(cp);reset();for(unsigned i=0;i<bytes.size();++i)vm_write8(src+alignment+i,bytes[i]);
  auto c=context(src+alignment,bytes.size());func_0166C0E8(&c);
  CHECK(int32_t(c.gpr[3])==int32_t(cp?bytes.size():0) && vm_read16(out)==cp,"complete single-byte and multibyte conversion matches code point");abi(c);
  for(unsigned split=1;split<bytes.size();++split){reset();for(unsigned i=0;i<bytes.size();++i)vm_write8(src+alignment+i,bytes[i]);
   c=context(src+alignment,split);func_0166C0E8(&c);CHECK(int32_t(c.gpr[3])==-2 && vm_read16(out)==0xBEEF,"incomplete sequence records state without output");abi(c);
   c=context(src+alignment+split,bytes.size()-split);func_0166C0E8(&c);CHECK(int32_t(c.gpr[3])==int32_t(bytes.size()-split) && vm_read16(out)==cp,"split sequence resumes from original conversion state");abi(c);}
  reset();for(unsigned i=0;i<bytes.size();++i)vm_write8(src+i,bytes[i]);c=context(src,bytes.size(),0);func_0166C0E8(&c);CHECK(int32_t(c.gpr[3])==int32_t(cp?bytes.size():0) && vm_read16(out)==0xBEEF,"null destination measures conversion without output");abi(c);
 }
 const std::vector<std::vector<uint8_t>> invalid={{0x80},{0xFF},{0xF0,0x90,0x80,0x80},{0xC2,0x20},{0xE2,0x82,0x20}};
 for(const auto &bytes:invalid){reset();for(unsigned i=0;i<bytes.size();++i)vm_write8(src+i,bytes[i]);auto c=context(src,bytes.size());func_0166C0E8(&c);CHECK(int32_t(c.gpr[3])==-1 && vm_read16(out)==0xBEEF,"unsupported or malformed sequence returns failure without output");CHECK(vm_read32(c.gpr[13]-28092)==0x8001001D,"original TLS errno helper publishes conversion error");abi(c);}
 reset();auto c=context(0x70000000,0);func_0166C0E8(&c);CHECK(int32_t(c.gpr[3])==-2 && !vm_read64(state),"zero length never reads input and leaves initial state");abi(c);
 reset();vm_write64(state,0x12345678);c=context(0,0);func_0166C0E8(&c);CHECK(!c.gpr[3] && !vm_read64(state),"null source resets state from original default template");abi(c);
 for(unsigned ch=1;ch<128;++ch){reset();vm_write32(base+0x1018,table);c=context(src,1);c.gpr[2]=0x19B5040;c.gpr[3]=base+0x1000;c.gpr[4]=ch;func_00B17510(&c);CHECK(c.gpr[3]==ch && c.gpr[1]==base+0x3000 && c.gpr[2]==0x19B5040 && c.lr==0x123456 && !g_trampoline_fn,"wide virtual wrapper and full original thunk return character with restored TOC and stack");}
 CHECK(entries>checks/4,"original conversion bodies actually executed");
 std::printf("Original conversion tests: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
