/*      (C)2000 FEUP  */

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

#define SERVER_PORT 6000
#define SERVER_ADDR "192.168.28.96"
#define MAX_LENGTH 20
struct hostent *h;

void parseArg(char *url, char *user, char *pass, char *host, char *path, char* fileName);
int checkUserAndPass(char* url);
char* getIp(char* host);
int getServerAns(int sockfd, char * response);

int main(int argc, char** argv){

	int	sockfd;
	struct sockaddr_in server_addr;
	char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";  
	int	bytes;
	
    char user[MAX_LENGTH];
    char password[MAX_LENGTH];
    char host[MAX_LENGTH];
    char path[MAX_LENGTH];
    char fileName[MAX_LENGTH];
    char* ipAddress;
    char response[3];

    parseArg(argv[1], user, password, host, path, fileName);

    ipAddress = getIp(host);
    printf("IP Address : %s\n",ipAddress);

	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ipAddress);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
    printf("socket will open \n");
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    	perror("socket()");
        exit(0);
    }
    printf("socket will connect \n");
	/*connect to the server*/
    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        printf("error \n");
        perror("connect()");
		exit(0);
	}
    printf("socket \n");
    getServerAns(sockfd, response);
    /*send a string to the server*/
	bytes = write(sockfd, buf, strlen(buf));
	printf("Bytes escritos %d\n", bytes);

	close(sockfd);
	exit(0);
}

void parseArg(char *url, char *user, char *pass, char *host, char *path, char* fileName)
{
    int length = strlen(url);
    char* check;
    int start = 6;
    int i = 6;

    if(length < 1)
    {
        printf(" There aren't any arguments");
    }

    check = strndup(url, 6);

    if(strcmp(check, "ftp://"))
    {
        printf("The input in incorrect \n");
    }

    if(checkUserAndPass(url))
    {
        //Reads User
        while(url[i] != ':')
        {
            user[i-start] = url[i];
            i++;
        }
        user[i-start] = '\0';
        i++;
        start = i;

        //Reads Password
        while(url[i] != '@')
        {
            pass[i-start] = url[i];
            i++;
        }
        pass[i-start] = '\0';
        i++;
        start = i;
    }


    //Reads Host
    while(url[i] != '/')
    {
        host[i-start] = url[i];
        i++;
    }
    host[i-start] = '\0';
    i++;
    start = i;

    // Reads Path
    while(url[i] != '\0')
    {
        path[i-start] = url[i];
        i++;
    }
    path[i-start] = '\0';

    //Reads FileName
    i=length;
    while(url[i] != '/')
    {
        i--;
    }
    i++;
    start = i;
    while(url[i] != '\0')
    {
        fileName[i - start] = url[i];
        i++;
    }
    fileName[i-start] = '\0';

    //checkUserAndPass(url);

    printf(" - Username:%s\n", user);
	printf(" - Password:%s\n", pass);
	printf(" - Host:%s\n", host);
	printf(" - Path :%s\n", path);
	printf(" - Filename:%s\n", fileName);
}

void sendCommand(int sockfd, char* cmdName, char* cmdCont )
{
	write(sockfd, cmdName, strlen(cmdName));
	write(sockfd, cmdCont, strlen(cmdCont));
	write(sockfd, "\n", 1);
}
 
int getServerAns(int sockfd, char * response)
{
    int i =0;
    char* character;

    while(i < 3)
    {
        printf("i: %d", i);
        read(sockfd, &character, 1);
        printf("%s", character);

        if(isdigit(character))
        {
            response[i] = *character;
            i++;
        }

        else if(*character == ' ')
        {
            return 0;
        }

        else{
            i = 0;
        }
    }

    if(response[0] == '2')
    {
        printf(" > Connection Estabilished\n"); 
        return 1;
    }
    else
    {
        printf(" > Error receiving response code\n");
        return 0;
    }
    
}

int checkUserAndPass(char* url)
{
    int i = 0;
    int user;
    int length = strlen(url);
    while(i < length)
    { 
        if(url[i] ==':')
        {
            user = 1;
            break;
        }
        i++;
    }

    i=0;
    while(i < length)
    {
        if(url[i] =='@' && user)
        {
    
            printf("found it \n");
            return 1;
        }
        i++;
    }
    printf("not found \n");
    return 0;
}
char* getIp(char* host)
{
    if ((h =gethostbyname(host)) == NULL) {  
        herror("gethostbyname");
        exit(1);
    }

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n",inet_ntoa(*((struct in_addr *)h->h_addr)));

    return inet_ntoa(*((struct in_addr *)h->h_addr));
}