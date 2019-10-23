#include "app.h"

volatile int END = FALSE;

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

    int numberPackages = fileSize / MAX_INF; //TODO: tratar em casos de resto n zero
    printf("number packages = %d\n", numberPackages);

    writeControlPackage(CONT_START, fileSize);

    printf("Depois start\n");

    for (int i = 0; i < numberPackages; i++)
    {
        unsigned char *package;
        int counter = 0;
        while (counter != MAX_INF)
        {
            package[counter] = fileData[j];
            j++;
            counter++;
        }

        writeDataPackage(package, i, MAX_INF);
        printf("Depois data\n");
    }

    writeControlPackage(CONT_END, fileSize);
    printf("Depois end\n");
    printf("number packages = %d\n", numberPackages);
}

void writeControlPackage(int type, off_t fileSize)
{
    unsigned char controlPackage[MAX_INF + 4];
    controlPackage[CONT_INDEX] = type;

    // File Size TLV
    controlPackage[FILE_SIZE_TYPE_INDEX] = FILE_SIZE_TYPE;
    controlPackage[FILE_SIZE_LENGTH_INDEX] = sizeof(fileSize);

    int i;
    for (i = 0; i < controlPackage[FILE_SIZE_LENGTH_INDEX]; i++)
    {
        controlPackage[FILE_SIZE_VALUE_INDEX + i] = (fileSize >> (i * 8)) & 0xFF; // writes first the least significant bits
    }

    // File Name TLV
    controlPackage[FILE_NAME_TYPE_INDEX + i] = FILE_NAME_TYPE;
    controlPackage[FILE_NAME_LENGTH_INDEX + i] = strlen(appLayer.fileName);

    int j;
    for (j = 0; j < controlPackage[FILE_NAME_LENGTH_INDEX + i]; j++)
    {
        controlPackage[FILE_NAME_VALUE_INDEX + i + j] = appLayer.fileName[j];
    }

    // Sends control package
    while (llwrite(appLayer.fileDescriptor, controlPackage, MAX_INF + 4) == -1)
    {
    }
}

void writeDataPackage(unsigned char *package, int packCount, int packSize)
{
    unsigned char dataPackage[MAX_INF + 4];
    dataPackage[CONT_INDEX] = CONT_DATA;
    dataPackage[N_SEQ_INDEX] = packCount % 255;
    dataPackage[L2_INDEX] = packSize / 255;
    dataPackage[L1_INDEX] = packSize % 255;

    printf("N_Sequence = %x\n", dataPackage[N_SEQ_INDEX]);

    // Data
    for (unsigned int i = 0; i < packSize; i++)
    {
        dataPackage[PACKAGE_INDEX + i] = package[i];
    }

    // Sends data package
    while (llwrite(appLayer.fileDescriptor, dataPackage, MAX_INF + 4) == -1)
    {
    }
    for (int k = 0; k < MAX_INF + 4; k++)
    {
        printf("DataPackage = %d\n", dataPackage[k]);
    }
}

void receiverApp()
{
    int packSize, numbPack = 0;
    off_t fileSize = 0;
    int begin = FALSE;

    unsigned char *fileData = malloc(0);
    off_t sizeCounter = 0;

    while (begin == FALSE)
    {
        unsigned char buff[MAX_INF + 4];
        packSize = llread(appLayer.fileDescriptor, buff);
        printf("buff %s \n", buff);

        if (packSize < 0)
        {
            printf("Error reading package or received wrong package...\n");
            continue;
        }

        fileSize = parseStartPackage(buff);

        if (fileSize > 0)
        {
            begin = TRUE;
        }
        else
        {
            printf("Error in start control package!\n");
            exit(-1);
        }
    }

    while (END == FALSE)
    {
        unsigned char buff[MAX_INF + 4];
        unsigned char package[MAX_INF + 4];
        int packFinalSize = 0;
        packSize = llread(appLayer.fileDescriptor, buff);
        printf("buff %s \n", buff);

        if (packSize < 0)
        {
            printf("Error reading package or received wrong package...\n");
            continue;
        }

        if (parsePackage(buff, packSize, numbPack, package, &packFinalSize) < 0)
        {
            printf("Whatever package\n");
            exit(-1);
        }

        numbPack++;
        printf("Adding package to data %x de %lx\n", packFinalSize, fileSize);
        // Add package to final data
        sizeCounter += packFinalSize;
        printf("Before realloc - size counter: %lx\n", sizeCounter);
        fileData = (unsigned char *)realloc(fileData, sizeCounter);
        printf("Before memcpy\n");
        memcpy(fileData + sizeCounter - packFinalSize, package, packFinalSize);
        printf("After memcpy\n");
    }
    if (fileSize != 0)
    {
        createFile(fileData, &fileSize);
    }
}

off_t parseStartPackage(unsigned char *buff)
{
    off_t fileSize = 0;

    if (buff[CONT_INDEX] != CONT_START)
    {
        return -1;
    }

    // Parses file length
    int fileSizeLength = buff[FILE_SIZE_LENGTH_INDEX];

    if (buff[FILE_SIZE_TYPE_INDEX] == FILE_SIZE_TYPE)
    {
        for (unsigned int i = 0; i < fileSizeLength; i++)
        {
            fileSize |= buff[FILE_SIZE_VALUE_INDEX + i] << (i * 8); // gets first the least significant bits
        }
    }
    else
    {
        printf("File size is not defined...\n");
        return -1;
    }

    // Parses file name
    if (buff[fileSizeLength + FILE_NAME_TYPE_INDEX] == 1)
    {
        for (unsigned int i = 0; i < buff[fileSizeLength + FILE_NAME_LENGTH_INDEX]; i++)
        {
            appLayer.fileName[i] = buff[fileSizeLength + FILE_NAME_VALUE_INDEX + i];
        }
    }
    else
    {
        printf("File name is not defined...\n");
        return -1;
    }

    return fileSize;
}

int parsePackage(unsigned char *buff, int packSize, int numbPack, unsigned char *package, int *packFinalSize)
{
    printf("CONT WHaT = %x\n", buff[CONT_INDEX]);
    if (buff[CONT_INDEX] == CONT_DATA)
    {
        printf("N_Sequence = %x\n", buff[N_SEQ_INDEX]);
        if (buff[N_SEQ_INDEX] != numbPack % 0x0FF)
        {
            printf("Wrong sequence number! A package was possibly lost.\n");
            return -1;
        }

        *packFinalSize = 256 * buff[L2_INDEX] + buff[L1_INDEX];
        package = (unsigned char *)malloc(*packFinalSize);

        for (unsigned int i = 0; i < *packFinalSize; i++)
        {
            package[i] = buff[PACKAGE_INDEX + i];
        }
        return 0;
    }
    else if (buff[CONT_INDEX] == CONT_END)
    {
        // checkEndPackage(buff);
        printf("ENNNND!\n");
        END = TRUE;
        return 0;
    }
    else
    {
        printf("Wrong control field!\n");
        return -1;
    }
}

void createFile(unsigned char *data, off_t *sizeFile)
{
    appLayer.fileName = "pinguim2.gif";
    FILE *file = fopen(appLayer.fileName, "wb+");
    fwrite((void *)data, 1, *sizeFile, file);
    printf("New file %s created\n", appLayer.fileName);
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
