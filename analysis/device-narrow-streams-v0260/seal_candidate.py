#!/usr/bin/env python3
"""Seal v0.26 only after full clean tests, source audit and package validation."""
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
FAULTS=[f'alloc-{n}-{kind}' for n in range(9,14) for kind in ('size','caller','stack','toc')]+[
    'registry','references','buffer','tie','locale','facet','lock','virtual','reorder','extra',
    'boundary-caller','boundary-stack','boundary-toc']

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    assert not (HERE/'candidate.nro').exists(),'Refuse to replace sealed evidence'
    logs={'regression-tests.log':Path('/tmp/narrow-streams-regression-tests.log'),
          'narrow-streams-tests.log':Path('/tmp/narrow-streams-final-clean-tests.log')}
    logs.update({f'{f}-fault.log':Path(f'/tmp/narrow-streams-{f}-fault.log') for f in FAULTS})
    checks={}
    for name,p in logs.items():
        s=p.read_text();found=re.findall(r'(\d+) checks, (\d+) failures',s)
        assert found and all(int(f)==0 for _,f in found),name
        assert not any(t in s for t in ('FAIL ','LeakSanitizer','ERROR: AddressSanitizer','runtime error:','unbound variable')),name
        checks[name]=sum(int(c) for c,_ in found)
    assert checks['regression-tests.log']==589
    assert checks['narrow-streams-tests.log']==322
    assert all(checks[f'{f}-fault.log']==6 for f in FAULTS)
    assert len(FAULTS)==33 and sum(checks.values())==1109
    clean=logs['narrow-streams-tests.log'].read_text()
    assert 'NARROW ok=1 stop=37 callee=0168bc68 functions=966 accesses=14378 completed=12' in clean
    observed={int(i):f'0x{int(a,16):08X}' for i,a in re.findall(r'^(\d+): ([a-f0-9]{8})$',clean,re.M)}
    assert len(observed)==966 and set(observed)==set(range(966))
    expected=[observed[i] for i in range(966)]
    old=json.loads((HERE.parent/'device-first-stream-v0250/entry-provenance.json').read_text())
    new=json.loads((PORT/'build/entry/entry-provenance.json').read_text())
    prior=json.loads((HERE.parent/'device-first-stream-v0250/expected-device-trace.json').read_text())['functions']
    assert expected[:810]==prior
    assert len(new['functions'])==215 and new['functions']==old['functions']
    assert new['original_bctrl_return_pcs']==old['original_bctrl_return_pcs']
    assert sum(map(len,new['original_bctrl_return_pcs'].values()))==63
    assert new['verified_cross_toc_tail_thunks']==old['verified_cross_toc_tail_thunks']
    assert new['strict_unsupported_later_functions']==old['strict_unsupported_later_functions']
    assert all(new['explicit_checkpoint_functions'].get(k)==v for k,v in old['explicit_checkpoint_functions'].items())
    assert set(new['explicit_checkpoint_functions'])-set(old['explicit_checkpoint_functions'])=={'0168BC68'}
    generated=(PORT/'build/entry/entry_slice.cpp').read_bytes()
    assert generated==(PORT/'build-host-explore-tests/entry/entry_slice.cpp').read_bytes()
    assert b'ds_test_narrow_fault' not in generated and b'ds_test_first_stream_fault' not in generated
    subprocess.run([sys.executable,str(PORT/'tests/audit_narrow_streams.py'),'--probe',str(logs['narrow-streams-tests.log'])],check=True)
    lifter=Path('/tmp/narrow-streams-lifter-tests.log').read_text();assert '9 checks, 0 failures' in lifter
    build=Path('/tmp/narrow-streams-switch-build.log').read_text();assert 'elf2nro' in build and 'error:' not in build
    binary=(PORT/'demons-souls-loader.nro').read_bytes()
    assert binary[0x10:0x14]==b'NRO0' and b'startup v0.26.0' in binary
    assert binary==(PORT/'sdcard/switch/demons-souls-loader/demons-souls-loader.nro').read_bytes()
    manifest=json.loads((PORT/'sdcard/manifest.json').read_text());assert manifest['version']=='0.26.0' and manifest['device_tested'] is False
    with zipfile.ZipFile(PORT/'demons-souls-switch-startup.zip') as archive:
        assert archive.testzip() is None
        for item in manifest['files']:
            data=archive.read(item['path']);assert len(data)==item['size'] and hashlib.sha256(data).hexdigest()==item['sha256']
        assert archive.read('switch/demons-souls-loader/demons-souls-loader.nro')==binary
    # Check everything above before creating any sealed artifact.
    shutil.copy2(PORT/'demons-souls-loader.nro',HERE/'candidate.nro')
    for name,p in logs.items():shutil.copy2(p,HERE/name)
    shutil.copy2('/tmp/narrow-streams-switch-build.log',HERE/'switch-build.log')
    shutil.copy2('/tmp/narrow-streams-lifter-tests.log',HERE/'lifter-tests.log')
    shutil.copy2(PORT/'build/entry/entry-provenance.json',HERE/'entry-provenance.json')
    (HERE/'expected-device-trace.json').write_text(json.dumps({'functions':expected},indent=2)+'\n')
    sources=[PORT/p for p in ('Makefile','source/main.c','source/ds_image.c','include/ds_image.h','include/ds_reference.h',
             'entry/ds_entry.cpp','entry/ds_entry.h','entry/generate_entry.py','entry/ds_narrow_reference.h',
             'entry/ds_heap.c','entry/ds_heap.h','entry/ds_sync.cpp','entry/ds_sync.h','entry/ds_atomic.c',
             'entry/ds_clock.cpp','entry/ds_kernel_mutex.cpp','entry/ds_rwlock.cpp',
             'tests/test_narrow_streams.cpp','tests/audit_narrow_streams.py','tests/test_first_stream.cpp',
             'tests/test_entry.cpp','tests/run_entry_tests.sh','tools/prepare_sd.py','reference.json')]
    sources += [Path(__file__),HERE/'validate_device.py',HERE.parent/'NARROW_STREAMS_AUDIT.md']
    for p in sources:
        target=HERE/'source-snapshot'/p.relative_to(ROOT);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
    result={'version':'0.26.0','size':len(binary),'sha256':digest(HERE/'candidate.nro'),
            'constructors_completed':12,'constructor_thirteen_started':True,'function_entries':966,'checked_accesses':14378,
            'pinned_bodies':215,'runtime_allocations_served':[40,4,52,40,4,4,24,8,4,40,4,4,4],
            'runtime_requested_bytes':232,'runtime_live_bytes':368,'runtime_backed_bytes':65536,
            'total_guest_backed_bytes':31687456,'locale_references':9,'registered_narrow_streams':4,
            'crt_mutexes':15,'thread_id_calls':31,'guest_locks':42,'guest_unlocks':42,'native_locks_held_at_stop':0,
            'sanitizer_checks':checks,'sanitizer_checks_total':sum(checks.values()),'compiled_lifter_link_checks':9,
            'source_model_word_checks':258,'original_instruction_checks':9,
            'source_sha256':{str(p.relative_to(ROOT)):digest(p) for p in sources},
            'evidence_sha256':{str(p.relative_to(HERE)):digest(p) for p in sorted(HERE.rglob('*')) if p.is_file() and p.name!='build-verification.json' and '__pycache__' not in p.parts},
            'device_execution_passed':False}
    (HERE/'build-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('source_sha256','evidence_sha256','sanitizer_checks')},indent=2))

if __name__=='__main__':main()
