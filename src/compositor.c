#include "compositor.h"
#include "canvas.h"
#include <zirvflux.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

/* ── Color palette (Cascade theme) ────────────────────────────────── */
#define BG_COLOR       0xFF3A6EA5
#define WIN_BORDER     0xFF000000
#define TITLE_ACTIVE   0xFF4A8AC5
#define TITLE_INACTIVE 0xFF3A6080
#define TITLE_TEXT     0xFFFFFFFF
#define CLOSE_COLOR    0xFFCC3333
#define CLOSE_HOVER    0xFFFF5555
#define CLOSE_X        0xFFFFFFFF
#define CONTENT_BG     0xFFEEEEEE

static int cnv_max(int a, int b) { return a > b ? a : b; }

/* ── Window system state ─────────────────────────────────────────── */
static Window g_windows[MAX_WINDOWS];
static int g_num_windows = 0;
static int g_active_win  = -1;
static int g_drag_win    = -1;
static int g_drag_ox     = 0;
static int g_drag_oy     = 0;
static int g_close_hover = -1;

static uint32_t g_fb[1024 * 768];
static zf_buffer_t g_buf;
static uint32_t g_scr_w = 1024;
static uint32_t g_scr_h = 768;
static int g_cursor_x = 512;
static int g_cursor_y = 384;
static uint8_t g_prev_btn = 0;

/* ── Window frame helpers ─────────────────────────────────────────── */
static int frame_x(Window *w)   { return w->x; }
static int frame_y(Window *w)   { return w->y; }
static int frame_w(Window *w)   { return w->content_w + BORDER * 2; }
static int frame_h(Window *w)   { return w->content_h + TITLE_H + BORDER * 3; }
static int title_x(Window *w)   { return w->x + BORDER; }
static int title_y(Window *w)   { return w->y + BORDER; }
static int title_w(Window *w)   { return w->content_w; }

static int close_x(Window *w)   { return title_x(w) + title_w(w) - CLOSE_SIZE - CLOSE_PAD; }
static int close_y(Window *w)   { return title_y(w) + ((int)TITLE_H - (int)CLOSE_SIZE) / 2; }
static int content_x(Window *w) { return w->x + BORDER; }
static int content_y(Window *w) { return w->y + BORDER + TITLE_H + BORDER; }

/* ── Demo content functions ───────────────────────────────────────── */

static void draw_file_content(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                               int x, int y, int w, int h)
{
    canvas_fill_rect(fb, fb_w, fb_h, x, y, (uint32_t)w, (uint32_t)h, 0xFFFFFFFF);
    static const uint8_t *items[] = {
        (const uint8_t *)"Documents/", (const uint8_t *)"Pictures/",
        (const uint8_t *)"Music/",     (const uint8_t *)"Videos/",
        (const uint8_t *)"readme.txt", (const uint8_t *)"notes.txt",
        (const uint8_t *)"project.zip",(const uint8_t *)".config",
    };
    int ly = y + 6;
    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); i++) {
        uint32_t col = items[i][items[i][0] == '.' ? 0 : (items[i][0] == 'D' || items[i][0] == 'P' || items[i][0] == 'M' || items[i][0] == 'V') ? 0 : 0] ? 0xFF2266AA : 0xFF444444;
        (void)col;
        uint32_t tc = 0xFF2266AA;
        const uint8_t *s = items[i];
        size_t slen = 0;
        while (s[slen]) slen++;
        if (slen > 0 && s[slen-1] == '/') {
            tc = 0xFF2266AA;
        } else if (s[0] == '.') {
            tc = 0xFF888888;
        } else {
            tc = 0xFF333333;
        }
        canvas_draw_text(fb, fb_w, fb_h, x + 8, ly, items[i], tc);
        ly += 16;
    }
}

static void draw_terminal_content(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                                   int x, int y, int w, int h)
{
    canvas_fill_rect(fb, fb_w, fb_h, x, y, (uint32_t)w, (uint32_t)h, 0xFF1A1A2E);
    static const uint8_t *lines[] = {
        (const uint8_t *)"Zirvium OS 1.0", (const uint8_t *)"",
        (const uint8_t *)"~ $ ls -la",     (const uint8_t *)"",
        (const uint8_t *)"total 128",      (const uint8_t *)"",
        (const uint8_t *)"drwxr-xr-x  5 root root  4096",
        (const uint8_t *)"-rw-r--r--  1 root root   256",
        (const uint8_t *)"",
        (const uint8_t *)"~ $ █",
    };
    int ly = y + 6;
    for (size_t i = 0; i < sizeof(lines)/sizeof(lines[0]); i++) {
        canvas_draw_text(fb, fb_w, fb_h, x + 8, ly, lines[i], 0xFF44DD44);
        ly += 16;
    }
}

