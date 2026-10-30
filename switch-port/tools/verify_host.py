#!/usr/bin/env python3
"""Load the real ELF through the portable C core and compare its memory image."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


class Segment(ctypes.Structure):
    _fields_ = [("guest_address", ctypes.c_uint32), ("flags", ctypes.c_uint32),
                ("file_offset", ctypes.c_uint64), ("file_size", ctypes.c_size_t),
                ("memory_size", ctypes.c_size_t), ("data", ctypes.c_void_p),
                ("from_elf", ctypes.c_bool)]


class Image(ctypes.Structure):
    _fields_ = [("segments", Segment * 16), ("segment_count", ctypes.c_size_t),
                ("allocated_bytes", ctypes.c_size_t), ("memory_limit", ctypes.c_size_t),
                ("entry_descriptor", ctypes.c_uint32), ("entry_code", ctypes.c_uint32),
                ("toc", ctypes.c_uint32), ("tls_address", ctypes.c_uint32),
                ("tls_file_size", ctypes.c_size_t), ("tls_memory_size", ctypes.c_size_t)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF")
    parser.add_argument("--output", type=Path, default=ROOT / "analysis/switch-host-image.json")
    args = parser.parse_args()
    reference = json.loads((ROOT / "switch-port/reference.json").read_text())
    with args.input.open("rb") as stream:
        file_hash = hashlib.file_digest(stream, "sha256").hexdigest()
    if file_hash != reference["file_sha256"]:
        raise SystemExit("Input does not match verified BLUS30443 1.00 EBOOT.ELF")
    build = ROOT / "switch-port/build-host-tests"
    build.mkdir(exist_ok=True)
    library = build / "libds_image.so"
    subprocess.run(["clang", "-std=c17", "-O2", "-fPIC", "-shared", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "switch-port/include"), str(ROOT / "switch-port/source/ds_image.c"),
                    "-o", str(library)], check=True)
    core = ctypes.CDLL(str(library))
    core.ds_image_load_file.argtypes = [ctypes.POINTER(Image), ctypes.c_char_p, ctypes.c_size_t,
                                        ctypes.c_char_p, ctypes.c_size_t]
    core.ds_image_load_file.restype = ctypes.c_bool
    core.ds_image_destroy.argtypes = [ctypes.POINTER(Image)]
    core.ds_image_destroy.restype = None
    image = Image()
    error = ctypes.create_string_buffer(256)
    if not core.ds_image_load_file(ctypes.byref(image), str(args.input).encode(),
                                   reference["allocated_bytes"], error, len(error)):
        raise SystemExit(error.value.decode())
    try:
        segments = []
        for index in range(image.segment_count):
            segment = image.segments[index]
            memory = ctypes.string_at(segment.data, segment.memory_size)
            segments.append({"guest_address": segment.guest_address, "flags": segment.flags,
                             "file_offset": segment.file_offset, "file_size": segment.file_size,
                             "memory_size": segment.memory_size,
                             "file_sha256": hashlib.sha256(memory[:segment.file_size]).hexdigest(),
                             "memory_sha256": hashlib.sha256(memory).hexdigest(),
                             "bss_zero": not any(memory[segment.file_size:])})
        checks = {
            "file_identity": args.input.stat().st_size == reference["file_size"],
            "segment_count": image.segment_count == len(reference["segments"]),
            "allocated_bytes": image.allocated_bytes == reference["allocated_bytes"],
            "entry_descriptor": image.entry_descriptor == reference["entry_descriptor"],
            "entry_code": image.entry_code == reference["entry_code"],
            "toc": image.toc == reference["toc"],
            "tls": image.tls_address == reference["tls"]["guest_address"] and
                   image.tls_file_size == reference["tls"]["file_size"] and
                   image.tls_memory_size == reference["tls"]["memory_size"],
        }
        for index, segment in enumerate(segments):
            ref = reference["segments"][index]
            checks[f"segment_{index}"] = all(segment[key] == ref[key] for key in
                ("guest_address", "flags", "file_offset", "file_size", "memory_size",
                 "file_sha256", "memory_sha256")) and segment["bss_zero"]
        result = {"platform": "native Linux host, shared portable C image core", "input": str(args.input),
                  "file_sha256": file_hash, "allocated_bytes": image.allocated_bytes,
                  "entry_descriptor": image.entry_descriptor, "entry_code": image.entry_code,
                  "toc": image.toc, "segments": segments, "checks": checks,
                  "reference_checks_passed": all(checks.values()), "device_run": False,
                  "scope": "ELF segments/BSS/entry image validation only; no guest code executed."}
    finally:
        core.ds_image_destroy(ctypes.byref(image))
    result["cleanup"] = image.segment_count == 0 and image.allocated_bytes == 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f'Host memory image: {len(segments)} segments, {result["allocated_bytes"]} bytes; checks={result["reference_checks_passed"]}; cleanup={result["cleanup"]}')
    print(args.output)
    return 0 if result["reference_checks_passed"] and result["cleanup"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
