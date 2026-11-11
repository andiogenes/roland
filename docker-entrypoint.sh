#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The image's entry point: build_drum.sh with the arguments given (--release X.Y[-suffix]), in /src (the mounted repository).
set -e
cd /src
[ -f build_drum.sh ] || { echo "docker: mount the repository at /src (-v \"\$PWD:/src\")"; exit 1; }
ulimit -n 8192 2>/dev/null || true
exec sh build_drum.sh "$@"
