/* INITIAL READER */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A 0x03
#define C 0x03
#define BCC A^C

#define C_CONFIRM 0x07

volatile int RECEIVED=FALSE;


typedef enum { INITIAL, STATE_FLAG, STATE_A, STATE_C, STATE_BCC } State;

/* state machine for handling received bytes */
void handle_input(State* state, unsigned char byte, unsigned char setup[]) {
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

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];

    if ((argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 char is received */


  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }
    printf("New termios structure set\n");


	unsigned char setup[5];
	State state = INITIAL;

    while (RECEIVED==FALSE) {       		/* loop for input */
      res = read(fd,buf,1);     			/* returns after 1 char has been input */
	  handle_input(&state, buf[0], setup);	/* handle byte received */
    }
	printf("SET received!\n");
    
	/* send confirmation */
	setup[2] = C_CONFIRM;
	write(fd,setup,5);
	printf("UA sent!\n");
	sleep(2);
    
   /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião 
   */

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
