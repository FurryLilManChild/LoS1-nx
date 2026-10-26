# Direct3D map: Sonic Adventure 2 to Mirror of Fate HD

Both games link the XDK's Direct3D (SA2 2.0.21076, Mirror of Fate 2.0.21256). Found on 2026-10-23 with
`tools/d3d_match.py` (sa2-nx): SA2's XBLA default.xex (0.0.0.3, the edition sa2-nx is built from; Mark's own copy,
used only as a reference and never stored here), translated with sa2-nx's own manifest and overrides, against this
game's generated tree. The score compares instruction shapes (opcodes and registers, immediates left out); 1.00 is
identical. "Next" is the best other candidate.

| Function | SA2 | Mirror of Fate | Score | Next best | Renut hooks it |
|---|---|---|---|---|---|
| is resource bound | 825498C8 | **826E25A0** | 1.00 | 82410E80 0.23 |  |
| D3DResource_Destroy | 8254A200 | **826E2F68** | 1.00 | 8212D558 0.32 | yes |
| side buffer out of memory | 8254D040 | **826F7C10** | 1.00 | 82654E18 0.44 |  |
| RingBufferDeviceAllocate | 8254D4B8 | **826F83B8** | 1.00 | 82855AB0 0.15 | yes |
| CDevice_BlockOnFence | 8254D6E8 | **826F8748** | 1.00 | 826F8610 0.57 | yes |
| AllocateInlineData | 8254D9A8 | **826F8AA8** | 1.00 | 8244F558 0.34 | yes |
| RingSegmentAllocate (Sa2NewSegment) | 8254DA90 | **826F8B90** | 1.00 | 826D5168 0.26 | yes |
| CDevice_KickOffSegment | 8254E830 | **826F9A20** | 1.00 | 8274C9B0 0.22 | yes |
| CDevice_KickOff | 8254EBB8 | **826F9DA8** | 1.00 | 82566E78 0.45 | yes |
| IsFencePending | 8254ECF8 | **826F9F50** | 1.00 | 826F8610 0.68 | yes |
| BeginRingBig | 8254EDF8 | **826FA050** | 1.00 | 826EC090 0.45 | yes |
| InsertCallback | 8254EEE8 | **826FA298** | 1.00 | 82832DF0 0.65 | yes |
| SetRingBufferParameters | 8254EF40 | **826FA498** | 1.00 | 8271F1E8 0.15 | yes |
| CBlocker_Check | 8254F400 | **826F2B60** | 1.00 | 823F7700 0.25 | yes |
| Direct3D_CreateDevice | 8254FC10 | **826F36D0** | 1.00 | 8271A1F8 0.43 | yes |
| SetGammaRamp (device +15408) | 825527D8 | **826DA7C0** | 1.00 | 826DA6D8 1.00 |  |
| draw-time shader load | 82556A60 | **827037B0** | 1.00 | 826DC748 0.11 |  |
| SynchronizeToPresentationInterval | 825589D8 | **826E0D30** | 1.00 | 8270C970 0.27 | yes |
| D3DDevice_Swap | 82558E98 | **826E11F0** | 1.00 | 82338378 0.13 | yes |
| InitializeApiState | 8255C150 | **8270DE68** | 1.00 | 826E2C78 0.17 |  |
| CreateDevice part (side buffer) | 8255C808 | **8270E520** | 1.00 | 8249A8C8 0.61 |  |
| CreateDevice part 2 | 8255C878 | **8270E590** | 1.00 | 828F72B0 0.83 |  |
| InitializePresentationParameters | 8255CB20 | **8270E838** | 1.00 | 827E9598 0.21 |  |
| CreateDevice hardware path | 8255CDA0 | **8270EAB8** | 1.00 | 8270E660 0.35 |  |
| XGRegisterPixelShader | 8255DBF8 | **826E6B18** | 1.00 | 8274A1F0 0.41 | yes |
| D3DDevice_CreateVertexDeclaration | 8255E198 | **826E70C0** | 1.00 | 826EBEE0 0.65 | yes |
| D3DDevice_CreatePixelShader | 8255E568 | **826E9AB8** | 1.00 | 826E9D28 0.63 |  |
| XGRegisterVertexShader | 8255E658 | **826E9C30** | 1.00 | 82548BB0 0.31 | yes |
| D3DDevice_CreateVertexShader | 8255E750 | **826E9D28** | 1.00 | 826E9AB8 0.63 |  |
| D3DDevice_ClearF | 82563978 | **826DD368** | 1.00 | 826D7688 0.59 | yes |
| D3DDevice_Clear | 82563AB0 | **826DD4A0** | 1.00 | 828C42D0 0.37 |  |
| D3DDevice_Resolve | 825656F8 | **826EFAA0** | 1.00 | 826014D0 0.50 | yes |
| D3DDevice_BeginVertices | 82565758 | **826DDBF0** | 1.00 | 826DE6D8 0.75 | yes |
| D3DDevice_DrawVerticesUP | 82565C18 | **826DE0B0** | 1.00 | 828F2E18 0.47 | yes |
| D3DDevice_DrawVertices | 82565C60 | **826DE6D8** | 1.00 | 826DEF68 0.85 | yes |
| D3DDevice_DrawIndexedVertices | 82566078 | **826DEAF0** | 1.00 | 826DE6D8 0.73 | yes |

Notes:

- SetGammaRamp has an identical twin in both games (SA2 82552708, Mirror of Fate 826DA6D8): both write the ramp at
  device +15408. Renut only takes that offset from it, so the pair does not need to be told apart.
- `D3DDevice_Swap` 826E11F0 is also the only caller of `VdSwap`, an independent check.

## Found later

| Function | SA2 | Mirror of Fate | How |
|---|---|---|---|
| Window offset/scissor writer (registers 0x2080-0x2082; sa2-nx hooks it in `me/sa2_me_hooks.cpp` to fill the device's window packet for Renut, since Direct3D keeps no copy at +0x28C0) | 8254FCE8 | **826D7A48** | d3d_match 1.00 (next 0.32). Mirror of Fate's version also stores nothing at +0x28C0-0x28C8, so it needs the same hook |
| GameXMemFree (what D3DResource_Destroy calls; Renut's `resource.cpp` imports it) | 828074B8 | **82447D48** | the same call in the matched D3DResource_Destroy, at all 3 call sites (instructions 61, 64, 163). The same pairing puts BlockOnFence's calls on 826F8748, the matcher's BlockOnFence |

## Data addresses

Read by pairing the instructions of matched functions (same shape, so the same instruction builds the same
object in both games):

| Constant (Renut, d3d/hooks/device.cpp) | SA2 | Mirror of Fate | From |
|---|---|---|---|
| kGlobalQwordVa | 0x82980268 | **0x8298AA68** | CreateDevice part 8255C808 / 8270E520 |
| kVdGlobalDeviceImportSlot | 0x82000710 | **0x82000884** | hardware path 8255CDA0 / 8270EAB8 |
| the game's device holder | 0x8200070C | **0x82000888** | hardware path 8255CDA0 / 8270EAB8 |

Not needed: `kFullViewport` (N&B's data, used only by N&B's tiling code), the texture format and anisotropy tables
(their contents are copied into Renut's source).

## Device layout

Every SA2 device offset Renut uses appears in this game's Direct3D code (0x826D0000-0x82730000), and the
shifted values do not: the fence tag (11036) 66 times, the swap flags (11069) 63, the depth-stencil slot (12832) 41,
predication (13084) 36, against 0-3 for the same offsets shifted by N&B's -128, -376 or -400. So the layout is
taken to be SA2's; each offset is still to be checked in the matched function it is read from.
