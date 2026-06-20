#include "compositor.h"
#include "canvas.h"
#include <zirvflux.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <datetime.h>
#include <dirent.h>
#include <unistd.h>

/* ── Colours ─────────────────────────────────────────────────────── */
#define BG_COLOR      0xFF3A6EA5
#define WIN_BORDER    0xFF000000
#define TITLE_ACTIVE  0xFF4A8AC5
#define TITLE_INACTIVE 0xFF3A6080
#define TITLE_TEXT    0xFFFFFFFF
#define CLOSE_COLOR   0xFFCC3333
#define CLOSE_HOVER   0xFFFF5555
#define CLOSE_X       0xFFFFFFFF
#define CONTENT_BG    0xFFFFFFFF
#define TASKBAR_BG    0xFF2A2A3A
#define TASKBAR_TEXT  0xFFCCCCCC
#define TASKBAR_ACTIVE 0xFF4A6A8A
#define START_COLOR   0xFF4A8AC5
#define MENU_BG       0xFFF0F0F0
#define MENU_BORDER   0xFF888888
#define MENU_ITEM_H   48
#define MENU_COLS     2

static int cnv_max(int a, int b) { return a > b ? a : b; }

/* ── Buffers and display ─────────────────────────────────────────── */
static uint32_t g_fb[1024 * 768];
static zf_buffer_t g_buf;
static uint32_t g_sw = 1024, g_sh = 768;
static int g_mx = 512, g_my = 384;
static uint8_t g_prev_btn = 0;

/* ── Windows and apps ────────────────────────────────────────────── */
static Window g_wins[MAX_WINDOWS];
static int g_nwins = 0;
static int g_active = -1;
static int g_drag_win = -1;
static int g_drag_ox = 0, g_drag_oy = 0;
static int g_close_hover = -1;
static int g_menu_open = 0;
static int g_menu_hover = -1;

/* ── App declarations ────────────────────────────────────────────── */
static void draw_calc(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h);
static int  click_calc(int id, int mx, int my, int btn);
static void draw_clock(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h);
static int  click_clock(int id, int mx, int my, int btn);
static void tick_clock(int id);
static void draw_term(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h);
static int  click_term(int id, int mx, int my, int btn);
static void draw_fm(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h);
static int  click_fm(int id, int mx, int my, int btn);
static void draw_about(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h);

static AppDef g_apps[MAX_APPS] = {
    { "Calculator", 0xFF4488CC, "=",  240, 280, draw_calc,  click_calc,  0 },
    { "Clock",      0xFF44AA66, "C",  240, 140, draw_clock, click_clock, tick_clock },
    { "Terminal",   0xFF226622, ">_", 480, 300, draw_term,  click_term,  0 },
    { "Files",      0xFFCC8844, "F",  340, 280, draw_fm,    click_fm,    0 },
    { "About",      0xFF8844AA, "i",  320, 200, draw_about, 0,           0 },
};
static int g_napps = 5;

/* ── Window frame helpers ────────────────────────────────────────── */
static int frame_x(Window *w)   { return w->x; }
static int frame_y(Window *w)   { return w->y; }
static int frame_w(Window *w)   { return w->content_w + BORDER * 2; }
static int frame_h(Window *w)   { return w->content_h + TITLE_H + BORDER * 3; }
static int title_x(Window *w)   { return w->x + BORDER; }
static int title_y(Window *w)   { return w->y + BORDER; }
static int title_w(Window *w)   { return w->content_w; }
static int close_x(Window *w)   { return title_x(w) + title_w(w) - CLOSE_SIZE - CLOSE_PAD; }
static int close_y(Window *w)   { return title_y(w) + ((int)TITLE_H - (int)CLOSE_SIZE) / 2; }
static int cont_x(Window *w)    { return w->x + BORDER; }
static int cont_y(Window *w)    { return w->y + BORDER + TITLE_H + BORDER; }

/* ── Window management ───────────────────────────────────────────── */
static int launch_app(int app_idx)
{
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_wins[i].visible) continue;
        AppDef *a = &g_apps[app_idx];
        Window *w = &g_wins[i];
        memset(w, 0, sizeof(Window));
        w->app_id = app_idx;
        w->content_w = a->def_w;
        w->content_h = a->def_h;
        w->x = 20 + (i * 30) % (g_sw / 2);
        w->y = 40 + (i * 40) % (g_sh - TASKBAR_H - 200);
        w->visible = 1;
        size_t slen = strlen(a->name);
        size_t cplen = slen < sizeof(w->title) - 1 ? slen : sizeof(w->title) - 1;
        for (size_t j = 0; j < cplen; j++) w->title[j] = a->name[j];
        w->title[cplen] = '\0';
        g_nwins++;
        g_active = i;
        if (g_apps[app_idx].tick) g_apps[app_idx].tick(i);
        return i;
    }
    return -1;
}

