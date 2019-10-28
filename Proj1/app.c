#include "app.h"

volatile int MAX_INF = (MAX_BUF - 7) / 2 - 4; // Max data bytes that can be sent in each package
volatile int END = FALSE;                     // TRUE if end control package was received, FALSE otherwise

int main(int argc, char **argv)
{
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
    // Reads file bytes and saves its size
    struct stat data;
    unsigned char *fileData = readFile(&data);
    off_t fileSize = data.st_size;

    // Gets number of packages to send
    int numberPackages = fileSize / MAX_INF;
    int remainder = fileSize % MAX_INF;
    if (remainder != 0)
    {
        numberPackages++;
    }

    // Writes start control package
    printf("Sending start control package\n");
    writeControlPackage(CONT_START, fileSize);

    // Writes data packages
    int packSize = MAX_INF; // Package size
    int j = 0;              // File data index

    for (int i = 0; i < numberPackages; i++)
    {
        // Changes last package size if it's not the max value allowed
        if (i == numberPackages - 1)
        {
            if (remainder != 0)
            {
                packSize = remainder;
            }
        }

        unsigned char package[packSize + 4];
        int counter = 0;
        while (counter != packSize)
        {
            package[counter] = fileData[j];
            j++;
            counter++;
        }

        printf("Sending package %d of %d\n", i + 1, numberPackages);
        writeDataPackage(package, i, packSize);
    }

    // Write end control package
    printf("Sending end control package\n");
    writeControlPackage(CONT_END, fileSize);
}

