#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>

#define SCREEN_W 1280
#define SCREEN_H 720

typedef enum { FONT_SMALL, FONT_LABEL, FONT_VALUE, FONT_BIG, FONT_GEAR, FONT_COUNT } FontId;
typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } Align;

// Font data must stay alive while fonts are open.
bool render_init(SDL_Renderer *renderer, const void *font_data, int font_size);
void render_shutdown(void);
void render_begin_frame(void);

// Draws text with its vertical center at y. Returns the drawn width.
int draw_text(FontId font, const char *text, SDL_Color color, int x, int y, Align align);
void fill_rect(int x, int y, int w, int h, SDL_Color color);
void outline_rect(int x, int y, int w, int h, SDL_Color color);

static inline SDL_Color rgb(Uint8 r, Uint8 g, Uint8 b) { return (SDL_Color){r, g, b, 255}; }