static void raise_win(int idx)
{
    if (idx < 0 || idx >= MAX_WINDOWS || !g_wins[idx].visible) return;
    Window tmp = g_wins[idx];
    for (int i = idx; i < MAX_WINDOWS - 1; i++)
        g_wins[i] = g_wins[i + 1];
    int last = MAX_WINDOWS - 1;
    g_wins[last] = tmp;
    g_active = last;
    /* Find the last visible window */
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        if (g_wins[i].visible) { g_active = i; break; }
    }
}

/* ── Draw start button logo (7-bar Z from SVG) ───────────────────── */
static void draw_logo(uint32_t *fb, uint32_t fw, uint32_t fh, int bx, int by)
{
    (void)fw; (void)fh;
    static const struct { int y, h, x, w; uint32_t color; } bars[] = {
        { 5,  2, 14, 16, 0xFFFFFFFF },
        { 9,  2, 10, 24, 0xFFFFFFFF },
        { 13, 2, 6,  32, 0xFF88CCFF },
        { 17, 3, 2,  40, 0xFFFFFFFF },
        { 22, 2, 6,  32, 0xFF88CCFF },
        { 26, 2, 10, 24, 0xFFFFFFFF },
        { 30, 2, 14, 16, 0xFFFFFFFF },
    };
    for (int i = 0; i < 7; i++)
        canvas_fill_rect(fb, fw, fh, bx + bars[i].x, by + bars[i].y, bars[i].w, bars[i].h, bars[i].color);
}

/* ── Taskbar ─────────────────────────────────────────────────────── */
static void draw_taskbar(void)
{
    int y = (int)g_sh - TASKBAR_H;
    canvas_fill_rect(g_fb, g_sw, g_sh, 0, y, g_sw, TASKBAR_H, TASKBAR_BG);
    canvas_hline(g_fb, g_sw, g_sh, 0, y, g_sw, 0xFF000000);

    /* Start button */
    uint32_t sc = g_menu_open ? 0xFF5A9AD5 : START_COLOR;
    canvas_fill_rect(g_fb, g_sw, g_sh, 2, y + 2, START_BTN_W, TASKBAR_H - 4, sc);
    canvas_fill_rect(g_fb, g_sw, g_sh, 4, y + 4, START_BTN_W - 4, TASKBAR_H - 8, sc);
    draw_logo(g_fb, g_sw, g_sh, 2, y + 2);

    /* Running app buttons */
    int bx = START_BTN_W + 6;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_wins[i].visible) continue;
        uint32_t bc = (i == g_active) ? TASKBAR_ACTIVE : 0xFF3A3A5A;
        uint32_t tc = TASKBAR_TEXT;
        int bw = (int)strlen(g_wins[i].title) * CANVAS_FONT_STEP + 16;
        if (bw > 180) bw = 180;
        if (bx + bw > (int)g_sw - 80) break;
        canvas_fill_rect(g_fb, g_sw, g_sh, bx, y + 2, bw, TASKBAR_H - 4, bc);
        canvas_draw_text(g_fb, g_sw, g_sh, bx + 8, y + (TASKBAR_H - CANVAS_FONT_H) / 2,
                         (const uint8_t *)g_wins[i].title, tc);
        g_wins[i].active = (i == g_active) ? 1 : 0;
        bx += bw + 2;
    }

    /* Clock */
    struct datetime dt;
    char time_buf[12];
    int time_len = 0;
    if (getdatetime(&dt) == 0 && dt.year >= 2024) {
        time_buf[0] = '0' + (dt.hour / 10) % 10; time_buf[1] = '0' + dt.hour % 10;
        time_buf[2] = ':';
        time_buf[3] = '0' + (dt.minute / 10) % 10; time_buf[4] = '0' + dt.minute % 10;
        time_len = 5;
    }
    if (time_len == 0) {
        static const uint8_t def[] = "00:00";
        for (int i = 0; i < 5; i++) time_buf[i] = def[i];
        time_len = 5;
    }
    int tw = time_len * CANVAS_FONT_STEP;
    int tx = (int)g_sw - tw - 10;
    canvas_draw_text(g_fb, g_sw, g_sh, tx, y + (TASKBAR_H - CANVAS_FONT_H) / 2,
                     (const uint8_t *)time_buf, 0xFFFFFFFF);

    /* Start menu */
    if (g_menu_open) {
        int mx = 2, my = y - MENU_H;
        if (my < 0) my = 0;
        canvas_fill_rect(g_fb, g_sw, g_sh, mx, my, MENU_W, MENU_H, MENU_BG);
        canvas_fill_rect(g_fb, g_sw, g_sh, mx, my, MENU_W, MENU_H, MENU_BORDER);
        canvas_fill_rect(g_fb, g_sw, g_sh, mx + 1, my + 1, MENU_W - 2, MENU_H - 2, MENU_BG);

        int tile_w = (MENU_W - 10) / MENU_COLS;
        int tile_h = MENU_ITEM_H;
        g_menu_hover = -1;
        for (int i = 0; i < g_napps; i++) {
            int col = i % MENU_COLS;
            int row = i / MENU_COLS;
            int ix = mx + 4 + col * (tile_w + 4);
            int iy = my + 4 + row * (tile_h + 2);
            int hover = (g_mx >= ix && g_mx < ix + tile_w && g_my >= iy && g_my < iy + tile_h);
            if (hover) g_menu_hover = i;
            canvas_fill_round_rect(g_fb, g_sw, g_sh, ix + 2, iy + 2, tile_w - 4, tile_h - 4, 6, g_apps[i].color);
            canvas_draw_text(g_fb, g_sw, g_sh, ix + (tile_w - (int)strlen(g_apps[i].name) * CANVAS_FONT_STEP) / 2,
                             iy + tile_h - 18, (const uint8_t *)g_apps[i].name, 0xFFFFFFFF);
            canvas_draw_char_scaled(g_fb, g_sw, g_sh, ix + tile_w / 2 - 8, iy + 6,
                                    g_apps[i].icon[0], 0xFFFFFFFF, 2);
        }
    }
}

