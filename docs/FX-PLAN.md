# The effects pool: plan and status

**Decisions** (yours, from the planning round): one INSERT per track **and** a two-slot master chain; the pedals under neutral
on-screen names (credited by their real names here and in the README); the Harmonic Percolator in its **smoother, Albini-style**
variant; the reverb is the **Dragonfly Room** only (no Hall, no Plate); the other FX-layer effects (REPEAT, REVERSE, TAPE STOP,
FREEZE, OCT UP / DN, REV / ECHO THROW, the filters) stay.

**The pool**

| # | Effect | Where | Source |
|---|---|---|---|
| 1 | Harmonic Percolator (Albini-style) | INSERT | [Madbean Pepper Spray PDF](https://madbeanpedals.com/projects/_folders/StandardSeries/pdf/PepperSpray.pdf), [Amplified Parts stock guide](https://www.amplifiedparts.com/pedal-guides/build-project-harmonic-percolator-stock-version) |
| 2 | BOSS HM-2 | INSERT | AionFX **Graviton** documentation v1.1.1 (a clone of the HM-2: full schematic, parts list, notes), supplied by you |
| 3 | Intersound IVP preamp | INSERT, master | AionFX **IVP Preamp** documentation v2.0.0 (controls and ranges, signal flow, schematic, parts list), supplied by you |
| 4 | 1178-style (1176 FET) compressor | INSERT, master | to model from the published 1176 behaviour (4 / 8 / 12 / 20, all buttons) |
| 5 | Tape delay | the delay send | reuses the 128 KB line |
| 6 | Tape rot | INSERT, master | wow, flutter, dropouts, high-frequency loss, hiss, saturation |
| 7 | Dragonfly Room | the reverb send | [dragonfly-reverb](https://github.com/michaelwillis/dragonfly-reverb) (GPL-3.0; its core is Freeverb3's progenitor2 + early reflections) |

**Order of work and status**

| Step | | Status |
|---|---|---|
| 1 | Remove DIST, the SLICER, the chorus, the old INSERT types, FLANGER and PHASER | **Done.** Audio of every kit bit-identical; whole firmware type-checks; UI lint clean. |
| 2 | Float reference models (Python / SPICE) and test signals | **Started.** The HM-2 and IVP documents are read. `tools/fxlab/gyrator.py` holds their component values, and `tests/test_fxlab.py` checks them against what the documents state in words (all match). ngspice is installed for the nonlinear stages. Next: SPICE netlists of both circuits, and the Percolator (schematic supplied; values recorded). |
| 3 | Compressor, tape delay, tape rot | **Done.** Compressor (`fx_comp.c`, INSERT type COMP; test_comp.py; kits unchanged with INSERT off). **Tape delay done** (`fx_tape.c`, the delay send: wow / flutter, tape saturation, head loss; test_tape.py; default kit renders unchanged). **Tape rot done** (`fx_rot.c`, INSERT type ROT: WOW, DROP, LOSS, HISS, DRIVE; test_rot.py). Step 3 is complete. |
| 4 | HM-2, Percolator, IVP | **HM-2 done**: a SPICE netlist of the Graviton schematic (`tools/fxlab/spice/hm2.py`), a fit (`fit_hm2.py`) and a float model (`hm2_model.py`); the firmware's `fx_hm2.c` and its generated tables; `tests/test_hm2.py`. The distortion pot's wiring is read from the schematic as drawn (wiper to ground); the circuit's small-signal gain moves 7 dB over the knob, which the model keeps. **Percolator done** (`spice/perc.py`, `perc_model.py`, `fx_perc.c`, `tests/test_perc.py`; the netlist's two transistors in series bias with the germanium one near saturation, which the SPICE model reproduces and the fit keeps; the fit is looser than the HM-2's: level within about 3 dB, the second harmonic roughly). IVP: not started. |
| 5 | Dragonfly Room (fixed point, 22.05 kHz) | Not started |
| 6 | UI, parameters, presets, the master chain, docs | **Started.** The INSERT's four device faces (`overlay/firmware/src/ui_faces.c`: COMP, PRE, HARM, METAL) and their parameters (A to E, TYPE names) are in and linted; the types pass the sound unchanged until steps 3 and 4. Next: the master chain's two slots, the tape effects' pages, presets. |

Every step is checked on the host: renders against goldens, response and distortion curves against the reference models, the
instruction counts against the CPU budget (about 59 instructions a sample per 1 % of the audio CPU), the whole-firmware
type-check and the UI lint. None of it says anything about the FM-1's own speed or flash; the real build is not possible yet.

**Answered**

* "Felucca clipping should go": DIST, the old INSERT drives and the removed engines' drives are gone; **the master limiter and
  soft clip stay** (the last stage before the DAC, which stops the sum wrapping), as you confirmed.
* The modulation matrix's `DIST` and `CHO` destinations are retired: they load as OFF and the knob steps over them. Turning
  AFTERTOUCH on for a drum track now routes it to AMP (the level) instead of the old DIST.

**Still needed**

* Nothing is missing now: the Percolator's schematic (AionFX Particle documentation v1.1.1) is supplied and its Albini and stock
  values are recorded in `tools/fxlab/gyrator.py`.
