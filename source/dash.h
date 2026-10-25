#pragma once
#include "net.h"

#define DASH_PAGE_COUNT 2

typedef struct {
    NetState net;
    const char *host;
    int port;
    double time;  // seconds since start, for blinking
    double dt;    // seconds since last frame
} DashContext;

void dash_draw(int page, const PropValue *v, const DashContext *ctx);

typedef enum { MENU_CUSTOM, MENU_MOONLIGHT, MENU_SET_PC, MENU_EXIT, MENU_COUNT } MenuItem;
void menu_draw(int selected, const char *host, int port, const char *message);