/* ── Window chrome ────────────────────────────────────────────────── */
static void draw_chrome(Window *w)
{
    int fx = frame_x(w), fy = frame_y(w);
    int fw_ = frame_w(w), fh_ = frame_h(w);
    int tx = title_x(w), ty = title_y(w);
    int tw = title_w(w);

    canvas_fill_rect(g_fb, g_sw, g_sh, fx, fy, fw_, fh_, WIN_BORDER);
    canvas_fill_rect(g_fb, g_sw, g_sh, fx + BORDER, fy + BORDER, fw_ - BORDER * 2, fh_ - BORDER * 2, CONTENT_BG);

    int is_active = (w == &g_wins[g_active]);
    uint32_t tc = is_active ? TITLE_ACTIVE : TITLE_INACTIVE;
    canvas_fill_rect(g_fb, g_sw, g_sh, tx, ty, tw, TITLE_H, tc);
    canvas_hline(g_fb, g_sw, g_sh, tx, ty + TITLE_H - 1, tw, WIN_BORDER);

    uint8_t *t = (uint8_t *)w->title;
    int tlen = 0; while (t[tlen]) tlen++;
    int text_x = tx + cnv_max((tw - tlen * CANVAS_FONT_STEP) / 2, 4);
    canvas_draw_text(g_fb, g_sw, g_sh, text_x, ty + (TITLE_H - CANVAS_FONT_H) / 2, t, TITLE_TEXT);

    int cx = close_x(w), cy = close_y(w);
    int is_ch = (g_close_hover >= 0 && &g_wins[g_close_hover] == w);
    canvas_fill_rect(g_fb, g_sw, g_sh, cx, cy, CLOSE_SIZE, CLOSE_SIZE, is_ch ? CLOSE_HOVER : CLOSE_COLOR);

    int xm = 4, xw = CLOSE_SIZE - xm * 2;
    for (int i = 0; i < xw; i++) {
        int px = cx + xm + i, py1 = cy + xm + i, py2 = cy + xm + (xw - 1 - i);
        if (px >= 0 && (uint32_t)px < g_sw && py1 >= 0 && (uint32_t)py1 < g_sh)
            g_fb[(uint32_t)py1 * g_sw + (uint32_t)px] = CLOSE_X;
        if (py2 != py1 && px >= 0 && (uint32_t)px < g_sw && py2 >= 0 && (uint32_t)py2 < g_sh)
            g_fb[(uint32_t)py2 * g_sw + (uint32_t)px] = CLOSE_X;
    }
}

