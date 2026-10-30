# Switch main-thread TLS checkpoint — 2026-10-23

Implemented the next bounded slice in `switch-port` after the existing real-entry
import checkpoint. The default Switch loader now uses `ds_entry_run_tls`; the old
`ds_entry_run` pre-service API remains available.

The real translated entry reaches sys_initialize_tls (NID 0x744680A2). The adapter
validates the expected main-thread arguments and LR, explicitly backs 36 KiB at
0x0E000000, copies the actual four-byte template, zeroes the TLS tail, and sets
r13=0x0E007000 and r3=0 (CELL_OK). Layout and register effects follow the pinned
local desktop HLE in runtime/ppu/ppu_sysprx.cpp. No later CRT instructions execute;
the known subsequent null-access issue and TOC restore behavior still need audit.
This does not implement secondary-thread TLS or a PS3 scheduler.

Validation:

- Host startup tests: 85 checks passed on real instruction bodies, plus 6 checks
  passed on the deliberately unsupported-import fixture.
- AddressSanitizer, UndefinedBehaviorSanitizer and leak detection enabled.
  LeakSanitizer required running outside the sandbox for process inspection.
- Switch devkitA64/libnx cross-build passed with warnings treated as errors.
- SD ZIP generation verified the real EBOOT SHA-256, NRO headers, archive CRCs,
  and archived payload hashes.
- Artifact: switch-port/demons-souls-switch-startup.zip (9,716,906 bytes).

No device run has been performed for this version. The loader screen and SD
/switch/demons-souls-loader/loader.log distinguish successful main-thread TLS
initialization from a failure. Graphics, menus and gameplay are not implemented.
