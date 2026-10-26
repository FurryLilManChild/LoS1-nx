#!/usr/bin/env bash
# Builds the ReXGlue code generator on Ubuntu 24.04 (x86-64) and runs it on this project.
# This is how the 2026-10-23 feasibility scan was done. It needs your own default.xex in assets/game_root/.
#
# Usage: tools/setup_linux_codegen.sh [--dump-image]
#   --dump-image  also applies patches/rexglue-dump-image.patch and writes the decrypted image to out/image.bin
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${REXGLUE_SDK:-$REPO/out/rexglue-sdk}"

sudo apt-get install -y clang-20 lld-20 cmake ninja-build python3 \
  libx11-xcb-dev libxcb1-dev libxcb-xkb-dev libxkbcommon-x11-dev libxkbcommon-dev \
  libxcursor-dev libxi-dev libxrandr-dev libxss-dev libxtst-dev libgtk-3-dev libasound2-dev libpulse-dev

if [ ! -d "$SDK" ]; then
  git clone --recurse-submodules --shallow-submodules --depth 1 https://github.com/rexglue/rexglue-sdk.git "$SDK"
fi

if [ "${1:-}" = "--dump-image" ]; then
  git -C "$SDK" apply --check "$REPO/patches/rexglue-dump-image.patch" 2>/dev/null \
    && git -C "$SDK" apply "$REPO/patches/rexglue-dump-image.patch"
fi

# clang 20 is needed: clang 18 with this libstdc++ lacks std::expected. -march=x86-64-v2 is needed for SSSE3.
cmake -S "$SDK" -B "$SDK/out/host" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 \
  -DREXGLUE_ENABLE_TRACY=OFF -DSDL_WAYLAND=OFF
ninja -C "$SDK/out/host" rexglue

REXGLUE="$SDK/out/linux-amd64/rexglue"
cd "$REPO/app"
mkdir -p "$REPO/out"
if [ "${1:-}" = "--dump-image" ]; then
  LOS_DUMP_IMAGE="$REPO/out/image.bin" "$REXGLUE" codegen los_manifest.toml | tee "$REPO/out/codegen.log"
else
  "$REXGLUE" codegen los_manifest.toml | tee "$REPO/out/codegen.log"
fi
