# Starts the PC build to play: sound on, no time limit. Close the game window (or press Alt+F4) when done.
# The log is kept like the boot test's: out\logs\play_<date>_<time>.log, and out\logs\play_latest.log.
# -Renderer renut uses Renut's renderer, -Renderer xenos the emulated GPU (default: what los.toml says).
# Usage (from C:\LoS1-nx): powershell -ExecutionPolicy Bypass -File tools\win\play.ps1

param([string]$Renderer = '')

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $repo 'out\pc\los.exe'
$game = Join-Path $repo 'assets\game_root'
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

if (-not (Test-Path $exe)) { Write-Host 'out\pc\los.exe is missing: run tools\win\build_pc.ps1 first.'; exit 1 }

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$log = Join-Path $logDir "play_$stamp.log"
$runArgs = @("`"--game_data_root=$game`"", "`"--log_file=$log`"", '--audio_mute=false')
if ($Renderer) { $runArgs += "--sa2_renderer=$Renderer" }

Write-Host 'Starting the game with sound. Close its window when you are done.'
$start = Get-Date
$p = Start-Process -FilePath $exe -ArgumentList $runArgs -WorkingDirectory (Split-Path $exe) -PassThru
$p.WaitForExit()
$minutes = [math]::Round(((Get-Date) - $start).TotalMinutes, 1)

$result = "Played for $minutes min; exit code $($p.ExitCode)."
Add-Content -Path $log -Value "`n[play] $result"
Copy-Item $log (Join-Path $logDir 'play_latest.log') -Force
Write-Host $result
Write-Host "Log: out\logs\play_$stamp.log (Claude can read it from the connected folder)."
