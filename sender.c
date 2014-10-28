#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "common/protocol.h"

static char file[256] = "";
static char hostname[256] = "";
static char port[8];
static int sber = 0;
static int splr = 0;
static int delay = 0;

int main (int argc, char **argv)
{
  int c;
  while (1) {
    static struct option long_options[] =
        {
          {"file",  required_argument, 0, 'f'},
          {"sber",  required_argument, 0, 's'},
          {"splr",  required_argument, 0, 'p'},
          {"delay", required_argument, 0, 'd'},
          {0, 0, 0, 0}
        };
    int option_index = 0;

    if ((c = getopt_long (argc, argv, "f:s:p:d:", long_options, &option_index)) )
    {
      if (c == -1)
        break;
      switch (c) 
      {
        case 'f':
          strcpy(file, optarg);
          break;
        case 'd':
            delay = atoi(optarg);
          break;
        case 'p':
          splr = atoi(optarg);
          break;
        case 's':
          sber = atoi(optarg);
	  break;
        default:
          abort();
       }
    }
  }
  if (2 == (argc - optind))
  {
    while (optind < argc)
    {
      strcpy(hostname, argv[optind]);
      optind++;
      if (optind < argc)
      {
        strcpy(port, argv[optind]);
        optind++;
      } else {
        printf("port number needed !\n");
        return EXIT_FAILURE;
      }
    }
  } else {
        printf("hostname and port number needed !\n");
        return EXIT_FAILURE;
  }
  
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;
  struct hostent *host;
  frame data, ack;
  char buffer[MAX_PAYLOAD_SIZE];
  char sending[FRAME_SIZE];
  char receiving[FRAME_SIZE];
  window wdw[MAX_WINDOW_SIZE];
  window removed[MAX_WINDOW_SIZE];
  FILE* fd;
  int len;
  ssize_t nread;
  char seq = 0;

  fd = (strcmp(file, "") == 0)?stdin:fopen(file,"r");
  init_window(wdw);

  bzero(&hints, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;

  s = getaddrinfo(hostname, port, &hints, &result);

  for (rp = result; rp !=NULL; rp = rp->ai_next)
  {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;
    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
      break;
    close(sfd);
  }

  if (rp == NULL)
  {
    printf("Could not connect.\n");
    return EXIT_FAILURE;
  }

  freeaddrinfo(result);

  while((len = fread(buffer, sizeof(char),MAX_PAYLOAD_SIZE, fd)) != 0 )
  {
    // check timer and resend !
    create_data_frame(seq, buffer, len, &data);
    if (is_free_window(wdw, MAX_WINDOW_SIZE))
    {
      serialize(data, sending);
      if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
      {
        printf("writing to socket failed !\n");
        return EXIT_FAILURE;
      }
    }

    nread = read(sfd, receiving, FRAME_SIZE);
    if (nread == -1)
    {
      printf("Reading from socket failed !\n");
      return EXIT_FAILURE;
    }
 
    seq++;
  }

  unserialize(sending, &ack);
  if(valid_frame(ack))
  {
    init_window(removed);
    size_t rem_len = 0;
    clean_window(ack.seq - 1, wdw, removed, &rem_len, SEND);
  }

  close(sfd);
  fclose(fd);
  
  return EXIT_SUCCESS;
}