/* ── Apps: Calculator ────────────────────────────────────────────── */
static void draw_calc(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h)
{
    (void)fw; (void)fh; (void)w; (void)h;
    Window *win = &g_wins[id];
    canvas_fill_rect(fb, g_sw, g_sh, x, y, 240, 280, 0xFFF0F0F0);

    /* Display */
    canvas_fill_rect(fb, g_sw, g_sh, x + 8, y + 8, 224, 32, 0xFFFFFFFF);
    canvas_fill_rect(fb, g_sw, g_sh, x + 8, y + 8, 224, 32, 0xFFDDDDDD);
    char dbuf[32];
    int di = 0;
    if (win->u.calc.fresh) {
        double v = win->u.calc.cur;
        if (v == (int)v) di = snprintf(dbuf, sizeof(dbuf), "%d", (int)v);
        else di = snprintf(dbuf, sizeof(dbuf), "%.2f", v);
    } else {
        di = snprintf(dbuf, sizeof(dbuf), "%d", (int)win->u.calc.cur);
    }
    if (di > 24) di = 24;
    int dx = x + 220 - di * CANVAS_FONT_STEP;
    canvas_draw_text(fb, g_sw, g_sh, dx, y + 12, (const uint8_t *)dbuf, 0xFF222222);

    /* Buttons: 7-8-9-/, 4-5-6-*, 1-2-3--, 0-C-=+ */
    static const char *btn_rows[4] = { "789/", "456*", "123-", "0C=+" };
    int bw = 52, bh = 38, gap = 6;
    int bx0 = x + 8;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = bx0 + c * (bw + gap);
            int by = y + 50 + r * (bh + gap);
            char ch = btn_rows[r][c];
            uint32_t col = (ch >= '0' && ch <= '9') ? 0xFFEEEEEE :
                           (ch == '=') ? 0xFF4488CC : 0xFFDDDDDD;
            canvas_fill_rect(fb, g_sw, g_sh, bx, by, bw, bh, col);
            canvas_fill_rect(fb, g_sw, g_sh, bx + 1, by + 1, bw - 2, bh - 2, col);
            uint32_t tcol = (ch == '=') ? 0xFFFFFFFF : 0xFF222222;
            canvas_draw_char(fb, g_sw, g_sh, bx + (bw - CANVAS_FONT_STEP) / 2, by + (bh - CANVAS_FONT_H) / 2, ch, tcol);
        }
    }
}

static int click_calc(int id, int mx, int my, int btn)
{
    (void)btn;
    Window *win = &g_wins[id];
    int x = cont_x(win), y = cont_y(win);
    int bw = 52, bh = 38, gap = 6;
    int bx0 = x + 8;
    static const char *btn_rows[4] = { "789/", "456*", "123-", "0C=+" };

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = bx0 + c * (bw + gap);
            int by = y + 50 + r * (bh + gap);
            if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) {
                char ch = btn_rows[r][c];
                if (ch >= '0' && ch <= '9') {
                    int d = ch - '0';
                    if (win->u.calc.fresh) { win->u.calc.cur = 0; win->u.calc.fresh = 0; }
                    win->u.calc.cur = win->u.calc.cur * 10 + d;
                } else if (ch == 'C') {
                    win->u.calc.cur = 0; win->u.calc.acc = 0;
                    win->u.calc.op = 0; win->u.calc.fresh = 1;
                } else if (ch == '=') {
                    if (win->u.calc.op) {
                        switch (win->u.calc.op) {
                        case '+': win->u.calc.cur = win->u.calc.acc + win->u.calc.cur; break;
                        case '-': win->u.calc.cur = win->u.calc.acc - win->u.calc.cur; break;
                        case '*': win->u.calc.cur = win->u.calc.acc * win->u.calc.cur; break;
                        case '/': if (win->u.calc.cur != 0) win->u.calc.cur = win->u.calc.acc / win->u.calc.cur; break;
                        }
                    }
                    win->u.calc.op = 0;
                    win->u.calc.acc = 0;
                    win->u.calc.fresh = 1;
                } else { /* +, -, *, / */
                    if (win->u.calc.op) {
                        switch (win->u.calc.op) {
                        case '+': win->u.calc.cur = win->u.calc.acc + win->u.calc.cur; break;
                        case '-': win->u.calc.cur = win->u.calc.acc - win->u.calc.cur; break;
                        case '*': win->u.calc.cur = win->u.calc.acc * win->u.calc.cur; break;
                        case '/': if (win->u.calc.cur != 0) win->u.calc.cur = win->u.calc.acc / win->u.calc.cur; break;
                        }
                    }
                    win->u.calc.acc = win->u.calc.cur;
                    win->u.calc.op = ch;
                    win->u.calc.fresh = 1;
                }
                return 1;
            }
        }
    }
    return 0;
}

