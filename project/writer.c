#include "writer.h"

int RECEIVED = FALSE;

DataLink *dl;

typedef enum
{
  INITIAL,
  STATE_FLAG,
  STATE_A,
  STATE_C,
  STATE_BCC
} State;

// ----------------- UTILS ----------------------
unsigned char *stuff(unsigned char *buf, int *bufsize)
{
  unsigned char *stuffedBuf = (unsigned char *)malloc(*bufsize);

  int i = 0;
  for (; i < *bufsize; i++)
  {
    if (buf[i] == FLAG)
    {
      stuffedBuf = (unsigned char *)realloc(stuffedBuf, (*bufsize)++);
      stuffedBuf[i] = ESCAPE;
      stuffedBuf[i + 1] = FLAG ^ 0x20;
      i += 2;
    }
    else if (buf[i] == ESCAPE)
    {
      stuffedBuf = (unsigned char *)realloc(stuffedBuf, (*bufsize)++);
      stuffedBuf[i] = ESCAPE;
      stuffedBuf[i + 1] = ESCAPE ^ 0x20;
      i += 2;
    }
    else
    {
      stuffedBuf[i] = buf[i];
    }
  }
  //free(buf);
  
  return stuffedBuf;
}

void printArr(unsigned char arr[], int size)
{
  int i;
  for (i = 0; i < size; i++)
    printf("%x ", arr[i]);
  printf("\n");
}
// --------------- END OF UTILS --------------------

// ----------- ALARM HANDLING -----------------
void timeoutHandler(int signo)
{
  if (signo == SIGALRM)
    dl->timeout = 1;
}

void installAlarm()
{
  struct sigaction action;
  action.sa_handler = timeoutHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGALRM, &action, NULL);
}

void uninstallAlarm()
{
  struct sigaction action;
  action.sa_handler = NULL;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGALRM, &action, NULL);
}

// ----------- DATA LINK LAYER -----------------
DataLink *initDataLinkLayer()
{
  // allocate data link struct
  dl = (DataLink *)malloc(sizeof(DataLink));

  // initialize data link struct
  dl->frame = 0;
  dl->timeout = 0;
  dl->ntries = 0;

  return dl;
}

void destroyDataLinkLayer()
{
  // free datalink layer
  free(dl);
}

unsigned char *readControlFrame(int fd, unsigned char c)
{
  unsigned char* control = (unsigned char *)malloc(5);
  unsigned char byte;

  State state = INITIAL;
  while (!RECEIVED && !dl->timeout)
  {
    read(fd, &byte, 1);

    switch (state)
    {
    case INITIAL:
      if (byte == FLAG)
      {
        control[0] = FLAG;
        state = STATE_FLAG;
      }
      break;
    case STATE_FLAG:
      if (byte == A_TRANSMITTER)
      {
        control[1] = A_TRANSMITTER;
        state = STATE_A;
      }
      else if (byte == FLAG)
      {
        control[0] = FLAG;
        state = STATE_FLAG;
      }
      else
      {
        state = INITIAL;
      }
      break;
    case STATE_A:
      if (byte == c)
      {
        control[2] = c;
        state = STATE_C;
      }
      else if (byte == FLAG)
      {
        control[0] = FLAG;
        state = STATE_FLAG;
      }
      else
      {
        state = INITIAL;
      }
      break;
    case STATE_C:
      if (byte == control[1] ^ control[2])
      {
        control[3] = control[1] ^ control[2];
        state = STATE_BCC;
      }
      else if (byte == FLAG)
      {
        control[0] = FLAG;
        state = STATE_FLAG;
      }
      else
      {
        state = INITIAL;
      }
      break;
    case STATE_BCC:
      if (byte == FLAG)
      {
        RECEIVED = TRUE;
        control[4] = FLAG;
      }
      else
        state = INITIAL;
      break;
    default:
      break;
    }
  }

  return control;
}

