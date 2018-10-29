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
#include <time.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define TIMEOUT 3 // number of seconds for timeout
#define MAX_TRIES 3   // number of maximum timeout tries

#define ESCAPE 0x7d

// data package flags
#define C_START 0x02 // control package start state byte
#define C_END 0x03   // control package end state byte

// control package flags
#define DATAFRAME_C 0x01
#define T1 0x00 // filesize parameter type
#define L1 0x04 // filesize 4 octets
#define T2 0x01 // filepath parameter type

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
    int fd;            // serial port descriptor
    char *filepath;    // path of file to be sent
    int package;       // number of current package
    int totalpackages; // total number of packages
} Application;

typedef struct
{
    int frame;                     // current frame number
    int timeout;                   // timeout flag
    int ntries;                    // number of timeout tries
    struct termios oldtio, newtio; // structs storing terminal configurations
} DataLink;

Application *initApplicationLayer(char *port, char *filepath);

void destroyApplicationLayer(Application *app);

unsigned char *getDataPackage(Application *app, unsigned char *buf, off_t filesize, int *packagesize);

int sendDataPackage(Application *app, unsigned char *buf, int packagesize);

unsigned char *getControlPackage(unsigned char c, off_t filesize, char *filepath, int *packagesize);

int sendControlPackage(int fd, int c, off_t filesize, char *filepath);

int sendFile(Application *app);

DataLink *initDataLinkLayer();

void destroyDataLinkLayer();

unsigned char *readControlFrame(int fd, unsigned char c);

unsigned char readControlC(int fd);

int llopen(int fd);

int llclose(int fd);

int llwrite(int fd, unsigned char *buf, int bufsize);

void timeoutHandler(int signo);

void installAlarm();

void uninstallAlarm();

void stuff(unsigned char *buf, int *bufsize);

void printArr(unsigned char arr[], int size);