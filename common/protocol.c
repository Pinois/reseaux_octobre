#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <zlib.h>
#include <sys/time.h>
#include "protocol.h"

static int is_empty_window(window window);
static int is_empty_frame(frame frame);
static int is_empty_timer(timer timer);
static char get_seq(uint8_t seq);
static void init_payload(char* payload);
static void init_frame(frame* frame);
static void init_timer(timer * timer);
static uLong compute_crc(frame  frame);

int frame_in_window(window* wdw, frame frm) 
{
  int i = 0;
  while (i < MAX_WINDOW_SIZE)
  {
    if (memcmp(&wdw[i].frame, &frm, sizeof(frame)) == 0)
      return 1;
    i++;
  }

  return 0;
}

int timer_reached(window* wdw, window* resend, int* len)
{
  int i;
  int j = 0;
  for (i=0; i<MAX_WINDOW_SIZE; i++)
  {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    uint64_t current_time = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;

    if ((wdw[i].timer.timerid != 0) && (current_time - wdw[i].timer.timerid > MICROSEC_TIMEOUT))
    {
      wdw[i].timer.counter++;
      memcpy(&resend[j],&wdw[i], sizeof(window));
      j++;
    }
  }
  *len = j;

  return *len > 0;
}

int is_free_window(window* wdw, size_t ind)
{
  return is_empty_window(*(wdw + (ind)));
}

int is_empty_window(window window)
{
  return is_empty_frame(window.frame) && is_empty_timer(window.timer);
}

int is_empty_frame(frame frame)
{
  return frame.type == 0 &&
         frame.window == 0 &&
         frame.seq == 0 &&
         (strcmp(frame.payload, "") == 0) &&
         frame.crc == 0;
}

int is_empty_timer(timer timer)
{
  return timer.timerid == 0 &&
         timer.counter == 0;
}

void init_window(window * window)
{
  int i;
  for (i = 0; i < MAX_WINDOW_SIZE; i++)
  {
    init_frame(&window[i].frame);
    init_timer(&window[i].timer);
  }
}

void init_frame(frame * frame) 
{
  frame->type = 0;
  frame->window = 0;
  frame->seq = 0;
  init_payload(frame->payload);
  frame->crc = 0;
}

void init_timer(timer * timer) 
{
  timer->timerid = 0;
  timer->counter = 0;
}

int add_frame_to_window(frame frame, window* wdw)
{
  int i = 0;
  if (!is_empty_window(*(wdw + (i - 1)))) return 0;
  while (i < MAX_WINDOW_SIZE && !is_empty_window(*(wdw +i)))
  {
    i++;
  }
  memcpy(&wdw[i].frame, &frame, sizeof (frame));

  struct timeval tv;
  gettimeofday(&tv,NULL);
  wdw[i].timer.timerid = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;

  return 1;
}

void clean_window(uint8_t seq, window* to_clean, window* removed, int* len, int flags)
{
  seq = get_seq(seq);
  int i = 0;
  int j = 0;
  while (i < MAX_WINDOW_SIZE)
  {
    if (flags == RECV)
    {
      if (!is_empty_frame(to_clean[i].frame) && seq == to_clean[i].frame.seq)
      {
        memcpy(&removed[*len] , &to_clean[i], sizeof(window)+1);
	init_frame(&to_clean[i].frame);
        init_timer(&to_clean[i].timer);
        (*len)++;
        clean_window(get_seq(seq+1), to_clean, removed, len, RECV);
      }
    }else {
      if (!is_empty_frame(to_clean[i].frame) && seq == to_clean[i].frame.seq)
      {
        memcpy(&removed[*len], &to_clean[i], sizeof(window));
        init_frame(&to_clean[i].frame);
        init_timer(&to_clean[i].timer);
        (*len)++;
        clean_window(get_seq(seq-1), to_clean, removed, len, SEND);
       }
    }
    i++;
  }

  window temp[MAX_WINDOW_SIZE];
  init_window(temp);
 
  for (i=0, j=0; i < MAX_WINDOW_SIZE; i++)
  {
    if (!is_empty_window(to_clean[i]))
    {
      temp[j] = to_clean[i];
      j++;
    }
  }

  init_window(to_clean);
  for (i=0; i < MAX_WINDOW_SIZE; i++)
  {
    to_clean[i] = temp[i];
  }
}

void serialize(frame frame, char* data)
{
  char type = frame.type; 
  type <<= 5;
  char window = frame.window;
  char temp = type | window;

  memcpy(data, &temp , sizeof(char));
  memcpy(data +1, &frame.seq, sizeof(char));
  memcpy(data + 2 , &frame.length, sizeof(frame.length));

  memcpy(data + 4, frame.payload, sizeof(frame.payload)+1); 

  memcpy(data+ 516, &frame.crc, sizeof(frame.crc));
}

void unserialize(char * data, frame *frame)
{
  char temp;
  memcpy(&temp, data, sizeof(char));
  frame->window = temp & 0x1F;
  
  temp >>= 5;
  frame->type = temp & 7;

  memcpy(&frame->seq, &data[1], sizeof(frame->seq));
  memcpy(&frame->length, &data[2], sizeof(frame->length));
  memcpy(frame->payload, &data[4], sizeof(frame->payload));
  memcpy(&frame->crc, &data[516], sizeof(frame->crc));
}

int valid_frame(frame frame)
{
  if (frame.crc != compute_crc(frame))
  {
    return 0;
  }
  
  if (frame.type ==  1)
  {
    if (frame.length > MAX_PAYLOAD_SIZE)
    {
      return 0;
    }
    if (frame.window != 0)
    {
      return 0;
    }
  } else if (frame.type == 2) {
    if (frame.length != 0)
    {
      return 0;
    }
    if (strlen(frame.payload) != 0)
    {
      return 0;
    }
  } else
    return 0;

  return 1;
}

void create_data_frame(uint8_t seq, char* data, uint16_t len, frame* frame)
{
  frame->type = 1;
  frame->window = 0;
  frame->seq = get_seq(seq);
  frame->length = len;

  init_payload(frame->payload);
  memcpy(frame->payload, data, frame->length + 1);

  frame->crc = compute_crc(*frame);
}

void create_ack_frame(uint8_t seq, frame* frame)
{
  frame->type = 2;
  frame->window = MAX_WINDOW_SIZE - 1;
  frame->seq = get_seq(seq);
  frame->length = 0;
  init_payload(frame->payload);
  frame->crc = compute_crc(*frame);
}

char get_seq(uint8_t seq)
{
  return seq % (MAX_WINDOW_SIZE * 2);
}

void init_payload(char * payload)
{
  memset(payload, '\0', MAX_PAYLOAD_SIZE);
}

uLong compute_crc(frame frame)
{
  uint32_t crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (Bytef*) &frame, FRAME_SIZE - sizeof(frame.crc));

  return crc;
}