static void draw_about_content(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                                int x, int y, int w, int h)
{
    canvas_fill_rect(fb, fb_w, fb_h, x, y, (uint32_t)w, (uint32_t)h, 0xFFFFFFFF);
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
        canvas_draw_text(fb, fb_w, fb_h, x + 12, ly, lines[i], tc);
        ly += 16;
    }
}

/* ── Window chrome drawing ────────────────────────────────────────── */

static void draw_window_chrome(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, Window *w)
{
    int fx = frame_x(w), fy = frame_y(w);
    int fw_ = frame_w(w), fh_ = frame_h(w);
    int tx = title_x(w), ty = title_y(w);
    int tw = title_w(w);

    /* outer border */
    canvas_fill_rect(fb, fb_w, fb_h, fx, fy, (uint32_t)fw_, (uint32_t)fh_, WIN_BORDER);

    /* inner area fill (content background) */
    int inner_x = fx + BORDER, inner_y = fy + BORDER;
    int inner_w = fw_ - BORDER * 2;
    int inner_h = fh_ - BORDER * 2;
    canvas_fill_rect(fb, fb_w, fb_h, inner_x, inner_y, (uint32_t)inner_w, (uint32_t)inner_h, CONTENT_BG);

    /* title bar */
    int is_active = (w - g_windows == g_active_win);
    uint32_t title_col = is_active ? TITLE_ACTIVE : TITLE_INACTIVE;
    canvas_fill_rect(fb, fb_w, fb_h, tx, ty, (uint32_t)tw, (uint32_t)TITLE_H, title_col);

    /* draw 1px line under title */
    canvas_hline(fb, fb_w, fb_h, tx, ty + TITLE_H - 1, (uint32_t)tw, WIN_BORDER);

    /* title text */
    uint8_t *t = (uint8_t *)w->title;
    size_t tlen = 0;
    while (t[tlen]) tlen++;
    int text_w = (int)tlen * CANVAS_FONT_STEP;
    int text_x = tx + cnv_max((tw - text_w) / 2, 4);
    int text_y = ty + (TITLE_H - CANVAS_FONT_H) / 2;
    canvas_draw_text(fb, fb_w, fb_h, text_x, text_y, t, TITLE_TEXT);

    /* close button */
    int cx = close_x(w), cy = close_y(w);
    int is_close_hover = (g_close_hover == (int)(w - g_windows));
    uint32_t close_col = is_close_hover ? CLOSE_HOVER : CLOSE_COLOR;
    canvas_fill_rect(fb, fb_w, fb_h, cx, cy, CLOSE_SIZE, CLOSE_SIZE, close_col);

    /* X mark on close button */
    int xm = 4, ym = 4;
    int xe = cx + xm, ye = cy + ym;
    int xw = (int)CLOSE_SIZE - xm * 2;
    for (int i = 0; i < xw; i++) {
        int px = xe + i;
        int py1 = ye + i;
        int py2 = ye + (xw - 1 - i);
        if (px >= 0 && (uint32_t)px < fb_w && py1 >= 0 && (uint32_t)py1 < fb_h)
            fb[(uint32_t)py1 * fb_w + (uint32_t)px] = CLOSE_X;
        if (py2 != py1 && px >= 0 && (uint32_t)px < fb_w && py2 >= 0 && (uint32_t)py2 < fb_h)
            fb[(uint32_t)py2 * fb_w + (uint32_t)px] = CLOSE_X;
    }
}

/* ── Hit testing ──────────────────────────────────────────────────── */

static int hit_title_bar(Window *w, int mx, int my)
{
    return mx >= title_x(w) && mx < title_x(w) + (int)title_w(w)
        && my >= title_y(w) && my < title_y(w) + TITLE_H;
}

static int hit_close(Window *w, int mx, int my)
{
    return mx >= close_x(w) && mx < close_x(w) + (int)CLOSE_SIZE
        && my >= close_y(w) && my < close_y(w) + (int)CLOSE_SIZE;
}

static int hit_window(Window *w, int mx, int my)
{
    int fx = frame_x(w), fy = frame_y(w);
    int fw_ = frame_w(w), fh_ = frame_h(w);
    return mx >= fx && mx < fx + fw_ && my >= fy && my < fy + fh_;
}

/* ── Window management ────────────────────────────────────────────── */

static int add_window(int x, int y, int cw, int ch, const char *title,
                       void (*draw)(uint32_t *, uint32_t, uint32_t, int, int, int, int))
{
    if (g_num_windows >= MAX_WINDOWS) return -1;
    Window *w = &g_windows[g_num_windows];
    w->x = x;
    w->y = y;
    w->content_w = cw;
    w->content_h = ch;
    w->visible = 1;
    w->draw_content = draw;
    w->drag_grab_x = -1;
    w->drag_grab_y = -1;
    size_t slen = strlen(title);
    size_t cplen = slen < sizeof(w->title) - 1 ? slen : sizeof(w->title) - 1;
    for (size_t i = 0; i < cplen; i++) w->title[i] = title[i];
    w->title[cplen] = '\0';
    g_num_windows++;
    g_active_win = g_num_windows - 1;
    return g_num_windows - 1;
}

