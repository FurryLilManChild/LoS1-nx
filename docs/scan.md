# Castlevania: Lords of Shadow (Xbox 360 disc): feasibility scan (2026-10-25)

Mark's own disc 1 image (`Castlevania - Lords of Shadow (USA, Europe) (En,Fr,De,Es,It) (Disc 1).iso`, XGD2).
Only `default.xex` was copied to the cloud session. Nothing from the game is in this repository.

## The disc and the executable

- Disc 1: 424 files, 7.0 GB: 395 Bink videos (`Video/`, the largest 938 MB), 4 `.packed` data archives, `default.xex`.
  Disc 2 not scanned yet.
- `default.xex`: title 4B4E0809, version 0.0.0.3, disc 1 of 2, image 15.7 MB (Mirror of Fate's: 34.5 MB), entry
  0x824B9BD0, AES-encrypted, basic compression. No guest DLLs on disc 1.
- Built with XDK **2.0.11626** (xam/xboxkrnl import libraries; Mirror of Fate 2.0.21256, SA2 2.0.21076).

## Translation

- ReXGlue 0.10 (sa2-nx's fork): one unresolved tail call (0x8273DBE8 from 0x8274176C) in the first run; with it as
  an override, a complete run: **59,556 functions** (Mirror of Fate 50,887), 182 files, 6.8 million lines, no warnings.
- System calls: 389 imports (285 kernel, 104 XAM), all in the SDK; three of them are stubs: MicDeviceRequest,
  RmcDeviceRequest (microphone, media remote: harmless) and ObReferenceObject (may need a real version).

## Direct3D

- d3d_match against SA2: of the 36 functions Renut hooks or calls, 30 match at 0.95-1.00; to check: Swap (0.84),
  D3DResource_Destroy (0.81), the ring segment allocator (0.86), the draw-time shader load (0.88), CBlocker::Check
  (0.93), the side buffer's out-of-memory function (0.40, a 10-instruction function).
- **The device layout differs.** In matched pairs, Mirror of Fate's device offsets equal SA2's (DrawVertices 7 of 7,
  KickOff 8 of 9, BeginVertices 20 of 20, InitializeApiState 23 of 23); Lords of Shadow's do not (1 of 7, 0 of 9,
  1 of 20, 1 of 23), mostly shifted by a constant per region (0x2B3C -> 0x2ABC, render targets 0x3210 -> 0x3148,
  depth 0x3220 -> 0x3158). Renut's device offsets (renut_engine/gpu/d3d) need a map for this XDK, made the same way
  (pairing instructions in the matched functions).

## Risks

- Performance on the Switch: a full 3D console game; the closest precedent (Sonic Unleashed's Switch port) runs at
  about 400p and 30 fps. Measure early, before polishing.
- Bink video: decoded in software by the recompiled game code; the cutscenes may be heavy for the Switch's CPU.
- Two discs (the game asks for disc 2): the SDK's disc swap or both discs' files in one game folder.
