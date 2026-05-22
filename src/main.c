#include <zirvflux.h>
#include <unistd.h>
#include <stdio.h>
#include <datetime.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

typedef zf_mouse_event_t mouse_event_t;

#define PANEL_H    36
#define MAX_W      1920
#define MAX_H      1080
#define FONT_W     8
#define FONT_H     13
#define MENU_W     160
#define MENU_ITEM_H 28
#define MAX_MENU_ITEMS 8
#define BTN_W      120
#define BTN_H      40

static uint8_t compositor_fb[MAX_W * MAX_H * 4];
static zf_buffer_t g_buf;
static zf_display_info_t g_info;

static int cursor_x = 0, cursor_y = 0;
static int prev_buttons = 0;
static int g_dirty = 1;

/* ── Settings state ──────────────────────────────────────────────────────── */
static int g_font_scale = 1;
static int g_frame_ms = 16;

/* ── Window dragging state ──────────────────────────────────────────────── */
static int g_win_x = 40;
static int g_win_y = 56;
static int g_dragging = 0;
static int g_drag_off_x = 0;
static int g_drag_off_y = 0;

/* ── 8x13 bitmap font ──────────────────────────────────────────────────── */
static const uint8_t font_8x13[95][FONT_H] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    {0x00,0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x24,0x24,0x7E,0x24,0x24,0x7E,0x24,0x24,0x00,0x00,0x00},
    {0x00,0x08,0x3E,0x49,0x48,0x3E,0x09,0x49,0x3E,0x08,0x00,0x00,0x00},
    {0x00,0x61,0x92,0x64,0x08,0x10,0x26,0x49,0x86,0x00,0x00,0x00,0x00},
    {0x00,0x18,0x24,0x24,0x18,0x28,0x44,0x44,0x38,0x00,0x00,0x00,0x00},
    {0x00,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x04,0x08,0x10,0x10,0x10,0x10,0x10,0x08,0x04,0x00,0x00,0x00},
    {0x00,0x20,0x10,0x08,0x08,0x08,0x08,0x08,0x10,0x20,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x08,0x49,0x2A,0x1C,0x2A,0x49,0x08,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x08,0x08,0x08,0x7F,0x08,0x08,0x08,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x08,0x10,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},
    {0x00,0x02,0x04,0x04,0x08,0x08,0x10,0x10,0x20,0x20,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x46,0x4A,0x52,0x62,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x08,0x18,0x28,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x02,0x04,0x08,0x10,0x20,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x02,0x1C,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x04,0x0C,0x14,0x24,0x44,0x7E,0x04,0x04,0x00,0x00,0x00,0x00},
    {0x00,0x7E,0x40,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x1C,0x20,0x40,0x7C,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x42,0x3C,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x42,0x42,0x3E,0x02,0x04,0x38,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x08,0x10,0x00},
    {0x00,0x00,0x04,0x08,0x10,0x20,0x10,0x08,0x04,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x02,0x04,0x08,0x08,0x00,0x08,0x08,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x42,0x4E,0x52,0x4E,0x40,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x7C,0x42,0x42,0x7C,0x42,0x42,0x42,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x40,0x40,0x40,0x40,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x78,0x44,0x42,0x42,0x42,0x42,0x44,0x78,0x00,0x00,0x00,0x00},
    {0x00,0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x40,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x40,0x40,0x4E,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00},
    {0x00,0x1F,0x04,0x04,0x04,0x04,0x04,0x44,0x38,0x00,0x00,0x00,0x00},
    {0x00,0x42,0x44,0x48,0x50,0x70,0x48,0x44,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x42,0x66,0x5A,0x5A,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x7C,0x42,0x42,0x42,0x7C,0x40,0x40,0x40,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00,0x00,0x00,0x00},
    {0x00,0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x3C,0x42,0x40,0x3C,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00,0x00},
    {0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x42,0x42,0x42,0x42,0x5A,0x5A,0x66,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x42,0x42,0x24,0x18,0x18,0x24,0x42,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x41,0x22,0x14,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00,0x00},
    {0x00,0x7E,0x02,0x04,0x08,0x10,0x20,0x40,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x1E,0x10,0x10,0x10,0x10,0x10,0x10,0x1E,0x00,0x00,0x00,0x00},
    {0x00,0x20,0x10,0x10,0x08,0x08,0x04,0x04,0x02,0x02,0x00,0x00,0x00},
    {0x00,0x78,0x08,0x08,0x08,0x08,0x08,0x08,0x78,0x00,0x00,0x00,0x00},
    {0x00,0x08,0x14,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x10,0x08,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x3C,0x02,0x3E,0x42,0x42,0x3E,0x00,0x00,0x00,0x00},
    {0x00,0x40,0x40,0x5C,0x62,0x42,0x42,0x62,0x5C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x3C,0x42,0x40,0x40,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x02,0x02,0x3E,0x42,0x42,0x42,0x46,0x3A,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x3C,0x42,0x7E,0x40,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x0C,0x12,0x10,0x7C,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x3E,0x42,0x42,0x46,0x3A,0x02,0x42,0x3C,0x00,0x00},
    {0x00,0x40,0x40,0x5C,0x62,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x08,0x00,0x38,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00},
    {0x00,0x04,0x00,0x1C,0x04,0x04,0x04,0x04,0x44,0x44,0x38,0x00,0x00},
    {0x00,0x40,0x40,0x44,0x48,0x50,0x70,0x48,0x44,0x00,0x00,0x00,0x00},
    {0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x76,0x49,0x49,0x49,0x49,0x49,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x5C,0x62,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x5C,0x62,0x42,0x62,0x5C,0x40,0x40,0x40,0x00,0x00},
    {0x00,0x00,0x00,0x3A,0x46,0x42,0x42,0x46,0x3A,0x02,0x02,0x00,0x00},
    {0x00,0x00,0x00,0x5C,0x62,0x40,0x40,0x40,0x40,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x3E,0x40,0x3C,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x10,0x10,0x7E,0x10,0x10,0x10,0x12,0x0C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x46,0x3A,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x42,0x42,0x42,0x24,0x24,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x41,0x49,0x49,0x49,0x49,0x36,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x42,0x24,0x18,0x18,0x24,0x42,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x42,0x42,0x42,0x46,0x3A,0x02,0x42,0x3C,0x00,0x00},
    {0x00,0x00,0x00,0x7E,0x04,0x08,0x10,0x20,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x06,0x08,0x08,0x08,0x30,0x08,0x08,0x08,0x06,0x00,0x00,0x00},
    {0x00,0x08,0x08,0x08,0x08,0x00,0x08,0x08,0x08,0x08,0x00,0x00,0x00},
    {0x00,0x60,0x10,0x10,0x10,0x0C,0x10,0x10,0x10,0x60,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x20,0x54,0x08,0x00,0x00,0x00,0x00,0x00},
};

