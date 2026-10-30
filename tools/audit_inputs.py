#!/usr/bin/env python3
"""Audit extracted inputs against the pinned local upstream's reference hashes.

Run from any directory. Writes reports only; never modifies game inputs.
"""
import argparse
from collections import Counter
from dataclasses import asdict
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "upstream/DemonsSoulsRecomp"
sys.path.insert(0, str(UPSTREAM / "tools/setup"))
from des_setup import gamefiles, sfo


def commit(path):
    return subprocess.check_output(
        ["git", "-C", str(path), "rev-parse", "HEAD"], text=True
    ).strip()


def inventory(root, listing):
    """Compare all listed PS3_GAME paths and sizes, independently of known hashes."""
    expected = {}
    for block in listing.read_text().split("\n\n"):
        fields = dict(line.split(" = ", 1) for line in block.splitlines() if " = " in line)
        name = fields.get("Path", "")
        if name.startswith("PS3_GAME/") and fields.get("Folder") == "-":
            expected[name] = int(fields["Size"])
    if not expected:
        raise ValueError("No game files found in ISO listing")
    missing, wrong_size = [], []
    for name, size in expected.items():
        path = root / name
        if not path.is_file():
            missing.append(name)
        elif path.stat().st_size != size:
            wrong_size.append(name)
    return {"listed_files": len(expected), "listed_bytes": sum(expected.values()),
            "missing": missing, "wrong_size": wrong_size,
            "note": "Sizes and presence checked for all files; only upstream reference files are hashed."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, default=ROOT / "gamedata")
    parser.add_argument("--decrypted", type=Path, action="append", default=[])
    parser.add_argument("--firmware", type=Path)
    args = parser.parse_args()
    report = gamefiles.check(args.game, args.decrypted, args.firmware)
    local_tools = {
        "cmake": ROOT / ".tools/cmake-4.4.4-linux-x86_64/bin/cmake",
        "ninja": ROOT / ".tools/bin/ninja",
        "rpcs3": ROOT / ".tools/rpcs3/AppDir/AppRun",
        "aarch64-none-elf-gcc": ROOT / ".tools/switch/root/opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc",
    }
    tool_paths = {}
    for tool in ("clang", "cmake", "ninja", "rpcs3", "aarch64-none-elf-gcc"):
        local = local_tools.get(tool)
        tool_paths[tool] = shutil.which(tool) or (str(local) if local and local.is_file() else None)
    output = {
        "upstream_commit": commit(UPSTREAM),
        "runtime_commit": commit(UPSTREAM / "ps3recomp"),
        "reference_inputs_ready": report.ok,
        "items": [asdict(item) for item in report.items],
        "tools": tool_paths,
        "switch_build_ready": False,
        "note": "Input readiness does not establish that the PC runtime works on Switch.",
    }
    if report.root:
        output["metadata"] = sfo.parse((report.root / "PS3_GAME/PARAM.SFO").read_bytes())
        output["inventory"] = inventory(report.root, ROOT / "analysis/iso9660-listing.txt")
        eboot = (report.root / "PS3_GAME/USRDIR/EBOOT.BIN").read_bytes()
        if eboot[:4] == b"SCE\0":
            elf_offset = struct.unpack_from(">Q", eboot, 48)[0]
            fields = struct.unpack_from(">16sHHIQQQIHHHHHH", eboot, elf_offset)
            output["executable"] = {
                "container": "SELF (not a directly loadable ELF)",
                "elf_class": fields[0][4], "elf_endianness": fields[0][5],
                "elf_machine": fields[2], "elf_entry_descriptor": hex(fields[4]),
                "program_headers": fields[10], "section_headers": fields[12],
            }
    # Firmware is optional to upstream.check; make its absence explicit here.
    if args.firmware is None:
        output["firmware_status"] = "Not supplied; the current upstream build documents a decrypted libsre.prx dependency."
        output["reference_inputs_ready"] = False
    target = ROOT / "analysis/input-audit.json"
    target.write_text(json.dumps(output, indent=2) + "\n")
    counts = Counter((item.kind, item.state) for item in report.items)
    for (kind, state), count in sorted(counts.items()):
        print(f"{kind}: {state}: {count}")
    if "inventory" in output:
        inv = output["inventory"]
        print(f"Inventory: {inv['listed_files']} files; {len(inv['missing'])} missing; {len(inv['wrong_size'])} size mismatches")
    print(output.get("firmware_status", "Firmware check included"))
    print(f"Report: {target}")
    inventory_ok = "inventory" in output and not output["inventory"]["missing"] and not output["inventory"]["wrong_size"]
    return 0 if output["reference_inputs_ready"] and inventory_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
