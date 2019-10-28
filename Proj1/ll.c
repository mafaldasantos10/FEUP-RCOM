#include "ll.h"
#include "ll_app.h"

// Global variables
volatile int STOP = FALSE;           // Used to exit the while loop in readFrame function
volatile int SET_ON = FALSE;         // TRUE if a SET frame was read when it's not expected to, FALSE otherwise
volatile int FRAME_RECEIVED = FALSE; // TRUE if the rigth frame was received, FALSE otherwise
volatile int DUPLICATE = FALSE;      // TRUE if a duplicate frame was received, FALSE otherwise

volatile int fileDescriptor = 0; // Serial port file descriptor
volatile int num_return = 1;     // Number of times the alarm was activated
unsigned char A_expected;        // Expected A frame parameter to be compared in state machines
unsigned char C_expected;        // Expected C frame parameter to be compared in state machines
unsigned char BCC_expected;      // Expected BCC frame parameter to be compared in state machines
int flag = 1;                    // When set allows the transmiter to send a frame again

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
    /* Sends frame again */
    printf("Sending frame again...\n");
    writeFrame(linkStruct.frame);
    alarm(TIMEOUT);

    printf("Waiting for acknowledging frame...\n\n");
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
  linkStruct.status = status;
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
  int count_ignore = 0;

  /* Receive SET frame from transmitter*/
  A_expected = A;
  C_expected = C_S;
  BCC_expected = BCC_S;

  printf("Waiting for SET frame...\n");
  while (readFrame(0, "", &count_ignore) != 0)
  {
  }

  /* Send UA frame to transmitter*/
  unsigned char frame[MAX_BUF];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A;
  frame[C_INDEX] = C_UA;
  frame[BCC_INDEX] = BCC_UA;
  frame[FLAG2_INDEX] = FLAG;

  printf("Sending UA frame...\n\n");
  writeFrame(frame);
}

int transmitter()
{
  // Creates alarm signal
  (void)signal(SIGALRM, alarmHandler);

  // While UA frame is not received, when an alarm signal occurs another SET frame is sent
  while (FRAME_RECEIVED == FALSE)
  {
    if (flag == 1)
    {
      int count_ignore = 0;

      /* Send SET frame to receiver */
      linkStruct.frame[FLAG_INDEX] = FLAG;
      linkStruct.frame[A_INDEX] = A;
      linkStruct.frame[C_INDEX] = C_S;
      linkStruct.frame[BCC_INDEX] = BCC_S;
      linkStruct.frame[FLAG2_INDEX] = FLAG;

      printf("Sending SET frame...\n");
      writeFrame(linkStruct.frame);

      alarm(TIMEOUT);

      /* Receive UA frame from receiver */
      A_expected = A;
      C_expected = C_UA;
      BCC_expected = BCC_UA;

      printf("Waiting for UA frame...\n\n");
      readFrame(0, "", &count_ignore);

      // Resets alarm flag
      flag = 0;
    }
  }

  // Deactivates alarm
  alarm(0);
}

int llwrite(int fd, char *buffer, int length)
{
  FRAME_RECEIVED = FALSE;
  fileDescriptor = fd;
  flag = 1;

  while (FRAME_RECEIVED == FALSE)
  {
    if (flag == 1)
    {
      int count_ignore = 0;

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

      printf("Waiting for acknowledging frame...\n\n");
      readFrame(0, "", &count_ignore);
    }
  }

  linkStruct.sequenceNumber = (linkStruct.sequenceNumber + 1) % 2;

  return length;
}

int llread(int fd, char *buffer)
{
  fileDescriptor = fd;
  int length = 0;
  // Reads transmitter's data frame
  A_expected = A;
  C_expected = C_I | (linkStruct.sequenceNumber << 6);
  BCC_expected = A ^ C_expected;
  unsigned char frame[MAX_BUF];
  printf("Waiting for data frame...\n");
  if (readFrame(1, buffer, &length) == 0)
  {
    if (SET_ON == TRUE)
    {
      /* Send UA frame to transmitter*/
      frame[FLAG_INDEX] = FLAG;
      frame[A_INDEX] = A;
      frame[C_INDEX] = C_UA;
      frame[BCC_INDEX] = BCC_UA;
      frame[FLAG2_INDEX] = FLAG;
      length = -1;
    }
    else
    {
      if (DUPLICATE == TRUE)
      {
        length = -1;
      }
      else
      {
        linkStruct.sequenceNumber = (linkStruct.sequenceNumber + 1) % 2;
      }
      // Writes acknowledging frame to transmitter
      frame[FLAG_INDEX] = FLAG;
      frame[A_INDEX] = A;
      frame[C_INDEX] = C_RR | (linkStruct.sequenceNumber << 7);
      frame[BCC_INDEX] = A ^ frame[C_INDEX];
      frame[FLAG2_INDEX] = FLAG;
    }
  }
  else
  {
    // Writes rejected acknowledging frame to transmitter
    frame[FLAG_INDEX] = FLAG;
    frame[A_INDEX] = A;
    frame[C_INDEX] = C_REJ | (linkStruct.sequenceNumber << 7);
    frame[BCC_INDEX] = A ^ frame[C_INDEX];
    frame[FLAG2_INDEX] = FLAG;

    length = -1;
  }
  if (length == -1)
  {
    printf("Sending negative acknowledging frame...\n\n");
  }
  else
  {
    printf("Sending positive acknowledging frame...\n\n");
  }
  writeFrame(frame);

  return length;
}

