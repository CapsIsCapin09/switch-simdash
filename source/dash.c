#include "dash.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "render.h"

#define C_BG rgb(8, 9, 12)
#define C_PANEL rgb(22, 24, 30)
#define C_LINE rgb(48, 52, 62)
#define C_LABEL rgb(140, 146, 160)
#define C_TEXT rgb(240, 242, 246)
#define C_DIM rgb(70, 74, 84)
#define C_GREEN rgb(40, 210, 90)
#define C_YELLOW rgb(250, 200, 30)
#define C_RED rgb(240, 50, 50)
#define C_BLUE rgb(50, 120, 255)
#define C_ORANGE rgb(255, 140, 20)

static double shown_rpm_pct;  // smoothed, the server only sends 10 updates a second

static void panel(int x, int y, int w, int h) {
    fill_rect(x, y, w, h, C_PANEL);
    outline_rect(x, y, w, h, C_LINE);
}

static void value_box(int x, int y, int w, int h, const char *label, const char *value, SDL_Color color) {
    panel(x, y, w, h);
    draw_text(FONT_LABEL, label, C_LABEL, x + 18, y + 26, ALIGN_LEFT);
    draw_text(FONT_VALUE, value, color, x + w - 18, y + h - 42, ALIGN_RIGHT);
}

static double rpm_target(const PropValue *v) {
    if (v[P_RPM_PCT].valid && v[P_RPM_PCT].num > 0) return v[P_RPM_PCT].num / 100.0;
    if (v[P_MAX_RPM].valid && v[P_MAX_RPM].num > 0) return v[P_RPM].num / v[P_MAX_RPM].num;
    return 0;
}

static bool at_redline(const PropValue *v) {
    if (v[P_REDLINE_RPM].valid && v[P_REDLINE_RPM].num > 0) return v[P_RPM].num >= v[P_REDLINE_RPM].num;
    return shown_rpm_pct >= 0.97;
}

static void draw_rpm_bar(const PropValue *v, const DashContext *ctx) {
    double target = rpm_target(v);
    if (target < 0) target = 0;
    if (target > 1) target = 1;
    shown_rpm_pct += (target - shown_rpm_pct) * (1.0 - exp(-ctx->dt * 18.0));

    const int segs = 24, x0 = 16, y = 14, h = 70, gap = 6;
    int seg_w = (SCREEN_W - 2 * x0 - (segs - 1) * gap) / segs;
    bool pit = v[P_PIT_LIMITER].valid && v[P_PIT_LIMITER].num > 0;
    bool flash = at_redline(v) && fmod(ctx->time, 0.15) < 0.075;
    bool pit_blink = fmod(ctx->time, 0.6) < 0.3;
    int lit = (int)(shown_rpm_pct * segs + 0.5);

    for (int i = 0; i < segs; i++) {
        double pos = (double)i / segs;
        SDL_Color c = pos < 0.6 ? C_GREEN : pos < 0.85 ? C_YELLOW : C_RED;
        if (pit) c = ((i % 2 == 0) == pit_blink) ? C_BLUE : C_DIM;
        else if (flash) c = C_BLUE;
        else if (i >= lit) c = C_DIM;
        fill_rect(x0 + i * (seg_w + gap), y, seg_w, h, c);
    }
    if (pit) {
        fill_rect(SCREEN_W / 2 - 170, y + 8, 340, h - 16, C_BG);
        draw_text(FONT_LABEL, "PIT LIMITER", C_BLUE, SCREEN_W / 2, y + h / 2, ALIGN_CENTER);
    }
}

static SDL_Color flag_color(const char *flag, bool *show) {
    *show = true;
    if (strcasecmp(flag, "Yellow") == 0) return C_YELLOW;
    if (strcasecmp(flag, "Blue") == 0) return C_BLUE;
    if (strcasecmp(flag, "White") == 0) return C_TEXT;
    if (strcasecmp(flag, "Black") == 0) return rgb(60, 60, 60);
    if (strcasecmp(flag, "Orange") == 0) return C_ORANGE;
    if (strcasecmp(flag, "Checkered") == 0) return C_TEXT;
    if (strcasecmp(flag, "Green") == 0) return C_GREEN;
    *show = false;
    return C_PANEL;
}

