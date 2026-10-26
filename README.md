# LoS1-nx

A work-in-progress native PC and Nintendo Switch port of **Castlevania: Lords of Shadow** (Xbox 360, 2010), made with
[ReXGlue](https://github.com/rexglue/rexglue-sdk) static recompilation, following
[mof-nx](https://github.com/FurryLilManChild/mof-nx) (Mirror of Fate HD) and
[sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx).

It is not an emulator: the PowerPC code of the game's `default.xex` is translated to C++ and compiled for the PC and
the Switch, and the game's Direct3D calls are drawn by a native renderer (deko3d on the Switch, D3D12 or Vulkan on the
PC).

## Status

| Stage | State |
| --- | --- |
| Code translation (`rexglue codegen`) | Done: 59,682 functions |
| PC build | Playable with the native renderer (Vulkan) and with the emulated GPU |
| Switch build | Playable on the console: the first levels tested; 40-50 fps in light scenes, 17-25 in the heaviest |
| Disc 2 | Written (the disc swap switches to a `game_root_disc2` folder), not yet tested at the swap |
| Known issues | Switch: one-time stutters the first time a shader is used (a precompiled shader file helps), slowdowns in heavy scenes |

Session-by-session notes: [docs/progress-log.md](docs/progress-log.md). The native renderer and what this game needed:
[docs/renderer.md](docs/renderer.md), [docs/d3d-map.md](docs/d3d-map.md).

## What you need

Your own copy of the game: both discs as `.iso` dumps. Supported `default.xex` SHA-256:
`0287bbd803d36d3da59750df4de69b2e040b4f8604d85ddd528d9027a6dbb46b`.

Extract each disc with `tools/extract_iso.py` into `assets/game_root` (disc 1) and `assets/game_root_disc2` (disc 2).
The `assets/` folder never goes into git.

Building on Windows: [docs/windows-build.md](docs/windows-build.md). The Switch build: `tools/linux/build_switch.sh`
(devkitPro's devkitA64 and libnx), with the Switch testing notes in [docs/mof-nx/switch-testing.md](docs/mof-nx/switch-testing.md).

## Layout

| Folder | Contents |
| --- | --- |
| `app/` | The game project: manifest, hand-written fixes (`overrides.toml`), the native renderer (`src/renut_engine`) and its glue, settings |
| `tools/` | Disc extraction, shader extraction, code generator setup; `tools/win/` the Windows build and test scripts; `tools/linux/` the Switch build |
| `sdk/` | sa2-nx's fork of the ReXGlue SDK (0.10) |
| `patches/` | Local changes to third-party code, as diffs |
| `docs/` | Progress log, renderer notes, mof-nx's documents (`docs/mof-nx/`) |

## Legal

No affiliation with Konami, MercurySteam, Microsoft or Nintendo. "Castlevania" is a trademark of Konami.
This repository contains no assets or program code from the original game. Users must provide their own legally
obtained copy, and everything made from it (`assets/`, `app/generated/`, shader dumps and packages) is made on their
own computer. Running homebrew requires custom firmware, which violates Nintendo's terms of service.

Builds are made from your own copy of the game: the executables and NROs contain the game's translated code, and the
shader files contain its translated shaders. Do not share built files (`.exe`, `.nro`, `deko3d_shaders.bin`, shader
packages); everyone builds from their own game.

Source code is provided under the GPL-3.0 License (see [LICENSE](LICENSE)), like the ports it builds on.

## Credits

- **Konami and MercurySteam**: creators of Lords of Shadow. This is an unofficial, fan-made port.
- **[ChanseyIsTheBest](https://github.com/ChanseyIsTheBest/sa2-nx)**: sa2-nx, its porting guide and tools.
- **[Tom Clay](https://github.com/rexglue/rexglue-sdk)**: the ReXGlue SDK, built on the work of the **[Xenia](https://xenia.jp)** team.
- **masterspike52**: reNut, the Direct3D-level renderer (through banjo-nx and sa2-nx).
- **[hedge-dev](https://github.com/hedge-dev/XenosRecomp)**: XenosRecomp, the shader translator.
- **devkitPro**: devkitA64, libnx and deko3d, and **fincs**' uam shader compiler.
