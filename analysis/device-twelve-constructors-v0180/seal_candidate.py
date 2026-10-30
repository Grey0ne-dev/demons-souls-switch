#!/usr/bin/env python3
"""Freeze the local v0.18 candidate only after host/build/package checks."""
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
    0x3F46B0,0x3F44E0,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,
    0xAE4D38,0xAE2148,0xADEBA0,0x3F3F10,0x3F3F18,0x10BA0,0x16693E0,
    0xADCAD8,0xADCAD8,0xADCAD8,0xADCAB0,0xADCAB0,0xAE4490,
    0x3F3F30,0x3F3F48,0x3F3F60,0x3F3F78,
    0xAE47A0,0xAE47A0,0xAE47A0,0xAE47A0,0xAE0BE8,0xADF198,0xAE0BE8,
    0xCE3DF0,0xB0AF28,0xAE7768,0xB25E58,0xA34BB8,0xA34C48,0xADE478,
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    prefix = json.loads((HERE.parent / "device-eleven-constructors-v0170/expected-device-trace.json").read_text())["functions"]
    assert len(prefix) == 603 and len(SUFFIX) == 38
    expected = prefix + [f"0x{address:08X}" for address in SUFFIX]
    logs = {
        "regression-tests.log": "/tmp/ctor12-regression.log",
        "twelfth-constructor-tests.log": "/tmp/ctor12-validation.log",
        "twelfth-state-fault-tests.log": "/tmp/ctor12-state-fault.log",
        "twelfth-key-fault-tests.log": "/tmp/ctor12-key-fault.log",
        "twelfth-sort-fault-tests.log": "/tmp/ctor12-sort-fault.log",
        "eleventh-regression-tests.log": "/tmp/ctor12-eleventh-regression.log",
        "dispatch-equivalence-tests.log": "/tmp/ctor12-dispatch-regression.log",
        "older-checkpoint-tests.log": "/tmp/ctor12-older-checkpoints.log",
        "unsupported-thirteenth-tests.log": "/tmp/ctor12-unsupported-thirteenth.log",
    }
    checks = {}
    for name, source in logs.items():
        text = Path(source).read_text()
        results = re.findall(r"(\d+) checks, (\d+) failures", text)
        assert results and all(int(failures) == 0 for _, failures in results), name
        assert not any(marker in text for marker in ("FAIL ", "LeakSanitizer", "ERROR: AddressSanitizer", "runtime error:")), name
        checks[name] = sum(int(count) for count, _ in results)
    probe = Path(logs["twelfth-constructor-tests.log"]).read_text()
    observed = {int(i): int(address,16) for i,address in re.findall(r"^(\d+): ([0-9a-f]{8})$",probe,re.M)}
    assert observed == {i:int(expected[i],16) for i in range(137,641)}
    frontier = Path(logs["unsupported-thirteenth-tests.log"]).read_text()
    assert "stop=3 callee=015166f0" in frontier and "ctors=12" in frontier
    assert "Unimplemented lifted indirect target 0x015166F0" in frontier
    provenance = json.loads((PORT / "build/entry/entry-provenance.json").read_text())
    assert len(provenance["functions"]) == 186
    assert "015166F0" not in provenance["functions"]
    candidate = PORT / "demons-souls-loader.nro"
    assert candidate.read_bytes()[0x10:0x14] == b"NRO0"
    assert b"startup v0.18.0" in candidate.read_bytes()
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
    shutil.copy2("/tmp/ctor12-switch-build.log", HERE / "switch-build.log")
    shutil.copy2(PORT / "build/entry/entry-provenance.json",HERE / "entry-provenance.json")
    (HERE / "expected-device-trace.json").write_text(json.dumps({"functions":expected},indent=2)+"\n")
    sources = [PORT / "Makefile",PORT / "source/main.c",PORT / "entry/ds_entry.cpp",
               PORT / "entry/ds_entry.h",PORT / "entry/generate_entry.py",
               PORT / "tests/test_entry.cpp",PORT / "tests/run_entry_tests.sh",
               PORT / "tools/prepare_sd.py",Path(__file__),HERE / "validate_device.py"]
    result = {
        "version":"0.18.0", "size":target.stat().st_size,"sha256":digest(target),
        "constructors_completed":12,"function_entries":641,"checked_accesses":12134,
        "names":10,"opaque_key_pairs":40,"pinned_bodies":186,
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
