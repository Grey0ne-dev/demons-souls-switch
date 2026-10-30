# Real-game headless boot smoke plan

Prepared from the locally checked-out port and pinned runtime. This is a plan,
not evidence that the executable has booted. Do not run until `des_boot` links.

## Configuration supported by source

- Build the port with `DES_VKD3D=OFF`, `DES_UI=OFF`, using the matching null-backend
  runtime. `boot_main.cpp` selects `rsx_null_backend_*` on Linux without
  `PS3RECOMP_RSX_D3D12_VKD3D`; this has no window or display requirement. It is a
  limited software backend, not proof that the final renderer works.
- Launch from `/home/grey/Games/For-Switch/upstream/DemonsSoulsRecomp`.
  The generated firmware glue loads the relative path
  `work/libsre/libsre_image.bin` from the current working directory.
- Pass `work/game/EBOOT.ELF` as the first argument. That causes the game ELF's
  actual segments and entry OPD to be used, rather than a synthetic test title.
- Set `PS3_VFS_ROOT=/home/grey/Games/For-Switch/gamedata` (contains `PS3_GAME`).
  The existing encrypted SDATA inputs are supported by the runtime's own SDATA
  resolver; the workspace's decrypted overlay is not required by this plan.

## Small environment, with reasons

These three settings reproduce `tools/des/deslib.sh`'s known-good queue route:

```text
LIBSRE_LLE=1
LIBSRE_LLE_ONLY=011EE38B,1656D49F,8A85674D,73E06F91,082BFB09,E5443BE7,039D70B7,91066667
SPURS_NO_ROUND_NOTIFY=1
```

`LIBSRE_LLE` loads the lifted firmware. The NID allowlist sends LFQueue and
SpursQueue calls to that firmware while leaving the rest on the port's HLE.
`SPURS_NO_ROUND_NOTIFY` disables the heuristic that previously signaled completion
too early and broke decompression. See upstream docs/25 and `deslib.sh`.

For a test that is unattended, isolated and easy to assess:

```text
PS3_NO_USER_INPUT=1
DES_ONLINE=0
PS3_VERBOSE=0
SDL_AUDIODRIVER=dummy
AUDIO_MUTE=1
DES_PRECOMPILE=0
DES_SKIP_INTRO=1
DES_SKIP_INTRO_MOVIE=1
```

The first two keep the run deterministic and offline. Quiet logging is important:
the upstream source documents per-event logging changing thread timing and causing
apparent stalls. SDL's dummy audio avoids requiring a sound device; `AUDIO_MUTE`
alone still opens a device. Startup shader precompile has no useful role in a null
renderer test. Skipping logos/movie reduces waiting for a smoke test; these are
existing title hooks, not substitute guest code.

Set these absolute output paths under a fresh run directory, for example
`analysis/boot-smoke-01`, and create the directory first:

```text
PS3_HDD0_ROOT=<run>/hdd0
PS3_HDD1_ROOT=<run>/hdd1
PS3_SAVEDATA_ROOT=<run>/savedata
PS3_TROPHY_DIR=<run>/trophies
PS3_SHADER_CACHE=<run>/shader-cache
PS3_MILESTONE_OUT=<run>/milestones.txt
DES_FRAMELOG=<run>/frames.txt
```

This isolates game data/cache, savedata and trophies from the disc input and other
runs. The milestone stream records first occurrences and checkpoints counts;
entries survive forced termination. The frame log describes the game's flip
requests; an initialized backend alone is not a game frame.

Do not copy `VP_VB_VERTS=1572864` (a D3D12 buffer reservation of about 768 MB),
lock-screen detection, Windows priority/affinity, debug console, screenshots or
profiling flags for this initial Linux null test. `RSX_PRESENT=0` is not the switch
for headless operation. A display driver override is unnecessary.

## Two bounded tests

