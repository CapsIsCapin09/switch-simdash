// SimDash: a native SimHub dashboard for Nintendo Switch homebrew.
// Builds for the Switch with devkitA64/libnx, and for a desktop with SDL2
// so the dash can be tried without a console.

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "config.h"
#include "dash.h"
#include "net.h"
#include "render.h"

enum {
    BTN_UP = 1 << 0,
    BTN_DOWN = 1 << 1,
    BTN_LEFT = 1 << 2,
    BTN_RIGHT = 1 << 3,
    BTN_A = 1 << 4,
    BTN_B = 1 << 5,
    BTN_Y = 1 << 6,
    BTN_PLUS = 1 << 7,
    BTN_QUIT = 1 << 8,  // window closed / home menu exit
};

static Config cfg;

#ifdef __SWITCH__
static PadState pad;
static PlFontData shared_font;

static bool platform_init(void) {
    socketInitializeDefault();
    plInitialize(PlServiceType_User);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    // Keep the screen awake while the dash is up. Either call may be refused
    // depending on how the app was launched, which is fine.
    appletSetAutoSleepDisabled(true);
    appletSetMediaPlaybackState(true);
    return true;
}

static void platform_exit(void) {
    appletSetMediaPlaybackState(false);
    appletSetAutoSleepDisabled(false);
    plExit();
    socketExit();
}

static bool load_font(const void **data, int *size) {
    if (R_FAILED(plGetSharedFontByType(&shared_font, PlSharedFontType_Standard))) return false;
    *data = shared_font.address;
    *size = (int)shared_font.size;
    return true;
}

static unsigned platform_buttons(void) {
    padUpdate(&pad);
    u64 down = padGetButtonsDown(&pad);
    unsigned b = 0;
    if (down & (HidNpadButton_Up | HidNpadButton_StickLUp)) b |= BTN_UP;
    if (down & (HidNpadButton_Down | HidNpadButton_StickLDown)) b |= BTN_DOWN;
    if (down & (HidNpadButton_Left | HidNpadButton_StickLLeft | HidNpadButton_L | HidNpadButton_ZL)) b |= BTN_LEFT;
    if (down & (HidNpadButton_Right | HidNpadButton_StickLRight | HidNpadButton_R | HidNpadButton_ZR)) b |= BTN_RIGHT;
    if (down & HidNpadButton_A) b |= BTN_A;
    if (down & HidNpadButton_B) b |= BTN_B;
    if (down & HidNpadButton_Y) b |= BTN_Y;
    if (down & HidNpadButton_Plus) b |= BTN_PLUS;
    if (!appletMainLoop()) b |= BTN_QUIT;
    return b;
}

// Asks for the PC address with the system keyboard. Returns true if changed.
static bool edit_host(void) {
    SwkbdConfig kbd;
    char out[128] = {0};
    if (R_FAILED(swkbdCreate(&kbd, 0))) return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    swkbdConfigSetLeftOptionalSymbolKey(&kbd, ".");
    swkbdConfigSetRightOptionalSymbolKey(&kbd, ":");
    swkbdConfigSetGuideText(&kbd, "SimHub PC address, e.g. 192.168.1.50");
    swkbdConfigSetInitialText(&kbd, cfg.host);
    swkbdConfigSetStringLenMax(&kbd, 63);
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_FAILED(rc) || !out[0]) return false;

    char *colon = strchr(out, ':');
    if (colon) {
        *colon = 0;
        int port = atoi(colon + 1);
        if (port > 0 && port < 65536) cfg.port = port;
    }
    snprintf(cfg.host, sizeof(cfg.host), "%s", out);
    return true;
}

static const char *find_moonlight(void) {
    static const char *candidates[] = {
        "sdmc:/switch/Moonlight-Switch.nro",
        "sdmc:/switch/Moonlight-Switch/Moonlight-Switch.nro",
        "sdmc:/switch/moonlight/Moonlight-Switch.nro",
        "sdmc:/switch/Moonlight.nro",
        "sdmc:/switch/moonlight.nro",
    };
    struct stat st;
    if (cfg.moonlight_path[0] && stat(cfg.moonlight_path, &st) == 0) return cfg.moonlight_path;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
        if (stat(candidates[i], &st) == 0) return candidates[i];
    return NULL;
}

// Hands over to Moonlight-Switch when this app exits. Returns an error
// message, or NULL if the hand-over is set up.
static const char *launch_moonlight(void) {
    if (!envHasNextLoad()) return "Start SimDash from the Homebrew Menu to use this";
    const char *path = find_moonlight();
    if (!path) return "Moonlight-Switch.nro not found in /switch/";
    static char argv[300];
    snprintf(argv, sizeof(argv), "\"%s\"", path);
    if (R_FAILED(envSetNextLoad(path, argv))) return "Couldn't start Moonlight";
    return NULL;
}

#else  // desktop build

static void *font_file;

static bool platform_init(void) {
    const char *host = getenv("SIMDASH_HOST");
    if (host) snprintf(cfg.host, sizeof(cfg.host), "%s", host);
    return true;
}

