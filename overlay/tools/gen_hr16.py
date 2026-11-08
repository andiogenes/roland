#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build DRUM's HR-16 sample kits from a folder of WAV files into a C header (felucca_hr16.h).

  gen_hr16.py OUT.h                 (tools/build.py with FELUCCA_HR16=1; the folder is HR16_DIR, default assets/hr16)
  gen_hr16.py --list [DIR]          show which file each lane would take, change nothing
  gen_hr16.py OUT.h --preview DIR   also write what the firmware will play (decoded ADPCM) as WAV files into DIR

Environment (when run by build.py): HR16_DIR the folder of WAVs, HR16_MAP a JSON file that picks the files by hand,
HR16_BUDGET the most bytes of ADPCM data to embed (default 98304), HR16_RATE the stored sample rate (default 22050),
HR16_MAX_KITS (default 6).

Each lane of the DRUM engine (KICK SNARE CLAP HATCL HATOP TOM RIM BELL) takes one file per kit. Files are sorted into lanes
by the words in their names (see CLASSES); --list shows the result, a JSON map overrides it:

  {"kits": [{"name": "A", "KICK": "BD_01.wav", "SNARE": "sub/SD_03.wav", ... all 8 lanes ...}, ...]}

Kit k takes the k-th file of each lane (wrapping), so six kick files and three snares make six kits with the snares
repeating. A file used by several kits is stored once. Samples are mixed to mono, trimmed, normalised per lane, resampled
and stored as IMA ADPCM (4 bit) exactly as firmware/src/eng_sample.c decodes it, one-shot, from predictor 0 and index 0.

