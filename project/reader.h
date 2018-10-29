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

#define DEBUG printf("DEBUG: %s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__);

#define ESCAPE 0x7d

// data package flags
#define C_START 0x02 // control package start state byte
#define C_END 0x03   // control package end state byte

// information frame flags
#define MAX_SIZE 255
#define DSIZE 10
#define C_0 0x00
#define C_1 0x40

// control frame flags
#define FLAG 0x7E
#define A_TRANSMITTER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_DISC 0x0B
#define C_UA 0x07
#define C_RR0 0x05
#define C_RR1 0x85
#define C_REJ0 0x01
#define C_REJ1 0x81

typedef struct
{
    int fd;         // serial port descriptor
    char *filepath; // path of received file
    off_t filesize; // filesize to be received
} Application;

typedef struct
{
    int frame;                     // current frame number
    int expectedFrame;             // expected frame number
    struct termios oldtio, newtio; // structs storing terminal configurations
} DataLink;