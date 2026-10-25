#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __SWITCH__
#define CONFIG_DIR "sdmc:/config/simdash"
#else
#define CONFIG_DIR "."
#endif
#define CONFIG_PATH CONFIG_DIR "/config.ini"

static void trim(char *s) {
    char *end = s + strlen(s);
    while (end > s && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
}

void config_load(Config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->host, sizeof(cfg->host), "192.168.1.100");
    cfg->port = 18082;

    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return;
    char line[320];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = 0;
        const char *key = line, *val = eq + 1;
        if (strcmp(key, "host") == 0)
            snprintf(cfg->host, sizeof(cfg->host), "%s", val);
        else if (strcmp(key, "port") == 0)
            cfg->port = atoi(val);
        else if (strcmp(key, "moonlight_path") == 0)
            snprintf(cfg->moonlight_path, sizeof(cfg->moonlight_path), "%s", val);
        else if (strcmp(key, "page") == 0)
            cfg->page = atoi(val);
    }
    fclose(f);
    if (cfg->port <= 0 || cfg->port > 65535) cfg->port = 18082;
}

void config_save(const Config *cfg) {
#ifdef __SWITCH__
    mkdir("sdmc:/config", 0777);
    mkdir(CONFIG_DIR, 0777);
#endif
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) return;
    fprintf(f, "# SimDash settings\n");
    fprintf(f, "host=%s\n", cfg->host);
    fprintf(f, "port=%d\n", cfg->port);
    fprintf(f, "moonlight_path=%s\n", cfg->moonlight_path);
    fprintf(f, "page=%d\n", cfg->page);
    fclose(f);
}
