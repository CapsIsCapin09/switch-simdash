#pragma once
#include <stdbool.h>

#include "net.h"

// Receives the JPEG video feed sent by SimDash Streamer on the PC and
// decodes it in a background thread.
void stream_start(const char *host, int port);
void stream_stop(void);

typedef struct {
    NetState state;
    unsigned seq;         // increments with every decoded frame
    int width, height;
    float fps;            // frames decoded per second, recent average
} StreamInfo;

// Copies the newest frame (RGBA, width*height*4 bytes) into dst if it is
// newer than last_seq and fits in dst_size. Always fills info.
bool stream_get_frame(unsigned last_seq, void *dst, int dst_size, StreamInfo *info);
