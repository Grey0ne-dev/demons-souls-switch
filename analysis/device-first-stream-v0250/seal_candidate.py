#!/usr/bin/env python3
"""Freeze v0.25 after sanitizer, source-provenance and package checks pass."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PORT=ROOT/"switch-port"
FAULTS=("registration","fill","slot","virtual","ninth","stack","lock","zero","five","forty","caller","toc","other-stack","descriptor","target")
CHECKED_ACCESSES=13312
NORMAL_CHECKS=158
SUFFIX=[
    0x009E96A0,0x01684D70,0x0166F990,0x01676150,0x016BB2E4,0x016BB244,
    0x0166F910,0x01676108,0x016BB264,0x01688138,0x01685FC8
]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    previous=HERE.parent/"device-first-facet-v0240"
    assert CHECKED_ACCESSES>13238 and NORMAL_CHECKS>0,"Clean v0.25 evidence counts must be pinned"
    assert not (HERE/"candidate.nro").exists(),"Refuse to replace a sealed candidate"
    prefix=json.loads((previous/"expected-device-trace.json").read_text())["functions"]
    assert len(prefix)==799 and len(SUFFIX)==11
    expected=prefix+[f"0x{a:08X}" for a in SUFFIX]
    logs={"regression-tests.log":"/tmp/first-stream-regression-tests.log",
          "first-stream-tests.log":"/tmp/first-stream-final-clean-tests.log"}
    logs.update({f"{fault}-fault.log":f"/tmp/first-stream-{fault}-fault.log" for fault in FAULTS})
    checks={}
    for name,source in logs.items():
        content=Path(source).read_text()
        results=re.findall(r"(\d+) checks, (\d+) failures",content)
        assert results and all(int(f)==0 for _,f in results),name
        assert not any(s in content for s in ("FAIL ","LeakSanitizer","ERROR: AddressSanitizer","runtime error:")),name
        checks[name]=sum(int(c) for c,_ in results)
    assert checks["regression-tests.log"]==589,"Full regression suite must finish"
    assert checks["first-stream-tests.log"]==NORMAL_CHECKS
    assert all(checks[f"{fault}-fault.log"]==5 for fault in FAULTS)
    probe=Path(logs["first-stream-tests.log"]).read_text()
    observed={int(i):int(a,16) for i,a in re.findall(r"^(\d+): ([0-9a-f]{8})$",probe,re.M)}
    assert observed=={i:int(a,16) for i,a in enumerate(expected)}
    assert f"FIRST STREAM ok=1 stop=36 nid=1bc200f4 callee=0168abd0 functions=810 accesses={CHECKED_ACCESSES} completed=12" in probe
    links=Path("/tmp/first-stream-lifter-tests.log").read_text()
    assert "9 checks, 0 failures" in links
    provenance=json.loads((PORT/"build/entry/entry-provenance.json").read_text())
    old=json.loads((previous/"entry-provenance.json").read_text())
    assert len(provenance["functions"])==215
    assert all(provenance["functions"].get(k)==v for k,v in old["functions"].items())
    assert set(provenance["functions"])-set(old["functions"])=={"009E96A0","01684D70"}
    sites=provenance["original_bctrl_return_pcs"];old_sites=old["original_bctrl_return_pcs"]
    assert all(sites.get(k)==v for k,v in old_sites.items())
    assert sites==old_sites
    assert sum(map(len,sites.values()))==63 and len(sites)==28
    assert provenance["verified_cross_toc_tail_thunks"]==old["verified_cross_toc_tail_thunks"]
    assert set(provenance["strict_unsupported_later_functions"])==set(old["strict_unsupported_later_functions"])-{"01684D70"}
    assert provenance["explicit_checkpoint_functions"]==old["explicit_checkpoint_functions"]
    assert (PORT/"build/entry/entry_slice.cpp").read_bytes()==(PORT/"build-host-explore-tests/entry/entry_slice.cpp").read_bytes()

    assert "ds_test_first_stream_fault" not in (PORT/"build/entry/entry_slice.cpp").read_text()
    candidate=PORT/"demons-souls-loader.nro";binary=candidate.read_bytes()
    assert binary[0x10:0x14]==b"NRO0" and b"startup v0.25.0" in binary
    assert binary==(PORT/"sdcard/switch/demons-souls-loader/demons-souls-loader.nro").read_bytes()
    manifest=json.loads((PORT/"sdcard/manifest.json").read_text())
    assert manifest["version"]=="0.25.0" and manifest["device_tested"] is False
    with zipfile.ZipFile(PORT/"demons-souls-switch-startup.zip") as archive:
        assert archive.testzip() is None
        for item in manifest["files"]:
            assert hashlib.sha256(archive.read(item["path"])).hexdigest()==item["sha256"]
        assert archive.read("switch/demons-souls-loader/demons-souls-loader.nro")==binary
    target=HERE/"candidate.nro"
    assert not target.exists(),"Refuse to replace a sealed candidate"
    shutil.copy2(candidate,target)
    for name,source in logs.items():shutil.copy2(source,HERE/name)
    shutil.copy2("/tmp/first-stream-switch-build.log",HERE/"switch-build.log")
    shutil.copy2("/tmp/first-stream-lifter-tests.log",HERE/"lifter-tests.log")
    shutil.copy2(PORT/"build/entry/entry-provenance.json",HERE/"entry-provenance.json")
    (HERE/"expected-device-trace.json").write_text(json.dumps({"functions":expected},indent=2)+"\n")
    sources=["Makefile","source/main.c","entry/ds_entry.cpp","entry/ds_entry.h","entry/generate_entry.py",
             "entry/ds_heap.c","entry/ds_heap.h","entry/ds_sync.cpp","source/ds_image.c","include/ds_image.h",
             "tests/test_first_stream.cpp","tests/test_first_facet.cpp","tests/test_stream_locale.cpp","tests/test_published.cpp","tests/test_locale.cpp",
             "tests/test_allocated.cpp","tests/test_entry.cpp","tests/run_entry_tests.sh","tools/prepare_sd.py"]
    files=[PORT/p for p in sources]+[Path(__file__),HERE/"validate_device.py",HERE.parent/"FIRST_STREAM_AUDIT.md"]
    result=dict(version="0.25.0",size=len(binary),sha256=digest(target),constructors_completed=12,
                constructor_thirteen_started=True,function_entries=810,checked_accesses=CHECKED_ACCESSES,pinned_bodies=215,
                runtime_allocations_served=[40,4,52,40,4,4,24,8],
                runtime_guest_addresses=["0x60000000","0x60000030","0x60000040","0x60000080","0x600000B0","0x600000C0","0x600000D0","0x600000F0"],
                runtime_requested_bytes=176,runtime_live_bytes=256,runtime_backed_bytes=65536,
                locale_name="C",locale_references=5,tls_locale_published=True,
                guest_locks=26,guest_unlocks=26,thread_id_calls=15,
                first_stream_address="0x01CE65D0",first_stream_registration=1,first_stream_fill=32,
                stream_registry_address="0x01CE6488",stream_registry_references=1,
                stream_registry_slot="0x01CE65D0",next_runtime_request_bytes=4,native_locks_held_at_stop=0,
                sanitizer_checks=checks,sanitizer_checks_total=sum(checks.values()),compiled_lifter_link_checks=9,
                source_sha256={str(p.relative_to(ROOT)):digest(p) for p in files},
                evidence_sha256={p.name:digest(p) for p in sorted(HERE.iterdir()) if p.is_file() and p.name!="build-verification.json"},
                device_execution_passed=False)
    (HERE/"build-verification.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({k:v for k,v in result.items() if k not in ("source_sha256","evidence_sha256")},indent=2))


if __name__=="__main__":main()
