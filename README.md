# Roland: drum machine firmware for the M-VAVE FM-1

*The name is a nod to Steve Albini's Big Black, whose records credit their drummer as "Roland": a Roland TR-606 drum
machine.*

*(Steve Albini would hate this, of course. It has presets, a software update and a README with pictures. We like to think he
would at least have approved of the drummer's reliability: never late, never drunk, never asks for a bigger monitor mix. And
there isn't a single microphone in it to have an opinion about.)*

<p align="center"><img src="docs/host-render/screens_palettes.png" alt="The drum grid in the five colour schemes" width="100%"></p>
<p align="center"><sub>The 8-lane drum grid on a TR-606 kit, drawn by the firmware's own UI code in its five colour schemes
(host render: the real screen code on a test display, not a photo of the FM-1).</sub></p>

Drum-only firmware for the M-VAVE FM-1, built on [Felucca](https://github.com/hugelton/Felucca) (GPL-3.0-only). It keeps
Felucca's drum engine and its sequencer and offers nothing else: four drum tracks, each with its own pattern, and a choice of

* **synthesized kits** (Felucca's own DRUM engine: DRUM, DEEP, TIGHT, PERC and the 80 / 10 / 66 / 55 / 77 machine kits),
* **a TR-606 kit** (`TR606`), SLOOP's synthesized TR-606,
* **SLOOP's acoustic drum set** (`ACOUS`, `AC-DP`, `AC-TI`, `AC-BR`, `AC-DU`): real drum recordings in five treatments, and
* **Alesis HR-16B sample kits** (`HR-A`, `HR-B`, ...), built from the WAV files of the
  [Cluster Sound HR-16B freebie](https://clustersound.com/product/freebies/hr-16b/).

All of them are KIT values of the same DRUM engine, so they share the 8-lane grid (KICK SNARE CLAP HATCL HATOP TOM RIM
BELL), the lane mutes and levels, ratchets, chance, conditions, parameter locks, song chaining and MIDI. Any track can
use any kit, so a synthesized kick can play under HR-16 hats.

## Listen

Each clip is the same two-bar GM beat (kick, snare, claps, closed and open hats, toms, rim, cowbell, a crash), 4 seconds,
rendered by the firmware's own engine, mixer and effects on a PC (so no FM-1 speaker, and no knob moves). Click a link, then
the download or "view raw" button. All mono mp3.

| Kit | What it is | Clip |
|---|---|---|
| `STD` | Felucca's own synthesized kit (the power-on kit) | [kit_00_STD.mp3](docs/audio/kit_00_STD.mp3) |
| `80` `10` `66` `55` `77` | Felucca's five model kits | [80](docs/audio/kit_04_80.mp3) · [10](docs/audio/kit_05_10.mp3) · [66](docs/audio/kit_06_66.mp3) · [55](docs/audio/kit_07_55.mp3) · [77](docs/audio/kit_08_77.mp3) |
| `DEEP` `TIGHT` `PERC` | Felucca's kits put together from the model kits' pieces | [DEEP](docs/audio/kit_09_DEEP.mp3) · [TIGHT](docs/audio/kit_10_TIGHT.mp3) · [PERC](docs/audio/kit_11_PERC.mp3) |
| `TR606` | SLOOP's synthesized TR-606 (hats of about 35 and 220 ms, a crash of about 0.7 s) | [kit_12_TR606.mp3](docs/audio/kit_12_TR606.mp3) |
| `ACOUS` | SLOOP's acoustic kit as recorded | [kit_13_ACOUS.mp3](docs/audio/kit_13_ACOUS.mp3) |
| `AC-DP` | the same, pitched down and softened | [kit_14_AC-DP.mp3](docs/audio/kit_14_AC-DP.mp3) |
| `AC-TI` | the same with a quick decay | [kit_15_AC-TI.mp3](docs/audio/kit_15_AC-TI.mp3) |
| `AC-BR` | the same, pitched up 2 semitones | [kit_16_AC-BR.mp3](docs/audio/kit_16_AC-BR.mp3) |
| `AC-DU` | the same, down a semitone, darker and 8-bit | [kit_17_AC-DU.mp3](docs/audio/kit_17_AC-DU.mp3) |
| `HR-A` `HR-B` ... | the HR-16B kits | *no clips: that pack's licence is not known to me, so its sounds are not published here* |

<p align="center"><img src="docs/host-render/audio_overview.png" alt="Waveform and spectrogram of each kit playing the same beat" width="100%"></p>
<p align="center"><sub>Each kit playing the same beat: waveform on top, spectrogram (frequency on a log scale) below.</sub></p>

## Pictures

<p align="center">
  <img src="docs/host-render/screen_grid_big.png" alt="The drum grid, TR606 kit" width="360">
  <img src="docs/host-render/screen_kit_big.png" alt="The KIT page, ACOUS kit" width="360">
</p>
<p align="center"><sub>Left: the drum grid (BD SD CP CH OH TM RS CB). Right: the KIT page of the ACOUS kit. The picture in the
middle of the KIT page is the test display's stand-in signal, not the sound. The row at the bottom says ACOUSTIC because it
is the track's preset, and the host render sets the kit directly.</sub></p>

<p align="center"><img src="docs/host-render/screens_new_kits.png" alt="Grid, KIT and HIT pages of four of the new kits" width="720"></p>
<p align="center"><sub>The grid, KIT and HIT pages for TR606, ACOUS, AC-DU and HR-A: the new kit names fit the screen.</sub></p>

## Status: read this first

| | |
|---|---|
| Firmware sources (`patches/felucca-drum.patch`) | **Compile and run on the host** (`./host_render.sh`, below): the patched engine, the ported SLOOP synth and the sample player build with gcc through Felucca's own test harness, and every kit renders. **Not built for the FM-1**: the JieLi toolchain could not be fetched, so there is no `.fwsc`. Expect the unexpected there (flash size, the audio interrupt's CPU budget). |
| On the FM-1 | **Not tested.** Installing firmware is at your own risk; Felucca's installer offers a way back to the official firmware, and recovery from a failed install needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter). |
| Sound | **Not heard by me.** The clips and charts are the engine's real output, and I checked levels, lengths, pitches and clipping numerically, but nobody has listened critically, and the FM-1's own speaker and codec are not in the loop. |
| Sample importer (`overlay/tools/gen_hr16.py`) | **Tested**: 13 tests on synthetic WAVs and on the bundled recordings, including a gcc build of the generated header, a bit-exact check of the firmware's ADPCM decode against the generator's, and the budget logic. |
| TR606 kit | SLOOP's synth code compiles warning-free and all 16 sounds render, end and choke (`tests/test_tr606.py`); not compared with SLOOP's own output. Its level against Felucca's kits is a guess (`TR606_GAIN_DB`, default -3). |
| Acoustic kit | The sample player and its five treatments are cut out of the patch, compiled and run on the real recordings (`tests/test_acoustic.py`). |
| HR-16B samples | **Not included** (git-ignored). The importer was run on the Cluster Sound *Ableton Live* edition (`CS-HR-16B-Live.zip`, the WAVs in its `HR-16B/Samples/` folder, names like `cs_big_foot_kk.wav`): 47 files, 37 sorted into lanes. Other editions may name files differently: run `--list` and check. The 10 FX hits (bottles, pipe, ...) are not used. |
| Flash size | **Unknown.** The 144 KiB sample budget (`HR16_BUDGET`) for all sample kits together is a guess about what fits beside Felucca; the build's linker check will tell, and HR-16 kits are dropped first to fit. Felucca's CC0 piano/flute/sax and the SLICE engine are dropped to make room. |

## Host render: what the firmware sources do (no FM-1 needed)

`./host_render.sh` builds the **same patched firmware sources** for the host with gcc (Felucca's own test harness: the real
engines, voices, mixer, FX, sequencer and UI code on a stub display) and captures what they do. It clones Felucca, applies
the patch, generates the tables, compiles, and writes `out/host/`: a WAV and an mp3 per kit, all kits in one file, the screens
as PNGs, and the charts above (`PUBLISH=1` also copies the shareable ones into `docs/`). About 40 seconds.

* **It compiles.** With `FELUCCA_DRUMONLY=1 FELUCCA_SLICE=0 FELUCCA_TR606=1 FELUCCA_ACOUSTIC=1 FELUCCA_HR16=1` the only
  warnings are Felucca's (two unused variables with SLICE off, 64-bit pointer casts in the hardware layer).
* **All the kits render** (18 with the three HR-16 ones; 21 KIT values, 3 of them retired aliases) a two-bar GM beat through
  the real mixer and FX with **no clipped sample** ([levels](docs/host-render/kit_levels.txt)). Synthesized kits sit at
  3.7-5.1 k RMS (TR606 3.7 k at the default -3 dB); the acoustic kits are quieter, 2.0-2.8 k, the HR-16 ones
  3.7-3.9 k. The acoustic kits could take about 3-5 dB more (`PEAK` in `gen_hr16.py`); not changed without listening.
* **Felucca's UI lint passes** on the patched build: 233 screens x 10 palettes, 0 layout findings, 0 alignment errors
  ([log](docs/host-render/ui_lint.txt)).

This says nothing about the FM-1's own CPU budget (the render is faster than real time on a PC), flash size, or how it
sounds on the speaker.

## Build

You need what Felucca's [BUILDING.md](https://github.com/hugelton/Felucca/blob/main/BUILDING.md) lists: Python 3 with
Pillow and fonttools, the JieLi toolchain (`tools/get_toolchain.sh` in Felucca) and the AC79 SDK (Linux x86-64 runs it
natively, otherwise Docker).

1. Optional: download the HR-16B pack and put its WAV files in `assets/hr16/` (for the Live edition: everything in
   `HR-16B/Samples/`, not the `.adg` or `.asd` files). They are git-ignored; check the pack's licence before you share an
   image that contains them.
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
   without the HR-16 kits; `FELUCCA_TR606=0` and `FELUCCA_ACOUSTIC=0` leave those kits out.
4. Install as Felucca's BUILDING.md describes (`tools/fm1_install.py out/roland-drum.fwsc`, or a local copy of its web
   installer).

Tests: `python3 -I tests/test_gen_hr16.py`, `tests/test_tr606.py` and `tests/test_acoustic.py` (27 tests; need `gcc`).

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
* ADPCM sample kits are mono, trimmed, normalised per lane and stored at 22.05 kHz (`HR16_RATE`), one-shot.
* Not removed: the other engines' code is still compiled in (it only costs flash), and the editor can still switch a
  track to another engine.

Kit k of the HR-16 pack takes the k-th file of each lane (wrapping), up to `HR16_MAX_KITS` (6). All sample kits share
`HR16_BUDGET` (144 KiB by default). The acoustic kit comes first and is never dropped (it takes about 50 KiB); HR-16 kits are
dropped from the end to fit (the three of the Live edition together take about 89 KiB).

## Credits

[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments; [SLOOP](https://github.com/isod89/sloop-fm1)
by isod89 (the TR-606 kit and the synth that plays it, the acoustic kit and its treatments);
the acoustic recordings are Versilian Studios' VCSL (CC0); the Alesis HR-16B samples are Cluster Sound's.

## Licence

GPL-3.0-only, as Felucca; see `LICENSE`. The patch is a derivative of Felucca. Felucca's engines and hardware layer
keep their own licences ([LICENSING.md](https://github.com/hugelton/Felucca/blob/main/LICENSING.md)). M-VAVE, FM-1, Alesis,
Roland (TR-606) are trademarks of their owners; this project is not affiliated with them or with Felucca's or SLOOP's
authors.
