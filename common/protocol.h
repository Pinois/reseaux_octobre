#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stddef.h>
#include <time.h>
#include "../zlib/zlib.h" 

#define MAX_WINDOW_SIZE 31

typedef struct
{
    unsigned int type:3;
    unsigned int window:5;
    unsigned int seq:8;
    unsigned int length;
    char payload [512];
    ulong crc;
}frame;

typedef struct
{
    timer_t timerid;
    int counter;
}timer;

typedef struct
{
    frame frame;
    timer timer;
}window;

int is_free_window(window window[]);

int add_frame_to_window(frame frame, window window[]);

void clean_window(frame frame, window to_clean[], window removed[], size_t* len);

#endif
