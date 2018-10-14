/* INITIAL WRITER */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <signal.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 		0x7E
#define A	 		0x03
#define A_RECEIVED	0x01
#define C 			0x03
#define C_RECEIVED 	0x07
#define BCC 		A^C

int timeout = 0, n_tries = 0;

volatile int RECEIVED_UA = FALSE;

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
			if (byte == C_RECEIVED) {
				setup[2] = C_RECEIVED;
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
				RECEIVED_UA = TRUE;
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
		printf("%x ", arr[i]);
	printf("\n");
}

/* timeout alarm */
void timeout_alarm() {
	timeout = 1;
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

    newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 0;   /* unables the read call to unblock */


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

	/* installs alarm handler */
	(void) signal(SIGALRM, timeout_alarm);

	setup[0] = FLAG;
	setup[1] = A;
	setup[2] = C;
	setup[3] = A^C;
	setup[4] = FLAG;

	State state = INITIAL;
	while (!RECEIVED_UA && n_tries < 3) {

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

			res = read(fd,buf,1);     			
			handle_input(&state, buf[0], ua);
    	}

		timeout = 0;
		n_tries++;
	}

    tcsetattr(fd,TCSANOW,&oldtio);

    close(fd);
    return 0;
}