static const uint8_t *font_get(char c)
{
    if (c >= 32 && c <= 126) return font_8x13[c - 32];
    return font_8x13[0];
}

/* ── Drawing helpers ────────────────────────────────────────────────────── */
static uint32_t blend(uint32_t fg, uint32_t bg, uint8_t alpha)
{
    uint8_t fr = (fg >> 16) & 0xFF, fg_ = (fg >> 8) & 0xFF, fb = fg & 0xFF;
    uint8_t br = (bg >> 16) & 0xFF, bg_ = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    uint8_t r = (uint8_t)(((uint32_t)fr * alpha + (uint32_t)br * (255 - alpha)) / 255);
    uint8_t g = (uint8_t)(((uint32_t)fg_ * alpha + (uint32_t)bg_ * (255 - alpha)) / 255);
    uint8_t b = (uint8_t)(((uint32_t)fb * alpha + (uint32_t)bb * (255 - alpha)) / 255);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void fill_rect(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x, int y, int w, int h, uint32_t color)
{
    for (int row = 0; row < h; row++) {
        int py = y + row;
        if (py < 0 || py >= (int)fb_h) continue;
        for (int col = 0; col < w; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)fb_w) continue;
            fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
        }
    }
}

static void draw_char(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x, int y, char c, uint32_t color)
{
    const uint8_t *bm = font_get(c);
    for (int row = 0; row < FONT_H && (y + row) < (int)fb_h; row++) {
        uint8_t bits = bm[row];
        if (bits == 0) continue;
        for (int col = 0; col < FONT_W && (x + col) < (int)fb_w; col++) {
            if (bits & (1 << (7 - col))) {
                int px = x + col;
                int py = y + row;
                if (px < 0 || py < 0) continue;
                uint32_t off = (uint32_t)py * fb_w + (uint32_t)px;
                /* Check if edge pixel (any of 4 neighbors is 0) for AA */
                int tl = (col > 0 && (bits & (1 << (7 - col + 1)))) ? 1 : 0;
                int tr = (col < FONT_W - 1 && (bits & (1 << (7 - col - 1)))) ? 1 : 0;
                int tu = (row > 0 && (bm[row - 1] & (1 << (7 - col)))) ? 1 : 0;
                int td = (row < FONT_H - 1 && (bm[row + 1] & (1 << (7 - col)))) ? 1 : 0;
                if (tl && tr && tu && td)
                    fb[off] = color;
                else
                    fb[off] = blend(color, fb[off], 180);
            }
        }
    }
}

