#include "render.h"

#include <stdio.h>
#include <string.h>

static const int font_px[FONT_COUNT] = {
    [FONT_SMALL] = 22,
    [FONT_LABEL] = 28,
    [FONT_VALUE] = 54,
    [FONT_BIG] = 96,
    [FONT_GEAR] = 340,
};

static SDL_Renderer *ren;
static TTF_Font *fonts[FONT_COUNT];

// Rendering text is slow, so each string is turned into a texture once
// and reused for as long as it keeps being drawn.
#define CACHE_SIZE 160
typedef struct {
    FontId font;
    SDL_Color color;
    char text[64];
    SDL_Texture *tex;
    int w, h;
    unsigned last_frame;
} CachedText;

static CachedText cache[CACHE_SIZE];
static unsigned frame;

bool render_init(SDL_Renderer *renderer, const void *font_data, int font_size) {
    ren = renderer;
    if (TTF_Init() != 0) return false;
    for (int i = 0; i < FONT_COUNT; i++) {
        SDL_RWops *rw = SDL_RWFromConstMem(font_data, font_size);
        fonts[i] = TTF_OpenFontRW(rw, 1, font_px[i]);
        if (!fonts[i]) return false;
        if (i >= FONT_VALUE) TTF_SetFontStyle(fonts[i], TTF_STYLE_BOLD);
    }
    return true;
}

void render_shutdown(void) {
    for (int i = 0; i < CACHE_SIZE; i++)
        if (cache[i].tex) SDL_DestroyTexture(cache[i].tex);
    for (int i = 0; i < FONT_COUNT; i++)
        if (fonts[i]) TTF_CloseFont(fonts[i]);
    TTF_Quit();
}

void render_begin_frame(void) { frame++; }

static bool same_color(SDL_Color a, SDL_Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

static CachedText *get_text(FontId font, const char *text, SDL_Color color) {
    CachedText *slot = NULL;
    for (int i = 0; i < CACHE_SIZE; i++) {
        CachedText *c = &cache[i];
        if (c->tex && c->font == font && same_color(c->color, color) && strcmp(c->text, text) == 0) {
            c->last_frame = frame;
            return c;
        }
        if (!slot || c->last_frame < slot->last_frame) slot = c;
    }
    if (slot->tex) SDL_DestroyTexture(slot->tex);
    slot->tex = NULL;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(fonts[font], text, color);
    if (!surf) return NULL;
    slot->tex = SDL_CreateTextureFromSurface(ren, surf);
    slot->w = surf->w;
    slot->h = surf->h;
    SDL_FreeSurface(surf);
    slot->font = font;
    slot->color = color;
    snprintf(slot->text, sizeof(slot->text), "%s", text);
    slot->last_frame = frame;
    return slot->tex ? slot : NULL;
}

int draw_text(FontId font, const char *text, SDL_Color color, int x, int y, Align align) {
    if (!text || !text[0]) return 0;
    CachedText *c = get_text(font, text, color);
    if (!c) return 0;
    SDL_Rect dst = {x, y - c->h / 2, c->w, c->h};
    if (align == ALIGN_CENTER) dst.x -= c->w / 2;
    if (align == ALIGN_RIGHT) dst.x -= c->w;
    SDL_RenderCopy(ren, c->tex, NULL, &dst);
    return c->w;
}

void fill_rect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(ren, &r);
}

void outline_rect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderDrawRect(ren, &r);
    SDL_Rect r2 = {x + 1, y + 1, w - 2, h - 2};
    SDL_RenderDrawRect(ren, &r2);
}
