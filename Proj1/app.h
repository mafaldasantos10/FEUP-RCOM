#ifndef APP_LAYER_H
#define APP_LAYER_H

struct applicationLayer
{
    int fileDescriptor; /*Descritor correspondente à porta série*/
    int status;         /*TRANSMITTER | RECEIVER*/
};
struct applicationLayer appLayer;

/* Function prototypes */
void checkArguments(int argc, char **argv);

#endif /* APP_LAYER_H */