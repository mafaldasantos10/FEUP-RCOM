#include "ll.h"
#include "ll_app.h"

//Global variables
volatile int STOP = FALSE;
volatile int SET_ON = FALSE;
volatile int FRAME_RECEIVED = FALSE;
volatile int DUPLICATE = FALSE;
volatile int fileDescriptor = 0;
volatile int num_return = 1;
unsigned char A_expected;
unsigned char C_expected;
unsigned char BCC_expected;
int flag = 1;

/* Handling alarm interruption */
void alarmHandler()
{
  flag = 1;

  if (FRAME_RECEIVED == TRUE)
  {
    flag = 1;
    num_return = 0;
    return;
  }

  if (num_return < MAX_RETURN)
  {
    /* Send SET frame again */
    writeFrame(linkStruct.frame);

    alarm(TIMEOUT);
    num_return++;
  }
  else
  {
    printf("\nError transmitting frames. Try again!\n");
    exit(-1);
  }
}

int llopen(int porta, int status)
{
  linkStruct.sequenceNumber = 0;
  setUP(porta);

  if (status == TRANSMITTER)
  {
    transmitter();
  }
  else if (status == RECEIVER)
  {
    receiver();
  }

  return fileDescriptor;
}

void setUP(int porta)
{
  struct termios newtio;

  char portaStr[2];
  sprintf(portaStr, "%d", porta);
  memcpy(linkStruct.port, "/dev/ttyS", 10);
  strcat(linkStruct.port, portaStr);

  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */
  fileDescriptor = open(linkStruct.port, O_RDWR | O_NOCTTY);
  if (fileDescriptor < 0)
  {
    perror(linkStruct.port);
    exit(-1);
  }

  if (tcgetattr(fileDescriptor, &linkStruct.oldtio) == -1)
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

  tcflush(fileDescriptor, TCIOFLUSH);

  if (tcsetattr(fileDescriptor, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n\n");
}

int receiver()
{
  printf("Entrei no receiver\n");
  int count_ignore = 0;
  /* Receive SET frame from transmitter*/
  A_expected = A_S;
  C_expected = C_S;
  BCC_expected = BCC_S;

  while (readFrame(0, "", &count_ignore) != 0)
  {
  }

  /* Send UA frame to transmitter*/
  unsigned char frame[FRAME_SIZE];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A_UA;
  frame[C_INDEX] = FLAG;
  frame[BCC_INDEX] = BCC_UA;
  frame[FLAG2_INDEX] = FLAG;

  writeFrame(frame);
  printf("Sai no receiver\n");
}

int transmitter()
{
  printf("Entrei no transmitter\n");
  (void)signal(SIGALRM, alarmHandler);

  while (FRAME_RECEIVED == FALSE)
  {
    if (flag == 1)
    {
      int count_ignore = 0;

      /* Send SET frame to receiver */
      linkStruct.frame[FLAG_INDEX] = FLAG;
      linkStruct.frame[A_INDEX] = A_S;
      linkStruct.frame[C_INDEX] = C_S;
      linkStruct.frame[BCC_INDEX] = BCC_S;
      linkStruct.frame[FLAG2_INDEX] = FLAG;
      writeFrame(linkStruct.frame);

      alarm(TIMEOUT);
      printf("num_ret = %x\n", num_return);

      /* Receive UA frame from receiver */
      A_expected = A_UA;
      C_expected = C_UA;
      BCC_expected = BCC_UA;

      readFrame(0, "", &count_ignore);
      flag = 0;
    }
  }
  alarm(0);
  printf("Sai no transmitter\n");
}

int llwrite(int fd, char *buffer, int length)
{
  printf("Entrei no llwrite\n");
  FRAME_RECEIVED = FALSE;
  fileDescriptor = fd;
  flag = 1;

  while (FRAME_RECEIVED == FALSE)
  {
    if (flag == 1)
    {
      printf("Entrei na flag\n");
      int count_ignore = 0;

      int what = 0;

      // Writes data frame to receiver
      linkStruct.frame[FLAG_INDEX] = FLAG;
      linkStruct.frame[A_INDEX] = A;
      linkStruct.frame[C_INDEX] = C_I | (linkStruct.sequenceNumber << 6);
      linkStruct.frame[BCC_INDEX] = A ^ linkStruct.frame[C_INDEX];

      for (unsigned int i = 0; i < length; i++)
      {
        linkStruct.frame[DATA_INDEX + i] = buffer[i];
      }

      linkStruct.frame[BCC2_INDEX + length - 1] = bcc2Calculator(buffer, length);
      linkStruct.frame[FLAG2_I_INDEX + length - 1] = FLAG;

      byteStuffing(linkStruct.frame, FLAG2_I_INDEX + length);
      writeFrame(linkStruct.frame);

      alarm(TIMEOUT);

      // Reads receiver's acknowledging frame
      A_expected = A;
      C_expected = C_RR | (((linkStruct.sequenceNumber + 1) % 2) << 7);
      BCC_expected = A ^ C_expected;
      printf("A %x\n", A_expected);
      printf("C %x\n", C_expected);
      printf("BCC %x\n", BCC_expected);
      what = readFrame(0, "", &count_ignore);
      printf("What? %x\n", what);
      flag = 0;
    }
  }

  linkStruct.sequenceNumber = (linkStruct.sequenceNumber + 1) % 2;
  printf("Sai no llwrite\n");

  return length;
}

int llread(int fd, char *buffer)
{
  printf("Entrei no llread\n");
  fileDescriptor = fd;
  int length = 0;
  // Reads transmitter's data frame
  A_expected = A;
  C_expected = C_I | (linkStruct.sequenceNumber << 6);
  BCC_expected = A ^ C_expected;
  printf("sequenceNumber = %x\n", linkStruct.sequenceNumber);
  unsigned char frame[FRAME_SIZE];
  if (readFrame(1, buffer, &length) == 0)
  {
    if (SET_ON == TRUE)
    {
      /* Send UA frame to transmitter*/
      frame[FLAG_INDEX] = FLAG;
      frame[A_INDEX] = A_UA;
      frame[C_INDEX] = C_UA;
      frame[BCC_INDEX] = BCC_UA;
      frame[FLAG2_INDEX] = FLAG;
      length = -1;
    }
    else
    {
      linkStruct.sequenceNumber = (linkStruct.sequenceNumber + 1) % 2;
      // Writes acknowledging frame to transmitter
      frame[FLAG_INDEX] = FLAG;
      frame[A_INDEX] = A_UA;
      frame[C_INDEX] = C_RR | (linkStruct.sequenceNumber << 7);
      frame[BCC_INDEX] = A_UA ^ frame[C_INDEX];
      frame[FLAG2_INDEX] = FLAG;

      if (DUPLICATE == TRUE)
      {
        printf("Foudn dup\n");
        length = -1;
      }
    }
  }
  else
  {
    linkStruct.sequenceNumber = (linkStruct.sequenceNumber + 1) % 2;
    // Writes rejected acknowledging frame to transmitter
    frame[FLAG_INDEX] = FLAG;
    frame[A_INDEX] = A_UA;
    frame[C_INDEX] = C_REJ | (linkStruct.sequenceNumber << 7);
    frame[BCC_INDEX] = A_UA ^ frame[C_INDEX];
    frame[FLAG2_INDEX] = FLAG;

    length = -1;
  }

  printf("%x C \n", frame[C_INDEX]);
  printf("%x BCC\n", frame[BCC_INDEX]);
  writeFrame(frame);
  sleep(1);

  printf("Sai no llread\n");
  return length;
}

int llclose(int fd)
{
  tcsetattr(fd, TCSANOW, &linkStruct.oldtio);
  close(fd);

  return 0;
}

int bcc2Calculator(unsigned char *buffer, int lenght)
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

int readFrame(int operation, char *data, int *counter)
{
  int resR;
  int max_buf = 0;
  unsigned char buf[MAX_BUF];

  //state machine initialization
  enum startSt state;
  state = Start;
  *counter = 0;

  STOP = FALSE;
  SET_ON = FALSE;
  DUPLICATE = FALSE;

  while (STOP == FALSE && max_buf != MAX_BUF)
  {                                      /* loop for input */
    resR = read(fileDescriptor, buf, 1); /* returns after 1 char has been input */
    max_buf++;
    //printf("Max_buf = %d\n", max_buf);

    if (operation == 0)
    {
      state = startUpStateMachine(state, buf);
    }
    else
    {
      state = dataStateMachine(state, buf, data, counter);
    }
  }

  if (SET_ON == TRUE || DUPLICATE == TRUE || FRAME_RECEIVED == TRUE)
  {
    printf("Here 0 \n");
    return 0;
  }
  else
  {
    printf("Here 1 \n");
    return 1;
  }
}

void writeFrame(unsigned char frame[])
{
  int resW = write(fileDescriptor, frame, FRAME_SIZE);
  printf("Sent frame with %x bytes.\n", resW);
}

enum startSt startUpStateMachine(enum startSt state, unsigned char *buf)
{
  switch (state)
  {

  case Start:
    //printf("Started\n");
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
    printf("FlagRecieved\n");
    if (*buf == A_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else
    {
      state = Start;
    }
    break;

  case ARecieved:
    printf("ARecieved\n");
    if (*buf == C_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else if (*buf == (C_REJ | (((linkStruct.sequenceNumber + 1) % 2) << 7)) || *buf == C_UA)
    {
      STOP = TRUE;
      flag = 1;
    }
    else
    {
      state = Start;
    }
    break;

  case CRecieved:
    printf("CRecieved\n");
    if (*buf == BCC_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else
    {
      state = Start;
    }
    break;

  case BCCok:
    printf("BCCok\n");
    if (*buf == FLAG)
    {
      STOP = TRUE;
      FRAME_RECEIVED = TRUE;
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

enum dataSt dataStateMachine(enum dataSt state, unsigned char *buf, unsigned char *data, int *counter)
{
  switch (state)
  {

  case StartData:
    printf("startDta state: %x \n", *buf);
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
    printf("flagReceive state \n");
    if (*buf == A_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else
    {
      state = StartData;
    }
    break;

  case ARecievedData:
    printf("AReceive state c = %x \n", (C_I | (linkStruct.sequenceNumber << 6)));
    printf("C_expected = %x", C_expected);
    printf("buf = %x", *buf);
    if (*buf == C_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else if (*buf == C_S)
    {
      SET_ON = TRUE;
      STOP = TRUE;
    }
    else if (*buf == (C_I | (linkStruct.sequenceNumber << 6)))
    {
      printf("ENtreu dupdup with c =\n");
      DUPLICATE = TRUE;
      STOP = TRUE;
    }
    else
    {
      state = StartData;
    }
    break;

  case CRecievedData:
    printf("cReceive state \n");
    if (*buf == BCC_expected)
    {
      state++;
    }
    else if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else
    {
      state = StartData;
    }
    break;

  case BCCokData:
    printf("bccok state \n");
    if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else
    {
      data[*counter] = *buf;
      (*counter)++;
      state++;
    }
    break;

  case Data:
    printf("data state num %d com valor %x \n", *counter, *buf);

    if (*buf == bcc2Calculator(data, *counter))
    {
      state = BCC2ok;
    }
    else if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else if (*buf == ESC)
    {
      //printf("data recebe esc");
      state++;
    }
    else
    {
      data[*counter] = *buf;
      (*counter)++;
    }

    break;

  case DestuffingData:
    //printf("destuffing \n");
    if (*buf == FLAG_STUFFING)
    {
      //printf("== flag stuffing \n");
      if (FLAG == bcc2Calculator(data, *counter))
      {
        //printf("para o bcc2 \n");
        state = BCC2ok;
      }
      else
      {
        //printf("= flag para a data");
        data[*counter] = FLAG;
        (*counter)++;
        state = Data;
      }
    }
    else if (*buf == ESC_STUFFING)
    {
      //printf("== esc stuffing \n");
      if (ESC == bcc2Calculator(data, *counter))
      {
        //printf("para o bcc2 \n");
        state = BCC2ok;
      }
      else
      {
        //printf("= esc para a data");
        data[*counter] = ESC;
        (*counter)++;
        state = Data;
      }
    }
    else
    {
      STOP = TRUE;
    }

    break;

  case BCC2ok:

    if (*buf == FLAG)
    {
      STOP = TRUE;
      FRAME_RECEIVED = TRUE;
      printf("Received valid Data frame %s.\n", data);
    }
    else
    {

      data[*counter] = bcc2Calculator(data, *counter);
      (*counter)++;

      if (*buf == ESC)
      {
        state = DestuffingData;
      }

      if (*buf != bcc2Calculator(data, *counter))
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
