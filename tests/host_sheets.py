#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Pictures of the host renders (host_render.sh): PPM -> PNG, a sheet of the screens of some kits, and a waveform /
spectrogram chart of the kit renders.   host_sheets.py SHOTS_DIR AUDIO_DIR OUT_DIR   (Pillow, numpy)"""
import sys
import wave
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

shots, audio, out = (Path(a) for a in sys.argv[1:4])
out.mkdir(parents=True, exist_ok=True)
for f in sorted((shots / "ppm").glob("*.ppm")):
    Image.open(f).convert("RGB").save(shots / (f.stem + ".png"))


def sheet(kits, kinds, name, scale):
    W = H = 240 * scale
    img = Image.new("RGB", (W * len(kinds), (H + 16) * len(kits)), (24, 24, 24))
    dr = ImageDraw.Draw(img)
    for r, k in enumerate(kits):
        for c, kind in enumerate(kinds):
            p = shots / f"GREY_{kind}_{k}.png"
            if not p.exists():
                continue
            img.paste(Image.open(p).resize((W, H), Image.NEAREST), (c * W, r * (H + 16) + 16))
            dr.text((c * W + 4, r * (H + 16) + 2), f"{kind} {k}", fill=(200, 200, 200))
    img.save(out / name)


sheet(["TR606", "ACOUS", "AC-DU", "HR-A"], ["grid", "kit", "hit"], "screens_new_kits.png", 1)
sheet(["STD", "TR606", "ACOUS"], ["grid"], "screens_grid.png", 2)

# the same screen in every palette
pals = [p for p in ("GREY", "MONO", "GREEN", "PAPER", "NIGHT") if (shots / f"{p}_grid_TR606.png").exists()]
if pals:
    img = Image.new("RGB", (240 * len(pals) + 8 * (len(pals) - 1), 256), (24, 24, 24))
    dr = ImageDraw.Draw(img)
    for i, pl in enumerate(pals):
        img.paste(Image.open(shots / f"{pl}_grid_TR606.png"), (i * 248, 16))
        dr.text((i * 248 + 4, 2), pl, fill=(200, 200, 200))
    img.save(out / "screens_palettes.png")
# one screen big
Image.open(shots / "GREY_grid_TR606.png").resize((720, 720), Image.NEAREST).save(out / "screen_grid_big.png")
Image.open(shots / "GREY_kit_ACOUS.png").resize((720, 720), Image.NEAREST).save(out / "screen_kit_big.png")

# the INSERT's four devices (ui_faces.c): page 1 and page 2 of each, in colour (NIGHT), then big
devs = ["COMP", "PRE", "HARM", "METAL", "ROT"]
if all((shots / f"NIGHT_insert1_{d}.png").exists() for d in devs):
    img = Image.new("RGB", (240 * 5 + 32, 2 * 256), (24, 24, 24))
    dr = ImageDraw.Draw(img)
    for i, d in enumerate(devs):
        for pg in (1, 2):
            img.paste(Image.open(shots / f"NIGHT_insert{pg}_{d}.png"), (i * 248, (pg - 1) * 256 + 16))
            dr.text((i * 248 + 4, (pg - 1) * 256 + 2), f"{d}  INSERT{'' if pg == 1 else ' 2'}", fill=(200, 200, 200))
    img.save(out / "screens_insert.png")
    for d in devs:                        # the panel of each, 3x (a face is 220 x 88 on the device)
        Image.open(shots / f"NIGHT_insert1_{d}.png").crop((0, 0, 240, 126)).resize((720, 378), Image.NEAREST).save(out / f"insert_{d}.png")

if (shots / "NIGHT_master_A.png").exists():      # the master chain's pages (MASTER A: PRE, MASTER B: COMP)
    img = Image.new("RGB", (240 * 3 + 16, 256), (24, 24, 24))
    dr = ImageDraw.Draw(img)
    for i, (n, t) in enumerate((("master_A", "MASTER A: PRE"), ("master_B", "MASTER B: COMP"), ("reverb_page", "REVERB: the Dragonfly Room"))):
        img.paste(Image.open(shots / f"NIGHT_{n}.png"), (i * 248, 16))
        dr.text((i * 248 + 4, 2), t, fill=(200, 200, 200))
    img.save(out / "screens_master.png")

# waveform + spectrogram per kit
kits = [(f.stem, f.stem.split("_", 2)[2]) for f in sorted(audio.glob("kit_*.wav"))]
W, RH, SH = 900, 110, 90
img = Image.new("RGB", (W, (RH + SH + 22) * len(kits)), (20, 20, 20))
dr = ImageDraw.Draw(img)
for r, (stem, label) in enumerate(kits):
    w = wave.open(str(audio / f"{stem}.wav"))
    x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").reshape(-1, 2).astype(float).mean(1)[:int(4.2 * 44100)]
    y0 = r * (RH + SH + 22)
    dr.text((6, y0 + 3), label, fill=(220, 220, 220))
    mid = y0 + 20 + RH // 2
    for c, seg in enumerate(np.array_split(x, W)):
        a = np.abs(seg).max() / 32768 * (RH / 2 - 2)
        dr.line([(c, mid - a), (c, mid + a)], fill=(120, 200, 255))
    nfft, hop = 1024, len(x) // W
    cols = []
    for c in range(W):
        s = x[c * hop:c * hop + nfft]
        s = np.pad(s, (0, nfft - len(s))) if len(s) < nfft else s
        cols.append(np.abs(np.fft.rfft(s * np.hanning(nfft)))[:nfft // 2])
    sp = 20 * np.log10(np.array(cols).T + 1) / (20 * np.log10(32768 * nfft / 4))
    sp = np.clip((sp - 0.15) / 0.6, 0, 1)
    bins = np.logspace(np.log10(2), np.log10(nfft // 2 - 1), SH).astype(int)[::-1]
    img.paste(Image.fromarray((sp[bins] * 255).astype(np.uint8), "L").convert("RGB"), (0, y0 + 20 + RH))
img.save(out / "audio_overview.png")
print(f"host_sheets: {len(kits)} kits, PNGs in {out}")
