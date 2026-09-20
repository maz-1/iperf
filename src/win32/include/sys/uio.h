#ifndef IPERF_WIN32_SYS_UIO_H
#define IPERF_WIN32_SYS_UIO_H

#include <stddef.h>

struct iovec {
    void *iov_base;
    size_t iov_len;
};

#endif /* IPERF_WIN32_SYS_UIO_H */
