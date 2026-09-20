#ifndef IPERF_WIN32_SYS_MMAN_H
#define IPERF_WIN32_SYS_MMAN_H

#include <stddef.h>
#include <stdint.h>

#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define MAP_FAILED ((void *)(intptr_t)-1)

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long long offset);
int munmap(void *addr, size_t length);

#endif /* IPERF_WIN32_SYS_MMAN_H */