unsigned char readControlC(int fd)
{
  unsigned char control[5];
  unsigned char byte;

  State state = INITIAL;
  while (!RECEIVED && !dl->timeout)
  {
    read(fd, &byte, 1);

    switch (state)
    {
    case INITIAL:
      if (byte == FLAG)
      {
        control[0] = FLAG;
        state = STATE_FLAG;
      }
      break;
    case STATE_FLAG:
      if (byte == A_TRANSMITTER)
      {
        control[1] = A_TRANSMITTER;
        state = STATE_A;
      }
      else if (byte == FLAG)
      {
        control[0] = FLAG;
        state = STATE_FLAG;
      }
      else
      {
        state = INITIAL;
      }
      break;
    case STATE_A:
      if (byte == C_RR0 || byte == C_RR1 || byte == C_REJ0 || byte == C_REJ1)
      {
        control[2] = byte;
        state = STATE_C;
      }
      else if (byte == FLAG)
      {
        control[0] = FLAG;
        state = STATE_FLAG;
      }
      else
      {
        state = INITIAL;
      }
      break;
    case STATE_C:
      if (byte == control[1] ^ control[2])
      {
        control[3] = control[1] ^ control[2];
        state = STATE_BCC;
      }
      else if (byte == FLAG)
      {
        control[0] = FLAG;
        state = STATE_FLAG;
      }
      else
      {
        state = INITIAL;
      }
      break;
    case STATE_BCC:
      if (byte == FLAG)
      {
        RECEIVED = TRUE;
        control[4] = FLAG;
      }
      else
        state = INITIAL;
      break;
    default:
      break;
    }
  }

  if (RECEIVED)
  {
    unsigned char *frameC = (unsigned char *)malloc(sizeof(unsigned char));
    *frameC = control[2];
    return *frameC;
  }
  else
  {
    return 0xFF;
  }
}

int llopen(int fd)
{
  unsigned char setup[5];

  if (tcgetattr(fd, &dl->oldtio) == -1)
  { /* save current port settings */
    perror("llopen - tcgetattr error!\n");
    exit(-1);
  }

  bzero(&dl->newtio, sizeof(dl->newtio));
  dl->newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  dl->newtio.c_iflag = IGNPAR;
  dl->newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  dl->newtio.c_lflag = 0;

  /*
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
  leitura do(s) próximo(s) caracter(es)
  */
  dl->newtio.c_cc[VTIME] = 1; /* inter-character timer unused */
  dl->newtio.c_cc[VMIN] = 0;  /* blocking read until 1 char is received */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &dl->newtio) == -1)
  {
    perror("llopen - tcsetattr error!\n");
    exit(-1);
  }
  printf("llopen: new termios structure set\n");

  State state = INITIAL;
  setup[0] = FLAG;
  setup[1] = A_TRANSMITTER;
  setup[2] = C_SET;
  setup[3] = setup[1] ^ setup[2]; //BCC
  setup[4] = FLAG;

  RECEIVED = FALSE;
  while (!RECEIVED && dl->ntries < MAX_TRIES)
  {
    // writes SET to serial port
    write(fd, setup, 5);
    printf("llopen - SET sent: ");
    printArr(setup, 5);

    alarm(TIMEOUT);

    // read ua confirmation frame
    unsigned char *ua = readControlFrame(fd, C_UA);
    if (RECEIVED)
    {
      printf("llopen - UA received: ");
      printArr(ua, 5);
    }

    alarm(0);

    dl->timeout = 0;
    dl->ntries++;
  }
  dl->ntries = 0;

  if (RECEIVED)
  {
    return fd;
  }
  else
  {
    return -1;
  }
}

int llclose(int fd)
{
  unsigned char disc[5], ua[5];

  State state = INITIAL;
  disc[0] = FLAG;
  disc[1] = A_TRANSMITTER;
  disc[2] = C_DISC;
  disc[3] = disc[1] ^ disc[2]; //BCC
  disc[4] = FLAG;

  RECEIVED = FALSE;
  while (!RECEIVED && dl->ntries < MAX_TRIES)
  {
    /* writes SET to serial port */
    write(fd, disc, 5);
    printf("llclose - DISC sent: ");
    printArr(disc, 5);

    alarm(TIMEOUT);

    // read disc confirmation frame
    unsigned char *disc = readControlFrame(fd, C_DISC);
    if (RECEIVED)
    {
      printf("llclose - DISC received: ");
      printArr(disc, 5);
    }

    alarm(0);

    dl->timeout = 0;
    dl->ntries++;
  }
  dl->ntries = 0;

  if (RECEIVED)
  {
    ua[0] = FLAG;
    ua[1] = A_TRANSMITTER;
    ua[2] = C_UA;
    ua[3] = ua[1] ^ ua[2];
    ua[4] = FLAG;

    write(fd, ua, 5);
    printf("llclose - UA sent: ");
    printArr(ua, 5);
  }

  tcsetattr(fd, TCSANOW, &dl->oldtio);

  if (close(fd) == -1)
  {
    return -1;
  }
  else
  {
    return 1;
  }
}

