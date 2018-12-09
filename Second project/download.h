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

#define SERVER_PORT 6000
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
struct hostent *getip(char *hostname);

/**
 * Outputs the URL parsed arguments to the screen
 */
void printArguments(struct URLarguments arguments);

/**
 * Frees the allocated memory
 */
void freeResources(struct URLarguments arguments, char *filename);