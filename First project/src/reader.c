#include "reader.h"

int DEBUG = FALSE;
int received = FALSE;
int success = FALSE;

DataLink *dl;

int main(int argc, char **argv)
{
  if ((argc < 2) ||
      ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
       (strcmp("/dev/ttyS1", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  // initialize application layer
  Application *app = initApplicationLayer(argv[1]);

  // open connection
  if (!llopen(app->fd))
  {
    printf("llopen: failed to open connection!\n");
    return -1;
  }

  // receives file being transmitted
  receiveFile(app);

  // close connection
  if (!llclose(app->fd))
  {
    printf("llclose: failed to close connection!\n");
    return -1;
  }

  // destroy application layer
  destroyApplicationLayer(app);

  return 0;
}

// --------------- APPLICATION LAYER --------------------

Application *initApplicationLayer(char *port)
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

  // initialize package numeration
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

int receiveFile(Application *app)
{
  // receive first control package
  unsigned char *startPackage = parseStartPackage(app);
  if (startPackage != NULL)
    printf("receiveFile: START package received!\n");
  else
    return -1;

  // allocate array to store received file data
  unsigned char *filebuf = (unsigned char *)malloc(app->filesize);

  off_t idx = 0;
  unsigned char *package;

  while (TRUE)
  {
    int packageSize = 0;
    package = llread(app->fd, &packageSize);
    if (package == NULL)
      continue;

    // if end package is received
    if (endPackageReceived(app->fd, startPackage, package, packageSize))
      break;

    parseDataPackage(package, &packageSize);
    memcpy(filebuf + idx, package, packageSize);
    idx += packageSize;

    app->package++;
    printProgressBar(app);
  }

  saveFile(app->filename, app->filesize, filebuf);

  // deallocate buffer storing file data
  free(filebuf);

  return 1;
}

unsigned char *parseStartPackage(Application *app)
{
  int packageSize = 0;
  unsigned char *startPackage = llread(app->fd, &packageSize);

  if (startPackage == NULL || startPackage[0] != C_START)
  {
    printf("parseStartPackage: package is not START package!\n");
    return NULL;
  }

  // save file size
  off_t filesizeLength = startPackage[2];

  int i = 0;
  app->filesize = 0;
  for (; i < filesizeLength; i++)
    app->filesize |= (startPackage[3 + i] << (8 * filesizeLength - 8 * (i + 1)));

  // save file path
  int filenameLength = (int)startPackage[8];
  app->filename = (unsigned char *)malloc(filenameLength + 1);

  // building the c-string for filepath
  int j = 0;
  for (; j < filenameLength; j++)
    app->filename[j] = startPackage[9 + j];
  app->filename[j] = '\0';

  app->totalPackages = app->filesize / PACKAGE_SIZE;
  if (app->filesize % PACKAGE_SIZE != 0)
    app->totalPackages++;

  return startPackage;
}

int endPackageReceived(int fd, unsigned char *start, unsigned char *package, int packageSize)
{
  // if received package is equal to the start package
  if (package[0] == C_END && !memcmp(start + 1, package + 1, packageSize - 1))
  {
    printf("\nreceiveFile: END package received!\n");
    return TRUE;
  }
  else
    return FALSE;
}

void parseDataPackage(unsigned char *package, int *packageSize)
{
  *packageSize -= 4;
  unsigned char *data = (unsigned char *)malloc(*packageSize);

  int i = 0;
  for (; i < *packageSize; i++)
    data[i] = package[4 + i];

  package = (unsigned char *)realloc(package, *packageSize);
  memcpy(package, data, *packageSize);

  free(data);
}

// ----------- END OF APPLICATION LAYER -----------------

// ---------------- DATA LINK LAYER ---------------------

DataLink *initDataLinkLayer()
{
  // allocate data link struct
  dl = (DataLink *)malloc(sizeof(DataLink));

  // initialize data link struct
  dl->frame = 0;
  dl->expectedFrame = 0;

  return dl;
}

void destroyDataLinkLayer()
{
  // free datalink layer
  free(dl);
}

int llopen(int fd)
{
  unsigned char ua[5];

  if (tcgetattr(fd, &dl->oldtio) == -1)
  { /* save current port settings */
    perror("llopen - tcgetattr error!");
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
  dl->newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  dl->newtio.c_cc[VMIN] = 1;  /* blocking read until 1 char is received */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &dl->newtio) == -1)
  {
    perror("llopen - tcsetattr error!");
    exit(-1);
  }
  printf("llopen - new termios structure set\n");

  // wait for SET control frame
  unsigned char *setup = readControlFrame(fd, C_SET);
  printf("llopen - SET received: ");
  printArr(setup, 5);

  // send UA confirmation
  ua[0] = setup[0];
  ua[1] = A_TRANSMITTER;
  ua[2] = C_UA;
  ua[3] = ua[1] ^ ua[2];
  ua[4] = setup[4];

  write(fd, ua, 5);
  printf("llopen - UA sent: ");
  printArr(ua, 5);

  printf("llopen - connection successfully opened!\n");
  return TRUE;
}

int llclose(int fd)
{
  // wait for DISC control frame
  unsigned char *disc = readControlFrame(fd, C_DISC);
  printf("llclose - DISC received: ");
  printArr(disc, 5);

  // send DISC confirmation frame
  write(fd, disc, 5);
  printf("llclose - DISC sent: ");
  printArr(disc, 5);

  // wait for UA control frame
  unsigned char *ua = readControlFrame(fd, C_UA);
  printf("llclose - UA received: ");
  printArr(ua, 5);

  tcsetattr(fd, TCSANOW, &dl->oldtio);

  printf("llclose - connection successfully closed!\n");

  return TRUE;
}

unsigned char *readControlFrame(int fd, unsigned char c)
{
  unsigned char *control = (unsigned char *)malloc(5 + sizeof(unsigned char));
  unsigned char byte;

  State state = INITIAL;
  received = FALSE;

  while (!received)
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

unsigned char *llread(int fd, int *frameSize)
{
  int stuffedByte = 0;
  unsigned char control[5], byte, c;

  unsigned char *frameI = (unsigned char *)malloc(*frameSize);

  State state = INITIAL;
  received = FALSE, success = FALSE;

  while (!received)
  {
    read(fd, &byte, 1);

    switch (state)
    {
    case INITIAL:
      if (byte == FLAG)
        state = STATE_FLAG;
      break;
    case STATE_FLAG:
      if (byte == A_TRANSMITTER)
        state = STATE_A;
      else if (byte == FLAG)
        state = STATE_FLAG;
      else
        state = INITIAL;
      break;
    case STATE_A:
      if (byte == C_0)
      {
        c = byte;
        dl->frame = 0;
        state = STATE_C;
      }
      else if (byte == C_1)
      {
        c = byte;
        dl->frame = 1;
        state = STATE_C;
      }
      else if (byte == FLAG)
        state = STATE_FLAG;
      else
        state = INITIAL;
      break;
    case STATE_C:
      if (byte == (A_TRANSMITTER ^ c))
        state = STATE_BCC;
      else if (byte == FLAG)
        state = STATE_FLAG;
      else
        state = INITIAL;
      break;
    case STATE_BCC:
      // if FLAG is received
      if (byte == FLAG)
      {
        // if BCC2 was correct
        if (receivedBcc2(frameI, *frameSize))
          acceptFrame(fd);
        else
          rejectFrame(fd);

        received = TRUE;
      }
      // if ESCAPE is received
      else if (byte == ESCAPE)
        stuffedByte = 1;
      else if (stuffedByte)
      {
        frameI = (unsigned char *)realloc(frameI, ++(*frameSize));
        frameI[*frameSize - 1] = byte ^ 0x20;
        stuffedByte = 0;
      }
      else
      {
        frameI = (unsigned char *)realloc(frameI, ++(*frameSize));
        frameI[*frameSize - 1] = byte;
      }
      break;
    default:
      break;
    }
  }

  // remove bcc2 from data frame
  frameI = (unsigned char *)realloc(frameI, --(*frameSize));

  if (success)
  {
    return frameI;
  }
  else
  {
    free(frameI);
    return NULL;
  }
}

void acceptFrame(int fd)
{
  unsigned char control[5];

  if (dl->frame == 0)
  {
    // send C_RR1
    control[0] = FLAG;
    control[1] = A_TRANSMITTER;
    control[2] = C_RR1;
    control[3] = control[1] ^ control[2];
    control[4] = FLAG;

    write(fd, control, 5);
  }
  else if (dl->frame == 1)
  {
    // send C_RR0
    control[0] = FLAG;
    control[1] = A_TRANSMITTER;
    control[2] = C_RR0;
    control[3] = control[1] ^ control[2];
    control[4] = FLAG;

    write(fd, control, 5);
  }

  // if RR was sent
  if (dl->frame == dl->expectedFrame)
  {
    dl->expectedFrame ^= 1;
    success = TRUE;

    if (DEBUG)
      printf("\nllread: RR%x sent for frame %d!\n", dl->frame ^ 1, dl->frame);
  }
}

void rejectFrame(int fd)
{
  unsigned char control[5];

  if (dl->frame == 0)
  {
    // send C_REJ1
    control[0] = FLAG;
    control[1] = A_TRANSMITTER;
    control[2] = C_REJ1;
    control[3] = control[1] ^ control[2];
    control[4] = FLAG;

    write(fd, control, 5);
  }
  else if (dl->frame == 1)
  {
    // send C_REJ0
    control[0] = FLAG;
    control[1] = A_TRANSMITTER;
    control[2] = C_REJ0;
    control[3] = control[1] ^ control[2];
    control[4] = FLAG;

    write(fd, control, 5);
  }

  success = FALSE;

  if (DEBUG)
    printf("\nllread: REJ%x sent for frame %d!\n", dl->frame ^ 1, dl->frame);
}

// -------------- END OF DATA LINK LAYER ----------------

// --------------------- UTILS --------------------------

int receivedBcc2(unsigned char *frameI, int frameSize)
{
  if (frameSize == 0)
    return FALSE;

  unsigned char bcc2 = frameI[0];

  int i = 1;
  for (; i < frameSize - 1; i++)
    bcc2 ^= frameI[i];

  if (frameI[frameSize - 1] == bcc2)
    return TRUE;
  else
    return FALSE;
}

int saveFile(unsigned char *filename, off_t filesize, unsigned char *filebuf)
{
  // open new file
  FILE *file = fopen(filename, "wb");
  if (file == NULL)
  {
    perror("saveFile: could not open new file!\n");
    exit(-1);
  }

  if (fwrite(filebuf, sizeof(unsigned char), filesize, file) == -1)
  {
    printf("saveFile: error writing to file %s!\n", filename);
    return -1;
  }

  if (fclose(file) == -1)
  {
    printf("saveFile: error closing file %s!\n", filename);
    return -1;
  }

  return 1;
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