static void draw_status_strip(const PropValue *v, const DashContext *ctx) {
    const int y = 650, h = 56;
    char buf[64];
    bool show_flag;
    SDL_Color fc = flag_color(v[P_FLAG].str, &show_flag);

    if (ctx->net != NET_CONNECTED) {
        panel(16, y, SCREEN_W - 32, h);
        if (ctx->net == NET_FAILED && ctx->error && ctx->error[0]) {
            char line[200];
            snprintf(line, sizeof(line), "Can't reach SimHub at %s:%d: %s", ctx->host, ctx->port, ctx->error);
            draw_text(FONT_SMALL, line, C_ORANGE, SCREEN_W / 2, y + 17, ALIGN_CENTER);
            snprintf(line, sizeof(line), "This Switch: %s   (Y to change the PC address)", ctx->switch_ip);
            draw_text(FONT_SMALL, line, C_LABEL, SCREEN_W / 2, y + 40, ALIGN_CENTER);
        } else {
            snprintf(buf, sizeof(buf), "Connecting to %s:%d", ctx->host, ctx->port);
            draw_text(FONT_LABEL, buf, C_ORANGE, SCREEN_W / 2, y + h / 2, ALIGN_CENTER);
        }
        return;
    }
    if (!v[P_GAME_RUNNING].valid || v[P_GAME_RUNNING].num == 0) {
        panel(16, y, SCREEN_W - 32, h);
        draw_text(FONT_LABEL, "Connected. Waiting for a game...", C_LABEL, SCREEN_W / 2, y + h / 2, ALIGN_CENTER);
        return;
    }

    panel(16, y, SCREEN_W - 32, h);
    int cx = 40;
    const struct { const char *label; PropId id; const char *fmt; } items[] = {
        {"TC", P_TC, "%.0f"},
        {"ABS", P_ABS, "%.0f"},
        {"BIAS", P_BRAKE_BIAS, "%.1f"},
    };
    for (int i = 0; i < 3; i++) {
        cx += draw_text(FONT_LABEL, items[i].label, C_LABEL, cx, y + h / 2, ALIGN_LEFT) + 12;
        if (v[items[i].id].valid)
            snprintf(buf, sizeof(buf), items[i].fmt, v[items[i].id].num);
        else
            snprintf(buf, sizeof(buf), "-");
        cx += draw_text(FONT_LABEL, buf, C_TEXT, cx, y + h / 2, ALIGN_LEFT) + 48;
    }
    if (show_flag) {
        fill_rect(SCREEN_W - 16 - 360, y, 360, h, fc);
        snprintf(buf, sizeof(buf), "%s FLAG", v[P_FLAG].str);
        for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        bool dark_text = fc.r + fc.g + fc.b > 380;
        draw_text(FONT_LABEL, buf, dark_text ? C_BG : C_TEXT, SCREEN_W - 16 - 180, y + h / 2, ALIGN_CENTER);
    }
}

static void pedal_bar(int x, int y, int w, int h, double amount, SDL_Color color) {
    fill_rect(x, y, w, h, C_PANEL);
    if (amount < 0) amount = 0;
    if (amount > 100) amount = 100;
    int fh = (int)(h * amount / 100.0);
    fill_rect(x, y + h - fh, w, fh, color);
}