int llwrite(int fd, unsigned char *buf, int bufsize)
{
  // allocate frame memory space
  int totalsize = bufsize + 6;
  unsigned char *frame = (unsigned char *)malloc(totalsize);

  // frame construction
  frame[0] = FLAG;
  frame[1] = A_TRANSMITTER;
  dl->frame == 0 ? (frame[2] = C_0) : (frame[2] = C_1);
  frame[3] = frame[1] ^ frame[2];

  // save bcc2 before stuffing
  unsigned char bcc2 = buf[0];

  int i = 1;
  for (; i < bufsize; i++)
    bcc2 ^= buf[i];

  // data stuffing
  unsigned char *stuffedbuf = stuff(buf, &bufsize);
  if (bufsize != totalsize - 6)
  {
    // new totalsize, after stuffing data
    totalsize = bufsize + 6;
    frame = (unsigned char *)realloc(frame, totalsize);

    for (i = 0; i < bufsize; i++)
      frame[4 + i] = stuffedbuf[i];
  }
  else
  {
    for (i = 0; i < bufsize; i++)
      frame[4 + i] = stuffedbuf[i];
  }

  // bcc2 stuffing
  int bcc2size = 1;
  unsigned char *stuffedBcc2 = stuff(&bcc2, &bcc2size);

  if (bcc2size > 1)
  {
    // in case bcc2 was stuffed
    frame = (unsigned char *)realloc(frame, totalsize++);

    int j = 0;
    for (; j < bcc2size; j++)
      frame[4 + bufsize + j] = stuffedBcc2[j];
  }
  else
  {
    frame[4 + bufsize] = bcc2;
  }

  frame[4 + bufsize + bcc2size] = FLAG;

  // send frame and wait confirmation
  RECEIVED = FALSE;
  while (!RECEIVED && dl->ntries < MAX_TRIES)
  {
    // send frame
    write(fd, frame, totalsize);
    printf("llwrite - frame sent: ");
    printArr(frame, totalsize);

    alarm(TIMEOUT);

    // read control c parameter
    unsigned char c = readControlC(fd);
    if ((c == C_RR0 && dl->frame == 1) || (c == C_RR1 && dl->frame == 0))
    {
      RECEIVED = TRUE;
      dl->frame ^= 1;
      printf("llwrite: RR %x received for frame %d.\n", c, dl->frame);
    }
    else if (c == C_REJ0 || c == C_REJ1)
    {
      printf("llwrite: REJ %x received for frame %d.\n", c, dl->frame);
    }

    alarm(0);

    dl->timeout = 0;
    dl->ntries++;
  }
  dl->ntries = 0;

  if (RECEIVED)
    return 1;
  else
    return -1;
}
// ------------ END OF DATA LINK LAYER ----------------

// -------------- APPLICATION LAYER -------------------
Application *initApplicationLayer(char *port, char *filepath)
{
  Application *app = (Application *)malloc(sizeof(Application));

  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */
  app->fd = open(port, O_RDWR | O_NOCTTY);
  if (app->fd < 0)
  {
    perror(port);
    exit(-1);
  }

  // store file path
  app->filepath = filepath;

  // start frame numbers
  app->package = 0;
  app->totalpackages = 0;

  // initialize datalink layer
  dl = initDataLinkLayer();

  return app;
}

void destroyApplicationLayer(Application *app)
{
  destroyDataLinkLayer();

  // free application layer
  free(app);
}

unsigned char *getDataPackage(Application *app, unsigned char *buf, off_t filesize, int *packagesize)
{
  unsigned char *dataPackage = (unsigned char *)malloc(filesize + 4);

  // construct dataFrame
  dataPackage[0] = DATAFRAME_C;
  dataPackage[1] = app->package % 255;
  dataPackage[2] = filesize / 256; // filesize MSB
  dataPackage[3] = filesize % 256; // filesize LSB

  int i;
  for (i = 4; i < *packagesize; i++)
    dataPackage[i] = buf[i];

  app->package++;
  app->totalpackages++;
  *packagesize += 4;

  return dataPackage;
}