int llclose(int fd)
{

  if (linkStruct.status == TRANSMITTER)
  {
    transmitterClose();
  }
  else if (linkStruct.status == RECEIVER)
  {
    receiverClose();
  }

  tcsetattr(fd, TCSANOW, &linkStruct.oldtio);
  close(fd);

  return 0;
}

int receiverClose()
{
  int count_ignore = 0;
  /* Receive SET frame from transmitter*/
  A_expected = A;
  C_expected = C_DISC;
  BCC_expected = BCC_DISC;

  printf("Waiting for DISC frame...\n");
  while (readFrame(0, "", &count_ignore) != 0)
  {
  }

  /* Send UA frame to transmitter*/
  unsigned char frame[MAX_BUF];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A;
  frame[C_INDEX] = C_DISC;
  frame[BCC_INDEX] = BCC_DISC;
  frame[FLAG2_INDEX] = FLAG;

  printf("Sending DISC frame...\n");
  writeFrame(frame);

  /* Receive SET frame from transmitter*/
  A_expected = A;
  C_expected = C_UA;
  BCC_expected = BCC_UA;

  printf("Waiting for UA frame...\n\n");
  if (readFrame(0, "", &count_ignore) == 0)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}

int transmitterClose()
{
  FRAME_RECEIVED = FALSE;
  flag = 1;

  while (FRAME_RECEIVED == FALSE)
  {
    if (flag == 1)
    {
      int count_ignore = 0;

      /* Send SET frame to receiver */
      linkStruct.frame[FLAG_INDEX] = FLAG;
      linkStruct.frame[A_INDEX] = A;
      linkStruct.frame[C_INDEX] = C_DISC;
      linkStruct.frame[BCC_INDEX] = BCC_DISC;
      linkStruct.frame[FLAG2_INDEX] = FLAG;

      printf("Sending DISC frame...\n");
      writeFrame(linkStruct.frame);
      alarm(TIMEOUT);

      /* Receive UA frame from receiver */
      A_expected = A;
      C_expected = C_DISC;
      BCC_expected = BCC_DISC;

      printf("Waiting for DISC frame...\n");
      readFrame(0, "", &count_ignore);
      flag = 0;
    }
  }
  alarm(0);

  unsigned char frame[MAX_BUF];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A;
  frame[C_INDEX] = C_UA;
  frame[BCC_INDEX] = BCC_UA;
  frame[FLAG2_INDEX] = FLAG;

  printf("Sending UA frame...\n\n");
  writeFrame(frame);
  sleep(1);

  return 1;
}

int bcc2Calculator(unsigned char *buffer, int length)
{
  int bcc2 = buffer[0];
  for (unsigned int i = 1; i < length; i++)
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
  int max_buf = 0; // Amount of data that can be received before leaving the while loop
  unsigned char buf[MAX_BUF];

  // State machine initialization
  enum startSt state;
  state = Start;

  // Resets the reading data byte counter
  *counter = 0;

  STOP = FALSE;
  SET_ON = FALSE;
  DUPLICATE = FALSE;
  FRAME_RECEIVED = FALSE;

  while (STOP == FALSE && max_buf != MAX_BUF)
  {                                      /* loop for input */
    resR = read(fileDescriptor, buf, 1); /* returns after 1 char has been input */

    if (operation == 0) // Non Data Frames
    {
      state = startUpStateMachine(state, buf);
    }
    else // Data Frames
    {
      state = dataStateMachine(state, buf, data, counter);
    }

    // Only starts counting when it's not trash
    if (state != StartData && state != Start)
    {
      max_buf++;
    }
  }

  //  TODO: CHECK THIS!!!
  if (FRAME_RECEIVED == TRUE)
  {
    flag = 0;
    return 0;
  }
  if (SET_ON == TRUE || DUPLICATE == TRUE)
  {
    flag = 0;
    return 0;
  }
  else
  {
    flag = 0;
    return 1;
  }
}

void writeFrame(unsigned char frame[])
{
  int resW = write(fileDescriptor, frame, MAX_BUF);
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
      STOP = TRUE;
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
      STOP = TRUE;
    }
    else if (*buf == (C_REJ | (((linkStruct.sequenceNumber + 1) % 2) << 7)))
    {
      STOP = TRUE;
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
      STOP = TRUE;
    }
    else
    {
      state = Start;
    }
    break;

  case BCCok:
    if (*buf == FLAG)
    {
      STOP = TRUE;
      FRAME_RECEIVED = TRUE;
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
    else if (*buf == (C_I | (((linkStruct.sequenceNumber + 1) % 2) << 6)))
    {
      DUPLICATE = TRUE;
      STOP = TRUE;
    }
    else
    {
      state = StartData;
    }
    break;

  case CRecievedData:
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
    if (*buf == FLAG)
    {
      STOP = TRUE;
    }
    else if (*buf == ESC)
    {
      state++;
    }
    else if (*buf == bcc2Calculator(data, *counter))
    {
      state = BCC2ok;
    }
    else
    {
      data[*counter] = *buf;
      (*counter)++;
    }

    break;

  case DestuffingData:
    if (*buf == FLAG_STUFFING)
    {
      if (FLAG == bcc2Calculator(data, *counter))
      {
        state = BCC2ok;
      }
      else
      {
        data[*counter] = FLAG;
        (*counter)++;
        state = Data;
      }
    }
    else if (*buf == ESC_STUFFING)
    {
      if (ESC == bcc2Calculator(data, *counter))
      {
        state = BCC2ok;
      }
      else
      {
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
    }
    else
    {

      data[*counter] = bcc2Calculator(data, *counter);
      (*counter)++;

      if (*buf == ESC)
      {
        state = DestuffingData;
        break;
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
