#include "download.h"

// test file: ftp://anonymous:anonymous@speedtest.tele2.net/1KB.zip

int main(int argc, char **argv)
{
  Sockets sockets;
  struct URLarguments arguments;

  if (argc != 2)
  {
    fprintf(stderr, "# [main] usage: ./download ftp://<user>:<password>@<host>/<url-path>\n");
    exit(1);
  }

  /*** URL PROCESSING ***/

  // parse url argument
  parseURL(argv[1], &arguments);
  // parse filename from the file's path
  char *filename = parseFilename(arguments.path);
  // get host ip address
  char *ip = getip(arguments.host);

  /*** FTP CONNECTION HANDLING ***/

  // init ftp connection
  initConnection(&sockets, ip);

  // login to the server
  login(&sockets, arguments.user, arguments.pwd);

  // enter pasv
  enterPasvMode(&sockets);

  // free the allocated mem
  freeResources(arguments, filename);
  // close the open sockets
  close(sockets.controlSocketFd);
  close(sockets.dataSocketFd);

  return 0;
}

int initConnection(Sockets *sockets, char *ip)
{
  // open login socket
  sockets->controlSocketFd = openSocket(ip);

  // wait for server response
  char *responseCode = receiveResponse(sockets->controlSocketFd);
  // if code is invalid exit
  if (responseCode[0] != '2')
  {
    printf("# [initConnection]: Error connecting to server!\n");
    exit(0);
  }
}

void parseURL(char *url, struct URLarguments *arguments)
{
  // allocate empty argument buffers
  arguments->user = (char *)malloc(0);
  arguments->pwd = (char *)malloc(0);
  arguments->host = (char *)malloc(0);
  arguments->path = (char *)malloc(0);

  // initial state
  URLstate state = INIT;
  const char *ftp = "ftp://";

  int i = 0, j = 0;
  for (i; i < strlen(url); i++)
  {
    // state machine for url matching
    switch (state)
    {
    case INIT:
      if (url[i] == ftp[i])
      {
        if (i == 5)
          state = FTP;
        continue;
      }
      else
      {
        fprintf(stderr, "# [parseURL]: Error parsing ftp://\n");
      }
      break;
    case FTP:
      if (url[i] == ':')
      {
        state = USER;
        j = 0;
      }
      else
      {
        arguments->user = (char *)realloc(arguments->user, j + 1);
        arguments->user[j++] = url[i];
      }
      break;
    case USER:
      if (url[i] == '@')
      {
        state = PWD;
        j = 0;
      }
      else
      {
        arguments->pwd = (char *)realloc(arguments->pwd, j + 1);
        arguments->pwd[j++] = url[i];
      }
      break;
    case PWD:
      if (url[i] == '/')
      {
        state = HOST;
        j = 0;
      }
      else
      {
        arguments->host = (char *)realloc(arguments->host, j + 1);
        arguments->host[j++] = url[i];
      }
      break;
    case HOST:
      arguments->path = (char *)realloc(arguments->path, j + 1);
      arguments->path[j++] = url[i];
      break;
    default:
      fprintf(stderr, "# [parseURL]: Invalid URL state\n");
      exit(1);
    }
  }

  printArguments(arguments);
}

char *parseFilename(char *path)
{
  // allocate empty filename buffer
  char *filename = (char *)malloc(0);

  int i = 0, j = 0;
  for (i; i < strlen(path); i++)
  {
    if (path[i] == '/')
    {
      // reset filename buffer
      filename = (char *)realloc(filename, 0);
      j = 0;
    }
    else
    {
      filename = (char *)realloc(filename, j + 1);
      filename[j++] = path[i];
    }
  }

  printf("# Filename: %s\n", filename);

  return filename;
}

