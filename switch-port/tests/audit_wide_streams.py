#!/usr/bin/env python3
"""Reconstruct live wide startup state from original ELF constants and stores."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
ROOT=Path(__file__).resolve().parents[2]
HEADER=ROOT/'switch-port/entry/ds_wide_reference.h'
OLD=ROOT/'analysis/device-narrow-streams-v0260'

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--probe',type=Path,required=True);p.add_argument('--write-reference',action='store_true');p.add_argument('--audit',type=Path);args=p.parse_args()
 b=(ROOT/'upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF').read_bytes();assert hashlib.sha256(b).hexdigest()=='90eaa5e9e48b4874292badc311ccea253e6a622ff188bfbdbe76e6f6821269d8'
 o=struct.unpack_from('>Q',b,32)[0];z,n=struct.unpack_from('>HH',b,54);ss=[struct.unpack_from('>IIQQQQQQ',b,o+i*z) for i in range(n)]
 def w(a):
  for k,f,off,v,_,fs,ms,_ in ss:
   if k==1 and v<=a and a+4<=v+fs:return struct.unpack_from('>I',b,off+a-v)[0]
   if k==1 and v+fs<=a and a+4<=v+ms:return 0
  raise ValueError(hex(a))
 def original(name):
  h=OLD/'source-snapshot/switch-port/entry/ds_narrow_reference.h';s=h.read_text()
  seal=json.loads((OLD/'build-verification.json').read_text());assert hashlib.sha256(h.read_bytes()).hexdigest()==seal['source_sha256']['switch-port/entry/ds_narrow_reference.h']
  a=re.search(r'\b'+name+r'\[\]=\{(.*?)\};',s,re.S)[1];return [int(x,16) for x in re.findall(r'0x([A-F0-9]+)u',a)]
 toc=0x19D3BAC;registry=original('kNarrowRegistry');heap=original('kNarrowHeap')+[0]*((0x2A0-0x170)//4);narrow=original('kNarrowAggregate');wide=[0]*(0x234//4)
 for offset,mutex,handle,metadata in [(0,0x170,0x1A0,0x4D2C),(0x70,0x1B0,0x1E0,0x4D30),(0xE0,0x1F0,0x220,0x4D34)]:
  base=0x1CE6750+offset
  values={0:w(toc+0x4D28),4:0x60000000+mutex,0x10:base+8,0x14:base+12,0x20:base+0x18,0x24:base+0x1C,0x30:base+0x28,0x34:base+0x2C,0x38:0x60000000+handle,0x50:base+0x40,0x54:base+0x48,0x60:w(w(toc+0x4CC8)),0x64:w(w(toc+0x4CC8)+4),0x6C:w(toc+metadata)}
  for k,v in values.items():wide[(offset+k)//4]=v
 streams=[(0x158,0x150,0x4CFC,0x230,0),(0x190,0x18C,0x4D00,0x270,0x70),(0x1C8,0x1C4,0x4D00,0x280,0xE0),(0x200,0x1FC,0x4D00,0x290,0xE0)]
 for i,(offset,prefix,vt,handle,buffer) in enumerate(streams,5):
  wide[prefix//4]=w(toc+vt)
  values={0:w(toc+vt)+0x14,4:i,0x10:0x203 if i==7 else 0x201,0x14:6,0x24:0x60000000+handle,0x28:0x1CE6750+buffer,0x2C:0x1CE68DC if i in (5,7,8) else 0,0x30:0x00200000}
  for k,v in values.items():wide[(offset+k)//4]=v
  registry[(0xC+i*4)//4]=0x1CE6750+offset
 registry[0]=0x00010101;registry[1]=0x01010101;registry[2]=0x01000000
 heap[0x44//4]=16
 for a in (0x188,0x1C8,0x208):heap[a//4]=2
 for a in (0x1A0,0x1E0,0x220,0x230,0x270,0x280,0x290):heap[a//4]=0x60000040
 vals={0x240:w(toc+0x4CE4),0x244:1,0x248:w(w(toc+0x4D10)),0x24C:w(w(toc+0x4D14)),0x250:w(w(toc+0x4D18)),0x258:0xE007000-27888,0x25C:0xE007000-27700,0x260:0x600000F0,0x264:0x60000240}
 for a,v in vals.items():heap[a//4]=v
 globals_=[1 if (a-0x1AD61B0)%8==4 and a<=0x1AD61D4 else w(a) for a in range(0x1AD61B0,0x1AD6220,4)]
 for a in range(0x1AD61B0,0x1AD61D8,8):assert w(a)==w(a+4)==0
 text=args.probe.read_text();checks=0
 for label,start,values in [('NARROW',0x1CE6488,registry),('NARROW',0x1CE64E8,narrow),('WSTATE',0x1CE6750,wide),('HEAP',0x60000000,heap),('GLOBAL',0x1AD61B0,globals_),('TLS',0xE000300,[0]*(0xE0//4))]:
  actual={int(a,16):int(v,16) for a,v in re.findall(r'^'+label+r' ([a-f0-9]+) ([a-f0-9]+)$',text,re.M)}
  for i,value in enumerate(values):
   assert actual.get(start+4*i)==value,(hex(start+4*i),actual.get(start+4*i),value)
   checks+=1
 meta={0x1AD5630:78,0x1AD6088:1,0x1B93E84:0x60000240,0x1CE6720:0x60000260,0x1CE6734:2,0x1CE58E4:0x19AC6D0,0x1CE58E0:w(0x1CE58E0),0x1CE6994:2}
 observed_meta={int(a,16):int(v,16) for a,v in re.findall(r'^META ([a-f0-9]+) ([a-f0-9]+)$',text,re.M)}
 assert observed_meta==meta,(observed_meta,meta)
 checks+=len(meta)
 sites={0x168E68C:0x4E800421,0x168E690:0xE8410028,0x1967FD0:0xB17510,0x1967FD4:0x19B5040,0x1875318:0x1967FD0,
        0x10C50:0xF8410028,0x10C54:0x3C420002,0x10C58:0x3842EB6C,0x10C5C:0x4965B48C,0x168BD64:0x38600004,0x168BE30:0x38600004,0x168BEF4:0x38600004,0x168E784:0x38600020,0x16884E8:0x38600008,0x16A0510:0x38600028,0x1685FD8:0x38600004}
 for a,v in sites.items():assert w(a)==v,(hex(a),hex(w(a)))
 # Verify the three direct four-byte callers, the shared handle, facet and cleanup sizes.
 for pc,insn in [(0x168BD68,0x168ABD0),(0x168BE34,0x168ABD0),(0x168BEF8,0x168ABD0),(0x168E78C,0x168ABD0)]:
  branch=w(pc);disp=branch&0x03FFFFFC
  if disp&0x02000000:disp-=0x04000000
  assert branch&0xFC000003==0x48000001 and pc+disp==insn
 trace={int(i):int(a,16) for i,a in re.findall(r'^(\d+): ([a-f0-9]{8})$',text,re.M)};assert set(trace)==set(range(1265))
 prior=json.loads((OLD/'expected-device-trace.json').read_text())['functions'];assert [trace[i] for i in range(966)]==[int(a,16) for a in prior]
 arrays=[('kWideRegistry',registry),('kWideAggregate',wide),('kWideHeap',heap),('kWideGlobals',globals_),('kWideTraceSuffix',[trace[i] for i in range(966,1265)])]
 header='// Live wide state modeled from original stores and ELF metadata; reviewed trace suffix.\n#pragma once\n#include <stdint.h>\n'
 for name,values in arrays:
  header+='static constexpr uint32_t '+name+'[]={\n'
  for i in range(0,len(values),8):header+='    '+','.join(f'0x{v:08X}u' for v in values[i:i+8])+',\n'
  header+='};\n'
 if args.write_reference:HEADER.write_text(header)
 else:assert HEADER.read_text()==header
 result={'state_word_checks':checks,'original_instruction_checks':len(sites)+4,'prior_trace_entries_preserved':966,'function_entries':1265,'checked_accesses':16590,'state_reference_sha256':hashlib.sha256(header.encode()).hexdigest()}
 if args.audit:args.audit.write_text(json.dumps(result,indent=2)+'\n')
 print(f'Original wide-state model: {checks} words and {len(sites)+4} instruction checks passed; prior 966-entry trace preserved')
if __name__=='__main__':main()
