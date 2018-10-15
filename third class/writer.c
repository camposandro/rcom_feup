/* INITIAL READER */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define ESCAPE 0x7d
#define A 0x03
#define C 0x40

#define C_CONFIRM 0x07

#define DSIZE 10

volatile int RECEIVED_UA=FALSE;

int timeout = 0, n_tries = 0;


typedef enum { INITIAL, STATE_FLAG, STATE_A, STATE_C, STATE_BCC } State;

void shiftRight();
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
    int fd, fileD, c, res, bcc2=0x00, ua[5], buf[255];
	unsigned char dados[DSIZE];
    struct termios oldtio,newtio;

    if ((argc < 3) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort File\n\tex: nserial /dev/ttyS1 Penguin.gif\n");
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
    newtio.c_cc[VMIN]     = 0;   /* blocking read until 1 char is received */


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


	fileD = open(argv[2], O_RDONLY);
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

	/* installs alarm handler */
	(void) signal(SIGALRM, timeout_alarm);

	State state = INITIAL;

	setup[0] = FLAG;
	setup[1] = A;
	setup[2] = C;
	setup[3] = setup[1]^setup[2]; //BCC


	i = 0;
	while(i<DSIZE+numShifts){
		setup[i+4]=dados[i];
		i++;
	}

	setup[DSIZE+4+numShifts] = bcc2;
	setup[DSIZE+5+numShifts] = FLAG;

	

	while (!RECEIVED_UA && n_tries < 3) {

		/* writes SET to serial port */
		write(fd,setup,DSIZE+6+numShifts);
		printf("SET sent: ");
		print_arr(setup, DSIZE+6+numShifts);
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


	close(fileD);
    close(fd);
    return 0;
}

void shiftRight(char* buffer, int position, int length){
	char temp = buffer[position];
	char temp2;
	buffer[position]=0;
	for(int i = position ; i <= length ; i++){
		temp2 = buffer[i+1];
		buffer[i+1] = temp;
		temp = temp2; 
	}
}
