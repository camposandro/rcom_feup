#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>

#define BAUDRATE      B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE         0
#define TRUE          1

#define TRIES         3

#define FLAG          0x7E
#define ESCAPE        0x7d
#define A             0x03
#define C             0x40
#define C_DISC        0x0B

#define C_UA          0x07

#define DSIZE     10

volatile int RECEIVED = FALSE;

int timeout = 0, n_tries = 0;

typedef enum { INITIAL, STATE_FLAG, STATE_A, STATE_C, STATE_BCC } State;
/* state machine for handling received bytes */
void handle_setup(State* state, unsigned char byte, unsigned char setup[]) {
  switch(*state) {
    case INITIAL:
    if (byte == FLAG) {
      setup[0] = FLAG;
      *state = STATE_FLAG;
    }
    break;
    case STATE_FLAG:
    if (byte == A) {
      setup[1] = A;
      *state = STATE_A;
    } else if (byte == FLAG) {
      setup[0] = FLAG;
      *state = STATE_FLAG;
    } else {
      *state = INITIAL;
    }
    break;
    case STATE_A:
    if (byte == C) {
      setup[2] = C;
      *state = STATE_C;
    } else if (byte == FLAG) {
      setup[0] = FLAG;
      *state = STATE_FLAG;
    } else {
      *state = INITIAL;
    }
    break;
    case STATE_C:
    if (byte == setup[1]^setup[2]) {
      setup[3] = setup[1]^setup[2];
      *state = STATE_BCC;
    } else if (byte == FLAG) {
      setup[0] = FLAG;
      *state = STATE_FLAG;
    } else {
      *state = INITIAL;
    }
    break;
    case STATE_BCC:
    if (byte == FLAG) {
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

void shiftRight(char* buffer, int position, int length)
{
  char temp = buffer[position];
  char temp2;
  buffer[position]=0;

  int i;
  for(i = position ; i <= length ; i++){
    temp2 = buffer[i+1];
    buffer[i+1] = temp;
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

int llwrite(int fd, char* buffer, int length)
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

int llopen(char* port, int flag)
{
  int n_tries = 0;

  unsigned char ua[5], setup[5], buf[255];

  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */
  int fd = open(port, O_RDWR | O_NOCTTY);
  if (fd < 0) { perror(port); exit(-1); }

  /* installs alarm handler */
  (void) signal(SIGALRM, timeout_alarm);

  State state = INITIAL;

  setup[0] = FLAG;
  setup[1] = A;
  setup[2] = C;
  setup[3] = setup[1]^setup[2]; //BCC

  while (!RECEIVED && n_tries < TRIES) {

    /* writes SET to serial port */
    write(fd,setup,5);
    printf("SET sent: ");
    print_arr(setup, 5);
    alarm(3);

    while (!timeout) {

      if (RECEIVED_UA) {
        printf("UA received: ");
        print_arr(ua, 5);
        break;
      }

      read(fd,buf,1);
      handle_setup(&state, buf[0], ua);
    }

    timeout = 0;
    n_tries++;
  }

  return fd;
}

int llclose(int fd)
{
  RECEIVED = FALSE;

  int n_tries = 0;

  unsigned char ua[5], disc[5], buf[255];

  /* installs alarm handler */
  (void) signal(SIGALRM, timeout_alarm);

  State state = INITIAL;

  disc[0] = FLAG;
  disc[1] = A;
  disc[2] = C_DISC;
  disc[3] = disc[1]^disc[2]; //BCC

  while (!RECEIVED && n_tries < TRIES) {

    /* writes SET to serial port */
    write(fd,disc,5);
    printf("DISC sent: ");
    print_arr(disc, 5);
    alarm(3);

    while (!timeout) {

      if (RECEIVED) {
        printf("DISC received: ");
        print_arr(disc, 5);
        break;
      }

      read(fd,buf,1);
      handle_setup(&state, buf[0], disc);
    }

    timeout = 0;
    n_tries++;
  }

  ua[0] = FLAG;
  ua[1] = A;
  ua[2] = C_UA;
  ua[3] = ua[1]^ua[2];

  write(fd,ua,5);

  close(fd);
}

FILE* openfile(char* filepath)
{
  FILE* img = fopen(filepath, "r");
  if (img == NULL) {
    perror("Could not read file!\n");
    return NULL;
  }

  return img;
}

int writefile(int fd, FILE* file)
{
   while (!feof(img)) {
    char c = fgetc(img);
    write(fd,c,1);
    printf("%c",c);
  }
}

int main(int argc, char** argv)
{
  struct termios oldtio, newtio;

  if ((argc < 3) ||
  ((strcmp("/dev/ttyS0", argv[1])!=0) &&
  (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort File\n\tex: nserial /dev/ttyS1 Penguin.gif\n");
    exit(1);
  }

  int fd = llopen(argv[1], 1);

  if (tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
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
  newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 0;   /* blocking read until 1 char is received */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  printf("New termios structure set\n");

  FILE* file = openfile("penguin.gif");
  if (file != NULL)
    writefile(fd,file);

  sleep(2);

  llclose(fd);

  tcsetattr(fd,TCSANOW,&oldtio);

  return 0;
}
