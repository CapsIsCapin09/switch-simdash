#pragma once

typedef struct {
    char host[128];          // PC running SimHub
    int port;                // SimHub Property Server port
    char moonlight_path[256];  // optional override for the Moonlight .nro
    int page;                // last custom dash page shown
} Config;

void config_load(Config *cfg);
void config_save(const Config *cfg);
