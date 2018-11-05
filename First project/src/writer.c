#include "writer.h"

int DEBUG = FALSE;
int received = FALSE;

DataLink *dl;

int main(int argc, char **argv)
{
  struct timespec start, end;

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
  clock_gettime(CLOCK_REALTIME, &start);

  // open connection
  if (!llopen(app->fd))
  {
    printf("[llopen] - failed to open connection!\n");
    return -1;
  }

  // opens file to transmit
  sendFile(app);

  // close connection
  if (!llclose(app->fd))
  {
    printf("[llclose] - failed to close connection!\n");
    return -1;
  }

  // stop transfer timing
  clock_gettime(CLOCK_REALTIME, &end);

  double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1E9;
  printf("# File transferred in %.2f seconds!\n", elapsed);

  uninstallAlarm();

  // destroy application layer
  destroyApplicationLayer(app);

  return 0;
}

// --------------- APPLICATION LAYER --------------------
Application *initApplicationLayer(char *port, char *filename)
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

  // store file name
  app->filename = filename;

  // start package numeration
  app->package = 0;

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

int sendFile(Application *app)
{
  // open file
  FILE *file = fopen(app->filename, "rb");
  if (file == NULL)
  {
    perror("[openfile] - could not read file!\n");
    exit(-1);
  }

  // extract file metadata
  struct stat metadata;
  stat((char *)app->filename, &metadata);
  off_t filesize = metadata.st_size;

  if (DEBUG)
    printf("[sendFile] - file %s of size %ld bytes.\n", app->filename, filesize);

  app->totalPackages = filesize / PACKAGE_SIZE;
  if (filesize % PACKAGE_SIZE != 0)
    app->totalPackages++;

  // allocate array to store file data
  unsigned char *filebuf = (unsigned char *)malloc(filesize);
  fread(filebuf, sizeof(unsigned char), filesize, file);

  // send start control package
  sendControlPackage(app->fd, C_START, filesize, app->filename);

  int packageSize = PACKAGE_SIZE;

  off_t idx = 0;
  while (idx < filesize)
  {
    unsigned char *package = getBufPackage(filebuf, &idx, &packageSize, filesize);
    if (sendDataPackage(app, package, packageSize, filesize) == -1)
      break;

    printProgressBar(app);
  }
  printf("\n");

  // send end control package
  sendControlPackage(app->fd, C_END, filesize, app->filename);

  // free file buf auxiliary array
  free(filebuf);

  // close file
  if (fclose(file) == -1)
  {
    printf("[closefile] - could not close file!\n");
    return -1;
  }

  return 1;
}

