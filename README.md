# Demon's Souls Switch port

Experimental Nintendo Switch startup prototype for Demon's Souls US v1.00
(BLUS30443), built around a checked, segmented-memory backend and a bounded
slice of locally translated PowerPC code.

**This is not a playable port. Graphics, audio, menus, and gameplay are not
implemented.** Version 0.27.0 completes thirteen static constructors and all
eight narrow/wide C++ stream initializers, then stops before constructor
fourteen. The latest checkpoint was installed and read back on hardware;
execution of v0.27 on hardware is still pending in the recorded evidence.

## Repository layout

- [`switch-port/`](switch-port/README.md): Switch loader, memory and native
  service backends, entry generator, host tests, and local packaging tools.
- [`switch-probe/`](switch-probe/README.md): earlier memory and launch diagnostics.
- [`analysis/SWITCH_WIDE_STREAMS_STATUS.md`](analysis/SWITCH_WIDE_STREAMS_STATUS.md):
  latest checkpoint, test results, and hardware-validation status.
- [`analysis/PORT_STATUS.md`](analysis/PORT_STATUS.md): broader development status.
- [`analysis/CONTINUATION_STATE.md`](analysis/CONTINUATION_STATE.md): handoff notes
  and next-constructor investigation.
- `tools/`: input auditing and Linux reference-build helpers.

Game executables, game assets, firmware, keys, generated game translations,
compiled binaries, and local installation archives are excluded. Build and
packaging instructions in the detailed development notes refer to artifacts
you generate locally.

## Prepare the local inputs

Clone this repository and run the following from its root. The directory
layout matters because the existing scripts resolve paths relative to it.

```sh
git clone https://github.com/JonathanDC64/DemonsSoulsRecomp.git upstream/DemonsSoulsRecomp
git -C upstream/DemonsSoulsRecomp checkout e99b4f02cd60139c6f795b1209e8c8ba35207376
git clone https://github.com/JonathanDC64/ps3recomp.git upstream/DemonsSoulsRecomp/ps3recomp
git -C upstream/DemonsSoulsRecomp/ps3recomp checkout bae620c0cfa040d5dbc4b3e0bcb5b025576ef5a4
```

Supply your own US v1.00 game executable at
`upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF`, then follow the pinned
upstream repository's input preparation and PowerPC lifting instructions.
The Switch entry generator expects `work/loader/EBOOT.functions.json`,
`work/recomp/ppu_recomp.h`, and the corresponding `ppu_recomp_*.cpp` chunks.
Its hash checks reject mismatched executables or translated function bodies.

The expected EBOOT.ELF SHA-256 is:

```text
90eaa5e9e48b4874292badc311ccea253e6a622ff188bfbdbe76e6f6821269d8
```

The optional Linux reference build also uses
[`analysis/source-preparation.patch`](analysis/source-preparation.patch).
Apply it to the pinned upstream checkout before using `tools/build-headless.sh`.
That helper expects the local CMake and Ninja paths described in its source;
it is separate from the Switch build.

## Validate and build

Host checks require Python 3, Bash, Clang/Clang++, OpenSSL development headers,
and the prepared local inputs. Run from this repository's root:

```sh
python3 switch-port/tools/generate_reference.py
bash switch-port/tests/run_host_tests.sh
bash switch-port/tests/run_heap_tests.sh
bash switch-port/tests/run_entry_tests.sh --explore-wide-streams
```

The detailed [port README](switch-port/README.md) documents the remaining
regression and fault-injection modes. These tests execute startup code;
they do not establish graphics or gameplay support.

The Switch build requires devkitA64 and libnx from devkitPro:

```sh
make -C switch-port DEVKITPRO=/path/to/devkitpro
python3 switch-port/tools/prepare_sd.py
```

Packaging creates a local SD-card archive containing your supplied executable
and the compiled loader. Keep that archive and all generated translations
local. The generated SD-card instructions describe how to run the checkpoint
and collect `loader.log`.

## Credits and licensing

The upstream reference is
[JonathanDC64/DemonsSoulsRecomp](https://github.com/JonathanDC64/DemonsSoulsRecomp),
with its pinned [ps3recomp fork](https://github.com/JonathanDC64/ps3recomp).
See [third-party notices](THIRD_PARTY_NOTICES.md) for attribution and the
upstream license. No new license grant is specified for the original
Switch-port code in this repository.