The WAV files are not part of this repository and are never committed: check the licence of the pack you use before you
share a firmware image that contains it.
"""
import argparse
import json
import math
import os
import re
import struct
import sys
from pathlib import Path

LANES = ["KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"]   # eng_drum.c DRUM_LANE_NOTE order
MAX_SEC = {"KICK": 0.60, "SNARE": 0.50, "CLAP": 0.50, "HATCL": 0.25, "HATOP": 0.80, "TOM": 0.60, "RIM": 0.20, "BELL": 1.20}
PEAK = {"KICK": 22000, "SNARE": 22000, "CLAP": 18000, "HATCL": 14000, "HATOP": 14000, "TOM": 20000, "RIM": 16000,
        "BELL": 14000}                     # per-lane peak of the stored sample (Q15 full scale 32767; the engine adds x1.25)
SILENCE = 1.0 / 1500                       # leading / trailing silence: below this share of the peak (about -63 dB)
FADE_SEC = 0.008

IMA_STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88,
            97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
            724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
            4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
            18500, 20350, 22385, 24623, 27086, 29794, 32767]   # = eng_sample.c IMA_STEP
IMA_IDX = [-1, -1, -1, -1, 2, 4, 6, 8]                             # = eng_sample.c IMA_IDX


# ------------------------------------------------------------------ WAV in, out

def read_wav(path):
    """-> (mono floats -1..1, rate). PCM 8/16/24/32 bit, float 32/64, WAVE_FORMAT_EXTENSIBLE; any channel count."""
    d = Path(path).read_bytes()
    if len(d) < 12 or d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        raise ValueError("not a WAV file")
    i, fmt, data = 12, None, None
    while i + 8 <= len(d):
        cid, n = d[i:i + 4], struct.unpack_from("<I", d, i + 4)[0]
        body = d[i + 8:i + 8 + n]
        if cid == b"fmt " and fmt is None:
            fmt = body
        elif cid == b"data" and data is None:
            data = body
        i += 8 + n + (n & 1)
    if fmt is None or data is None or len(fmt) < 16:
        raise ValueError("WAV without fmt or data")
    tag, nch, rate, _, _, bits = struct.unpack_from("<HHIIHH", fmt, 0)
    if tag == 0xFFFE and len(fmt) >= 26:
        tag = struct.unpack_from("<H", fmt, 24)[0]
    if nch < 1 or rate < 1000 or rate > 400000:
        raise ValueError(f"unusable WAV format ({nch} ch, {rate} Hz)")
    width = bits // 8
    if tag == 1 and bits in (8, 16, 24, 32):
        pass
    elif tag == 3 and bits in (32, 64):
        pass
    else:
        raise ValueError(f"unsupported WAV encoding (tag {tag}, {bits} bit)")
    frames = len(data) // (width * nch)
    out = []
    for f in range(frames):
        acc = 0.0
        for c in range(nch):
            o = (f * nch + c) * width
            if tag == 3:
                v = struct.unpack_from("<f" if bits == 32 else "<d", data, o)[0]
            elif bits == 8:
                v = (data[o] - 128) / 128.0
            elif bits == 16:
                v = struct.unpack_from("<h", data, o)[0] / 32768.0
            elif bits == 24:
                v = int.from_bytes(data[o:o + 3], "little", signed=True) / 8388608.0
            else:
                v = struct.unpack_from("<i", data, o)[0] / 2147483648.0
            acc += v
        out.append(acc / nch)
    return out, rate


def write_wav(path, samples, rate):
    pcm = b"".join(struct.pack("<h", max(-32768, min(32767, int(round(s))))) for s in samples)
    Path(path).write_bytes(b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " +
                           struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" +
                           struct.pack("<I", len(pcm)) + pcm)


# ---------------------------------------------------------------- processing

def resample(x, src, dst):
    """windowed-sinc resampling (low-passed when going down)"""
    if src == dst or not x:
        return list(x)
    ratio = src / dst
    cut = min(1.0, dst / src)                      # cutoff relative to the source Nyquist
    half = 16
    n = max(1, int(len(x) / ratio))
    out = []
    for j in range(n):
        pos = j * ratio
        i0 = int(math.floor(pos))
        acc = 0.0
        for k in range(i0 - half + 1, i0 + half + 1):
            if 0 <= k < len(x):
                t = pos - k
                w = 0.5 + 0.5 * math.cos(math.pi * t / half) if abs(t) < half else 0.0
                s = 1.0 if t == 0 else math.sin(math.pi * t * cut) / (math.pi * t * cut)
                acc += x[k] * s * w * cut
        out.append(acc)
    return out


def prepare(x, rate, lane, dst_rate):
    """mono floats at `rate` -> ints (Q15 range) at dst_rate: DC removed, trimmed, faded, resampled, normalised"""
    if not x:
        raise ValueError("empty sample")
    mean = sum(x) / len(x)
    x = [v - mean for v in x]
    peak = max(abs(v) for v in x)
    if peak < 1e-6:
        raise ValueError("silent sample")
    thr = peak * SILENCE
    a = next(i for i, v in enumerate(x) if abs(v) > thr)
    b = len(x) - next(i for i, v in enumerate(reversed(x)) if abs(v) > thr)
    x = x[a:b]
    x = x[:max(1, int(MAX_SEC[lane] * rate))]
    x = resample(x, rate, dst_rate)
    fade = min(len(x), max(1, int(FADE_SEC * dst_rate)))
    for k in range(fade):                          # the cut or the sample's own end fades out
        x[len(x) - 1 - k] *= k / fade
    peak = max(abs(v) for v in x) or 1.0
    g = PEAK[lane] / peak
    return [int(round(v * g)) for v in x]


def ima_encode(pcm):
    """-> (bytes, decoded ints). The reconstruction is eng_sample.c's decode, step for step: a low nibble first."""
    pred, idx, nib, dec = 0, 0, [], []
    for x in pcm:
        step = IMA_STEP[idx]
        diff, code = x - pred, 0
        if diff < 0:
            code, diff = 8, -diff
        if diff >= step:
            code |= 4
            diff -= step
        if diff >= step >> 1:
            code |= 2
            diff -= step >> 1
        if diff >= step >> 2:
            code |= 1
        vd = step >> 3
        if code & 4:
            vd += step
        if code & 2:
            vd += step >> 1
        if code & 1:
            vd += step >> 2
        pred = max(-32768, min(32767, pred + (-vd if code & 8 else vd)))
        idx = max(0, min(88, idx + IMA_IDX[code & 7]))
        nib.append(code)
        dec.append(pred)
    if len(nib) & 1:
        nib.append(0)
    return bytes(nib[i] | nib[i + 1] << 4 for i in range(0, len(nib), 2)), dec


def ima_decode(data, n):
    """the firmware's decode (hr_next in eng_drum.c) of n samples"""
    pred, idx, out = 0, 0, []
    for pos in range(n):
        b = data[pos >> 1]
        code = b >> 4 if pos & 1 else b & 15
        step = IMA_STEP[idx]
        vd = step >> 3
        if code & 4:
            vd += step
        if code & 2:
            vd += step >> 1
        if code & 1:
            vd += step >> 2
        pred = max(-32768, min(32767, pred + (-vd if code & 8 else vd)))
        idx = max(0, min(88, idx + IMA_IDX[code & 7]))
        out.append(pred)
    return out


