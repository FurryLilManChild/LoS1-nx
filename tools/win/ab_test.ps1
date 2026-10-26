# Runs the game several times with Renut's renderer, each time with one suspect feature switched off, and takes
# 20 screenshots at the title screen each time (out\ab\<variant>\NN.jpg). Claude compares them to find which feature
# makes parts of the picture flicker. No playing needed: leave the PC alone while it runs (about 10 minutes), and
# do not cover the game window.
# Usage (from C:\LoS1-nx): powershell -ExecutionPolicy Bypass -File tools\win\ab_test.ps1

param([int]$WaitSeconds = 45)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $repo 'out\pc\los.exe'
$game = Join-Path $repo 'assets\game_root'
$outRoot = Join-Path $repo 'out\ab'
if (-not (Test-Path $exe)) { Write-Host 'out\pc\los.exe is missing: run tools\win\build_pc.ps1 first.'; exit 1 }

$variants = [ordered]@{
    'a_base'          = @()
    'b_vulkan'        = @('--renut_gpu_backend=vulkan')
    'c_resolve_skip'  = @('--banjo_renut_resolve_skip=false')
    'd_deltas'        = @('--sa2_renut_snapshot_deltas=false')
    'e_record_worker' = @('--banjo_renut_record_worker=false')
    'f_exact_streams' = @('--banjo_renut_exact_streams=false')
    'g_shared_dedupe' = @('--banjo_renut_shared_dedupe=false')
    'h_noncpu_enh'    = @('--sa2_noncpu_enhancements=false')
    'i_cpu_enh'       = @('--sa2_cpu_enhancements=false')
}

function Shot([string]$path) {
    $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Jpeg)
    $g.Dispose(); $bmp.Dispose()
}

$n = 0
foreach ($name in $variants.Keys) {
    $n++
    $dir = Join-Path $outRoot $name
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    Write-Host "[$n of $($variants.Count)] $name ..."
    $log = Join-Path $dir 'game.log'
    $runArgs = @("`"--game_data_root=$game`"", "`"--log_file=$log`"", '--sa2_renderer=renut') + $variants[$name]
    $p = Start-Process -FilePath $exe -ArgumentList $runArgs -WorkingDirectory (Split-Path $exe) -PassThru
    Start-Sleep -Seconds $WaitSeconds
    if ($p.HasExited) {
        Write-Host "  the game closed by itself (exit code $($p.ExitCode))"
        continue
    }
    for ($i = 1; $i -le 20; $i++) {
        Shot (Join-Path $dir ('{0:D2}.jpg' -f $i))
        Start-Sleep -Milliseconds 70
    }
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 3
}
Write-Host 'Done. The screenshots are in out\ab (Claude can read them from the connected folder).'
