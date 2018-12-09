#include "download.h"

int main(int argc, char **argv)
{
  struct URLarguments arguments;

  if (argc != 2)
  {
    fprintf(stderr, "[main] usage: ./download ftp://<user>:<password>@<host>/<url-path>\n");
    exit(1);
  }

  // parse url argument
  parseURL(argv[1], &arguments);
  printArguments(arguments);

  // parse filename from the file's path
  char *filename = parseFilename(arguments.path);
  printf("# Filename: %s\n", filename);

  // free the allocated mem
  freeResources(arguments, filename);

  return 0;
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
        fprintf(stderr, "[parseURL]: error parsing ftp://\n");
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
      fprintf(stderr, "[parseURL]: invalid URL state\n");
      exit(1);
    }
  }
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

  return filename;
}

struct hostent *getip(char *hostname)
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

      #define h_addr h_addr_list[0]	The first address in h_addr_list. 
    */

  if ((h = gethostbyname(hostname)) == NULL)
  {
    herror("gethostbyname");
    exit(1);
  }

  printf("Host name  : %s\n", h->h_name);
  printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr_list[0])));

  return h;
}

void printArguments(struct URLarguments arguments)
{
  printf("\nURLarguments:\n");
  printf("# User: %s\n", arguments.user);
  printf("# Password: %s\n", arguments.pwd);
  printf("# Host: %s\n", arguments.host);
  printf("# Path: %s\n\n", arguments.path);
}

void freeResources(struct URLarguments arguments, char *filename)
{
  free(arguments.user);
  free(arguments.pwd);
  free(arguments.host);
  free(arguments.path);
  
  free(filename);
}

void codigoTCP()
{
  int sockfd;
  struct sockaddr_in server_addr;
  char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
  int bytes;

  /*server address handling*/
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR); /*32 bit Internet address network byte ordered*/
  server_addr.sin_port = htons(SERVER_PORT);            /*server TCP port must be network byte ordered */

  /*open an TCP socket*/
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("socket()");
    exit(0);
  }

  /*connect to the server*/
  if (connect(sockfd,
              (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0)
  {
    perror("connect()");
    exit(0);
  }

  /*send a string to the server*/
  bytes = write(sockfd, buf, strlen(buf));
  printf("Bytes escritos %d\n", bytes);

  close(sockfd);
  exit(0);
}