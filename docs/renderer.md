# Renut's renderer in Lords of Shadow

Renut's Direct3D-level renderer (reNut by masterspike52, through banjo-nx, sa2-nx and mof-nx) draws the game's Direct3D
calls directly with plume: D3D12 or Vulkan on the PC, deko3d on the Switch. It came over from mof-nx
(docs/mof-nx/renut-port.md) with Mirror of Fate's addresses; this page is what Lords of Shadow needed on top.

Build it with `tools\win\build_pc.ps1 -Renut` (the option `LOS_RENUT_RENDERER`), and choose it at boot:
`--sa2_renderer=renut` (or `-Renderer renut` with `tools\win\play.ps1` and `boot_test.ps1`).

## The Direct3D map

Lords of Shadow links an older Direct3D (XDK 2.0.11626) than Mirror of Fate and SA2: every function moved, and the
device's private part has another layout. [d3d-map.md](d3d-map.md) has the 41 functions, the data words and every
device offset, with how each was found. The offsets are in `renut_engine/gpu/d3d/d3d.h`, `d3d_native.h`,
`hooks/device.cpp`, `sa2_renut_pacing.cpp` and `backend/queued.cpp` (each marked "los1-nx").

## The shaders

The game keeps its shaders in its archives, not in its executable: the `bmp/vsh/*.x360.vsb` and `*.x360.psb` entries of
`Data00.packed` (zlib), one container each, most of them at offsets that are not a multiple of 4.
`tools/los_dump_shaders.py` unpacks both discs' archives: 2,006 shaders (1,634 pixel, 372 vertex); disc 2 adds none.
A first dump that kept reNut's alignment test found 512, and the game used 1,494 others at its title screen. Renut
still captures any shader its cache lacks (`out\pc\shader_capture`), and the next build adds it.

## What this game does that Mirror of Fate and SA2 do not

| What | Found | Change |
|---|---|---|
| Its render thread waits after each Swap until the display has shown it (device +16784 catching up with the swap count at +16776), on an event its vertical blank callback sets. Renut's device has no hardware path, so Direct3D's interrupt handler never runs | first Linux boot, 2026-10-26: stopped at the loading screen, its render thread (sub_82591FF8) waiting | The Swap hook counts each frame shown at once; the pacing paces it (`hooks/device.cpp`, `d3d.h`) |
| It asks for the ring buffer's free and used space every frame (sub_824A3150). For a no-hardware device the original takes it from the command buffer being recorded and calls its allocator callback: guest address 0 | second Linux boot: "Call to invalid or unregistered function at guest address 0x00000000" | Answered in `los_renut_glue.cpp`: the sink's room, nothing used |
| Its FVF draws bind a declaration Direct3D builds in the device itself (+0x3050, elements from +0x34), whose contents change from draw to draw | first Linux boot: "Draw dropped: vertex declaration 4003BCD0"; then, taken as no declaration, every frame black (Mark's PC too) | Each different element list gets a permanent copy, registered like the game's own declarations; the draws bind the copy (`los_resource_copies.cpp`) |
| It renders its 3D scenes with predicated tiling: two 1280x384 tiles of the 1280x720 frame | the frame trace: "resolve ... not done (does not fit)" for the lower tile | Its BeginTiling (sub_824AE280) and EndTiling (sub_824AE5F0) tell the render thread the tiles, as N&B's hooks did; the game's own code does the rest (`hooks/draw.cpp`) |
| The clear's viewport clamp read N&B's viewport offset (0x3168) | reading the code | This Direct3D's viewport, +0x3218 (`d3d.h`) |

## Testing without the PC

The Linux build runs on the cloud machine (Xvfb, lavapipe; about 3 frames a second at the title screen). With
`LOS_SHOTS=<folder>,<every>` the presented frame is written every <every> game frames as a PPM file.
`LOS_TRACE_FRAMES=<n>` with `LOS_TRACE_AT=<seconds>` traces n frames (every draw, clear, resolve and target change).
