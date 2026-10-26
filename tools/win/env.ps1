# Sets up the build environment in the current PowerShell session: the Visual Studio C++ libraries and Windows SDK
# (through vcvars64.bat), then LLVM clang 20, CMake and Ninja first on the PATH.
# Dot-source it: . tools\win\env.ps1
# Overrides: LOS_LLVM (folder holding bin\clang.exe), LOS_VSDIR (Visual Studio or Build Tools folder).
#
# Only this PowerShell session is changed. Nothing is changed in the system's settings.

$ErrorActionPreference = 'Stop'

# Unix-style tool folders (devkitPro's MSYS2, MSYS2, Cygwin) carry their own cmd, cmake, git and make. A PC build
# that picks those up fails, and a CMake configure through them wipes the build folder's cache (sa2-nx's
# building.md). They are dropped from this session's PATH; the Switch build will set up devkitPro on its own.
$script:MofUnixPathPattern = '\\(devkitPro\\msys2|msys64|msys2|cygwin(64)?)(\\|$)'

function Find-VsDir {
    if ($env:LOS_VSDIR) { return $env:LOS_VSDIR }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found: is the Visual Studio 2022 Build Tools install finished?" }
    $dir = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $dir) { throw "No Visual Studio install with the C++ tools (VCTools workload) was found." }
    return $dir
}

function Find-Llvm {
    $candidates = @()
    if ($env:LOS_LLVM) { $candidates += $env:LOS_LLVM }
    $candidates += (Join-Path $env:ProgramFiles 'LLVM')
    if (${env:ProgramFiles(x86)}) { $candidates += (Join-Path ${env:ProgramFiles(x86)} 'LLVM') }
    $candidates += (Join-Path $env:LOCALAPPDATA 'Programs\LLVM')
    foreach ($c in $candidates) {
        if (Test-Path (Join-Path $c 'bin\clang.exe')) { return $c }
    }
    throw ("clang.exe not found. Looked in: " + ($candidates -join ', ') +
        ". Install it with: winget install --id LLVM.LLVM -e --version 20.1.8")
}

if (-not $env:LOS_ENV_READY) {
    $removed = @($env:PATH -split ';' | Where-Object { $_ -and ($_ -match $script:MofUnixPathPattern) })
    $env:PATH = (@($env:PATH -split ';' | Where-Object { $_ -and ($_ -notmatch $script:MofUnixPathPattern) }) -join ';')
    if ($removed.Count -gt 0) { Write-Host ("Left out of this build session's PATH: " + ($removed -join ', ')) }

    $vsDir = Find-VsDir
    $vcvars = Join-Path $vsDir 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsDir" }
    # Run vcvars64 in Windows' own cmd.exe (by full path) and copy the environment it sets into this session.
    $cmdExe = if ($env:ComSpec) { $env:ComSpec } else { Join-Path $env:SystemRoot 'System32\cmd.exe' }
    & $cmdExe /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
    }
    if (-not $env:VCToolsInstallDir) { throw "vcvars64.bat ran but did not set up the C++ tools ($vcvars)" }

    $llvm = Find-Llvm
    $extra = @((Join-Path $llvm 'bin'))
    foreach ($p in @((Join-Path $env:ProgramFiles 'CMake\bin'),
                     (Join-Path $env:ProgramFiles 'Git\cmd'),
                     (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links'))) {
        if (Test-Path $p) { $extra += $p }
    }
    # winget's Ninja package may not create a link: add its own folder if ninja is still not found.
    $ninjaPkg = Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages') -Directory -Filter 'Ninja-build.Ninja*' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($ninjaPkg) { $extra += $ninjaPkg.FullName }
    $env:PATH = ($extra -join ';') + ';' + $env:PATH

    # Git: if only the MSYS2 copy exists, call it by full path (its folder stays off the PATH).
    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        $msysGit = $removed | ForEach-Object { Join-Path $_ 'git.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($msysGit) {
            $global:MofGit = $msysGit
            function global:git { & $global:MofGit @args }
        }
    }
    $env:LOS_ENV_READY = '1'
}
