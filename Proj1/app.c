#include "app.h"


int main(int argc, char **argv)
{
    checkArguments(argc, argv);

    int port = (int)argv[1][9] - 48;
    appLayer.fileDescriptor = llopen(port, appLayer.status);

    printf("MAX INF = %d\n", MAX_INF);
    if (appLayer.fileDescriptor < 0)
    {
        printf("There was an error starting the tranference in llopen.\n");
        exit(-1);
    }

    /* Writes or reads according to application status */
    if (appLayer.status == TRANSMITTER)
    {
        transmitterApp();
    }
    else
    {
        receiverApp();
    }

    llclose(appLayer.fileDescriptor);

    return 0;
}

void transmitterApp()
{
    off_t fileSize;
    unsigned char *fileData = readFile(&fileSize);
    int j = 0;

    int numberPackages = fileSize / MAX_INF;
    printf("number packages = %d\n", numberPackages);

    writeControlPackage(2, fileSize);

    for (int i = 0; i < numberPackages; i++)
    {
        unsigned char *package;
        int counter = 0;
        while (counter != MAX_BUF)
        {
            package[counter] = fileData[j];
            j++;
            counter++;
        }

        writeDataPackage(package, i, MAX_INF);
    }

     writeControlPackage(3, fileSize);
}

void writeControlPackage(int type, off_t fileSize){
    unsigned char *controlPackage;
    controlPackage[0] = type;
    controlPackage[1] = 0x00;
    controlPackage[2] = sizeof(fileSize);
    controlPackage[3] = fileSize;   //sparar byte a byte

    controlPackage[4] = 0x01;
    controlPackage[5] = strlen(appLayer.fileName);
    controlPackage[6] = appLayer.fileName;  //sparar byte a byte
}

void writeDataPackage(unsigned char *package, int packCount, int packSize)
{
    unsigned char *dataPackage;
    dataPackage[0] = 1;
    dataPackage[1] = packCount % 255;
    dataPackage[2] = packSize / 255;
    dataPackage[3] = packSize % 255;

    for (unsigned int i = 0; i < packSize; i++)
    {
        dataPackage[4 + i] = package[i];
    }

    while (llwrite(appLayer.fileDescriptor, dataPackage, MAX_INF + 4) == -1){
    }
}

void receiverApp()
{
    char buff[MAX_BUF];
    llread(appLayer.fileDescriptor, buff);
    printf("buff %s \n", buff);
}

void createFile(unsigned char *data, off_t *sizeFile, char *filename)
{
    FILE *file = fopen(filename, "wb+");
    fwrite((void *)data, 1, *sizeFile, file);
    printf("New file %s created\n", filename);
    fclose(file);
}

unsigned char *readFile(off_t *fileSize)
{
    FILE *file;
    struct stat data;

    if ((file = fopen(((const char *)appLayer.fileName), "rb")) == NULL)
    {
        perror("Error while opening the file");
        exit(1);
    }

    //get the file metadata
    stat(appLayer.fileName, &data);

    //gets file size in bytes
    *fileSize = data.st_size;

    unsigned char *fileData = (unsigned char *)malloc(*fileSize);

    fread(fileData, sizeof(unsigned char), *fileSize, file);
    if (ferror(file))
    {
        perror("error writting to file\n");
    }
    fclose(file);
    return fileData;
}

void checkArguments(int argc, char **argv)
{
    if ((argc < 4) ||
        ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
         (strcmp("/dev/ttyS1", argv[1]) != 0) &&
         (strcmp("/dev/ttyS2", argv[1]) != 0) &&
         (strcmp("/dev/ttyS3", argv[1]) != 0)) ||
        ((strcmp("0", argv[2]) != 0) &&
         (strcmp("1", argv[2]) != 0)))
    {
        printf("Usage:\tnserial SerialPort State FileName\n\tex: nserial /dev/ttyS1 0 pinguim.gif\n\tState: 0 - Transmitter\t1 - Receiver\n");
        exit(1);
    }
    appLayer.status = atoi(argv[2]);
    appLayer.fileName = argv[3];
}
