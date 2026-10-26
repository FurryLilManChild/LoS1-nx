# Direct3D map: Mirror of Fate HD to Lords of Shadow

Renut's renderer came over from mof-nx with Mirror of Fate's Direct3D addresses and SA2's device layout (Mirror of
Fate's Direct3D, XDK 2.0.21256, has SA2's layout). Lords of Shadow links an older Direct3D (XDK 2.0.11626), so both
the function addresses and the device's private layout differ. Found on 2026-10-26.

## Functions

`tools/d3d_match.py` (sa2-nx) with Mirror of Fate's loaded image and generated tree as the reference and the 42
functions mof-nx names (re-derived from docs/mof-nx/d3d-map.md and the render target hooks), against this game's. The
score compares instruction shapes (opcodes and registers, immediates left out); 1.00 is identical. "Next" is the best
other candidate.

| Function | Mirror of Fate | Lords of Shadow | Score | Next best | Also checked |
|---|---|---|---|---|---|
| AllocateInlineData | 826F8AA8 | **824A3D30** | 1.00 | 0.42 | |
| BeginRingBig | 826FA050 | **824A5130** | 1.00 | 0.45 | |
| CBlocker::Check | 826F2B60 | **8249A408** | 0.93 | 0.27 | called by BlockOnFence and Swap, as in MoF |
| CDevice::BlockOnFence | 826F8748 | **824A3E18** | 1.00 | 0.42 | |
| CDevice::KickOff | 826F9DA8 | **824A4EF8** | 1.00 | 0.43 | |
| CDevice::KickOffSegment | 826F9A20 | **824A4D70** | 1.00 | 0.22 | |
| CreateDevice hardware path | 8270EAB8 | **824ADA68** | 0.96 | 0.23 | |
| CreateDevice part (side buffer) | 8270E520 | **824AD9F8** | 1.00 | 0.58 | |
| CreateDevice part 2 | 8270E590 | **824ADCD0** | 1.00 | 0.84 | |
| D3DDevice_BeginVertices | 826DDBF0 | **82900FB0** | 0.96 | 0.70 | |
| D3DDevice_Clear | 826DD4A0 | **8249A238** | 1.00 | 0.37 | |
| D3DDevice_ClearF | 826DD368 | **8249A110** | 0.97 | 0.41 | |
| D3DDevice_CreatePixelShader | 826E9AB8 | **8249E2A8** | 1.00 | 0.63 | |
| D3DDevice_CreateVertexDeclaration | 826E70C0 | **8249CD90** | 1.00 | 0.59 | |
| D3DDevice_CreateVertexShader | 826E9D28 | **8249E490** | 1.00 | 0.63 | |
| D3DDevice_DrawIndexedVertices | 826DEAF0 | **82901880** | 0.95 | 0.67 | |
| D3DDevice_DrawVertices | 826DE6D8 | **82901490** | 0.95 | 0.71 | |
| D3DDevice_DrawVerticesUP | 826DE0B0 | **82901448** | 1.00 | 0.47 | |
| D3DDevice_Resolve | 826EFAA0 | **824AC870** | 1.00 | 0.46 | the same stack arguments (+92, +100) |
| D3DDevice_Swap | 826E11F0 | **824AF2F8** | 0.84 | 0.11 | the only caller of VdSwap |
| D3DResource_Destroy | 826E2F68 | **824A26A0** | 0.81 | 0.25 | an older, shorter version; calls BlockOnFence and GameXMemFree |
| Direct3D_CreateDevice | 826F36D0 | **8249AB48** | 1.00 | 0.42 | |
| GameXMemFree | 82447D48 | **824B9E48** | | | what D3DResource_Destroy calls, 3 times as in MoF |
| InitializeApiState | 8270DE68 | **824AD2F8** | 1.00 | 0.17 | |
| InitializePresentationParameters | 8270E838 | **824AD560** | 0.95 | 0.23 | |
| InsertCallback | 826FA298 | **824A5220** | 1.00 | 0.65 | |
| IsFencePending | 826F9F50 | **824A5038** | 1.00 | 0.71 | |
| RingBufferDeviceAllocate | 826F83B8 | **824A3B00** | 1.00 | 0.18 | |
| ring segment allocator | 826F8B90 | **824A4220** | 0.86 | 0.29 | called by KickOffSegment, BeginRingBig and Swap, as in MoF |
| SetDepthStencilSurface | 826DB338 | **824986D8** | 1.00 | 0.44 | |
| SetGammaRamp (and its twin) | 826DA7C0 | **82497E28** | 1.00 | 1.00 | the twins differ only in immediates; Renut takes only the offset |
| SetRenderTarget | 826DAFA8 | **82498388** | 0.95 | 0.43 | |
| SetRingBufferParameters | 826FA498 | **824A5278** | 1.00 | 0.14 | |
| the four-target setter | 826DB610 | **824989B0** | 0.89 | 0.15 | |
| SynchronizeToPresentationInterval | 826E0D30 | **824AEEC8** | 0.97 | 0.30 | |
| XGRegisterPixelShader | 826E6B18 | **8249C8C0** | 1.00 | 0.44 | |
| XGRegisterVertexShader | 826E9C30 | **8249E398** | 1.00 | 0.31 | |
| draw-time shader load | 827037B0 | **824AA428** | 0.88 | 0.10 | called by the three draws |
| is resource bound | 826E25A0 | **824A1D68** | 1.00 | 0.25 | |
| side buffer out of memory | 826F7C10 | **824A3A88** | | | called by both CreateDevice and CreateDevice part 2, as in MoF |
| window offset/scissor writer | 826D7A48 | **82495420** | 0.94 | 0.25 | |

