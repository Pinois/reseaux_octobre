#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netdb.h>
#include "common/protocol.h"

static char file[256] = "";
static char hostname[256] = "";
static char port[8] = "";

int main (int argc, char **argv)
{
  int c;
  while (1) {
    static struct option long_options[] =
        {
          {"file",  required_argument, 0, 'f'},
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
        default:
          abort();
       }
    }
  }
  if (optind < argc)
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
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;

  s = getaddrinfo(NULL, port, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(EXIT_FAILURE);
  }

  for (rp = result; rp != NULL; rp = rp->ai_next)
  {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;
    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    close(sfd);
  }

  if (rp == NULL)
  {
    fprintf(stderr, "Could not bind\n");
    return EXIT_FAILURE;
  }

  freeaddrinfo(result);

  char seq = 0;
  int nread;
  char receiving[FRAME_SIZE];
  char sending[FRAME_SIZE];
  window cache[MAX_WINDOW_SIZE];
  window to_write[MAX_WINDOW_SIZE];
  FILE * fd;

  fd = (strcmp(file, "") == 0)?stdout:fopen(file,"w");
  init_window(cache);

  for(;;)
  {
    frame data, ack;
    nread = recvfrom(sfd, receiving, sizeof(receiving), 9, (struct sockaddr*) &peer_addr, &peer_addr_len);
    
    unserialize(receiving, &data);
    if (valid_frame(data))
    {
      add_frame_to_window(data, cache);

      create_ack_frame(data.seq, &ack); 
      serialize(ack, sending);
      if (sendto(sfd, sending, nread, 0, (struct sockaddr *) &peer_addr, peer_addr_len) != nread)
      {
        fprintf(stderr, "Error sending response\n");
      }

      size_t len = 0; 
      init_window(to_write);
      clean_window(seq, cache, to_write,  &len, RECV);
      int i;
      for (i=0; i<len ; i++)
      {
        frame temp = to_write[i].frame; 
        if (fwrite(to_write[i].frame.payload, sizeof(char), temp.length, fd) != temp.length)
        {
          printf("Writing to file failed !\n");
          goto end;
        }
        if (temp.length < MAX_PAYLOAD_SIZE)
          goto end;
      }
      seq += len;
    }
  }

end:
  close(sfd);
  fclose(fd);

  return EXIT_SUCCESS;
}