/* ── Apps: Clock ─────────────────────────────────────────────────── */
static void draw_clock(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h)
{
    (void)fw; (void)fh; (void)w; (void)h;
    Window *win = &g_wins[id];
    canvas_fill_rect(fb, g_sw, g_sh, x, y, 240, 140, 0xFFF8F8F8);

    char buf[20];
    int len = snprintf(buf, sizeof(buf), "%02d:%02d:%02d", win->u.clk.h, win->u.clk.m, win->u.clk.s);
    int tw = len * CANVAS_FONT_STEP * 3;
    int tx = x + (240 - tw) / 2;
    canvas_draw_text_large(fb, g_sw, g_sh, tx, y + 25, (const uint8_t *)buf, 0xFF333333, 3);

    struct datetime dt;
    if (getdatetime(&dt) == 0 && dt.year >= 2024) {
        int len2 = snprintf(buf, sizeof(buf), "%04d-%02d-%02d", dt.year, dt.month, dt.day);
        int tw2 = len2 * CANVAS_FONT_STEP;
        canvas_draw_text(fb, g_sw, g_sh, x + (240 - tw2) / 2, y + 90,
                         (const uint8_t *)buf, 0xFF888888);
    }
}

static int click_clock(int id, int mx, int my, int btn)
{
    (void)id; (void)mx; (void)my; (void)btn;
    return 0;
}

static void tick_clock(int id)
{
    Window *win = &g_wins[id];
    struct datetime dt;
    if (getdatetime(&dt) == 0 && dt.year >= 2024) {
        win->u.clk.h = dt.hour;
        win->u.clk.m = dt.minute;
        win->u.clk.s = dt.second;
    }
}

/* ── Apps: Terminal ──────────────────────────────────────────────── */
static void term_puts(int id, const char *s)
{
    Window *win = &g_wins[id];
    while (*s) {
        char *line = win->u.term.lines[win->u.term.n % 80];
        int llen = 0; while (line[llen]) llen++;
        if (llen < 58) { line[llen] = *s; line[llen + 1] = 0; }
        else {
            win->u.term.n++;
            line = win->u.term.lines[win->u.term.n % 80];
            line[0] = *s; line[1] = 0;
        }
        s++;
    }
}

static void term_run(int id, const char *cmd)
{
    Window *win = &g_wins[id];
    while (*cmd == ' ') cmd++;
    if (*cmd == 0) return;

    /* Help */
    if (strcmp(cmd, "help") == 0) {
        term_puts(id, "Commands: help, ls, echo, date, clear, calc");
    }
    /* Date */
    else if (strcmp(cmd, "date") == 0) {
        struct datetime dt;
        char buf[48];
        if (getdatetime(&dt) == 0 && dt.year >= 2024)
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                     dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        else
            snprintf(buf, sizeof(buf), "RTC not available");
        term_puts(id, buf);
    }
    /* Clear */
    else if (strcmp(cmd, "clear") == 0) {
        for (int i = 0; i < 80; i++) win->u.term.lines[i][0] = 0;
        win->u.term.n = 0;
        win->u.term.scroll = 0;
    }
    /* Echo */
    else if (strncmp(cmd, "echo ", 5) == 0) {
        term_puts(id, cmd + 5);
    }
    /* Calc */
    else if (strncmp(cmd, "calc ", 5) == 0) {
        const char *p = cmd + 5;
        int a = 0, b = 0; char op = 0; int neg = 1;
        if (*p == '-') { neg = -1; p++; }
        while (*p >= '0' && *p <= '9') { a = a * 10 + (*p - '0'); p++; }
        a *= neg;
        while (*p == ' ') p++;
        if (*p) { op = *p; p++; }
        while (*p == ' ') p++;
        neg = 1;
        if (*p == '-') { neg = -1; p++; }
        while (*p >= '0' && *p <= '9') { b = b * 10 + (*p - '0'); p++; }
        b *= neg;
        int r = 0;
        switch (op) { case '+': r = a + b; break; case '-': r = a - b; break; case '*': r = a * b; break; case '/': r = b ? a / b : 0; break; default: op = 0; break; }
        if (op) { char buf[32]; snprintf(buf, sizeof(buf), "%d %c %d = %d", a, op, b, r); term_puts(id, buf); }
        else term_puts(id, "Usage: calc <a> <op> <b>");
    }
    /* Ls */
    else if (strcmp(cmd, "ls") == 0) {
        int fd = open("/", 0);
        if (fd >= 0) {
            struct dirent ents[32];
            int n = getdents(fd, ents, 32);
            for (int i = 0; i < n && i < 32; i++) {
                if (ents[i].d_name[0]) term_puts(id, ents[i].d_name);
            }
            close(fd);
        } else term_puts(id, "Cannot open root");
    }
    else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Unknown: %s (try help)", cmd);
        term_puts(id, buf);
    }
}