# ------------------------------------------------------------ classification

# lane -> words in a file name (lower case, split at non-letters, digits and camelCase), most wanted first
CLASSES = [
    ("HATOP", ["open", "opn", "ohh", "oh", "ohat"]),
    ("HATCL", ["closed", "close", "clsd", "chh", "ch", "chat", "hat", "hihat", "hh", "hats", "pedal"]),
    ("KICK", ["kick", "kik", "kk", "bd", "bassdrum"]),
    ("SNARE", ["snare", "snr", "sn", "sd"]),
    ("CLAP", ["clap", "claps", "handclap", "cp"]),
    ("TOM", ["tom", "toms", "conga", "bongo", "timbale"]),
    ("RIM", ["rim", "rimshot", "sidestick", "stick", "clave", "claves", "woodblock", "rs"]),
    ("BELL", ["cowbell", "bell", "crash", "ride", "cymbal", "tambourine", "tamb", "shaker", "cabasa", "maracas",
              "triangle", "agogo", "splash"]),
]


def tokens(stem):
    s = re.sub(r"([a-z])([A-Z])", r"\1 \2", stem)
    return [t for t in re.split(r"[^a-z]+", s.lower()) if t]


def classify(stem):
    """-> (lane, rank) or None. rank orders the candidates of a lane (lower first)"""
    tk = tokens(stem)
    joined = "".join(tk)
    words = set(tk)
    is_hat = bool(words & {"hat", "hats", "hihat", "hh", "chh", "ohh", "chat", "ohat"}) or "hihat" in joined or "hat" in joined
    if is_hat:
        if words & {"open", "opn", "ohh", "oh", "ohat"} or "open" in joined:
            return "HATOP", 0
        return "HATCL", 0
    for lane, names in CLASSES:
        if lane in ("HATOP", "HATCL"):
            continue
        for rank, w in enumerate(names):
            if w in words or (len(w) > 4 and w in joined):
                return lane, rank
    return None


def scan(folder):
    files = sorted(p for p in Path(folder).rglob("*") if p.suffix.lower() == ".wav" and p.is_file())
    by_lane = {l: [] for l in LANES}
    unknown = []
    for p in files:
        c = classify(p.stem)
        if c:
            by_lane[c[0]].append((c[1], str(p.relative_to(folder)), p))
        else:
            unknown.append(str(p.relative_to(folder)))
    for l in LANES:
        by_lane[l].sort(key=lambda e: (e[0], e[1].lower()))
    return by_lane, unknown, files


def plan_kits(folder, by_lane, map_file, max_kits):
    """-> [(kit name, {lane: Path})]"""
    if map_file:
        m = json.loads(Path(map_file).read_text())
        kits = []
        for k in m["kits"]:
            lanes = {}
            for l in LANES:
                if l not in k:
                    raise SystemExit(f"gen_hr16: map kit {k.get('name')!r} has no {l}")
                p = Path(folder) / k[l]
                if not p.is_file():
                    raise SystemExit(f"gen_hr16: map: {p} not found")
                lanes[l] = p
            kits.append((str(k.get("name", f"{len(kits) + 1}")), lanes))
        return kits
    missing = [l for l in LANES if not by_lane[l]]
    if missing:
        raise SystemExit("gen_hr16: no WAV file found for lane(s) " + ", ".join(missing) +
                         f" in {folder} (see --list; a JSON map picks files by hand)")
    nk = min(max_kits, max(len(by_lane[l]) for l in LANES))
    return [(chr(ord("A") + k), {l: by_lane[l][k % len(by_lane[l])][2] for l in LANES}) for k in range(nk)]


# ------------------------------------------------------------------- build

def build(folder, map_file, rate, budget, max_kits):
    by_lane, unknown, _ = scan(folder)
    kits = plan_kits(folder, by_lane, map_file, max_kits)
    while True:
        samples, order, kit_idx = [], {}, []
        for name, lanes in kits:
            row = []
            for l in LANES:
                key = (l, lanes[l])
                if key not in order:
                    try:
                        x, sr = read_wav(lanes[l])
                        pcm = prepare(x, sr, l, rate)
                    except ValueError as e:
                        raise SystemExit(f"gen_hr16: {lanes[l]}: {e}")
                    data, dec = ima_encode(pcm)
                    order[key] = len(samples)
                    samples.append({"lane": l, "path": lanes[l], "n": len(pcm), "data": data, "dec": dec})
                row.append(order[key])
            kit_idx.append(row)
        total = sum(len(s["data"]) for s in samples)
        if total <= budget or len(kits) == 1:
            break
        print(f"gen_hr16: {total} bytes over the {budget} byte budget with {len(kits)} kits: dropping kit "
              f"{kits[-1][0]}", file=sys.stderr)
        kits = kits[:-1]
    if total > budget:
        raise SystemExit(f"gen_hr16: one kit is {total} bytes, over the {budget} byte budget "
                         f"(HR16_RATE lower, e.g. 16000, or HR16_BUDGET higher)")
    return kits, samples, kit_idx, total, unknown


