# Local firmware and RPCS3 route (2026-10-22)

## Confirmed inputs

- The user's ISO contains `PS3_UPDATE/PS3UPDAT.PUP`, recorded size
  268,435,456 bytes. Reading its PUP header directly from the ISO gives entry
  `0x100 = "2.76\n"`; it is the disc's **2.76 firmware**, not current firmware.
  No firmware module was extracted during the initial investigation.
- The upstream reference table requires decrypted `libsre.prx`, size 239,344,
  SHA-256 `115f9df9596dd0b358e5a8c4eaaf77ee44c1f3ec16f9056b3abc517b739140c9`.
- Upstream `docs/51-legal-decryption.md` says this reference came from an old
  upstream checkout. Its author's installed firmware was 4.89, but that
  installed module **was not compared** with the reference. Therefore 4.89
  is not an established version requirement. Do not fetch the module from
  old git history.

## Executable sequence

Use a local RPCS3 build with its writable config/data/cache directories inside
this workspace. Check that build's `--help` first. With a recent build:

```sh
rpcs3 --headless --installfw /absolute/path/to/PS3UPDAT.PUP
rpcs3 --decrypt /absolute/path/to/dev_flash/sys/external/libsre.sprx
rpcs3 --decrypt /absolute/path/to/staged/EBOOT.BIN
rpcs3 --decrypt /absolute/path/to/staged/file.sdat
```

`--decrypt` already selects a headless application. Repeat `--decrypt FILE`
to process multiple files in one invocation. It writes alongside each input:
`libsre.sprx -> libsre.prx`, `EBOOT.BIN -> EBOOT.elf`,
and `file.sdat -> file.sdat.unedat`. Stage copies before invoking to preserve
the original dump. The decrypter may prompt for KLIC when an input fails;
feed EOF and verify output size/hash independently. Exit status alone is
insufficient: RPCS3 can return zero after the prompt was abandoned.
[RPCS3 decrypter](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Crypto/decrypt_binaries.cpp)
and [CLI source](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/rpcs3.cpp).

`--headless --installfw` calls the installer with a null window. **Do not use
`--no-gui --installfw`**, which the CLI explicitly rejects. Headless firmware
installation avoids interactive confirmation and extracts `dev_flash`
modules, which remain encrypted until separately decrypted.
[Installer source](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/rpcs3qt/main_window.cpp).

## Firmware source and compatibility