static int scancode_to_ascii(uint8_t code)
{
    static const unsigned char map[] = {
        0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
        'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
        'a','s','d','f','g','h','j','k','l',';',39,'`',0,
        92,'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,
    };
    if (code < sizeof(map) && map[code]) return map[code];
    return 0;
}

static void draw_term(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h)
{
    (void)fw; (void)fh;
    Window *win = &g_wins[id];
    canvas_fill_rect(fb, g_sw, g_sh, x, y, (uint32_t)w, (uint32_t)h, 0xFF1A1A2E);

    int ly = y + h - 24;
    int max_lines = (h - 32) / 16;
    int start = win->u.term.n > max_lines ? win->u.term.n - max_lines : 0;
    for (int i = start; i <= win->u.term.n && i < start + max_lines + 1; i++) {
        const char *line = win->u.term.lines[i % 80];
        if (line[0]) canvas_draw_text(fb, g_sw, g_sh, x + 4, y + 4 + (i - start) * 16,
                                       (const uint8_t *)line, 0xFF44DD44);
    }

    /* Prompt and input */
    char prompt[68];
    int plen = snprintf(prompt, sizeof(prompt), "~ $ %s", win->u.term.ibuf);
    if (plen > 64) plen = 64;
    prompt[plen] = 0;
    canvas_draw_text(fb, g_sw, g_sh, x + 4, ly, (const uint8_t *)prompt, 0xFF44DD44);
    /* Cursor */
    int cw = (plen + 1) * CANVAS_FONT_STEP;
    if (cw < w - 8) canvas_set_pixel(fb, g_sw, g_sh, x + 4 + cw, ly + CANVAS_FONT_H - 1, 0xFF44DD44);
}

static int click_term(int id, int mx, int my, int btn)
{
    (void)mx; (void)my; (void)btn;
    Window *win = &g_wins[id];
    key_event_t kev;
    while (read_keys(&kev) == 0) {
        if (!kev.pressed) continue;
        char c = (char)scancode_to_ascii((uint8_t)kev.keycode);
        if (c == '\n') {
            win->u.term.n++;
            char *line = win->u.term.lines[win->u.term.n % 80];
            for (int j = 0; j < 60; j++) line[j] = 0;
            term_run(id, win->u.term.ibuf);
            win->u.term.n++;
            for (int j = 0; j < 60; j++) win->u.term.ibuf[j] = 0;
            win->u.term.ipos = 0;
        } else if (c == '\b') {
            if (win->u.term.ipos > 0) {
                win->u.term.ipos--;
                win->u.term.ibuf[win->u.term.ipos] = 0;
            }
        } else if (c >= 32 && c < 127) {
            if (win->u.term.ipos < 58) {
                win->u.term.ibuf[win->u.term.ipos++] = c;
                win->u.term.ibuf[win->u.term.ipos] = 0;
            }
        }
    }
    return 0;
}

/* ── Apps: File Manager ──────────────────────────────────────────── */
static void draw_fm(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h)
{
    (void)fw; (void)fh;
    Window *win = &g_wins[id];
    canvas_fill_rect(fb, g_sw, g_sh, x, y, (uint32_t)w, (uint32_t)h, 0xFFFFFFFF);

    if (win->u.list.n == 0) {
        int fd = open("/", 0);
        if (fd >= 0) {
            struct dirent ents[64];
            int n = getdents(fd, ents, 64);
            win->u.list.n = n > 64 ? 64 : n;
            for (int i = 0; i < win->u.list.n; i++) {
                size_t sl = 0; while (ents[i].d_name[sl]) sl++;
                size_t cpl = sl < 48 ? sl : 47;
                for (size_t j = 0; j < cpl; j++) win->u.list.items[i][j] = ents[i].d_name[j];
                win->u.list.items[i][cpl] = 0;
            }
            close(fd);
        } else {
            win->u.list.n = 1;
            { const char *emsg = "(empty)"; int _j;
              for (_j = 0; emsg[_j] && _j < 47; _j++) win->u.list.items[0][_j] = emsg[_j];
              win->u.list.items[0][_j] = 0; }
        }
    }

    int ly = y + 4;
    int max_show = (h - 8) / 16;
    for (int i = 0; i < win->u.list.n && i < max_show; i++) {
        const char *name = win->u.list.items[i];
        uint32_t col = 0xFF333333;
        size_t sl = 0; while (name[sl]) sl++;
        if (sl > 0 && name[sl - 1] == '/') col = 0xFF2266AA;
        canvas_draw_text(fb, g_sw, g_sh, x + 8, ly, (const uint8_t *)name, col);
        ly += 16;
    }
}

