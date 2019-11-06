#ifndef LINK_APP_H
#define LINK_APP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FALSE 0
#define TRUE 1

#define TRANSMITTER 0
#define RECEIVER 1

#define MAX_BUF 215

/* Function prototypes */
int llopen(int porta, int channel);
int llwrite(int fd, char *buffer, int length);
int llread(int fd, char *buffer);
int llclose(int fd);

#endif /* LINK_APP_H */
