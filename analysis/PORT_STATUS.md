# Demon's Souls / Atmosphere investigation

Target: a native Switch homebrew port. v0.27 completes thirteen startup
constructors, including all eight narrow/wide stream objects and original
wide-character conversion. It stops before constructor14 (00745248), at1,265
entries/16,590 checked accesses. All9,926 sanitizer checks, source/ELF model,
nine compiled lifter cases and strict Switch build/package checks pass.
v0.27 is installed with exact530,629-byte readback; hardware execution is
pending. Last confirmed hardware pass remains v0.26. Sealed sources/snapshots
and evidence are intact; MTP released. Graphics and gameplay remain absent.
Current detail:SWITCH_WIDE_STREAMS_STATUS.md and ../switch-port/README.md.
Notes below describe earlier stages.

This workspace contains extracted/decrypted inputs, locally translated game code, a compiled PC headless executable, and a passing real-game initialization smoke test.

## Verified locally

- Release: BLUS30443, APP_VER 01.00, VERSION 01.00.
- Extracted 12,510 files (8,447,322,552 bytes) to `gamedata/PS3_GAME`.
- Every extracted file matches the ISO9660 directory's recorded size and path.
- All 28 original files in upstream's reference hash table match SHA-256.
  This is not a whole-disc hash validation.
- EBOOT.BIN is a retail SELF containing an ELF64 big-endian PowerPC header:
  machine 21, entry descriptor 0x01916138, 8 program headers, 32 section headers.
  The original payload is encrypted. A separate decrypted EBOOT.elf now exactly matches the reference SHA-256.
- Sample model and texture data have recognizable FLVER and TPF headers.
- The default 7-Zip UDF reader reported a header error. Explicit `-tIso`
  extraction succeeded with exit status 0 and `Everything is Ok`.
- The input audit correctly exits 1 because decrypted build inputs are missing.

## Existing source investigated