static void platform_exit(void) { SDL_free(font_file); }

static bool load_font(const void **data, int *size) {
    const char *path = getenv("SIMDASH_FONT");
    if (!path) path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    size_t len = 0;
    font_file = SDL_LoadFile(path, &len);
    if (!font_file) return false;
    *data = font_file;
    *size = (int)len;
    return true;
}

static unsigned platform_buttons(void) { return 0; }
static bool edit_host(void) { return false; }
static const char *launch_moonlight(void) { return "Moonlight is only available on the Switch"; }

#endif

static unsigned read_input(int *touch_x) {
    unsigned b = platform_buttons();
    SDL_Event e;
    *touch_x = -1;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            b |= BTN_QUIT;
            break;
        case SDL_FINGERDOWN:
            *touch_x = (int)(e.tfinger.x * SCREEN_W);
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.which != SDL_TOUCH_MOUSEID) *touch_x = e.button.x;
            break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym) {
            case SDLK_UP: b |= BTN_UP; break;
            case SDLK_DOWN: b |= BTN_DOWN; break;
            case SDLK_LEFT: b |= BTN_LEFT; break;
            case SDLK_RIGHT: b |= BTN_RIGHT; break;
            case SDLK_RETURN: b |= BTN_A; break;
            case SDLK_ESCAPE:
            case SDLK_BACKSPACE: b |= BTN_B; break;
            case SDLK_y: b |= BTN_Y; break;
            case SDLK_q: b |= BTN_PLUS; break;
            }
            break;
        }
    }
    return b;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    config_load(&cfg);
    if (!platform_init()) return 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 1;
    SDL_Window *win = SDL_CreateWindow("SimDash", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);

    const void *font_data = NULL;
    int font_size = 0;
    if (!load_font(&font_data, &font_size) || !render_init(ren, font_data, font_size)) {
        fprintf(stderr, "font init failed: %s\n", SDL_GetError());
        return 1;
    }

    net_start(cfg.host, cfg.port);

    const char *shot = getenv("SIMDASH_SCREENSHOT");  // desktop testing: save one frame and quit
    bool in_dash = getenv("SIMDASH_DASH") != NULL;
    if (getenv("SIMDASH_PAGE")) cfg.page = atoi(getenv("SIMDASH_PAGE"));
    int menu_sel = MENU_CUSTOM;
    const char *message = NULL;
    bool running = true;
    Uint64 start = SDL_GetPerformanceCounter(), last = start;
    PropValue vals[P_COUNT];
    int frames = 0;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double freq = (double)SDL_GetPerformanceFrequency();
        DashContext ctx = {
            .host = cfg.host,
            .port = cfg.port,
            .time = (now - start) / freq,
            .dt = (now - last) / freq,
        };
        last = now;

        int touch_x;
        unsigned b = read_input(&touch_x);
        if (b & (BTN_QUIT | BTN_PLUS)) running = false;

        if (in_dash) {
            if (b & BTN_B) in_dash = false;
            if (b & BTN_RIGHT || (touch_x >= 0 && touch_x >= SCREEN_W / 2)) cfg.page = (cfg.page + 1) % DASH_PAGE_COUNT;
            if (b & BTN_LEFT || (touch_x >= 0 && touch_x < SCREEN_W / 2)) cfg.page = (cfg.page + DASH_PAGE_COUNT - 1) % DASH_PAGE_COUNT;
            if (b & BTN_Y && edit_host()) {
                net_set_target(cfg.host, cfg.port);
                config_save(&cfg);
            }
        } else {
            if (b & BTN_UP) menu_sel = (menu_sel + MENU_COUNT - 1) % MENU_COUNT;
            if (b & BTN_DOWN) menu_sel = (menu_sel + 1) % MENU_COUNT;
            if (b & BTN_A) {
                message = NULL;
                switch (menu_sel) {
                case MENU_CUSTOM:
                    in_dash = true;
                    break;
                case MENU_MOONLIGHT:
                    message = launch_moonlight();
                    if (!message) running = false;
                    break;
                case MENU_SET_PC:
                    if (edit_host()) {
                        net_set_target(cfg.host, cfg.port);
                        config_save(&cfg);
                    }
                    break;
                case MENU_EXIT:
                    running = false;
                    break;
                }
            }
        }

        ctx.net = net_snapshot(vals);
        render_begin_frame();
        SDL_SetRenderDrawColor(ren, 8, 9, 12, 255);
        SDL_RenderClear(ren);
        if (in_dash)
            dash_draw(cfg.page, vals, &ctx);
        else
            menu_draw(menu_sel, cfg.host, cfg.port, message);

        if (shot && ++frames == 90) {
            SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
            SDL_SaveBMP(s, shot);
            SDL_FreeSurface(s);
            running = false;
        }
        SDL_RenderPresent(ren);
    }

    config_save(&cfg);
    net_stop();
    render_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    platform_exit();
    return 0;
}
