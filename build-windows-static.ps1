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
$ToolchainArchive = Join-Path $Root ".toolchain\w64devkit-x64-$ToolchainVersion.7z.exe"
$ToolchainUrl = "https://github.com/skeeto/w64devkit/releases/download/v$ToolchainVersion/w64devkit-x64-$ToolchainVersion.7z.exe"

$StrawberryVersion = '5.32.1.1'
$StrawberrySha256 = '692646105b0f5e058198a852dc52a48f1cebcaf676d63bbdeae12f4eaee9bf5c'
$StrawberryArchive = Join-Path $Root ".toolchain\strawberry-perl-$StrawberryVersion-64bit-portable.zip"
$StrawberryUrl = "https://strawberryperl.com/download/$StrawberryVersion/strawberry-perl-$StrawberryVersion-64bit-portable.zip"
$PerlCompatDir = Join-Path $Root '.toolchain\perl-compat'

$TmpDir = Join-Path $Root '.tmp'
$OpenSSLSource = Join-Path $Root 'third_party\openssl'
$OpenSSLSnapshot = Join-Path $TmpDir 'openssl-src'
$OpenSSLArchive = Join-Path $TmpDir 'openssl-src.tar'
$OpenSSLInstall = Join-Path $TmpDir 'openssl-install'

function Assert-LastExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE"
    }
}

function Convert-ToMsysPath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path).Replace('\', '/')
    if ($full -match '^([A-Za-z]):/(.*)$') {
        return '/' + $Matches[1].ToLowerInvariant() + '/' + $Matches[2]
    }
    return $full
}

function Ensure-PerlCompatibilityModules([string]$PerlExe) {
    if (-not (Test-Path $PerlCompatDir)) {
        New-Item -ItemType Directory -Force $PerlCompatDir | Out-Null
    }

    $env:PERL5LIB = Convert-ToMsysPath $PerlCompatDir
    & $PerlExe -MLocale::Maketext::Simple -MExtUtils::MakeMaker -MPod::Usage -e '1' 2>$null
    if ($LASTEXITCODE -eq 0) {
        return
    }

    if (-not (Test-Path $StrawberryArchive)) {
        Write-Host "Downloading Strawberry Perl module source $StrawberryVersion..."
        & curl.exe -L --fail --retry 3 -o $StrawberryArchive $StrawberryUrl
        Assert-LastExitCode 'Strawberry Perl download'
    }

    $actualHash = (Get-FileHash -Algorithm SHA256 $StrawberryArchive).Hash.ToLowerInvariant()
    if ($actualHash -ne $StrawberrySha256) {
        throw "Strawberry Perl SHA-256 mismatch. Expected $StrawberrySha256, got $actualHash"
    }

    Write-Host 'Extracting the minimal pure-Perl modules needed by OpenSSL Configure...'
    Remove-Item $PerlCompatDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $PerlCompatDir | Out-Null

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($StrawberryArchive)
    try {
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\', '/')
            if ($name -notmatch '^perl/lib/(Locale|I18N|ExtUtils|Pod)(/|\.pm)') {
                continue
            }

            $relative = $name.Substring('perl/lib/'.Length).Replace('/', '\')
            $destination = Join-Path $PerlCompatDir $relative
            if ([string]::IsNullOrEmpty($entry.Name)) {
                New-Item -ItemType Directory -Force $destination | Out-Null
                continue
            }

            $parent = Split-Path $destination -Parent
            New-Item -ItemType Directory -Force $parent | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $destination, $true)
        }
    }
    finally {
        $zip.Dispose()
    }

    $env:PERL5LIB = Convert-ToMsysPath $PerlCompatDir
    & $PerlExe -MLocale::Maketext::Simple -MExtUtils::MakeMaker -MPod::Usage -e '1'
    Assert-LastExitCode 'Perl compatibility module validation'
}

New-Item -ItemType Directory -Force (Join-Path $Root '.toolchain') | Out-Null
New-Item -ItemType Directory -Force $TmpDir | Out-Null

if (-not (Test-Path $Gcc)) {
    if (-not (Test-Path $ToolchainArchive)) {
        Write-Host "Downloading w64devkit $ToolchainVersion..."
        & curl.exe -L --fail --retry 3 -o $ToolchainArchive $ToolchainUrl
        Assert-LastExitCode 'w64devkit download'
    }

    Write-Host 'Extracting w64devkit...'
    New-Item -ItemType Directory -Force $ToolchainDir | Out-Null
    & $ToolchainArchive -y "-o$ToolchainDir"
    Assert-LastExitCode 'w64devkit extraction'
}

if (-not (Test-Path $Gcc)) {
    throw "GCC was not found after toolchain setup: $Gcc"
}

