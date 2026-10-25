// SimDash: a native SimHub dashboard for Nintendo Switch homebrew.
// Two views: a custom dash drawn from SimHub Property Server data, and a
// live picture of a real SimHub dashboard sent by SimDash Streamer on the PC.
// Builds for the Switch with devkitA64/libnx, and for a desktop with SDL2
// so the dash can be tried without a console.

#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "config.h"
#include "dash.h"
#include "net.h"
#include "render.h"
#include "sockutil.h"
#include "stream.h"

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

#endif

typedef enum { VIEW_MENU, VIEW_CUSTOM, VIEW_STREAM } View;

static SDL_Texture *stream_tex;
static int stream_tex_w, stream_tex_h;
static uint8_t *stream_pixels;
static unsigned stream_seq;

// Shows the newest frame from SimDash Streamer, scaled to fit the screen.
static void draw_stream(SDL_Renderer *ren, const DashContext *ctx, bool show_info) {
    const int buf_size = 1920 * 1080 * 4;
    if (!stream_pixels) stream_pixels = malloc(buf_size);
    StreamInfo info;
    if (stream_pixels && stream_get_frame(stream_seq, stream_pixels, buf_size, &info)) {
        stream_seq = info.seq;
        if (!stream_tex || stream_tex_w != info.width || stream_tex_h != info.height) {
            if (stream_tex) SDL_DestroyTexture(stream_tex);
            stream_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, info.width, info.height);
            SDL_SetTextureScaleMode(stream_tex, SDL_ScaleModeLinear);
            stream_tex_w = info.width;
            stream_tex_h = info.height;
        }
        SDL_UpdateTexture(stream_tex, NULL, stream_pixels, info.width * 4);
    }

    char buf[320];
    if (info.state == NET_CONNECTED && stream_tex && info.seq > 0) {
        double scale = fmin((double)SCREEN_W / stream_tex_w, (double)SCREEN_H / stream_tex_h);
        int w = (int)(stream_tex_w * scale), h = (int)(stream_tex_h * scale);
        SDL_Rect dst = {(SCREEN_W - w) / 2, (SCREEN_H - h) / 2, w, h};
        SDL_RenderCopy(ren, stream_tex, NULL, &dst);
        if (show_info) {
            snprintf(buf, sizeof(buf), "%dx%d  %.0f fps", stream_tex_w, stream_tex_h, info.fps);
            fill_rect(10, 10, 260, 40, rgb(0, 0, 0));
            draw_text(FONT_SMALL, buf, rgb(240, 242, 246), 22, 30, ALIGN_LEFT);
        }
        return;
    }

    if (info.state == NET_CONNECTED)
        snprintf(buf, sizeof(buf), "Connected. Waiting for the first picture...");
    else if (info.state == NET_FAILED)
        snprintf(buf, sizeof(buf), "Can't reach SimDash Streamer at %s:%d: %s", ctx->host, cfg.stream_port, info.error);
    else
        snprintf(buf, sizeof(buf), "Connecting to SimDash Streamer at %s:%d", ctx->host, cfg.stream_port);
    draw_text(FONT_VALUE, "SimHub dash", rgb(240, 242, 246), SCREEN_W / 2, 300, ALIGN_CENTER);
    draw_text(info.state == NET_FAILED ? FONT_SMALL : FONT_LABEL, buf, rgb(255, 140, 20), SCREEN_W / 2, 380, ALIGN_CENTER);
    snprintf(buf, sizeof(buf), "This Switch: %s", ctx->switch_ip);
    draw_text(FONT_SMALL, buf, rgb(140, 146, 160), SCREEN_W / 2, 430, ALIGN_CENTER);
    draw_text(FONT_SMALL, "Start SimDash Streamer on the PC and pick your SimHub dash window.   B back", rgb(140, 146, 160),
              SCREEN_W / 2, 690, ALIGN_CENTER);
}

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
    View view = VIEW_MENU;
    if (getenv("SIMDASH_DASH")) view = VIEW_CUSTOM;
    if (getenv("SIMDASH_STREAM")) {
        view = VIEW_STREAM;
        stream_start(cfg.host, cfg.stream_port);
    }
    if (getenv("SIMDASH_PAGE")) cfg.page = atoi(getenv("SIMDASH_PAGE"));
    int menu_sel = MENU_CUSTOM;
    bool show_stream_info = false;
    bool running = true;
    Uint64 start = SDL_GetPerformanceCounter(), last = start;
    PropValue vals[P_COUNT];
    int frames = 0;
    char net_error[128] = "", switch_ip[48] = "";

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

        if (view == VIEW_CUSTOM) {
            if (b & BTN_B) view = VIEW_MENU;
            if (b & BTN_RIGHT || (touch_x >= 0 && touch_x >= SCREEN_W / 2)) cfg.page = (cfg.page + 1) % DASH_PAGE_COUNT;
            if (b & BTN_LEFT || (touch_x >= 0 && touch_x < SCREEN_W / 2)) cfg.page = (cfg.page + DASH_PAGE_COUNT - 1) % DASH_PAGE_COUNT;
            if (b & BTN_Y && edit_host()) {
                net_set_target(cfg.host, cfg.port);
                config_save(&cfg);
            }
        } else if (view == VIEW_STREAM) {
            if (b & BTN_B) {
                stream_stop();
                view = VIEW_MENU;
            }
            if (b & BTN_Y) show_stream_info = !show_stream_info;
        } else {
            if (b & BTN_UP) menu_sel = (menu_sel + MENU_COUNT - 1) % MENU_COUNT;
            if (b & BTN_DOWN) menu_sel = (menu_sel + 1) % MENU_COUNT;
            if (b & BTN_A) {
                switch (menu_sel) {
                case MENU_CUSTOM:
                    view = VIEW_CUSTOM;
                    break;
                case MENU_STREAM:
                    stream_start(cfg.host, cfg.stream_port);
                    view = VIEW_STREAM;
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
        net_last_error(net_error, sizeof(net_error));
        if (frames % 60 == 0) local_ip(switch_ip, sizeof(switch_ip));
        frames++;
        ctx.error = net_error;
        ctx.switch_ip = switch_ip;
        render_begin_frame();
        SDL_SetRenderDrawColor(ren, 8, 9, 12, 255);
        SDL_RenderClear(ren);
        if (view == VIEW_CUSTOM)
            dash_draw(cfg.page, vals, &ctx);
        else if (view == VIEW_STREAM)
            draw_stream(ren, &ctx, show_stream_info);
        else
            menu_draw(menu_sel, cfg.host, cfg.port, switch_ip);

        if (shot && frames == 90) {
            SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
            SDL_SaveBMP(s, shot);
            SDL_FreeSurface(s);
            running = false;
        }
        SDL_RenderPresent(ren);
    }

    config_save(&cfg);
    stream_stop();
    net_stop();
    if (stream_tex) SDL_DestroyTexture(stream_tex);
    free(stream_pixels);
    render_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    platform_exit();
    return 0;
}
