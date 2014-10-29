#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
//#include <fcntl.h>
#include "common/protocol.h"

static char file[256] = "";
static char hostname[256] = "";
static char port[8];
static int sber = 0;
static int splr = 0;
static int delay = 0;
static int mili_delay = 0;

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
            mili_delay = delay * 1000;
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
  
  /*INITIALISATION*/
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
  window resend[MAX_WINDOW_SIZE];
  FILE* fd;
  int len = 0;
  int resend_len = 0;
  ssize_t nread = 0;
  uint8_t seq = 0;

  srand(time(NULL));
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

  //fcntl(sfd, F_SETFL, O_NONBLOCK); 

  freeaddrinfo(result);

  /*THREE WAY HANDSHAKE*/ 
  create_data_frame(seq, "", len, &data);
  printf("type: %d, window: %d, seq: %d, length: %d, crc: %d\n", data.type, data.window, data.seq, data.length, data.crc);
  serialize(data, sending);
  if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
  {
     printf("writing to socket failed !\n");
    return EXIT_FAILURE;
  }

  nread = recv(sfd, receiving, FRAME_SIZE, 0);
  if (nread == -1)
  {
    printf("Reading from socket failed !\n");
    return EXIT_FAILURE;
  }
  unserialize(receiving, &ack);
//  printf("ACK ! type: %d, window: %d, seq: %d, length: %d, crc: %d\n", ack.type, ack.window, ack.seq, ack.length, ack.crc);

  create_data_frame(ack.seq, "", len, &data);
//  printf("type: %d, window: %d, seq: %d, length: %d, crc: %d\n", data.type, data.window, data.seq, data.length, data.crc);
  serialize(data, sending);
  if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
  {
      printf("writing to socket failed !\n");
      return EXIT_FAILURE;
  }
 
  seq = data.seq + 1;

/*START TRANSMISSION*/
  while((len = fread(buffer, sizeof(char),MAX_PAYLOAD_SIZE, fd)) != 0 )
  {
    bzero(&receiving, sizeof(receiving));
    create_data_frame(seq, buffer, len, &data);
    if (is_free_window(wdw, MAX_WINDOW_SIZE - 1))
    {
      printf("type: %d, window: %d, seq: %d, length: %d, crc: %d\n", data.type, data.window, data.seq, data.length, data.crc);
      if (rand()%1000 < sber)
        data.payload[0] ^= 0xFF; 
      if (delay)
        usleep(mili_delay);
      if ((rand()%100) >= splr)
      {
        serialize(data, sending);
        if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
        {
          printf("writing to socket failed !\n");
          return EXIT_FAILURE;
        }
      }
      if (!frame_in_window(wdw, data))
        add_frame_to_window(data, wdw);
    }

    nread = recv(sfd, receiving, FRAME_SIZE, 0);
    if (nread == -1)
    {
      printf("Reading from socket failed !\n");
      return EXIT_FAILURE;
    }
 
     unserialize(receiving, &ack);
//    printf("ACK ! type: %d, window: %d, seq: %d, length: %d, crc: %d\n", ack.type, ack.window, ack.seq, ack.length, ack.crc);
    if(valid_frame(ack))
    {
      init_window(removed);
      int rem_len = 0;
      clean_window(ack.seq - 1, wdw, removed, &rem_len, SEND);
      printf("clean window !,seq: %d, rem_len: %d\n",ack.seq, rem_len);
    }

    if (timer_reached(wdw, resend, &resend_len))
    {
      int i;
      for (i=0; i<resend_len; i++)
      {
        if (rand()%1000 < sber)
          resend[i].frame.payload[0] ^= 0xFF; 
        serialize(resend[i].frame, sending);
        if (delay)
          usleep(mili_delay);
        if ((rand()%100)+1 > splr)
        {
          if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
          {
            printf("writing to socket failed !\n");
            return EXIT_FAILURE;
          }
        }
      }
    }
 

printf("seq: %d\n", seq);
    seq++;
  }

  close(sfd);
  fclose(fd);
  
  return EXIT_SUCCESS;
}
