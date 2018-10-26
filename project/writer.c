#include "writer.h"

int RECEIVED = FALSE;
int timeout = 0;
struct termios oldtio, newtio;

unsigned char c;

/* state machine for handling received bytes */
typedef enum
{
  INITIAL,
  STATE_FLAG,
  STATE_A,
  STATE_C,
  STATE_BCC
} State;

void handle_control(State *state, unsigned char byte, unsigned char control[])
{
  switch (*state)
  {
  case INITIAL:
    if (byte == FLAG)
    {
      control[0] = FLAG;
      *state = STATE_FLAG;
    }
    break;
  case STATE_FLAG:
    if (byte == A_TRANSMITTER)
    {
      control[1] = A_TRANSMITTER;
      *state = STATE_A;
    }
    else if (byte == FLAG)
    {
      control[0] = FLAG;
      *state = STATE_FLAG;
    }
    else
    {
      *state = INITIAL;
    }
    break;
  case STATE_A:
    if (byte == c)
    {
      control[2] = c;
      *state = STATE_C;
    }
    else if (byte == FLAG)
    {
      control[0] = FLAG;
      *state = STATE_FLAG;
    }
    else
    {
      *state = INITIAL;
    }
    break;
  case STATE_C:
    if (byte == control[1] ^ control[2])
    {
      control[3] = control[1] ^ control[2];
      *state = STATE_BCC;
    }
    else if (byte == FLAG)
    {
      control[0] = FLAG;
      *state = STATE_FLAG;
    }
    else
    {
      *state = INITIAL;
    }
    break;
  case STATE_BCC:
    if (byte == FLAG)
    {
      RECEIVED = TRUE;
      control[4] = FLAG;
    }
    else
      *state = INITIAL;
    break;
  default:
    break;
  }
}

void shiftRight(char *buffer, int position, int length)
{
  char temp = buffer[position];
  char temp2;
  buffer[position] = 0;

  int i;
  for (i = position; i <= length; i++)
  {
    temp2 = buffer[i + 1];
    buffer[i + 1] = temp;
    temp = temp2;
  }
}

void print_arr(unsigned char arr[], int size)
{
  int i;
  for (i = 0; i < size; i++)
    printf("%x ", arr[i]);
  printf("\n");
}

/* timeout alarm */
void timeout_handler(int signal)
{
  if (signal == SIGALRM)
    timeout = 1;
}

int llwrite(int fd, char *buffer, int length)
{
  /*unsigned char dados[DSIZE], setup[255];
  int numShifts = 0;

  int i = 0;
  while (i < DSIZE)
  {
    if (buffer[i] == FLAG)
    {
      shiftRight(buffer, i, DSIZE);
      buffer[i] = ESCAPE;
      buffer[i + 1] = 0x5e;
      numShifts++;
    }
    else if (buffer[i] == ESCAPE)
    {
      shiftRight(buffer, i, DSIZE);
      buffer[i] = ESCAPE;
      buffer[i + 1] = 0x5d;
      numShifts++;
    }

    i++;
  }

  i = 0;
  while (i < DSIZE + numShifts)
  {
    setup[i + 4] = dados[i];
    i++;
  }

  setup[DSIZE + 4 + numShifts] = bcc2;
  setup[DSIZE + 5 + numShifts] = FLAG;

  close(fileD);

  return 0;*/
}

int llopen(int fd)
{
  unsigned char ua[5], setup[5], buf[255];

  if (tcgetattr(fd, &oldtio) == -1)
  { /* save current port settings */
    perror("llopen - tcgetattr error!\n");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  /*
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
  leitura do(s) próximo(s) caracter(es)
  */
  newtio.c_cc[VTIME] = 1; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 0;  /* blocking read until 1 char is received */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
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

  int n_tries = 0;
  c = C_UA;
  RECEIVED = FALSE;
  while (!RECEIVED && n_tries < TRIES)
  {
    // writes SET to serial port
    write(fd, setup, 5);
    printf("llopen - SET sent: ");
    print_arr(setup, 5);

    alarm(TIMEOUT);

    while (!timeout)
    {
      if (RECEIVED)
      {
        printf("llopen - UA received: ");
        print_arr(ua, 5);
        return fd;
      }

      read(fd, buf, 1);
      handle_control(&state, buf[0], ua);
    }

    alarm(0);

    timeout = 0;
    n_tries++;
  }

  return -1;
}

int llclose(int fd)
{
  unsigned char ua[5], disc[5], buf[255];

  State state = INITIAL;
  disc[0] = FLAG;
  disc[1] = A_TRANSMITTER;
  disc[2] = C_DISC;
  disc[3] = disc[1] ^ disc[2]; //BCC
  disc[4] = FLAG;

  int n_tries = 0;
  c = C_DISC;
  RECEIVED = FALSE;
  while (!RECEIVED && n_tries < TRIES)
  {
    /* writes SET to serial port */
    write(fd, disc, 5);
    printf("llclose - DISC sent: ");
    print_arr(disc, 5);

    alarm(TIMEOUT);

    while (!timeout)
    {
      if (RECEIVED)
      {
        printf("llclose - DISC received: ");
        print_arr(disc, 5);
        break;
      }

      read(fd, buf, 1);
      handle_control(&state, buf[0], disc);
    }

    alarm(0);

    timeout = 0;
    n_tries++;
  }

  if (RECEIVED)
  {
    ua[0] = FLAG;
    ua[1] = A_TRANSMITTER;
    ua[2] = C_UA;
    ua[3] = ua[1] ^ ua[2];
    ua[4] = FLAG;

    write(fd, ua, 5);
    printf("llclose - UA sent: ");
    print_arr(ua, 5);
  }

  tcsetattr(fd, TCSANOW, &oldtio);

  if (close(fd) == -1)
  {
    return -1;
  }
  else
  {
    return 1;
  }
}

int sendfile(Application *app)
{
  time_t start, end;
  int readBytes;

  // open file
  FILE *file = fopen(app->filepath, "rb");
  if (file == NULL)
  {
    printf("openfile: could not read file!\n");
    return -1;
  }

  // allocate array to send data
  char *fileArr = (char *)malloc(MAX_SIZE);

  time(&start);
  while ((readBytes = fread(fileArr, sizeof(char), MAX_SIZE, file)) > 0)
  {
    write(app->fd, fileArr, readBytes);
  }
  time(&end);

  double elapsed = difftime(end, start);
  printf("# File transfered in %.2f seconds!\n", elapsed);

  free(fileArr);

  // close file
  if (fclose(file) == -1)
  {
    printf("closefile: could not close file!\n");
    return -1;
  }

  return 1;
}

void installAlarm()
{
  struct sigaction action;
  action.sa_handler = timeout_handler;
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

int main(int argc, char **argv)
{
  if ((argc < 3) ||
      ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
       (strcmp("/dev/ttyS1", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort File\n\tex: nserial /dev/ttyS1 Penguin.gif\n");
    exit(1);
  }

  Application *app = (Application *)malloc(sizeof(Application));

  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */
  app->fd = open(argv[1], O_RDWR | O_NOCTTY);
  if (app->fd < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  // store file path
  app->filepath = argv[2];

  // install alarm handler
  installAlarm();

  // open connection
  if (llopen(app->fd) == -1)
  {
    printf("llopen: failed to open connection!\n");
    return -1;
  }

  // opens file to transmit
  sendfile(app);

  // close connection
  if (llclose(app->fd) == -1)
  {
    printf("llclose: failed to close connection!\n");
    return -1;
  }

  uninstallAlarm();

  return 0;
}