static int click_fm(int id, int mx, int my, int btn)
{
    (void)id; (void)mx; (void)my; (void)btn;
    return 0;
}

/* ── Apps: About ─────────────────────────────────────────────────── */
static void draw_about(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h)
{
    (void)id; (void)fw; (void)fh; (void)w; (void)h;
    canvas_fill_rect(fb, g_sw, g_sh, x, y, 320, 200, 0xFFFFFFFF);
    static const uint8_t *lines[] = {
        (const uint8_t *)"Zirvium OS",
        (const uint8_t *)"Version 0.1.0",
        (const uint8_t *)"Architecture: x86_64",
        (const uint8_t *)"",
        (const uint8_t *)"Display: 1024x768 @ 32bpp",
        (const uint8_t *)"Theme: Cascade",
        (const uint8_t *)"",
        (const uint8_t *)"Copyright 2026",
    };
    int ly = y + 8;
    for (size_t i = 0; i < sizeof(lines)/sizeof(lines[0]); i++) {
        uint32_t tc = (i == 0) ? 0xFF225588 : 0xFF444444;
        canvas_draw_text(fb, g_sw, g_sh, x + 12, ly, lines[i], tc);
        ly += 16;
    }
}

/* ── Hit testing ──────────────────────────────────────────────────── */
static int h_title(Window *w, int mx, int my)
{
    return mx >= title_x(w) && mx < title_x(w) + (int)title_w(w)
        && my >= title_y(w) && my < title_y(w) + TITLE_H;
}

static int h_close(Window *w, int mx, int my)
{
    return mx >= close_x(w) && mx < close_x(w) + (int)CLOSE_SIZE
        && my >= close_y(w) && my < close_y(w) + (int)CLOSE_SIZE;
}

static int h_win(Window *w, int mx, int my)
{
    int fx = frame_x(w), fy = frame_y(w);
    int fw_ = frame_w(w), fh_ = frame_h(w);
    return mx >= fx && mx < fx + fw_ && my >= fy && my < fy + fh_;
}

static int h_taskbar_btn(int *out_idx)
{
    int y = (int)g_sh - TASKBAR_H;
    if (g_my < y || g_my >= (int)g_sh) return 0;
    /* Start button */
    if (g_mx >= 2 && g_mx < 2 + START_BTN_W) { *out_idx = -1; return 1; }
    /* App buttons */
    int bx = START_BTN_W + 6;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_wins[i].visible) continue;
        int bw = (int)strlen(g_wins[i].title) * CANVAS_FONT_STEP + 16;
        if (bw > 180) bw = 180;
        if (bx + bw > (int)g_sw - 80) break;
        if (g_mx >= bx && g_mx < bx + bw) { *out_idx = i; return 2; }
        bx += bw + 2;
    }
    return 0;
}

