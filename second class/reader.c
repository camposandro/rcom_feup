/* INITIAL READER */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#define BAUDRATE 		B38400
#define _POSIX_SOURCE 	1 /* POSIX compliant source */
#define FALSE 			0
#define TRUE 			1

#define FLAG 		0x7E
#define A_RECEIVED 	0x03
#define A_CONFIRM	0x01
#define C 			0x03
#define C_CONFIRM 	0x07
#define BCC 		A^C

volatile int RECEIVED = FALSE;

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
			if (byte == A_RECEIVED) {
				setup[1] = A_RECEIVED;
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

void print_arr(unsigned char arr[], int size) {
	for (int i = 0; i < size; i++)
		printf("%x ",  arr[i]);
	printf("\n");
}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    unsigned char buf[255], setup[5], ua[5];

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
    leitura do(s) pr�ximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }
    printf("New termios structure set\n");

	State state = INITIAL;
    while (RECEIVED == FALSE) {       		/* loop for input */
      	res = read(fd,buf,1);     			/* returns after 1 char has been input */
		handle_input(&state, buf[0], setup);	/* handle byte received */
    }
	printf("SET received: ");
	print_arr(setup, 5);
    
	/* send confirmation */
	for (int i = 0; i < 5; i++) {
		if (i == 1)
			ua[i] = A_CONFIRM;
		else if (i == 2) 
			ua[i] = C_CONFIRM;
		else if (i == 3)
			ua[i] = ua[1]^ua[2];
		else
			ua[i] = setup[i];
	}
	sleep(7);
	write(fd,ua,5);
	printf("UA sent: ");
	print_arr(ua, 5);

	sleep(2);
	
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
