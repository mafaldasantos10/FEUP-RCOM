#include "download.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Wrong number of arguments.\nUsage: %s ftp://[<user>:<password>@]<host>/<url-path>\n\n", argv[0]);
        return -1;
    }

    //Parse arguments received
    if (parseArg(argv[1]) < 0)
    {
        return -1;
    }

    //Get IP
    if (getIp() < 0)
    {
        printf("Cannot get ip to hostname %s.\n", url.host);
    }

    //Create a control connection with server
    if ((tcp.control_sockfd = connectToServer(url.ipAddress, 21)) < 0)
    {
        printf("Failed to connect to server\n");
        return -1;
    }

    char answer[MAX_LENGTH];
    int ret = getServerAns(tcp.control_sockfd, answer);
    if (ret < 0)
    {
        close(tcp.control_sockfd);
        return -1;
    }
    if (ret != 220)
    {
        printf("Received an error answer from the server with code: %d!\n", ret);
        close(tcp.control_sockfd);
        return -1;
    }

    //Login
    if (login() < 0)
    {
        printf("Failed to login!\n");
        close(tcp.control_sockfd);
        return -1;
    }

    //Enter passive mode
    if (enterPasv() < 0)
    {
        printf("Failed to enter passive mode!\n");
        close(tcp.control_sockfd);
        return -1;
    }

    //Create a data connection with server
    if ((tcp.data_sockfd = connectToServer(tcp.data_ip, tcp.data_port)) < 0)
    {
        printf("Error connecting to the server\n");
        close(tcp.control_sockfd);
        return -1;
    }

    if (sendRetrieve() < 0)
    {
        close(tcp.control_sockfd);
        close(tcp.data_sockfd);
        return -1;
    }

    if (downloadFile() < 0)
    {
        close(tcp.control_sockfd);
        close(tcp.data_sockfd);
        return -1;
    }

    //CLOSE EVERYTHING
    //sendCommand("QUIT\r\n");
    close(tcp.control_sockfd);
    close(tcp.data_sockfd);
    printf("Disconnected sockets\n");
    return 0;
}

int parseArg(char *arg)
{
    int length = strlen(arg);
    char *check_ftp;
    int start = 6;
    int i = 6;

    printf("\nParsing url received... \n");

    check_ftp = strndup(arg, start);

    if (strcmp(check_ftp, "ftp://"))
    {
        printf("ERROR: The input is incorrect.  \n");
        return -1;
    }

    if (checkUserAndPass(arg))
    {
        //Reads User
        while (arg[i] != ':')
        {
            url.user[i - start] = arg[i];
            i++;
        }
        url.user[i - start] = '\0';
        i++;
        start = i;

        //Reads Password
        while (arg[i] != '@')
        {
            url.password[i - start] = arg[i];
            i++;
        }
        url.password[i - start] = '\0';
        i++;
        start = i;
    }

    //Reads Host
    while (arg[i] != '/')
    {
        url.host[i - start] = arg[i];
        i++;
    }
    url.host[i - start] = '\0';
    i++;
    start = i;

    // Reads Path
    while (arg[i] != '\0')
    {
        url.path[i - start] = arg[i];
        i++;
    }
    url.path[i - start] = '\0';

    //Reads FileName
    i = length;
    while (arg[i] != '/')
    {
        i--;
    }
    i++;
    start = i;
    while (arg[i] != '\0')
    {
        url.fileName[i - start] = arg[i];
        i++;
    }
    url.fileName[i - start] = '\0';

    printf("Finished parsing url.\n");

    printf("\n Url:\n - Username:%s\n", url.user);
    printf(" - Password:%s\n", url.password);
    printf(" - Host:%s\n", url.host);
    printf(" - Path:%s\n", url.path);
    printf(" - Filename:%s\n\n", url.fileName);

    return 0;
}

int checkUserAndPass(char *url)
{
    int i = 0;
    int user;
    int length = strlen(url);
    while (i < length)
    {
        if (url[i] == ':')
        {
            user = 1;
            break;
        }
        i++;
    }

    i = 0;
    while (i < length)
    {
        if (url[i] == '@' && user)
        {
            return 1;
        }
        i++;
    }
    return 0;
}

int getIp()
{
    struct hostent *h;

    if ((h = gethostbyname(url.host)) == NULL)
    {
        herror("gethostbyname");
        return -1;
    }
    char *ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    strcpy(url.ipAddress, ip);

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n\n", ip);

    return 0;
}

int connectToServer(char *ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);          /*server TCP port must be network byte ordered */

    /*open an TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        return -1;
    }
    else
    {
        printf("Created socket\n");
    }

    /*connect to the server*/
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        return -1;
    }
    else
    {
        printf("Connected to server by port %d\n\n", port);
    }

    return sockfd;
}

