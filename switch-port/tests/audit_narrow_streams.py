#!/usr/bin/env python3
"""Model the original narrow initializer's defined state from ELF constants."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[2]
ELF=ROOT/'upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF'
HEADER=ROOT/'switch-port/entry/ds_narrow_reference.h'
EXPECTED_ELF='90eaa5e9e48b4874292badc311ccea253e6a622ff188bfbdbe76e6f6821269d8'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',type=Path,required=True)
    parser.add_argument('--write-reference',action='store_true')
    parser.add_argument('--audit',type=Path)
    args=parser.parse_args()
    data=ELF.read_bytes();assert hashlib.sha256(data).hexdigest()==EXPECTED_ELF
    phoff=struct.unpack_from('>Q',data,32)[0];sz,n=struct.unpack_from('>HH',data,54)
    segs=[struct.unpack_from('>IIQQQQQQ',data,phoff+i*sz) for i in range(n)]
    def word(a):
        for kind,_,off,start,_,filesz,memsz,_ in segs:
            if kind==1 and start<=a and a+4<=start+filesz:return struct.unpack_from('>I',data,off+a-start)[0]
            if kind==1 and start+filesz<=a and a+4<=start+memsz:return 0
        raise ValueError(hex(a))
    toc=0x19D3BAC
    aggregate=[0]*(0x238//4);registry=[0]*(0x60//4);heap=[0]*(0x170//4)
    def put(offset,value):aggregate[offset//4]=value
    # Three stream buffers share the original layout; their data tables and
    # mutex/locale handles differ. All stores below correspond to 01686CE8.
    for offset,table_offset,mutex,handle in [(0,0x4C6C,0,0x30),(0x70,0x4C70,0x80,0xB0),(0x158,0x4C74,0x110,0x140)]:
        meta=word(toc+table_offset)
        values={0:word(toc+0x4C68),4:0x60000000+mutex,0x10:meta,0x14:meta,
                0x20:meta+8,0x24:meta+8,0x30:meta+12,0x34:meta+16,
                0x38:0x60000000+handle,0x50:meta+0x40,0x54:meta+0x50,0x6C:meta-8}
        for field,value in values.items():put(offset+field,value)
        assert word(word(toc+0x4C1C))==0 and word(word(toc+0x4C1C)+4)==0
    first_type=word(toc+0x4C3C);later_type=word(toc+0x4C40)
    streams=[(0xE8,0xE0,first_type,0xC0,0),(0x120,0x11C,later_type,0x100,0x70),
             (0x1CC,0x1C8,later_type,0x150,0x158),(0x204,0x200,later_type,0x160,0x158)]
    for index,(offset,prefix,vtable,handle,tied) in enumerate(streams,1):
        put(prefix,vtable)
        fields={0:vtable+0x14,4:index,0x10:0x203 if index==3 else 0x201,0x14:6,
                0x24:0x60000000+handle,0x28:0x1CE64E8+tied,
                0x2C:0x1CE6604 if index in (1,3,4) else 0,0x30:0x20000000}
        for field,value in fields.items():put(offset+field,value)
        registry[(0xC+4*index)//4]=0x1CE64E8+offset
    registry[0]=0x00010101;registry[1]=0x01000000
    for offset in (0x18,0x98,0x128):heap[offset//4]=2
    for offset in (0x30,0xB0,0xC0,0x100,0x140,0x150,0x160):heap[offset//4]=0x60000040
    for offset,value in {0x40:0x18C7DE0,0x44:9,0x50:63,0x5C:0x43000000,0x6C:1,0x70:15,
                         0xD0:word(toc+0x4C30),0xD4:1,0xD8:word(word(toc+0x4C50)),
                         0xDC:word(word(toc+0x4C54)),0xE0:word(word(toc+0x4C58)),0xF4:0x600000D0}.items():heap[offset//4]=value
    instructions={0x1685FD8:0x38600004,0x1685FE8:0x48004BE9,0x16A0510:0x38600028,
                  0x16A0518:0x4BFEA6B9,0x1687044:0x38600004,0x1687048:0x48003B89,
                  0x168729C:0x4E800421,0x16872A0:0xE8410028}
    # Check the relative branch from the original caller independently.
    branch=word(0x15165E4);disp=branch&0x03FFFFFC
    if disp&0x02000000:disp-=0x04000000
    assert branch&0xFC000003==0x48000001 and 0x15165E4+disp==0x168BC68
    for addr,expected in instructions.items():assert word(addr)==expected,(hex(addr),hex(word(addr)))
    probe=args.probe.read_text()
    observed={int(i):int(a,16) for i,a in re.findall(r'^(\d+): ([a-f0-9]{8})$',probe,re.M)}
    assert len(observed)==966 and set(observed)==set(range(966))
    old=json.loads((ROOT/'analysis/device-first-stream-v0250/expected-device-trace.json').read_text())['functions']
    assert [observed[i] for i in range(810)]==[int(a,16) for a in old]
    checks=0
    for label,start,expected in [('STATE',0x1CE6488,registry),('STATE',0x1CE64E8,aggregate),('HEAP',0x60000000,heap)]:
        actual={int(a,16):int(v,16) for a,v in re.findall(r'^'+label+r' ([a-f0-9]+) ([a-f0-9]+)$',probe,re.M)}
        for i,value in enumerate(expected):
            assert actual.get(start+4*i)==value,(hex(start+4*i),actual.get(start+4*i),value)
            checks+=1
    arrays=[('kNarrowRegistry',registry),('kNarrowAggregate',aggregate),('kNarrowHeap',heap),('kNarrowTraceSuffix',[observed[i] for i in range(810,966)])]
    header='// Expected state modeled from original 01686CE8; trace from reviewed clean execution.\n#pragma once\n#include <stdint.h>\n'
    for name,values in arrays:
        header+='static constexpr uint32_t '+name+'[]={\n'
        for i in range(0,len(values),8):header+='    '+','.join(f'0x{v:08X}u' for v in values[i:i+8])+',\n'
        header+='};\n'
    if args.write_reference:HEADER.write_text(header)
    else:assert HEADER.read_text()==header,'State model or reviewed reference trace changed'
    result={'elf_sha256':EXPECTED_ELF,'state_word_checks':checks,'instruction_checks':len(instructions)+1,'prior_trace_entries_preserved':810,
            'trace_entries':966,'state_reference_sha256':hashlib.sha256(header.encode()).hexdigest(),
            'allocation_contexts':[{'size':size,'return_pc':f'0x{pc:08X}','sp':f'0x{sp:08X}','toc':'0x019D3BAC'} for size,pc,sp in
             [(4,0x1685FEC,0xFEFFA00),(40,0x16A051C,0xFEFFB60),(4,0x168704C,0xFEFFBF0),(4,0x1685FEC,0xFEFFA00),(4,0x1685FEC,0xFEFFB60)]]}
    if args.audit:args.audit.write_text(json.dumps(result,indent=2)+'\n')
    print(f'Original narrow-state model: {checks} words and {len(instructions)+1} instruction checks passed; prior 810-entry trace preserved')

if __name__=='__main__':main()
