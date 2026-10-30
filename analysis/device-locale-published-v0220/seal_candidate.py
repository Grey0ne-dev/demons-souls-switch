#!/usr/bin/env python3
"""Freeze the v0.22 candidate only after sanitizer and package validation."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PORT=ROOT/"switch-port"
SUFFIX=[0x1676088,0xF17448,0x10BA0,0x16693E0,0xF172D0,0x179118,0x11720,0x1666594,
        0x16B7F40,0x16693E0,0x1666594,0x166F990,0x1676150,0x16BB2E4,0x166F910,
        0x1676108,0x166F910,0x1676108,0x16BB264,0x16B7F30,0x166F990,0x1676150,
        0x16BB2E4,0x16BB244,0x166F910,0x1676108,0x16BB264,0x16A0500]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    previous=HERE.parent/"device-locale-object-v0210"
    prefix=json.loads((previous/"expected-device-trace.json").read_text())["functions"]
    assert len(prefix)==669 and len(SUFFIX)==28
    expected=prefix+[f"0x{a:08X}" for a in SUFFIX]
    logs={"regression-tests.log":"/tmp/published-regression-tests.log",
          "published-tests.log":"/tmp/published-tests.log","copy-tests.log":"/tmp/published-copy-tests.log"}
    for fault in ("state","tls-state","copy-input","copy-output","tls-consumer","capacity","argument"):
        logs[f"{fault}-fault.log"]=f"/tmp/published-{fault}-fault.log"
    checks={}
    for name,source in logs.items():
        content=Path(source).read_text()
        results=re.findall(r"(\d+) checks, (\d+) failures",content)
        assert results and all(int(f)==0 for _,f in results),name
        assert not any(s in content for s in ("FAIL ","LeakSanitizer","ERROR: AddressSanitizer","runtime error:")),name
        checks[name]=sum(int(c) for c,_ in results)
    assert checks["regression-tests.log"]==589,"Full regression suite must finish"
    assert checks["published-tests.log"]==78 and checks["copy-tests.log"]==7681
    assert all(checks[f"{fault}-fault.log"]==5 for fault in
               ("state","tls-state","copy-input","copy-output","tls-consumer","capacity","argument"))
    probe=Path(logs["published-tests.log"]).read_text()
    observed={int(i):int(a,16) for i,a in re.findall(r"^(\d+): ([0-9a-f]{8})$",probe,re.M)}
    assert observed=={i:int(a,16) for i,a in enumerate(expected)}
    assert "PUBLISHED ok=1 stop=33 callee=0168abd0 functions=697 accesses=12492 completed=12" in probe
    links=Path("/tmp/published-lifter-tests.log").read_text()
    assert "9 checks, 0 failures" in links
    provenance=json.loads((PORT/"build/entry/entry-provenance.json").read_text())
    old=json.loads((previous/"entry-provenance.json").read_text())
    assert len(provenance["functions"])==203
    assert all(provenance["functions"].get(k)==v for k,v in old["functions"].items())
    assert set(provenance["functions"])-set(old["functions"])=={
        "01676088","00F17448","00F172D0","00179118","00011720","01666594","016B7F40"}
    assert provenance["original_bctrl_return_pcs"]==old["original_bctrl_return_pcs"]
    assert set(provenance["verified_cross_toc_tail_thunks"])=={"01676088","00011720"}
    candidate=PORT/"demons-souls-loader.nro"
    binary=candidate.read_bytes()
    assert binary[0x10:0x14]==b"NRO0" and b"startup v0.22.0" in binary
    assert binary==(PORT/"sdcard/switch/demons-souls-loader/demons-souls-loader.nro").read_bytes()
    manifest=json.loads((PORT/"sdcard/manifest.json").read_text())
    assert manifest["version"]=="0.22.0" and manifest["device_tested"] is False
    with zipfile.ZipFile(PORT/"demons-souls-switch-startup.zip") as archive:
        assert archive.testzip() is None
        for item in manifest["files"]:
            assert hashlib.sha256(archive.read(item["path"])).hexdigest()==item["sha256"]
        assert archive.read("switch/demons-souls-loader/demons-souls-loader.nro")==binary
    target=HERE/"candidate.nro"
    assert not target.exists(),"Refuse to replace a sealed candidate"
    shutil.copy2(candidate,target)
    for name,source in logs.items():shutil.copy2(source,HERE/name)
    shutil.copy2("/tmp/published-switch-build.log",HERE/"switch-build.log")
    shutil.copy2("/tmp/published-lifter-tests.log",HERE/"lifter-tests.log")
    shutil.copy2(PORT/"build/entry/entry-provenance.json",HERE/"entry-provenance.json")
    (HERE/"expected-device-trace.json").write_text(json.dumps({"functions":expected},indent=2)+"\n")
    sources=["Makefile","source/main.c","entry/ds_entry.cpp","entry/ds_entry.h","entry/generate_entry.py",
             "entry/ds_heap.c","entry/ds_heap.h","entry/ds_sync.cpp","source/ds_image.c","include/ds_image.h",
             "tests/test_published.cpp","tests/test_copy.cpp","tests/run_copy_tests.sh",
             "tests/test_locale.cpp","tests/test_allocated.cpp","tests/test_entry.cpp","tests/run_entry_tests.sh",
             "tools/prepare_sd.py"]
    files=[PORT/p for p in sources]+[Path(__file__),HERE/"validate_device.py"]
    result=dict(version="0.22.0",size=len(binary),sha256=digest(target),constructors_completed=12,
                constructor_thirteen_started=True,function_entries=697,checked_accesses=12492,pinned_bodies=203,
                runtime_allocations_served=[40,4,52],runtime_guest_addresses=["0x60000000","0x60000030","0x60000040"],
                runtime_requested_bytes=96,runtime_live_bytes=128,runtime_backed_bytes=65536,
                locale_name="C",tls_locale_published=True,guest_locks=19,guest_unlocks=19,native_locks_held_at_stop=0,
                sanitizer_checks=checks,sanitizer_checks_total=sum(checks.values()),compiled_lifter_link_checks=9,
                source_sha256={str(p.relative_to(ROOT)):digest(p) for p in files},
                evidence_sha256={p.name:digest(p) for p in sorted(HERE.iterdir()) if p.is_file() and p.name!="build-verification.json"},
                device_execution_passed=False)
    (HERE/"build-verification.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({k:v for k,v in result.items() if k not in ("source_sha256","evidence_sha256")},indent=2))


if __name__=="__main__":main()
