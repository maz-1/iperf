#ifndef IPERF_WIN32_TERMIOS_H
#define IPERF_WIN32_TERMIOS_H

typedef unsigned long tcflag_t;

struct termios {
    tcflag_t c_lflag;
};

#define ECHO 0x00000004UL
#define TCSAFLUSH 2

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);

#endif /* IPERF_WIN32_TERMIOS_H */
