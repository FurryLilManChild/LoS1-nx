# Runs the game for 60 s with Renut's renderer and logs 150 frames of the title screen: the draws the game issued
# in each frame (LOS_FRAME_DIAG), and what the renderer did with each (LOS_TRACE_FRAMES: every draw with its
# textures, every resolve, clear and change of target). Claude reads out\logs\frame_diag.log. No playing needed.
# Usage (from C:\LoS1-nx): powershell -ExecutionPolicy Bypass -File tools\win\frame_diag.ps1

param([int]$StartSeconds = 40, [int]$Seconds = 60)

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $repo 'out\pc\los.exe'
$game = Join-Path $repo 'assets\game_root'
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
if (-not (Test-Path $exe)) { Write-Host 'out\pc\los.exe is missing: run tools\win\build_pc.ps1 first.'; exit 1 }

$log = Join-Path $logDir 'frame_diag.log'
Remove-Item $log -ErrorAction SilentlyContinue
$env:LOS_FRAME_DIAG = "$StartSeconds"
$env:LOS_TRACE_FRAMES = '150'
$env:LOS_TRACE_AT = "$StartSeconds"
$runArgs = @("`"--game_data_root=$game`"", "`"--log_file=$log`"", '--sa2_renderer=renut')
Write-Host "Starting the game for $Seconds seconds (leave it on the title screen)..."
$p = Start-Process -FilePath $exe -ArgumentList $runArgs -WorkingDirectory (Split-Path $exe) -PassThru
if (-not $p.WaitForExit($Seconds * 1000)) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
Remove-Item Env:\LOS_FRAME_DIAG
Remove-Item Env:\LOS_TRACE_FRAMES
Remove-Item Env:\LOS_TRACE_AT
Write-Host 'Done. Log: out\logs\frame_diag.log (Claude can read it from the connected folder).'
