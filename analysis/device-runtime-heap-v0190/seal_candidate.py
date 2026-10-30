#!/usr/bin/env python3
"""Freeze the local v0.19 runtime-prefix candidate after verification."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
PORT = ROOT / "switch-port"
SUFFIX = [
    0x15166F0,0x1516590,0x1686CE8,0x166F990,0x1676150,0x16BB2E4,
    0x16BB244,0x166F910,0x1676108,0x16BB264,0x16A0500,
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    prefix = json.loads((HERE.parent / "device-twelve-constructors-v0180/expected-device-trace.json").read_text())["functions"]
    assert len(prefix) == 641 and len(SUFFIX) == 11
    expected = prefix + [f"0x{address:08X}" for address in SUFFIX]
    logs = {
        "regression-tests.log": "/tmp/runtime-regression.log",
        "runtime-heap-tests.log": "/tmp/runtime-heap-tests.log",
        "runtime-state-fault-tests.log": "/tmp/runtime-state-fault.log",
        "runtime-lock-fault-tests.log": "/tmp/runtime-lock-fault.log",
        "runtime-argument-fault-tests.log": "/tmp/runtime-argument-fault.log",
        "twelfth-regression-tests.log": "/tmp/runtime-twelfth-regression.log",
        "dispatch-equivalence-tests.log": "/tmp/runtime-dispatch-regression.log",
        "older-checkpoint-tests.log": "/tmp/runtime-older-checkpoints.log",
    }
    checks = {}
    for name, source in logs.items():
        text = Path(source).read_text()
        results = re.findall(r"(\d+) checks, (\d+) failures", text)
        assert results and all(int(failures) == 0 for _, failures in results), name
        assert not any(marker in text for marker in ("FAIL ", "LeakSanitizer", "ERROR: AddressSanitizer", "runtime error:")), name
        checks[name] = sum(int(count) for count, _ in results)
    probe = Path(logs["runtime-heap-tests.log"]).read_text()
    observed = {int(i): int(address,16) for i,address in re.findall(r"^(\d+): ([0-9a-f]{8})$",probe,re.M)}
    assert observed == {i:int(expected[i],16) for i in range(641,652)}
    assert "RUNTIME ok=1 stop=29 callee=0168abd0 functions=652 accesses=12206 completed=12" in probe
    provenance = json.loads((PORT / "build/entry/entry-provenance.json").read_text())
    assert len(provenance["functions"]) == 190
    assert "015166F0" in provenance["functions"] and "0168ABD0" not in provenance["functions"]
    previous = json.loads((HERE.parent / "device-twelve-constructors-v0180/entry-provenance.json").read_text())
    assert all(provenance["functions"].get(k)==v for k,v in previous["functions"].items())
    assert sum(len(v) for v in provenance["original_bctrl_return_pcs"].values())==60
    candidate = PORT / "demons-souls-loader.nro"
    assert candidate.read_bytes()[0x10:0x14] == b"NRO0"
    assert b"startup v0.19.0" in candidate.read_bytes()
    assert candidate.read_bytes() == (PORT / "sdcard/switch/demons-souls-loader/demons-souls-loader.nro").read_bytes()
    with zipfile.ZipFile(PORT / "demons-souls-switch-startup.zip") as archive:
        assert archive.testzip() is None
        assert archive.read("switch/demons-souls-loader/demons-souls-loader.nro") == candidate.read_bytes()
    target = HERE / "candidate.nro"
    if target.exists():
        assert target.read_bytes() == candidate.read_bytes(), "Refuse to overwrite a different sealed candidate"
    else:
        shutil.copy2(candidate,target)
    for name, source in logs.items():
        shutil.copy2(source,HERE / name)
    shutil.copy2("/tmp/runtime-switch-build.log", HERE / "switch-build.log")
    shutil.copy2(PORT / "build/entry/entry-provenance.json",HERE / "entry-provenance.json")
    (HERE / "expected-device-trace.json").write_text(json.dumps({"functions":expected},indent=2)+"\n")
    sources = [PORT / "Makefile",PORT / "source/main.c",PORT / "entry/ds_entry.cpp",
               PORT / "entry/ds_entry.h",PORT / "entry/generate_entry.py",
               PORT / "tests/test_entry.cpp",PORT / "tests/run_entry_tests.sh",
               PORT / "tools/prepare_sd.py",Path(__file__),HERE / "validate_device.py"]
    result = {
        "version":"0.19.0", "size":target.stat().st_size,"sha256":digest(target),
        "constructors_completed":12,"constructor_thirteen_started":True,
        "function_entries":652,"checked_accesses":12206,
        "runtime_allocation_request_bytes":40,"runtime_allocation_served":False,
        "names":10,"opaque_key_pairs":40,"pinned_bodies":190,
        "sanitizer_checks":checks,"sanitizer_checks_total":sum(checks.values()),
        "compiled_lifter_link_checks":9,
        "source_sha256":{str(p.relative_to(ROOT)):digest(p) for p in sources},
        "evidence_sha256":{p.name:digest(p) for p in sorted(HERE.iterdir()) if p.is_file() and p.name!="build-verification.json"},
        "device_execution_passed":False,
    }
    (HERE / "build-verification.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({k:v for k,v in result.items() if k not in ("source_sha256","evidence_sha256")},indent=2))


if __name__ == "__main__":
    main()
