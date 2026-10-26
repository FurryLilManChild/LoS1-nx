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
- Next: Mark's PC build with `-Renut` and a boot test.

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
