#!/usr/bin/env python3
"""Seal v0.27 only after full clean tests, source audit and package validation."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PORT=ROOT/'switch-port'
FAULTS=[f'alloc-{n}-{kind}' for n in range(14,26) for kind in ('size','caller','stack','toc')]+[
    'registry','references','fill','handle','buffer','cleanup','counter','callback','next-target','extra','lock',
    'virtual-target','virtual-argument','virtual-stack','virtual-toc','conversion-pointer','native-last','conversion-result','reorder']

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    assert not (HERE/'candidate.nro').exists(),'Refuse to replace sealed evidence'
    logs={'regression-tests.log':Path('/tmp/wide-streams-regression-tests.log'),
          'wide-streams-tests.log':Path('/tmp/wide-streams-final-clean-tests.log'),
          'conversion-tests.log':Path('/tmp/wide-streams-conversion-tests.log')}
    logs.update({f'{f}-fault.log':Path(f'/tmp/wide-streams-{f}-fault.log') for f in FAULTS})
    checks={}
    for name,p in logs.items():
        s=p.read_text();found=re.findall(r'(\d+) checks, (\d+) failures',s)
        assert found and all(int(f)==0 for _,f in found),name
        assert not any(t in s for t in ('FAIL ','LeakSanitizer','ERROR: AddressSanitizer','runtime error:','unbound variable')),name
        checks[name]=sum(int(c) for c,_ in found)
    assert checks['regression-tests.log']==589
    assert checks['wide-streams-tests.log']==549
    assert checks['conversion-tests.log']==8386
    assert all(checks[f'{f}-fault.log']==6 for f in FAULTS)
    assert len(FAULTS)==67 and sum(checks.values())==9926
    clean=logs['wide-streams-tests.log'].read_text()
    assert 'WIDE ok=1 stop=38 callee=00745248 functions=1265 accesses=16590 completed=13' in clean
    observed={int(i):f'0x{int(a,16):08X}' for i,a in re.findall(r'^(\d+): ([a-f0-9]{8})$',clean,re.M)}
    assert len(observed)==1265 and set(observed)==set(range(1265))
    expected=[observed[i] for i in range(1265)]
    old=json.loads((HERE.parent/'device-narrow-streams-v0260/entry-provenance.json').read_text())
    new=json.loads((PORT/'build/entry/entry-provenance.json').read_text())
    prior=json.loads((HERE.parent/'device-narrow-streams-v0260/expected-device-trace.json').read_text())['functions']
    assert expected[:966]==prior
    added={'0168BC68','0168E548','0166CA50','0166FE88','00B17510','00010C50','0166C0E8','01664E60'}
    assert len(new['functions'])==223 and set(new['functions'])-set(old['functions'])==added
    assert all(new['functions'].get(k)==v for k,v in old['functions'].items())
    assert all(new['original_bctrl_return_pcs'].get(k)==v for k,v in old['original_bctrl_return_pcs'].items())
    assert set(new['original_bctrl_return_pcs'])-set(old['original_bctrl_return_pcs'])=={'0168E548'}
    assert sum(map(len,new['original_bctrl_return_pcs'].values()))==65 and len(new['original_bctrl_return_pcs'])==29
    assert all(new['verified_cross_toc_tail_thunks'].get(k)==v for k,v in old['verified_cross_toc_tail_thunks'].items())
    assert set(new['verified_cross_toc_tail_thunks'])-set(old['verified_cross_toc_tail_thunks'])=={'00010C50'}
    assert set(new['strict_unsupported_later_functions'])==set(old['strict_unsupported_later_functions'])-added
    assert new['explicit_checkpoint_functions']==old['explicit_checkpoint_functions']
    generated=(PORT/'build/entry/entry_slice.cpp').read_bytes()
    assert generated==(PORT/'build-host-explore-tests/entry/entry_slice.cpp').read_bytes()
    assert b'ds_test_wide_fault' not in generated and b'ds_test_narrow_fault' not in generated and b'ds_test_first_stream_fault' not in generated
    subprocess.run([sys.executable,str(PORT/'tests/audit_wide_streams.py'),'--probe',str(logs['wide-streams-tests.log'])],check=True)
    lifter=Path('/tmp/wide-streams-lifter-tests.log').read_text();assert '9 checks, 0 failures' in lifter
    build=Path('/tmp/wide-streams-switch-build.log').read_text();assert 'elf2nro' in build and 'error:' not in build
    binary=(PORT/'demons-souls-loader.nro').read_bytes()
    assert binary[0x10:0x14]==b'NRO0' and b'startup v0.27.0' in binary
    assert binary==(PORT/'sdcard/switch/demons-souls-loader/demons-souls-loader.nro').read_bytes()
    manifest=json.loads((PORT/'sdcard/manifest.json').read_text());assert manifest['version']=='0.27.0' and manifest['device_tested'] is False
    with zipfile.ZipFile(PORT/'demons-souls-switch-startup.zip') as archive:
        assert archive.testzip() is None
        for item in manifest['files']:
            data=archive.read(item['path']);assert len(data)==item['size'] and hashlib.sha256(data).hexdigest()==item['sha256']
        assert archive.read('switch/demons-souls-loader/demons-souls-loader.nro')==binary
    # Check everything above before creating any sealed artifact.
    shutil.copy2(PORT/'demons-souls-loader.nro',HERE/'candidate.nro')
    for name,p in logs.items():shutil.copy2(p,HERE/name)
    shutil.copy2('/tmp/wide-streams-switch-build.log',HERE/'switch-build.log')
    shutil.copy2('/tmp/wide-streams-lifter-tests.log',HERE/'lifter-tests.log')
    shutil.copy2(PORT/'build/entry/entry-provenance.json',HERE/'entry-provenance.json')
    (HERE/'expected-device-trace.json').write_text(json.dumps({'functions':expected},indent=2)+'\n')
    sources=[PORT/p for p in ('Makefile','source/main.c','source/ds_image.c','include/ds_image.h','include/ds_reference.h',
             'entry/ds_entry.cpp','entry/ds_entry.h','entry/generate_entry.py','entry/ds_narrow_reference.h','entry/ds_wide_reference.h',
             'entry/ds_heap.c','entry/ds_heap.h','entry/ds_sync.cpp','entry/ds_sync.h','entry/ds_atomic.c',
             'entry/ds_clock.cpp','entry/ds_kernel_mutex.cpp','entry/ds_rwlock.cpp',
             'tests/test_narrow_streams.cpp','tests/test_wide_streams.cpp','tests/audit_narrow_streams.py','tests/audit_wide_streams.py',
             'tests/test_conversion.cpp','tests/run_conversion_tests.sh','tests/test_first_stream.cpp',
             'tests/test_entry.cpp','tests/run_entry_tests.sh','tools/prepare_sd.py','reference.json')]
    sources += [Path(__file__),HERE/'validate_device.py',HERE.parent/'WIDE_STREAMS_AUDIT.md']
    for p in sources:
        target=HERE/'source-snapshot'/p.relative_to(ROOT);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
    result={'version':'0.27.0','size':len(binary),'sha256':digest(HERE/'candidate.nro'),
            'constructors_completed':13,'constructor_thirteen_completed':True,'function_entries':1265,'checked_accesses':16590,
            'pinned_bodies':223,'runtime_allocations_served':[40,4,52,40,4,4,24,8,4,40,4,4,4,40,4,40,4,40,4,4,32,8,4,4,4],
            'runtime_requested_bytes':420,'runtime_live_bytes':672,'runtime_backed_bytes':65536,
            'total_guest_backed_bytes':31687456,'locale_references':16,'registered_narrow_streams':4,'registered_wide_streams':4,
            'crt_mutexes':18,'thread_id_calls':58,'guest_locks':66,'guest_unlocks':66,'native_locks_held_at_stop':0,
            'sanitizer_checks':checks,'sanitizer_checks_total':sum(checks.values()),'compiled_lifter_link_checks':9,
            'source_model_word_checks':567,'original_instruction_checks':20,
            'source_sha256':{str(p.relative_to(ROOT)):digest(p) for p in sources},
            'evidence_sha256':{str(p.relative_to(HERE)):digest(p) for p in sorted(HERE.rglob('*')) if p.is_file() and p.name!='build-verification.json' and '__pycache__' not in p.parts},
            'device_execution_passed':False}
    (HERE/'build-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('source_sha256','evidence_sha256','sanitizer_checks')},indent=2))

if __name__=='__main__':main()
