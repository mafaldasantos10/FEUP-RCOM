#ifndef LINK_APP_H
#define LINK_APP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#define TRANSMITTER 0
#define RECEIVER 1

#define MAX_BUF 255

/* Function prototypes */
int llopen(int porta, int channel);
int llwrite(int fd, char *buffer, int length);
int llread(int fd, char *buffer);
int llclose(int fd);

#endif /* LINK_APP_H */