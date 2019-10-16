#include "ll.h"

//Global variables
volatile int STOP = FALSE;
volatile int UA_RECEIVED = FALSE;
volatile int fd = 0;
volatile int num_return = 1;
unsigned char A_expected;
unsigned char C_expected;
unsigned char BCC_expected;

/* Handling alarm interruption */
void alarmHandler()
{
  if (UA_RECEIVED == TRUE)
  {
    num_return = 0;
    return;
  }

  if (num_return < MAX_RETURN)
  {
    /* Send SET frame again */
    unsigned char frame[FRAME_SIZE];
    frame[0] = FLAG;
    frame[1] = A_S;
    frame[2] = C_S;
    frame[3] = BCC_S;
    frame[4] = FLAG;
    writeFrame(frame);

    alarm(TIMEOUT);
    num_return++;
  }
  else
  {
    printf("\nError transmitting frames. Try again!\n");
    exit(1);
  }
}

int llopen(int porta, int channel)
{

  if (channel == TRANSMITTER)
  {
    transmitter(porta);
  }
  else if (channel == RECEIVER)
  {
    receiver(porta);
  }
  return 0;
}

int llwrite(int fd, char *buffer, int length)
{
  // Writes data frame to receiver
  unsigned char frame[FRAME_SIZE];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A;
  frame[C_INDEX] = C_I;
  frame[BCC_INDEX] = BCC1_I;

  for (unsigned int i = 0; i < length; i++)
  {
    frame[DATA_INDEX + i] = buffer[i];
  }

  frame[BCC2_INDEX + length - 1] = bcc2Calculator(buffer, length);
  frame[FLAG2_I_INDEX + length - 1] = FLAG;

  //byteStuffing(frame, FLAG2_I_INDEX + length);
  writeFrame(frame);

  // Reads receiver's acknowledging frame
  A_expected = A;
  C_expected = 0x085;
  BCC_expected = A ^ 0x085;

  readFrame(0, "");
}

int llread(int fd, char *buffer)
{
  // Reads transmitter's data frame
  A_expected = A;
  C_expected = C_I;
  BCC_expected = A ^ C_I;
  readFrame(1, buffer);

  // Writes acknowledging frame to transmitter
  unsigned char frame[FRAME_SIZE];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A_UA;
  frame[C_INDEX] = 0x85;
  frame[BCC_INDEX] = 0x85 ^ A_UA;
  frame[FLAG2_INDEX] = FLAG;

  writeFrame(frame);
  sleep(1);
}

int bcc2Calculator(char *buffer, int lenght)
{
  int bcc2 = buffer[0];
  for (unsigned int i = 1; i < lenght; i++)
  {
    bcc2 ^= buffer[i];
  }

  return bcc2;
}

void byteStuffing(unsigned char *frame, int length)
{
  unsigned char newFrame[MAX_BUF];
  unsigned int j = 0;

  for (int i = 0; i < length; i++, j++)
  {
    if (i < DATA_INDEX || i == length - 1)
    {
      newFrame[j] = frame[i];
      continue;
    }
    if (frame[i] == FLAG)
    {
      newFrame[j] = ESC;
      j++;
      newFrame[j] = FLAG_STUFFING;
    }
    else if (frame[i] == ESC)
    {
      newFrame[j] = ESC;
      j++;
      newFrame[j] = ESC_STUFFING;
    }
    else
    {
      newFrame[j] = frame[i];
    }
  }

  memcpy(frame, newFrame, j);
}

void setUP(int argc, char **argv, struct termios *oldtio)
{
  struct termios newtio;
  fflush(stdout);

  if ((argc < 3) ||
      ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
       (strcmp("/dev/ttyS1", argv[1]) != 0) &&
       (strcmp("/dev/ttyS2", argv[1]) != 0) &&
       (strcmp("/dev/ttyS3", argv[1]) != 0)) ||
      (atoi(argv[2]) != 0 && atoi(argv[2]) != 1))
  {
    printf("Usage:\tnserial SerialPort Channel\n\tex: nserial /dev/ttyS1 0\n\tChannel: 0 - Transmitter\t1 - Receiver\n");
    exit(1);
  }

  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open(argv[1], O_RDWR | O_NOCTTY);
  if (fd < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  if (tcgetattr(fd, oldtio) == -1)
  { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 1 char received */

  /*
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
  leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n\n");
  //fflush(stdout);
}

void readFrame(int operation, char *data)
{
  int resR;
  unsigned char buf[MAX_BUF];

  //state machine initialization
  enum startSt state;
  state = Start;
  int counter = 0;

  STOP = FALSE;

  while (STOP == FALSE)
  {                          /* loop for input */
    resR = read(fd, buf, 1); /* returns after 1 char has been input */
    //printf("\n buffer %x \n", buf[0]);
    if (operation == 0)
    {
      state = startUpStateMachine(state, buf);
    }
    else
    {
      state = dataStateMachine(state, buf, data, &counter);
    }
  }
}