static void raise_window(int idx)
{
    if (idx < 0 || idx >= g_num_windows || !g_windows[idx].visible) return;
    Window tmp = g_windows[idx];
    for (int i = idx; i < g_num_windows - 1; i++)
        g_windows[i] = g_windows[i + 1];
    g_windows[g_num_windows - 1] = tmp;
    g_active_win = g_num_windows - 1;
}

/* ── Main compositor entry point ──────────────────────────────────── */

int compositor_run(void)
{
    if (zf_connect() != 0) {
        printf("[CASCADE] Failed to connect as compositor\n");
        return -1;
    }
    zf_display_info_t info;
    if (zf_get_info(&info) != 0 || !info.connected) {
        printf("[CASCADE] No display connected\n");
        return -1;
    }
    g_scr_w = info.width;
    g_scr_h = info.height;

    if (zf_create_buffer(g_scr_w, g_scr_h, &g_buf) != 0) {
        printf("[CASCADE] Failed to create display buffer\n");
        return -1;
    }

    /* Create demo windows */
    add_window(20, 40, 280, 160, "File Manager", draw_file_content);
    add_window(330, 60, 320, 180, "Terminal", draw_terminal_content);
    add_window(200, 280, 280, 160, "About Zirvium", draw_about_content);

    int running = 1;
    while (running) {
        /* Render desktop background */
        canvas_fill_rect(g_fb, g_scr_w, g_scr_h, 0, 0, g_scr_w, g_scr_h, BG_COLOR);

        /* Render windows from back to front */
        for (int i = 0; i < g_num_windows; i++) {
            Window *w = &g_windows[i];
            if (!w->visible) continue;
            draw_window_chrome(g_fb, g_scr_w, g_scr_h, w);
            if (w->draw_content) {
                w->draw_content(g_fb, g_scr_w, g_scr_h,
                                content_x(w), content_y(w),
                                w->content_w, w->content_h);
            }
        }

        /* Write and present */
        zf_write_buffer(&g_buf, g_fb, g_scr_w * g_scr_h * 4);
        zf_present(&g_buf);

        /* Handle input */
        zf_mouse_event_t ev;
        while (zf_read_mouse(&ev) == 0) {
            g_cursor_x += ev.dx;
            g_cursor_y += ev.dy;
            if (g_cursor_x < 0) g_cursor_x = 0;
            if (g_cursor_x >= (int)g_scr_w) g_cursor_x = (int)g_scr_w - 1;
            if (g_cursor_y < 0) g_cursor_y = 0;
            if (g_cursor_y >= (int)g_scr_h) g_cursor_y = (int)g_scr_h - 1;
            zf_set_cursor(g_cursor_x, g_cursor_y);

            uint8_t btn = ev.buttons & 1;
            uint8_t prev = g_prev_btn;
            g_prev_btn = btn;

            /* Dragging */
            if (g_drag_win >= 0) {
                Window *dw = &g_windows[g_drag_win];
                if (!btn) {
                    g_drag_win = -1;
                } else {
                    dw->x = g_cursor_x - g_drag_ox;
                    dw->y = g_cursor_y - g_drag_oy;
                    if (dw->x < 0) dw->x = 0;
                    if (dw->y < 0) dw->y = 0;
                    continue;
                }
            }

            /* Hover for close button */
            g_close_hover = -1;
            for (int i = g_num_windows - 1; i >= 0; i--) {
                Window *w = &g_windows[i];
                if (!w->visible) continue;
                if (hit_close(w, g_cursor_x, g_cursor_y)) {
                    g_close_hover = i;
                    break;
                }
            }

            /* Mouse press */
            if (btn && !prev) {
                /* Find topmost window under cursor */
                int hit = -1;
                for (int i = g_num_windows - 1; i >= 0; i--) {
                    if (!g_windows[i].visible) continue;
                    if (hit_window(&g_windows[i], g_cursor_x, g_cursor_y)) {
                        hit = i;
                        break;
                    }
                }
                if (hit >= 0) {
                    raise_window(hit);
                    /* Update active_win after raise */
                    int new_active = -1;
                    for (int i = g_num_windows - 1; i >= 0; i--) {
                        if (g_windows[i].visible) { new_active = i; break; }
                    }
                    g_active_win = new_active;

                    if (hit_close(&g_windows[g_num_windows - 1], g_cursor_x, g_cursor_y)) {
                        g_windows[g_num_windows - 1].visible = 0;
                    } else if (hit_title_bar(&g_windows[g_num_windows - 1], g_cursor_x, g_cursor_y)) {
                        g_drag_win = g_num_windows - 1;
                        g_drag_ox = g_cursor_x - g_windows[g_num_windows - 1].x;
                        g_drag_oy = g_cursor_y - g_windows[g_num_windows - 1].y;
                    }
                }
            }
        }
    }

    zf_destroy_buffer(&g_buf);
    return 0;
}
