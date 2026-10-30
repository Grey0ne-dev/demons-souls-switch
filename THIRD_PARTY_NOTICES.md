# Third-party notices

The Switch-port development work uses the following upstream projects as
local reference and generation dependencies:

- DemonsSoulsRecomp, Jonathan Del Corpo and contributors:
  https://github.com/JonathanDC64/DemonsSoulsRecomp,
  commit `e99b4f02cd60139c6f795b1209e8c8ba35207376`.
- Its ps3recomp fork and the original ps3recomp contributors:
  https://github.com/JonathanDC64/ps3recomp,
  commit `bae620c0cfa040d5dbc4b3e0bcb5b025576ef5a4`.
- devkitPro/devkitA64 and libnx provide the local Switch toolchain and native
  APIs. They are not vendored here.

`analysis/source-preparation.patch` contains changes to the MIT-licensed
DemonsSoulsRecomp project. Its copyright and license notice is preserved in
[`licenses/DemonsSoulsRecomp-MIT.txt`](licenses/DemonsSoulsRecomp-MIT.txt).
Consult each upstream checkout for its other dependency notices.

No game executable, game assets, Sony firmware, or keys are included. The
upstream MIT license does not grant rights to those materials. A license for
the original Switch-port work has not been selected.
