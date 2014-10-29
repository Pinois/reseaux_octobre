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

  /*INITIALISATION*/
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len;
  uint8_t seq = 1;
  int nread;
  frame data, ack;
  char receiving[FRAME_SIZE];
  char sending[FRAME_SIZE];
  window cache[MAX_WINDOW_SIZE];
  window to_write[MAX_WINDOW_SIZE];
  FILE * fd;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;

  s = getaddrinfo(NULL, port, &hints, &result);
  if (s != 0) {
    printf("Error while getting network address\n");
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
    printf("Could not bind\n");
    return EXIT_FAILURE;
  }

  freeaddrinfo(result);

  fd = (strcmp(file, "") == 0)?stdout:fopen(file,"w");

  init_window(cache);

  /*START THREE WAY HANDSHAKE*/
  nread = recvfrom(sfd, receiving, sizeof(receiving), 9, (struct sockaddr*) &peer_addr, &peer_addr_len);
  if (nread == -1)
    printf("FAILED Reading ! \n");
  unserialize(receiving, &data);
//  printf("type: %d, window: %d, seq: %d, length: %d, crc: %d\n", data.type, data.window, data.seq, data.length, data.crc);

  create_ack_frame(data.seq + 1, &ack); 
  serialize(ack, sending);
//  printf("ACK ! type: %d, window: %d, seq: %d, length: %d, crc: %d\n", ack.type, ack.window, ack.seq, ack.length, ack.crc);
  if (sendto(sfd, sending, nread, 0, (struct sockaddr *) &peer_addr, peer_addr_len) != nread)
    printf("Error sending response\n");

  nread = recvfrom(sfd, receiving, sizeof(receiving), 9, (struct sockaddr*) &peer_addr, &peer_addr_len);
  if (nread == -1)
    printf("FAILED Reading ! \n");
  unserialize(receiving, &data);
//  printf("type: %d, window: %d, seq: %d, length: %d, crc: %d\n", data.type, data.window, data.seq, data.length, data.crc);

  seq = data.seq + 2;  

  /*START TRANSMISSION*/
  for(;;)
  {
    bzero(&receiving, sizeof(receiving));
    nread = recvfrom(sfd, receiving, sizeof(receiving), 9, (struct sockaddr*) &peer_addr, &peer_addr_len);
    if (nread == -1)
      printf("FAILED Reading ! \n");
    unserialize(receiving, &data);
//    printf("type: %d, window: %d, seq: %d, length: %d, crc: %d\n", data.type, data.window, data.seq, data.length, data.crc);
    if (valid_frame(data))
    {
      if (!frame_in_window(cache, data))
        add_frame_to_window(data, cache);

      create_ack_frame(seq, &ack); 
//      printf("ACK ! type: %d, window: %d, seq: %d, length: %d, crc: %d\n", ack.type, ack.window, ack.seq, ack.length, ack.crc);
      serialize(ack, sending);
      if (sendto(sfd, sending, nread, 0, (struct sockaddr *) &peer_addr, peer_addr_len) != nread)
      {
        printf("Error sending response\n");
      }

      int len = 0; 
      int i;

      init_window(to_write);
      clean_window(seq - 1, cache, to_write,  &len, RECV);
//      printf("seq: %d, len: %d\n", seq - 1, len);
      for (i=0; i<len ; i++)
      {
        frame temp = to_write[i].frame; 
//        printf("WRITING ! type: %d, window: %d, seq: %d, length: %d, crc: %d\n", temp.type, temp.window, temp.seq, temp.length, temp.crc);
        if (fwrite(temp.payload, sizeof(char), temp.length, fd) != temp.length)
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