$gitCommand = Get-Command git.exe -ErrorAction Stop
$gitRoot = Split-Path (Split-Path $gitCommand.Source -Parent) -Parent
$gitUsr = Join-Path $gitRoot 'usr'
$gitUsrJunction = Join-Path $Root '.toolchain\gitusr'
Remove-Item $gitUsrJunction -Force -ErrorAction SilentlyContinue
New-Item -ItemType Junction -Path $gitUsrJunction -Target $gitUsr | Out-Null
$gitUsrBin = Join-Path $gitUsrJunction 'bin'
$perl = Join-Path $gitUsrBin 'perl.exe'
if (-not (Test-Path $perl)) {
    throw "Git for Windows Perl was not found through the local junction: $perl"
}

if (-not (Test-Path (Join-Path $OpenSSLSource 'Configure'))) {
    Write-Host 'Initializing OpenSSL submodule...'
    & git.exe -C $Root submodule update --init --recursive -- third_party/openssl
    Assert-LastExitCode 'OpenSSL submodule initialization'
}

Ensure-PerlCompatibilityModules $perl

$BinUnix = $Bin.Replace('\', '/')
$CompatInclude = Join-Path $Root 'src\win32\include'
$CompatIncludeUnix = $CompatInclude.Replace('\', '/')
$OpenSSLInstallUnix = $OpenSSLInstall.Replace('\', '/')
$perlMake = 'PERL=' + $perl.Replace('\', '/')

$env:PATH = "$Bin;$gitUsrBin;$env:PATH"
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
$env:PERL5LIB = Convert-ToMsysPath $PerlCompatDir

Push-Location $Root
try {
    Write-Host 'Creating a clean OpenSSL source snapshot from the pinned submodule commit...'
    Remove-Item $OpenSSLSnapshot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $OpenSSLArchive -Force -ErrorAction SilentlyContinue
    Remove-Item $OpenSSLInstall -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $OpenSSLSnapshot | Out-Null
    New-Item -ItemType Directory -Force $OpenSSLInstall | Out-Null

    & git.exe -C $OpenSSLSource archive --format=tar HEAD -o $OpenSSLArchive
    Assert-LastExitCode 'OpenSSL source snapshot creation'
    & tar.exe -xf $OpenSSLArchive -C $OpenSSLSnapshot
    Assert-LastExitCode 'OpenSSL source snapshot extraction'

    $opensslCommit = (& git.exe -C $OpenSSLSource rev-parse HEAD).Trim()
    $opensslTag = (& git.exe -C $OpenSSLSource describe --tags --exact-match 2>$null).Trim()
    Write-Host "Building OpenSSL $opensslTag ($opensslCommit)..."

    Push-Location $OpenSSLSnapshot
    try {
        & $perl .\Configure `
            mingw64 `
            no-shared `
            no-module `
            no-tests `
            no-apps `
            no-docs `
            no-asm `
            "--prefix=$OpenSSLInstallUnix" `
            "--openssldir=$OpenSSLInstallUnix/ssl" `
            --libdir=lib
        Assert-LastExitCode 'OpenSSL configure'
    }
    finally {
        Pop-Location
    }

    & (Join-Path $Bin 'make.exe') -C $OpenSSLSnapshot "-j$Jobs" $perlMake
    Assert-LastExitCode 'OpenSSL build'
    & (Join-Path $Bin 'make.exe') -C $OpenSSLSnapshot install_sw $perlMake
    Assert-LastExitCode 'OpenSSL install'

    $sslLibrary = Join-Path $OpenSSLInstall 'lib\libssl.a'
    $cryptoLibrary = Join-Path $OpenSSLInstall 'lib\libcrypto.a'
    if (-not (Test-Path $sslLibrary) -or -not (Test-Path $cryptoLibrary)) {
        throw 'OpenSSL static libraries were not installed as expected.'
    }

    $env:LIBS = '-lws2_32 -liphlpapi -lcrypt32 -lgdi32'
    $env:CPPFLAGS = "-I$CompatIncludeUnix -DWIN32"
    $env:CFLAGS = '-O2 -Wall'

    Write-Host 'Configuring native Windows static iperf3 build with OpenSSL authentication...'
    & (Join-Path $Bin 'sh.exe') ./configure `
        --build=x86_64-w64-mingw32 `
        --host=x86_64-w64-mingw32 `
        --without-sctp `
        "--with-openssl=$OpenSSLInstallUnix" `
        --without-ldconfig `
        --disable-shared `
        --enable-static-bin
    Assert-LastExitCode 'iperf3 configure'

    Write-Host 'Cleaning previous iperf3 build outputs...'
    & (Join-Path $Bin 'make.exe') -C src clean
    Assert-LastExitCode 'iperf3 clean'

    # compat.o is explicitly linked from src/Makefile.am. Build it every time so
    # changes to the Win32 shim or its headers can never leave a stale object.
    Write-Host 'Compiling Win32 compatibility layer...'
    $includeFlag = '-I' + $CompatInclude
    & $Gcc -O2 -Wall $includeFlag -DWIN32 -c 'src\win32\compat.c' -o 'src\win32\compat.o'
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
    Write-Host 'iperf3 feature summary:'
    & $Exe --version
    Assert-LastExitCode 'iperf3 version check'

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
