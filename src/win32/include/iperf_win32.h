#ifndef IPERF_WIN32_COMPAT_H
#define IPERF_WIN32_COMPAT_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int iperf_win32_init(void);
void iperf_win32_cleanup(void);
int iperf_win_close(int fd);
void iperf_win32_set_errno_from_wsa(int wsa_error);
int iperf_win_random(void *buffer, size_t length);
char *iperf_win_strndup(const char *src, size_t maxlen);
int iperf_win_daemon(int nochdir, int noclose);
int iperf_win_has_transmitfile(void);
int iperf_win_create_zerocopy_file(void);
int iperf_win_prepare_zerocopy_file(int fd, const void *buffer, size_t length);
int iperf_win_transmitfile(int file_fd, int socket_fd, size_t count);
int iperf_win_socket(int af, int type, int protocol);
int iperf_win_connect(int s, const struct sockaddr *name, int namelen);
int iperf_win_bind(int s, const struct sockaddr *name, int namelen);
int iperf_win_listen(int s, int backlog);
int iperf_win_accept(int s, struct sockaddr *addr, int *addrlen);
int iperf_win_recv(int s, void *buf, int len, int flags);
int iperf_win_recvfrom(int s, void *buf, int len, int flags, struct sockaddr *from, int *fromlen);
int iperf_win_send(int s, const void *buf, int len, int flags);
int iperf_win_sendto(int s, const void *buf, int len, int flags, const struct sockaddr *to, int tolen);
int iperf_win_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout);
int iperf_win_getsockname(int s, struct sockaddr *name, int *namelen);
int iperf_win_getpeername(int s, struct sockaddr *name, int *namelen);
int iperf_win_setsockopt(int s, int level, int optname, const void *optval, int optlen);
int iperf_win_getsockopt(int s, int level, int optname, void *optval, int *optlen);
int iperf_win_sigemptyset(unsigned long *set);
int iperf_win_sigaddset(unsigned long *set, int sig);
const char *strsignal(int sig);
int kill(int pid, int sig);

#ifdef __cplusplus
}
#endif

#ifndef IPERF_WIN32_COMPAT_IMPL
#define close iperf_win_close
#define strndup iperf_win_strndup
#define socket(af, type, protocol) iperf_win_socket((af), (type), (protocol))
#define bind(s, name, namelen) iperf_win_bind((int)(s), (name), (int)(namelen))
#define recv(s, buf, len, flags) iperf_win_recv((int)(s), (buf), (int)(len), (flags))
#define recvfrom(s, buf, len, flags, from, fromlen) iperf_win_recvfrom((int)(s), (buf), (int)(len), (flags), (from), (int *)(fromlen))
#define send(s, buf, len, flags) iperf_win_send((int)(s), (buf), (int)(len), (flags))
#define sendto(s, buf, len, flags, to, tolen) iperf_win_sendto((int)(s), (buf), (int)(len), (flags), (to), (int)(tolen))
#define select(nfds, readfds, writefds, exceptfds, timeout) iperf_win_select((nfds), (readfds), (writefds), (exceptfds), (timeout))
#define getsockname(s, name, namelen) iperf_win_getsockname((int)(s), (name), (int *)(namelen))
#define getpeername(s, name, namelen) iperf_win_getpeername((int)(s), (name), (int *)(namelen))
#define setsockopt(s, level, optname, optval, optlen) iperf_win_setsockopt((int)(s), (level), (optname), (optval), (int)(optlen))
#define getsockopt(s, level, optname, optval, optlen) iperf_win_getsockopt((int)(s), (level), (optname), (optval), (int *)(optlen))
#endif

#ifndef _SIGSET_T_DEFINED
#define _SIGSET_T_DEFINED
#define sigset_t unsigned long
#endif
#ifndef SIG_BLOCK
#define SIG_BLOCK 0
#endif
#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef MSG_TRUNC
#define MSG_TRUNC 0
#endif
#ifndef sigemptyset
#define sigemptyset(set) iperf_win_sigemptyset((unsigned long *)(set))
#endif
#ifndef sigaddset
#define sigaddset(set, sig) iperf_win_sigaddset((unsigned long *)(set), (sig))
#endif

#endif /* _WIN32 */
#endif /* IPERF_WIN32_COMPAT_H */
