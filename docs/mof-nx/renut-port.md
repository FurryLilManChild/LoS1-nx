# Renut's renderer in Mirror of Fate HD

Renut's Direct3D-level renderer (reNut by masterspike52, as banjo-nx and sa2-nx carry it) draws the game's Direct3D
calls directly with plume: D3D12 or Vulkan on the PC, deko3d on the Switch. There is no PM4 ring, no Xenos register
file and no EDRAM model. It is what the Switch build uses; the SDK's emulated Xenos GPU stays built in as the
reference picture.

Choose at boot (in `mof.toml`, or on the command line as `--sa2_renderer=renut`):

```toml
sa2_renderer = "renut"   # "xenos" (the default) is the emulated GPU
```

The key keeps sa2-nx's name so its code can be taken over as it is.

## What was taken from sa2-nx (commit adf3a4e, 1.0.3.1)

| Part | Here | How |
| --- | --- | --- |
| Renut's renderer, with sa2-nx's SA2 hook blocks | `app/src/renut_engine` | Tracked; SA2's addresses replaced by this game's (below) |
| plume, reblue-XenosRecomp, uam, Dear ImGui | `app/thirdparty` | Fetched by `tools/fetch_renut_deps.py`, not tracked; DXC from renderbag/dxc-bin (the submodule upstream XenosRecomp uses) |
| The build glue | `app/cmake/mof_renut_gpu.cmake` | From `cmake/sa2_renut_gpu.cmake` |
| The enhancement switches, the console's render mode, the capture interface | `app/src/cpu/sa2_enhancements.*`, `app/src/me/sa2_render_mode.*`, `app/src/sa2_capture.h` | As they are |
| The shader and deko3d tools | `tools/deko3d_*`, `tools/renut_*`, `tools/early_alpha.py` | As they are |

## Retargeting

Every SA2 address in `app/src/renut_engine` was replaced with this game's from [d3d-map.md](d3d-map.md): 38 functions
and 3 data words, in the hook macros, the calls to the originals (`__imp__sub_X`) and the comments. A scan of the
compiled files for any other `0x82xxxxxx` / `sub_82xxxxxx` in code found only N&B's constants, each on a path this
configuration does not run (the same as in sa2-nx's SA2 build):

| Constant | Where | Why it is not reached |
| --- | --- | --- |
| render/sampler state tables, export skips, format table (`d3d_native_state.cpp`) | N&B's native state code | SA2's device creation calls the game's own InitializeApiState and InitializePresentationParameters |
| `kFormatTable`, `kGameNoLargePagesVa`, `kHiZOwnerVa` (`resource.cpp`) | N&B's native resource creation | Only D3DResource_Destroy is hooked here, and its SA2 hook does not call N&B's |
| `kDeviceCapsVa`, `kFusionVa` (`device.cpp`) | N&B's GetDeviceCaps / IsFusion hooks | Not hooked |
| `kFullViewport` (`draw.cpp`) | N&B's tiling | Not hooked |

Game-side pieces in `app/src/mof_renut_glue.cpp`: the window offset/scissor hook (sub_826D7A48; Direct3D keeps no
copy of them in the device, so the hook fills the device's window packet, as sa2-nx does for SA2), the six hooks that
hand the shared Direct3D functions to Renut (the four Draw functions, BeginVertices, AllocateInlineData,
CBlocker::Check: in sa2-nx, sa2-nx's own hooks at those addresses do it), the relaxed vsync switch, and stand-ins for
sa2-nx's frame capture and self-checks.

## mof-nx's changes to the fetched code

`patches/renut-deps/`, applied by `tools/fetch_renut_deps.py`:

- `reblue-XenosRecomp-128-temps.patch`: the translator declared 32 temporary registers; this game's shaders use up
  to r56 (5 of its 104 failed in DXC). All 128 of the Xenos's are declared now.

## Status

- 2026-10-23: builds and links on Linux (all 104 shaders translate and compile).
- 2026-10-23, first boot on Mark's PC (D3D12, Radeon): the game runs to the title screen, but the screen stays black:
  "0 draws" in every report. Renut's side of the shared Direct3D functions was never attached (above). Fixed.
  Also seen: "Draw binds a depth target smaller than its colour target: colour 40064A20 1x8705" (4 surfaces 0x30
  apart, size word 0x11000). To look at once draws arrive.
- 2026-10-23, second boot: draws arrive, the game plays, but the picture flickers. The game's surface objects are
  short-lived (see `app/src/mof_render_targets.cpp`); Renut now uses each surface's descriptor as it was at its bind
  (mof-nx's change in `gpu/render/surfaces.cpp` and `draw.cpp`, marked "mof-nx").
- 2026-10-24: the same for textures, vertex buffers and index buffers (`app/src/mof_resource_copies.cpp`; the
  resolve and Swap hooks in `gpu/d3d/hooks/draw.cpp` and `device.cpp`, marked "mof-nx"). Flicker of whole layers
  on the PC (D3D12 and Vulkan) traced to it.
- 2026-10-24: render target surfaces as stable copies by content as well (the game reuses a surface object for
  targets of other sizes). Mark's playtest after it: the picture is right (no flicker, shadows, HUD fade, tearing).
