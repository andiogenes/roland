# Roland: drum machine firmware for the M-VAVE FM-1

Drum-only firmware for the M-VAVE FM-1, built on [Felucca](https://github.com/hugelton/Felucca) (GPL-3.0-only). It keeps
Felucca's drum engine and its sequencer and offers nothing else: four drum tracks, each with its own pattern, and a choice of

* **synthesized kits** (Felucca's own DRUM engine: DRUM, DEEP, TIGHT, PERC and the 80 / 10 / 66 / 55 / 77 machine kits), and
* **Alesis HR-16B sample kits** (`HR-A`, `HR-B`, ...), built from the WAV files of the
  [Cluster Sound HR-16B freebie](https://clustersound.com/product/freebies/hr-16b/).

Both kinds are KIT values of the same DRUM engine, so they share the 8-lane grid (KICK SNARE CLAP HATCL HATOP TOM RIM
BELL), the lane mutes and levels, ratchets, chance, conditions, parameter locks, song chaining and MIDI. Any track can
use any kit, so a synthesized kick can play under HR-16 hats.

## Status: read this first

| | |
|---|---|
| Sample importer (`overlay/tools/gen_hr16.py`) | **Tested**: 12 tests on synthetic WAVs, including a gcc build of the generated header and a bit-exact check of the firmware's ADPCM decode against the generator's. |
| Firmware changes (`patches/felucca-drum.patch`) | **Not compiled, not run.** Written by reading Felucca 1.8.1's source. The JieLi toolchain and SDK could not be fetched in the session that wrote this, and Felucca's host tests were not run. Expect to fix compile errors on the first build. The patch does apply cleanly to the pinned Felucca commit. |
| On the FM-1 | **Not tested.** Installing firmware is at your own risk; Felucca's installer offers a way back to the official firmware, and recovery from a failed install needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter). |
| HR-16B samples | **Not included and never fetched.** `clustersound.com` was blocked in the session that wrote this, so nothing here is derived from the real pack. File-name matching in the importer is a best guess; run `--list` and check it. |
| Flash size | **Unknown.** The 98 KiB sample budget (`HR16_BUDGET`) is a guess about what fits beside Felucca; the build's linker check will tell. Felucca's CC0 piano/flute/sax and the SLICE engine are dropped to make room. |

## Build

You need what Felucca's [BUILDING.md](https://github.com/hugelton/Felucca/blob/main/BUILDING.md) lists: Python 3 with
Pillow and fonttools, the JieLi toolchain (`tools/get_toolchain.sh` in Felucca) and the AC79 SDK (Linux x86-64 runs it
natively, otherwise Docker).

1. Download the HR-16B pack and put its WAV files in `assets/hr16/` (git-ignored; check the pack's licence before you
   share an image that contains it).
2. Check how the files will be sorted into lanes:
   ```
   python3 overlay/tools/gen_hr16.py --list assets/hr16
   ```
   If a lane is empty or a file is in the wrong one, write a map (format in the script's docstring) and set `HR16_MAP`.
3. Build:
   ```
   ./build_drum.sh            # -> out/roland-drum.fwsc
   ```
   It clones Felucca at the pinned commit, applies the patch, copies `overlay/`, and runs Felucca's own `build.sh` with
   `FELUCCA_DRUMONLY=1 FELUCCA_SLICE=0 FELUCCA_HR16=1`. With no WAV files it builds synthesized drums only.
4. Install as Felucca's BUILDING.md describes (`tools/fm1_install.py out/roland-drum.fwsc`, or a local copy of its web
   installer).

Tests of the importer: `python3 -I tests/test_gen_hr16.py` (needs `gcc`).

## What changed from Felucca

* `FELUCCA_DRUMONLY=1`: the only engine offered is DRUM (PRESETS, the EDIT layer and the editor list); the other engines
  stay in the table with their numbers so stores keep loading; a sound of any other engine plays as DRUM. Power-on:
  DRUM KIT, DEEP KIT, TIGHT KIT, and the first HR-16 kit on track 4.
* `FELUCCA_HR16=1`: KIT values `HR-A`.. appended after PERC. A hit plays one IMA ADPCM sample per lane, resampled by
  **TUNE**; **DECY** below 64 shortens it, **TONE** below 64 darkens it, **DRV** distorts it; the closed hat chokes the open
  hat. **SNAP, KICK and ACC do nothing** on a sample kit. Level and accent come from velocity. GM notes keep Felucca's
  pitch offsets, so one tom sample plays all the toms.
* Samples are mono, trimmed, normalised per lane and stored at 22.05 kHz (`HR16_RATE`), one-shot.
* Not removed: the other engines' code is still compiled in (it only costs flash), and the editor can still switch a
  track to another engine.

Kit k takes the k-th file of each lane (wrapping), up to `HR16_MAX_KITS` (6) kits; kits are dropped from the end to fit
`HR16_BUDGET`.

## Licence

GPL-3.0-only, as Felucca; see `LICENSE`. The patch is a derivative of Felucca. Felucca's engines and hardware layer
keep their own licences ([LICENSING.md](https://github.com/hugelton/Felucca/blob/main/LICENSING.md)). M-VAVE, FM-1 and
Alesis are trademarks of their owners; this project is not affiliated with them or with Felucca's author.