Sony's current official page identifies **4.93** and supplies its download
under “Update using a computer.” Obtain that PUP through the official page,
install it into the isolated RPCS3 data directory, decrypt its `libsre.sprx`,
and compare to the reference hash before doing a libsre lift.
[Sony PS3 system software](https://www.playstation.com/en-us/support/hardware/ps3/system-software/).

If the current firmware module differs, retain its provenance/hash and assess
it explicitly; do not silently relax the input checker. The disc's 2.76 PUP
is another user-supplied source but has not been tested for compatibility.

Current `scripts/build_libsre.py` reads TOC dynamically from the module's own
exports (its former default was `0x30039AB0`). That helps with differing
modules, but does not establish compatibility:

- `scripts/gen_libsre_exports.py` places import bridges at `0x30040000` and
  assumes the image ends around `0x30032340`; check actual size/ranges.
- `scripts/gen_taskset_policy_blob.py` assumes policy vaddr `0x23680`.
- Runtime policy code in `runtime/spu/spu_dma.h` and `spu_workload.c`
  contains corresponding `0x30023680` assumptions. Some routes are optional,
  but alternate firmware needs a review of the paths actually enabled.
- `tools/setup/des_setup/game_table.json` currently admits one exact module
  hash only.

## Official firmware downloaded

Downloaded `.tools/downloads/PS3UPDAT-4.93.PUP` from the **exact HTTP href**
published on Sony's HTTPS support page:

`http://dus01.ps3.update.playstation.net/update/ps3/image/us/2026_0318_a2b60b6ac1d2e49e230144345616927c/PS3UPDAT.PUP`

The HTTPS variant of this firmware endpoint had a hostname/certificate
mismatch. TLS checks were not disabled. The official page's HTTP link worked.

- Actual size: 206,197,916 bytes, matching the endpoint's Content-Length.
- MD5: `a2b60b6ac1d2e49e230144345616927c`, matching URL and endpoint ETag.
- SHA-256: `158471fd834f8ea8036136b6aab43cd86c7ba73d79ca30e0af3c0fe0001cf365`.
- Parsed PUP version entry: `4.93\n`.
- No third-party firmware mirror was used.

## Official 4.93 module: measured compatibility, not reference equivalence

Root installed that PUP using RPCS3; installation extracted firmware but then
aborted during teardown (`analysis/firmware-install.log`). Separate module
decryption succeeded. Actual `work/firmware/libsre.prx` is 239,344 bytes,
SHA-256 `9e7ee6edc99b1d372686cf927b883b18f19ed4d32d1faf35e214cca4ae0737b6`.
It **does not match** the upstream reference hash. The reference table was
left unchanged; strict input auditing must still report that mismatch.

Ran `lift_prx.py` into `work/firmware-inspect`. These metrics match the older
reference's documented measurements in upstream `docs/22-libsre-lle-attempt.md`:

| Item | Actual official 4.93 module |
| --- | --- |
| Module / base | `cellSre_Library` / `0x30000000` |
| TOC | `0x30039AB0` |
| Relocated image size / end | 205,632 / `0x30032340` |
| Relocations | type 1 × 1,366; type 4 × 32; type 6 × 32 |
| Exports / imports / function seeds | 267 / 32 / 352 |
| Fully lifted function count | 756 |

All 32 import stubs decode successfully (30 `sysPrxForUser`, two `cellLibprof`).
Bridge OPDs occupy `0x30040000..0x30040100`, trampolines
`0x30040800..0x30040880`; neither overlaps this image. The policy module at
vaddr `0x23680` (file offset `0x23780`) starts with `0x4306DC02`, satisfying
the existing extraction script's layout assertion. Diagnostic YDKJ/CRI
policy routes remain optional and were not enabled by this inspection.

All eight configured LLE NIDs exist with the expected TOC:

| NID | Code address | OPD |
| --- | --- | --- |
| `011EE38B` | `0x30017134` | `0x30031844` |
| `1656D49F` | `0x300174F8` | `0x3003185C` |
| `8A85674D` | `0x300171B8` | `0x3003184C` |
| `73E06F91` | `0x300178D8` | `0x30031864` |
| `082BFB09` | `0x300164C0` | `0x300317EC` |
| `E5443BE7` | `0x30016778` | `0x300317FC` |
| `039D70B7` | `0x30016630` | `0x300317F4` |
| `91066667` | `0x30016CFC` | `0x30031824` |

The last code address is also explicitly documented for queue pop in upstream
`docs/28-in-game.md`.

At root's request, ran `scripts/build_libsre.py` using this official module;
outputs are in the normal upstream `work/libsre/` and `port/gen/` directories.
It exited zero, producing 756 functions / 479,854 source lines and glue for
267 exports / 32 imports. The first sandbox run failed on multiprocessing
forkserver binding; an approved escalated rerun completed.

The successful lift log (`analysis/libsre-lift.log`) has **26 warnings**:
“has no continuation at 0x3002E000; emitting halt”. The emitted fallback is
actually a C `return`, not a runtime trap. Twenty-five warning functions start
at or after `0x3002A0A0`, past the PPU text end `0x3001D718` documented upstream.
The remaining warning is exported `cellSpudll` NID `3FBCF1D6`, code
`0x3001D6B4`; its ordinary control flow already returns at `0x3001D714`,
before the decoder's long unreachable tail and fallback. None of the eight
required LLE entry functions warns. These observations lower concern for the
planned baseline, but dynamic reachability has not been exhaustively proven.
Prior documentation supplies the same function count, **not** a warning count.

This supports an explicitly **experimental PC build** using official 4.93.
It does not prove byte equivalence to the unknown reference module or a working
game. Machine-readable measurements: `analysis/libsre-compatibility.json`.
