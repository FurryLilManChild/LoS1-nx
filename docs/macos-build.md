# Apple Silicon macOS build

This build translates the Xbox 360 game's PowerPC code to C++ and compiles it
for ARM64 using the LoS1-nx fork of ReXGlue. Graphics use Renut's Direct3D-level renderer, Vulkan,
and MoltenVK to reach Metal, with the Xenos backend available as a fallback.
This is static recompilation, not recovery of the
original MercurySteam source code.

The starting project is [LoS1-nx](https://github.com/FurryLilManChild/LoS1-nx),
commit `9c70bb54fc268d14400f7884470879188db70132`. Its SDK is based on
[ReXGlue](https://github.com/rexglue/rexglue-sdk), commit
`c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`, with the project's game and Switch
fixes. ReXGlue incorporates instruction translation and analysis work from
[XenonRecomp](https://github.com/hedge-dev/XenonRecomp).
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) handles original Xbox x86
XBE programs and cannot translate this Xbox 360 PowerPC XEX.

## Requirements

- Apple Silicon Mac with Xcode and its command line tools selected. This build
  used the macOS 26.2 SDK and was validated on macOS 15.6. The build helper
  requests a macOS 15.0 deployment target; older systems have not been tested.
- CMake, Ninja, Python 3, and Git. This machine has Apple Clang 17, CMake 4.4.5,
  Ninja 1.13.2, and macOS 15.6.
- Extracted game data in `assets/game_root`.
- The executable SHA-256 must be
  `0287bbd803d36d3da59750df4de69b2e040b4f8604d85ddd528d9027a6dbb46b`.
  `tools/macos/build.sh` checks this before building.

## Build and run

1. Extract Disc 1 into `assets/game_root` using `tools/extract_iso.py` (see
   [Disc data](#disc-data) below).
2. Open Terminal in the repository root and run:

```sh
tools/macos/build.sh
tools/macos/package.sh
tools/macos/play.sh
```

3. After a successful build, open `Lords of Shadow.app` beside the checkout
   in Finder, or use `tools/macos/play.sh` as above.

The bundle contains the
ARM64 executable and its graphics/runtime libraries; its configuration points
to this checkout's game assets. Keep the checkout's `assets/` folder in place. Run
the package script again after rebuilding to update the application. Close the
game before packaging. Double-clicking the app in Finder and `play.sh` both use
the normal macOS application launch path. Direct execution from the build task
produced a black window during testing, so it is not the recommended launcher.
Set `LOS_APP_PATH` for both packaging and launching to use a different app location.

The build defaults to four compiler jobs on this 16 GB Mac. Set
`LOS_BUILD_JOBS` to change the count. The first build fetches SDK dependencies,
builds the code generator, translates the executable, and builds the game and
graphics libraries. Later builds reuse existing outputs.

The executable is `out/mac/los`. Keep its adjacent `.dylib` files and `vulkan/`
directory. Build logs are in `out/logs/mac-*.log`. App runtime logs are in
`Lords of Shadow.app/Contents/MacOS/logs/los_*.log`. Settings are in
`out/mac/los.toml`; rebuilding preserves edits to that file.
The packaged app has its own `Contents/MacOS/los.toml`. The runtime's default
save/profile/cache location on this Mac is `~/.local/share/los` (or beneath
`XDG_DATA_HOME` if that environment variable is set).

Sound and keyboard input are enabled by default. A connected controller is
also supported by the runtime. Keyboard mappings:

| Action | Key |
| --- | --- |
| Start | Return |
| A / confirm | Space |
| B / back | Backspace |
| X, Y | L, P |
| Left stick | WASD |
| Right stick | Arrow keys |
| Left/right trigger | Q, E |
| Left/right bumper | 1, 3 |
| Back | Tab |
| D-pad | Shift + arrow keys |

Command-Q quits. Return pauses during gameplay. F3 toggles the runtime debug
overlay and F4 opens its settings.

## Validation on this Mac (2026-11-07)

The default Renut renderer measures **59–60 FPS** in the opening village's
first combat tutorial overlay, versus **8.2 FPS** with Xenos GPU emulation.
Both use 1280x720 internal resolution and full shadows. Four steady 10-second
windows averaged 59.57 FPS versus 8.17 FPS, approximately **7.3x** faster.
Three subsequent windows during the short combat test measured 51.59, 53.39,
and 59.17 FPS. These are window averages, not minimum frame rates.

The Xenos baseline saturated the GPU at approximately 95%. Changing its
presentation mode did not help; its FSI mode was slower with visible
corruption. Both experiments were reverted. Initial pipeline compilation can
still cause hitches. These measurements were collected locally; generated
builds, shader caches, and runtime logs are not included in the repository.

- Built an ARM64 executable with Apple Clang 17 on the Apple M2, macOS 15.6.
- Translated 59,494 function definitions from the matching Disc 1 executable.
- Visually verified the title screen, the Besieged Village introduction,
  opening combat, keyboard movement/attacks, and the pause menu through the
  packaged app. The renderer uses MoltenVK 1.4.1 on the Apple M2 via Metal.
- Save creation succeeded: the runtime wrote `data.dat` and `__thumbnail.png`
  beneath `~/.local/share/los/B13EBABEBABEBABE/4B4E0809/00000001/savegame`.
  Audio is enabled; the SDL output stream ran without persistent underruns.
- This is an opening-level smoke test, not a complete campaign playthrough.
  Disc 2 was not part of this validation. Controller input, later
  chapters, DLC, and the late-game disc transition have not been tested here.

Local build fixes: the registration-only translation unit compiles at `-O0`
without a PCH and without Apple LLVM GlobalISel, whose localizer otherwise
spent minutes on the enormous registration function. Gameplay code remains
optimized. The SDK's Vulkan staging helper now resolves its ICD file relative
to the CMake function definition, which works when embedded in this app.
Renut also needed Cocoa/Metal-layer window integration, Vulkan loader
discovery, macOS DXC selection, Apple framework linkage, and corrected shader
extraction. All 1,634 pixel and 372 vertex shader containers were translated.

## Renderer selection and profiling

Renut is built and selected by default on macOS. It avoids the Xenos
PM4/EDRAM emulation path. The build helper fetches the pinned renderer
dependencies and macOS DXC, installs
`xxhash` into `out/macos-tools-venv`, extracts the game's 2,006 shaders locally,
and compiles their SPIR-V cache. The packaged app selects it with
`sa2_renderer = "renut"` in `Contents/MacOS/los.toml`.
Keep `gpu_plugin = "xenos"` so setting `sa2_renderer = "xenos"` and restarting restores the
emulated renderer without another build. Neither renderer needs an external
Vulkan installation when the bundled `vulkan/` directory is present.

To omit Renut from a build, set `LOS_RENUT_RENDERER=OFF` when running the build
helper and use `sa2_renderer = "xenos"` in the runtime configuration.

Renut logs frame-rate and GPU timing every 10 seconds. For the Xenos renderer,
temporarily set `present_painted_profile = true` to obtain comparable host
presentation counts. Measure after loading and shader compilation finish,
without rebuilding in the background.

MoltenVK does not expose geometry shaders. Renut reports a fallback for
rectangle lists and point sprites; the tested opening scene did not report
use of the rectangle fallback. Effects in later chapters remain unverified.

## Disc data

Use the repository's existing extractor with an explicit path to each ISO:

```sh
python3 tools/extract_iso.py '/path/to/Disc 1.iso' -o assets/game_root
python3 tools/extract_iso.py '/path/to/Disc 2.iso' -o assets/game_root_disc2
```

Disc 2 belongs in `assets/game_root_disc2`. The SDK mounts this folder at
startup and redirects the game's disc-swap request to it. Disc 1 is sufficient
to test the opening; the complete campaign requires both discs. Upstream has
not verified the actual late-game disc swap.

Game files, translated C++, shader caches, and built binaries are local outputs
ignored by Git.
