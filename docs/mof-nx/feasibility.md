# Feasibility scan (2026-10-23)

## The game files

- XBLA package: title `58411408`, content type `000D0000`, display name "Castlevania: LOS - Mirror of Fate HD".
  1.9 GB of data in about 4,100 files and folders.
- One executable: `default.xex`, 10.5 MB on disk, 34.5 MB in memory (`0x020E0000`), XEX version 0.0.0.2,
  built 2013-09-25. Encrypted (normal) and LZX-compressed; ReXGlue's loader opens it with the retail key.
- No guest DLLs: nothing to add as `[[modules]]`.
- Data formats: `.bctex` textures, `.pkg` map packs, `.ogg` audio, `.lc` compiled Lua scripts,
  `.bshdat` shader files, `system/files.toc` file table.

## Libraries linked

Every XDK library is build **2.0.21256** (D3D9, D3DX9, XAUDIO2, XAPILIB, XBOXKRNL, XNET, XONLINE...).
The C runtime and compiler are 16.0.11886.

| Game | Direct3D build |
| --- | --- |
| Sonic Adventure 2 (sa2-nx) | 2.0.21076 |
| Sonic & All-Stars Racing Transformed | 2.0.21119 |
| **Mirror of Fate HD** | **2.0.21256** |

sa2-nx's `docs/api-renderer.md` says SASRT's Direct3D flush functions match SA2's instruction for instruction, so
the device layout of a 2012-2013 XDK is likely the same or close. To confirm with `d3d_match.py` before reusing
sa2-nx's hooks.

Middleware found in the decrypted image: **FMOD Ex** (audio; the game's music and sounds are Ogg Vorbis, decoded in
software) and **Lua 5.1** (game and GUI scripts).

## Code translation

ReXGlue 0.10 (upstream, commit c94f5eb), built on Ubuntu 24.04 with clang 20 (`tools/setup_linux_codegen.sh`).

1. First run: analysis stopped on 14 unresolved tail calls to 10 addresses.
2. Those 10 went into `app/overrides.toml`. Second run: complete.
   **50,135 functions, 138 C++ files, 5.2 million lines.**

Open items (see `app/overrides.toml`): the overestimated boundary of `sub_82412000`, two unresolved conditional
branches.

## System calls

315 distinct kernel and XAM imports, all implemented by the SDK. (The first scan listed `DbgPrint`, `sprintf` and
`vsprintf` as missing: it only looked for `REX_EXPORT`, and those three are written with `REX_HOOK_RAW` in
`xboxkrnl_strings.cpp`.)

## Shaders

22 `.bshdat` files with about 80 shader containers in total (53 in `shader_xen.bshdat`, 7 in `shader_fx_xen.bshdat`,
one in each post-processing file), compiled with XDK 2.0.21173. The main shader is an uber-shader that imitates the
3DS GPU's texture combiners (`aCombiners`, `vOperandA`..`D`, `fRGBScale`). For comparison, masseffect-nx's shader
package is about 0.9 GB.

## PC version (Steam, CMOF.exe)

- Built 2014-05-05, 32-bit, from branch `CMOFHD_WINDOWS_STEAM`. Uses Direct3D 9, FMOD Ex, Steamworks.
- The code section is encrypted by Steam's DRM wrapper. Only its readable strings were used; the DRM was left alone.
- All 130 game component class names (`CAlucardAttackComponent`, `CCameraComponent`...) match the 360 build exactly.
  Useful for working out what unnamed 360 functions do.
- Worker threads named in the PC build: `PackLoaderThread`, `LoadingScreenThread`, `SaveDataThread`.

## Expected hot spots on the Switch

- Software Ogg Vorbis decoding in FMOD: a candidate for a native (host) decoder.
- The Lua interpreter.
- Game logic should be light: the same game also ran on the original 3DS (dual-core ARM11 at 268 MHz).

## Risks

- Nothing has run yet. The first boot is where the real unknowns start.
- FMOD on the 360 may use system features the SDK handles less well than plain XAudio2.