def header(kits, samples, kit_idx, rate, folder):
    off, offs = 0, []
    for s in samples:
        offs.append(off)
        off += len(s["data"])
    blob = b"".join(s["data"] for s in samples)
    q16 = int(round(rate / 44100.0 * 65536))
    L = ["/* Generated by tools/gen_hr16.py: DRUM's HR-16 sample kits. Do not edit; do not commit. */",
         f"/* {len(kits)} kits, {len(samples)} samples, {len(blob)} bytes of IMA ADPCM at {rate} Hz */",
         f"#define HR16_NKITS {len(kits)}",
         f"#define HR16_NSAMP {len(samples)}"]
    L.append("#define HR16_SAMP_INIT \\")
    for i, s in enumerate(samples):
        L.append(f"    {{{offs[i]}u, {s['n']}u, {q16}u}}{',' if i + 1 < len(samples) else ''} /* {s['lane']} {s['path'].name} */ \\")
    L.append("")
    L.append("static const uint8_t HR16_KIT[HR16_NKITS][8] = {")
    for (name, _), row in zip(kits, kit_idx):
        L.append("    {" + ", ".join(str(v) for v in row) + "},   /* " + name + ": " + " ".join(LANES) + " */")
    L.append("};")
    names = [f"HR-{n}"[:5] for n, _ in kits]
    L.append("#define HR16_FOR_EACH_KIT(X) \\")      # X(index, KIT name (5 characters), preset name (8))
    for k, (n, _) in enumerate(kits):
        L.append(f'    X({k}, "HR-{n}", "HR16 {n}"){" \\" if k + 1 < len(kits) else ""}')
    L.append("")
    L.append(f"static const uint8_t HR16_DATA[{max(1, len(blob))}] = {{")
    for i in range(0, len(blob), 24):
        L.append("    " + ", ".join(str(b) for b in blob[i:i + 24]) + ",")
    L.append("};")
    return "\n".join(L) + "\n", names


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?", help="the header to write (or, with --list, the folder)")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--preview", metavar="DIR")
    a = ap.parse_args()
    env = os.environ.get
    folder = env("HR16_DIR", "assets/hr16")
    rate = int(env("HR16_RATE", "22050"))
    budget = int(env("HR16_BUDGET", "98304"))
    max_kits = int(env("HR16_MAX_KITS", "6"))
    if a.list:
        folder = a.out or folder
        by_lane, unknown, files = scan(folder)
        print(f"{len(files)} WAV files in {folder}")
        for l in LANES:
            print(f"  {l:6} " + (", ".join(e[1] for e in by_lane[l]) or "-- none --"))
        if unknown:
            print("  not sorted into a lane: " + ", ".join(unknown))
        return 0
    if not a.out:
        ap.error("OUT.h")
    if not Path(folder).is_dir():
        raise SystemExit(f"gen_hr16: {folder}: no such folder (put the HR-16 WAV files there, or set HR16_DIR)")
    kits, samples, kit_idx, total, unknown = build(folder, env("HR16_MAP"), rate, budget, max_kits)
    text, names = header(kits, samples, kit_idx, rate, folder)
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    Path(a.out).write_text(text)
    print(f"gen_hr16: {len(kits)} kits ({', '.join(names)}), {len(samples)} samples, {total} of {budget} bytes, {rate} Hz")
    for name, row in zip((k[0] for k in kits), kit_idx):
        print(f"  kit {name}: " + "  ".join(f"{l}={samples[i]['path'].name}" for l, i in zip(LANES, row)))
    if unknown:
        print("  not used (no lane in the name): " + ", ".join(unknown))
    if a.preview:
        Path(a.preview).mkdir(parents=True, exist_ok=True)
        for i, s in enumerate(samples):
            write_wav(Path(a.preview) / f"{i:02d}_{s['lane']}_{s['path'].stem}.wav", s["dec"], rate)
    return 0


if __name__ == "__main__":
    sys.exit(main())
