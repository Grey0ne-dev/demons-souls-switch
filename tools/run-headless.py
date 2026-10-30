#!/usr/bin/env python3
"""Bounded PC boot observation, with isolated saves and durable milestone logs."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "upstream/DemonsSoulsRecomp"


def stop_child(child):
    if child.poll() is not None:
        return
    try:
        os.killpg(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        child.wait(timeout=5)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        child.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=120)
    parser.add_argument("--stop-at", choices=("save-load", "trophies", "timeout"), default="save-load")
    args = parser.parse_args()
    if args.seconds < 1:
        parser.error("--seconds must be positive")
    executable = SOURCE / "port/build-headless/des_boot"
    if not executable.is_file():
        raise SystemExit("Build des_boot first with bash tools/build-headless.sh")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    folder = ROOT / "analysis/runs" / stamp
    folder.mkdir(parents=True)
    environment = os.environ.copy()
    for key in ("EXIT_ON", "EXIT_ON_HLE", "EXIT_AFTER_MS", "EXIT_DELAY_MS", "DES_AUTO_CONTINUE"):
        environment.pop(key, None)
    environment.update({
        "PS3_VFS_ROOT": str(ROOT / "gamedata"),
        "PS3_HDD0_ROOT": str(folder / "hdd0"),
        "PS3_SAVEDATA_ROOT": str(folder / "saves"),
        "PS3_HDD1_ROOT": str(folder / "hdd1"),
        "PS3_TROPHY_DIR": str(folder / "trophies"),
        "PS3_SHADER_CACHE": str(folder / "shader-cache"),
        "PS3_MILESTONE_OUT": str(folder / "milestones.tsv"),
        "DES_FRAMELOG": str(folder / "frames.tsv"),
        "PS3_NO_USER_INPUT": "1", "PS3_VERBOSE": "0",
        "SDL_AUDIODRIVER": "dummy", "AUDIO_MUTE": "1",
        "DES_UI": "0", "DES_ONLINE": "0", "DES_SKIP_INTRO": "1",
        "DES_SKIP_INTRO_MOVIE": "1", "DES_PRECOMPILE": "0",
        "LIBSRE_LLE": "1", "SPURS_NO_ROUND_NOTIFY": "1",
        "LIBSRE_LLE_ONLY": "011EE38B,1656D49F,8A85674D,73E06F91,082BFB09,E5443BE7,039D70B7,91066667",
    })
    if args.stop_at != "timeout":
        environment["EXIT_ON_HLE"] = "FBD5C856" if args.stop_at == "save-load" else "1197B52C"
        environment["EXIT_DELAY_MS"] = "1000"
    if args.stop_at == "trophies":
        environment["DES_AUTO_CONTINUE"] = "1"
    for key in ("PS3_HDD0_ROOT", "PS3_SAVEDATA_ROOT", "PS3_HDD1_ROOT", "PS3_TROPHY_DIR", "PS3_SHADER_CACHE"):
        Path(environment[key]).mkdir()
    command = [str(executable), str(SOURCE / "work/game/EBOOT.ELF")]
    print(f"Observing PC boot for up to {args.seconds}s; logs: {folder}", flush=True)
    start = time.monotonic()
    timed_out = False
    with (folder / "stdout.log").open("w") as log:
        child = subprocess.Popen(command, cwd=SOURCE, env=environment,
                                 stdin=subprocess.DEVNULL, stdout=log,
                                 stderr=subprocess.STDOUT, start_new_session=True)
        try:
            child.wait(timeout=args.seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
        finally:
            stop_child(child)
    facts = {}
    events = []
    milestones = folder / "milestones.tsv"
    if milestones.is_file():
        for line in milestones.read_text().splitlines():
            parts = line.split("\t")
            if len(parts) == 3 and parts[0] == "kv":
                facts[parts[1]] = parts[2]
            elif len(parts) == 2 and parts[0].isdigit():
                events.append(parts[1])
    output = (folder / "stdout.log").read_text(errors="replace")
    fatal_markers = ("entry function unwound", "[ppu] unresolved indirect call", "Assertion failed", "[PANIC]")
    fatal_lines = [line for line in output.splitlines() if any(marker in line for marker in fatal_markers)]
    target_nid = {"save-load": "FBD5C856", "trophies": "1197B52C"}.get(args.stop_at)
    checks = {
        "firmware_loaded": "[libsre] LLE load: ok=1" in output,
        "entry_dispatched": "[boot] dispatching entry OPD" in output,
        "entry_milestone": "boot:entry" in events,
        "target_trigger": bool(target_nid and f"[exit-on] first call of 0x{target_nid}" in output),
        "no_known_fatal_markers": not fatal_lines,
    }
    target_reached = bool(target_nid and all(checks.values()) and not timed_out and child.returncode == 0)
    summary = {
        "platform": "Linux PC, null renderer", "command": command,
        "returncode": child.returncode, "timed_out": timed_out, "stop_at": args.stop_at,
        "elapsed_seconds": round(time.monotonic() - start, 2),
        "facts": facts, "events_observed": events,
        "checks": checks, "target_reached": target_reached,
        "fatal_diagnostics": fatal_lines,
        "note": "Milestones show execution progress; they do not establish visible rendering, gameplay, or Switch support.",
    }
    (folder / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Exit: {child.returncode}; timeout: {timed_out}; milestones: {len(events)}; target reached: {target_reached}")
    print(f"Summary: {folder / 'summary.json'}")
    return 124 if timed_out else int(not target_reached if target_nid else child.returncode != 0)


if __name__ == "__main__":
    raise SystemExit(main())
