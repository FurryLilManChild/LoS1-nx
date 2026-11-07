# Apple Silicon macOS build

Requires an Apple Silicon Mac, Xcode command-line tools, CMake, Ninja, Python 3,
and Git. Validated on Apple M2 / macOS 15.6 / Apple Clang 17. Older macOS
versions and Intel Macs have not been tested.

1. Extract the supported game into `assets/game_root` using the existing
   `tools/extract_iso.py`. See the main README for the executable hash.
2. From the repository root, fetch dependencies and configure the native build:

```sh
python3 tools/fetch_thirdparty.py
python3 tools/fetch_renut_deps.py
python3 -m venv out/macos-tools-venv
out/macos-tools-venv/bin/python -m pip install xxhash
cmake -S app -B out/mac -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DREXSDK_DIR="$PWD/sdk" \
  -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DREXGLUE_ENABLE_TRACY=OFF -DLOS_RENUT_RENDERER=ON \
  -DPython3_EXECUTABLE="$PWD/out/macos-tools-venv/bin/python"
cmake --build out/mac --target los_codegen -j 4
cmake -S app -B out/mac
cmake --build out/mac --target los -j 4
```

3. The executable is `out/mac/los`; keep its adjacent runtime libraries and
   `vulkan/` directory. The first build copies `app/los_mac.toml` beside it.
   Existing configurations are preserved: set `sa2_renderer = "renut"` manually
   if an older configuration is already there. Audio remains muted as in the
   upstream defaults; input and audio preferences are configured in `los.toml`.

The existing Renut renderer uses Vulkan through MoltenVK to reach Metal.
The macOS platform implementation supplies the Cocoa window, Metal layer,
and Vulkan loader setup. It does not add a direct Metal renderer.

Opening gameplay was exercised on this Mac. After separating platform code,
the native build and a window/Vulkan-device smoke test passed. Later chapters,
Disc 2, controller input, and other-platform builds remain unverified.

Finder app packaging, convenience scripts, app artwork, additional diagnostics,
and shader-extraction retry handling are reserved for a follow-up contribution.
Game assets, generated game sources, shader caches, and binaries stay local.
