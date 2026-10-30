#!/usr/bin/env python3
"""Stage this workspace's verified game input and NRO into an SD-card folder."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PORT = ROOT / "switch-port"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=ROOT / "upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF")
    args = parser.parse_args()
    reference = json.loads((PORT / "reference.json").read_text())
    if args.elf.stat().st_size != reference["file_size"] or digest(args.elf) != reference["file_sha256"]:
        raise SystemExit("Input ELF does not match the prepared US v1.00 reference")
    nro = PORT / "demons-souls-loader.nro"
    binary = nro.read_bytes()
    if binary[0x10:0x14] != b"NRO0":
        raise SystemExit("Build the Switch NRO first")
    length = struct.unpack_from("<I", binary, 0x18)[0]
    if length > len(binary) or binary[length:length + 4] != b"ASET":
        raise SystemExit("NRO asset header is invalid")
    folder = PORT / "sdcard/switch/demons-souls-loader"
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copy2(nro, folder / nro.name)
    shutil.copy2(args.elf, folder / "EBOOT.ELF")
    instructions = """Demon's Souls Switch port: startup development build v0.27.0

Copy the switch folder to the SD card root, then open Demon's Souls Loader in
Homebrew Menu. The ELF is your existing game input prepared in this workspace.

This build verifies the actual executable and runs 1,265 function entries across
223 pinned translated functions. Thirteen real startup constructors complete.
Four narrow and four wide C++ streams initialize and register. The wide fill
character is produced by the game's original conversion code and TLS state.
Execution stops before constructor fourteen (0x00745248).

Twenty-five exact operator-new contexts use the same backed 64 KiB arena:
420 requested / 672 rounded bytes. Guest backing remains about 30.22 MiB.
Eighteen real CRT mutex records initialize; all 66 locks release normally.
Native and guest backing are released on exit or failure. This stage needs no
launch/configuration changes. No game graphics, menus or gameplay run yet.
Guest threads, waiting, stream I/O and general allocation services remain absent.

A reloads the image. Test the checkpoint once, press A for a fresh second pass,
then + to exit. Reopen DBI MTP so the log can be verified.
Log: /switch/demons-souls-loader/loader.log
"""
    (PORT / "sdcard/README.txt").write_text(instructions)
    manifest = {
        "scope": "Actual game image loading and bounded startup development; no gameplay implementation",
        "contains_locally_game_derived_startup_code": True,
        "version": "0.27.0",
        "startup_boundary": "thirteen constructors completed; all eight streams initialized, before constructor fourteen 0x00745248",
        "files": [{"path": "switch/demons-souls-loader/" + name,
                   "size": (folder / name).stat().st_size, "sha256": digest(folder / name)}
                  for name in (nro.name, "EBOOT.ELF")],
        "device_tested": False,
    }
    (PORT / "sdcard/manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    archive = PORT / "demons-souls-switch-startup.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for path in sorted((PORT / "sdcard").rglob("*")):
            if path.is_file() and path.name in ("README.txt", "manifest.json", nro.name, "EBOOT.ELF"):
                output.write(path, path.relative_to(PORT / "sdcard"))
    with zipfile.ZipFile(archive) as output:
        if output.testzip() is not None:
            raise SystemExit("Archive CRC verification failed")
        for item in manifest["files"]:
            if hashlib.sha256(output.read(item["path"])).hexdigest() != item["sha256"]:
                raise SystemExit("Archive content verification failed")
    print(f"Prepared: {archive} ({archive.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
