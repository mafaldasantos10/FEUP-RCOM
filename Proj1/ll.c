#include "ll.h"

// Global variables
volatile int STOP = FALSE;           // Used to exit the while loop in readFrame function
volatile int SET_ON = FALSE;         // TRUE if a SET frame was read when it's not expected to, FALSE otherwise
volatile int FRAME_RECEIVED = FALSE; // TRUE if the rigth frame was received, FALSE otherwise
volatile int DUPLICATE = FALSE;      // TRUE if a duplicate frame was received, FALSE otherwise
volatile int REJ = FALSE;            // TRUE if a negative acknowledging frame was received, FALSE otherwise

unsigned char A_expected;   // Expected A frame parameter to be compared in state machines
unsigned char C_expected;   // Expected C frame parameter to be compared in state machines
unsigned char BCC_expected; // Expected BCC frame parameter to be compared in state machines
int flag = 1;               // When set allows the transmiter to send a frame again

/* Handling alarm interruption */
void alarmHandler()
{
  flag = 1;

  if (FRAME_RECEIVED == TRUE)
  {
    linkStruct.num_return = 0;
    return;
  }

  if (linkStruct.num_return < MAX_RETURN)
  {
    /* Sends frame again */
    printf("Sending frame again... Remaining attempts: %d\n", MAX_RETURN - linkStruct.num_return - 1);
    writeFrame(linkStruct.frame);

    alarm(TIMEOUT);

    printf("Waiting for acknowledging frame...\n\n");
    linkStruct.num_return++;
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

  return linkStruct.fileDescriptor;
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
  linkStruct.fileDescriptor = open(linkStruct.port, O_RDWR | O_NOCTTY);
  if (linkStruct.fileDescriptor < 0)
  {
    perror(linkStruct.port);
    exit(-1);
  }

  if (tcgetattr(linkStruct.fileDescriptor, &linkStruct.oldtio) == -1)
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

  tcflush(linkStruct.fileDescriptor, TCIOFLUSH);

  if (tcsetattr(linkStruct.fileDescriptor, TCSANOW, &newtio) == -1)
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
  // Initializes variable before creating alarm
  linkStruct.num_return = 0;

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
  flag = 1;
  linkStruct.num_return = 0;

  linkStruct.fileDescriptor = fd;

  // While positive acknowledging frame is not received, when an alarm signal occurs the data frame is sent again
  while (FRAME_RECEIVED == FALSE)
  {
    if (flag == 1)
    {
      int count_ignore = 0;

      /* Writes data frame to receiver */
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

      /* Reads receiver's acknowledging frame */
      A_expected = A;
      C_expected = C_RR | (((linkStruct.sequenceNumber + 1) % 2) << 7);
      BCC_expected = A ^ C_expected;

      flag = 0;

      printf("Waiting for acknowledging frame...\n");
      if (readFrame(0, "", &count_ignore) != 0)
      {
        if (REJ == TRUE)
        {
          flag = 1;
          printf("Received REJ frame!\n");
        }
        else
        {
          printf("Received wrong frame!\n");
        }
      }
      printf("\n");
    }
  }

  // Changes sequence number when the last acknowledging frame was received successfully
  linkStruct.sequenceNumber = (linkStruct.sequenceNumber + 1) % 2;

  return length;
}

int llread(int fd, char *buffer)
{
  linkStruct.fileDescriptor = fd;

  int length = 0; // Received frame length
  unsigned char frame[MAX_BUF];

  /* Reads transmitter's data frame */
  A_expected = A;
  C_expected = C_I | (linkStruct.sequenceNumber << 6);
  BCC_expected = A ^ C_expected;

  if (readFrame(1, buffer, &length) == 0)
  {
    if (SET_ON == TRUE) // Received SET frame
    {
      /* Send UA frame to transmitter*/
      frame[FLAG_INDEX] = FLAG;
      frame[A_INDEX] = A;
      frame[C_INDEX] = C_UA;
      frame[BCC_INDEX] = BCC_UA;
      frame[FLAG2_INDEX] = FLAG;

      length = -1;
      printf("Sending UA frame...\n\n");
    }
    else
    {
      if (DUPLICATE == TRUE) // Received duplicate frame
      {
        length = -1;
        printf("Duplicate! Sending positive acknowledging frame...\n\n");
      }
      else // Received data frame successfully, changing sequence number
      {
        linkStruct.sequenceNumber = (linkStruct.sequenceNumber + 1) % 2;
        printf("Sending positive acknowledging frame...\n\n");
      }

      /* Writes positive acknowledging frame to transmitter */
      frame[FLAG_INDEX] = FLAG;
      frame[A_INDEX] = A;
      frame[C_INDEX] = C_RR | (linkStruct.sequenceNumber << 7);
      frame[BCC_INDEX] = A ^ frame[C_INDEX];
      frame[FLAG2_INDEX] = FLAG;
    }
  }
  else // Received data frame with errors
  {
    /* Writes negative acknowledging frame to transmitter */
    frame[FLAG_INDEX] = FLAG;
    frame[A_INDEX] = A;
    frame[C_INDEX] = C_REJ | (((linkStruct.sequenceNumber + 1) % 2) << 7);
    frame[BCC_INDEX] = A ^ frame[C_INDEX];
    frame[FLAG2_INDEX] = FLAG;

    length = -1;
    printf("Sending negative acknowledging frame...\n\n");
  }

  writeFrame(frame);

  return length;
}

int llclose(int fd)
{
  int ret = 0;

  if (linkStruct.status == TRANSMITTER)
  {
    ret = transmitterClose();
  }
  else if (linkStruct.status == RECEIVER)
  {
    ret = receiverClose();
  }

  tcsetattr(fd, TCSANOW, &linkStruct.oldtio);
  close(fd);

  return ret;
}

int receiverClose()
{
  int count_ignore = 0;

  /* Receive DISC frame from transmitter*/
  A_expected = A;
  C_expected = C_DISC;
  BCC_expected = BCC_DISC;

  printf("Waiting for DISC frame...\n");
  while (readFrame(0, "", &count_ignore) != 0)
  {
  }

  /* Send DISC frame to transmitter*/
  unsigned char frame[MAX_BUF];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A;
  frame[C_INDEX] = C_DISC;
  frame[BCC_INDEX] = BCC_DISC;
  frame[FLAG2_INDEX] = FLAG;

  printf("Sending DISC frame...\n");
  writeFrame(frame);

  /* Receive UA frame from transmitter*/
  A_expected = A;
  C_expected = C_UA;
  BCC_expected = BCC_UA;

  printf("Waiting for UA frame...\n\n");
  if (readFrame(0, "", &count_ignore) == 0)
  {
    printf("Received UA frame successfully! Closing program...\n\n");
    return 0;
  }
  else
  {
    printf("Error reading UA frame\n\n");
    return -1;
  }
}

int transmitterClose()
{
  FRAME_RECEIVED = FALSE;
  flag = 1;
  linkStruct.num_return = 0;

  // While DISC frame is not received, when an alarm signal occurs another DISC frame is sent
  while (FRAME_RECEIVED == FALSE)
  {
    if (flag == 1)
    {
      int count_ignore = 0;

      /* Send DISC frame to receiver */
      linkStruct.frame[FLAG_INDEX] = FLAG;
      linkStruct.frame[A_INDEX] = A;
      linkStruct.frame[C_INDEX] = C_DISC;
      linkStruct.frame[BCC_INDEX] = BCC_DISC;
      linkStruct.frame[FLAG2_INDEX] = FLAG;

      printf("Sending DISC frame...\n");
      writeFrame(linkStruct.frame);

      alarm(TIMEOUT);

      /* Receive DISC frame from receiver */
      A_expected = A;
      C_expected = C_DISC;
      BCC_expected = BCC_DISC;

      printf("Waiting for DISC frame...\n");
      readFrame(0, "", &count_ignore);

      // Resets alarm flag
      flag = 0;
    }
  }

  // Deactivates alarm
  alarm(0);

  /* Send UA frame to receiver */
  unsigned char frame[MAX_BUF];
  frame[FLAG_INDEX] = FLAG;
  frame[A_INDEX] = A;
  frame[C_INDEX] = C_UA;
  frame[BCC_INDEX] = BCC_UA;
  frame[FLAG2_INDEX] = FLAG;

  printf("Sending UA frame...\n\n");
  writeFrame(frame);

  sleep(1); // Ensures the frame was read by the receiver before changing serial port settings

  return 0;
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
  unsigned int j = 0; // Current index of new frame

  for (int i = 0; i < length; i++, j++)
  {
    // Frame header (except BCC2) stays the same
    if (i < DATA_INDEX || i == length - 1)
    {
      newFrame[j] = frame[i];
      continue;
    }

    if (frame[i] == FLAG) // Byte stuffing for FLAG bytes
    {
      newFrame[j] = ESC;
      j++;
      newFrame[j] = FLAG_STUFFING;
    }
    else if (frame[i] == ESC) // Byte stuffing for escape bytes
    {
      newFrame[j] = ESC;
      j++;
      newFrame[j] = ESC_STUFFING;
    }
    else // Other bytes stay the same
    {
      newFrame[j] = frame[i];
    }
  }

  // Returns frame with byte stuffing implemented
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
  REJ = FALSE;
  DUPLICATE = FALSE;
  FRAME_RECEIVED = FALSE;

  while (STOP == FALSE && max_buf != MAX_BUF)
  {                                                 /* loop for input */
    resR = read(linkStruct.fileDescriptor, buf, 1); /* returns after 1 char has been input */

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

  if (SET_ON == TRUE || DUPLICATE == TRUE || FRAME_RECEIVED == TRUE)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

void writeFrame(unsigned char frame[])
{
  int resW = write(linkStruct.fileDescriptor, frame, MAX_BUF);
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
      //STOP = TRUE; //TODOOOO
      break;
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
      REJ = TRUE;
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
      // STOP = TRUE;
      break;
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
