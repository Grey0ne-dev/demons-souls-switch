# Runtime and Switch portability audit

Local source inspection: 2026-10-22. No firmware or game data was downloaded for this audit.

## What can be built first

The upstream port has a headless Linux path. This can test generated game code, threading, files and SPURS without introducing the renderer and UI dependencies. It does not produce a Switch application, and a successful runtime-library build alone does not establish that the game boots.

The following commands run from `upstream/DemonsSoulsRecomp`, after CMake and Ninja are available. This workspace has CMake at `.tools/cmake-4.4.4-linux-x86_64/bin/cmake` and Ninja at `.tools/bin/ninja`, relative to the workspace root. When they are not on `PATH`, invoke CMake by absolute path and add `-DCMAKE_MAKE_PROGRAM=/home/grey/Games/For-Switch/.tools/bin/ninja` at configure time.

```sh
cmake -S ps3recomp -B ps3recomp/build-headless -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DPS3RECOMP_RSX_D3D12_VKD3D=OFF \
  -DPS3RECOMP_RSX_VULKAN=OFF \
  -DPS3RECOMP_FFMPEG_RUNTIME=OFF \
  -DPS3RECOMP_BUILD_TESTS=ON
cmake --build ps3recomp/build-headless --target ps3recomp_runtime --parallel 2
```

SDL2 development files and pthreads are required even in headless mode. C17 and C++20 are required. FFmpeg headers are optional with the flags above; enabling `PS3RECOMP_FFMPEG_RUNTIME` adds actual movie decoding. Vulkan and vkd3d are unnecessary for the null renderer. No UI libraries are fetched when `DES_UI=OFF`.

Once the real PPU and SPU lifts exist, configure the game separately. Use absolute paths for the imported runtime library to avoid build-directory-relative ambiguity:

```sh
cmake -S port -B port/build-headless -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DPS3RECOMP_BUILD_DIR="$PWD/ps3recomp/build-headless" \
  -DDES_VKD3D=OFF -DDES_UI=OFF -DDES_DEV=OFF
cmake --build port/build-headless --target des_boot --parallel 2
```

The main executable requires `work/recomp/ppu_recomp_*.cpp`, generated `hle_nid_table.cpp` and headers, `ps3recomp/work_spu/*/spu_recomp.c`, and the generated SPU workload registration file. The PPU chunks are large: upstream docs/76 reports about 2 GB compiler memory per chunk at `-O2`; parallelism must fit available memory.

`libsre` is optional at compile time but required for the documented known-good boot path. Upstream tools/des/deslib.sh selects:

```sh
export LIBSRE_LLE=1
export LIBSRE_LLE_ONLY=011EE38B,1656D49F,8A85674D,73E06F91,082BFB09,E5443BE7,039D70B7,91066667
export SPURS_NO_ROUND_NOTIFY=1
```

The relevant decrypted firmware module is lifted using `scripts/build_libsre.py`; both `work/libsre/lifted/ppu_recomp.c` and `port/gen/libsre_glue.c` must exist before configuring the game. The runtime image must be at `work/libsre/libsre_image.bin` relative to the launch working directory. Upstream docs/25 explains why the HLE queue path can spin without it.

The Linux executable is launched directly from the project root with `port/build-headless/des_boot work/game/EBOOT.ELF`. Set `PS3_VFS_ROOT` to the disc root containing `PS3_GAME`, and use separate `PS3_SAVEDATA_ROOT`, `PS3_HDD1_ROOT` and `PS3_SHADER_CACHE` folders for initial runs. `SDL_AUDIODRIVER=dummy`, `AUDIO_MUTE=1`, `DES_UI=0`, and `DES_ONLINE=0` suit a headless test. The inherited shell helpers include Windows-specific process management and should not be treated as a Linux launcher without checking them.

## Changes and focused verification

`port/CMakeLists.txt` formerly passed `-msse4.1` to every non-MSVC host. It now probes the actual C and C++ compilers and enables SSE4.1 only when both accept it, as the runtime already does for its own C sources. The PPU and SPU builds share the result. This removes one immediate ARM compile failure; it does not implement a Switch platform.

Existing self-contained SPU helper tests were compiled and executed on this x86-64 Linux host:

```sh
clang -std=gnu17 -O2 -I include -I runtime/spu \
  -o /tmp/des-test-spu-scalar runtime/spu/tests/test_spu_helpers.c -lm
/tmp/des-test-spu-scalar
clang -std=gnu17 -O2 -msse4.1 -I include -I runtime/spu \
  -o /tmp/des-test-spu-sse runtime/spu/tests/test_spu_helpers.c -lm
/tmp/des-test-spu-sse
```

Both configurations passed **76 tests, 0 failures**. These verify existing scalar/SSE semantics on this host; they are not ARM execution tests.

`python3 tools/check_ppu_scaffold.py` and the same command with `--cxx clang++` both passed, with **0 compile errors** across the six PPU scaffold/template units, including `boot_main.cpp`. These checks generate temporary stub declarations from the lifter and compile without linking or running a game.