/* Scaled variant — each font pixel becomes a scale×scale block */
static void draw_char_scaled(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                             int x, int y, char c, uint32_t color)
{
    const uint8_t *bm = font_get(c);
    for (int row = 0; row < FONT_H && (y + row * g_font_scale) < (int)fb_h; row++) {
        uint8_t bits = bm[row];
        if (bits == 0) continue;
        for (int col = 0; col < FONT_W && (x + col * g_font_scale) < (int)fb_w; col++) {
            if (bits & (1 << (7 - col))) {
                for (int dy = 0; dy < g_font_scale; dy++) {
                    for (int dx = 0; dx < g_font_scale; dx++) {
                        int px = x + col * g_font_scale + dx;
                        int py = y + row * g_font_scale + dy;
                        if (px >= 0 && py >= 0 && px < (int)fb_w && py < (int)fb_h)
                            fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
                    }
                }
            }
        }
    }
}

static void draw_text(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x, int y, const char *s, uint32_t color)
{
    int step = (FONT_W + 1) * g_font_scale;
    while (*s) {
        if (g_font_scale > 1)
            draw_char_scaled(fb, fb_w, fb_h, x, y, *s, color);
        else
            draw_char(fb, fb_w, fb_h, x, y, *s, color);
        x += step;
        s++;
    }
}

static void draw_clock(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                       int x, int y, uint32_t color)
{
    struct datetime dt;
    if (getdatetime(&dt) < 0) {
        draw_char(fb, fb_w, fb_h, x, y, '-', color);
        return;
    }
    char buf[6];
    buf[0] = (char)('0' + dt.hour / 10);
    buf[1] = (char)('0' + dt.hour % 10);
    buf[2] = ':';
    buf[3] = (char)('0' + dt.minute / 10);
    buf[4] = (char)('0' + dt.minute % 10);
    buf[5] = '\0';
    draw_text(fb, fb_w, fb_h, x, y, buf, color);
}

static void draw_shadow(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                        int x, int y, int w, int h, int radius)
{
    uint32_t c = 0x00000000u;
    for (int r = 1; r <= radius; r++) {
        int alpha = 32 - (r * 32) / (radius + 1);
        if (alpha <= 0) continue;
        c = (uint32_t)(alpha & 0xFF) << 24;
        for (int row = -r; row < h + r; row++) {
            int py = y + row;
            if (py < 0 || py >= (int)fb_h) continue;
            for (int col = -r; col < w + r; col++) {
                int px = x + col;
                if (px < 0 || px >= (int)fb_w) continue;
                if (row >= 0 && row < h && col >= 0 && col < w) continue;
                uint32_t off = (uint32_t)py * fb_w + (uint32_t)px;
                fb[off] = blend(c, fb[off], (uint8_t)alpha);
            }
        }
    }
}

/* ── Menu / App system ──────────────────────────────────────────────────── */
enum {
    APP_TERMINAL,
    APP_CALCULATOR,
    APP_ABOUT,
    APP_FILES,
    APP_SETTINGS,
    NUM_APPS,
};

static const char *g_app_names[NUM_APPS] = {
    "Terminal", "Calculator", "About", "Files", "Settings",
};

static int g_menu_open = 0;
static int g_menu_hover = -1;
static int g_active_app = -1;
static int g_close_hover = 0;
static int g_menu_btn_hover = 0;

static int g_launcher_hover = -1;
static int g_settings_hover = -1;

#define MENU_BTN_X 8
#define MENU_BTN_W 80
#define MENU_BTN_H PANEL_H
#define MENU_X 0
#define MENU_TOP (PANEL_H)

