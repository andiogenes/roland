#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Type-check the WHOLE firmware (firmware/src/felucca.c, every file that ships) on the host, and compare its size with every
# engine compiled in against the drum-only build. Run by host_render.sh from the patched Felucca tree (build/gen made).
#   host_size.sh FLAGS_COMMON     e.g. "-DFELUCCA_SLICE=0 -DFELUCCA_ADPCM=0 -DFELUCCA_TR606=1 ..."
# The sizes are x86-64 -Os code and read-only data (the FM-1's CPU is denser): good for the difference, not the total.
set -e
COMMON="$1"
HALI="-Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 -Ifirmware/src"
for mode in 0 1; do
    cc -fsyntax-only -Wall -Wno-unused-function -Wno-unused-variable -Werror=implicit-function-declaration -Wno-int-to-pointer-cast \
        -Wno-builtin-declaration-mismatch $HALI $COMMON -DFELUCCA_DRUMONLY=$mode -Ibuild/gen -Ifirmware/src firmware/src/felucca.c
    echo "felucca.c type-checks with FELUCCA_DRUMONLY=$mode"
done
sz() {   # -> "text data bss" of felucca.c at -Os (the FM-1's inline assembly, fenced by #APP, is dropped: it is not x86)
    cc -S -Os -ffunction-sections -fdata-sections -fno-builtin -w $HALI $COMMON -DFELUCCA_DRUMONLY=$1 -Ibuild/gen -Ifirmware/src \
        firmware/src/felucca.c -o build/size_$1.s
    sed -i '/#APP/,/#NO_APP/d' build/size_$1.s
    as -o build/size_$1.o build/size_$1.s
    size build/size_$1.o | awk 'NR==2 {print $1, $2, $3}'
}
a=$(sz 0); b=$(sz 1)
set -- $a $b
echo "code + read-only data, bytes:   all engines $1   drum only $4   saved $(($1 - $4)) ($((($1 - $4) * 100 / $1)) %)"
echo "RAM (data + bss), bytes:        all engines $(($2 + $3))   drum only $(($5 + $6))"