char *getip(char *hostname)
{
  struct hostent *h;

  /* 
      struct hostent {
        char    *h_name;	Official name of the host. 
            char    **h_aliases;	A NULL-terminated array of alternate names for the host. 
        int     h_addrtype;	The type of address being returned; usually AF_INET.
            int     h_length;	The length of the address in bytes.
        char    **h_addr_list;	A zero-terminated array of network addresses for the host. 
              Host addresses are in Network Byte Order. 
      };
    */

  if ((h = gethostbyname(hostname)) == NULL)
  {
    herror("# gethostbyname");
    exit(1);
  }

  char *ip_addr = inet_ntoa(*((struct in_addr *)h->h_addr));

  printf("# Host name : %s\n", h->h_name);
  printf("# Host IP Address : %s\n\n", ip_addr);

  return ip_addr;
}

int openSocket(char *ip)
{
  // socket file descriptor
  int sockFd;
  // server struct
  struct sockaddr_in server_addr;

  /* server address handling */
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip); /* 32 bit Internet address network byte ordered */
  server_addr.sin_port = htons(FTP_PORT);      /* server TCP port must be network byte ordered */

  /* open an TCP socket */
  if ((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("# socket()");
    exit(0);
  }

  /* connect to the server */
  if (connect(sockFd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("# connect()");
    exit(0);
  }

  return sockFd;
}

char *receiveResponse(int sockfd)
{
  char *responseCode = (char *)malloc(3 * sizeof(char));

  char c;
  int i = 0;

  ResponseState state = READ_CODE;
  while (state != FINAL)
  {
    read(sockfd, &c, 1);

    switch (state)
    {
    case READ_CODE:
      if (c == ' ')
      {
        if (i == 3)
          state = READ_MSG;
        else
          i = 0;
      }
      else if (isdigit(c) && i < 3)
        responseCode[i++] = c;
      break;
    case READ_MSG:
      if (c == '\n')
        state = FINAL;
      break;
    default:
      fprintf(stderr, "# [getResponse]: Invalid response state");
      break;
    }
  }

  printf("# [getResponse]: %s\n\n", responseCode);

  return responseCode;
}

int login(Sockets *sockets, char *user, char *pwd)
{
  char *loginResponse, *passResponse;
  char *msg = (char *)malloc(sizeof(user) + 5);

  // send user
  sprintf(msg, "user %s\n", user);
  printMessage(msg);
  sendCmd(sockets, msg);
  printf("# [login]: User sent!\n");

  loginResponse = receiveResponse(sockets->controlSocketFd);
  if (loginResponse[0] != '3')
  {
    printf("# [login]: Error receiving user response!\n");
    exit(0);
  }

  // send password
  sprintf(msg, "pass %s\n", pwd);
  printMessage(msg);
  sendCmd(sockets, msg);
  printf("# [login]: Pass sent!\n");

  passResponse = receiveResponse(sockets->controlSocketFd);
  if (passResponse[0] != '2')
  {
    printf("# [login]: Error receiving pass response!\n");
    exit(0);
  }

  return 0;
}

int enterPasvMode(Sockets *sockets)
{
  char *pasvResponse;

  printMessage("pasv");
  sendCmd(sockets, "pasv\n");
  printf("# [enterPasvMode]: Entering pasv mode!\n");

  pasvResponse = receiveResponse(sockets->controlSocketFd);
  if (pasvResponse[0] != '2')
  {
    printf("# [enterPasvMode]: Error entering passive mode!\n");
    exit(0);
  }

  return 0;
}

int sendCmd(Sockets *sockets, char *msg)
{
  if (write(sockets->controlSocketFd, msg, strlen(msg)) < 0)
  {
    perror("# [sendCmd]: Error sending control message\n");
    return 1;
  }

  return 0;
}

void printMessage(char *msg)
{
  printf("# %s\n", msg);
}

void printArguments(struct URLarguments *arguments)
{
  printf("\nURLarguments:\n");
  printf("# User: %s\n", arguments->user);
  printf("# Password: %s\n", arguments->pwd);
  printf("# Host: %s\n", arguments->host);
  printf("# Path: %s\n\n", arguments->path);
}

void freeResources(struct URLarguments arguments, char *filename)
{
  free(arguments.user);
  free(arguments.pwd);
  free(arguments.host);
  free(arguments.path);
  free(filename);
}