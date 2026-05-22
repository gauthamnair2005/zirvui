#include <zirvflux.h>
#include <unistd.h>
#include <stdio.h>
#include <datetime.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

typedef zf_mouse_event_t mouse_event_t;

#define PANEL_H    36
#define TASKBAR_H  42
#define MAX_W      1920
#define MAX_H      1080
#define FONT_W     8
#define FONT_H     13
#define MENU_W     180
#define MENU_ITEM_H 28
#define MAX_MENU_ITEMS 8
#define BTN_W      120
#define BTN_H      40

/* XP Blue colour scheme */
#define XP_TASKBAR_TOP      0xFF3366CCu
#define XP_TASKBAR_BOTTOM   0xFF0A246Au
#define XP_START_GREEN      0xFF3C9B3Cu
#define XP_START_HOVER      0xFF5BBF5Bu
#define XP_START_GREEN2     0xFF2E7D2Eu
#define XP_TITLE_TOP        0xFF0A5ADFu
#define XP_TITLE_BOTTOM     0xFF083E8Cu
#define XP_TITLE_INACT_TOP  0xFF7B97C5u
#define XP_TITLE_INACT_BOT  0xFF4B6A96u
#define XP_CLOSE_BG         0xFFE06B5Cu
#define XP_CLOSE_HOVER      0xFFFF8C7Cu
#define XP_CLOSE_X          0xFFFFFFFFu
#define XP_DESKTOP_TOP      0xFF6FB5E8u
#define XP_DESKTOP_BOT      0xFF5A9A3Au
#define XP_WIN_BG           0xFFECE9D8u
#define XP_WIN_TEXT         0xFF000000u
#define XP_TASKBAR_TEXT     0xFFFFFFFFu
#define XP_TITLE_TEXT       0xFFFFFFFFu
#define XP_BTN_BORDER       0xFFD4D0C8u

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
static int g_win_y = 80;
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
static int g_peek_hover = 0;
static int g_showing_desktop = 0;

#define MENU_BTN_X 8
#define MENU_BTN_W 80
#define MENU_BTN_H PANEL_H
#define MENU_X 0
#define MENU_TOP (PANEL_H)
/* MENU_X and MENU_TOP kept for reference; XP-style uses dynamic coords */

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
    (void)w; (void)h;
    fill_rect(fb, w, h, x, y, cw, ch, XP_WIN_BG);
    draw_text(fb, w, h, x + 10, y + 10, "7  8  9  /", 0xFF333333u);
    draw_text(fb, w, h, x + 10, y + 30, "4  5  6  *", 0xFF333333u);
    draw_text(fb, w, h, x + 10, y + 50, "1  2  3  -", 0xFF333333u);
    draw_text(fb, w, h, x + 10, y + 70, "0  .  =  +", 0xFF333333u);
    draw_text(fb, w, h, x + 10, y + 100, "__________", 0xFF999999u);
    draw_text(fb, w, h, x + 10, y + 110, "> 0", 0xFF000000u);
}

static void draw_about(uint32_t *fb, uint32_t w, uint32_t h,
                       int x, int y, int cw, int ch)
{
    (void)w; (void)h;
    fill_rect(fb, w, h, x, y, cw, ch, XP_WIN_BG);
    draw_text(fb, w, h, x + 20, y + 20, "Zirvium OS v0.1", 0xFF000000u);
    draw_text(fb, w, h, x + 20, y + 50, "DisplayJet compositor", 0xFF333333u);
    draw_text(fb, w, h, x + 20, y + 70, "8x13 bitmap font", 0xFF333333u);
    char res[32];
    int rl = 0;
    uint32_t rw = g_info.width, rh = g_info.height;
    if (rw >= 100) { res[rl++] = (char)('0' + rw / 100); rw %= 100; }
    res[rl++] = (char)('0' + rw / 10); rw %= 10;
    res[rl++] = (char)('0' + rw); res[rl++] = 'x';
    if (rh >= 100) { res[rl++] = (char)('0' + rh / 100); rh %= 100; }
    res[rl++] = (char)('0' + rh / 10); rh %= 10;
    res[rl++] = (char)('0' + rh); res[rl] = '\0';
    draw_text(fb, w, h, x + 20, y + 90, "Resolution:", 0xFF333333u);
    draw_text(fb, w, h, x + 100, y + 90, res, 0xFF0000CCu);
    draw_text(fb, w, h, x + 20, y + 130, "ZirvTK: Rust GUI toolkit", 0xFF333333u);
}

