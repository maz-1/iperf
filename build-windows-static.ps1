param(
    [int]$Jobs = [Environment]::ProcessorCount,
    [switch]$NoStrip
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = $PSScriptRoot
$ToolchainVersion = '2.10.0'
$ToolchainDir = Join-Path $Root '.toolchain\w64devkit'
$ToolchainRoot = Join-Path $ToolchainDir 'w64devkit'
$Bin = Join-Path $ToolchainRoot 'bin'
$Gcc = Join-Path $Bin 'gcc.exe'
$Archive = Join-Path $Root ".toolchain\w64devkit-x64-$ToolchainVersion.7z.exe"
$DownloadUrl = "https://github.com/skeeto/w64devkit/releases/download/v$ToolchainVersion/w64devkit-x64-$ToolchainVersion.7z.exe"
$TmpDir = Join-Path $Root '.tmp'

function Assert-LastExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE"
    }
}

New-Item -ItemType Directory -Force (Join-Path $Root '.toolchain') | Out-Null
New-Item -ItemType Directory -Force $TmpDir | Out-Null

if (-not (Test-Path $Gcc)) {
    if (-not (Test-Path $Archive)) {
        Write-Host "Downloading w64devkit $ToolchainVersion..."
        & curl.exe -L --fail --retry 3 -o $Archive $DownloadUrl
        Assert-LastExitCode 'w64devkit download'
    }

    Write-Host 'Extracting w64devkit...'
    New-Item -ItemType Directory -Force $ToolchainDir | Out-Null
    & $Archive -y "-o$ToolchainDir"
    Assert-LastExitCode 'w64devkit extraction'
}

if (-not (Test-Path $Gcc)) {
    throw "GCC was not found after toolchain setup: $Gcc"
}

$BinUnix = $Bin.Replace('\', '/')
$CompatInclude = (Join-Path $Root 'src\win32\include')
$CompatIncludeUnix = $CompatInclude.Replace('\', '/')

$env:PATH = "$Bin;$env:PATH"
$env:CC = "$BinUnix/gcc.exe"
$env:AR = "$BinUnix/ar.exe"
$env:RANLIB = "$BinUnix/ranlib.exe"
$env:STRIP = "$BinUnix/strip.exe"
$env:SED = "$BinUnix/sed.exe"
$env:AWK = "$BinUnix/awk.exe"
$env:GREP = "$BinUnix/grep.exe"
$env:EGREP = "$BinUnix/grep.exe -E"
$env:FGREP = "$BinUnix/grep.exe -F"
$env:TMPDIR = $TmpDir
$env:LIBS = '-lws2_32 -liphlpapi'
$env:CPPFLAGS = "-I$CompatIncludeUnix -DWIN32"
$env:CFLAGS = '-O2 -Wall'

Push-Location $Root
try {
    Write-Host 'Configuring native Windows static build...'
    & (Join-Path $Bin 'sh.exe') ./configure `
        --build=x86_64-w64-mingw32 `
        --host=x86_64-w64-mingw32 `
        --without-sctp `
        --without-openssl `
        --without-ldconfig `
        --disable-shared `
        --enable-static-bin
    Assert-LastExitCode 'configure'

    Write-Host 'Cleaning previous build outputs...'
    & (Join-Path $Bin 'make.exe') -C src clean
    Assert-LastExitCode 'clean'

    # compat.o is explicitly linked from src/Makefile.am. Build it every time so
    # changes to the Win32 shim or its headers can never leave a stale object.
    Write-Host 'Compiling Win32 compatibility layer...'
    $IncludeFlag = '-I' + $CompatInclude
    & $Gcc -O2 -Wall $IncludeFlag -DWIN32 -c 'src\win32\compat.c' -o 'src\win32\compat.o'
    Assert-LastExitCode 'Win32 compatibility layer compilation'

    Write-Host "Building iperf3.exe with $Jobs job(s)..."
    & (Join-Path $Bin 'make.exe') -C src "-j$Jobs" iperf3.exe
    Assert-LastExitCode 'iperf3 build'

    $Exe = Join-Path $Root 'src\iperf3.exe'
    if (-not $NoStrip) {
        & (Join-Path $Bin 'strip.exe') --strip-unneeded $Exe
        Assert-LastExitCode 'strip'
    }

    Write-Host ''
    Write-Host 'Build complete:'
    Get-Item $Exe | Select-Object FullName, Length, LastWriteTime | Format-List

    Write-Host 'Imported DLLs:'
    & (Join-Path $Bin 'objdump.exe') -p $Exe |
        Select-String 'DLL Name' |
        ForEach-Object { $_.Line.Trim() }

    Write-Host 'SHA-256:'
    Get-FileHash -Algorithm SHA256 $Exe | Format-List
}
finally {
    Pop-Location
}
