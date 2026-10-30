#!/usr/bin/env python3
"""Validate the preserved v0.14 device evidence against frozen expectations."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
raw = (HERE / "loader.log").read_bytes()
log = raw.decode()
reference = json.loads((ROOT / "switch-port/reference.json").read_text())
expected = json.loads((HERE / "expected-device-trace.json").read_text())
checks = 0
mismatches = []


def check(condition, description):
    global checks
    checks += 1
    if not condition:
        mismatches.append(description)


def fields(block, label, wanted):
    line = re.search(r"^" + re.escape(label) + r"(.*)$", block, re.M)
    check(line is not None, label + " present")
    values = {} if line is None else {
        k: int(v, 16 if v.startswith("0x") else 10)
        for k, v in re.findall(r"(\w+)=(0x[0-9a-fA-F]+|\d+)", line[1])
    }
    for key, value in wanted.items():
        check(values.get(key) == value, f"{label} {key}: {values.get(key)} != {value}")
    return values


check(log.startswith("Demon's Souls Switch startup v0.14.0;"), "v0.14 log header")
parts = re.split(r"^Attempt (\d+) memory_limit=(\d+)\n", log, flags=re.M)
check(len(parts) == 7, "exactly two device attempts")
attempts = []
for offset in range(1, len(parts), 3):
    number, cap, block = int(parts[offset]), int(parts[offset+1]), parts[offset+2]
    start_errors = len(mismatches)
    check(number == len(attempts)+1, "sequential attempt number")
    check(cap == 48*1024*1024, "configured memory cap")
    check("EBOOT file SHA256: " + reference["file_sha256"] in block, "exact input ELF hash")
    for i, segment in enumerate(reference["segments"]):
        header = (f"Segment {i} guest=0x{segment['guest_address']:08x} "
                  f"file={segment['file_size']} backed={segment['memory_size']} flags=0x{segment['flags']:08x}")
        check(header in block, f"segment {i} metadata")
        check("  loaded file bytes SHA256: " + segment["file_sha256"] in block, f"segment {i} file hash")
        check("  complete memory SHA256 (including BSS): " + segment["memory_sha256"] in block, f"segment {i} memory hash")
    fields(block, "IMAGE VERIFIED:", dict(descriptor=reference["entry_descriptor"], code=reference["entry_code"], toc=reference["toc"], backed=reference["allocated_bytes"]))
    fields(block, "TLS ", dict(initialized=1, block=0xE000000, r13=0xE007000))
    fields(block, "CRT argv=", dict(envp=0x10000010, toc=reference["toc"], last_syscall=100))
    fields(block, "MEMORY QUERY:", dict(completed=1, total=50331648, available=17825792, backed=31621920, cap=50331648))
    check("CRT mutex records=12\n" in block, "twelve initialized CRT mutex records")
    fields(block, "CONSTRUCTOR:", dict(descriptor=0x1951EA8, target=0x7CF848, toc=reference["toc"], started=1, completed=1))
    fields(block, "SYNC:", dict(thread_id_calls=1, locks=14, unlocks=14, held_at_stop=0, native_live_after_cleanup=0))
    fields(block, "ATEXIT:", dict(free_slots=79, callback=0x1967920))
    fields(block, "SINGLETON:", dict(started=1, completed=1, address=0x1B4C6E8, manager=0x1B91128))
    fields(block, "RWLOCK:", dict(calls=1, handle=1, protocol=2, pshared=0x200, native_live_after_cleanup=0))
    fields(block, "RWLOCK WRITER:", dict(locks=1, unlocks=1, held_at_stop=0))
    fields(block, "MANAGER:", dict(published=1, pool=0x1AD6B78, pool_bytes=0x60000, allocator_setup_executed=1))
    fields(block, "ALLOCATION:", dict(calls=12, address=0x1AD6C00, bytes=384, bootstrap_fallback_calls=0))
    fields(block, "ATOMIC:", dict(attempts=1, successes=1))
    fields(block, "REGISTRY:", dict(initialized=1, address=0x1B92A78, payload=0x1AD6D90, entries=5, allocation_bytes=8192))
    fields(block, "REGISTRATION:", dict(completed=1, key_registry=0x1B92A5C, payload=0x1AD8DA0, entries=20, allocation_bytes=4096))
    fields(block, "KERNEL MUTEX:", dict(calls=4, handle=0x1004, output=0x1AD9F84, protocol=2, recursive=0x10, pshared=0x200, adaptive=0x2000, native_live_after_cleanup=0))
    fields(block, "NODE:", dict(address=0x1AD9DB0, vector=0x1AD9E00, second_singleton=0x1B4BB0C))
    fields(block, "CONSTRUCTORS:", dict(completed=6))
    fields(block, "NEXT CONSTRUCTOR:", dict(descriptor=0x1920778, target=0xBF6E0))
    fields(block, "ENTRY RESULT:", dict(reached=1, stop=24, nid=0x1BC200F4, callee=0xBF6E0, functions=424, accesses=7407))
    check(re.search(r"^ENTRY RESULT:.* error=$", block, re.M) is not None, "empty checkpoint error")
    fields(block, "ENTRY ABI:", dict(r3=0x1B36EB0, r4=0x1B36D60, r5=0, r6=0x1AD8E40, lr=0x16B84F4, sp=0xFEFFE00))
    trace = re.findall(r"^  guest function\[(\d+)\]=(0x[0-9a-f]+)$", block, re.M)
    check(len(trace) == len(expected["functions"]) == 424, "full function trace length")
    for i, (index, address) in enumerate(trace):
        check(int(index) == i, f"function index {i}")
        check(i < len(expected["functions"]) and int(address, 16) == int(expected["functions"][i], 16), f"function address {i}")
    check("memory fault address=" not in block and "LOAD FAILED" not in block, "no reported fault")
    clock = fields(block, "CLOCK:", dict(calls=1, frequency=19200000))
    check(clock.get("microseconds") == clock.get("ticks", -1)*1000000//19200000, "exact hardware clock conversion")
    attempts.append(dict(attempt=number, passed=len(mismatches)==start_errors,
                         constructors_completed=6, function_entries=len(trace), checked_accesses=7407,
                         clock_ticks=clock.get("ticks"), clock_microseconds=clock.get("microseconds")))

check(len(attempts) == 2 and all(a["passed"] for a in attempts), "both attempts pass")
if len(attempts) == 2:
    check(attempts[1]["clock_ticks"] > attempts[0]["clock_ticks"], "hardware clock advances across reload")
check(log.rstrip().endswith("Cleanup complete: backed bytes=0"), "complete guest backing release at exit")
result = dict(version="0.14.0", source="DBI MTP device log", passed=not mismatches,
              validation_checks=checks, validation_mismatches=mismatches,
              log_sha256=hashlib.sha256(raw).hexdigest(), attempts=attempts,
              reload_observed=len(attempts)==2, cleanup_backed_bytes=0 if not mismatches else None,
              scope="Six static constructors only; no graphics or gameplay execution",
              evidence_limits="Object and method memory is checked by the on-device checkpoint validator; this log independently exposes summaries, the full function trace and final cleanup.")
result["provenance"] = {
    "validator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    "expected_trace_sha256": hashlib.sha256((HERE / "expected-device-trace.json").read_bytes()).hexdigest(),
    "reference_manifest_sha256": hashlib.sha256((ROOT / "switch-port/reference.json").read_bytes()).hexdigest(),
    "installed_candidate_sha256": hashlib.sha256((HERE / "candidate.nro").read_bytes()).hexdigest(),
}
(HERE / "device-result.json").write_text(json.dumps(result, indent=2)+"\n")
print(json.dumps(result, indent=2))
raise SystemExit(bool(mismatches))