int sendDataPackage(Application *app, unsigned char *buf, int packagesize)
{
  unsigned char *dataPackage = getDataPackage(app, buf, app->package, &packagesize);

  if (llwrite(app->fd, dataPackage, packagesize) == -1)
  {
    printf("sendfile: could not send data package number %d!\n", app->totalpackages);
    return -1;
  }
  else
  {
    printf("sendfile: data package number %d sent!\n", app->totalpackages);
    return 1;
  }
}

unsigned char *getControlPackage(unsigned char c, off_t filesize, char *filepath, int *packagesize)
{
  *packagesize = 9 + strlen(filepath);
  unsigned char *package = (unsigned char *)malloc(*packagesize);

  if (!(c == C_START || c == C_END))
  {
    printf("getControlPackage: invalid control package state!\n");
    return NULL;
  }

  // package control state
  package[0] = c;

  // package first argument - filesize
  package[1] = T1;
  package[2] = L1;
  package[3] = (filesize >> 24) & 0xFF; // 1st byte
  package[4] = (filesize >> 16) & 0xFF; // 2nd byte
  package[5] = (filesize >> 8) & 0xFF;  // 3rd byte
  package[6] = filesize & 0xFF;         // 4th byte

  // package second argument - filepath
  package[7] = T2;
  package[8] = strlen(filepath);
  int i;
  for (i = 0; i < strlen(filepath); i++)
    package[i + 9] = filepath[i];

  return package;
}

int sendControlPackage(int fd, int c, off_t filesize, char *filepath)
{
  if (!(c == C_START || c == C_END))
  {
    printf("sendControlPackage: invalid control package state!\n");
    return -1;
  }

  // create start package with arguments filesize and filepath
  int packagesize;
  unsigned char *package = getControlPackage(c, filesize, filepath, &packagesize);

  // send it via serial port
  if (llwrite(fd, package, packagesize) != -1)
  {
    if (c == C_START)
      printf("sendControlPackage: START package sent!\n");
    else if (c == C_END)
      printf("sendControlPackage: END package sent!\n");

    return 1;
  }
  else
  {
    printf("sendControlPackage: could not send package!\n");
    return -1;
  }
}

int sendFile(Application *app)
{
  // open file
  FILE *file = fopen(app->filepath, "rb");
  if (file == NULL)
  {
    perror("openfile: could not read file!\n");
    exit(-1);
  }

  // extract file metadata
  struct stat metadata;
  stat(app->filepath, &metadata);
  off_t filesize = metadata.st_size;
  printf("File of size %ld bytes.\n", filesize);

  // send start control package
  sendControlPackage(app->fd, C_START, filesize, app->filepath);

  // allocate array to store file data
  unsigned char *filebuf = (unsigned char *)malloc(MAX_SIZE);

  int readBytes;
  while ((readBytes = fread(filebuf, sizeof(unsigned char), MAX_SIZE, file)) > 0) {
    sendDataPackage(app, filebuf, readBytes);
  }

  // send end control package
  sendControlPackage(app->fd, C_END, filesize, app->filepath);

  // free file buf auxiliary array
  free(filebuf);

  // close file
  if (fclose(file) == -1)
  {
    printf("closefile: could not close file!\n");
    return -1;
  }

  return 1;
}
// ----------- END OF APPLICATION LAYER -----------------

int main(int argc, char **argv)
{
  time_t start, end;

  if ((argc < 3) ||
      ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
       (strcmp("/dev/ttyS1", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort File\n\tex: nserial /dev/ttyS1 Penguin.gif\n");
    exit(1);
  }

  // initialize application layer
  Application *app = initApplicationLayer(argv[1], argv[2]);

  // install alarm handler
  installAlarm();

  // start transfer timing
  time(&start);

  // open connection
  if (llopen(app->fd) == -1)
  {
    printf("llopen: failed to open connection!\n");
    return -1;
  }

  // opens file to transmit
  sendFile(app);

  // close connection
  if (llclose(app->fd) == -1)
  {
    printf("llclose: failed to close connection!\n");
    return -1;
  }

  // stop transfer timing
  time(&end);

  double elapsed = difftime(end, start);
  printf("# File transfered in %.2f seconds!\n", elapsed);

  uninstallAlarm();

  // destroy application layer
  destroyApplicationLayer(app);

  return 0;
}