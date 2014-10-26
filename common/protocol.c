#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <zlib.h>
#include "protocol.h"

static char next_seq(char seq);
static void init_payload(char* payload);
static uLong compute_crc(frame  frame);

int is_free_window(window* window, size_t len)
{
  return 1;
}

int add_frame_to_window(frame frame, window window[])
{
  return 1;
}

void clean_window(frame frame, window to_clean[], window removed[], size_t* len)
{
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
  memset(payload, 0, MAX_PAYLOAD_SIZE);
}

uLong compute_crc(frame frame)
{
  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (Bytef*) &frame, sizeof(frame) - sizeof(crc));

  return crc;
}
