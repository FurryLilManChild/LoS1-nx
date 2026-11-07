#!/usr/bin/env bash
# Build the local Xbox 360 executable as native Apple Silicon code.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$REPO/out/mac"
JOBS="${LOS_BUILD_JOBS:-4}"
RENUT="${LOS_RENUT_RENDERER:-ON}"
XEX="$REPO/assets/game_root/default.xex"
EXPECTED=0287bbd803d36d3da59750df4de69b2e040b4f8604d85ddd528d9027a6dbb46b

[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || {
    echo 'This build script requires an Apple Silicon Mac.' >&2; exit 1;
}
for tool in cmake ninja python3 xcrun; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
[[ -f "$XEX" ]] || {
    echo "Extract Disc 1 with tools/extract_iso.py into $REPO/assets/game_root first." >&2; exit 1;
}
ACTUAL="$(shasum -a 256 "$XEX" | cut -d ' ' -f 1)"
[[ "$ACTUAL" == "$EXPECTED" ]] || {
    echo "Unsupported default.xex SHA-256: $ACTUAL" >&2; exit 1;
}

if [[ ! -f "$REPO/sdk/thirdparty/sdl3/CMakeLists.txt" ]]; then
    python3 "$REPO/tools/fetch_thirdparty.py"
fi
PYTHON_PATH="$(command -v python3)"
if [[ "$RENUT" == ON ]]; then
    if [[ ! -f "$REPO/app/thirdparty/plume/CMakeLists.txt" ||
          ! -f "$REPO/app/thirdparty/reblue-XenosRecomp/thirdparty/dxc-bin/bin/arm64/dxc-macos" ]]; then
        python3 "$REPO/tools/fetch_renut_deps.py"
    fi
    if [[ ! -x "$REPO/out/macos-tools-venv/bin/python" ]]; then
        python3 -m venv "$REPO/out/macos-tools-venv"
    fi
    PYTHON_PATH="$REPO/out/macos-tools-venv/bin/python"
    if ! "$PYTHON_PATH" -c 'import xxhash' 2>/dev/null; then
        "$PYTHON_PATH" -m pip install xxhash
    fi
elif [[ "$RENUT" != OFF ]]; then
    echo 'LOS_RENUT_RENDERER must be ON or OFF.' >&2; exit 1
fi
mkdir -p "$REPO/out/logs"
# Apple's shims honor xcode-select and keep stable compiler paths in CMake.
CC_PATH=/usr/bin/clang
CXX_PATH=/usr/bin/clang++
CONFIG=(
    -S "$REPO/app" -B "$BUILD" -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    "-DREXSDK_DIR=$REPO/sdk"
    "-DCMAKE_C_COMPILER=$CC_PATH" "-DCMAKE_CXX_COMPILER=$CXX_PATH"
    "-DCMAKE_OBJC_COMPILER=$CC_PATH" "-DCMAKE_OBJCXX_COMPILER=$CXX_PATH"
    -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
    -DREXGLUE_ENABLE_TRACY=OFF
    "-DLOS_RENUT_RENDERER=$RENUT"
    "-DPython3_EXECUTABLE=$PYTHON_PATH"
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
)
cmake "${CONFIG[@]}" 2>&1 | tee "$REPO/out/logs/mac-configure.log"
cmake --build "$BUILD" --target los_codegen -j "$JOBS" 2>&1 | tee "$REPO/out/logs/mac-codegen.log"
# The first configuration has no generated source list. Load the list now.
cmake "${CONFIG[@]}" 2>&1 | tee "$REPO/out/logs/mac-configure2.log"
cmake --build "$BUILD" --target los -j "$JOBS" 2>&1 | tee "$REPO/out/logs/mac-build.log"
echo "Built: $BUILD/los"
echo 'Close the game, then run tools/macos/package.sh to update the Mac application.'
