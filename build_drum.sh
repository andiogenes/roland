#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the drum machine firmware for the M-VAVE FM-1: Felucca (pinned) + patches/felucca-drum.patch + overlay/.
#   ./build_drum.sh
# Environment:
#   HR16_DIR        folder of Alesis HR-16B WAV files (default assets/hr16). Without WAVs there: no HR-16 kits.
#   FELUCCA_ACOUSTIC 1 (default): add SLOOP's acoustic kit (assets/acoustic, CC0) in its five treatments; 0: leave it out
#   HR16_MAP        JSON file that picks the files of each kit by hand (see overlay/tools/gen_hr16.py)
#   HR16_BUDGET     most bytes of sample data to embed, all sample kits together (default 147456)
#   JIELI_TOOLCHAIN, AC79_SDK   as Felucca's BUILDING.md (tools/get_toolchain.sh and the SDK clone there)
#   FELUCCA_TR606   1 (default): add SLOOP's TR-606 kit; 0: leave it out.  TR606_GAIN_DB: its level in dB (default -3)
#   KEEP_CC0=1      keep Felucca's CC0 piano / flute / sax in the image (dropped by default: a drum machine does not use
#                   them, and their flash is what the HR-16 kits take)
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"
FELUCCA_REV="a4f007c9211ea5b37298c01ff6d73430fe2cb96a"          # hugelton/Felucca, docs: keep README focused (1.8.1)
WORK="$ROOT/build/felucca"
HR16_DIR="${HR16_DIR:-$ROOT/assets/hr16}"

if [ ! -d "$WORK/.git" ]; then
    mkdir -p build
    git clone https://github.com/hugelton/Felucca.git "$WORK"
fi
git -C "$WORK" fetch -q origin "$FELUCCA_REV" 2>/dev/null || true
git -C "$WORK" checkout -q -f "$FELUCCA_REV"
git -C "$WORK" clean -fdxq -e build
git -C "$WORK" submodule update --init --recursive -q

git -C "$WORK" apply "$ROOT/patches/felucca-drum.patch"
cp "$ROOT"/overlay/tools/*.py "$WORK/tools/"
cp "$ROOT"/overlay/firmware/src/*.c "$WORK/firmware/src/"
[ "${KEEP_CC0:-0}" = 1 ] || rm -rf "$WORK/assets/samples-cc0"

export FELUCCA_DRUMONLY=1 FELUCCA_SLICE=0 FELUCCA_ADPCM=0
export FELUCCA_TR606="${FELUCCA_TR606:-1}"            # SLOOP's TR-606 kit (FELUCCA_TR606=0: leave it out)
export TR606_GAIN_DB="${TR606_GAIN_DB:--3}"           # its level against Felucca's kits: a guess, tune by ear
if [ -d "$HR16_DIR" ] && [ -n "$(find "$HR16_DIR" -iname '*.wav' | head -n 1)" ]; then
    export FELUCCA_HR16=1 HR16_DIR
    echo "build_drum: HR-16 samples from $HR16_DIR"
else
    export FELUCCA_HR16=0 HR16_DIR=""
    echo "build_drum: no WAV files in $HR16_DIR: no HR-16 kits"
fi
export FELUCCA_ACOUSTIC="${FELUCCA_ACOUSTIC:-1}"      # SLOOP's acoustic kit and its treatments (assets/acoustic, CC0)
if [ "$FELUCCA_ACOUSTIC" = 1 ]; then
    export ACOUSTIC_DIR="$ROOT/assets/acoustic"
else
    export ACOUSTIC_DIR=""
fi

(cd "$WORK" && ./build.sh)
mkdir -p "$ROOT/out"
cp "$WORK/build/felucca.fwsc" "$ROOT/out/roland-drum.fwsc"
echo "build_drum: out/roland-drum.fwsc"
echo "install: python3 $WORK/tools/fm1_install.py out/roland-drum.fwsc   (see BUILDING.md, Install, in Felucca)"