void writeFrame(unsigned char frame[])
{
  int resW = write(fd, frame, FRAME_SIZE);
  printf("Sent frame with %x bytes.\n", resW);
}

int main(int argc, char **argv)
{
  char buff[MAX_BUF];
  struct termios oldtio;

  fflush(stdin);
  fflush(stdout);

  setUP(argc, argv, &oldtio);

  int channel = atoi(argv[2]);
  llopen(fd, channel);

  if (channel == TRANSMITTER)
  {
    char *buffer = "c~c}c";
    llwrite(fd, buffer, 6);
  }
  else
  {
    llread(fd, buff);
    printf("buff %s \n", buff);
  }

  tcsetattr(fd, TCSANOW, &oldtio);
  close(fd);
  return 0;
}

int receiver(int fd)
{

  /* Receive SET frame from transmitter*/
  A_expected = A_S;
  C_expected = C_S;
  BCC_expected = BCC_S;

  readFrame(0, "");

  /* Send UA frame to transmitter*/
  unsigned char frame[FRAME_SIZE];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A_UA;
  frame[C_INDEX] = C_UA;
  frame[BCC_INDEX] = BCC_UA;
  frame[FLAG2_INDEX] = FLAG;

  writeFrame(frame);
}

int transmitter(int fd)
{
  (void)signal(SIGALRM, alarmHandler);

  /* Send SET frame to receiver */
  unsigned char frame[FRAME_SIZE];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A_S;
  frame[C_INDEX] = C_S;
  frame[BCC_INDEX] = BCC_S;
  frame[FLAG2_INDEX] = FLAG;
  writeFrame(frame);

  alarm(TIMEOUT);

  /* Receive UA frame from receiver */
  A_expected = A_UA;
  C_expected = C_UA;
  BCC_expected = BCC_UA;
  readFrame(0, "");
}

enum startSt startUpStateMachine(enum startSt state, unsigned char *buf)
{
  switch (state)
  {

  case Start:
    if (*buf == FLAG)
    {
      state++;
    }
    else
    {
      state = Start;
    }
    break;

  case FlagRecieved:
    if (*buf == A_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      state = FlagRecieved;
    }
    else
    {
      state = Start;
    }
    break;

  case ARecieved:
    if (*buf == C_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      state = FlagRecieved;
    }
    else
    {
      state = Start;
    }
    break;

  case CRecieved:
    if (*buf == BCC_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      state = FlagRecieved;
    }
    else
    {
      state = Start;
    }
    break;

  case BCCok:
    if (*buf == FLAG)
    {
      state++;
      STOP = TRUE;
      printf("Received valid SET frame.\n");
    }
    else
    {
      state = Start;
    }
    break;
  }
  return state;
}

enum dataSt dataStateMachine(enum dataSt state, char *buf, char *data, int *counter)
{
  switch (state)
  {

  case StartData:
    // printf("startDta state: %x \n", *buf);
    if (*buf == FLAG)
    {
      state++;
    }
    else
    {
      state = StartData;
    }
    break;

  case FlagRecievedData:
    // printf("flagReceive state \n");
    if (*buf == A_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      state = FlagRecievedData;
    }
    else
    {
      state = StartData;
    }
    break;

  case ARecievedData:
    //printf("AReceive state \n");
    if (*buf == C_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      state = FlagRecievedData;
    }
    else
    {
      state = StartData;
    }
    break;

  case CRecievedData:
    // printf("cReceive state \n");
    if (*buf == BCC_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      state = FlagRecievedData;
    }
    else
    {
      state = StartData;
    }
    break;

  case BCCokData:
    //printf("bccok state \n");
    if (*buf == FLAG)
    {
      state = StartData;
    }
    else
    {
      data[*counter] = *buf;
      (*counter)++;
      state++;
    }
    break;

  case Data:
    // printf("data state num %d com valor %x \n", *counter, *buf);

    if (*buf == bcc2Calculator(data, *counter))
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      state = FlagRecievedData;
    }
    else
    {
      data[*counter] = *buf;
      (*counter)++;
    }

    break;

  case BCC2ok:
    //printf("bcc2ok state \n");

    if (*buf == FLAG)
    {
      STOP = TRUE;
      printf("Received valid Data frame %s.\n", data);
    }
    else
    {

      data[*counter] = bcc2Calculator(data, *counter);
      (*counter)++;

      if (*counter > 0 && *buf != bcc2Calculator(data, *counter))
      {
        data[*counter] = *buf;
        (*counter)++;
        state = Data;
      }
    }
    break;
  }
  return state;
}
