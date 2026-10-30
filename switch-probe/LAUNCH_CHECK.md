# Read-only launch check

Artifact: `for-switch-launch-check.nro`, Homebrew Menu title **For-Switch Launch Check**.
Compiled and structurally checked locally; device behavior is not yet tested.

The earlier memory probes established that the observed launch is a non-application
process with a small memory allowance. They do not establish why game interception
failed. This check gathers the missing evidence from the device itself.

Copy the NRO to SD `/switch/` and run it from the Homebrew Menu that already works.
Press the **R bumper** once and release it. The screen records whether R or ZR
was seen, so a photograph can be taken afterward. Send the screen or the SD file
`/switch/for-switch/launch-check.log`, then use **+** to exit.

It reads the exact active path `/atmosphere/config/override_config.ini`, up to
16 KiB, and writes those original bytes into its own log before extracting a few
literal values for the screen. It reports file absence/read errors, a UTF-8 BOM,
and truncation. All program-specific entries remain available in the full log.
These are **file observations**, not proof of Atmosphere's currently cached
configuration. Omitted keys may retain values from previous reloads.

It checks the default loader `/atmosphere/hbl.nsp`, the configured path when one
is present, `/hbmenu.nro`, and the template file's presence. It also queries the
kernel's application flag/program ID and reads the public Exosphere API version
through SPL config item 65000; failures remain unknown. It never queries device
IDs, keys or secrets.

No configuration is changed, no game is launched, no custom memory mappings are
created, and no data is transmitted. The only file written is the check's own
log, which a repeat run replaces. A successful R detection inside homebrew does
not prove Atmosphere's separate loader-time HID query succeeded.

Build from the workspace root:

```sh
make -C switch-probe launch-check -j1
```

Primary sources:

- [Atmosphere configuration and override matching](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/source/cfg/cfg_override.board.nintendo_nx.inc)
- [Public SPL config definitions](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/include/stratosphere/spl/spl_types.hpp)
- [Exosphere API version encoding](https://github.com/Atmosphere-NX/Atmosphere/blob/master/exosphere/program/source/smc/secmon_smc_info.cpp)