The draw functions sit far from the rest of Direct3D here (0x829xxxxx, each called from one wrapper of the game's).

## Data addresses

| Constant (Renut, d3d/hooks/device.cpp) | Mirror of Fate | Lords of Shadow | From |
|---|---|---|---|
| kGlobalQwordVa | 0x8298AA68 | **0x82CC1E58** | CreateDevice part 824AD9F8 (`ld r11,7768(r11)` after `lis r11,-32052`) |
| kVdGlobalDeviceImportSlot | 0x82000884 | **0x8200085C** | hardware path 824ADA68; the image word there is xboxkrnl ordinal 0x1BE (VdGlobalDevice) in both games |
| the game's device holder | 0x82000888 | **0x82000858** | the same; ordinal 0x1BF |

## Device layout

The public device (the dirty masks, ring pointers, call tables, constants and the register packets, to +0x2A5F) is
the same. The private part moved: by -0x80 from +0x2B00, by -0xC8 for the bound objects (+0x3200 on), and by -0xC0
from +0x3500 on. The device is 0x5F00 bytes (0x6080 in SA2 and MoF). Each offset Renut uses was voted on by pairing
the aligned instructions of the matched functions (MoF to LoS, and SA2 to LoS over 366 matched Direct3D functions):

| Field | SA2 / MoF | Lords of Shadow | Votes (MoF pairs) |
|---|---|---|---|
| global qword | 11008 | 10880 (+0x2A80) | 1/1 |
| creation thread, title process | 11016, 11020 | 10888, 10892 | 2/2, 1/1 |
| completed-fence word | 11024 | 10896 | 18/18 |
| the swap's scratch word | 11028 | 10900 | 4/4 |
| fence tag | 11036 | 10908 | 15/15 |
| swap fence tag | 11056 | 10928 | 1/1 |
| flags bytes | 11068-11071 | 10940-10943 | 27, 14, 25, 20 |
| vertex declaration | 12240 | 11992 (+0x2ED8) | 1/1 (and SA2's 2/3) |
| index buffer | 12812 | 12612 (+0x3144) | 2/2 |
| render targets, depth | 12816, 12832 | 12616, 12632 | 12/12, 15/15 |
| streams, strides | 12836, 12904 | 12636, 12704 | 1/1, 3/3 |
| textures | 12920 | 12720 (+0x31B0) | 1/1 (26 slots: the viewport follows at +0x3218, as it does in MoF) |
| viewport, scissor rect | 13024, 13052 | 12824, 12852 | SA2's 5/5 |
| pixel, vertex shader | 13068, 13072 | 12868, 12872 | 2/2, 3/3 |
| predication | 13084 | 12884 | 3/3 (snapshot size 12888) |
| in KickOff | 13600 | 13408 | 7/7 |
| inline ring end | 13844 | 13652 | 2/2 |
| command buffer | 13920 | 13728 | 3/3 |
| presentation interval | 14000 | 13808 | 2/2 |
| present fence tag | 15344 | 15152 | 4/4 |
| critical sections | 15348, 15376 | 15156, 15184 | 1/1, 1/1 |
| gamma ramp | 15408 | 15216 | 3/3 |
| swap count | 16968 | 16776 | 2/3 |
| side buffer | 17136 | 16944 | 1/1 |
| words set to -1 | 21968, 21972, 15340 | 21776, 21780, 15148 | 2/2, 2/2, 4/4 |
| timer frequency | 21996 | 21804 | 1/1 |
| behavior flags | 24392 | 24200 | 4/4 |
| the device's own vertex declaration | 12568 | 12368 (+0x3050) | InitializeApiState writes its header (5, 1) at both |

The anisotropy and Z filter arrays (SA2 +0x2A70, +0x2A8A) moved to +0x2F44 and +0x2F92; Renut does not read them.
