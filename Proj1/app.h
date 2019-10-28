#ifndef APP_LAYER_H
#define APP_LAYER_H

#include "ll_app.h"
#include <sys/types.h>

/* Data Package */
#define CONT_DATA 1
#define CONT_START 2
#define CONT_END 3

#define CONT_INDEX 0
#define N_SEQ_INDEX 1
#define L2_INDEX 2
#define L1_INDEX 3
#define PACKAGE_INDEX 4

/* Control Package */
#define FILE_SIZE_TYPE 0x00
#define FILE_NAME_TYPE 0x01

#define FILE_SIZE_TYPE_INDEX 1
#define FILE_SIZE_LENGTH_INDEX 2
#define FILE_SIZE_VALUE_INDEX 3
#define FILE_NAME_TYPE_INDEX 3
#define FILE_NAME_LENGTH_INDEX 4
#define FILE_NAME_VALUE_INDEX 5

struct applicationLayer
{
    int fileDescriptor;     /*Descritor correspondente à porta série*/
    int status;             /*TRANSMITTER | RECEIVER*/
    char fileName[MAX_BUF]; /*File name*/
};
struct applicationLayer appLayer;

struct startPackage
{
    off_t fileSize;
    int fileSizeLength;
    int fileNameLength;
};
struct startPackage startPack;

/* Function prototypes */
void checkArguments(int argc, char **argv);
unsigned char *readFile(struct stat *data);
void createFile(unsigned char *data, off_t *sizeFile);
void receiverApp();
void transmitterApp();
void writeDataPackage(unsigned char *package, int packCount, int packSize);
void writeControlPackage(int type, off_t fileSize);
unsigned char *parsePackage(unsigned char *buff, int numbPack, int *packFinalSize);
off_t parseStartPackage(unsigned char *buff);
void checkEndPackage(unsigned char *buff);

#endif /* APP_LAYER_H */
