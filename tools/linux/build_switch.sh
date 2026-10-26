#!/usr/bin/env bash
# LoS1-nx: the Switch NRO on Linux (a cloud session), with devkitPro's Windows devkitA64 under Wine.
#
# devkitPro's package server is not reachable from the cloud session, so its Windows install is copied from Mark's PC
# (C:\devkitPro: devkitA64, libnx, portlibs\switch, tools, cmake; about 480 MB as two tar files) and run under Wine.
# The GCC wrappers rewrite the dependency files' Z:/ paths for ninja. The host tools of the deko3d shader chain
# (SPIRV-Cross, uam) and Renut's translator are built natively.
#
#   bash tools/linux/build_switch.sh setup <folder with dkp_a64.tar and dkp_rest.tar>   (once a session)
#   bash tools/linux/build_switch.sh shaders     (the deko3d shader package and Switch shader cache; needs the Linux
#                                                PC-style build in out/linux_fork and out/renut_shader_dump)
#   bash tools/linux/build_switch.sh configure
#   bash tools/linux/build_switch.sh build       (out/nx/los.nro)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
W=/opt/devkitpro-win
L=/opt/devkitpro
export DEVKITPRO=$L WINEDEBUG=-all

case "${1:-}" in
setup)
  src="${2:?folder with dkp_a64.tar and dkp_rest.tar}"
  command -v wine >/dev/null || { apt-get update && apt-get install -y wine64 zstd; }
  mkdir -p $W && tar -xf "$src/dkp_a64.tar" -C $W && tar -xf "$src/dkp_rest.tar" -C $W
  rm -rf $L && mkdir -p $L/devkitA64/bin $L/tools/bin
  for d in libnx portlibs cmake; do ln -s $W/$d $L/$d; done
  for d in $W/devkitA64/*; do n=$(basename "$d"); [ "$n" = bin ] || ln -s "$d" $L/devkitA64/$n; done
  for f in $W/devkitA64/bin/*.exe $W/tools/bin/*.exe; do
    n=$(basename "$f" .exe); dir=$L/devkitA64/bin; [[ "$f" == $W/tools/* ]] && dir=$L/tools/bin
    printf '#!/bin/sh\nexport WINEDEBUG=-all\nexec /usr/lib/wine/wine64 %s "$@"\n' "$f" > $dir/$n; chmod +x $dir/$n
  done
  for n in aarch64-none-elf-gcc aarch64-none-elf-g++ aarch64-none-elf-c++; do
    cat > $L/devkitA64/bin/$n <<WRAP
#!/bin/bash
export WINEDEBUG=-all
mf=""; prev=""
for a in "\$@"; do [ "\$prev" = "-MF" ] && mf="\$a"; prev="\$a"; done
/usr/lib/wine/wine64 $W/devkitA64/bin/$n.exe "\$@"; rc=\$?
[ -n "\$mf" ] && [ -f "\$mf" ] && sed -i 's#Z:/#/#g' "\$mf"
exit \$rc
WRAP
    chmod +x $L/devkitA64/bin/$n
  done
  # A stand-in for the Mesa Horizon SDK (REXGLUE_SWITCH_NVK_SDK): with REXGLUE_SWITCH_NO_NVK the SDK only needs its
  # Vulkan headers, a lib/libvulkan.a to find (empty) and libzstd.a (devkitPro has no Switch zstd: built here).
  S="$ROOT/out/switch_sdk"; Z="$ROOT/app/thirdparty/reblue-XenosRecomp/thirdparty/zstd"
  mkdir -p "$S/include" "$S/lib" "$S/zstd-build"
  cp -r "$ROOT/sdk/thirdparty/vulkan-headers/include/"* "$S/include/"
  cp "$Z/lib/zstd.h" "$Z/lib/zstd_errors.h" "$Z/lib/zdict.h" "$S/include/"
  (cd "$S/zstd-build" && for f in "$Z"/lib/{common,compress,decompress,dictBuilder}/*.c; do
     $L/devkitA64/bin/aarch64-none-elf-gcc -O2 -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE \
       -ffunction-sections -fdata-sections -D__SWITCH__ -DZSTD_DISABLE_ASM -I "$Z/lib" -I "$Z/lib/common" \
       -c "$f" -o "$(basename "$f" .c).o"; done
   rm -f "$S/lib/libzstd.a"; $L/devkitA64/bin/aarch64-none-elf-ar rcs "$S/lib/libzstd.a" ./*.o
   $L/devkitA64/bin/aarch64-none-elf-ar rcs "$S/lib/libvulkan.a")
  # The deko3d chain's host tools.
  T="$ROOT/out/deko3d_tools"; mkdir -p "$T"
  [ -d "$T/spirv-cross-src" ] || git clone -q --depth 1 --branch vulkan-sdk-1.4.304.0 \
      https://github.com/KhronosGroup/SPIRV-Cross.git "$T/spirv-cross-src"
  cmake -S "$T/spirv-cross-src" -B "$T/spirv-cross-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 -DSPIRV_CROSS_ENABLE_TESTS=OFF \
      -DSPIRV_CROSS_SHARED=OFF -DSPIRV_CROSS_ENABLE_C_API=OFF >/dev/null
  ninja -C "$T/spirv-cross-build" spirv-cross
  cmake -S "$ROOT/app/thirdparty/uam" -B "$T/uam-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 >/dev/null
  ninja -C "$T/uam-build" uam_tool
  ;;
shaders)
  cd "$ROOT"
  mkdir -p out/deko3d_shaders/spirv
  BANJO_DEKO3D_SPIRV_DIR=out/deko3d_shaders/spirv BANJO_HLSL_OVERRIDE=out/linux_fork/renut_gpu/early_alpha_hlsl \
    out/linux_fork/xenos_recomp/build/XenosRecomp/XenosRecomp out/renut_shader_dump \
    out/deko3d_shaders/spirv_pass.cpp app/thirdparty/reblue-XenosRecomp/XenosRecomp/shader_common.h
  python3 tools/deko3d_shaders.py --spirv out/deko3d_shaders/spirv --out out/deko3d_shaders/glsl --no-uam
  python3 tools/deko3d_glsl_opt.py --in out/deko3d_shaders/glsl/glsl --out out/deko3d_shaders/glsl_opt --check
  python3 tools/deko3d_shader_package.py --glsl out/deko3d_shaders/glsl_opt --out app/out/renut_switch
  python3 tools/renut_switch_shader_cache.py --pc out/linux_fork
  ;;
configure)
  cd "$ROOT"
  cmake -S app -B out/nx -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/switch/cmake/switch-devkitA64.cmake" -DDEVKITPRO=$L -DREXSDK_DIR="$ROOT/sdk" \
    -DREXGLUE_SWITCH_NO_NVK=ON -DREXGLUE_SWITCH_NVK_SDK="$ROOT/out/switch_sdk" \
    -DBANJO_DEKO3D_SPIRV_CROSS="$ROOT/out/deko3d_tools/spirv-cross-build/spirv-cross" \
    -DBANJO_DEKO3D_UAM="$ROOT/out/deko3d_tools/uam-build/uam"
  ;;
build)
  ninja -C "$ROOT/out/nx" -k 0 -j "${JOBS:-2}"
  ;;
*)
  sed -n 2,15p "$0"; exit 1;;
esac
