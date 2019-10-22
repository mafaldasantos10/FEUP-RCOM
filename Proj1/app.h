#ifndef APP_LAYER_H
#define APP_LAYER_H


#include "ll_app.h"

struct applicationLayer
{
    int fileDescriptor; /*Descritor correspondente à porta série*/
    int status;         /*TRANSMITTER | RECEIVER*/
    char* fileName;
};
struct applicationLayer appLayer;

/* Function prototypes */
void checkArguments(int argc, char **argv);
unsigned char *readFile(off_t *fileSize);
void createFile(unsigned char *data, off_t *sizeFile, char *filename);
void receiverApp();
void transmitterApp();
void writeDataPackage(unsigned char *package, int packCount, int packSize);
void writeControlPackage(int type, off_t fileSize);

#endif /* APP_LAYER_H */