#!/usr/bin/env python3
"""Stage copies, run local RPCS3, and independently verify decrypted outputs."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "upstream/DemonsSoulsRecomp"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    table = json.loads((UPSTREAM / "tools/setup/des_setup/game_table.json").read_text())
    stage = ROOT / "work/decrypted"
    command = ["bash", str(ROOT / "tools/rpcs3-local.sh")]
    for item in table["decrypted"]:
        source = ROOT / "gamedata" / item["of"]
        target = stage / item["of"]
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(source, target)
        # Verify the staging copy against the original before executing tools.
        if digest(source) != digest(target):
            raise SystemExit(f"Staging copy differs: {target}")
        output = stage / item["path"]
        if output.is_file() and item.get("sha256") and digest(output) == item["sha256"]:
            continue
        command.extend(["--decrypt", str(target)])
    log = ROOT / "analysis/decryption.log"
    if len(command) > 2:
        with log.open("w") as stream:
            result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=180)
        print(f"RPCS3 exit status: {result.returncode}; output verification follows")
    results = []
    for item in table["decrypted"]:
        output = stage / item["path"]
        actual = digest(output) if output.is_file() else None
        size = output.stat().st_size if output.is_file() else None
        expected = item.get("sha256")
        good = bool(actual and expected and actual == expected and size == item["size"])
        results.append({"path": str(output.relative_to(ROOT)), "size": size,
                        "sha256": actual, "expected_sha256": expected, "ok": good})
    report = ROOT / "analysis/decryption-audit.json"
    report.write_text(json.dumps(results, indent=2) + "\n")
    count = sum(item["ok"] for item in results)
    print(f"Verified decrypted inputs: {count}/{len(results)}")
    print(f"Report: {report}")
    if count != len(results):
        raise SystemExit(1)
    destination = UPSTREAM / "work/game/EBOOT.ELF"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(stage / "PS3_GAME/USRDIR/EBOOT.elf", destination)
    shutil.copy2(ROOT / "gamedata/PS3_GAME/ICON0.PNG", destination.parent / "ICON0.PNG")
    print(f"Executable staged for the lift: {destination}")


if __name__ == "__main__":
    main()
