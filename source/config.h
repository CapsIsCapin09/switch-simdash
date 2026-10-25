#pragma once

typedef struct {
    char host[128];          // PC running SimHub
    int port;                // SimHub Property Server port
    int stream_port;         // SimDash Streamer port
    int page;                // last custom dash page shown
} Config;

void config_load(Config *cfg);
void config_save(const Config *cfg);
