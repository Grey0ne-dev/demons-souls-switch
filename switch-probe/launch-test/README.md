# Temporary application-launch diagnostic

This is an optional configuration test, not a game port or a known fix.
The user's current configuration already defaults to R for application override;
merely adding `override_any_app_key=R` would not change that behavior.

The supplied `override_config.ini` deliberately changes application override to
`override_any_app_key=!R`. A freshly started game should then open Homebrew Menu
without any button held. Holding R allows a normal game launch instead. Album's
R binding and the user's `[default_config]` controls are preserved.

1. Save a copy of the SD card's actual `/atmosphere/config/override_config.ini`.
2. Put this test file at that exact path. Do not use `config_templates`.
3. Close the running game from HOME using X and Close. Selecting an already
   running title can resume its process without running the creation-time override.
4. Start the installed game from HOME with no shoulder buttons held.
5. If Homebrew Menu opens, run For-Switch Probe v0.1.1 and capture its summary.
6. After testing, replace the active file with `override_config.normal.ini`,
   renamed to `override_config.ini`. This explicitly restores the any-application
   R binding while preserving the user's Album and default-config bindings.

Atmosphere reloads values into existing settings without resetting defaults.
The original photo's any-application R key was commented, so restoring that file
alone may leave the temporary `!R` value cached. If restoring the exact saved
original instead, reboot into Atmosphere afterward, or explicitly set
`override_any_app_key=R` before restoring it. Do not treat comments as resetting
an already-loaded value.

If the normal game starts, do not keep retrying the same steps. Record its name,
whether it was fully closed, and which SD/CFW environment contains the edited file.
The test has not yet established whether configuration loading, launch method,
controller input, or another loader is responsible.

Application status is checked by `Kernel application` in the probe. System
resources can remain zero in application mode; that separate result is not a
launch-mode test.

Primary source: Atmosphere's current override parser and XOR match logic:
https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/source/cfg/cfg_override.board.nintendo_nx.inc
