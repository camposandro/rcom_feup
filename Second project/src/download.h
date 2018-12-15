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
#include <ctype.h>

#define h_addr h_addr_list[0]
#define FTP_PORT 21
#define SOCKET_BUF_SIZE 8192

/**
 * Arguments obtained from the URL
 */
typedef struct URLarguments
{
  char *user;
  char *pwd;
  char *hostname;
  char *filepath;
  char *filename;
  char *hostIp;
} URLarguments;

/**
 * Sockets file descriptors, server address and port
 */
typedef struct Sockets
{
  int controlSockFd;
  int dataSockFd;
  char serverAddr[15]; // maximum length for ipv4
  int serverPort;
} Sockets;

/**
 * States for the URL parsing state machine  
 */
typedef enum URLstate
{
  INIT,
  FTP,
  USER,
  PWD,
  HOST,
  PATH
} URLstate;

/**
 * States for the server response state
 */
typedef enum ResponseState
{
  READ_CODE,
  READ_MSG,
  READ_MULTIPLE,
  FINAL
} ResponseState;

/**
 * Server response code and message
 */
typedef struct ServerResponse
{
  char *code;
  char *msg;
} ServerResponse;

/**
 * Parses the URL arguments (USER, PASS, HOST and PATH)
 */
void parseURL(char *url, struct URLarguments *arguments);

/**
 * Parses the file's name from its path 
 */
char *parseFilename(char *path);

/**
 * Gets the host's ip address according to its name
 */
char *getip(char *hostname);

/**
 * Handles the connection to the server through a control socket
 */
int initConnection(Sockets *sockets, char *ip);

/**
 * Performs login in to the ftp server
 */
int login(Sockets *sockets, char *user, char *pwd);

/**
 * Switches to file directory
 */
int changeDir(Sockets *sockets, char *filepath);

/**
 * Sets passive mode for file tranfer
 */
int enterPasvMode(Sockets *sockets);

/**
 * Handles the connection to the server through a data socket
 */
int openDataConnection(Sockets *sockets);

/**
 * Initializes the file transfer
 */
void transferFile(Sockets *sockets, char *filename);

/**
 * Frees the allocated memory
 */
void freeResources(struct URLarguments *arguments, Sockets *sockets);

/**
 * Handles the creation and the connection to a socket
 */
int openSocket(char *ip, int port);

/**
 * Sends a control command to the server
 */
int sendCmd(Sockets *sockets, char *msg);

/**
 * Retrieves the server response code - read in format [%d%d%d][ -]
 */
void receiveResponse(ServerResponse *response, int sockfd);

/**
 * Calculates server port to connect data socket to
 */
int calculatePort(Sockets *sockets, char *response);

/**
 * Reads data received from the data socket, saving it locally
 */
void saveFile(int fd, char *filename);

/**
 * Outputs the URL parsed arguments to the screen
 */
void printArguments(struct URLarguments *arguments);