[DemonsSoulsRecomp](https://github.com/JonathanDC64/DemonsSoulsRecomp)
has game-specific patches and a PS3 replacement runtime. Its README reports
early PC playability through Boletarian Palace 1-1; that claim has not been
tested here. It targets this exact game release.

- Port checkout: `e99b4f02cd60139c6f795b1209e8c8ba35207376`.
- Runtime checkout: exact declared pin `bae620c0cfa040d5dbc4b3e0bcb5b025576ef5a4` (clean source).
- Sources are under `upstream/DemonsSoulsRecomp`. The port CMake build now probes SSE4.1 support instead of requiring an x86 flag on ARM. Two optional UI/D3D12 dependencies were also fixed for headless linking; normal D3D12/UI paths were retained.

## Inputs and translation completed

- Official RPCS3 0.0.43-20373, CMake 4.4.4 and Ninja 1.13.2 are installed under
  `.tools` only. Their SHA-256 digests match the official GitHub release metadata;
  provenance is in `analysis/tool-downloads.json`.
- RPCS3 decrypted copies of EBOOT and all 26 SDATA scripts. **27/27 match their
  reference hashes**, in `work/decrypted`; originals and ISO are preserved.
- `upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF` is staged for the lift.
- Main PPU translation produced 94,017 lifted function bodies in 14 source chunks
  (429 MB). These are generated instruction translations, not recovered original
  source code or proof of complete functional correctness.
- Generated 1,254 NID handlers and applied the upstream boot allocator patch.
- All fingerprints passed for 12 embedded SPU images and two raw SPURS jobs.
  Fourteen SPU translations and workload registration are ready.
- Sony's official 4.93 PUP was downloaded from the exact link on its HTTPS support
  page. MD5 is `a2b60b6ac1d2e49e230144345616927c`; SHA-256 is
  `158471fd834f8ea8036136b6aab43cd86c7ba73d79ca30e0af3c0fe0001cf365`.
- RPCS3's headless firmware installer extracted `dev_flash` but aborted on teardown.
  Subsequent separate module decryption succeeded, and RPCS3 reports firmware 4.93.

### Firmware compatibility experiment

`work/firmware/libsre.prx` is 239,344 bytes, SHA-256
`9e7ee6edc99b1d372686cf927b883b18f19ed4d32d1faf35e214cca4ae0737b6`.
It **differs** from upstream's reference hash
`115f9df9596dd0b358e5a8c4eaaf77ee44c1f3ec16f9056b3abc517b739140c9`.
The strict input table is unchanged and still rejects it.

The module matches the documented image footprint, TOC, required queue addresses,
267 exports and 32 imports. All eight required LLE NIDs exist. It translated into
756 functions and generated bridge glue. This supported an explicitly experimental
PC boot test, which passed through trophy/save initialization. It does not prove
full firmware equivalence or gameplay compatibility. There were 26 translation
continuation warnings; none directly names the eight required entry functions.
See `analysis/libsre-compatibility.json` and `analysis/firmware-route.md`.

## PC build evidence

- Exact pinned runtime: headless Release build completed (206 build steps).
- Runtime CTest: **5/5 passed**.
- Synthetic boot smoke: passed all 11 markers, guest threading and three null-backend
  flips. This is not a Demon's Souls boot result.
- SPU helper semantics: **76/76 scalar**, **76/76 SSE** on this x86 Linux host.
- PPU scaffold compile checks: no errors with GCC and Clang.
- CMake flag selection checked with x86 compilers and an AArch64 cross compiler:
  SSE enabled only where supported. This is not a Horizon platform build.
- Real-game PPU/SPU/firmware compilation and link completed. Executable:
  `upstream/DemonsSoulsRecomp/port/build-headless/des_boot` (about 263 MiB).
- The first unattended test (no menu input) timed out at 100 s after 185 milestones
  and game draw/flip requests; it did not reach the chosen save-load trigger.
- The second test with built-in menu input **passed**: 192 milestones, firmware
  load, actual entry, trophy-registration trigger, save load and save completion;
  normal exit 0 after 4.23 s. No known fatal markers matched. This uses the null
  backend, so no visible rendering, level gameplay or performance is established.
- Passing run: `analysis/runs/20261022T200622.558009Z/summary.json`.
  First run: `analysis/runs/20261022T200432.872575Z/summary.json`.
- No process from either bounded game test was left running.

## Switch artifact

`switch-probe/for-switch-memory-probe.nro` is a 223,429-byte diagnostic containing
no game code. It compiled with official workspace-local devkitA64 r30/libnx 4.12.0,
with `-Wall -Wextra -Werror`; all nine toolchain archives matched official package
metadata sizes/hashes. Its AArch64 ELF and NRO structure were checked.

It records the actual launch's address ranges, allowances and SVC hints and may
run one guarded 4 KiB explicit mapping test. It never maps 4 GiB physically or
resizes the loader heap. It has not been run on hardware. Device instructions:
`switch-probe/README.md`. Expected device log: SD `/switch/for-switch/memory-probe.log`.
The next required evidence is that real device log; Switch memory implementation
and graphics porting are still outstanding.

## Porting work still required

| Area | Finding / next implementation |
| --- | --- |
| Reference build | Headless initialization through trophy/save services passed. Visible PC rendering and level gameplay remain untested locally. |
| Switch toolchain | Official devkitA64/libnx and local CMake/Ninja are ready under `.tools`. The diagnostic cross-build passes. |
| Graphics | Existing renderer is D3D12; Linux uses vkd3d/Vulkan. Implement a Switch renderer and shader pipeline. [deko3d](https://github.com/devkitPro/deko3d) is a candidate, not an implemented backend. |
| Memory | Runtime assumes desktop virtual memory and POSIX mmap paths. Design guest address translation and allocation for Switch, then measure actual memory use. |
| Threads / services | Adapt synchronization, timing, exceptions, file paths, input and audio to [libnx](https://github.com/switchbrew/libnx). ARM support alone does not supply Horizon OS support. |
| CPU | The unconditional SSE build flag has been fixed and compiler-probed. SPU helper headers contain scalar fallbacks; real-game correctness and performance on ARM remain untested. |
| Firmware | Current official libsre has matching structural metrics but a different hash. The experimental PC initialization test passed with this module; retain provenance and test further services/gameplay separately. |
| Performance | No Switch performance or playability evidence yet. |
| Packaging | Produce an NRO after a working Switch implementation. A diagnostic NRO exists, with no game code; no game NRO or NSP/NSZ packaging exists here. |

## Reproduce

From the workspace root:

```sh
python3 tools/audit_inputs.py --decrypted work/decrypted --firmware work/firmware/libsre.prx
```

Build and observe the PC reference target after all generated inputs are ready:

```sh
bash tools/build-headless.sh
python3 tools/run-headless.py --seconds 100 --stop-at trophies
make -C switch-probe -j1
```

The audit writes `analysis/input-audit.json`, checks all listed file sizes,
and uses upstream's hash checker for known originals and decrypted inputs.
The strict audit currently fails only for the alternate official firmware module, even though the bounded initialization test passed. Its reference table is deliberately unchanged.
It never writes into game inputs. See `analysis/GAME_BOOT_PLAN.md` for actual boot
criteria; compilation or an exit code alone does not establish successful boot.