static void draw_files(uint32_t *fb, uint32_t w, uint32_t h,
                       int x, int y, int cw, int ch)
{
    fill_rect(fb, w, h, x, y, cw, ch, XP_WIN_BG);
    draw_text(fb, w, h, x + 10, y + 10, "[DIR]  ..", 0xFF886600u);
    draw_text(fb, w, h, x + 10, y + 30, "[DIR]  usr", 0xFF886600u);
    draw_text(fb, w, h, x + 10, y + 50, "[DIR]  etc", 0xFF886600u);
    draw_text(fb, w, h, x + 10, y + 70, "[DIR]  home", 0xFF886600u);
    draw_text(fb, w, h, x + 10, y + 90, "[DIR]  mnt", 0xFF886600u);
    draw_text(fb, w, h, x + 10, y + 110, "[DIR]  tmp", 0xFF886600u);
    draw_text(fb, w, h, x + 10, y + 130, "[FILE] README.md", 0xFF333333u);
    draw_text(fb, w, h, x + 10, y + 150, "[FILE] init.bin", 0xFF333333u);
}

static void draw_settings(uint32_t *fb, uint32_t w, uint32_t h,
                          int x, int y, int cw, int ch)
{
    fill_rect(fb, w, h, x, y, cw, ch, XP_WIN_BG);
    draw_text(fb, w, h, x + 20, y + 20, "Display Settings", 0xFF000000u);

    const int row_h = 28;
    int r0_y = y + 50;
    uint32_t r0c = (g_settings_hover == 0) ? 0xFFC0D8F8u : 0xFFE8E8E0u;
    fill_rect(fb, w, h, x + 4, r0_y, cw - 8, row_h, r0c);
    draw_text(fb, w, h, x + 20, r0_y + 6, "Font size:", 0xFF000000u);
    draw_text(fb, w, h, x + cw - 100, r0_y + 6,
              g_font_scale > 1 ? "Large" : "Small", 0xFF000088u);

    int r1_y = y + 50 + row_h + 4;
    uint32_t r1c = (g_settings_hover == 1) ? 0xFFC0D8F8u : 0xFFE8E8E0u;
    fill_rect(fb, w, h, x + 4, r1_y, cw - 8, row_h, r1c);
    draw_text(fb, w, h, x + 20, r1_y + 6, "Frame rate:", 0xFF000000u);
    draw_text(fb, w, h, x + cw - 100, r1_y + 6,
              g_frame_ms <= 16 ? "60 FPS" : "30 FPS", 0xFF000088u);

    draw_text(fb, w, h, x + 20, y + ch - 30,
              "Click an option to toggle", 0xFF666666u);
}

static void (*g_app_draw[NUM_APPS])(uint32_t *, uint32_t, uint32_t,
                                     int, int, int, int) = {
    draw_terminal, draw_calculator, draw_about,
    draw_files, draw_settings,
};

/* ── Helper: draw a vertical gradient rect ────────────────────────────────── */
static void vgrad(uint32_t *fb, uint32_t w, uint32_t h,
                  int x, int y, uint32_t rw, uint32_t rh,
                  uint32_t top_col, uint32_t bot_col)
{
    uint8_t tr = (top_col >> 16) & 0xFF, tg = (top_col >> 8) & 0xFF, tb = top_col & 0xFF;
    uint8_t br = (bot_col >> 16) & 0xFF, bg = (bot_col >> 8) & 0xFF, bb = bot_col & 0xFF;
    for (uint32_t row = 0; row < rh; row++) {
        int py = y + (int)row;
        if (py < 0 || py >= (int)h) continue;
        uint8_t r = (uint8_t)(tr + ((br - tr) * row) / rh);
        uint8_t g = (uint8_t)(tg + ((bg - tg) * row) / rh);
        uint8_t b = (uint8_t)(tb + ((bb - tb) * row) / rh);
        uint32_t c = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        for (uint32_t col = 0; col < rw; col++) {
            int px = x + (int)col;
            if (px < 0 || px >= (int)w) continue;
            fb[(uint32_t)py * w + (uint32_t)px] = c;
        }
    }
}

