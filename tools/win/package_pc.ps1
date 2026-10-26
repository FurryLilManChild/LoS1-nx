# Makes a portable copy of the PC build: one folder (and, with -Zip, a .zip of it) that runs on its own, without the
# build tools or this repository. Double-click los.exe inside it to play.
#   out\package\Castlevania Lords of Shadow (PC)\
#     los.exe and its two DLLs, pipeline_list.bin, los.toml (native renderer, sound on), game_root\ (your game files)
# The game files are your own copy: keep the package for yourself, do not share it.
# Saves are not in the package: they are in Documents\mof (shared by every copy on this PC).
# Usage (from C:\LoS1-nx): powershell -ExecutionPolicy Bypass -File tools\win\package_pc.ps1 [-Zip]

param([switch]$Zip)

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$build = Join-Path $repo 'out\pc'
$game = Join-Path $repo 'assets\game_root'
$name = 'Castlevania Lords of Shadow (PC)'
$dest = Join-Path $repo "out\package\$name"

foreach ($f in @('los.exe', 'rexruntimerd.dll', 'rexgpu-xenosrd.dll', 'pipeline_list.bin')) {
    if (-not (Test-Path (Join-Path $build $f))) { Write-Host "out\pc\$f is missing: run tools\win\build_pc.ps1 first."; exit 1 }
}
if (-not (Test-Path (Join-Path $game 'default.xex'))) { Write-Host 'assets\game_root\default.xex is missing.'; exit 1 }

Write-Host "Making $dest ..."
New-Item -ItemType Directory -Force -Path $dest | Out-Null
foreach ($f in @('los.exe', 'rexruntimerd.dll', 'rexgpu-xenosrd.dll', 'pipeline_list.bin')) {
    Copy-Item (Join-Path $build $f) $dest -Force
}
$cache = Join-Path $build 'cache'
if (Test-Path $cache) { Copy-Item $cache $dest -Recurse -Force }  # compiled pipelines: a smoother first run

@'
# Castlevania: Lords of Shadow, native PC build (LoS1-nx). Edit with Notepad if you like.
license_mask = 1
gpu_plugin = "xenos"
# The native renderer ("xenos" would be the slower emulated GPU).
sa2_renderer = "renut"
log_level = "info"
audio_mute = false
'@ | Set-Content -Path (Join-Path $dest 'los.toml') -Encoding ASCII

@'
Castlevania: Lords of Shadow, native PC build (LoS1-nx)

Play: double-click los.exe. Alt+F4 or the game's "Exit Game" closes it.
Controller: any XInput controller (Xbox style). Keyboard play: start los.exe with --mnk_mode=true.
Saves: Documents\mof (shared by every copy on this PC).
Logs: the logs folder next to los.exe.
Another PC needs the Microsoft Visual C++ 2015-2022 Redistributable (x64), and a GPU with D3D12.
The game_root folder holds your own copy of the game: do not share this package.
'@ | Set-Content -Path (Join-Path $dest 'README.txt') -Encoding ASCII

Write-Host 'Copying the game files (about 2 GB)...'
robocopy $game (Join-Path $dest 'game_root') /E /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { Write-Host "Copying the game files failed (robocopy code $LASTEXITCODE)."; exit 1 }

if ($Zip) {
    $zipPath = Join-Path $repo "out\package\$name.zip"
    Remove-Item $zipPath -ErrorAction SilentlyContinue
    Write-Host 'Making the .zip (a few minutes)...'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory($dest, $zipPath, [System.IO.Compression.CompressionLevel]::Fastest, $true)
    Write-Host "Zip: $zipPath"
}
Write-Host "Done: $dest"
