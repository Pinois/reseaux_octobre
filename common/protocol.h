#ifndef protocol_h 
#define protocol_h

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "../zlib/zlib.h" 

#define MAX_WINDOW_SIZE 32 
#define MAX_PAYLOAD_SIZE 512
#define FRAME_SIZE 520
#define RECV 0
#define SEND 1

typedef struct
{
    char type:3;
    char window:5;
    char seq;
    uint16_t length;
    char payload [MAX_PAYLOAD_SIZE];
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

extern void create_data_frame(char seq, char* data, uint16_t len, frame* frame);

extern void create_ack_frame(char seq, frame* frame);

extern int valid_frame(frame frame);

extern void serialize(frame, char*);

extern void unserialize(char*, frame*);

extern int is_free_window(window* window, size_t len);

extern int add_frame_to_window(frame frame, window* window);

extern void clean_window(char seq, window* to_clean, window* removed , size_t* len, int flags);


extern void init_window(window * window);

#endif
