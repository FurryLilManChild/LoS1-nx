# Starts the PC build, lets it run for a while, then closes it and keeps the log.
# -Renderer renut uses Renut's renderer, -Renderer xenos the emulated GPU (default: what los.toml says).
# -Backend vulkan or d3d12 picks Renut's graphics API (default: D3D12 on Windows).
# Usage (from C:\LoS1-nx): powershell -ExecutionPolicy Bypass -File tools\win\boot_test.ps1 [-Seconds 60]
# The game window opens; you can watch it (and play if it gets that far). It is closed after -Seconds.
# Each run's log: out\logs\boot_<date>_<time>.log, and out\logs\boot_latest.log is always the newest.

param([int]$Seconds = 60, [string]$Renderer = '', [string]$Backend = '')

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $repo 'out\pc\los.exe'
$game = Join-Path $repo 'assets\game_root'
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

if (-not (Test-Path $exe)) { Write-Host 'out\pc\los.exe is missing: run tools\win\build_pc.ps1 first.'; exit 1 }

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$log = Join-Path $logDir "boot_$stamp.log"

$runArgs = @("`"--game_data_root=$game`"", "`"--log_file=$log`"")
if ($Renderer) { $runArgs += "--sa2_renderer=$Renderer" }
if ($Backend) { $runArgs += "--renut_gpu_backend=$Backend" }  # Renut: d3d12 or vulkan
Write-Host "Starting the game for $Seconds seconds..."
$p = Start-Process -FilePath $exe -ArgumentList $runArgs -WorkingDirectory (Split-Path $exe) -PassThru

$exited = $p.WaitForExit($Seconds * 1000)
if ($exited) {
    $result = "The game closed by itself after less than $Seconds s, exit code $($p.ExitCode) (a crash, most likely)."
} else {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    $result = "Still running after $Seconds s: closed it."
}

Add-Content -Path $log -Value "`n[boot_test] $result"
Copy-Item $log (Join-Path $logDir 'boot_latest.log') -Force
Write-Host $result
Write-Host "Log: out\logs\boot_$stamp.log (Claude can read it from the connected folder)."