void writeControlPackage(int type, off_t fileSize)
{
    unsigned char controlPackage[MAX_INF + 4];

    // Type of package
    controlPackage[CONT_INDEX] = type;

    // File Size TLV
    controlPackage[FILE_SIZE_TYPE_INDEX] = FILE_SIZE_TYPE;
    controlPackage[FILE_SIZE_LENGTH_INDEX] = sizeof(fileSize);

    int i;
    for (i = 0; i < controlPackage[FILE_SIZE_LENGTH_INDEX]; i++)
    {
        controlPackage[FILE_SIZE_VALUE_INDEX + i] = (fileSize >> (i * 8)) & 0xFF; // writes the least significant bits first
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
    while (llwrite(appLayer.fileDescriptor, (char*)controlPackage, FILE_NAME_VALUE_INDEX + i + j) == -1)
    {
    }
}

void writeDataPackage(unsigned char *package, int packCount, int packSize)
{
    unsigned char dataPackage[packSize + 4];

    // Package Header
    dataPackage[CONT_INDEX] = CONT_DATA;
    dataPackage[N_SEQ_INDEX] = packCount % 255;
    dataPackage[L2_INDEX] = packSize / 255;
    dataPackage[L1_INDEX] = packSize % 255;

    // Data
    for (int i = 0; i < packSize; i++)
    {
        dataPackage[PACKAGE_INDEX + i] = package[i];
    }

    // Sends data package
    while (llwrite(appLayer.fileDescriptor, (char*)dataPackage, packSize + 4) == -1)
    {
    }
}

void receiverApp()
{
    int packSize;       // Read package size
    int numbPack = 0;   // Received packages counter
    off_t fileSize = 0; // Received file size
    int begin = FALSE;  // TRUE if begin control package received, FALSE otherwise

    unsigned char *fileData = malloc(0); // Total file data read
    off_t sizeCounter = 0;               // Size of total file data read

    while (begin == FALSE)
    {
        unsigned char buff[MAX_INF + 4];

        printf("Waiting for start control package...\n");
        packSize = llread(appLayer.fileDescriptor, (char*)buff);
        if (packSize < 0)
        {
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
        int packDataSize = 0; // Read data size

        printf("Waiting for package %d\n", numbPack + 1);
        packSize = llread(appLayer.fileDescriptor, (char*)buff);

        if (packSize < 0)
        {
            continue;
        }

        unsigned char *package = parsePackage(buff, numbPack, &packDataSize);

        // Adds package to final data array
        sizeCounter += packDataSize;
        fileData = (unsigned char *)realloc(fileData, sizeCounter);
        memcpy(fileData + sizeCounter - packDataSize, package, packDataSize);

        numbPack++;
    }
    if (fileSize != 0)
    {
        createFile(fileData, &fileSize);
    }
}

off_t parseStartPackage(unsigned char *buff)
{
    startPack.fileSize = 0; // Received file size

    if (buff[CONT_INDEX] != CONT_START)
    {
        return -1;
    }

    // Parses file length
    startPack.fileSizeLength = buff[FILE_SIZE_LENGTH_INDEX];

    if (buff[FILE_SIZE_TYPE_INDEX] == FILE_SIZE_TYPE)
    {
        for (int i = 0; i < startPack.fileSizeLength; i++)
        {
            startPack.fileSize |= buff[FILE_SIZE_VALUE_INDEX + i] << (i * 8); // gets first the least significant bits
        }
    }
    else
    {
        return -1;
    }

    // Parses file name
    startPack.fileNameLength = buff[startPack.fileSizeLength + FILE_NAME_LENGTH_INDEX];

    if (buff[startPack.fileSizeLength + FILE_NAME_TYPE_INDEX] == 1)
    {
        for (int i = 0; i < startPack.fileNameLength; i++)
        {
            appLayer.fileName[i] = buff[startPack.fileSizeLength + FILE_NAME_VALUE_INDEX + i];
        }
    }
    else
    {
        printf("File name is not defined...\n");
        return -1;
    }

    return startPack.fileSize;
}

unsigned char *parsePackage(unsigned char *buff, int numbPack, int *packDataSize)
{
    unsigned char *package;

    if (buff[CONT_INDEX] == CONT_DATA)
    {
        if (buff[N_SEQ_INDEX] != numbPack % 255)
        {
            printf("Wrong sequence number! A package was possibly lost.\n");
            exit(-1);
        }

        *packDataSize = 255 * buff[L2_INDEX] + buff[L1_INDEX];
        package = (unsigned char *)malloc(*packDataSize);

        for (int i = 0; i < *packDataSize; i++)
        {
            package[i] = buff[PACKAGE_INDEX + i];
        }

        return package;
    }
    else if (buff[CONT_INDEX] == CONT_END)
    {
        checkEndPackage(buff);
        END = TRUE;
        printf("Received end control package!\n\n");

        return (unsigned char*)"";
    }
    else
    {
        printf("Wrong control field!\n");
        exit(-1);
    }
}

void checkEndPackage(unsigned char *buff)
{
    /* Checks File Size */
    off_t fileSize = 0; // Received file size

    // Checks file size length
    if (startPack.fileSizeLength != buff[FILE_SIZE_LENGTH_INDEX])
    {
        printf("Wrong file size length.\n");
        exit(-1);
    }

    // Checks file size type
    if (buff[FILE_SIZE_TYPE_INDEX] == FILE_SIZE_TYPE)
    {
        for (int i = 0; i < startPack.fileSizeLength; i++)
        {
            fileSize |= buff[FILE_SIZE_VALUE_INDEX + i] << (i * 8);
        }

        // Checks file size
        if (startPack.fileSize != fileSize)
        {
            printf("Wrong file size.\n");
            exit(-1);
        }
    }
    else
    {
        printf("File size is not defined...\n");
        exit(-1);
    }

    /* Checks File Name */
    char *fileName; // Received file name

    // Checks file name length
    if (startPack.fileNameLength != buff[startPack.fileSizeLength + FILE_NAME_LENGTH_INDEX])
    {
        printf("Wrong file name length.\n");
        exit(-1);
    }

    // Checks file name type
    if (buff[startPack.fileSizeLength + FILE_NAME_TYPE_INDEX] == FILE_NAME_TYPE)
    {
        fileName = (char *)malloc(buff[startPack.fileSizeLength + FILE_NAME_LENGTH_INDEX]);
        for (unsigned int i = 0; i < buff[startPack.fileSizeLength + FILE_NAME_LENGTH_INDEX]; i++)
        {
            fileName[i] = buff[startPack.fileSizeLength + FILE_NAME_VALUE_INDEX + i];
        }

        // Checks file name
        if (strcmp(appLayer.fileName, fileName) != 0)
        {
            printf("Wrong file name.\n");
            exit(-1);
        }
    }
    else
    {
        printf("File name is not defined...\n");
        exit(-1);
    }
}

void createFile(unsigned char *data, off_t *sizeFile)
{
    FILE *file = fopen(appLayer.fileName, "wb+");

    // Writes file bytes read
    fwrite((void *)data, 1, *sizeFile, file);
    printf("*New file %s created*\n\n", appLayer.fileName);
    fclose(file);
}

unsigned char *readFile(struct stat *data)
{
    FILE *file;

    if ((file = fopen(((const char *)appLayer.fileName), "rb")) == NULL)
    {
        perror("Error while opening the file\n");
        exit(1);
    }

    // Gets the file metadata
    stat(appLayer.fileName, data);

    // Gets file size in bytes
    off_t fileSize = data->st_size;

    unsigned char *fileData = (unsigned char *)malloc(fileSize);

    // Reads file bytes
    fread(fileData, sizeof(unsigned char), fileSize, file);
    if (ferror(file))
    {
        perror("Error writting to file\n");
    }
    fclose(file);

    return fileData;
}

void checkArguments(int argc, char **argv)
{
    if ((argc < 3) ||
        ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
         (strcmp("/dev/ttyS1", argv[1]) != 0) &&
         (strcmp("/dev/ttyS2", argv[1]) != 0) &&
         (strcmp("/dev/ttyS3", argv[1]) != 0)) ||
        ((strcmp("0", argv[2]) != 0) &&
         (strcmp("1", argv[2]) != 0)) ||
        ((strcmp("0", argv[2]) == 0) && argc != 4) || ((strcmp("1", argv[2]) == 0) && argc != 3))
    {
        printf("Usage:\tnserial SerialPort State [FileName]\n\tex: nserial /dev/ttyS1 0 pinguim.gif\n\t    nserial /dev/ttyS1 1\n\tState: 0-Transmitter\t1-Receiver\n");
        exit(1);
    }
    appLayer.status = atoi(argv[2]);
    if (appLayer.status == 0)
    {
        memcpy(appLayer.fileName, argv[3], strlen(argv[3]));
    }
}
