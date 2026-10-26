# Testing mof-nx on the Switch

## SD card layout

```
sdmc:/switch/mof-nx/mof.nro
sdmc:/switch/mof-nx/game_root/          your game files (assets\game_root), first start only
sdmc:/switch/mof-nx/game_root.nxpack    made from game_root/ on the first start (one file: fast to read)
sdmc:/switch/mof-nx/game_root.nxidx     its index
sdmc:/switch/mof-nx/deko3d_shaders.bin   compiled shader variants (the console adds what it compiles; tools/deko3d_variant_cache.py)
sdmc:/switch/mof-nx/mof.toml            your options (app/mof_switch.toml), written when missing
sdmc:/switch/mof-nx/logs/               the test builds' logs (logs/rex/rex_crash.log after a crash)
```

Once the pack exists it is always used, and `game_root/` can be deleted from the card.

## Launching

The game needs more memory than a homebrew app gets from the Album (applet mode). Start it from the homebrew menu
opened with a title override: hold **R** while starting any installed game.

## How the NRO is built (2026-10-24)

`tools/linux/build_switch.sh` in a cloud session: devkitPro's Windows devkitA64 (GCC 16.1) from Mark's PC under Wine,
the deko3d shader chain's host tools built natively, Renut on deko3d with no Vulkan driver (`REXGLUE_SWITCH_NO_NVK`),
the game shaders' GLSL package built into the NRO (test builds), no LTO or PGO yet, logging on
(`app/mof_switch_defaults.toml`). The ELF of each NRO handed over is kept in `out/nx/elf/` for crash logs.

Checks on the first NRO (sa2-nx's check_switch_build.py by hand): `_start` at 0, no Mesa string, one vector fmla (in
sub_824DA2D0, whose code has std::fma), five branch-to-self loops in recompiled functions: none is a guest `b .`; each
is a loop the guest itself would never leave on a degenerate path (sub_82680788's list search off a null node, and
loops with no progress and no memory another thread writes), not a polling loop.

## Builds

| Date | NRO SHA-256 (first 16) | Notes |
| --- | --- | --- |
| 2026-10-24 | 4c4893c544e07983 | First build: no LTO, logging on |

## First console session (2026-10-24, Mark's V1 Switch, handheld)

Boots, packs the game files (first start), title without flicker, new game, cutscenes, sound, play: "plays incredibly
well". 30 fps held (31 ms a frame), heap 1.3 of 3.1 GB, no crash. Found:

- the last letter of each line of text cut off, and triangle artifacts in torches and flames (both not on the PC: the
  deko3d path; open);
- one-time stutters when something is first drawn: uam compiled a shader variant at run time (avg 549 ms each).
  `out/deko3d_cache/deko3d_shaders.bin`: 371 variants precompiled from the PC run's pipeline list (1,038 pipelines),
  for the card.
