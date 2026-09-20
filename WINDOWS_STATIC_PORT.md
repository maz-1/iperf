# Native Windows Static Port

This branch contains an experimental native Win64 port of iperf3. Its primary goal is a standalone `iperf3.exe` that does not require Cygwin, MSYS2, libwinpthread, libgcc, OpenSSL, or any other non-system runtime DLL.

## Build

Run from PowerShell on Windows 10/11:

```powershell
.\build-windows-static.ps1
```

The script downloads a pinned portable w64devkit toolchain when needed, configures a MinGW-w64 native build, disables SCTP and OpenSSL authentication, statically links the MinGW/pthread runtime, builds `src\iperf3.exe`, strips it, and prints the PE DLL import table and SHA-256 hash.

The downloaded toolchain is a build-time dependency only. It is not required to run the resulting executable.

## Runtime dependencies

The current x64 release build imports only Windows system DLLs:

- `KERNEL32.dll`
- `msvcrt.dll`
- `WS2_32.dll`

The Windows system RNG is obtained from the system `advapi32.dll` at runtime via `SystemFunction036` (`RtlGenRandom`). No third-party DLL is loaded.

## Validated functionality

Validated on Windows 11 using two instances of the native executable on loopback:

- TCP client/server
- UDP client/server
- TCP reverse mode (`-R`)
- Parallel TCP streams (`-P 4`)
- JSON output (`-J`)
- One-shot server (`-1`)

Representative local tests during development:

| Test | Result |
| --- | --- |
| TCP, 1 stream, 2 s | ~6.17 Gbit/s |
| TCP reverse, 2 s | ~7.66 Gbit/s |
| TCP, 4 streams, 2 s | ~30.7 Gbit/s aggregate |
| UDP, 200 Mbit/s, 2 s | 200 Mbit/s, 0% loss |
| JSON TCP, 1 s | valid JSON, exit code 0 |

These loopback numbers validate the data paths; they are not intended as hardware performance benchmarks.

## Windows compatibility layer

The port keeps the upstream iperf3 protocol/state-machine implementation and adds a thin Win32 compatibility layer under `src\win32` for:

- Winsock startup, socket I/O, socket options, and error translation
- non-blocking connect and socket mode changes
- process CPU-time reporting
- minimal `uname`, signal, terminal, and memory-map compatibility
- Windows system random data
- anonymous stream buffers without the Unix `mkstemp` + unlink-while-open behavior

## Current limitations

- SCTP is disabled.
- OpenSSL authentication is disabled in the static build script.
- Unix daemon mode is not implemented on native Windows; run the server in the foreground or manage it as a Windows process/service externally.
- Zero-copy/sendfile is not available in this port.
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
