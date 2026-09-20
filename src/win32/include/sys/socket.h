#ifndef IPERF_WIN32_SYS_SOCKET_H
#define IPERF_WIN32_SYS_SOCKET_H

#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef SHUT_RD
#define SHUT_RD SD_RECEIVE
#endif
#ifndef SHUT_WR
#define SHUT_WR SD_SEND
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

#endif /* IPERF_WIN32_SYS_SOCKET_H */
