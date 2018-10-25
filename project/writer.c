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

void handle_control(State *state, unsigned char byte, unsigned char setup[])
{
  switch (*state)
  {
  case INITIAL:
    if (byte == FLAG)
    {
      setup[0] = FLAG;
      *state = STATE_FLAG;
    }
    break;
  case STATE_FLAG:
    if (byte == A_TRANSMITTER)
    {
      setup[1] = A_TRANSMITTER;
      *state = STATE_A;
    }
    else if (byte == FLAG)
    {
      setup[0] = FLAG;
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
      setup[2] = c;
      *state = STATE_C;
    }
    else if (byte == FLAG)
    {
      setup[0] = FLAG;
      *state = STATE_FLAG;
    }
    else
    {
      *state = INITIAL;
    }
    break;
  case STATE_C:
    if (byte == setup[1] ^ setup[2])
    {
      setup[3] = setup[1] ^ setup[2];
      *state = STATE_BCC;
    }
    else if (byte == FLAG)
    {
      setup[0] = FLAG;
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
      setup[4] = FLAG;
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
void timeout_alarm()
{
  timeout = 1;
}

int llwrite(int fd, char *buffer, int length)
{
  unsigned char dados[DSIZE];

  /*fileD = open(argv[2], O_RDONLY);
  if (fd <0) {perror(argv[2]); exit(-1); }

  printf("File opened\n");
  unsigned char setup[255];
  int i = 0;
  int numShifts = 0;

  while(i<DSIZE){

  if(i==5)
  dados[i]=FLAG;
  else if(i==8 )
  dados[i]=ESCAPE;
  else 	dados[i]=0x45;

  bcc2 = bcc2 ^ dados[i];
  i++;
}

//	check FLAGs in dados				o size aumenta nr de flags que houver

i = 0;
while(i < DSIZE){
if(dados[i] == FLAG){
shiftRight(dados, i, DSIZE);
dados[i]=ESCAPE;
dados[i+1]=0x5e;
numShifts++;
} else if(dados[i] == ESCAPE){
shiftRight(dados, i, DSIZE);
dados[i]=ESCAPE;
dados[i+1]=0x5d;
numShifts++;
}

i++;
}


i = 0;
while(i<DSIZE+numShifts){
setup[i+4]=dados[i];
i++;
}

setup[DSIZE+4+numShifts] = bcc2;
setup[DSIZE+5+numShifts] = FLAG;

close(fileD)*/

  return 0;
}

int llopen(int fd)
{
  unsigned char ua[5], setup[5], buf[255];

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
  newtio.c_cc[VTIME] = 1; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 0;  /* blocking read until 1 char is received */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
  {
    perror("llopen - tcsetattr error!");
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

  ua[0] = FLAG;
  ua[1] = A_TRANSMITTER;
  ua[2] = C_UA;
  ua[3] = ua[1] ^ ua[2];

  write(fd, ua, 5);
  printf("llclose - UA sent: ");
  print_arr(ua, 5);

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

int createPacket()
{
  char *packet;
}

int startTransmission(FILE *file)
{
  createPacket();
}

/*
int writefile(int fd, FILE *file)
{
  while (!feof(img))
  {
    char c = fgetc(img);
    write(fd, c, 1);
    printf("%c", c);
  }
}*/

FILE *openfile(char *filepath)
{
  FILE *img = fopen(filepath, "r");
  if (img == NULL)
  {
    printf("openfile: could not read file!\n");
    return NULL;
  }

  return img;
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

  // installs alarm handler
  (void)signal(SIGALRM, timeout_alarm);

  // open connection
  if (llopen(fd) == -1)
  {
    printf("llopen: failed to open connection!");
    return -1;
  }

  /* opens file to transmit
  FILE *file = openfile(argv[2]);
  if (file != NULL)
  {
    //startTransmission(fd, file);
  }*/

  // close connection
  if (llclose(fd) == -1)
  {
    printf("llclose: failed to close connection!");
    return -1;
  }

  return 0;
}
