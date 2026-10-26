# Builds the Windows PC test build: fills in the SDK's third-party sources (first time), configures, runs the code generator through the
# build, and compiles everything. Every step's full output goes to out\logs\.
#
# Usage (from C:\LoS1-nx): powershell -ExecutionPolicy Bypass -File tools\win\build_pc.ps1 [-Jobs N] [-Clean]
#   -Jobs   parallel compile jobs (default: half the logical CPUs, at most 8; each recompiled file needs ~1-2 GB RAM)
#   -Clean  delete the build folder first
#   -Renut  also build Renut's renderer (LOS_RENUT_RENDERER; first test build)

param(
    [int]$Jobs = 0,
    [switch]$Clean,
    [switch]$Renut
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

# The SDK: sa2-nx's fork of ReXGlue 0.10 (sdk/, tracked in this repository), whose third-party sources come from
# the official release it is based on (tools/fetch_thirdparty.py). An official checkout with its submodules in
# out\sdk (what earlier versions of this script fetched) is used as the source, so nothing is downloaded again.
$sdk = Join-Path $repo 'sdk'
$upstream = Join-Path $repo 'out\sdk'
$build = Join-Path $repo 'out\pc'

function Step($name, $logName, [scriptblock]$body) {
    $log = Join-Path $logDir $logName
    Write-Host ''
    Write-Host "== $name  (log: out\logs\$logName)"
    $start = Get-Date
    # git and cmake write progress to stderr; Windows PowerShell would treat that as an error, so run the step with
    # 'Continue' and turn every line (stdout or stderr) into plain text for the log.
    # The log is plain UTF-8, written as it goes (Windows PowerShell's Tee-Object would write UTF-16).
    $sw = [System.IO.StreamWriter]::new($log, $false, [System.Text.UTF8Encoding]::new($false))
    $sw.AutoFlush = $true
    try {
        & { $ErrorActionPreference = 'Continue'; & $body } 2>&1 | ForEach-Object { "$_" } |
            ForEach-Object -Begin { $n = 0 } -Process {
                $sw.WriteLine($_)
                # Show a heartbeat: every 25th line, plus anything that looks like an error.
                $n++
                if (($n % 25) -eq 0 -or $_ -match 'error|FAILED') { Write-Host "   $_" }
            }
    } finally {
        $sw.Close()
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host ''
        Write-Host "FAILED at: $name. Tell Claude; the log is out\logs\$logName"
        exit 1
    }
    Write-Host ("   done in {0:n0} min" -f ((Get-Date) - $start).TotalMinutes)
}

. (Join-Path $PSScriptRoot 'env.ps1')

if (-not (Test-Path (Join-Path $repo 'assets\game_root\default.xex'))) {
    Write-Host 'assets\game_root\default.xex is missing: extract disc 1 with tools\extract_iso.py first (docs\windows-build.md).'
    exit 1
}

if ($Jobs -le 0) { $Jobs = [math]::Max(1, [math]::Min(8, [int]([Environment]::ProcessorCount / 2))) }

# A build folder made with another SDK location starts over (its object paths name the old SDK).
$sdkMarker = Join-Path $build 'sdk_dir.txt'
if ((Test-Path $build) -and -not $Clean) {
    $prev = if (Test-Path $sdkMarker) { (Get-Content $sdkMarker -Raw).Trim() } else { '' }
    if ($prev -ne $sdk) {
        Write-Host "The build folder was made with another SDK ($prev): starting it over."
        $Clean = $true
    }
}
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

# 1. The SDK's third-party sources (one time). fetch_thirdparty.py also replaces Windows git's symlink stubs.
if (-not (Test-Path (Join-Path $sdk 'thirdparty\fmt\CMakeLists.txt'))) {
    if (Test-Path (Join-Path $upstream 'thirdparty\fmt\CMakeLists.txt')) {
        Step 'Filling in the SDK third-party sources (from out\sdk)' 'sdk_thirdparty.log' {
            python (Join-Path $repo 'tools\fetch_thirdparty.py') --source $upstream
        }
    } else {
        Step 'Fetching the SDK third-party sources (one time, a few minutes)' 'sdk_thirdparty.log' {
            git config --global core.longpaths true
            python (Join-Path $repo 'tools\fetch_thirdparty.py')
        }
    }
}

# 1b. Renut's renderer: its third-party code (plume, XenosRecomp with DXC, uam, ImGui, from sa2-nx; one time) and the
# game's shader containers, which its shader cache is translated from (tools/los_dump_shaders.py: the bmp/vsh entries
# of the .packed archives of both discs).
if (-not (Test-Path (Join-Path $repo 'app\thirdparty\plume\CMakeLists.txt')) -or
    -not (Test-Path (Join-Path $repo 'app\thirdparty\reblue-XenosRecomp\thirdparty\dxc-bin\bin\x64\dxc.exe'))) {
    Step "Fetching the renderer's libraries (one time)" 'renut_deps.log' {
        python (Join-Path $repo 'tools\fetch_renut_deps.py')
    }
}
# Only Renut's renderer needs it: -Renut runs this step.
$shaderDump = Join-Path $repo 'out\renut_shader_dump'
if ($Renut -and -not (Test-Path (Join-Path $shaderDump 'manifest.csv'))) {
    Step "Extracting the game's shaders" 'shader_dump.log' {
        python -m pip install --user --quiet --disable-pip-version-check xxhash
        python (Join-Path $repo 'tools\los_dump_shaders.py') --assets (Join-Path $repo 'assets\game_root') `
            --assets (Join-Path $repo 'assets\game_root_disc2') --out $shaderDump
    }
}

$cfgArgs = @(
    '-S', (Join-Path $repo 'app'), '-B', $build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
    "-DREXSDK_DIR=$sdk",
    '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++',
    '-DCMAKE_C_FLAGS=-march=x86-64-v2', '-DCMAKE_CXX_FLAGS=-march=x86-64-v2',
    '-DREXGLUE_ENABLE_TRACY=OFF',
    "-DLOS_RENUT_RENDERER=$(if ($Renut) { 'ON' } else { 'OFF' })",
    # LoS1-nx (2026-10-26): no early alpha pass for now. Mark's first -Renut build: the translator's second run (the
    # early alpha copies) stopped at 98 % with no message, after the first run had built every shader.
    '-DBANJO_RENUT_EARLY_ALPHA=OFF',
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
)

# 2. Configure.
Step 'Configuring' 'pc_configure.log' { cmake @cfgArgs }
Set-Content -Path $sdkMarker -Value $sdk -Encoding ascii

# 3. The code generator and the translation (the first time also builds the SDK: 15-40 min).
Step 'Building the SDK tools and translating the game code (first time: 15-40 min)' 'pc_codegen.log' {
    cmake --build $build --target los_codegen -- -j $Jobs
}

# 4. Configure again, so the build sees the translated files the code generator just wrote.
Step 'Configuring with the translated code' 'pc_configure2.log' { cmake @cfgArgs }

# 5. Everything else: the 138 translated C++ files and the app (10-30 min).
Step "Compiling ($Jobs jobs at a time)" 'pc_build.log' {
    cmake --build $build -- -j $Jobs
}

$exe = Join-Path $build 'los.exe'
Write-Host ''
if (Test-Path $exe) {
    Write-Host "BUILD OK: $exe"
    Write-Host 'Next: powershell -ExecutionPolicy Bypass -File tools\win\boot_test.ps1'
} else {
    Write-Host 'The build finished but los.exe is not where expected. Tell Claude.'
    exit 1
}
