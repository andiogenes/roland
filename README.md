# Roland: drum machine firmware for the M-VAVE FM-1

*The name is a nod to Steve Albini's Big Black, whose records credit their drummer as "Roland": a Roland TR-606 drum
machine.*

Drum-only firmware for the M-VAVE FM-1, built on [Felucca](https://github.com/hugelton/Felucca) (GPL-3.0-only). It keeps
Felucca's drum engine and its sequencer and offers nothing else: four drum tracks, each with its own pattern, and a choice of

* **synthesized kits** (Felucca's own DRUM engine: DRUM, DEEP, TIGHT, PERC and the 80 / 10 / 66 / 55 / 77 machine kits), and
* **a TR-606 kit** (`TR606`), SLOOP's synthesized TR-606,
* **SLOOP's acoustic drum set** (`ACOUS`, `AC-DP`, `AC-TI`, `AC-BR`, `AC-DU`): real drum recordings in five treatments, and
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
| Acoustic kit (`assets/acoustic`, `tests/test_acoustic.py`) | **Tested on the host.** The sample player (including the five treatments, TUNE, DECY, TONE and the hat choke) is cut out of the patch, compiled warning-free and run on the real recordings: every sound plays and ends, DEEP/BRIGHT/DUST shift the pitch by the right amount, TIGHT shortens the tails, DUST is 8-bit. Not heard. |
| TR606 kit (`overlay/firmware/src/drum_synth606.c`, `overlay/tools/gen_tr606.py`) | **Partly tested.** SLOOP's synth code compiles warning-free on the host against stand-ins for Felucca's DSP helpers, and all 16 sounds of the kit render, end, and choke as expected (`tests/test_tr606.py`). It was not heard, not compared with SLOOP's own output, and its level against Felucca's kits is a guess (`TR606_GAIN_DB`, default -3). Its integration into `eng_drum.c` is uncompiled like the rest of the patch. |
| On the FM-1 | **Not tested.** Installing firmware is at your own risk; Felucca's installer offers a way back to the official firmware, and recovery from a failed install needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter). |
| HR-16B samples | **Not included** (git-ignored). The importer was run on the Cluster Sound *Ableton Live* edition (`CS-HR-16B-Live.zip`, the WAVs in its `HR-16B/Samples/` folder, names like `cs_big_foot_kk.wav`): 47 files, 37 sorted into lanes, 3 kits (`HR-A..C`, 20 samples, 89 090 bytes) fit the default budget, and the decoded output has sane levels with no clipping. Other editions of the pack may name files differently: run `--list` and check. The 10 FX hits (bottles, pipe, ...) are not used. |
| Flash size | **Unknown.** The 98 KiB sample budget (`HR16_BUDGET`) is a guess about what fits beside Felucca; the build's linker check will tell. Felucca's CC0 piano/flute/sax and the SLICE engine are dropped to make room. |

## Build

You need what Felucca's [BUILDING.md](https://github.com/hugelton/Felucca/blob/main/BUILDING.md) lists: Python 3 with
Pillow and fonttools, the JieLi toolchain (`tools/get_toolchain.sh` in Felucca) and the AC79 SDK (Linux x86-64 runs it
natively, otherwise Docker).

1. Download the HR-16B pack and put its WAV files in `assets/hr16/` (for the Live edition: everything in `HR-16B/Samples/`, not the `.adg` or `.asd` files) (git-ignored; check the pack's licence before you
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
   `FELUCCA_DRUMONLY=1 FELUCCA_SLICE=0 FELUCCA_TR606=1 FELUCCA_ACOUSTIC=1 FELUCCA_HR16=1`. With no HR-16 WAV files it builds
   without the HR-16 kits.
4. Install as Felucca's BUILDING.md describes (`tools/fm1_install.py out/roland-drum.fwsc`, or a local copy of its web
   installer).

Tests: `python3 -I tests/test_gen_hr16.py`, `tests/test_tr606.py` and `tests/test_acoustic.py` (need `gcc`).

## What changed from Felucca

* `FELUCCA_DRUMONLY=1`: the only engine offered is DRUM (PRESETS, the EDIT layer and the editor list); the other engines
  stay in the table with their numbers so stores keep loading; a sound of any other engine plays as DRUM. Power-on:
  DRUM KIT, TR-606, TIGHT KIT, and the acoustic kit on track 4.
* `FELUCCA_HR16=1`: KIT values `HR-A`.. appended after PERC. A hit plays one IMA ADPCM sample per lane, resampled by
  **TUNE**; **DECY** below 64 shortens it, **TONE** below 64 darkens it, **DRV** distorts it; the closed hat chokes the open
  hat. **SNAP, KICK and ACC do nothing** on a sample kit. Level and accent come from velocity. GM notes keep Felucca's
  pitch offsets, so one tom sample plays all the toms.
* `FELUCCA_TR606=1`: KIT value `TR606` and a `TR-606` preset. The sounds, the synth that plays them and the kick tuning for
  small speakers are [SLOOP's](https://github.com/isod89/sloop-fm1) (GPL-3.0; thanks to isod89), ported to Felucca's DRUM
  engine: SLOOP's 808 kit with the 606's changes (kick, snare, hats, toms, cymbal). A TR-606 has no clap, rim, conga,
  cowbell or clave: SLOOP's 606 keeps the 808's behind the GM notes that ask for them. Each GM note plays SLOOP's own sound
  for it (16 sounds, so the congas, ride, shaker and clave are there), on the nearest of the 8 lanes. **TUNE** moves the
  pitch, **DECY** the decays (per hit, -4..+3 steps), **DRV** works; **TONE, SNAP, KICK and ACC do nothing** on it. The closed
  hat chokes the open hat.
* `FELUCCA_ACOUSTIC=1`: SLOOP's acoustic drum set, as five KIT values over one set of 8 recordings. The recordings are
  [VCSL](https://github.com/sgossner/VCSL) (Versilian Studios, CC0 1.0) in the cut, normalised mono versions SLOOP made;
  they are in `assets/acoustic/` with their attribution. The treatments are SLOOP's: `ACOUS` as recorded; `AC-DP` pitched
  down (5 semitones on the kick, 2 elsewhere) and softened; `AC-TI` with a quick decay; `AC-BR` up 2 semitones; `AC-DU`
  down 1 semitone, darker and 8-bit. The kick and toms get SLOOP's shorter body, the cymbals its 0.6 s with a fade, and the
  hats and cymbal keep a 32 kHz rate. **The 8 lanes carry one sample each**, so SLOOP's separate hi tom, ride, cowbell,
  conga, shaker, claves and tambourine are not in it: the TOM lane has the low tom (the other toms are its pitches), and
  the BELL lane the crash. Same knobs as the HR-16 kits.
* Samples are mono, trimmed, normalised per lane and stored at 22.05 kHz (`HR16_RATE`), one-shot.
* Not removed: the other engines' code is still compiled in (it only costs flash), and the editor can still switch a
  track to another engine.

Kit k takes the k-th file of each lane (wrapping), up to `HR16_MAX_KITS` (6) HR-16 kits. All sample kits share
`HR16_BUDGET` (144 KiB by default, about 135 KiB with the three kits the Live edition of the pack gives); HR-16 kits are
dropped from the end to fit, the acoustic kit never (it takes about 50 KiB).

## Credits

[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments; [SLOOP](https://github.com/isod89/sloop-fm1)
by isod89 (the TR-606 kit and the synth that plays it, the acoustic kit and its treatments); the acoustic recordings are
Versilian Studios' VCSL (CC0); the Alesis HR-16B samples are Cluster Sound's.

## Licence

GPL-3.0-only, as Felucca; see `LICENSE`. The patch is a derivative of Felucca. Felucca's engines and hardware layer
keep their own licences ([LICENSING.md](https://github.com/hugelton/Felucca/blob/main/LICENSING.md)). M-VAVE, FM-1 and
Alesis and Roland (TR-606) are trademarks of their owners; this project is not affiliated with them or with Felucca's author.
