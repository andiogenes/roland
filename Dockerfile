# SPDX-License-Identifier: GPL-3.0-only
# Builds the drum machine firmware for the M-VAVE FM-1 inside Docker, for an ARM64 Linux machine (a UTM virtual machine on Apple silicon).
#
# JieLi's compiler (clang 4.0.1 for pi32v2) only exists for Linux x86-64, so the image is linux/amd64: on ARM64 it runs under QEMU user-mode
# emulation (slow, a few minutes; works). Inside the image the build is an ordinary x86-64 Linux build, so Felucca's own build script
# needs no nested Docker. The image holds the toolchain, the AC79 SDK and the Python packages; the repository is mounted when you run it.
#
#   one time, in the ARM64 VM (Debian/Ubuntu):
#       sudo apt-get install -y docker.io docker-buildx qemu-user-static binfmt-support     # (or Docker's own packages)
#       sudo docker run --privileged --rm tonistiigi/binfmt --install amd64                # registers the x86-64 emulator
#   build the image (needs the network: JieLi's server and gitee):
#       sudo docker build --platform linux/amd64 -t roland-fm1 .
#   build the firmware (needs the network: it clones Felucca from GitHub; the result lands in ./dist):
#       sudo docker run --rm --platform linux/amd64 -v "$PWD:/src" roland-fm1 --release 0.1-roland       # dist/felucca-0.1-roland.fwsc
#       sudo docker run --rm --platform linux/amd64 -v "$PWD:/src" roland-fm1                            # out/roland-drum.fwsc
#   HR-16B / Drumulator kits: put the WAVs in assets/hr16 and assets/drumulator (they are git-ignored); the mount carries them in.
#   the host-side tests (no toolchain needed): sudo docker run --rm --platform linux/amd64 --entrypoint ./host_render.sh -v "$PWD:/src" roland-fm1
FROM --platform=linux/amd64 python:3.13-slim-bookworm

ARG SDK_TAG=AC79NN_SDK_V1.2.1_2023-12-13
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl git xz-utils gcc libc6-dev patch ffmpeg \
 && rm -rf /var/lib/apt/lists/*

# (Python 3.13 from the base image) the Python packages the UI font and icons are rasterised with at build time (and the host render's pictures)
RUN pip install --no-cache-dir Pillow fonttools numpy

# JieLi's toolchain (clang for pi32v2) and the SDK (the build uses three files of it)
RUN mkdir -p /opt/jieli-dl && cd /opt/jieli-dl \
 && curl -fL --retry 3 -o toolchain.tar.xz https://pkgman.jieliapp.com/s/linux-toolchain \
 && tar -xJf toolchain.tar.xz && rm toolchain.tar.xz \
 && ln -sfn "$(ls -d /opt/jieli-dl/jieli-linux-toolchains-* | sort | tail -1)" /opt/jieli-dl/toolchain \
 && git clone --depth 1 --branch "${SDK_TAG}" https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git /opt/fw-AC79_AIoT_SDK \
 && rm -rf /opt/fw-AC79_AIoT_SDK/.git
ENV JIELI_TOOLCHAIN=/opt/jieli-dl/toolchain AC79_SDK=/opt/fw-AC79_AIoT_SDK

# the repository is /src (a mounted folder, owned by someone else: git must trust it)
RUN git config --global --add safe.directory '*'
WORKDIR /src
COPY docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN chmod +x /usr/local/bin/docker-entrypoint.sh
ENTRYPOINT ["docker-entrypoint.sh"]
