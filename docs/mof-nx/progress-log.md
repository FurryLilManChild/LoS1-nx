# Progress log

Newest first. Each entry: what was done, what was found, what is next.

## 2026-10-24 (afternoon): the first Switch build

- devkitPro's package server is not reachable from the cloud session; Mark's installed devkitPro (devkitA64 r30, GCC
  16.1, libnx 4.12, deko3d 0.5, uam, switch-tools) was copied and runs under Wine (`tools/linux/build_switch.sh`).
- The deko3d shader chain (sa2-nx's): Renut's translator with BANJO_DEKO3D_SPIRV_DIR, SPIRV-Cross, the GLSL passes
  (all 104 compile with uam), the package (254 KB) and the Switch shader cache. Host tools built natively on Linux.
- Project: the toolchain file, `-mearly-ra=none`, `-ffp-contract=off -fno-strict-aliasing` for the game code, `-O0` for
  mof_register.cpp, the fixed settings compiled into the NRO (`app/mof_switch_defaults.toml`), the NRO entry.
- A stand-in for the Mesa SDK (Vulkan headers, an empty libvulkan.a, a Switch libzstd.a built from source).
- One build fix: the deko3d host shader step took DXC as one program (a wrapper script on a Linux host).
- First NRO: 53 MB, checks in [switch-testing.md](switch-testing.md). Handed to Mark for the console.
- First console session (Mark's V1 Switch): boots, plays "incredibly well", 30 fps, sound, no crash. Mark called it a
  major win and paused for a couple of days.

Next (when Mark is back): the last letter of each text line and the triangles in flames (deko3d path only; a console
screenshot of each first), a homebrew menu icon, LTO, a release build with logging off.

## 2026-10-23 (evening): first Renut boot

- Mark's PC built the Renut renderer (one-time setup: plume, XenosRecomp, DXC, shader dump). Boot with
  `-Renderer renut`: D3D12 on the Radeon, the game runs to the title screen (same file opens as the emulated GPU),
  black screen, "0 draws" in every report. Cause: Renut's six shared hooks (draws, BeginVertices, AllocateInlineData,
  CBlocker::Check) are only functions; sa2-nx's own hooks call them. Added the six hooks to `mof_renut_glue.cpp`.
- Second Renut boot: draws arrive and Mark played his save, but parts of the picture flickered constantly. The log had
  colour targets of 1x8705 and 7384x27854 (231 failed creations) and only 2 host render targets.
- The game files copied to the cloud workspace (7 tar parts through the device bridge, `out\xfer` on Mark's side) to
  run the Linux build there (Xvfb, lavapipe, 3 to 7 fps). Note: with no sound device, the Linux build crashes at
  boot (guest access violation right after the audio open fails); an ALSA null device (`~/.asoundrc`) avoids it.
- `app/src/mof_render_targets.cpp` (with `MOF_RT_DIAG=1`) logged every bound surface: the game builds a new surface
  object for each SetRenderTarget (sub_824C6630, call at 0x824C67E0), each a correct copy of the 1280x720 back
  buffer's header, in memory it soon reuses. Renut's render thread reads a draw's targets a frame or more later and
  found other data. Fix: the bind hooks remember each surface's descriptor (`RememberBoundSurface` in Renut's
  `surfaces.cpp`), and the render thread uses that copy. Linux run after it: no bad targets, the logos and title
  screen draw cleanly.
- Mark's third test (PC, D3D12): backgrounds steadier, the HUD fade no longer goes black, the logos no longer tear.
  Still: lighting and shadows flicker from the title screen on, and a square shadow under each character. His Game
  Bar clip: at the title, whole layers are missing in some frames (the moon; once the whole background). The Linux
  build (Vulkan, a few fps) does not flicker, and a comparison with the emulated GPU in the prologue showed the fire's
  glow cut along a diagonal edge there. `tools/win/ab_test.ps1`: title-screen screenshots with one suspect switched
  off a run (Vulkan instead of D3D12, resolve skip, snapshot deltas, record worker, exact streams, shared constants,
  the enhancement groups).
- ab_test results (Mark's PC): every variant flickers, Vulkan included, so it is not D3D12's and will matter on the
  Switch. The moon area of the title screen jumps between a few levels (96, 70, 53, 23, 0 of 255): whole layers
  present or missing.
- Not occlusion queries: the five Direct3D functions that write ZPASS_DONE (EVENT_WRITE_ZPD) are never called up to
  the title screen (counted on the Linux build).
- `MOF_FRAME_DIAG=<seconds>` (mof_renut_glue.cpp): one log line per frame for 150 frames, with the draws the game
  issued and a signature of the set. Linux build at the title: all 150 frames the same (44 indexed draws, one
  signature). `tools/win/frame_diag.ps1` runs it on the PC.
- Mark's PC, frame_diag: the same 44 draws with the same signature in all 150 frames. The game is steady; the
  renderer loses layers.
- The cause: the game builds its texture, vertex buffer and index buffer headers in memory it reuses (Linux build,
  title screen: a header address bound to a draw held other words than at its previous draw in 42 % of the index
  buffer checks, about 10 % of the texture and vertex buffer ones), and Renut's render thread reads them through the
  device's pointers a frame or more later. Renut also names host textures by header address, so a resolve into one
  header was not found by draws sampling the same memory through another.
- Fix: `app/src/mof_resource_copies.cpp`. Around each draw the device points at stable copies: one permanent copy
  per texture memory (address, format, size; Direct3D's own headers stand for themselves), used by the draws, the
  resolves and the Swap's front buffer (Renut's draw.cpp and device.cpp, marked mof-nx), and ring copies of the
  buffer headers by content. `MOF_HEADER_COPIES` (1 buffers, 2 textures, 4 resolves and Swap; default 7) for tests.
  Linux build after it: the title text is no longer garbled at its last letters, and the prologue's fire is no
  longer cut along a diagonal (matches the emulated GPU).
- Mark's PC with the copies (2026-10-24): the flicker is unchanged. His second clip: the whole 3D scene is black in
  single frames (title and in game) while the HUD and menus stay. Nothing skipped or pending in the renderer's
  reports.
- `MOF_TRACE_FRAMES=N` (`MOF_TRACE_AT` seconds, default 40): Renut's resolve trace and draw log for N frames in a row,
  with each draw's texture descriptors and the skipped or unfitting resolves. The title frame on Linux: 44 draws and
  about 15 resolves, a bloom chain through the one 1280x720 target. `tools/win/frame_diag.ps1` now records 150 frames
  of it on the PC.
- Mark's PC trace (150 title frames): the surface at 50139FF0 was 1280x720 in some frames and 640x360 in others.
  The game reuses surface objects for targets of other sizes (its bloom chain: 1280x720, 640x360, 320x180,
  160x90), and RememberBoundSurface kept each address's newest words: when the game ran a frame ahead, a pass took
  the next frame's size (the whole scene black in single frames). On Linux every pass had shown as 1280x720.
- Fix: render target surfaces get permanent copies by content too (mof_resource_copies.cpp), around the draws,
  ClearF and Resolve (Renut's hooks, marked mof-nx). Linux trace after it: one stable surface per size, right
  picture.
- Mark's playtest (2026-10-24, PC, Renut on D3D12): **flawless**. No flicker, the square shadows gone, the HUD fade
  correct, no tearing. Next: the Switch build.

## 2026-10-23 (afternoon): toward the Switch

- Shaders: `tools/mof_dump_shaders.py` (sa2-nx's dump script for the .bshdat files): 160 containers, 104 unique
  (77 ps, 27 vs). The executable holds none.
- Direct3D map ([d3d-map.md](d3d-map.md)): Mark's SA2 XBLA executable as the reference (sa2-nx's edition, 0.0.0.3),
  translated with sa2-nx's own manifest. All 36 Direct3D functions Renut hooks or calls match at 1.00; three data
  addresses and GameXMemFree (0x82447D48) by pairing instructions. The device layout is SA2's.
- The SDK is now sa2-nx's fork (`sdk/`, ReXGlue 0.10 c94f5eb plus sa2-nx's patch: the Switch layer and the app
  hooks Renut needs). Its third-party sources come from the official release (`tools/fetch_thirdparty.py`, which
  also replaces Windows git's symlink stubs). Checked on Linux: the fork's code generator gives the same result
  (50,882 functions, the same 3 warnings) and the app, GPU plugin and sample translated files compile.
  `app/cmake/rexglue.cmake` is now the fork's (it also allows a Switch cross build).

## 2026-10-23 (later): PC project and Windows scripts

- Mark's Windows PC set up (VS 2022 Build Tools, LLVM 20.1.8, CMake, Ninja, Python, Git + GCM). Repo cloned to
  `C:\mof-nx`, game extracted to `assets\game_root` (3,262 files, default.xex hash matches).
- App project from the `rexglue init` template: `app/CMakeLists.txt` (emulated Xenos GPU), `app/mof.toml`
  (license_mask = 1, muted), `app/cmake/rexglue.cmake`.
- `tools/win/`: `env.ps1`, `check_tools.ps1`, `build_pc.ps1` (SDK pinned to c94f5eb), `boot_test.ps1`.
  Logs go to `out\logs\` as UTF-8 so Claude can read them through the connected folder.
- Checked on Linux: the project configures, runs the code generator through the build (exit 0), and compiles the
  app sources, the xenos GPU plugin and sample translated files (mof_recomp.0, and mof_recomp.27, the largest: 20 s
  on one core). Not linked here (2 cores; the full compile is left to the Windows PC).
- Fix found by that check: rexruntime absorbs the SDK's rexui object library without passing on its include paths,
  so main.cpp could not find imgui.h. app/CMakeLists.txt adds imgui's and renderdoc's include paths to the app.
- Mark's PC has devkitPro installed, and `c:\devkitPro\msys2\usr\bin` comes first on his PATH: its cmd, cmake and
  git (2.49, without Credential Manager) shadowed the Windows ones. `tools/win/env.ps1` now drops MSYS2/Cygwin
  folders from the build session's PATH, runs vcvars through `%ComSpec%`, and searches several LLVM locations.
- First Windows build: stopped at libmspack's `lzxd.c` ("expected identifier"): Windows git checked the submodule's
  15 symlinks out as text files holding their target paths. `tools/fix_symlinks.py` replaces them with copies;
  `build_pc.ps1` runs it after the SDK fetch. Mark's copy was fixed in place through the device bridge.
- Second Windows build: complete in under 15 minutes. `out\pc\mof.exe` (52 MB), `rexruntimerd.dll`,
  `rexgpu-xenosrd.dll` (the loader asks for the same "rd" RelWithDebInfo postfix), `mof.toml`.

- First boot: D3D12 on the Radeon, guest memory mapped, audio opened, then 2 s in: "Call to invalid or unregistered
  function at guest address 0x82619740". A small leaf (`cmpwi; bnelr; b 0x8260B558`) whose address is only built in
  code (lis/addi at 0x8260B660/0x8260B670) and passed as a callback, so discovery never saw it.
- Swept the whole game instead of fixing one per boot: `tools/pointer_fixpoint.py` (+ `find_gaps.py`,
  `gap_classify.py`, from sa2-nx) over the decrypted image. Pass 1: 779 candidates, 758 accepted; pass 2: fixpoint.
- 5 of the 758 were switch jump tables (address built for `lwzx; mtctr; bctr`), which split their functions
  (new "Unresolved conditional branch to 0x825ABC28"). Removed; the tool now rejects jump tables (22 found in the
  game). Final: 753 pointer-reached functions, 50,887 functions in total, the same 2 warnings as the first run.

- Second boot: past 0x82619740. `NtCreateFile` of `game:\system.pkg` fails (no such file in the package: likely an
  optional probe), the GPU interrupt callback is set (826F8170), then exit code 0x80000003 (STATUS_BREAKPOINT) with
  no log line. Candidates: the SDK's DbgBreakPoint, RtlRaiseException (C++ throw 0xE06D7363) and KeBugCheck(Ex) all
  end in a host debugger break. Guest traps only log, and nothing was logged.
- `app/src/mof_diag.cpp` replaces those four imports: logs the guest call stack, decodes thrown C++ type names,
  lets DebugBreak continue (retail behaviour), stops with a clear [FATAL] line otherwise.

- Third boot: the diagnostics named it: "game bug check 00000000", called from the CRT's exit() after `exit(1)` in
  Lua's luaD_throw (sub_823E3718). Lua had a protected call active and longjmp'd to it (0x82609460), but longjmp
  recompiled as a plain call returns, so luaD_throw fell through to its panic branch: Lua panic, then exit(1).
  The stack ran through the game's Lua function `ImportLibrary`, then luaL_argerror ("bad argument #%d to '%s'").
- Fix: `setjmp_address = 0x82920D60`, `longjmp_address = 0x82609460` in overrides.toml (porting guide 3.2). The
  generator now emits host setjmp/longjmp: 5 setjmp and 19 longjmp sites.

- Fourth boot: **reaches the title screen** and is still running after 60 s. No audio only because test runs are
  muted (`audio_mute = true`). The only failed opens: `system.pkg` and the PS3 control-sprite scripts
  (`gui\scripts\sprites_ps3controls*.lc`), which the 360 package does not have; the game's Lua catches those.
- `tools/win/play.ps1`: runs with sound and no time limit, keeps the log.

- First playtest (8 min, Legion Go): **playable**. Prologue complete, Chapter 1 to past the first save font,
  every tutorial input works, sound works. Issues in [known-issues.md](known-issues.md): black scene during the
  HUD fade, tearing, minor stutter, exit code 0xC0000374 at the end of the session.

Decision: the HUD flash and tearing live in the emulated GPU path, which the Switch will not use. Next is the
road to the Switch: shaders, the native renderer from sa2-nx (tested on PC first), then the Switch build.

## 2026-10-23: feasibility scan and first translation

- Extracted the XBLA package with `tools/stfs_extract.py` (from sa2-nx).
- Built ReXGlue 0.10 on Linux (`tools/setup_linux_codegen.sh`) and translated `default.xex`:
  10 manual function entries, then a complete run (50,135 functions).
- Decrypted image dumped with `patches/rexglue-dump-image.patch` for string analysis.
- Compared with the PC (Steam) build: same 130 component classes.
- Findings: [feasibility.md](feasibility.md).

Next: PC test build and first boot on Windows. The game project (`main.cpp`, CMake) still has to be set up, starting
from sa2-nx's SDK fork, which has the Switch layer, rather than upstream ReXGlue.
