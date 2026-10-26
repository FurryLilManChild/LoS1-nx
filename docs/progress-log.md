# Progress log

Newest first. Each entry: what was done, what was found, what is next.

## 2026-10-25: start

- Feasibility scan of disc 1's `default.xex` ([scan.md](scan.md)): 59,556 functions, one override, every import in
  the SDK; XDK 2.0.11626, whose Direct3D device layout differs from SA2's and Mirror of Fate's.
- Disc 2 listed: 425 files, 6.6 GB: 394 Bink videos, `Data00.packed` (641 MB) and `Data01.packed` (1.68 GB) of its
  own (disc 1's: 653 MB and 1.37 GB), the same `English.packed` (36.6 MB) and `Music.packed` (220 MB), the same
  `default.xex`.
- Repository made from mof-nx's tree (the SDK fork, the app, Renut's renderer, the tools), renamed for this game.
  Renut's renderer is off (`LOS_RENUT_RENDERER`) until its Direct3D map is made for this XDK.
