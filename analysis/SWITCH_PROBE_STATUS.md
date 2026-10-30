# Switch diagnostic build status

## Current version: 0.1.1

Artifact: `switch-probe/for-switch-memory-probe-v0.1.1.nro`.
Build/structure checks passed; this new version is untested on hardware.
Evidence: `analysis/switch-probe-v0.1.1-build.json` and matching build log.
It adds direct kernel application classification, program ID, bounded loader
identity and a compact final screen. The raw log is versioned as
`/switch/for-switch/memory-probe-v0.1.1.log`.

User photographs show version 0.1.0 running twice with loader applet type 2,
a virtual reservation and skipped physical mapping. The user reports following
the application-override sequence. That cause remains unresolved; skipped
physical mapping also occurs with zero system resources in application mode.
Do not infer an incorrect user launch sequence from that skip. Details:
`analysis/DEVICE_PHOTO_RESULT.md`.

## Original 0.1.0 build-time record

**Compiled, not tested on a device. No game executable for Switch exists here.**

- Artifact: `switch-probe/for-switch-memory-probe.nro`.
- Source and device instructions: `switch-probe/README.md`.
- Local SDK: `.tools/switch/root/opt/devkitpro`.
- Toolchain: devkitA64 r30 / GCC 16.1.0 / binutils 2.46.0;
  libnx 4.12.0; switch-tools 1.13.1; general-tools 1.4.4.
- All nine official package archives matched HTTPS package metadata SHA-256
  and size. Exact package URLs, versions, dependencies, database hashes, and
  checksums: `.tools/switch/metadata/manifest.json`. Package signatures were
  not independently verified. Only `opt/devkitpro/*` was extracted locally;
  no system install, package hooks, or sudo was used.
- `make -C switch-probe -j1` passed `-Wall -Wextra -Werror`.
- The ELF is AArch64 PIE; NRO header, segment alignment/bounds/order, asset
  header and diagnostic NACP title checked. Exact artifact hash and checks:
  `analysis/switch-probe-build.json`; compiler log: `analysis/switch-probe-build.log`.
- No Switch console was connected. No log or memory measurement from a
  Switch has been fabricated. `device_run` is false in the build report.

The diagnostic records the launch's address-space regions, heap override,
memory/resource allowance, and SVC hints. It can reserve a 4 GiB-aligned
4 GiB + 64 KiB Alias-region interval as **userspace bookkeeping only**.
It excludes the alias extra tail, matching the pinned libnx selectors;
ordinary libnx allocations cannot select the remaining core Alias region.

If the environment permits, it immediately verifies one own 4 KiB interval
is unmapped, maps it explicitly, checks its complete mapping/permissions,
reads/writes sentinels only after that check, and unmaps it. These steps run
under the libnx VM mutex with no intervening logging or allocation. An
unexpected unmap failure triggers full host-process exit so kernel teardown
reclaims that page. It does not resize the loader's heap or fault on purpose.

A failed/skipped explicit map may reflect the loader heap or system-resource
budget. A successful tiny map and virtual reservation do not establish memory
capacity, guest code execution, fault recovery, graphics, or game performance.
The memory design and next EBOOT-loading experiment are in `analysis/SWITCH_NEXT.md`.
