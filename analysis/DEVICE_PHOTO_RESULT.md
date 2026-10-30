# First device result: photograph supplied by user

Source: `/tmp/codex-clipboard-yWB4ji.png`, rotated for reading. This is a
photograph interpretation, not an exact transcription or a raw device log.

Readable findings:

- The diagnostic ran on the user's Switch and opened its SD log.
- `Atmosphere=1`, `applet_type=2`. The installed libnx header identifies 2
  as `AppletType_LibraryApplet`.
- A 4 GiB-aligned userspace virtual reservation succeeded and was removed.
  The photograph explicitly says no 4 GiB physical allocation occurred.
- The displayed system-resource total is zero. The explicit mapping step
  reports `SKIP ... required SVC hints/system resources unavailable`.
- The app reached `Probe complete. Press + to exit.`

This establishes device execution and a virtual-reservation result in this
applet launch. It does not establish physical-map support, game memory capacity,
guest code execution, graphics, or gameplay. Exact hexadecimal addresses and
budgets should come from the raw SD log rather than this blurred photograph.

Next device input: `/switch/for-switch/memory-probe.log`, ideally also from a
full application-mode launch. A different launch may still have zero system
resource budget; that must be measured rather than assumed fixed.

## Second photograph

Source: `/tmp/codex-clipboard-NJCpqE.png`, rotated for reading. This second
run also clearly displays `applet_type=2`, a successful userspace virtual
reservation, zero system-resource allocation, and a skipped explicit-map test.
It does not independently establish the kernel's launch classification: the
displayed applet type is supplied through the loader ABI. The small memory
allowance corroborates limited resources, but the user's report that they
followed the application-override launch sequence remains unresolved. The log
opened and the probe completed. No exact addresses were transcribed.

The user's launch sequence should not be treated as an error. Version 0.1.1
adds the kernel `InfoType_IsApplication` query and loader identity to distinguish
the reported ABI type from actual kernel classification. Zero system-resource
allocation can also occur in application mode, so it cannot diagnose launch
mode or be promised fixed by repeating the R-button sequence.

## Version 0.1.1 device photograph

Source: `/tmp/codex-clipboard-asRbwD.png`, rotated for reading. This photograph
clearly shows the new independent results:

- Kernel application: **NO** (the query succeeded; not inferred from resources).
- Loader applet type: **2**.
- Loader: **nx-hbloader v2.4.5**.
- Program ID: **010000000000100d**.
- Process memory allowance: **393 MiB**.
- Loader heap override: **255 MiB**.
- System resources: **0 KiB**.
- Virtual window: **Found**.
- 4 KiB page test: **Skipped: zero system resources**.

The actual process is therefore not kernel-classified as an application in
this run. This does not establish why the user's application-override launch
sequence did not produce such a process. Next troubleshooting input is the
active SD `/atmosphere/config/override_config.ini` (if present) and the exact
installed title selected for the R-button launch. Do not ask the user to keep
repeating the same launch instructions or to change configuration blindly.

## Launch method clarified by user

The user subsequently clarified: **"ive launched album while holding r"**.
This explains the observed non-application/Album-host launch. The previous
statement that the prescribed sequence was followed did not mean a game icon
was selected. No configuration change is needed to distinguish those paths.

The supplied configuration photograph `/tmp/codex-clipboard-LYQZ4K.png` shows
`[hbl_config] override_any_app=true`, `path=atmosphere/hbl.nsp`, and
`override_key=R`. Current Atmosphere interprets that last key as the first
program-specific override (the default PhotoViewer entry); the application-wide
R key remains the default because `override_any_app_key` is commented out.
`title_id` is not handled by the current parser, but does not by itself disable
the enabled application override. Preserve the user's `[default_config]` settings.

Next actual comparison: hold R while launching an installed GAME icon from HOME,
keeping it held through any profile selection until Homebrew Menu appears,
then run probe v0.1.1. Check the independent kernel application flag and allowance.
Zero system resources can persist in application mode and is a separate result.

## Game launch still opens game

The user now reports launching an actual game with R, with the normal game
opening. The reason is unresolved. Prepared a separate, explicitly temporary
`switch-probe/launch-test/override_config.ini` using `override_any_app_key=!R`
to remove R timing/detection from the test. It preserves the user's Album and
default-config bindings. Rollback must explicitly restore `override_any_app_key=R`
(prepared normal file), or restore the original file and reboot Atmosphere;
commenting the key does not reset the already-loaded global setting.
No Switch configuration was modified remotely. The game must be fully closed
before testing a fresh process; resuming an existing game is not a fresh launch.

## Read-only launch collector prepared

The user reports the same failure after further instructions. Actual active-SD
file contents and button observations are still unverified; do not infer that
the proposed configuration was read or that button timing is the cause.

Built `switch-probe/for-switch-launch-check.nro`, which can run through the already
working Album/Homebrew Menu path. It reads the exact active configuration, logs
its bounded original bytes (including per-program entries), checks loader files,
reads public version/kernel metadata, and separately records R and ZR detection.
It writes only `/switch/for-switch/launch-check.log`; it changes no settings and
does not launch titles or create memory mappings. Build and NRO checks passed;
this collector has not run on the user's device yet. See `switch-probe/LAUNCH_CHECK.md`.

## Launch-check device result

Source: user photograph `/tmp/codex-clipboard-E0WGww.png`, rotated for reading.
Clear observations:

- Kernel application NO; allowance 393 MiB.
- Loader nx-hbloader v2.4.5; public API reports Atmosphere 1.12.0.
- Active SD config FOUND, 448 bytes; BOM=0, truncated=0, read_error=0.
- Literal `override_any_app=true` and `path=atmosphere/hbl.nsp`.
- Literal `override_any_app_key` is **not present in the active file**.
- Default hbl.nsp, configured loader path and root hbmenu.nro all FOUND.
- R bumper detected YES; ZR trigger detected YES (independently latched).

The active file does not contain the proposed temporary `!R` setting. This
establishes that the no-button diagnostic file contents are not currently
observed at the active path; it does not mean the missing key itself explains
the original R failure (R is the code default, and prior cached settings can
also exist). Next focused edit, if the user continues the launch test, is to
explicitly set `override_any_app_key=!R` under `[hbl_config]` at the actual
active path and then launch a fully closed game without shoulder buttons.
Rollback must explicitly restore R. Homebrew R detection does not prove the
separate Atmosphere loader-time HID query succeeded.
