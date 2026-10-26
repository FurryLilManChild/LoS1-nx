# Known issues

PC build with the SDK's emulated Xenos GPU (D3D12), as of 2026-10-23.

With Renut's renderer (`-Renderer renut`, what the Switch uses), as of 2026-10-24: none of the picture issues
below; Mark's playtest found the picture right.

| Issue | Where | Status |
| --- | --- | --- |
| The scene goes black while Simon's health/magic HUD fades in or out (entering and leaving combat), and comes back when the fade ends | Chapter 1 onward | Emulated GPU path. Revisit on the native renderer, which the Switch uses; not fixed in the emulated GPU |
| Screen tearing | Everywhere | The SDK 0.10 presenter has no vsync setting. The native renderer has its own presentation (sa2-nx: relaxed vsync) |
| Occasional small stutter | Everywhere | Likely first-time shader/pipeline compiles in the emulated GPU |
| Exit code 0xC0000374 (heap corruption) after quitting from the game's "Exit Game" menu | Shutdown | The game itself shuts down cleanly ("[XAPI RETURN VALUE] 0", "Execution complete"); the error comes from the SDK's teardown afterwards. Low priority. On the Switch, "Exit Game" should return to the HOME menu |

Not issues (expected failed file opens in the log):

- `game:\system.pkg`, `game:\packs\cutscenes\prologue01.pkg` and similar: the game looks for a packed version
  first, then loads the loose folders (`cutscenes\prologue01\...`), which the package has.
- `game:\gui\scripts\sprites_ps3controls*.lc`: PS3 button sprites; the 360 package does not ship them.
- `save:\profile0\`: no save folder yet on the first run.

Tested and working (Mark, 2026-10-23): title screen, menus, controls (Legion Go pad), sound, the whole prologue,
Chapter 1 up to just past the first save font. Performance on the Legion Go: playable, minor stutter.
