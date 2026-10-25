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
    cfg->stream_port = 18083;
    cfg->web_port = 8888;

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
        else if (strcmp(key, "stream_port") == 0)
            cfg->stream_port = atoi(val);
        else if (strcmp(key, "web_port") == 0)
            cfg->web_port = atoi(val);
        else if (strcmp(key, "web_path") == 0)
            snprintf(cfg->web_path, sizeof(cfg->web_path), "%s", val[0] == '/' ? val + 1 : val);
        else if (strcmp(key, "page") == 0)
            cfg->page = atoi(val);
    }
    fclose(f);
    if (cfg->port <= 0 || cfg->port > 65535) cfg->port = 18082;
    if (cfg->stream_port <= 0 || cfg->stream_port > 65535) cfg->stream_port = 18083;
    if (cfg->web_port <= 0 || cfg->web_port > 65535) cfg->web_port = 8888;
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
    fprintf(f, "stream_port=%d\n", cfg->stream_port);
    fprintf(f, "web_port=%d\n", cfg->web_port);
    fprintf(f, "web_path=%s\n", cfg->web_path);
    fprintf(f, "page=%d\n", cfg->page);
    fclose(f);
}