unsigned char *getControlPackage(unsigned char c, off_t filesize, char *filename, int *packageSize)
{
  *packageSize = 9 + strlen(filename);
  unsigned char *package = (unsigned char *)malloc(*packageSize);

  if (!(c == C_START || c == C_END))
  {
    printf("[getControlPackage] - invalid control package state!\n");
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
  package[8] = strlen(filename);
  int i;
  for (i = 0; i < strlen(filename); i++)
    package[i + 9] = filename[i];

  return package;
}

int sendControlPackage(int fd, int c, off_t filesize, char *filename)
{
  if (!(c == C_START || c == C_END))
  {
    printf("[sendControlPackage] - invalid control package state!\n");
    return -1;
  }

  // create start package with arguments filesize and filepath
  int packageSize = 0;
  unsigned char *package = getControlPackage(c, filesize, filename, &packageSize);

  // send it via serial port
  if (llwrite(fd, package, packageSize) != -1)
  {
    if (c == C_START)
      printf("[sendControlPackage] - START package sent!\n");
    else if (c == C_END)
      printf("[sendControlPackage] - END package sent!\n");

    return 1;
  }
  else
  {
    printf("[sendControlPackage] - could not send control package!\n");
    return -1;
  }
}

unsigned char *getBufPackage(unsigned char *filebuf, off_t *idx, int *packageSize, off_t filesize)
{
  if (*idx + *packageSize > filesize)
    *packageSize = filesize - *idx;

  unsigned char *package = (unsigned char *)malloc(*packageSize);
  memcpy(package, filebuf + *idx, *packageSize);
  *idx += *packageSize;

  return package;
}

unsigned char *getDataPackage(Application *app, unsigned char *buf, int *packageSize, off_t filesize)
{
  unsigned char *dataPackage = (unsigned char *)malloc(*packageSize + 4);

  // construct dataFrame
  dataPackage[0] = DATAFRAME_C;
  dataPackage[1] = app->package % 255;
  dataPackage[2] = (int)filesize / 256; // filesize MSB
  dataPackage[3] = (int)filesize % 256; // filesize LSB

  int i = 0;
  for (; i < *packageSize; i++)
    dataPackage[4 + i] = buf[i];

  app->package++;
  *packageSize += 4;

  return dataPackage;
}

int sendDataPackage(Application *app, unsigned char *buf, int packageSize, off_t filesize)
{
  unsigned char *dataPackage = getDataPackage(app, buf, &packageSize, filesize);

  if (llwrite(app->fd, dataPackage, packageSize) == -1)
  {
    if (DEBUG)
      printf("[sendDataPackage] - could not send data package number %d!\n\n", app->package);

    return -1;
  }
  else
  {
    if (DEBUG)
      printf("[sendDataPackage] - data package number %d sent!\n\n", app->package);

    return 1;
  }
}

// ----------- END OF APPLICATION LAYER -----------------

// ---------------- DATA LINK LAYER ---------------------

DataLink *initDataLinkLayer()
{
  // allocate data link struct
  dl = (DataLink *)malloc(sizeof(DataLink));

  // initialize data link struct
  dl->frame = 0;
  dl->timeout = 0;
  dl->nTries = 0;

  return dl;
}

void destroyDataLinkLayer()
{
  // free datalink layer
  free(dl);
}

unsigned char *readControlFrame(int fd, unsigned char c)
{
  unsigned char *control = (unsigned char *)malloc(5 * sizeof(unsigned char));
  unsigned char byte;

  State state = INITIAL;
  while (!received && !dl->timeout)
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
      if (byte == (control[1] ^ control[2]))
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
        received = TRUE;
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
  while (!received && !dl->timeout)
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
      if (byte == (control[1] ^ control[2]))
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
        received = TRUE;
        control[4] = FLAG;
      }
      else
        state = INITIAL;
      break;
    default:
      break;
    }
  }

  if (received)
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
    perror("[llopen] - tcgetattr error!\n");
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
    perror("[llopen] - tcsetattr error!\n");
    exit(-1);
  }
  printf("[llopen] - new termios structure set\n");

  setup[0] = FLAG;
  setup[1] = A_TRANSMITTER;
  setup[2] = C_SET;
  setup[3] = setup[1] ^ setup[2]; //BCC
  setup[4] = FLAG;

  received = FALSE;
  dl->nTries = 0;

  unsigned char *ua;
  while (!received && dl->nTries < MAX_TRIES)
  {
    // writes SET to serial port
    write(fd, setup, 5);
    printf("[llopen] - SET sent: ");
    printArr(setup, 5);

    alarm(TIMEOUT);

    // read ua confirmation frame
    ua = readControlFrame(fd, C_UA);

    alarm(0);

    dl->timeout = 0;
    dl->nTries++;
  }

  if (received)
  {
    printf("[llopen] - UA received: ");
    printArr(ua, 5);
    printf("[llopen] - connection opened successfully!\n");
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

int llclose(int fd)
{
  unsigned char disc[5], ua[5];

  disc[0] = FLAG;
  disc[1] = A_TRANSMITTER;
  disc[2] = C_DISC;
  disc[3] = disc[1] ^ disc[2]; //BCC
  disc[4] = FLAG;

  received = FALSE;
  dl->nTries = 0;

  while (!received && dl->nTries < MAX_TRIES)
  {
    /* writes SET to serial port */
    write(fd, disc, 5);
    printf("[llclose] - DISC sent: ");
    printArr(disc, 5);

    alarm(TIMEOUT);

    // read disc confirmation frame
    readControlFrame(fd, C_DISC);

    alarm(0);

    dl->timeout = 0;
    dl->nTries++;
  }

  if (received)
  {
    printf("[llclose] - DISC received: ");
    printArr(disc, 5);

    ua[0] = FLAG;
    ua[1] = A_TRANSMITTER;
    ua[2] = C_UA;
    ua[3] = ua[1] ^ ua[2];
    ua[4] = FLAG;

    write(fd, ua, 5);
    printf("[llclose] - UA sent: ");
    printArr(ua, 5);

    printf("[llclose] - connection successfully closed!\n");
  }

  if (tcsetattr(fd, TCSANOW, &dl->oldtio) == -1)
  {
    perror("[llclose] - tcsetattr error!\n");
    exit(-1);
  }

  if (close(fd) == -1)
  {
    printf("[llclose] - failed to close serial port!\n");
    return FALSE;
  }
  else
  {
    printf("[llclose] - serial port successfully closed!\n");
    return TRUE;
  }
}

void stuff(unsigned char *buf, int *bufsize)
{
  unsigned char *stuffedBuf = (unsigned char *)malloc(*bufsize);

  int i = 0, j = 0;
  for (; j < *bufsize; i++, j++)
  {
    if (buf[i] == FLAG)
    {
      stuffedBuf = (unsigned char *)realloc(stuffedBuf, ++(*bufsize));
      stuffedBuf[j] = ESCAPE;
      stuffedBuf[j + 1] = FLAG ^ 0x20;
      j++;
    }
    else if (buf[i] == ESCAPE)
    {
      stuffedBuf = (unsigned char *)realloc(stuffedBuf, ++(*bufsize));
      stuffedBuf[j] = ESCAPE;
      stuffedBuf[j + 1] = ESCAPE ^ 0x20;
      j++;
    }
    else
    {
      stuffedBuf[j] = buf[i];
    }
  }

  memcpy(buf, stuffedBuf, *bufsize);
  free(stuffedBuf);
}

int llwrite(int fd, unsigned char *buf, int bufSize)
{
  // allocate frame memory space
  int totalSize = bufSize + 6;
  unsigned char *frame = (unsigned char *)malloc(totalSize);

  // frame construction
  frame[0] = FLAG;
  frame[1] = A_TRANSMITTER;
  dl->frame == 0 ? (frame[2] = C_0) : (frame[2] = C_1);
  frame[3] = frame[1] ^ frame[2];

  // save bcc2 before stuffing
  int bcc2Size = 1;
  unsigned char *bcc2 = (unsigned char *)malloc(sizeof(unsigned char));
  bcc2[0] = buf[0];

  int i = 1;
  for (; i < bufSize; i++)
    bcc2[0] ^= buf[i];

  // data stuffing
  stuff(buf, &bufSize);
  if (bufSize != totalSize - 6)
  {
    // new totalsize, after stuffing data
    totalSize = bufSize + 6;
  }

  for (i = 0; i < bufSize; i++)
    frame[4 + i] = buf[i];

  // bcc2 stuffing
  stuff(bcc2, &bcc2Size);
  if (bcc2Size > 1)
  {
    // in case bcc2 was stuffed
    frame = (unsigned char *)realloc(frame, totalSize + bcc2Size);

    int j = 0;
    for (; j < bcc2Size; j++)
      frame[4 + bufSize + j] = bcc2[j];
  }
  else
  {
    frame[4 + bufSize] = bcc2[0];
  }

  frame[4 + bufSize + bcc2Size] = FLAG;

  // send frame and wait confirmation
  received = FALSE;
  dl->nTries = 0;

  while (!received && dl->nTries < MAX_TRIES)
  {
    // eventually change BCC1 to simulate errors
    unsigned char *errorFrame = errorBCC(frame, totalSize);
    if (memcmp(frame, errorFrame, totalSize) != 0 && DEBUG)
      printf("\n[llwrite] - error inserted in BCC!\n");

    unsigned char *errorFrame2 = errorBCC2(errorFrame, totalSize);
    if (memcmp(errorFrame, errorFrame2, totalSize) != 0 && DEBUG) {
      printf("\n[llwrite] - error inserted in BCC2!\n");
    }

    // send frame
    write(fd, errorFrame2, totalSize);

    if (DEBUG)
    {
      printf("\n[llwrite] - frame sent: ");
      printArr(frame, totalSize);
    }

    alarm(TIMEOUT);

    // read control c parameter
    unsigned char c = readControlC(fd);
    if ((c == C_RR0 && dl->frame == 1) || (c == C_RR1 && dl->frame == 0))
    {
      received = TRUE;

      if (DEBUG)
        printf("[llwrite] - RR%x received for frame %d.\n", dl->frame ^ 1, dl->frame);

      dl->frame ^= 1;
    }
    else if (c == C_REJ0 || c == C_REJ1)
    {
      received = FALSE;
      dl->nTries = 0;

      if (DEBUG)
        printf("[llwrite] - REJ%x received for frame %d.\n", dl->frame ^ 1, dl->frame);
    }

    alarm(0);

    dl->nTries++;
    dl->timeout = 0;
  }

  if (received)
    return 1;
  else
    return -1;
}

// -------------- END OF DATA LINK LAYER ----------------

// --------------------- UTILS --------------------------

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

unsigned char *errorBCC(unsigned char *package, int packageSize)
{
  unsigned char *errorPackage = (unsigned char *)malloc(packageSize);
  memcpy(errorPackage, package, packageSize);

  if ((rand() % 100) < BCC_PE)
  {
    // select a random character
    unsigned char randChar = (unsigned char)('A' + (rand() % 26));

    // insert character in A, C or BCC itself
    int errorIdx = (rand() % 3) + 1;
    errorPackage[errorIdx] = randChar;

    return errorPackage;
  }

  return package;
}

unsigned char *errorBCC2(unsigned char *package, int packageSize)
{
  unsigned char *errorPackage = (unsigned char *)malloc(packageSize);
  memcpy(errorPackage, package, packageSize);

  if ((rand() % 100) < BCC2_PE)
  {
    // select a random character
    unsigned char randChar = (unsigned char)('A' + (rand() % 26));

    // insert character in A, C or BCC itself
    int errorIdx = (rand() % (packageSize - 5)) + 4;
    errorPackage[errorIdx] = randChar;

    return errorPackage;
  }

  return package;
}

void printProgressBar(Application *app)
{
  float percentage = ((float)app->package / app->totalPackages) * 100;

  printf("\rProgress: [");

  int i = 0;
  for (; i < PROGRESS_BAR_DIM; i++)
  {
    if (i < percentage / (100 / PROGRESS_BAR_DIM))
      printf("#");
    else
      printf(" ");
  }

  printf("] %d%%", (int)percentage);
}

void printArr(unsigned char arr[], int size)
{
  int i;
  for (i = 0; i < size; i++)
    printf("%x ", arr[i]);
  printf("\n");
}

// ------------------ END OF UTILS ----------------------
