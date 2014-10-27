#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <zlib.h>
#include "protocol.h"

static int is_empty_window(window window);
static int is_empty_frame(frame frame);
static int is_empty_timer(timer timer);
static char next_seq(char seq);
static void init_payload(char* payload);
static void init_frame(frame* frame);
static void init_timer(timer * timer);
static uLong compute_crc(frame  frame);

int is_free_window(window* wdw, size_t len)
{
  return is_empty_window(*(wdw + (len-1)));
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
  for (i = 0; i < MAX_WINDOW_SIZE/2; i++)
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
  int i = MAX_WINDOW_SIZE/2;
  if (!is_empty_window(*(wdw + (i - 1)))) return 0;
  while (i > 0 && is_empty_window(*(wdw + (i - 1))))
  {
    i--;
  }
  memcpy(&wdw[i].frame, &frame, sizeof (frame));

  return 1;
}

void clean_window(char seq, window to_clean[], window removed[], size_t* len)
{
  
  int i = 0;
  while (i < MAX_WINDOW_SIZE/2 && !is_empty_window(*(to_clean + i)))
  {
    if (seq == to_clean[i].frame.seq)
    {
      memcpy(&removed[*len], &to_clean[i], sizeof(window));
      (*len)++;
      clean_window(seq+1, to_clean, removed, len);
      init_frame(&to_clean[i].frame);
      init_timer(&to_clean[i].timer);
    }
    i++;
  }

  window temp[MAX_WINDOW_SIZE];
  init_window(temp);
 
  int j; 
  for (i=0, j=0; i < MAX_WINDOW_SIZE/2; i++)
  {
    if (!is_empty_window(to_clean[i]))
    {
      temp[j] = to_clean[i];
      j++;
    }
  }

  init_window(to_clean);
  for (i=0; i < MAX_WINDOW_SIZE/2; i++)
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
    if (frame.length != MAX_PAYLOAD_SIZE)
    {
      if (frame.length != strlen(frame.payload))
      {
        return 0;
      }
    }
    if (frame.window != 0)
    {
      return 0;
    }
  } else if (frame.type == 2)
  {
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
  {
  }

  if (frame.crc != compute_crc(frame))
  {
    return 0;
  }

  return 1;
}

void create_data_frame(char seq, char* data, frame* frame)
{
  frame->type = 1;
  frame->window = 0;
  frame->seq = next_seq(seq);
  frame->length = strlen(data);

  init_payload(frame->payload);
  memcpy(frame->payload, data, frame->length + 1);

  frame->crc = compute_crc(*frame);
}

int create_ack_frame(char seq, uint16_t window , frame* frame)
{
  if (window >= MAX_WINDOW_SIZE / 2 )
  {
    return 0;
  } 
  frame->type = 2;
  frame->window = window;
  frame->seq = next_seq(seq);
  frame->length = 0;
  init_payload(frame->payload);
  frame->crc = compute_crc(*frame);

  return 1;
}

char next_seq(char seq)
{
  return seq % (MAX_WINDOW_SIZE/2);
}

void init_payload(char * payload)
{
  memset(payload, '\0', MAX_PAYLOAD_SIZE);
}

uLong compute_crc(frame frame)
{
  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (Bytef*) &frame, sizeof(frame) - sizeof(crc));

  return crc;
}
