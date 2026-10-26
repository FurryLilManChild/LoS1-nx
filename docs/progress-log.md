# Progress log

Newest first. Each entry: what was done, what was found, what is next.

## 2026-10-26: Renut's renderer mapped to this game

- Mark's first play (emulated GPU): the first level plays through; only the Travel Book menus flicker (the emulated
  GPU's "invalid texture fetch constant" warnings).
- Renut's renderer mapped to this Direct3D (XDK 2.0.11626): 41 functions, 3 data words and every device offset
  ([d3d-map.md](d3d-map.md)).
- Shader extractor for the .packed archives (`tools/los_dump_shaders.py`): 2,006 shaders from both discs, every one
  the game used at its title screen on the Linux test.
- Linux runs (lavapipe) found and fixed four things this game does that Mirror of Fate does not: the render thread's
  wait for the display's flip count, the ring space query, the device's own vertex declaration, predicated tiling
  ([renderer.md](renderer.md)). The game now runs with the renderer to its 3D title scene without a crash, but the
  frames captured there are black; whether that is lavapipe or the renderer is for Mark's PC test.
- Mark's first `-Renut` build: the shader translator's second run (Renut's early alpha copies) stopped at 98 % with
  no message; the first run had built all 2,006 (32 pixel shaders failed DXIL validation, "coord uninitialized": no
  D3D12 code for them, the Vulkan code is there). The early alpha pass is off in `build_pc.ps1` for now.
- Mark's PC had never pulled (its branch had no upstream: `git pull` did nothing); with the code in place the shader
  step still stopped at 98 %: XenosRecomp exits with code 2 when any shader fails, and the 32 DXIL validation
  failures count. The PC build now makes Vulkan code only (`RENUT_DXIL=OFF`, the renderer then always uses Vulkan).
- Mark's PC build (Vulkan): runs at 60 fps, but black. The same on the Linux run, whose captures work for Mirror of
  Fate. Cause: this Direct3D builds the declaration of an FVF draw inside the device (+0x3050) and binds that, so the
  renderer had no vertex layout for the 2D draws. Each element list now gets a registered copy: the Linux run shows
  the loading screen and the title screen.
- Mark's PC test (Vulkan): "looks and plays well"; the Travel Book menus no longer flicker.
- First Switch build (`tools/linux/build_switch.sh` in the cloud session, about 1 h 20 min on 2 cores): 2,006 shaders
  in the deko3d package (32 kept without the GLSL passes: uam failed on their rewrite). No asset pack on the Switch:
  the game files are 6.7 GB and a FAT32 card holds no file over 4 GB, so game_root/ is read as it is.
- Mark's first console test: "Switch: app initialization failed" at once (no Vulkan driver for the emulated GPU): the
  Switch configure never turned `LOS_RENUT_RENDERER` on, so the NRO had no Renut and fell back to the emulated GPU.
  `build_switch.sh configure` now turns it on; rebuilt (282 steps).
- Mark's second console test: plays through the first level into the second ("pretty dang successful"; "good, not
  quite great", some stutter). Log: 37-53 fps in light scenes, 17-25 fps in the heavy ones with the GPU at 40-47 ms
  a frame (GPU-bound); 130 run-time uam compiles at about 70 ms each (the stutter; no deko3d_shaders.bin yet).
- Next: the precompiled shader variants (deko3d_shaders.bin) from this run's log, then GPU time, then disc 2.

## 2026-10-26: first PC boots

- Mark's first build stopped at Mirror of Fate's shader dump step (no .bshdat files: "no inputs"). The dump and
  Renut's renderer now only run with `build_pc.ps1 -Renut`.
- First boot (Mark's PC, emulated GPU, D3D12): the game opens Data00.packed and English.packed, looks for `D:\Voices\`
  and `D:\mods\` (not on the disc), opens audio, sets the GPU interrupt callback (824A39F0), names its threads
  (SwapDiscThread, FileAlignThread, PS_Thread, PxSimulate_Thread), then "Call to invalid or unregistered function at
  guest address 0x826FFE88".
- `tools/pointer_fixpoint.py` over the image: 156 candidates, 131 accepted (among them 0x826FFE88), 25 rejected (8
  jump tables, 4 funclets, 11 no code, 1 no terminator, 1 fragment); the second pass found none.
- setjmp/longjmp found ahead of the Lua panic Mirror of Fate's third boot hit: 0x826F0A00 and 0x826F0CD0 (by Mirror
  of Fate's copies). Codegen: 59,682 functions, no warnings, setjmp emitted in 8 files.

## 2026-10-25: start

- Feasibility scan of disc 1's `default.xex` ([scan.md](scan.md)): 59,556 functions, one override, every import in
  the SDK; XDK 2.0.11626, whose Direct3D device layout differs from SA2's and Mirror of Fate's.
- Disc 2 listed: 425 files, 6.6 GB: 394 Bink videos, `Data00.packed` (641 MB) and `Data01.packed` (1.68 GB) of its
  own (disc 1's: 653 MB and 1.37 GB), the same `English.packed` (36.6 MB) and `Music.packed` (220 MB), the same
  `default.xex`.
- Repository made from mof-nx's tree (the SDK fork, the app, Renut's renderer, the tools), renamed for this game.
  Renut's renderer is off (`LOS_RENUT_RENDERER`) until its Direct3D map is made for this XDK.
- Linux build (clang 20, emulated GPU): compiles; the link needed sa2-nx's self-check stubs outside the renderer's
  glue (`app/src/los_stubs.cpp`). A 60 s run with only `default.xex` in the game folder: no crash; the game names a
  `SwapDiscThread`, tries to create `D:\filelist.2Bpack` on the (read-only) game disc, then to open it (not found).