int login()
{
    char answer[MAX_LENGTH];

    //USER
    printf("Logging in..\n");

    if (sendCommand(tcp.control_sockfd, "USER", url.user) < 0)
    {
        printf("Error writing USER command to server\n");
        return -1;
    }

    int ret = getServerAns(tcp.control_sockfd, answer);
    if (ret < 0)
    {
        return -1;
    }
    if (ret != 331)
    {
        printf("Received an error answer from the server with code: %d!\n", ret);
        return -1;
    }

    //PASSWORD
    if (sendCommand(tcp.control_sockfd, "PASS", url.password) < 0)
    {
        printf("Error writing PASS command to server\n");
        return -1;
    }

    ret = getServerAns(tcp.control_sockfd, answer);
    if (ret < 0)
    {
        return -1;
    }
    if (ret != 230)
    {
        printf("Received an error answer from the server with code: %d!\n", ret);
        return -1;
    }

    return 0;
}

int enterPasv()
{
    char answer[MAX_LENGTH];

    if (sendCommand(tcp.control_sockfd, "PASV", "") < 0)
    {
        printf("Error writing PASSV command to server\n");
        return -1;
    }

    int ret = getServerAns(tcp.control_sockfd, answer);
    if (ret < 0)
    {
        return -1;
    }
    if (ret != 227)
    {
        printf("Received an error answer from the server with code: %d!\n", ret);
        return -1;
    }

    ret = parsePasvReply(answer);

    return ret;
}

int parsePasvReply(char *answer)
{
    int ip1, ip2, ip3, ip4;
    int port1, port2;

    if ((sscanf(answer, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2)) < 0)
    {
        printf("Error parsing passive mode's answer from server\n");
        return -1;
    }

    //Form the new ip and port
    sprintf(tcp.data_ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    tcp.data_port = port1 * 256 + port2;

    printf("IP for data: %s\n", tcp.data_ip);
    printf("Port for data: %d\n", tcp.data_port);

    return 0;
}

int sendRetrieve()
{
    char answer[MAX_LENGTH];

    if (sendCommand(tcp.control_sockfd, "RETR", url.path) < 0)
    {
        printf("Error writing RETR command to server\n");
        return -1;
    }

    int ret = getServerAns(tcp.control_sockfd, answer);
    if (ret < 0)
    {
        return -1;
    }
    if (ret != 150)
    {
        printf("Received an error answer from the server with code: %d!\n", ret);
        return -1;
    }

    return 0;
}

int downloadFile()
{
   // char answer[MAX_LENGTH];
    char buffer[MAX_LENGTH];
    int res;

    FILE *file = fopen(url.fileName, "w");
    if (!file)
    {
        printf("Error opening file %s!\n", url.fileName);
        return -1;
    }

    printf("Downloading file...\n");

    //Read file
    while ((res = read(tcp.data_sockfd, buffer, MAX_LENGTH)))
    {
        if (res < 0)
        {
            printf("Error while reading file bytes from the socket!\n");
            return -1;
        }
        if (fwrite(buffer, res, 1, file) < 0)
        {
            printf("Error writing to requested file!\n");
            return -1;
        }
    }

    //Get server answer
   /* int ret = getServerAns(tcp.control_sockfd, answer);
    if (ret < 0)
    {
        return -1;
    }
    if (ret != 226)
    {
        printf("Received an error answer from the server with code: %d!\n", ret);
        return -1;
    }*/

    fclose(file);
    printf("*Finished downloading file %s*\n\n", url.fileName);
    return 0;
}

int getServerAns(int sockfd, char *answer)
{
    usleep(100 * 1000);
    char buffer[MAX_LENGTH];
    memset(buffer, 0, MAX_LENGTH);
    read(sockfd, &buffer, MAX_LENGTH);

    strcpy(answer, buffer);
    /* for (i = 0; i < MAX_LENGTH - 1; i += ret)
    {
        ret = read(sockfd, &answer[i], MAX_LENGTH);
        if (ret < 0)
        {
            printf("Error receiving an answer from the server!\n");
            return -1;
        }
        if (answer[i] == '\n')
        {
            break;
        }
    }*/
    //answer[i + 1] = 0;

    printf("Server's answer: %s\n", answer);

    char *answer_code = strndup(answer, 3);

    return atoi(answer_code);
}

int sendCommand(int sockfd, char *cmdName, char *cmdCont)
{
    char command[MAX_LENGTH];
    strcpy(command, cmdName);
    strcat(command, " ");
    strcat(command, cmdCont);
    printf("Sending command \"%s\"...\n", command);
    strcat(command, "\r\n");

    int ret = write(sockfd, command, strlen(command));
    return ret;
}
