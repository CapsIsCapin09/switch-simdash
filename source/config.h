#pragma once

typedef struct {
    char host[128];          // PC running SimHub
    int port;                // SimHub Property Server port
    int stream_port;         // SimDash Streamer port
    int web_port;            // SimHub's web dash server port
    char web_path[128];      // page to open on it, e.g. a specific dash
    int page;                // last custom dash page shown
} Config;

void config_load(Config *cfg);
void config_save(const Config *cfg);
