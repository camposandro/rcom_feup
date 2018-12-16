#include "download.h"

/* 
:TEST FILES:
ftp://anonymous:anonymous@speedtest.tele2.net/1KB.zip
ftp://anonymous:anonymous@ftp.up.pt/parrot/README.html
*/

int main(int argc, char **argv)
{
  Sockets sockets;
  struct URLarguments arguments;

  if (argc != 2)
  {
    fprintf(stderr, "# Usage: ./download ftp://<user>:<password>@<host>/<url-path>\n");
    exit(1);
  }

  /*** URL PROCESSING ***/

  // parse url argument
  parseURL(argv[1], &arguments);

  /*** FTP CONNECTION HANDLING ***/

  // init ftp connection
  initConnection(&sockets, arguments.hostIp);

  // login to the server
  login(&sockets, arguments.user, arguments.pwd);

  // change to file directory
  changeDir(&sockets, arguments.filepath);

  // enter pasv
  enterPasvMode(&sockets);

  // open data socket
  openDataConnection(&sockets);

  // require file for transfer
  transferFile(&sockets, arguments.filename);

  // free the allocated mem
  freeResources(&arguments, &sockets);

  return 0;
}

void parseURL(char *url, struct URLarguments *arguments)
{
  // allocate empty argument buffers
  arguments->user = (char *)malloc(0);
  arguments->pwd = (char *)malloc(0);
  arguments->hostname = (char *)malloc(0);
  arguments->filepath = (char *)malloc(0);

  // initial state
  URLstate state = INIT;
  const char *ftp = "ftp://";

  int i = 0, j = 0;
  for (; i < strlen(url); i++)
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
        printf("# Error parsing ftp://\n");
        exit(0);
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
        arguments->hostname = (char *)realloc(arguments->hostname, j + 1);
        arguments->hostname[j++] = url[i];
      }
      break;
    case HOST:
      arguments->filepath = (char *)realloc(arguments->filepath, j + 1);
      arguments->filepath[j++] = url[i];
      break;
    default:
      printf("# Invalid URL state\n");
      exit(0);
    }
  }

  // parse filename from the file's path
  arguments->filename = parseFilename(arguments->filepath);

  // remove filename in filepath
  char *path, *lastSlash = strrchr(arguments->filepath, '/');
  if (lastSlash != NULL)
  {
    int index = lastSlash - arguments->filepath;

    path = (char *)malloc(index);
    memcpy(path, arguments->filepath, index);
  }
  else
  {
    // directory is the root
    path = (char *)malloc(sizeof(char));
    *path = '.';
  }
  free(arguments->filepath);
  arguments->filepath = path;

  // get host ip address
  arguments->hostIp = getip(arguments->hostname);

  printArguments(arguments);
}

char *parseFilename(char *path)
{
  // allocate empty filename buffer
  char *filename = (char *)malloc(0);

  int i = 0, j = 0;
  for (; i < strlen(path); i++)
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

  return filename;
}

char *getip(char *hostname)
{
  struct hostent *h;

  if ((h = gethostbyname(hostname)) == NULL)
  {
    herror("# gethostbyname");
    exit(1);
  }

  char *ip_addr = inet_ntoa(*((struct in_addr *)h->h_addr));

  return ip_addr;
}

int initConnection(Sockets *sockets, char *ip)
{
  ServerResponse response;

  // open login socket
  sockets->controlSockFd = openSocket(ip, FTP_PORT);

  // wait for server response
  receiveResponse(&response, sockets->controlSockFd);

  // if code is invalid exit
  if (response.code[0] != '2')
  {
    printf("# Error connecting to server!\n");
    exit(0);
  }

  freeResponse(&response);

  return 0;
}

int login(Sockets *sockets, char *user, char *pwd)
{
  ServerResponse response;

  char *msg = (char *)malloc(sizeof(user) + 5);

  // send user
  sprintf(msg, "user %s\n", user);
  sendCmd(sockets, msg);
  printf("# User sent!\n");

  receiveResponse(&response, sockets->controlSockFd);
  if (response.code[0] != '3')
  {
    printf("# Error receiving user response!\n");
    exit(0);
  }

  // send password
  sprintf(msg, "pass %s\n", pwd);
  sendCmd(sockets, msg);
  printf("# Pass sent!\n");

  receiveResponse(&response, sockets->controlSockFd);
  if (response.code[0] != '2')
  {
    printf("# Error receiving pass response!\n");
    exit(0);
  }

  freeResponse(&response);
  free(msg);

  return 0;
}

int changeDir(Sockets *sockets, char *filepath)
{
  ServerResponse response;

  char *msg = (char *)malloc(sizeof(filepath) + 4);

  sprintf(msg, "cwd %s\n", filepath);
  sendCmd(sockets, msg);

  receiveResponse(&response, sockets->controlSockFd);
  if (response.code[0] != '2')
  {
    printf("# Error receiving cd response!\n");
    exit(0);
  }

  freeResponse(&response);

  return 0;
}

int enterPasvMode(Sockets *sockets)
{
  ServerResponse response;

  sendCmd(sockets, "pasv\n");
  printf("# Entering pasv mode!\n");

  receiveResponse(&response, sockets->controlSockFd);
  if (response.code[0] != '2')
  {
    printf("# Error entering passive mode!\n");
    exit(0);
  }

  // get port to connect data socket
  calculatePort(sockets, response.msg);

  freeResponse(&response);

  return 0;
}

