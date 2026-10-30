#!/usr/bin/env python3
"""Freeze v0.21 only after source, sanitizer and package checks pass."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PORT=ROOT/"switch-port"
SUFFIX=[0x16896C0,0x16891F0,0x16B7F30,0x166F990,0x1676150,0x16BB2E4,
        0x16BB244,0x16B7F30,0x168ABD0,0x167AC88,0x1689180]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    previous=HERE.parent/"device-backed-heap-v0200"
    prefix=json.loads((previous/"expected-device-trace.json").read_text())["functions"]
    assert len(prefix)==658 and len(SUFFIX)==11
    expected=prefix+[f"0x{a:08X}" for a in SUFFIX]
    logs={
        "regression-tests.log":"/tmp/locale-regression-tests.log",
        "locale-tests.log":"/tmp/locale-tests.log",
        "locale-state-fault.log":"/tmp/locale-state-fault.log",
        "locale-lock-fault.log":"/tmp/locale-lock-fault.log",
        "locale-tls-fault.log":"/tmp/locale-tls-fault.log",
        "locale-argument-fault.log":"/tmp/locale-argument-fault.log",
        "allocated-regression.log":"/tmp/locale-allocated-regression.log",
        "runtime-regression.log":"/tmp/locale-runtime-regression.log",
        "older-checkpoints.log":"/tmp/locale-older-checkpoints.log",
    }
    checks={}
    for name,source in logs.items():
        content=Path(source).read_text()
        results=re.findall(r"(\d+) checks, (\d+) failures",content)
        assert results and all(int(f)==0 for _,f in results),name
        assert not any(s in content for s in ("FAIL ","LeakSanitizer","ERROR: AddressSanitizer","runtime error:")),name
        checks[name]=sum(int(c) for c,_ in results)
    assert checks["regression-tests.log"]==589,"Full regression suite must finish"
    assert checks["locale-tests.log"]==51
    assert checks["older-checkpoints.log"]==224
    probe=Path(logs["locale-tests.log"]).read_text()
    observed={int(i):int(a,16) for i,a in re.findall(r"^(\d+): ([0-9a-f]{8})$",probe,re.M)}
    assert observed=={i:int(expected[i],16) for i in range(137,669)}
    assert "LOCALE ok=1 stop=32 callee=01676088 functions=669 accesses=12314 completed=12" in probe
    links=Path("/tmp/locale-lifter-tests.log").read_text()
    assert "9 checks, 0 failures" in links
    provenance=json.loads((PORT/"build/entry/entry-provenance.json").read_text())
    old=json.loads((previous/"entry-provenance.json").read_text())
    assert len(provenance["functions"])==196
    assert all(provenance["functions"].get(k)==v for k,v in old["functions"].items())
    assert set(provenance["functions"])-set(old["functions"])=={"016896C0","016891F0","016B7F30","01689180"}
    assert "01676088" not in provenance["functions"] and "016B7F40" not in provenance["functions"]
    assert provenance["original_bctrl_return_pcs"]==old["original_bctrl_return_pcs"]
    candidate=PORT/"demons-souls-loader.nro"
    binary=candidate.read_bytes()
    assert binary[0x10:0x14]==b"NRO0" and b"startup v0.21.0" in binary
    assert binary==(PORT/"sdcard/switch/demons-souls-loader/demons-souls-loader.nro").read_bytes()
    with zipfile.ZipFile(PORT/"demons-souls-switch-startup.zip") as archive:
        assert archive.testzip() is None
        assert archive.read("switch/demons-souls-loader/demons-souls-loader.nro")==binary
    target=HERE/"candidate.nro"
    if target.exists():assert target.read_bytes()==binary,"Refuse to replace a sealed candidate"
    else:shutil.copy2(candidate,target)
    for name,source in logs.items():shutil.copy2(source,HERE/name)
    shutil.copy2("/tmp/locale-switch-build.log",HERE/"switch-build.log")
    shutil.copy2("/tmp/locale-lifter-tests.log",HERE/"lifter-tests.log")
    shutil.copy2(PORT/"build/entry/entry-provenance.json",HERE/"entry-provenance.json")
    (HERE/"expected-device-trace.json").write_text(json.dumps({"functions":expected},indent=2)+"\n")
    sources=["Makefile","source/main.c","entry/ds_entry.cpp","entry/ds_entry.h","entry/generate_entry.py",
             "entry/ds_heap.c","entry/ds_heap.h","entry/ds_sync.cpp","source/ds_image.c","include/ds_image.h",
             "tests/test_locale.cpp","tests/test_allocated.cpp","tests/test_entry.cpp","tests/run_entry_tests.sh",
             "tools/prepare_sd.py"]
    files=[PORT/p for p in sources]+[Path(__file__),HERE/"validate_device.py"]
    result=dict(version="0.21.0",size=len(binary),sha256=digest(target),constructors_completed=12,
                constructor_thirteen_started=True,function_entries=669,checked_accesses=12314,pinned_bodies=196,
                runtime_allocations_served=[40,4,52],runtime_guest_addresses=["0x60000000","0x60000030","0x60000040"],
                runtime_requested_bytes=96,runtime_live_bytes=128,runtime_backed_bytes=65536,
                tls_locale_published=False,guest_locks=18,guest_unlocks=17,native_locks_held_at_stop=1,
                sanitizer_checks=checks,sanitizer_checks_total=sum(checks.values()),compiled_lifter_link_checks=9,
                source_sha256={str(p.relative_to(ROOT)):digest(p) for p in files},
                evidence_sha256={p.name:digest(p) for p in sorted(HERE.iterdir()) if p.is_file() and p.name!="build-verification.json"},
                device_execution_passed=False)
    (HERE/"build-verification.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({k:v for k,v in result.items() if k not in ("source_sha256","evidence_sha256")},indent=2))


if __name__=="__main__":main()
