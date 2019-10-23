#ifndef LINK_LAYER_H
#define LINK_LAYER_H


#include "ll_app.h"

#define BIT(n) (0x01 << (n))

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */

/* Alarm information */
#define MAX_RETURN 3
#define TIMEOUT 3

/* Frame information*/
#define FLAG 0x7E
#define A 0x03

/* Byte stuffing */
#define ESC 0x7D
#define FLAG_STUFFING 0x5E
#define ESC_STUFFING 0x5D

/* Indexes in frame */
#define FLAG_INDEX 0
#define A_INDEX 1
#define C_INDEX 2
#define BCC_INDEX 3
#define FLAG2_INDEX 4

#define DATA_INDEX 4
#define BCC2_INDEX 5
#define FLAG2_I_INDEX 6

/* SET frame*/
#define A_S 0X03
#define C_S 0X03
#define BCC_S (A_S ^ C_S)

/* UA frame*/
#define A_UA 0X03
#define C_UA 0X07
#define BCC_UA (A_UA ^ C_UA)

/* I frame*/
#define C_I 0X00 // BIT(6) to change N(s)
#define BCC1_I (A ^ C_I)

/* RR frame*/
#define C_RR 0X05 // BIT(7) to change N(r)
#define BCC_RR (A ^ C_RR)

/* REJ frame*/
#define C_REJ 0X01 // BIT(7) to change N(r)
#define BCC_REJ (A ^ C_REJ)

/* DISC frame*/
#define C_DISC 0X0B
#define BCC_DISC (A ^ C_DISC)
/*  */

/* State Machines */
// State machine for type I frames
enum startSt
{
    Start,
    FlagRecieved,
    ARecieved,
    CRecieved,
    BCCok
};

// State machine for type S and U frames
enum dataSt
{
    StartData,
    FlagRecievedData,
    ARecievedData,
    CRecievedData,
    BCCokData,
    Data,
    DestuffingData,
    BCC2ok
};
/*  */

struct linkLayer
{
    char port[20]; /*Dispositivo /dev/ttySx, x = 0, 1*/
    struct termios oldtio;
    int status;     /* TRANSMITTER | RECEIVER */
    unsigned int sequenceNumber;  /*Número de sequência da trama: 0, 1*/
    unsigned char frame[MAX_BUF]; /*Trama*/
};

struct linkLayer linkStruct;

/* Function prototypes */
void setUP(int porta);
int receiver();
int transmitter();
int receiverClose();
int transmitterClose();
int readFrame(int operation, char *data, int * counter);
void writeFrame(unsigned char frame[]);
int bcc2Calculator(unsigned char *buffer, int lenght);
void byteStuffing(unsigned char *frame, int length);
enum startSt startUpStateMachine(enum startSt state, unsigned char *buf);
enum dataSt dataStateMachine(enum dataSt state, unsigned char *buf, unsigned char *data, int *counter);

#endif /* LINK_LAYER_H */