/* ── Main compositor loop ────────────────────────────────────────── */
int compositor_run(void)
{
    if (zf_connect() != 0) { printf("[FAIL] Compositor connect failed\n"); return -1; }
    zf_display_info_t info;
    if (zf_get_info(&info) != 0 || !info.connected) { printf("[FAIL] No display\n"); return -1; }
    g_sw = info.width; g_sh = info.height;
    if (zf_create_buffer(g_sw, g_sh, &g_buf) != 0) { printf("[FAIL] Buffer create\n"); return -1; }

    int running = 1;
    int tick = 0;

    while (running) {
        tick++;

        /* Poll mouse */
        zf_mouse_event_t ev;
        while (zf_read_mouse(&ev) == 0) {
            g_mx += ev.dx; g_my += ev.dy;
            if (g_mx < 0) g_mx = 0;
            if (g_mx >= (int)g_sw) g_mx = (int)g_sw - 1;
            if (g_my < 0) g_my = 0;
            if (g_my >= (int)g_sh) g_my = (int)g_sh - 1;
            zf_set_cursor(g_mx, g_my);

            uint8_t btn = ev.buttons & 1;
            uint8_t prev = g_prev_btn;
            g_prev_btn = btn;

            /* Dragging */
            if (g_drag_win >= 0) {
                Window *dw = &g_wins[g_drag_win];
                if (!btn) { g_drag_win = -1; }
                else {
                    dw->x = g_mx - g_drag_ox;
                    dw->y = g_my - g_drag_oy;
                    if (dw->x < 0) dw->x = 0;
                    if (dw->y < 0) dw->y = 0;
                    if (dw->y + frame_h(dw) > (int)g_sh - TASKBAR_H)
                        dw->y = (int)g_sh - TASKBAR_H - frame_h(dw);
                    continue;
                }
            }

            /* Close hover */
            g_close_hover = -1;
            for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
                if (!g_wins[i].visible) continue;
                if (h_close(&g_wins[i], g_mx, g_my)) { g_close_hover = i; break; }
            }

            /* Menu hover */
            if (g_menu_open) {
                g_menu_hover = -1;
                int my = (int)g_sh - TASKBAR_H - MENU_H;
                if (my < 0) my = 0;
                int tile_w = (MENU_W - 10) / MENU_COLS;
                int tile_h = MENU_ITEM_H;
                for (int i = 0; i < g_napps; i++) {
                    int col = i % MENU_COLS;
                    int row = i / MENU_COLS;
                    int ix = 2 + 4 + col * (tile_w + 4);
                    int iy = my + 4 + row * (tile_h + 2);
                    if (g_mx >= ix && g_mx < ix + tile_w && g_my >= iy && g_my < iy + tile_h)
                        g_menu_hover = i;
                }
            }

            /* Click */
            if (btn && !prev) {
                /* Check taskbar first */
                int tb_idx = -1;
                int tb_type = h_taskbar_btn(&tb_idx);
                if (tb_type == 1) {
                    /* Start button */
                    g_menu_open = !g_menu_open;
                    continue;
                }
                if (tb_type == 2 && tb_idx >= 0) {
                    /* App button - raise or close menu */
                    g_menu_open = 0;
                    raise_win(tb_idx);
                    continue;
                }

                /* Start menu click */
                if (g_menu_open && g_menu_hover >= 0) {
                    launch_app(g_menu_hover);
                    g_menu_open = 0;
                    continue;
                }

                g_menu_open = 0;

                /* Find topmost window under cursor */
                int hit = -1;
                for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
                    if (!g_wins[i].visible) continue;
                    if (h_win(&g_wins[i], g_mx, g_my)) { hit = i; break; }
                }

                if (hit >= 0) {
                    raise_win(hit);
                    if (h_close(&g_wins[g_active], g_mx, g_my)) {
                        g_wins[g_active].visible = 0;
                        g_nwins--;
                        /* Find new active */
                        g_active = -1;
                        for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
                            if (g_wins[i].visible) { g_active = i; break; }
                        }
                    } else if (h_title(&g_wins[g_active], g_mx, g_my)) {
                        g_drag_win = g_active;
                        g_drag_ox = g_mx - g_wins[g_active].x;
                        g_drag_oy = g_my - g_wins[g_active].y;
                    } else {
                        /* Click in content area - forward to app */
                        int app_id = g_wins[g_active].app_id;
                        if (app_id >= 0 && app_id < g_napps && g_apps[app_id].click) {
                            int cmx = g_mx - cont_x(&g_wins[g_active]);
                            int cmy = g_my - cont_y(&g_wins[g_active]);
                            g_apps[app_id].click(g_active, cmx, cmy, 1);
                        }
                    }
                }
            }
        }

        /* Tick apps */
        if (tick % 10 == 0) {
            for (int i = 0; i < MAX_WINDOWS; i++) {
                if (!g_wins[i].visible) continue;
                int app_id = g_wins[i].app_id;
                if (app_id >= 0 && app_id < g_napps && g_apps[app_id].tick)
                    g_apps[app_id].tick(i);
            }
        }

        /* Render */
        canvas_fill_rect(g_fb, g_sw, g_sh, 0, 0, g_sw, g_sh, BG_COLOR);

        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (!g_wins[i].visible) continue;
            draw_chrome(&g_wins[i]);
            int app_id = g_wins[i].app_id;
            if (app_id >= 0 && app_id < g_napps && g_apps[app_id].draw)
                g_apps[app_id].draw(i, g_fb, g_sw, g_sh, cont_x(&g_wins[i]), cont_y(&g_wins[i]),
                                    g_wins[i].content_w, g_wins[i].content_h);
        }

        draw_taskbar();

        zf_write_buffer(&g_buf, g_fb, g_sw * g_sh * 4);
        zf_present(&g_buf);
    }

    zf_destroy_buffer(&g_buf);
    return 0;
}
