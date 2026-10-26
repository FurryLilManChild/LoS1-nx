# Building and testing on Windows

## One-time setup

In PowerShell (one line at a time):

```
winget install --id Git.Git -e
winget install --id Git.GCM -e
winget install --id Python.Python.3.13 -e
winget install --id LLVM.LLVM -e --version 20.1.8
winget install --id Kitware.CMake -e
winget install --id Ninja-build.Ninja -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.CMake.Project --add Microsoft.VisualStudio.Component.Windows11SDK.22621"
```

Close PowerShell and open a new one, then:

```
git config --global credential.helper manager
git clone https://github.com/FurryLilManChild/LoS1-nx.git C:\LoS1-nx
cd C:\LoS1-nx
python tools\extract_iso.py "assets\Castlevania - Lords of Shadow (USA, Europe) (En,Fr,De,Es,It) (Disc 1).iso" -o assets\game_root
python tools\extract_iso.py "assets\Castlevania - Lords of Shadow (USA, Europe) (En,Fr,De,Es,It) (Disc 2).iso" -o assets\game_root_disc2
```

Note: devkitPro's MSYS2 tools (`c:\devkitPro\msys2\usr\bin`) on the PATH shadow Windows' cmd, cmake and git. The
build scripts leave them out of their own session; nothing needs changing in the system settings.

Note: a Git that was installed earlier without Git Credential Manager asks for a username in the terminal and
seems to hang. Installing `Git.GCM` fixes it.

## Every build

From `C:\LoS1-nx`:

| Command | What it does | Log |
| --- | --- | --- |
| `git pull` | Get the latest changes | |
| `powershell -ExecutionPolicy Bypass -File tools\win\check_tools.ps1` | Checks every tool, the game files and disk space | `out\logs\check_tools.log` |
| `powershell -ExecutionPolicy Bypass -File tools\win\build_pc.ps1` | Fetches the SDK (first time), translates, compiles | `out\logs\pc_*.log` |
| `powershell -ExecutionPolicy Bypass -File tools\win\boot_test.ps1` | Runs the game for 60 s, muted, and keeps the log | `out\logs\boot_latest.log` |
| `powershell -ExecutionPolicy Bypass -File tools\win\play.ps1` | Runs the game to play: sound on, no time limit | `out\logs\play_latest.log` |
| `powershell -ExecutionPolicy Bypass -File tools\win\package_pc.ps1 -Zip` | A portable copy to keep and play: `out\package\`, with your game files (do not share it) | |

The first build takes the longest: the SDK and its tools (15-40 min), then the 138 translated files (10-30 min,
depending on the PC). Later builds only redo what changed.

`build_pc.ps1 -Jobs 4` uses fewer parallel compile jobs if the PC runs out of memory.
`build_pc.ps1 -Clean` starts from an empty build folder.

Settings for the PC build: `out\pc\mof.toml` (copied from `app\mof.toml` the first time; later edits are kept).