static void page_race(const PropValue *v, const DashContext *ctx) {
    char buf[64];
    draw_rpm_bar(v, ctx);

    // Center: gear and speed
    const int cx = SCREEN_W / 2;
    const char *gear = v[P_GEAR].valid && v[P_GEAR].str[0] ? v[P_GEAR].str : "-";
    SDL_Color gear_color = at_redline(v) ? C_RED : C_TEXT;
    draw_text(FONT_GEAR, gear, gear_color, cx, 320, ALIGN_CENTER);
    if (v[P_SPEED].valid)
        snprintf(buf, sizeof(buf), "%.0f", v[P_SPEED].num);
    else
        snprintf(buf, sizeof(buf), "---");
    draw_text(FONT_BIG, buf, C_TEXT, cx, 540, ALIGN_CENTER);
    const char *unit = v[P_SPEED_UNIT].str[0] ? v[P_SPEED_UNIT].str : "km/h";
    draw_text(FONT_LABEL, unit, C_LABEL, cx, 605, ALIGN_CENTER);

    pedal_bar(cx - 205, 120, 16, 500, v[P_THROTTLE].num, C_GREEN);
    pedal_bar(cx + 189, 120, 16, 500, v[P_BRAKE].num, C_RED);

    // Left: lap times
    const int lx = 16, bw = 400, bh = 160;
    prop_format_time(&v[P_CUR_LAP_TIME], buf, sizeof(buf));
    value_box(lx, 104, bw, bh, "LAP TIME", buf, C_TEXT);
    prop_format_time(&v[P_LAST_LAP_TIME], buf, sizeof(buf));
    value_box(lx, 104 + bh + 14, bw, bh, "LAST LAP", buf, C_TEXT);
    prop_format_time(&v[P_BEST_LAP_TIME], buf, sizeof(buf));
    value_box(lx, 104 + 2 * (bh + 14), bw, bh, "BEST LAP", buf, rgb(190, 110, 255));

    // Right: position, laps, fuel
    const int rx = SCREEN_W - 16 - bw;
    if (v[P_POSITION].valid && v[P_POSITION].num > 0) {
        if (v[P_OPPONENTS].valid && v[P_OPPONENTS].num > 0)
            snprintf(buf, sizeof(buf), "P%.0f / %.0f", v[P_POSITION].num, v[P_OPPONENTS].num);
        else
            snprintf(buf, sizeof(buf), "P%.0f", v[P_POSITION].num);
    } else {
        snprintf(buf, sizeof(buf), "P-");
    }
    value_box(rx, 104, bw, bh, "POSITION", buf, C_TEXT);

    if (v[P_TOTAL_LAPS].valid && v[P_TOTAL_LAPS].num > 0)
        snprintf(buf, sizeof(buf), "%.0f / %.0f", v[P_CUR_LAP].num, v[P_TOTAL_LAPS].num);
    else if (v[P_CUR_LAP].valid)
        snprintf(buf, sizeof(buf), "%.0f", v[P_CUR_LAP].num);
    else
        snprintf(buf, sizeof(buf), "-");
    value_box(rx, 104 + bh + 14, bw, bh, "LAP", buf, C_TEXT);

    if (v[P_FUEL].valid)
        snprintf(buf, sizeof(buf), "%.1f %s", v[P_FUEL].num, v[P_FUEL_UNIT].str[0] == 'G' ? "gal" : "L");
    else
        snprintf(buf, sizeof(buf), "-");
    int fy = 104 + 2 * (bh + 14);
    double fuel_pct = v[P_FUEL_PCT].valid ? v[P_FUEL_PCT].num : 0;
    value_box(rx, fy, bw, bh, "FUEL", buf, fuel_pct > 0 && fuel_pct < 10 ? C_RED : C_TEXT);
    fill_rect(rx + 18, fy + 52, bw - 36, 8, C_DIM);
    if (fuel_pct > 0)
        fill_rect(rx + 18, fy + 52, (int)((bw - 36) * fmin(fuel_pct, 100) / 100.0), 8, fuel_pct < 10 ? C_RED : C_GREEN);

    draw_status_strip(v, ctx);
}

static SDL_Color tyre_color(const PropValue *t) {
    if (!t->valid || t->num <= 0) return C_DIM;
    if (t->num < 65) return C_BLUE;
    if (t->num < 100) return C_GREEN;
    if (t->num < 110) return C_YELLOW;
    return C_RED;
}

static void tyre_box(int x, int y, const char *label, const PropValue *temp, const PropValue *pres, const char *unit) {
    const int w = 280, h = 200;
    SDL_Color c = tyre_color(temp);
    panel(x, y, w, h);
    fill_rect(x, y, 12, h, c);
    draw_text(FONT_LABEL, label, C_LABEL, x + 30, y + 28, ALIGN_LEFT);
    char buf[32];
    if (temp->valid)
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", temp->num);
    else
        snprintf(buf, sizeof(buf), "-");
    draw_text(FONT_VALUE, buf, c.r == C_DIM.r && c.g == C_DIM.g ? C_TEXT : c, x + w / 2 + 6, y + 100, ALIGN_CENTER);
    if (pres->valid)
        snprintf(buf, sizeof(buf), "%.1f %s", pres->num, unit);
    else
        snprintf(buf, sizeof(buf), "-");
    draw_text(FONT_LABEL, buf, C_TEXT, x + w / 2 + 6, y + 162, ALIGN_CENTER);
}

