#pragma once
#include "net.h"

#define DASH_PAGE_COUNT 2

typedef struct {
    NetState net;
    const char *host;
    int port;
    double time;  // seconds since start, for blinking
    double dt;    // seconds since last frame
    const char *error;      // why connecting to SimHub failed
    const char *switch_ip;  // this Switch's own address
} DashContext;

void dash_draw(int page, const PropValue *v, const DashContext *ctx);

typedef enum { MENU_CUSTOM, MENU_STREAM, MENU_SET_PC, MENU_EXIT, MENU_COUNT } MenuItem;
void menu_draw(int selected, const char *host, int port, const char *switch_ip);