With CMake/Ninja installed locally, the pinned runtime was configured and fully built in `ps3recomp/build-headless` with Clang 23.1.1, Release optimization, the null renderer and tests enabled. All 206 build steps completed successfully without runtime source changes. Outputs include `libps3recomp_runtime.a`, `ps3recomp_host` and `ps3recomp_boot_smoke`.

Runtime CTest results: **5/5 passed** (`sync_stress`, `fiber_switch`, `lv2_system_time`, `vdec_stub`, `pamf_reader`), 5.58 seconds total. The synthetic boot title also **passed**: all 11 markers appeared in order, a guest worker executed on its own host thread, 3 flips produced 3 frames, the vblank handler ran and the guest's exit status arrived correctly. The smoke invocation was:

```sh
env SDL_AUDIODRIVER=dummy AUDIO_MUTE=1 DES_ONLINE=0 \
  timeout 30 ps3recomp/build-headless/ps3recomp_boot_smoke \
  ps3recomp/build-headless/smoke/smoke.elf
```

The actual game CMake configuration in `port/build-headless` accepted both SSE probes on x86-64 and retained `-msse4.1`. The exact changed non-MSVC CMake branch was also tested separately against Clang's `aarch64-none-elf` target: both SSE probes correctly rejected that flag, then C17 and C++20 probe sources built into real ELF ARM aarch64 objects without any SSE flag. This demonstrates the flag fix on a cross compiler; it is not a full ARM or Switch runtime build.

## Work still needed for Switch

| Area | Evidence in current sources | Consequence |
| --- | --- | --- |
| Host platform | `runtime/platform/win32_compat.{h,c}` maps Windows behavior to pthreads, `mmap`, `mprotect`, signal contexts, dynamic loading and host timers; its exception handlers cover Linux and Darwin ARM64. No libnx/Horizon implementation was found. | A Horizon system layer and exception/memory strategy are required. ARM64 support in desktop CI does not establish Switch support. |
| Guest address space | `runtime/ppu/tests/boot_main.cpp` maps a flat 4 GB guest range plus a 64 KB guard with `MAP_NORESERVE`, aligned to 4 GB, temporarily requesting 8 GB plus the guard of virtual address space. | Virtual reservation is distinct from physical memory consumption. Switch must provide a compatible reservation/commit strategy or replace flat address translation. |
| Game allocator | `port/des_heap.cpp` allows a 1.5 GB guest heap at `0x60000000..0xC0000000`. | Measure actual resident/peak memory before choosing a physical budget. The range alone is not proof that all 1.5 GB is used. |
| Renderer | Game boot selects the validated D3D12/vkd3d backend on Linux, Metal on Apple, or null. The separate Vulkan implementation uses a different draw engine and is not automatically wired to this game's boot path. | A Switch graphics backend and shader pipeline need implementation and validation. Enabling the runtime's Vulkan option alone does not port the game renderer. |
| Upload memory | `tools/des/deslib.sh` sets `VP_VB_VERTS=1572864`; its own comment identifies 768 MB of vertex upload memory. | Desktop defaults require profiling and reduction for a constrained shared-memory device. |
| SPU/VMX performance | `spu_helpers.h` gates accelerated byte shuffles and related helpers on `__SSE4_1__`; scalar fallbacks exist. | ARM may compile these paths but still need profiling and NEON optimization. No Switch speed or frame-rate estimate has been measured. |
| Synchronization | Guest workers map to many host threads and atomic reservation/coherency helpers. | ARM memory-order correctness and scheduling need dedicated runtime tests and real-device testing. |
| Input, audio, files | SDL2 handles non-Windows input/audio; filesystem routing assumes desktop paths and host services. | Cross-build SDL/libnx adapters, SD layout, save routing and suspend behavior are still needed. |
| Build/package | Current CMake builds desktop executables and static archives. No devkitA64 toolchain or NRO metadata/assets step exists. | NRO packaging comes after the platform layer and an initial real-device smoke program. |

The runtime was fetched and checked out at the port's exact pin, `bae620c0cfa040d5dbc4b3e0bcb5b025576ef5a4`, before the successful runtime build. Its tracked source tree is clean. An earlier shallow checkout lacked this commit; that prerequisite is now resolved. `DES_REQUIRE_PIN=ON` requires this exact matching clean runtime checkout.

## Local evidence

- `upstream/DemonsSoulsRecomp/README.md`
- `upstream/DemonsSoulsRecomp/port/CMakeLists.txt`
- `upstream/DemonsSoulsRecomp/ps3recomp/CMakeLists.txt`
- `upstream/DemonsSoulsRecomp/docs/76-native-linux.md`
- `upstream/DemonsSoulsRecomp/docs/25-spurs-queue-lle-boot-assets-complete.md`
- `upstream/DemonsSoulsRecomp/ps3recomp/.github/workflows/macos.yml`
- `upstream/DemonsSoulsRecomp/ps3recomp/runtime/ppu/tests/boot_main.cpp`
- `upstream/DemonsSoulsRecomp/ps3recomp/runtime/spu/spu_helpers.h`
