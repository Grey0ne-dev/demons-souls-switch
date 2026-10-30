# Switch memory diagnostic v0.1.1 — new version untested on device

`for-switch-memory-probe-v0.1.1.nro` is a **diagnostic homebrew application**. It does
not contain or launch Demon's Souls. It has compiled successfully, but no real
Switch has run this new version here. User photographs show v0.1.0 executing,
finding a virtual window and skipping the explicit page map; exact raw logs
have not yet been supplied.

Version 0.1.1 adds an independent kernel `IsApplication` query, program ID and
bounded loader identity. The final screen is now a compact summary suitable for
a photograph. Kernel launch status, loader-reported applet type and memory
budgets are displayed separately. Zero system-resource allocation does not
identify launch mode and may also occur in application mode; repeating a launch
sequence cannot be promised to enable the page-map test. This release does not
change or force the applet type or loader configuration.

The app logs the current launch's address-space regions, memory allowance,
system-resource allocation, heap override, and syscall hints. It then looks for
an unmapped Alias-region interval covering 4 GiB plus 64 KiB, with its base
aligned to 4 GiB. It adds and removes a libnx reservation without accessing that
unmapped window. This reservation is userspace bookkeeping: it allocates no
4 GiB physical backing and does not reserve the range in the kernel.

If the required system resources and syscall hints are present, the app tries
to map **one 4 KiB page** at an explicit address inside its own reserved window.
The app requeries the exact page immediately before mapping while holding
libnx's VM mutex, then checks full-page Heap/RW permissions after a successful
map. It accesses sentinels only if that check succeeds and unmaps its page with
the same address and size regardless of the check's outcome. It never resizes
the loader's heap or deliberately triggers a memory fault. If unmapping
unexpectedly fails, it closes the log and **exits the host process** to guarantee
kernel cleanup; returning to Homebrew Menu alone would not reclaim that page.
Such a run is a cleanup failure, not a passing result.

The candidate interval excludes the alias extra tail, matching the pinned
libnx 4.12.0 implementation. All three normal libnx selectors (ASLR, Stack, and
CodeMemory) reject overlap with that core Alias region. This isolated diagnostic
has no other arbitrary reservation caller. A reservation does not establish
kernel exclusivity against an unrelated component mapping addresses directly.

The test may be skipped or fail because the launch lacks system-resource
allocation or the loader has already mapped most of the memory allowance.
That is inconclusive for alternative guest-memory implementations. A successful
test establishes these small primitives only; it does not prove sufficient
physical memory, fault recovery, game compatibility, or playable performance.

## Build

From the workspace root:

```sh
make -C switch-probe -j1
```

The Makefile defaults to the workspace-local toolchain at
`.tools/switch/root/opt/devkitpro`. To use another installation:

```sh
make -C switch-probe DEVKITPRO=/absolute/path/to/devkitpro -j1
```

Prepared toolchain: devkitA64 r30 (GCC 16.1.0), binutils 2.46.0,
newlib 4.6.0.20260123, libnx 4.12.0, and switch-tools 1.13.1. The package names
are the official `switch-tools` and `general-tools`; no separate `nx-tools`
package was needed. Downloads and metadata are under `.tools/switch/`.
Every package archive's size and SHA-256 were checked against official HTTPS
repository metadata before extracting only `opt/devkitpro/*` into the workspace.
No package hooks or system installation were run. Signatures were not
independently verified. The exact versions, hashes, URLs, and database hashes
are in `.tools/switch/metadata/manifest.json`.

The official package endpoint initially returned HTTP 403 with curl's default
client string; `User-Agent: devkitPro pacman` retrieved the databases and archives.
TLS verification remained enabled.

## Run and retrieve measurements

1. Copy `for-switch-memory-probe-v0.1.1.nro` to the SD card's `/switch/` directory.
2. Open it from Homebrew Menu. A launch with full application memory is most
   relevant; an applet launch may report different limits.
3. The app writes `/switch/for-switch/memory-probe-v0.1.1.log` on the SD card,
   preserving the old version's log. A repeat v0.1.1 run replaces that version's
   previous log. If opening the log fails, it prints to the console only.
4. Press **+** when the probe finishes to return to Homebrew Menu.

Keep the complete log when interpreting results. Its values are measured on
the device during that launch; the build verification file contains **no**
substitute device measurements.

## Primary references

- [libnx memory syscalls and information types](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/include/switch/kernel/svc.h)
- [libnx reservation API](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/include/switch/kernel/virtmem.h)
- [libnx selector exclusions and alias extra region](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/source/kernel/virtmem.c)
- [libnx environment APIs](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/include/switch/runtime/env.h)
- [Atmosphere physical-memory validation](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/svc/kern_svc_physical_memory.cpp)
- [Homebrew loader heap setup](https://github.com/switchbrew/nx-hbloader/blob/master/source/main.c)
- [Official devkitPro package configuration](https://devkitpro.org/viewtopic.php?p=18416)

The next runtime investigation is described in `analysis/SWITCH_NEXT.md`.
