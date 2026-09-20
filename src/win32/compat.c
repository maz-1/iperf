#define IPERF_WIN32_COMPAT_IMPL 1
#include "iperf_win32.h"
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <termios.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

static int g_wsa_started = 0;

static void filetime_to_timeval(const FILETIME *ft, struct timeval *tv)
{
    ULARGE_INTEGER value;
    value.LowPart = ft->dwLowDateTime;
    value.HighPart = ft->dwHighDateTime;
    value.QuadPart /= 10ULL;
    tv->tv_sec = (long)(value.QuadPart / 1000000ULL);
    tv->tv_usec = (long)(value.QuadPart % 1000000ULL);
}

void iperf_win32_set_errno_from_wsa(int error)
{
    switch (error) {
    case WSAEINTR: errno = EINTR; break;
    case WSAEBADF: errno = EBADF; break;
    case WSAEACCES: errno = EACCES; break;
    case WSAEFAULT: errno = EFAULT; break;
    case WSAEINVAL: errno = EINVAL; break;
    case WSAEMFILE: errno = EMFILE; break;
    case WSAEWOULDBLOCK: errno = EWOULDBLOCK; break;
    case WSAEINPROGRESS: errno = EINPROGRESS; break;
    case WSAEALREADY: errno = EALREADY; break;
    case WSAENOTSOCK: errno = ENOTSOCK; break;
    case WSAEDESTADDRREQ: errno = EDESTADDRREQ; break;
    case WSAEMSGSIZE: errno = EMSGSIZE; break;
    case WSAEPROTOTYPE: errno = EPROTOTYPE; break;
    case WSAENOPROTOOPT: errno = ENOPROTOOPT; break;
    case WSAEPROTONOSUPPORT: errno = EPROTONOSUPPORT; break;
    case WSAESOCKTNOSUPPORT: errno = EOPNOTSUPP; break;
    case WSAEOPNOTSUPP: errno = EOPNOTSUPP; break;
    case WSAEAFNOSUPPORT: errno = EAFNOSUPPORT; break;
    case WSAEADDRINUSE: errno = EADDRINUSE; break;
    case WSAEADDRNOTAVAIL: errno = EADDRNOTAVAIL; break;
    case WSAENETDOWN: errno = ENETDOWN; break;
    case WSAENETUNREACH: errno = ENETUNREACH; break;
    case WSAENETRESET: errno = ENETRESET; break;
    case WSAECONNABORTED: errno = ECONNABORTED; break;
    case WSAECONNRESET: errno = ECONNRESET; break;
    case WSAENOBUFS: errno = ENOBUFS; break;
    case WSAEISCONN: errno = EISCONN; break;
    case WSAENOTCONN: errno = ENOTCONN; break;
    case WSAETIMEDOUT: errno = ETIMEDOUT; break;
    case WSAECONNREFUSED: errno = ECONNREFUSED; break;
    case WSAEHOSTUNREACH: errno = EHOSTUNREACH; break;
    default: errno = EIO; break;
    }
}

int iperf_win32_init(void)
{
    WSADATA wsa;
    int rc;
    if (g_wsa_started)
        return 0;
    rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0) {
        iperf_win32_set_errno_from_wsa(rc);
        return -1;
    }
    g_wsa_started = 1;
    return 0;
}

void iperf_win32_cleanup(void)
{
    if (g_wsa_started) {
        WSACleanup();
        g_wsa_started = 0;
    }
}

static void __attribute__((constructor)) iperf_win32_ctor(void)
{
    (void)iperf_win32_init();
}

static void __attribute__((destructor)) iperf_win32_dtor(void)
{
    iperf_win32_cleanup();
}

int iperf_win_close(int fd)
{
    SOCKET s = (SOCKET)(uintptr_t)(unsigned int)fd;
    int type = 0;
    int type_len = (int)sizeof(type);

    if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &type_len) == 0) {
        if (closesocket(s) == 0)
            return 0;
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
        return -1;
    }

    return _close(fd);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long long offset)
{
    void *p;
    (void)addr;
    (void)prot;
    (void)flags;
    (void)fd;
    (void)offset;
    p = malloc(length);
    return p ? p : MAP_FAILED;
}

