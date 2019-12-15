#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>
#include <ctype.h>

#define MAX_LENGTH 1024

int parseArg(char *url);
int checkUserAndPass(char *url);
int getIp();
int getServerAns(int sockfd, char *answer);
int connectToServer(char *ip, int port);
int login();
int sendCommand(int sockfd, char *cmdName, char *cmdCont);
int enterPasv();
int parsePasvReply(char *answer);
int sendRetrieve();
int downloadFile();

struct URL
{
    char user[MAX_LENGTH];
    char password[MAX_LENGTH];
    char host[MAX_LENGTH];
    char path[MAX_LENGTH];
    char fileName[MAX_LENGTH];
    char ipAddress[MAX_LENGTH];
};
struct URL url;

struct TCP
{
    int control_sockfd;
    int data_sockfd;
    char data_ip[MAX_LENGTH];
    int data_port;
};
struct TCP tcp;

#endif /* DOWNLOAD_H */
