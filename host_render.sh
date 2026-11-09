#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the drum machine firmware's sources for the HOST (gcc, no FM-1, no JieLi toolchain) and capture what they do:
#   - every DRUM kit rendered through the real engine, voices, mixer and FX to WAV (tests/render_kits.c)
#   - the drum grid and EDIT pages drawn by the real UI code (tests/render_screens.c)
#   - Felucca's own UI lint over all 233 screens x 10 palettes (tests/ui_render.c)
#   ./host_render.sh            -> out/host/{kits,screens,docs}
# Needs gcc, python3 (a venv is made for Pillow, fonttools, numpy: pypi.org), git and, for the mp3, ffmpeg.
# Environment: FELUCCA_SRC (where to clone Felucca from), HR16_DIR, TR606_GAIN_DB (default -3), FELUCCA_TR606 / FELUCCA_ACOUSTIC.
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"
FELUCCA_REV="a4f007c9211ea5b37298c01ff6d73430fe2cb96a"
SRC="${FELUCCA_SRC:-https://github.com/hugelton/Felucca.git}"
WORK="$ROOT/build/felucca-host"
OUT="$ROOT/out/host"
HR16_DIR="${HR16_DIR:-$ROOT/assets/hr16}"
mkdir -p build "$OUT/kits" "$OUT/screens/ppm"

[ -d "$WORK/.git" ] || git clone -q "$SRC" "$WORK"
git -C "$WORK" fetch -q origin "$FELUCCA_REV" 2>/dev/null || true
git -C "$WORK" checkout -q -f "$FELUCCA_REV"
git -C "$WORK" clean -fdxq -e build -e firmware/bsp -e firmware/engines
git -C "$WORK" submodule update --init -q
git -C "$WORK" apply "$ROOT/patches/felucca-drum.patch"
cp "$ROOT"/overlay/tools/*.py "$WORK/tools/"
cp "$ROOT"/overlay/firmware/src/*.c "$WORK/firmware/src/"
rm -rf "$WORK/assets/samples-cc0"

[ -x build/venv/bin/python ] || { python3 -m venv build/venv && build/venv/bin/pip install -q Pillow fonttools numpy; }
PY="$ROOT/build/venv/bin/python"

cd "$WORK"
G=build/gen; mkdir -p $G build/host
for g in "gen_aa_font.py $G/ui_fonts.h --preset inter-tight" "gen_aa_icons.py $G/ui_icons.h" "gen_aa_keycaps.py $G/ui_keycaps.h" \
         "gen_ui_palettes.py $G/ui_palettes.h" "gen_tables.py $G/felucca_tables.h" "gen_fm6_patches.py $G/felucca_fm6.h" \
         "gen_samples.py $G/felucca_samples.h" "gen_adpcm.py $G/felucca_adpcm.h"; do
    $PY tools/$g >/dev/null
done
FLAGS="-DFELUCCA_DRUMONLY=1 -DFELUCCA_SLICE=0 -DFELUCCA_ADPCM=0"
if [ "${FELUCCA_TR606:-1}" = 1 ]; then
    TR606_GAIN_DB="${TR606_GAIN_DB:--3}" $PY tools/gen_tr606.py $G/felucca_tr606.h; FLAGS="$FLAGS -DFELUCCA_TR606=1"
fi
HR=""; [ -d "$HR16_DIR" ] && [ -n "$(find "$HR16_DIR" -iname '*.wav' | head -n 1)" ] && HR="$HR16_DIR"
if [ "${FELUCCA_ACOUSTIC:-1}" = 1 ] || [ -n "$HR" ]; then
    ACOUSTIC_DIR="$([ "${FELUCCA_ACOUSTIC:-1}" = 1 ] && echo "$ROOT/assets/acoustic")" HR16_DIR="$HR" $PY tools/gen_hr16.py $G/felucca_hr16.h
    [ "${FELUCCA_ACOUSTIC:-1}" = 1 ] && FLAGS="$FLAGS -DFELUCCA_ACOUSTIC=1"
    [ -n "$HR" ] && FLAGS="$FLAGS -DFELUCCA_HR16=1"
fi
echo "host_render: flags $FLAGS"

HALI="-Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 -Ifirmware/src"
FV=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -n 1)
cp "$ROOT/tests/render_kits.c" "$ROOT/tests/render_screens.c" tests/
# ui_render.c's main renamed (ui_test.c, which it includes, undefines any #define main)
sed 's/^int main(int argc, char \*\*argv)$/int ui_render_main(int argc, char **argv)/' tests/ui_render.c > tests/ui_render_lib.c
cc -O1 -Wall -Wno-unused-function -Wno-int-to-pointer-cast $HALI $FLAGS -Ibuild/gen -Ifirmware/src -Itests -o build/host/render_kits tests/render_kits.c -lm
cc -O1 -w $HALI $FLAGS "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o build/host/render_screens tests/render_screens.c -lm
cc -O1 -w $HALI $FLAGS "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o build/host/ui_render tests/ui_render.c -lm
echo "host_render: built (the patched firmware sources compile on the host)"

build/host/render_kits "$OUT/kits" | tee "$OUT/kits/levels.txt"
build/host/render_screens "$OUT/screens"
mkdir -p build/ui_new/ppm build/ui_slot
build/host/ui_render build/ui_new build/ui_slot 2>&1 | tail -4 | tee "$OUT/ui_lint.txt"
$PY "$ROOT/tests/host_sheets.py" "$OUT/screens" "$OUT/kits" "$OUT/docs"
if command -v ffmpeg >/dev/null; then
    ffmpeg -loglevel error -y -i "$OUT/kits/all_kits.wav" -b:a 192k "$OUT/kits/all_kits.mp3"
    mkdir -p "$OUT/mp3"
    for w in "$OUT"/kits/kit_*.wav; do
        ffmpeg -loglevel error -y -i "$w" -ac 1 -t 4.3 -b:a 96k "$OUT/mp3/$(basename "$w" .wav).mp3"
    done
fi
# PUBLISH=1: the pictures, and the clips of every kit except the HR-16 ones (that pack's licence is not known), into docs/
if [ "${PUBLISH:-0}" = 1 ]; then
    mkdir -p "$ROOT/docs/host-render" "$ROOT/docs/audio"
    cp "$OUT"/docs/*.png "$ROOT/docs/host-render/"
    cp "$OUT/kits/levels.txt" "$ROOT/docs/host-render/kit_levels.txt"
    cp "$OUT/ui_lint.txt" "$ROOT/docs/host-render/ui_lint.txt"
    rm -f "$ROOT"/docs/audio/*.mp3
    for m in "$OUT"/mp3/kit_*.mp3; do
        case "$m" in *HR-*) ;; *) cp "$m" "$ROOT/docs/audio/";; esac
    done
fi
echo "host_render: done -> $OUT"
