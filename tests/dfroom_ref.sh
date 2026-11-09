#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Builds the reference of the Dragonfly Room (tests/dfroom_ref.cpp) against a clone of michaelwillis/dragonfly-reverb and writes its
# impulse responses at 44.1 and 22.05 kHz, with the facts docs/FX-PLAN.md quotes.
#   DRAGONFLY_DIR=/path/to/dragonfly-reverb tests/dfroom_ref.sh OUTDIR
set -e
cd "$(dirname "$0")/.."
D="${DRAGONFLY_DIR:-/home/user/michaelwillis/dragonfly-reverb}/common"
OUT="${1:-build/dfroom}"
mkdir -p "$OUT/stub"
cat > "$OUT/stub/fftw3.h" <<'EOT'
/* the types Freeverb3's headers name; the model run here never calls an FFT */
#pragma once
typedef float fftwf_complex[2]; typedef double fftw_complex[2]; typedef void *fftwf_plan; typedef void *fftw_plan;
typedef long double fftwl_complex[2]; typedef void *fftwl_plan;
EOT
g++ -O1 -w -std=gnu++11 -DLIBFV3_FLOAT -I "$OUT/stub" -I "$D" -include "$D/fv3_config.h" tests/dfroom_ref.cpp \
    "$D"/freeverb/earlyref.cpp "$D"/freeverb/progenitor2.cpp "$D"/freeverb/progenitor.cpp "$D"/freeverb/revbase.cpp "$D"/freeverb/biquad.cpp \
    "$D"/freeverb/delay.cpp "$D"/freeverb/delayline.cpp "$D"/freeverb/allpass.cpp "$D"/freeverb/comb.cpp "$D"/freeverb/efilter.cpp \
    "$D"/freeverb/slot.cpp "$D"/freeverb/utils.cpp -o "$OUT/dfroom_ref"
"$OUT/dfroom_ref" 44100 "$OUT/ir44.raw"
"$OUT/dfroom_ref" 22050 "$OUT/ir22.raw"
python3 - "$OUT" <<'EOP'
import sys
import numpy as np
out = sys.argv[1]
for name, fs in (("ir44", 44100), ("ir22", 22050)):
    a = np.fromfile(f"{out}/{name}.raw", dtype=np.float32).reshape(-1, 4)
    l = a[:, 2]
    t = np.arange(len(l)) / fs
    e = np.cumsum((l ** 2)[::-1])[::-1]
    e = 10 * np.log10(e / e[0] + 1e-12)
    i5, i35 = np.argmax(e < -5), np.argmax(e < -35)
    print(f"{name}: fs {fs}: early peak {abs(a[:, :2]).max():.3f}, late peak {abs(a[:, 2:]).max():.4f}, late EDC -5 dB at {t[i5]:.3f} s and -35 dB at "
          f"{t[i35]:.3f} s (RT60 about {2 * (t[i35] - t[i5]):.2f} s), first late sample at {1000 * np.argmax(abs(l) > 1e-4) / fs:.1f} ms")
EOP