int munmap(void *addr, size_t length)
{
    (void)length;
    if (addr && addr != MAP_FAILED)
        free(addr);
    return 0;
}

int getrusage(int who, struct rusage *usage)
{
    FILETIME creation, exit_time, kernel, user;
    (void)who;
    if (!usage) {
        errno = EINVAL;
        return -1;
    }
    memset(usage, 0, sizeof(*usage));
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit_time, &kernel, &user)) {
        errno = EIO;
        return -1;
    }
    filetime_to_timeval(&user, &usage->ru_utime);
    filetime_to_timeval(&kernel, &usage->ru_stime);
    return 0;
}

int uname(struct utsname *name)
{
    DWORD node_len;
    OSVERSIONINFOA version;
    if (!name) {
        errno = EINVAL;
        return -1;
    }
    memset(name, 0, sizeof(*name));
    snprintf(name->sysname, sizeof(name->sysname), "Windows");
    node_len = (DWORD)sizeof(name->nodename);
    if (!GetComputerNameA(name->nodename, &node_len))
        snprintf(name->nodename, sizeof(name->nodename), "unknown");
    memset(&version, 0, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    if (GetVersionExA(&version)) {
        snprintf(name->release, sizeof(name->release), "%lu.%lu", (unsigned long)version.dwMajorVersion, (unsigned long)version.dwMinorVersion);
        snprintf(name->version, sizeof(name->version), "build %lu", (unsigned long)version.dwBuildNumber);
    } else {
        snprintf(name->release, sizeof(name->release), "unknown");
        snprintf(name->version, sizeof(name->version), "unknown");
    }
#pragma GCC diagnostic pop
#if defined(_M_X64) || defined(__x86_64__)
    snprintf(name->machine, sizeof(name->machine), "x86_64");
#elif defined(_M_ARM64) || defined(__aarch64__)
    snprintf(name->machine, sizeof(name->machine), "aarch64");
#else
    snprintf(name->machine, sizeof(name->machine), "x86");
#endif
    return 0;
}

int tcgetattr(int fd, struct termios *termios_p)
{
    intptr_t os_handle;
    DWORD mode;
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    os_handle = _get_osfhandle(fd);
    if (os_handle == -1 || !GetConsoleMode((HANDLE)os_handle, &mode)) {
        errno = ENOTTY;
        return -1;
    }
    termios_p->c_lflag = (mode & ENABLE_ECHO_INPUT) ? ECHO : 0;
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    intptr_t os_handle;
    DWORD mode;
    (void)optional_actions;
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    os_handle = _get_osfhandle(fd);
    if (os_handle == -1 || !GetConsoleMode((HANDLE)os_handle, &mode)) {
        errno = ENOTTY;
        return -1;
    }
    if (termios_p->c_lflag & ECHO)
        mode |= ENABLE_ECHO_INPUT;
    else
        mode &= ~ENABLE_ECHO_INPUT;
    if (!SetConsoleMode((HANDLE)os_handle, mode)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int iperf_win_setsockopt(int s, int level, int optname, const void *optval, int optlen)
{
    SOCKET sock = (SOCKET)(uintptr_t)(unsigned int)s;
    int rc = setsockopt(sock, level, optname, (const char *)optval, optlen);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_getsockopt(int s, int level, int optname, void *optval, int *optlen)
{
    SOCKET sock = (SOCKET)(uintptr_t)(unsigned int)s;
    int rc = getsockopt(sock, level, optname, (char *)optval, optlen);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_sigemptyset(unsigned long *set)
{
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = 0;
    return 0;
}

int iperf_win_sigaddset(unsigned long *set, int sig)
{
    if (!set || sig < 0 || sig >= (int)(sizeof(*set) * 8)) {
        errno = EINVAL;
        return -1;
    }
    *set |= (1UL << sig);
    return 0;
}

const char *strsignal(int sig)
{
    switch (sig) {
    case SIGINT: return "Interrupt";
    case SIGTERM: return "Terminated";
    case SIGABRT: return "Aborted";
    case SIGFPE: return "Floating point exception";
    case SIGILL: return "Illegal instruction";
    case SIGSEGV: return "Segmentation fault";
    case SIGPIPE: return "Broken pipe";
    default: return "Signal";
    }
}

int kill(int pid, int sig)
{
    HANDLE process;
    DWORD error;
    if (sig != 0) {
        errno = EINVAL;
        return -1;
    }
    process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (process) {
        CloseHandle(process);
        return 0;
    }
    error = GetLastError();
    if (error == ERROR_ACCESS_DENIED)
        return 0;
    errno = ESRCH;
    return -1;
}


static SOCKET iperf_win_socket_handle(int s)
{
    return (SOCKET)(uintptr_t)(unsigned int)s;
}

int iperf_win_socket(int af, int type, int protocol)
{
    SOCKET s = socket(af, type, protocol);
    if (s == INVALID_SOCKET) {
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
        return -1;
    }
    return (int)(uintptr_t)s;
}

int iperf_win_connect(int s, const struct sockaddr *name, int namelen)
{
    int rc = connect(iperf_win_socket_handle(s), name, namelen);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_bind(int s, const struct sockaddr *name, int namelen)
{
    int rc = bind(iperf_win_socket_handle(s), name, namelen);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_listen(int s, int backlog)
{
    int rc = listen(iperf_win_socket_handle(s), backlog);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_accept(int s, struct sockaddr *addr, int *addrlen)
{
    SOCKET accepted = accept(iperf_win_socket_handle(s), addr, addrlen);
    if (accepted == INVALID_SOCKET) {
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
        return -1;
    }
    return (int)(uintptr_t)accepted;
}

int iperf_win_recv(int s, void *buf, int len, int flags)
{
    int rc = recv(iperf_win_socket_handle(s), (char *)buf, len, flags);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_recvfrom(int s, void *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    int rc = recvfrom(iperf_win_socket_handle(s), (char *)buf, len, flags, from, fromlen);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_send(int s, const void *buf, int len, int flags)
{
    int rc = send(iperf_win_socket_handle(s), (const char *)buf, len, flags);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_sendto(int s, const void *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    int rc = sendto(iperf_win_socket_handle(s), (const char *)buf, len, flags, to, tolen);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout)
{
    int rc = select(nfds, readfds, writefds, exceptfds, timeout);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_getsockname(int s, struct sockaddr *name, int *namelen)
{
    int rc = getsockname(iperf_win_socket_handle(s), name, namelen);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}

int iperf_win_getpeername(int s, struct sockaddr *name, int *namelen)
{
    int rc = getpeername(iperf_win_socket_handle(s), name, namelen);
    if (rc == SOCKET_ERROR)
        iperf_win32_set_errno_from_wsa(WSAGetLastError());
    return rc;
}


int iperf_win_random(void *buffer, size_t length)
{
    typedef BOOLEAN (WINAPI *rtl_gen_random_fn)(PVOID, ULONG);
    static HMODULE advapi = NULL;
    static rtl_gen_random_fn rtl_gen_random = NULL;
    unsigned char *p = (unsigned char *)buffer;

    if (!buffer && length != 0) {
        errno = EINVAL;
        return -1;
    }
    if (length == 0)
        return 0;

    if (!rtl_gen_random) {
        advapi = GetModuleHandleA("advapi32.dll");
        if (!advapi)
            advapi = LoadLibraryA("advapi32.dll");
        if (!advapi) {
            errno = ENOSYS;
            return -1;
        }
        rtl_gen_random = (rtl_gen_random_fn)(void *)GetProcAddress(advapi, "SystemFunction036");
        if (!rtl_gen_random) {
            errno = ENOSYS;
            return -1;
        }
    }

    while (length > 0) {
        ULONG chunk = length > 0xffffffffULL ? 0xffffffffUL : (ULONG)length;
        if (!rtl_gen_random(p, chunk)) {
            errno = EIO;
            return -1;
        }
        p += chunk;
        length -= chunk;
    }
    return 0;
}


char *iperf_win_strndup(const char *src, size_t maxlen)
{
    size_t len = 0;
    char *copy;

    if (!src) {
        errno = EINVAL;
        return NULL;
    }

    while (len < maxlen && src[len] != '\0')
        ++len;

    copy = (char *)malloc(len + 1);
    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }

    memcpy(copy, src, len);
    copy[len] = '\0';
    return copy;
}

