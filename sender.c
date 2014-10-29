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
  char seq = 0;

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
  serialize(data, sending);
  if (delay)
    usleep(mili_delay);
  if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
  {
   // if (!(errno == EWOULDBLOCK || errno == EAGAIN))
   // {
      printf("writing to socket failed !\n");
      return EXIT_FAILURE;
   // }
  }

  nread = read(sfd, receiving, FRAME_SIZE);
 // if (!(errno == EWOULDBLOCK || errno == EAGAIN))
//  {
    if (nread == -1)
    {
      printf("Reading from socket failed !\n");
      return EXIT_FAILURE;
    }
//  }
  unserialize(sending, &ack);

  create_data_frame(ack.seq, "", len, &data);
  serialize(data, sending);
  if (delay)
    usleep(mili_delay);
  if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
  {
    printf("writing to socket failed !\n");
    return EXIT_FAILURE;
  }
 
  /*START TRANSMISSION*/
  fd_set rfds;
  struct timeval tv;
  int retval;

  FD_ZERO(&rfds);
  FD_SET(sfd, &rfds);
  
  tv.tv_sec = 0;
  tv.tv_usec = 0;
int h = 0;
  while((len = fread(buffer, sizeof(char),MAX_PAYLOAD_SIZE, fd)) != 0 )
  { 
        printf("INIT, %d\n",h);
    create_data_frame(seq, buffer, len, &data);
    if (is_free_window(wdw, MAX_WINDOW_SIZE))
    {
      if (rand()%1000 < sber)
        data.payload[0] ^= 0xFF; 
      serialize(data, sending);
      if (delay)
        usleep(mili_delay);
      if ((rand()%100) + 1 > splr)
      {
        printf("WRITE, %d\n",h);
        if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
        {
          printf("writing to socket failed !\n");
          return EXIT_FAILURE;
        }
      }
      if (!frame_in_window(wdw, data))
        add_frame_to_window(data, wdw);
    }

    int ret = select(sfd + 1, &rfds, NULL, NULL, &tv);
    if (ret == 0)
    {
      printf("ACK, %d\n",h);
      nread = read(sfd, receiving, FRAME_SIZE);
      printf("ACK2, %d\n",h);
//      if (!(errno == EWOULDBLOCK || errno == EAGAIN))
//      {
        if (nread == -1)
        {
          printf("Reading from socket failed !\n");
          return EXIT_FAILURE;
        }
//      }

      unserialize(sending, &ack);
      if(valid_frame(ack))
      {
        printf("ACK3, %d\n",h);
        init_window(removed);
        size_t rem_len = 0;
        clean_window(ack.seq - 1, wdw, removed, &rem_len, SEND);
      }
    }

        printf("BEFORE TIMER, %d\n",h);
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
          printf("RETRANSMISSION, %d\n",h);
          if (write(sfd, sending, sizeof(sending)) != sizeof(sending)) 
          {
            printf("writing to socket failed !\n");
            return EXIT_FAILURE;
          }
        }
      }
    }
 
    seq++;
    h++;
  }


  close(sfd);
  fclose(fd);
  
  return EXIT_SUCCESS;
}