static int point_in(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

/* ── App renderers ──────────────────────────────────────────────────────── */
static void draw_terminal(uint32_t *fb, uint32_t w, uint32_t h,
                          int x, int y, int cw, int ch)
{
    fill_rect(fb, w, h, x, y, cw, ch, 0xFF1A1A2Eu);
    draw_text(fb, w, h, x + 10, y + 10, "Zirvium Terminal v0.1", 0xFF00FF00u);
    draw_text(fb, w, h, x + 10, y + 30, "> Welcome to Zirvium OS", 0xFF00CC00u);
    draw_text(fb, w, h, x + 10, y + 50, "> Type 'help' for commands", 0xFF00AA00u);
    draw_text(fb, w, h, x + 10, y + 80, ">", 0xFF00FF00u);
}

static void draw_calculator(uint32_t *fb, uint32_t w, uint32_t h,
                            int x, int y, int cw, int ch)
{
    fill_rect(fb, w, h, x, y, cw, ch, 0xFFF0F0F0u);
    fill_rect(fb, w, h, x + 10, y + 10, cw - 20, 30, 0xFFFFFFFFu);
    draw_text(fb, w, h, x + cw - 30, y + 14, "0", 0xFF000000u);
    const char *keys = "789+456-123*0.=/";
    int kw = 30, kh = 26, gap = 4;
    int sx = x + 10, sy = y + 50;
    for (int i = 0; i < 16; i++) {
        int kx = sx + (i % 4) * (kw + gap);
        int ky = sy + (i / 4) * (kh + gap);
        uint32_t kc = (keys[i] >= '0' && keys[i] <= '9') ? 0xFFDDDDDDu : 0xFFFF9933u;
        fill_rect(fb, w, h, kx, ky, kw, kh, kc);
        char lbl[2] = {keys[i], 0};
        draw_text(fb, w, h, kx + 10, ky + 7, lbl, 0xFF000000u);
    }
}

static void draw_about(uint32_t *fb, uint32_t w, uint32_t h,
                       int x, int y, int cw, int ch)
{
    fill_rect(fb, w, h, x, y, cw, ch, 0xFF1A1A2Eu);
    draw_text(fb, w, h, x + 20, y + 20, "Zirvium OS", 0xFFFFFFFFu);
    draw_text(fb, w, h, x + 20, y + 40, "Version 0.1", 0xFFAAAAAAu);
    draw_text(fb, w, h, x + 20, y + 60, "DisplayJet compositor", 0xFF8888FFu);
    draw_text(fb, w, h, x + 20, y + 80, "MAEM encryption active", 0xFF88FF88u);
    draw_text(fb, w, h, x + 20, y + 100, "Booted via GRUB multiboot2", 0xFFFFCC88u);
    char res[32];
    int rl = 0;
    uint32_t rw = g_info.width, rh = g_info.height;
    if (rw >= 100) { res[rl++] = (char)('0' + rw / 100); rw %= 100; }
    res[rl++] = (char)('0' + rw / 10); rw %= 10;
    res[rl++] = (char)('0' + rw); res[rl++] = 'x';
    if (rh >= 100) { res[rl++] = (char)('0' + rh / 100); rh %= 100; }
    res[rl++] = (char)('0' + rh / 10); rh %= 10;
    res[rl++] = (char)('0' + rh); res[rl] = '\0';
    draw_text(fb, w, h, x + 20, y + 130, "Resolution: ", 0xFFCCCCCCu);
    draw_text(fb, w, h, x + 110, y + 130, res, 0xFFFFCC88u);
}

static void draw_files(uint32_t *fb, uint32_t w, uint32_t h,
                       int x, int y, int cw, int ch)
{
    fill_rect(fb, w, h, x, y, cw, ch, 0xFF1E1E2Eu);
    draw_text(fb, w, h, x + 10, y + 10, "File Browser", 0xFFAAAAFFu);
    draw_text(fb, w, h, x + 10, y + 35, "[DIR]  ..", 0xFF88CCFFu);
    draw_text(fb, w, h, x + 10, y + 55, "[DIR]  bin", 0xFF88CCFFu);
    draw_text(fb, w, h, x + 10, y + 75, "[DIR]  etc", 0xFF88CCFFu);
    draw_text(fb, w, h, x + 10, y + 95, "[DIR]  home", 0xFF88CCFFu);
    draw_text(fb, w, h, x + 10, y + 115, "[DIR]  usr", 0xFF88CCFFu);
    draw_text(fb, w, h, x + 10, y + 135, "[FILE] README.md", 0xFFCCCCCCu);
    draw_text(fb, w, h, x + 10, y + 155, "[FILE] init.bin", 0xFFCCCCCCu);
}

static void draw_settings(uint32_t *fb, uint32_t w, uint32_t h,
                          int x, int y, int cw, int ch)
{
    fill_rect(fb, w, h, x, y, cw, ch, 0xFF2A2A3Eu);
    draw_text(fb, w, h, x + 20, y + 20, "Display Settings", 0xFFFFFFFFu);

    /* Option row: Font size */
    const int row_h = 28;
    int r0_y = y + 50;
    uint32_t r0c = (g_settings_hover == 0) ? 0xFF3A3A5Eu : 0xFF2A2A3Eu;
    fill_rect(fb, w, h, x + 4, r0_y, cw - 8, row_h, r0c);
    draw_text(fb, w, h, x + 20, r0_y + 6, "Font size:", 0xFFCCCCCCu);
    draw_text(fb, w, h, x + cw - 100, r0_y + 6,
              g_font_scale > 1 ? "Large" : "Small", 0xFFAAAAFFu);

    /* Option row: Frame rate */
    int r1_y = y + 50 + row_h + 4;
    uint32_t r1c = (g_settings_hover == 1) ? 0xFF3A3A5Eu : 0xFF2A2A3Eu;
    fill_rect(fb, w, h, x + 4, r1_y, cw - 8, row_h, r1c);
    draw_text(fb, w, h, x + 20, r1_y + 6, "Frame rate:", 0xFFCCCCCCu);
    draw_text(fb, w, h, x + cw - 100, r1_y + 6,
              g_frame_ms <= 16 ? "60 FPS" : "30 FPS", 0xFFAAAAFFu);

    /* Hint */
    draw_text(fb, w, h, x + 20, y + ch - 30,
              "Click an option to toggle", 0xFF666688u);
}

static void (*g_app_draw[NUM_APPS])(uint32_t *, uint32_t, uint32_t,
                                     int, int, int, int) = {
    draw_terminal, draw_calculator, draw_about,
    draw_files, draw_settings,
};

/* ── Render one frame ───────────────────────────────────────────────────── */
static void render_frame(void)
{
    uint32_t w = g_info.width;
    uint32_t h = g_info.height;
    uint32_t *fb32 = (uint32_t *)compositor_fb;

    /* Gradient background */
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint32_t off = y * w + x;
            uint8_t r, g, b;
            r = (uint8_t)(((x * 35) / w + 18) & 0xFF);
            g = (uint8_t)((12 + (y * 28) / h) & 0xFF);
            b = (uint8_t)((28 + ((w - x) * 45) / w) & 0xFF);
            fb32[off] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }

    /* ── Panel shadow ───────────────────────────────────────────────────── */
    draw_shadow(fb32, w, h, 0, PANEL_H, (int)w, 6, 4);

    /* ── Top panel bar ──────────────────────────────────────────────────── */
    fill_rect(fb32, w, h, 0, 0, w, PANEL_H, 0xFF121220u);

    /* Panel accent line at bottom */
    for (uint32_t lx = 0; lx < w; lx++)
        fb32[(uint32_t)(PANEL_H - 1) * w + lx] = 0xFF333355u;

    /* Menu button */
    uint32_t menu_col = g_menu_btn_hover ? 0xFF2A2A50u : 0xFF181830u;
    fill_rect(fb32, w, h, MENU_BTN_X, 0, MENU_BTN_W, MENU_BTN_H, menu_col);
    draw_text(fb32, w, h, MENU_BTN_X + 10, (PANEL_H - FONT_H) / 2,
              g_menu_open ? "Zirvium ^" : "Zirvium v", 0xFFCCCCDDu);

    /* Separator after menu button */
    for (int ly = 4; ly < PANEL_H - 4; ly++)
        fb32[(uint32_t)ly * w + (uint32_t)(MENU_BTN_X + MENU_BTN_W)] = 0xFF333355u;

    /* Active app name in center */
    if (g_active_app >= 0) {
        const char *name = g_app_names[g_active_app];
        int len = 0;
        while (name[len]) len++;
        int tx = ((int)w - len * (FONT_W + 1)) / 2;
        draw_text(fb32, w, h, tx, (PANEL_H - FONT_H) / 2, name, 0xFFCCCCDDu);
    }

    /* Close button in panel */
    if (g_active_app >= 0) {
        int cx = (int)w - 30;
        int cy = (PANEL_H - 18) / 2;
        uint32_t cc = g_close_hover ? 0xFFFF5555u : 0xFF882222u;
        fill_rect(fb32, w, h, cx, cy, 18, 18, cc);
        draw_char(fb32, w, h, cx + 5, cy + 3, 'x', 0xFFFFFFFFu);
    }

    /* Clock */
    int clock_x = (int)w - 12 - (5 * (FONT_W + 1));
    draw_clock(fb32, w, h, clock_x, (PANEL_H - FONT_H) / 2, 0xFF9999AAu);

    /* ── Desktop launcher grid ──────────────────────────────────────────── */
    if (g_active_app < 0 && !g_menu_open) {
        int cols = 4, gap = 20;
        int total_w = cols * BTN_W + (cols - 1) * gap;
        int sx = ((int)w - total_w) / 2;
        int sy = (int)(PANEL_H + 100);

        for (int i = 0; i < NUM_APPS; i++) {
            int bx = sx + (i % cols) * (BTN_W + gap);
            int by = sy + (i / cols) * (BTN_H + gap);
            uint32_t bg = (i == g_launcher_hover) ? 0xFF353560u : 0xFF1E1E38u;
            fill_rect(fb32, w, h, bx + 2, by + 2, BTN_W, BTN_H, 0x20000000u);
            fill_rect(fb32, w, h, bx, by, BTN_W, BTN_H, bg);
            for (int l = 0; l < BTN_W; l++) {
                fb32[(uint32_t)by * w + (uint32_t)(bx + l)] = 0xFF666688u;
                fb32[(uint32_t)(by + BTN_H - 1) * w + (uint32_t)(bx + l)] = 0xFF333355u;
            }
            for (int l = 0; l < BTN_H; l++) {
                fb32[(uint32_t)(by + l) * w + (uint32_t)bx] = 0xFF555577u;
                fb32[(uint32_t)(by + l) * w + (uint32_t)(bx + BTN_W - 1)] = 0xFF444466u;
            }
            int len = 0;
            while (g_app_names[i][len]) len++;
            int tx = bx + (BTN_W - len * (FONT_W + 1)) / 2;
            int ty = by + (BTN_H - FONT_H) / 2;
            draw_text(fb32, w, h, tx, ty, g_app_names[i], 0xFFCCCCDDu);
        }
    }

    /* ── Start menu dropdown ────────────────────────────────────────────── */
    if (g_menu_open) {
        int my = MENU_TOP + 2;
        int mh = NUM_APPS * MENU_ITEM_H + 40;
        draw_shadow(fb32, w, h, MENU_X + 4, my, MENU_W, mh, 6);
        fill_rect(fb32, w, h, MENU_X + 4, my, MENU_W, mh, 0xFF181838u);
        for (int l = 0; l < MENU_W; l++) {
            fb32[(uint32_t)my * w + (uint32_t)(MENU_X + 4 + l)] = 0xFF444466u;
            fb32[(uint32_t)(my + mh - 1) * w + (uint32_t)(MENU_X + 4 + l)] = 0xFF222244u;
        }
        for (int l = 0; l < mh; l++) {
            fb32[(uint32_t)(my + l) * w + (uint32_t)(MENU_X + 4)] = 0xFF444466u;
            fb32[(uint32_t)(my + l) * w + (uint32_t)(MENU_X + 4 + MENU_W - 1)] = 0xFF222244u;
        }

        for (int i = 0; i < NUM_APPS; i++) {
            int iy = my + 4 + i * MENU_ITEM_H;
            if (i == g_menu_hover) {
                fill_rect(fb32, w, h, MENU_X + 6, iy, MENU_W - 4, MENU_ITEM_H, 0xFF2A2A58u);
                uint32_t hilite = 0xFF555588u;
                for (int l = 0; l < MENU_W - 4; l++)
                    fb32[(uint32_t)iy * w + (uint32_t)(MENU_X + 6 + l)] = hilite;
            }
            draw_text(fb32, w, h, MENU_X + 16, iy + (MENU_ITEM_H - FONT_H) / 2,
                      g_app_names[i], i == g_menu_hover ? 0xFFFFFFDDu : 0xFFBBBBCCu);
        }
        int sep_y = my + 4 + NUM_APPS * MENU_ITEM_H;
        fill_rect(fb32, w, h, MENU_X + 10, sep_y, MENU_W - 12, 1, 0xFF444466u);
        int rby = sep_y + 6;
        draw_text(fb32, w, h, MENU_X + 16, rby + (MENU_ITEM_H - FONT_H) / 2,
                  "Reboot", 0xFFFF7766u);
    }

    /* ── App content window ─────────────────────────────────────────────── */
    if (g_active_app >= 0) {
        int win_x = g_win_x, win_y = g_win_y;
        int win_w = (int)w - 80, win_h = (int)h - (int)PANEL_H - 64;

        /* Window shadow */
        draw_shadow(fb32, w, h, win_x - 4, win_y - 24, win_w + 8, win_h + 30, 8);

        /* Title bar */
        fill_rect(fb32, w, h, win_x, win_y - 24, win_w, 24, 0xFF1A1A3Au);
        for (int l = 0; l < win_w; l++) {
            fb32[(uint32_t)(win_y - 24) * w + (uint32_t)(win_x + l)] = 0xFF444477u;
            fb32[(uint32_t)(win_y - 1) * w + (uint32_t)(win_x + l)] = 0xFF333366u;
        }

        /* Content area */
        fill_rect(fb32, w, h, win_x, win_y, win_w, win_h, 0xFF161630u);

        /* Window border */
        for (int l = 0; l < win_w; l++) {
            fb32[(uint32_t)(win_y) * w + (uint32_t)(win_x + l)] = 0xFF444477u;
            fb32[(uint32_t)(win_y + win_h - 1) * w + (uint32_t)(win_x + l)] = 0xFF333366u;
        }
        for (int l = 0; l < win_h; l++) {
            fb32[(uint32_t)(win_y + l) * w + (uint32_t)win_x] = 0xFF444477u;
            fb32[(uint32_t)(win_y + l) * w + (uint32_t)(win_x + win_w - 1)] = 0xFF333366u;
        }

        /* Title text */
        draw_text(fb32, w, h, win_x + 10, win_y - 20,
                  g_app_names[g_active_app], 0xFFAAAAEEu);

        /* Window close button */
        int cx2 = win_x + win_w - 24, cy2 = win_y - 22;
        fill_rect(fb32, w, h, cx2, cy2, 18, 18, 0xFF992222u);
        draw_char(fb32, w, h, cx2 + 5, cy2 + 3, 'x', 0xFFFFFFFFu);

        g_app_draw[g_active_app](fb32, w, h, win_x + 4, win_y + 4,
                                  win_w - 8, win_h - 8);
    }
}

