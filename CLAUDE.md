# LoS1-nx: notes for Claude

Native PC and Switch port of Castlevania: Lords of Shadow (Xbox 360 disc, title 4B4E0809) via ReXGlue static
recompilation. It follows mof-nx (https://github.com/FurryLilManChild/mof-nx, the same owner's Mirror of Fate HD port,
which runs on PC and the Switch), itself built on sa2-nx's method (https://github.com/ChanseyIsTheBest/sa2-nx,
docs/porting-guide.md). mof-nx's documents are kept in docs/mof-nx/ as reference.

## Who is working on this

The owner (Mark) is very tech savvy but has almost no coding experience. Claude writes the code and scripts. Mark
runs builds on his own machines, tests on the PC and his Switch, and sends logs. Instructions for him must be numbered,
click by click, with copy-paste commands. Never use em dashes in anything written for him or in the docs.

His hardware: a Windows PC (Lenovo Legion Go), an unpatched V1 Switch on Atmosphere CFW, an Xbox 360 with Aurora.
His disc images are in `C:\LoS1-nx\assets\` (two .iso files); a cloud session reaches them through the device bridge.
He moves files to the Switch's SD card himself over USB; he does not want Claude to control the Switch's folders.
A good outcome for the Switch: the quality of the Sonic Unleashed Switch port (UnleashedRecomp-NX), which runs well on
his console; it may be used as a reference.

## Hard rules

- Never commit game data or anything derived from it: game files, disc images, `app/generated/`, decrypted images,
  shader dumps, shader packages. `.gitignore` covers these; check `git status` before every commit.
- Every entry in `app/overrides.toml` carries its evidence (which run or tool found it, callers, addresses).
- Log each session in `docs/progress-log.md` (newest first).

## Builds

- PC: `tools\win\build_pc.ps1` on Mark's PC (docs/windows-build.md). A cloud session can also build and run the Linux
  build (Xvfb, lavapipe, slow) with the game files copied over.
- Switch: `tools/linux/build_switch.sh` in a cloud session (devkitPro's Windows devkitA64 from Mark's PC under Wine;
  see mof-nx's docs/mof-nx/switch-testing.md).
- The code generator in a fresh cloud session: `tools/setup_linux_codegen.sh`, with `default.xex` in
  `assets/game_root/` (extract it on Mark's side with `tools/extract_iso.py --xex-only` and stage only that file).
