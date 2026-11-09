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
| 2 | Float reference models (Python / SPICE) and test signals | **Started.** The HM-2 and IVP documents are read. `tools/fxlab/gyrator.py` holds their component values, and `tests/test_fxlab.py` checks them against what the documents state in words (all match). ngspice is installed for the nonlinear stages. Next: SPICE netlists of both circuits, and the Percolator (needs its schematic: see below). |
| 3 | Compressor, tape delay, tape rot | Not started |
| 4 | HM-2, Percolator, IVP | Not started |
| 5 | Dragonfly Room (fixed point, 22.05 kHz) | Not started |
| 6 | UI, parameters, presets, the master chain, docs | Not started |

Every step is checked on the host: renders against goldens, response and distortion curves against the reference models, the
instruction counts against the CPU budget (about 59 instructions a sample per 1 % of the audio CPU), the whole-firmware
type-check and the UI lint. None of it says anything about the FM-1's own speed or flash; the real build is not possible yet.

**Answered**

* "Felucca clipping should go": DIST, the old INSERT drives and the removed engines' drives are gone; **the master limiter and
  soft clip stay** (the last stage before the DAC, which stops the sum wrapping), as you confirmed.
* The modulation matrix's `DIST` and `CHO` destinations are retired: they load as OFF and the knob steps over them. Turning
  AFTERTOUCH on for a drum track now routes it to AMP (the level) instead of the old DIST.

**Still needed**

* **The Percolator's schematic.** The [Madbean Pepper Spray PDF](https://madbeanpedals.com/projects/_folders/StandardSeries/pdf/PepperSpray.pdf)
  (the Albini-style variant) is on a host this session cannot reach. Please upload it (and, if you have it, the Amplified Parts
  stock-version guide for comparison); without a schematic I would only be able to model its behaviour, not its circuit.