/* ── Process mouse events ───────────────────────────────────────────────── */
static void process_mouse(void)
{
    int prev_mh = g_menu_hover;
    int prev_mo = g_menu_open;
    int prev_aa = g_active_app;
    int prev_ch = g_close_hover;
    int prev_lh = g_launcher_hover;
    int prev_sh = g_settings_hover;
    int prev_mb = g_menu_btn_hover;
    int prev_dr = g_dragging;

    mouse_event_t ev;
    while (zf_read_mouse(&ev) == 0) {
        int nx = cursor_x + ev.dx;
        int ny = cursor_y + ev.dy;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx >= (int)g_info.width)  nx = (int)g_info.width - 1;
        if (ny >= (int)g_info.height) ny = (int)g_info.height - 1;
        cursor_x = nx;
        cursor_y = ny;
        zf_set_cursor(cursor_x, cursor_y);

        int left_down  = (ev.buttons & 1) && !(prev_buttons & 1);
        int left_up    = !(ev.buttons & 1) && (prev_buttons & 1);
        prev_buttons = ev.buttons;

        uint32_t ww = g_info.width;
        int win_w = (int)ww - 80;

        /* ── Window dragging ─────────────────────────────────────────── */
        if (g_dragging) {
            int new_x = nx - g_drag_off_x;
            int new_y = ny - g_drag_off_y;
            if (new_x < 2) new_x = 2;
            if (new_x + win_w > (int)ww - 2) new_x = (int)ww - win_w - 2;
            if (new_y < PANEL_H + 2) new_y = PANEL_H + 2;
            if (new_y > (int)g_info.height - 60) new_y = (int)g_info.height - 60;
            if (new_x != g_win_x || new_y != g_win_y) {
                g_win_x = new_x;
                g_win_y = new_y;
                g_dirty = 1;
            }
            if (left_up) { g_dragging = 0; g_dirty = 1; }
            continue;
        }

        /* ── Hover tracking ──────────────────────────────────────────── */
        g_menu_btn_hover = point_in(nx, ny, MENU_BTN_X, 0, MENU_BTN_W, MENU_BTN_H);

        int close_x = (int)ww - 30;
        int close_y = (PANEL_H - 18) / 2;
        g_close_hover = g_active_app >= 0 &&
                        point_in(nx, ny, close_x, close_y, 18, 18);

        g_menu_hover = -1;
        if (g_menu_open) {
            int my = MENU_TOP + 2;
            for (int i = 0; i < NUM_APPS; i++) {
                int iy = my + 4 + i * MENU_ITEM_H;
                if (point_in(nx, ny, MENU_X + 6, iy, MENU_W - 4, MENU_ITEM_H)) {
                    g_menu_hover = i;
                    break;
                }
            }
        }

        g_settings_hover = -1;
        if (g_active_app == APP_SETTINGS && !g_menu_open) {
            int win_w = (int)ww - 80;
            int sx = g_win_x + 4, sy = g_win_y + 50;
            int row_h = 28, cw = win_w - 8;
            for (int i = 0; i < 2; i++) {
                if (point_in(nx, ny, sx, sy + i * (row_h + 4), cw, row_h)) {
                    g_settings_hover = i;
                    break;
                }
            }
        }

        g_launcher_hover = -1;
        if (g_active_app < 0 && !g_menu_open) {
            int cols = 4, gap = 20;
            int total_w = cols * BTN_W + (cols - 1) * gap;
            int sx = ((int)ww - total_w) / 2;
            int sy = (int)(PANEL_H + 80);
            for (int i = 0; i < NUM_APPS; i++) {
                int bx = sx + (i % cols) * (BTN_W + gap);
                int by = sy + (i / cols) * (BTN_H + gap);
                if (point_in(nx, ny, bx, by, BTN_W, BTN_H)) {
                    g_launcher_hover = i;
                    break;
                }
            }
        }

        /* ── Clicks ──────────────────────────────────────────────────── */
        if (left_down) {
            if (g_menu_btn_hover) {
                g_menu_open = !g_menu_open;
                g_dirty = 1;
            } else if (g_menu_open) {
                if (g_menu_hover >= 0) {
                    g_active_app = g_menu_hover;
                    g_win_x = 40;
                    g_win_y = PANEL_H + 24;
                    g_menu_open = 0;
                    g_dirty = 1;
                } else {
                    int my = MENU_TOP + 2;
                    int sep_y = my + 4 + NUM_APPS * MENU_ITEM_H;
                    if (point_in(nx, ny, MENU_X + 10, sep_y + 6, MENU_W - 20, MENU_ITEM_H))
                        zf_reboot();
                    else { g_menu_open = 0; g_dirty = 1; }
                }
            } else if (g_close_hover) {
                g_active_app = -1;
                g_dirty = 1;
            } else if (g_launcher_hover >= 0) {
                g_active_app = g_launcher_hover;
                g_win_x = 40;
                g_win_y = PANEL_H + 24;
                g_dirty = 1;
            } else if (g_active_app == APP_SETTINGS && g_settings_hover >= 0) {
                if (g_settings_hover == 0) {
                    g_font_scale = (g_font_scale > 1) ? 1 : 2;
                } else if (g_settings_hover == 1) {
                    g_frame_ms = (g_frame_ms <= 16) ? 33 : 16;
                }
                g_dirty = 1;
            } else if (g_active_app >= 0) {
                /* Check window title bar click → start drag */
                int wcx = g_win_x + win_w - 24;
                int wcy = g_win_y - 22;
                if (point_in(nx, ny, wcx, wcy, 18, 18)) {
                    g_active_app = -1;
                    g_dirty = 1;
                } else if (point_in(nx, ny, g_win_x, g_win_y - 24, win_w, 24)) {
                    g_dragging = 1;
                    g_drag_off_x = nx - g_win_x;
                    g_drag_off_y = ny - g_win_y;
                    g_dirty = 1;
                }
            }
        }
    }

    /* Re-render if any visual state changed */
    if (g_menu_hover != prev_mh || g_menu_open != prev_mo ||
        g_active_app != prev_aa || g_close_hover != prev_ch ||
        g_launcher_hover != prev_lh || g_settings_hover != prev_sh ||
        g_menu_btn_hover != prev_mb || g_dragging != prev_dr)
        g_dirty = 1;
}

