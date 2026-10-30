#!/usr/bin/env python3
"""Freeze v0.23 after sanitizer, source-provenance and package checks pass."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PORT=ROOT/"switch-port"
FAULTS=("state","handle","fourth","fifth","sixth","stack","native","tls")
SUFFIX=[
    0x168ABD0,0x167AC88,0x1676258,0x16BB2C4,0x168ABD0,0x167AC88,
    0x16896C0,0x16891F0,0x16B7F30,0x16B7F30,0x166F990,0x1676150,0x16BB2E4,
    0x16BB244,0x166F910,0x1676108,0x16BB264,0x1688138,0x1685FC8,0x168ABD0,
    0x167AC88,0x16896C0,0x16891F0,0x16B7F30,0x16B7F30,0x166F990,0x1676150,
    0x16BB2E4,0x16BB244,0x166F910,0x1676108,0x16BB264,0x1685248,0x166F990,
    0x1676150,0x16BB2E4,0x16BB244,0x166F910,0x1676108,0x16BB264,0x166F990,
    0x1676150,0x16BB2E4,0x16BB244,0x166F990,0x1676150,0x16BB2E4,0x166F910,
    0x1676108,0x1688538
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    previous=HERE.parent/"device-locale-published-v0220"
    prefix=json.loads((previous/"expected-device-trace.json").read_text())["functions"]
    assert len(prefix)==697 and len(SUFFIX)==50
    expected=prefix+[f"0x{a:08X}" for a in SUFFIX]
    logs={"regression-tests.log":"/tmp/stream-locale-regression-tests.log",
          "stream-locale-tests.log":"/tmp/stream-locale-final-clean-tests.log",
          "published-regression.log":"/tmp/stream-locale-published-regression.log"}
    logs.update({f"{fault}-fault.log":f"/tmp/stream-locale-{fault}-fault.log" for fault in FAULTS})
    checks={}
    for name,source in logs.items():
        content=Path(source).read_text()
        results=re.findall(r"(\d+) checks, (\d+) failures",content)
        assert results and all(int(f)==0 for _,f in results),name
        assert not any(s in content for s in ("FAIL ","LeakSanitizer","ERROR: AddressSanitizer","runtime error:")),name
        checks[name]=sum(int(c) for c,_ in results)
    assert checks["regression-tests.log"]==589,"Full regression suite must finish"
    assert checks["stream-locale-tests.log"]==84 and checks["published-regression.log"]==78
    assert all(checks[f"{fault}-fault.log"]==5 for fault in FAULTS)
    probe=Path(logs["stream-locale-tests.log"]).read_text()
    observed={int(i):int(a,16) for i,a in re.findall(r"^(\d+): ([0-9a-f]{8})$",probe,re.M)}
    assert observed=={i:int(a,16) for i,a in enumerate(expected)}
    assert "STREAM ok=1 stop=34 callee=0168abd0 functions=747 accesses=12849 completed=12" in probe
    assert "BOUNDARY NID: 350d454e" in probe
    links=Path("/tmp/stream-locale-lifter-tests.log").read_text()
    assert "9 checks, 0 failures" in links
    provenance=json.loads((PORT/"build/entry/entry-provenance.json").read_text())
    old=json.loads((previous/"entry-provenance.json").read_text())
    assert len(provenance["functions"])==207
    assert all(provenance["functions"].get(k)==v for k,v in old["functions"].items())
    assert set(provenance["functions"])-set(old["functions"])=={"01688138","01685FC8","01685248","01688538"}
    sites=provenance["original_bctrl_return_pcs"];old_sites=old["original_bctrl_return_pcs"]
    assert all(sites.get(k)==v for k,v in old_sites.items())
    assert set(sites)-set(old_sites)=={"01688138"} and len(sites["01688138"])==2
    assert sum(map(len,sites.values()))==63 and len(sites)==28
    assert provenance["verified_cross_toc_tail_thunks"]==old["verified_cross_toc_tail_thunks"]
    candidate=PORT/"demons-souls-loader.nro";binary=candidate.read_bytes()
    assert binary[0x10:0x14]==b"NRO0" and b"startup v0.23.0" in binary
    assert binary==(PORT/"sdcard/switch/demons-souls-loader/demons-souls-loader.nro").read_bytes()
    manifest=json.loads((PORT/"sdcard/manifest.json").read_text())
    assert manifest["version"]=="0.23.0" and manifest["device_tested"] is False
    with zipfile.ZipFile(PORT/"demons-souls-switch-startup.zip") as archive:
        assert archive.testzip() is None
        for item in manifest["files"]:
            assert hashlib.sha256(archive.read(item["path"])).hexdigest()==item["sha256"]
        assert archive.read("switch/demons-souls-loader/demons-souls-loader.nro")==binary
    target=HERE/"candidate.nro"
    assert not target.exists(),"Refuse to replace a sealed candidate"
    shutil.copy2(candidate,target)
    for name,source in logs.items():shutil.copy2(source,HERE/name)
    shutil.copy2("/tmp/stream-locale-switch-build.log",HERE/"switch-build.log")
    shutil.copy2("/tmp/stream-locale-lifter-tests.log",HERE/"lifter-tests.log")
    shutil.copy2(PORT/"build/entry/entry-provenance.json",HERE/"entry-provenance.json")
    (HERE/"expected-device-trace.json").write_text(json.dumps({"functions":expected},indent=2)+"\n")
    sources=["Makefile","source/main.c","entry/ds_entry.cpp","entry/ds_entry.h","entry/generate_entry.py",
             "entry/ds_heap.c","entry/ds_heap.h","entry/ds_sync.cpp","source/ds_image.c","include/ds_image.h",
             "tests/test_stream_locale.cpp","tests/test_published.cpp","tests/test_locale.cpp",
             "tests/test_allocated.cpp","tests/test_entry.cpp","tests/run_entry_tests.sh","tools/prepare_sd.py"]
    files=[PORT/p for p in sources]+[Path(__file__),HERE/"validate_device.py"]
    result=dict(version="0.23.0",size=len(binary),sha256=digest(target),constructors_completed=12,
                constructor_thirteen_started=True,function_entries=747,checked_accesses=12849,pinned_bodies=207,
                runtime_allocations_served=[40,4,52,40,4,4],
                runtime_guest_addresses=["0x60000000","0x60000030","0x60000040","0x60000080","0x600000B0","0x600000C0"],
                runtime_requested_bytes=144,runtime_live_bytes=208,runtime_backed_bytes=65536,
                locale_name="C",locale_references=6,tls_locale_published=True,
                guest_locks=23,guest_unlocks=22,native_locks_held_at_stop=1,
                sanitizer_checks=checks,sanitizer_checks_total=sum(checks.values()),compiled_lifter_link_checks=9,
                source_sha256={str(p.relative_to(ROOT)):digest(p) for p in files},
                evidence_sha256={p.name:digest(p) for p in sorted(HERE.iterdir()) if p.is_file() and p.name!="build-verification.json"},
                device_execution_passed=False)
    (HERE/"build-verification.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({k:v for k,v in result.items() if k not in ("source_sha256","evidence_sha256")},indent=2))


if __name__=="__main__":main()