/* ── Render one frame ───────────────────────────────────────────────────── */
static void render_frame(void)
{
    uint32_t w = g_info.width;
    uint32_t h = g_info.height;
    uint32_t *fb32 = (uint32_t *)compositor_fb;

    /* XP Bliss-like desktop background (sky blue → meadow green) */
    for (uint32_t y = 0; y < h; y++) {
        uint32_t top_h = h * 2 / 3;
        uint32_t c;
        if (y < top_h) {
            uint8_t r = (uint8_t)(0x6F + ((0x3A - 0x6F) * y) / top_h);
            uint8_t g = (uint8_t)(0xB5 + ((0x7A - 0xB5) * y) / top_h);
            uint8_t b = (uint8_t)(0xE8 + ((0x3A - 0xE8) * y) / top_h);
            c = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        } else {
            uint32_t dy = y - top_h;
            uint32_t bh = h - top_h;
            uint8_t r = (uint8_t)(0x3A + ((0x2A - 0x3A) * dy) / bh);
            uint8_t g = (uint8_t)(0x7A + ((0x5A - 0x7A) * dy) / bh);
            uint8_t b = (uint8_t)(0x3A + ((0x2A - 0x3A) * dy) / bh);
            c = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        for (uint32_t x = 0; x < w; x++)
            fb32[y * w + x] = c;
    }

    /* ── Taskbar (bottom) ───────────────────────────────────────────────── */
    int tb_y = (int)h - TASKBAR_H;
    vgrad(fb32, w, h, 0, tb_y, w, TASKBAR_H, XP_TASKBAR_TOP, XP_TASKBAR_BOTTOM);

    /* Taskbar highlight line at top */
    for (uint32_t lx = 0; lx < w; lx++)
        fb32[(uint32_t)tb_y * w + lx] = 0xFF4A7ADCu;

    /* Taskbar bevel line at bottom */
    for (uint32_t lx = 0; lx < w; lx++)
        fb32[(uint32_t)(tb_y + TASKBAR_H - 1) * w + lx] = 0xFF081844u;

    /* ── Start button ───────────────────────────────────────────────────── */
    #define START_BTN_W 90
    #define START_BTN_H (TASKBAR_H - 6)
    int sb_x = 4;
    int sb_y = tb_y + 3;
    uint32_t sc = g_menu_btn_hover ? XP_START_HOVER : XP_START_GREEN;
    uint32_t sc2 = g_menu_btn_hover ? XP_START_GREEN : XP_START_GREEN2;
    vgrad(fb32, w, h, sb_x, sb_y, START_BTN_W, START_BTN_H, sc, sc2);
    /* Start button border */
    for (int l = 0; l < START_BTN_W; l++) {
        fb32[(uint32_t)sb_y * w + (uint32_t)(sb_x + l)] = 0xFFD4FFD4u;
        fb32[(uint32_t)(sb_y + START_BTN_H - 1) * w + (uint32_t)(sb_x + l)] = 0xFF1A5C1Au;
    }
    for (int l = 0; l < START_BTN_H; l++) {
        fb32[(uint32_t)(sb_y + l) * w + (uint32_t)sb_x] = 0xFFB5E8B5u;
        fb32[(uint32_t)(sb_y + l) * w + (uint32_t)(sb_x + START_BTN_W - 1)] = 0xFF1A5C1Au;
    }
    /* Start button text */
    draw_text(fb32, w, h, sb_x + 12, sb_y + (START_BTN_H - FONT_H) / 2,
              g_menu_open ? "Zirvium >>" : "Zirvium", 0xFFFFFFFFu);

    /* Separator after start button */
    int sep_x = sb_x + START_BTN_W + 4;
    for (int ly = 4; ly < TASKBAR_H - 4; ly++)
        fb32[(uint32_t)(tb_y + ly) * w + (uint32_t)sep_x] = 0xFF0A1E52u;
    for (int ly = 4; ly < TASKBAR_H - 4; ly++)
        fb32[(uint32_t)(tb_y + ly) * w + (uint32_t)(sep_x + 1)] = 0xFF3A6ABAu;

    /* ── Open app taskbar button (only when an app is running) ──────────── */
    #define APP_BTN_H (TASKBAR_H - 8)
    #define APP_BTN_W 150
    static int g_app_btn_x = 0;
    if (g_active_app >= 0 && !g_showing_desktop) {
        int app_btn_y = tb_y + 4;
        g_app_btn_x = sep_x + 6;
        uint32_t abg = 0xFF4A7ADCu, abg2 = 0xFF2A5ABCu;
        vgrad(fb32, w, h, g_app_btn_x, app_btn_y, APP_BTN_W, APP_BTN_H, abg, abg2);
        for (int l = 0; l < APP_BTN_W; l++) {
            fb32[(uint32_t)app_btn_y * w + (uint32_t)(g_app_btn_x + l)] = 0xFF6A9AECu;
            fb32[(uint32_t)(app_btn_y + APP_BTN_H - 1) * w + (uint32_t)(g_app_btn_x + l)] = 0xFF081844u;
        }
        for (int l = 0; l < APP_BTN_H; l++) {
            fb32[(uint32_t)(app_btn_y + l) * w + (uint32_t)g_app_btn_x] = 0xFF5A8ADCu;
            fb32[(uint32_t)(app_btn_y + l) * w + (uint32_t)(g_app_btn_x + APP_BTN_W - 1)] = 0xFF081844u;
        }
        draw_text(fb32, w, h, g_app_btn_x + 4, app_btn_y + (APP_BTN_H - FONT_H) / 2,
                  g_app_names[g_active_app], XP_TASKBAR_TEXT);
    } else {
        g_app_btn_x = sep_x + 6;
    }

    /* ── System tray: speaker, battery, clock, peek button ──────────────── */
    int tray_right_x = (int)w - 4;

    /* Peek button (Win7 Aero-style slim button at far right) */
    #define PEEK_W 16
    #define PEEK_H (TASKBAR_H - 10)
    int peek_x = tray_right_x - PEEK_W;
    int peek_y = tb_y + 5;
    uint32_t peek_c = g_peek_hover ? 0xFF5A8ADCu : 0xFF1A3A7Au;
    fill_rect(fb32, w, h, peek_x, peek_y, PEEK_W, PEEK_H, peek_c);
    for (int l = 0; l < PEEK_W; l++) {
        fb32[(uint32_t)peek_y * w + (uint32_t)(peek_x + l)] = 0xFF6A9AECu;
        fb32[(uint32_t)(peek_y + PEEK_H - 1) * w + (uint32_t)(peek_x + l)] = 0xFF081844u;
    }
    for (int l = 0; l < PEEK_H; l++) {
        fb32[(uint32_t)(peek_y + l) * w + (uint32_t)peek_x] = 0xFF5A8ADCu;
        fb32[(uint32_t)(peek_y + l) * w + (uint32_t)(peek_x + PEEK_W - 1)] = 0xFF081844u;
    }
    /* Vertical separator line in peek button (looks like a slim divider) */
    for (int l = 3; l < PEEK_H - 3; l++)
        fb32[(uint32_t)(peek_y + l) * w + (uint32_t)(peek_x + PEEK_W / 2)] = 0xFFAAC8F8u;

    /* Clock */
    int clock_x = peek_x - 5 * (FONT_W + 1) - 14;
    int clock_y = tb_y + (TASKBAR_H - FONT_H) / 2;
    /* Tray area background for clock + icons */
    int tray_w = peek_x - clock_x + 4;
    fill_rect(fb32, w, h, clock_x - 4, tb_y + 2, tray_w + 8, TASKBAR_H - 4, 0xFF0A1A52u);
    for (int l = 0; l < tray_w + 8; l++) {
        fb32[(uint32_t)(tb_y + 2) * w + (uint32_t)(clock_x - 4 + l)] = 0xFF081844u;
        fb32[(uint32_t)(tb_y + TASKBAR_H - 3) * w + (uint32_t)(clock_x - 4 + l)] = 0xFF2A4A8Au;
    }
    draw_clock(fb32, w, h, clock_x, clock_y, XP_TASKBAR_TEXT);

    /* Speaker icon */
    int spk_x = clock_x - 18;
    draw_text(fb32, w, h, spk_x, clock_y, "))", XP_TASKBAR_TEXT);

    /* Battery icon */
    int bat_x = spk_x - 14;
    /* Battery outline */
    fill_rect(fb32, w, h, bat_x, clock_y + 1, 10, FONT_H - 2, XP_TASKBAR_TEXT);
    fill_rect(fb32, w, h, bat_x + 10, clock_y + 4, 2, 5, XP_TASKBAR_TEXT);
    /* Battery fill (half) */
    fill_rect(fb32, w, h, bat_x + 2, clock_y + 3, 6, FONT_H - 6, 0xFF66DD66u);

    /* ── XP-style Start menu (pops up from taskbar) ─────────────────────── */
    if (g_menu_open) {
        int mh = NUM_APPS * MENU_ITEM_H + 48;
        int my = tb_y - mh;
        int mx = 4;
        draw_shadow(fb32, w, h, mx - 2, my - 2, MENU_W + 4, mh + 4, 6);
        /* Menu background */
        fill_rect(fb32, w, h, mx, my, MENU_W, mh, 0xFFFFFFFFu);
        /* Menu border */
        for (int l = 0; l < MENU_W; l++) {
            fb32[(uint32_t)my * w + (uint32_t)(mx + l)] = 0xFF0A2A6Au;
            fb32[(uint32_t)(my + mh - 1) * w + (uint32_t)(mx + l)] = 0xFF0A2A6Au;
        }
        for (int l = 0; l < mh; l++) {
            fb32[(uint32_t)(my + l) * w + (uint32_t)mx] = 0xFF0A2A6Au;
            fb32[(uint32_t)(my + l) * w + (uint32_t)(mx + MENU_W - 1)] = 0xFF0A2A6Au;
        }
        /* User name bar at top */
        fill_rect(fb32, w, h, mx + 2, my + 2, MENU_W - 4, 20, 0xFF0A2A6Au);
        draw_text(fb32, w, h, mx + 10, my + 5, "Zirvium User", 0xFFFFFFFFu);

        for (int i = 0; i < NUM_APPS; i++) {
            int iy = my + 24 + i * MENU_ITEM_H;
            if (i == g_menu_hover) {
                fill_rect(fb32, w, h, mx + 4, iy, MENU_W - 8, MENU_ITEM_H, 0xFFD4E0F8u);
                for (int l = 0; l < MENU_W - 8; l++) {
                    fb32[(uint32_t)iy * w + (uint32_t)(mx + 4 + l)] = 0xFF0A5ADFu;
                    fb32[(uint32_t)(iy + MENU_ITEM_H - 1) * w + (uint32_t)(mx + 4 + l)] = 0xFF0A5ADFu;
                }
                for (int l = 0; l < MENU_ITEM_H; l++) {
                    fb32[(uint32_t)(iy + l) * w + (uint32_t)(mx + 4)] = 0xFF0A5ADFu;
                    fb32[(uint32_t)(iy + l) * w + (uint32_t)(mx + MENU_W - 5)] = 0xFF0A5ADFu;
                }
            }
            draw_text(fb32, w, h, mx + 16, iy + (MENU_ITEM_H - FONT_H) / 2,
                      g_app_names[i], i == g_menu_hover ? 0xFF000000u : 0xFF000000u);
        }
        /* Separator + Reboot */
        int sep_y = my + 24 + NUM_APPS * MENU_ITEM_H;
        fill_rect(fb32, w, h, mx + 8, sep_y, MENU_W - 16, 1, 0xFFC0C0C0u);
        int rby = sep_y + 6;
        fill_rect(fb32, w, h, mx + 6, rby, MENU_W - 12, MENU_ITEM_H, 0xFFF0F0F0u);
        draw_text(fb32, w, h, mx + 16, rby + (MENU_ITEM_H - FONT_H) / 2,
                  "Reboot", 0xFFCC0000u);
    }

    /* ── App content window (XP style) ──────────────────────────────────── */
    if (g_active_app >= 0 && !g_showing_desktop) {
        int win_x = g_win_x, win_y = g_win_y;
        int win_w = (int)w - 80;
        int win_h = (int)h - TASKBAR_H - g_win_y - 8;
        if (win_h < 80) win_h = 80;

        /* Window shadow */
        draw_shadow(fb32, w, h, win_x - 4, win_y - 24, win_w + 8, win_h + 30, 8);

        /* XP blue title bar (vertical gradient) */
        vgrad(fb32, w, h, win_x, win_y - 24, win_w, 24, XP_TITLE_TOP, XP_TITLE_BOTTOM);

        /* Title text */
        draw_text(fb32, w, h, win_x + 10, win_y - 20,
                  g_app_names[g_active_app], XP_TITLE_TEXT);

        /* Window close button (XP red) */
        int cx2 = win_x + win_w - 26, cy2 = win_y - 22;
        uint32_t clr = g_close_hover ? XP_CLOSE_HOVER : XP_CLOSE_BG;
        fill_rect(fb32, w, h, cx2, cy2, 18, 18, clr);
        for (int l = 0; l < 18; l++) {
            fb32[(uint32_t)cy2 * w + (uint32_t)(cx2 + l)] = 0xFFCC4444u;
            fb32[(uint32_t)(cy2 + 17) * w + (uint32_t)(cx2 + l)] = 0xFF882222u;
        }
        for (int l = 0; l < 18; l++) {
            fb32[(uint32_t)(cy2 + l) * w + (uint32_t)cx2] = 0xFFCC4444u;
            fb32[(uint32_t)(cy2 + l) * w + (uint32_t)(cx2 + 17)] = 0xFF882222u;
        }
        draw_char(fb32, w, h, cx2 + 5, cy2 + 3, 'x', XP_CLOSE_X);

        /* Content area (XP cream) */
        fill_rect(fb32, w, h, win_x, win_y, win_w, win_h, XP_WIN_BG);

        /* Window sunken border */
        for (int l = 0; l < win_w; l++) {
            fb32[(uint32_t)(win_y) * w + (uint32_t)(win_x + l)] = 0xFF888888u;
            fb32[(uint32_t)(win_y + 1) * w + (uint32_t)(win_x + l)] = 0xFFFFFFFFu;
            fb32[(uint32_t)(win_y + win_h - 2) * w + (uint32_t)(win_x + l)] = 0xFFFFFFFFu;
            fb32[(uint32_t)(win_y + win_h - 1) * w + (uint32_t)(win_x + l)] = 0xFF888888u;
        }
        for (int l = 0; l < win_h; l++) {
            fb32[(uint32_t)(win_y + l) * w + (uint32_t)win_x] = 0xFF888888u;
            fb32[(uint32_t)(win_y + l) * w + (uint32_t)(win_x + 1)] = 0xFFFFFFFFu;
            fb32[(uint32_t)(win_y + l) * w + (uint32_t)(win_x + win_w - 2)] = 0xFFFFFFFFu;
            fb32[(uint32_t)(win_y + l) * w + (uint32_t)(win_x + win_w - 1)] = 0xFF888888u;
        }

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
    int prev_ph = g_peek_hover;

    uint32_t ww = g_info.width;
    uint32_t wh = g_info.height;
    int tb_y = (int)wh - TASKBAR_H;

    /* ── Running-app button hit rect (single button for active app) ──── */
    #define APP_BTN_X (4 + 90 + 4 + 6)  /* after Start btn + separators */
    #define APP_BTN_WV 150
    #define APP_BTN_HV (TASKBAR_H - 8)
    int aby = tb_y + 4;

    /* ── Peek button rect ────────────────────────────────────────────── */
    #define PEEK_W 16
    #define PEEK_H (TASKBAR_H - 10)
    int peek_x = (int)ww - 4 - PEEK_W;
    int peek_y = tb_y + 5;

    mouse_event_t ev;
    while (zf_read_mouse(&ev) == 0) {
        int nx = cursor_x + ev.dx;
        int ny = cursor_y + ev.dy;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx >= (int)ww)  nx = (int)ww - 1;
        if (ny >= (int)wh) ny = (int)wh - 1;
        cursor_x = nx;
        cursor_y = ny;
        zf_set_cursor(cursor_x, cursor_y);

        int left_down  = (ev.buttons & 1) && !(prev_buttons & 1);
        int left_up    = !(ev.buttons & 1) && (prev_buttons & 1);
        prev_buttons = ev.buttons;

        int win_w = (int)ww - 80;

        /* ── Window dragging ─────────────────────────────────────────── */
        if (g_dragging) {
            int new_x = nx - g_drag_off_x;
            int new_y = ny - g_drag_off_y;
            if (new_x < 2) new_x = 2;
            if (new_x + win_w > (int)ww - 2) new_x = (int)ww - win_w - 2;
            if (new_y < 2) new_y = 2;
            if (new_y + 24 > tb_y) new_y = tb_y + 1 - 24;
            if (new_x != g_win_x || new_y != g_win_y) {
                g_win_x = new_x;
                g_win_y = new_y;
                g_dirty = 1;
            }
            if (left_up) { g_dragging = 0; g_dirty = 1; }
            continue;
        }

        /* ── Hover: Start button ─────────────────────────────────────── */
        g_menu_btn_hover = point_in(nx, ny, 4, tb_y + 3, 90, TASKBAR_H - 6);

        /* ── Hover: running app button on taskbar ────────────────────── */
        int app_click = -1;
        if (g_active_app >= 0 && !g_showing_desktop &&
            point_in(nx, ny, APP_BTN_X, aby, APP_BTN_WV, APP_BTN_HV))
            app_click = g_active_app;

        /* ── Hover: window close button ──────────────────────────────── */
        if (g_active_app >= 0) {
            int cx2 = g_win_x + win_w - 26, cy2 = g_win_y - 22;
            g_close_hover = point_in(nx, ny, cx2, cy2, 18, 18);
        } else {
            g_close_hover = 0;
        }

        /* ── Hover: menu items ───────────────────────────────────────── */
        g_menu_hover = -1;
        if (g_menu_open) {
            int mh = NUM_APPS * MENU_ITEM_H + 48;
            int my = tb_y - mh;
            int mx = 4;
            for (int i = 0; i < NUM_APPS; i++) {
                int iy = my + 24 + i * MENU_ITEM_H;
                if (point_in(nx, ny, mx + 4, iy, MENU_W - 8, MENU_ITEM_H)) {
                    g_menu_hover = i;
                    break;
                }
            }
        }

        /* ── Hover: settings rows ────────────────────────────────────── */
        g_settings_hover = -1;
        if (g_active_app == APP_SETTINGS && !g_menu_open) {
            int sww = (int)ww - 80;
            int sx = g_win_x + 4, sy = g_win_y + 50;
            int row_hh = 28, cw = sww - 8;
            for (int i = 0; i < 2; i++) {
                if (point_in(nx, ny, sx, sy + i * (row_hh + 4), cw, row_hh)) {
                    g_settings_hover = i;
                    break;
                }
            }
        }

        /* ── Hover: peek button ──────────────────────────────────────── */
        g_peek_hover = point_in(nx, ny, peek_x, peek_y, PEEK_W, PEEK_H);

        /* ── Clicks ──────────────────────────────────────────────────── */
        if (left_down) {
            if (g_menu_btn_hover) {
                g_menu_open = !g_menu_open;
                if (g_menu_open) g_showing_desktop = 0;
                g_dirty = 1;
            } else if (g_menu_open) {
                if (g_menu_hover >= 0) {
                    g_active_app = g_menu_hover;
                    g_win_x = 40;
                    g_win_y = 80;
                    g_menu_open = 0;
                    g_dirty = 1;
                } else {
                    int mh = NUM_APPS * MENU_ITEM_H + 48;
                    int my = tb_y - mh;
                    int sep_y = my + 24 + NUM_APPS * MENU_ITEM_H;
                    if (point_in(nx, ny, 10, sep_y + 6, MENU_W - 20, MENU_ITEM_H))
                        zf_reboot();
                    else { g_menu_open = 0; g_dirty = 1; }
                }
            } else if (g_close_hover) {
                g_active_app = -1;
                g_dirty = 1;
            } else if (app_click >= 0) {
                if (g_showing_desktop) g_showing_desktop = 0;
                g_active_app = app_click;
                g_win_x = 40;
                g_win_y = 80;
                g_dirty = 1;
            } else if (g_peek_hover) {
                g_showing_desktop = !g_showing_desktop;
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
                int wcx = g_win_x + win_w - 26;
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
        g_menu_btn_hover != prev_mb || g_dragging != prev_dr ||
        g_peek_hover != prev_ph)
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