/* ── Entry point ────────────────────────────────────────────────────────── */
int main(void)
{
    if (zf_connect() != 0) return 1;
    if (zf_get_info(&g_info) != 0 || !g_info.connected) return 1;
    if (zf_create_buffer(g_info.width, g_info.height, &g_buf) != 0) return 1;
    if (g_info.width > MAX_W || g_info.height > MAX_H) return 1;

    cursor_x = (int)g_info.width / 2;
    cursor_y = (int)g_info.height / 2;
    zf_set_cursor(cursor_x, cursor_y);

    size_t fb_size = (size_t)g_buf.stride * g_buf.height;
    render_frame();

    if (zf_write_buffer(&g_buf, compositor_fb, fb_size) < 0) return 1;
    if (zf_present(&g_buf) != 0) return 1;

    zf_suppress_dbg();

    struct datetime dt0;
    uint32_t last_minute = 0xFFFFFFFF;
    if (getdatetime(&dt0) == 0) last_minute = dt0.minute;

    for (;;) {
        process_mouse();

        struct datetime dt;
        if (getdatetime(&dt) == 0 && dt.minute != last_minute) {
            last_minute = dt.minute;
            g_dirty = 1;
        }

        if (g_dirty) {
            render_frame();
            zf_write_buffer(&g_buf, compositor_fb, fb_size);
            g_dirty = 0;
        }

        if (zf_present(&g_buf) != 0) return 1;
        msleep((uint32_t)g_frame_ms);
    }
}
