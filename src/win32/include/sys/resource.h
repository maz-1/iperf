#ifndef IPERF_WIN32_SYS_RESOURCE_H
#define IPERF_WIN32_SYS_RESOURCE_H

#include <sys/time.h>

#define RUSAGE_SELF 0

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};

int getrusage(int who, struct rusage *usage);

#endif /* IPERF_WIN32_SYS_RESOURCE_H */
