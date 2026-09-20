# Native Windows Static Port

This branch contains an experimental native Win64 port of iperf3. Its primary goal is a standalone `iperf3.exe` that does not require Cygwin, MSYS2, libwinpthread, libgcc, OpenSSL runtime DLLs, or any other non-system DLL.

OpenSSL authentication is enabled. OpenSSL is included as a pinned Git submodule and linked statically into the final executable.

## Build

Run from PowerShell on Windows 10/11:

```powershell
.\build-windows-static.ps1
```

The script performs the complete build:

1. Downloads the pinned portable w64devkit toolchain when needed.
2. Initializes `third_party/openssl` if the submodule is not present.
3. Uses the Git for Windows Perl runtime plus a small set of pure-Perl compatibility modules required by OpenSSL Configure.
4. Creates a clean temporary source snapshot from the pinned OpenSSL submodule commit.
5. Builds OpenSSL as static Win64 libraries with `no-shared`, `no-module`, `no-tests`, `no-apps`, and `no-asm`.
6. Configures iperf3 with OpenSSL authentication enabled.
7. Statically links the MinGW/pthread/OpenSSL runtime into `src\iperf3.exe`.
8. Strips the executable and prints its PE DLL import table and SHA-256 hash.

The build toolchain, Perl helper modules, and OpenSSL build tree are build-time dependencies only. They are not required to run the resulting executable.

### OpenSSL version

The submodule is currently pinned to:

- OpenSSL `3.6.4`
- commit `d3c1b1169b3569ff3069e5b399f47b2b28e03d79`

The build script compiles from the pinned submodule commit, not from OpenSSL `master`.

## Runtime dependencies

The current x64 static build imports only Windows system DLLs:

- `ADVAPI32.dll`
- `CRYPT32.dll`
- `KERNEL32.dll`
- `msvcrt.dll`
- `USER32.dll`
- `WS2_32.dll`

There is no dependency on `libcrypto-*.dll`, `libssl-*.dll`, `cygwin1.dll`, `msys-2.0.dll`, `libwinpthread-1.dll`, or `libgcc_s_*.dll`.

The Windows compatibility layer also obtains system random data through the Windows `RtlGenRandom` implementation.

## Validated functionality

Validated on Windows 11 using two instances of the native executable on loopback:

- TCP client/server
- UDP client/server
- TCP reverse mode (`-R`)
- Parallel TCP streams (`-P 4`)
- JSON output (`-J`)
- One-shot server (`-1`)
- RSA/OpenSSL authentication with a valid username/password
- Authentication rejection with an invalid password

Representative local tests during development:

| Test | Result |
| --- | --- |
| TCP, 1 stream, 2 s | ~6.17 Gbit/s |
| TCP reverse, 2 s | ~7.66 Gbit/s |
| TCP, 4 streams, 2 s | ~30.7 Gbit/s aggregate |
| UDP, 200 Mbit/s, 2 s | 200 Mbit/s, 0% loss |
| JSON TCP, 1 s | valid JSON, exit code 0 |
| Authenticated TCP, 1 s | ~6.05 Gbit/s, client/server exit code 0 |
| Wrong authentication password | rejected; client exit code 1 |

These loopback numbers validate the data paths; they are not intended as hardware performance benchmarks.

`iperf3.exe --version` reports `authentication` in the optional feature list when the OpenSSL-enabled build is active.

## Authentication usage

The server authorized-users file uses one record per line:

```text
username,sha256({username}password)
```

For example, run the server with a private key and authorized-users file:

```powershell
.\src\iperf3.exe -s `
  --rsa-private-key-path=.\private.pem `
  --authorized-users-path=.\authorized-user.txt
```

Run a client with the corresponding public key and username. The password can be supplied through `IPERF3_PASSWORD`:

```powershell
$env:IPERF3_PASSWORD = 'your-password'
.\src\iperf3.exe -c 192.168.1.10 `
  --rsa-public-key-path=.\public.pem `
  --username=your-user
```

The repository includes the upstream iperf3 authentication implementation; RSA key handling, credential encryption, and password verification are performed through the statically linked OpenSSL library.

## Windows compatibility layer

The port keeps the upstream iperf3 protocol/state-machine implementation and adds a thin Win32 compatibility layer under `src\win32` for:

- Winsock startup, socket I/O, socket options, and error translation
- non-blocking connect and socket mode changes
- process CPU-time reporting
- minimal `uname`, signal, terminal, and memory-map compatibility
- Windows system random data
- anonymous stream buffers without the Unix `mkstemp` + unlink-while-open behavior
- `strndup` compatibility needed by the authentication implementation

## Current limitations

- SCTP is disabled.
- Unix daemon mode is not implemented on native Windows; run the server in the foreground or manage it as a Windows process/service externally.
- Zero-copy/sendfile is not available in this port.
- The OpenSSL command-line application is not shipped; only the static libraries required by iperf3 authentication are built into `iperf3.exe`.
- The port still inherits upstream's use of `int` for socket identifiers. It is validated for normal iperf3 workloads but has not been hardened for unusually large Windows handle values or extremely high process handle counts.
- The platform-information string currently uses a compatibility Windows version query and can report a legacy Windows version number; this does not affect measurements.

## Basic smoke tests

```powershell
# Terminal 1
.\src\iperf3.exe -s

# Terminal 2: TCP
.\src\iperf3.exe -c 127.0.0.1 -t 2

# UDP
.\src\iperf3.exe -c 127.0.0.1 -u -b 200M -t 2

# Reverse TCP
.\src\iperf3.exe -c 127.0.0.1 -R -t 2

# Four TCP streams
.\src\iperf3.exe -c 127.0.0.1 -P 4 -t 2
```