1. **Reach the system-data loading call.** Keep input at rest. Add
   `EXIT_ON_HLE=FBD5C856`, `EXIT_DELAY_MS=1000`, `EXIT_AFTER_MS=90000`. Put an outer
   process timeout at 100 seconds, killing only this launched process/group. The
   timer is a backstop, not success. The NID trigger requests exit when the import
   is entered; the 1-second delay lets that call run, but its completion still
   needs separate log evidence.
2. **Pass the notice into trophy initialization.** Use a fresh run directory or
   deliberately retain test 1's isolated system data. Set `DES_AUTO_CONTINUE=1`
   and change `EXIT_ON_HLE=1197B52C`, keeping the same time limits. This built-in
   title hook emits Cross/Start at intervals and releases them when a level script
   opens. Its destination is the last save; an empty savedata folder does not
   supply a playable character or level. A first boot reaching character creation
   is not a failure of the boot smoke.

Exact current registration NIDs, from generated `work/recomp/hle_nid_table.cpp`:

| Import | NID |
| --- | --- |
| cellSpursInitializeWithAttribute | AA6269A8 |
| cellGameDataCheckCreate2 | C9645C41 |
| cellSaveDataAutoLoad2 | FBD5C856 |
| sceNpTrophyRegisterContext | 1197B52C |

Do not use `CDC6AEFD` for `cellSaveDataAutoLoad2`; an old source comment labels it
that way, but the real current table identifies it as `cellSaveDataUserAutoLoad`.

## Evidence and pass criteria

Require the target's exact trigger line in captured stdout/stderr:

```text
[exit-on] first call of 0xFBD5C856 cellSaveDataAutoLoad2
```

or, for test 2:

```text
[exit-on] first call of 0x1197B52C sceNpTrophyRegisterContext
```

Also require `[libsre] LLE load: ok=1`, the actual `[boot] dispatching entry OPD`
line and `boot:entry` in the milestone stream. Inspect progressively stronger
runtime milestones such as `hle:cellSpursInitializeWithAttribute`,
`hle:cellGameDataCheckCreate2`, `hle:cellSaveDataAutoLoad2`,
`hle:sceNpTrophyRegisterContext`, and `rsx:draw_arrays` / `rsx:draw_indexed`.
`[RSX null] headless software backend` only proves backend initialization.
The fixture's `load:functions` and `load:hle_registered` scalars establish which
lift/handler registry was loaded.

An exit code of 0 without the target evidence is not a pass: `EXIT_AFTER_MS`
also exits 0. An outer timeout, abort, signal, unexpected entry unwind, missing
firmware, unresolved indirect call or fatal guest assertion is a failed or
incomplete test; record the furthest positive milestone and exact blocker.
Expected missing optional-file probes should be assessed individually rather
than treating every `open FAIL` as fatal.

Do not blindly reuse `tools/des/milestones.sh`: it requires a D3D12-ready line,
uses Windows staging/process tools, and some old verbose thread regexes disappear
under `PS3_VERBOSE=0`. Native Linux also has no debug console or snapshot yet
(upstream docs/76). Capture both output streams and preserve milestones.

A successful test establishes only that native Linux executes the real lifted
game's boot path through the named service. It does not establish a visible
title screen, a rendered level, gameplay, Switch compatibility or performance.

## Local sources inspected

- `tools/des/deslib.sh`, `desprobe.sh`, `milestones.sh`, `desrun.sh`
- `docs/25-spurs-queue-lle-boot-assets-complete.md`, `docs/76-native-linux.md`
- `port/CMakeLists.txt`, `des_options.cpp`, `des_ui_stub.cpp`
- `scripts/gen_libsre_exports.py`, generated `work/recomp/hle_nid_table.cpp`
- `ps3recomp/runtime/ppu/tests/boot_main.cpp`, `ppu_hle.cpp`, `ppu_fs.cpp`
- `ps3recomp/runtime/milestone.c`, `include/ps3emu/milestone.h`
- `ps3recomp/libs/video/rsx_null_backend.c`, `rsx_commands.c`
- `ps3recomp/libs/input/cellPad.c`, `libs/system/cellSaveData.c`
