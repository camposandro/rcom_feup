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

// ESCAPE flag
#define ESCAPE 0x7d

// data package flags
#define C_START 0x02 // control package start state byte
#define C_END 0x03   // control package end state byte

// information frame flags
#define PACKAGE_SIZE 255
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

typedef enum
{
    INITIAL,
    STATE_FLAG,
    STATE_A,
    STATE_C,
    STATE_BCC,
    STATE_FINAL
} State;

typedef struct
{
    int fd;                  // serial port descriptor
    unsigned char *filepath; // path of received file
    off_t filesize;          // filesize to be received
} Application;

typedef struct
{
    int frame;                     // current frame number
    int expectedFrame;             // expected frame number
    struct termios oldtio, newtio; // structs storing terminal configurations
} DataLink;

// --------------- APPLICATION LAYER --------------------

Application *initApplicationLayer(char *port, char *filepath);

void destroyApplicationLayer(Application *app);

int receiveFile(Application *app);

unsigned char *parseStartPackage(Application *app);

int endPackageReceived(int fd, unsigned char *start, unsigned char *package, int packageSize);

void parseDataPackage(unsigned char *package, int *packageSize);

// ----------- END OF APPLICATION LAYER -----------------

// ---------------- DATA LINK LAYER ---------------------

DataLink *initDataLinkLayer();

void destroyDataLinkLayer();

int llopen(int fd);

int llclose(int fd);

unsigned char *llread(int fd, int *frameSize);

unsigned char *readControlFrame(int fd, unsigned char c);

// -------------- END OF DATA LINK LAYER ----------------

// --------------------- UTILS --------------------------

int receivedBcc2(unsigned char *frameI, int frameSize);

int saveFile(unsigned char *filepath, off_t filesize, unsigned char *filebuf);

void printArr(unsigned char arr[], int size);

// ------------------ END OF UTILS ----------------------
