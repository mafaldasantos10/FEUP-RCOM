#include "ll.h"

struct applicationLayer
{
    int fileDescriptor; /*Descritor correspondente à porta série*/
    int status;         /*TRANSMITTER | RECEIVER*/
};

struct applicationLayer appLayer;

void checkArguments(int argc, char **argv)
{
    if ((argc < 3) ||
        ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
         (strcmp("/dev/ttyS1", argv[1]) != 0) &&
         (strcmp("/dev/ttyS2", argv[1]) != 0) &&
         (strcmp("/dev/ttyS3", argv[1]) != 0)) ||
        ((strcmp("0", argv[2]) != 0) &&
         (strcmp("1", argv[2]) != 0)))
    {
        printf("Usage:\tnserial SerialPort State\n\tex: nserial /dev/ttyS1 0\n\tState: 0 - Transmitter\t1 - Receiver\n");
        exit(1);
    }
    appLayer.status = atoi(argv[2]);
}

int main(int argc, char **argv)
{
    char buff[MAX_BUF];

    checkArguments(argc, argv);

    int port = (int)argv[1][9] - 48;
    appLayer.fileDescriptor = llopen(port, appLayer.status);

    if (appLayer.fileDescriptor < 0)
    {
        printf("There was an error starting the tranference in llopen.\n");
        exit(-1);
    }

    /* Writes or reads according to application status */
    if (appLayer.status == TRANSMITTER)
    {
        char *buffer = "c~c}c";
        llwrite(appLayer.fileDescriptor, buffer, 6);
    }
    else
    {
        llread(appLayer.fileDescriptor, buff);
        printf("buff %s \n", buff);
    }

    return 0;
}