int openDataConnection(Sockets *sockets)
{
  // open login socket
  sockets->dataSockFd = openSocket(sockets->serverAddr, sockets->serverPort);

  return 0;
}

void transferFile(Sockets *sockets, char *filename)
{
  ServerResponse response;

  char *msg = (char *)malloc(sizeof(filename) + 5);

  // send retr
  sprintf(msg, "retr %s\n", filename);
  sendCmd(sockets, msg);
  printf("# Retr sent!\n");

  receiveResponse(&response, sockets->controlSockFd);
  if (response.code[0] != '1')
  {
    printf("# Error opening file!\n");
    exit(0);
  }

  freeResponse(&response);

  saveFile(sockets->dataSockFd, filename);

  free(msg);
}

void freeResources(struct URLarguments *arguments, Sockets *sockets)
{
  free(arguments->user);
  free(arguments->pwd);
  free(arguments->hostname);
  free(arguments->filepath);
  free(arguments->filename);

  // close the open sockets
  close(sockets->controlSockFd);
  close(sockets->dataSockFd);
}

int openSocket(char *ip, int port)
{
  // socket file descriptor
  int sockFd;
  // server struct
  struct sockaddr_in server_addr;

  /* server address handling */
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip); /* 32 bit Internet address network byte ordered */
  server_addr.sin_port = htons(port);          /* server TCP port must be network byte ordered */

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

int sendCmd(Sockets *sockets, char *msg)
{
  if (write(sockets->controlSockFd, msg, strlen(msg)) < 0)
  {
    perror("# Error sending control message\n");
    return 1;
  }

  return 0;
}

void receiveResponse(ServerResponse *response, int sockfd)
{
  response->code = (char *)malloc(3 * sizeof(char));
  response->msg = (char *)malloc(0);

  char c;
  int i = 0, j = 0;

  ResponseState state = READ_CODE;

  int multipleLineMsg = 0;

  while (state != READ_FINAL)
  {
    read(sockfd, &c, 1);

    switch (state)
    {
    case READ_CODE:
      // server 3-digit response code
      if (c == ' ')
      {
        if (i == 3)
        {
          state = READ_MSG;
          i = 0;
        }
        else
        {
          printf("# Error receiving server response code!\n");
          exit(0);
        }
      }
      else if (c == '-')
      {
        // multiple line message
        state = READ_MULTIPLE;
        multipleLineMsg = 1;
      }
      else if (isdigit(c))
      {
        response->code[i++] = c;
      }
      break;
    case READ_MSG:
      // reads message
      if (c == '\n')
        state = READ_FINAL;
      else
      {
        response->msg = realloc(response->msg, j + 1);
        response->msg[j++] = c;
      }
      break;
    case READ_MULTIPLE:
      if (c == '\n')
      {
        response->msg = realloc(response->msg, j + 1);
        response->msg[j++] = c;

        state = READ_CODE;
        i = 0;
      }
      else
      {
        response->msg = realloc(response->msg, j + 1);
        response->msg[j++] = c;
      }
      break;
    default:
      fprintf(stderr, "# Invalid response state");
      break;
    }
  }

  printf("# Response code: %s\n", response->code);

  if (multipleLineMsg)
    printf("# Response msg: \n%s\n\n", response->msg);
  else
    printf("# Response msg: %s\n\n", response->msg);
}

void freeResponse(ServerResponse *response)
{
  free(response->code);
  free(response->msg);
}

int calculatePort(Sockets *sockets, char *response)
{
  int ipPart1, ipPart2, ipPart3, ipPart4;
  int port1, port2;

  if ((sscanf(response, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
              &ipPart1, &ipPart2, &ipPart3, &ipPart4, &port1, &port2)) < 0)
  {
    printf("# Error retrieving pasv mode information!\n");
    return 1;
  }

  if ((sprintf(sockets->serverAddr, "%d.%d.%d.%d", ipPart1, ipPart2, ipPart3, ipPart4)) < 0)
  {
    printf("# Error forming server's ip address!\n");
    return 1;
  }

  sockets->serverPort = port1 * 256 + port2;

  printf("# Server address: %s\n", sockets->serverAddr);
  printf("# Server port: %d\n\n", sockets->serverPort);

  return 0;
}

void saveFile(int fd, char *filename)
{
  FILE *file = fopen((char *)filename, "wb");

  char bufSocket[SOCKET_BUF_SIZE];

  int bytes;
  while ((bytes = read(fd, bufSocket, SOCKET_BUF_SIZE)) > 0)
    bytes = fwrite(bufSocket, bytes, 1, file);

  fclose(file);

  printf("# Finished downloading file\n");
}

void printArguments(struct URLarguments *arguments)
{
  printf("\nURLarguments:\n");
  printf("# User: %s\n", arguments->user);
  printf("# Password: %s\n", arguments->pwd);
  printf("# Hostname: %s\n", arguments->hostname);
  printf("# Filepath: %s\n", arguments->filepath);
  printf("# Filename: %s\n", arguments->filename);
  printf("# Host IP: %s\n\n", arguments->hostIp);
}