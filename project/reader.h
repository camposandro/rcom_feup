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

#define BAUDRATE 	    B38400
#define _POSIX_SOURCE 	1 /* POSIX compliant source */
#define FALSE 		    0
#define TRUE 		    1

#define ESCAPE 		    0x7d

#define FLAG 		    0x7e
#define A_TRANSMITTER   0x03
#define A_RECEIVER      0x01 
#define C_SET	        0x03
#define C_DISC   	    0x0B
#define C_UA            0x07

#define MAX_SIZE	  255
#define DSIZE           10