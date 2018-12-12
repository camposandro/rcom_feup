#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>

#define h_addr h_addr_list[0]
#define FTP_PORT     21
#define SERVER_ADDR "192.168.28.96"

/**
 * Arguments obtained from the URL
 */
typedef struct URLarguments {
  char *user;
  char *pwd;
  char *host;
  char *path;
} URLarguments;

/**
 * States for the URL parsing state machine  
 */
typedef enum URLstate {
  INIT,
  FTP,
  USER,
  PWD,
  HOST,
  PATH
} URLstate;

/**
 * Sockets file descriptors
 */
typedef struct Sockets {
  int controlSocketFd;
  int dataSocketFd;
} Sockets;

typedef enum ResponseState {
  READ_CODE,
  READ_MSG,
  FINAL
} ResponseState;

/**
 * Parses the URL arguments (USER, PASS, HOST and PATH)
 */
void parseURL(char *url, struct URLarguments *arguments);

/**
 * Parses the file's name from its path 
 */
char* parseFilename(char *path);

/**
 * Gets the host's ip address according to its name
 */
char *getip(char *hostname);

/**
 * Handles the creation and the connection to a socket
 */
int openSocket(char* serverAddr);

/**
 * Retrieves the server response code - read in format [%d%d%d][ -]
 */
char *receiveResponse(int sockfd);

/**
 * Outputs the URL parsed arguments to the screen
 */
void printArguments(struct URLarguments *arguments);

/**
 * Frees the allocated memory
 */
void freeResources(struct URLarguments arguments, char *filename);