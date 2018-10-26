#include "reader.h"

int RECEIVED = FALSE;
int stuffedByte = 0;
int dataIdx = 4;
struct termios oldtio, newtio;

unsigned char c;

typedef enum
{
  INITIAL,
  STATE_FLAG,
  STATE_A,
  STATE_C,
  STATE_BCC,
  STATE_DATA,
  STATE_BCC2
} State;

/* state machine for handling received bytes */
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
      control[4] = FLAG;
      RECEIVED = TRUE;
    }
    else
    {
      *state = INITIAL;
    }
    break;
  default:
    break;
  }
}
/*
int nrBytes = 0;
void handle_frame(State *state, unsigned char byte, unsigned char frameI[])
{
  switch (*state)
  {
  case INITIAL:
    if (byte == FLAG)
    {
      frameI[0] = FLAG;
      *state = STATE_FLAG;
    }
    break;
  case STATE_FLAG:
    if (byte == A_RECEIVED)
    {
      frameI[1] = A_RECEIVED;
      *state = STATE_A;
    }
    else if (byte == FLAG)
    {
      frameI[0] = FLAG;
      *state = STATE_FLAG;
    }
    else
    {
      *state = INITIAL;
    }
    break;
  case STATE_A:
    if (byte == C_RECEIVED)
    {
      frameI[2] = C_RECEIVED;
      *state = STATE_C;
    }
    else if (byte == FLAG)
    {
      frameI[0] = FLAG;
      *state = STATE_FLAG;
    }
    else
    {
      *state = INITIAL;
    }
    break;
  case STATE_C:
    if (byte == frameI[1] ^ frameI[2])
    {
      frameI[3] = frameI[1] ^ frameI[2];
      *state = STATE_BCC;
    }
    else if (byte == FLAG)
    {
      frameI[0] = FLAG;
      *state = STATE_FLAG;
    }
    else
    {
      *state = INITIAL;
    }
    break;
  case STATE_BCC:
    if (byte == ESCAPE)
    {
      stuffedByte = 1;
    }
    else if (stuffedByte)
    {
      frameI[dataIdx++] = byte ^ 0x20;
      stuffedByte = 0;
    }
    else
    {
      frameI[dataIdx++] = byte;
      nrBytes++;
    }
    if (nrBytes == D_SIZE)
    {
      nrBytes = 0;
      *state = STATE_DATA;
    }
    break;
  case STATE_DATA:
  {
    unsigned char bcc2 = frameI[4];

    int i;
    for (i = 5; i < dataIdx; i++)
    {
      bcc2 ^= frameI[i];
    }

    if (byte == bcc2)
    {
      frameI[dataIdx++] = byte;
      *state = STATE_BCC2;
    }
    else
    {
      *state = INITIAL;
    }
  }
  break;
  case STATE_BCC2:
  {
    if (byte == FLAG)
    {
      frameI[dataIdx] = FLAG;
      RECEIVED = TRUE;
    }
    else
    {
      *state = INITIAL;
    }
  }
  break;
  default:
    break;
  }
}

int llread(int fd, char *buffer)
{
  int read = 0;
  unsigned char setup[5], frame[MAX_SIZE];

  State state = INITIAL;
  while (RECEIVED == FALSE)
  {
    read += read(fd, buffer, 1);         // returns after 1 char has been input /
    handle_setup(&state, buf[0], setup); // handle byte received /
  }
}*/

void print_arr(unsigned char arr[], int size)
{
  int i;
  for (i = 0; i < size; i++)
    printf("%x ", arr[i]);
  printf("\n");
}

int receivefile(int fd)
{
  int readBytes;

  FILE* file = fopen("penguin2.gif", "a"); 

  char* buf = (char *) malloc(MAX_SIZE);

  while ((readBytes = read(fd, buf, MAX_SIZE)) > 0) {
    fwrite(buf, sizeof(char), readBytes, file);
  }

  fclose(file);

  return 1;
}

int llclose(int fd)
{
  unsigned char ua[5], disc[5], buf[255];

  State state = INITIAL;
  c = C_DISC;
  RECEIVED = FALSE;
  while (RECEIVED == FALSE)
  {                                       /* loop for input */
    read(fd, buf, 1);                     /* returns after 1 char has been input */
    handle_control(&state, buf[0], disc); /* handle byte received */
  }
  printf("llclose - DISC received: ");
  print_arr(disc, 5);

  /* send confirmation DISC */
  write(fd, disc, 5);
  printf("llclose - DISC sent: ");
  print_arr(disc, 5);

  state = INITIAL;
  c = C_UA;
  RECEIVED = FALSE;
  while (RECEIVED == FALSE)
  {                                     /* loop for input */
    read(fd, buf, 1);                   /* returns after 1 char has been input */
    handle_control(&state, buf[0], ua); /* handle byte received */
  }
  printf("llclose - UA received: ");
  print_arr(ua, 5);

  tcsetattr(fd, TCSANOW, &oldtio);

  return fd;
}

int llopen(int fd)
{
  unsigned char buf[255], setup[5], ua[5];

  if (tcgetattr(fd, &oldtio) == -1)
  { /* save current port settings */
    perror("llopen - tcgetattr error!");
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
  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 1 char is received */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
  {
    perror("llopen - tcsetattr error!");
    exit(-1);
  }
  printf("llopen: new termios structure set\n");

  State state = INITIAL;
  c = C_SET;
  RECEIVED = FALSE;
  while (RECEIVED == FALSE)
  {                                        /* loop for input */
    read(fd, buf, 1);                      /* returns after 1 char has been input */
    handle_control(&state, buf[0], setup); /* handle byte received */
  }
  printf("llopen - SET received: ");
  print_arr(setup, 5);

  /* send confirmation */
  ua[0] = setup[0];
  ua[1] = A_TRANSMITTER;
  ua[2] = C_UA;
  ua[3] = ua[1] ^ ua[2];
  ua[4] = setup[4];

  write(fd, ua, 5);
  printf("llopen - UA sent: ");
  print_arr(ua, 5);

  return fd;
}

int main(int argc, char **argv)
{
  if ((argc < 2) ||
      ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
       (strcmp("/dev/ttyS1", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */
  int fd = open(argv[1], O_RDWR | O_NOCTTY);
  if (fd < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  // open connection
  llopen(fd);

  receivefile(fd);

  // close connection
  llclose(fd);

  return 0;
}