static void page_car(const PropValue *v, const DashContext *ctx) {
    char buf[64];
    draw_rpm_bar(v, ctx);

    const char *unit = v[P_TYRE_PRES_UNIT].str[0] ? v[P_TYRE_PRES_UNIT].str : "";
    const int tx = 60, ty = 110, gx = 300, gy = 220;
    tyre_box(tx, ty, "FRONT LEFT", &v[P_TYRE_TEMP_FL], &v[P_TYRE_PRES_FL], unit);
    tyre_box(tx + gx, ty, "FRONT RIGHT", &v[P_TYRE_TEMP_FR], &v[P_TYRE_PRES_FR], unit);
    tyre_box(tx, ty + gy, "REAR LEFT", &v[P_TYRE_TEMP_RL], &v[P_TYRE_PRES_RL], unit);
    tyre_box(tx + gx, ty + gy, "REAR RIGHT", &v[P_TYRE_TEMP_RR], &v[P_TYRE_PRES_RR], unit);

    const int rx = 740, bw = 480, bh = 120;
    const char *gear = v[P_GEAR].valid && v[P_GEAR].str[0] ? v[P_GEAR].str : "-";
    snprintf(buf, sizeof(buf), "%s   %.0f", gear, v[P_SPEED].num);
    value_box(rx, 110, bw, bh, "GEAR / SPEED", buf, C_TEXT);
    if (v[P_WATER_TEMP].valid)
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", v[P_WATER_TEMP].num);
    else
        snprintf(buf, sizeof(buf), "-");
    value_box(rx, 110 + bh + 10, bw, bh, "WATER TEMP", buf, C_TEXT);
    if (v[P_OIL_TEMP].valid)
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", v[P_OIL_TEMP].num);
    else
        snprintf(buf, sizeof(buf), "-");
    value_box(rx, 110 + 2 * (bh + 10), bw, bh, "OIL TEMP", buf, C_TEXT);
    prop_format_time(&v[P_SESSION_LEFT], buf, sizeof(buf));
    value_box(rx, 110 + 3 * (bh + 10), bw, bh, "SESSION LEFT", buf, C_TEXT);

    draw_status_strip(v, ctx);
}

void dash_draw(int page, const PropValue *v, const DashContext *ctx) {
    if (page == 1)
        page_car(v, ctx);
    else
        page_race(v, ctx);
}

void menu_draw(int selected, const char *host, int port, const char *switch_ip) {
    char buf[200];
    draw_text(FONT_BIG, "SimDash", C_TEXT, SCREEN_W / 2, 110, ALIGN_CENTER);
    draw_text(FONT_LABEL, "SimHub digital dash for Switch", C_LABEL, SCREEN_W / 2, 185, ALIGN_CENTER);

    snprintf(buf, sizeof(buf), "PC address: %s:%d", host, port);
    const char *labels[MENU_COUNT] = {
        "Custom dash",
        "SimHub dash",
        buf,
        "Exit",
    };
    for (int i = 0; i < MENU_COUNT; i++) {
        int y = 260 + i * 86;
        bool sel = i == selected;
        fill_rect(340, y, 600, 70, sel ? C_BLUE : C_PANEL);
        if (!sel) outline_rect(340, y, 600, 70, C_LINE);
        draw_text(FONT_LABEL, labels[i], C_TEXT, SCREEN_W / 2, y + 35, ALIGN_CENTER);
    }
    snprintf(buf, sizeof(buf), "This Switch: %s", switch_ip);
    draw_text(FONT_LABEL, buf, C_LABEL, SCREEN_W / 2, 625, ALIGN_CENTER);
    draw_text(FONT_SMALL, "Up/Down select   A open   + exit", C_LABEL, SCREEN_W / 2, 690, ALIGN_CENTER);
}
