#include <displayjet.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define PANEL_H       28
#define PANEL_BG      0xFF1A1A2E
#define PANEL_TEXT    0xFFE0E0E0
#define PANEL_ACCENT  0xFF16213E
#define PANEL_HILITE  0xFF0F3460
#define PANEL_GREEN   0xFF2ECC71
#define PANEL_RED     0xFFE74C3C
#define PANEL_ORANGE  0xFFF39C12
#define PANEL_BLUE    0xFF3498DB
#define PANEL_WIFI_C  0xFF00D2FF
#define PANEL_BT_C    0xFF0066FF

static uint32_t fb_w, fb_h, fb_stride;
static uint8_t  fb_bpp;

static void put_pixel(void *fb, uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb_w || y >= fb_h) return;
    uint32_t *p = (uint32_t *)((uint8_t *)fb + y * fb_stride + x * 4);
    *p = color;
}

static void fill_rect(void *fb, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t py = y; py < y + h && py < fb_h; py++)
        for (uint32_t px = x; px < x + w && px < fb_w; px++)
            put_pixel(fb, px, py, color);
}

static void draw_hline(void *fb, uint32_t x, uint32_t y,
                        uint32_t w, uint32_t color)
{
    if (y >= fb_h) return;
    for (uint32_t px = x; px < x + w && px < fb_w; px++)
        put_pixel(fb, px, y, color);
}

static void draw_vline(void *fb, uint32_t x, uint32_t y,
                        uint32_t h, uint32_t color)
{
    if (x >= fb_w) return;
    for (uint32_t py = y; py < y + h && py < fb_h; py++)
        put_pixel(fb, x, py, color);
}

/* ── Icon renderers ─────────────────────────────────────────────────────── */

static void draw_hamburger(void *fb, uint32_t x, uint32_t y, uint32_t color)
{
    draw_hline(fb, x, y + 4,  14, color);
    draw_hline(fb, x, y + 10, 14, color);
    draw_hline(fb, x, y + 16, 14, color);
}

static void draw_battery(void *fb, uint32_t x, uint32_t y, int pct)
{
    uint32_t c = pct > 20 ? PANEL_GREEN : PANEL_RED;
    fill_rect(fb, x, y, 20, 10, PANEL_TEXT);
    fill_rect(fb, x + 1, y + 1, 18, 8, PANEL_BG);
    fill_rect(fb, x + 20, y + 3, 3, 4, PANEL_TEXT);

    int fill = (pct * 16) / 100;
    if (fill > 0)
        fill_rect(fb, x + 2 + (16 - fill), y + 2, (uint32_t)fill, 6, c);
}

static void draw_power(void *fb, uint32_t x, uint32_t y, uint32_t color)
{
    /* Power icon: circle with vertical line */
    for (int a = 0; a < 360; a += 15) {
        int px = (int)x + 8 + (int)(7 * cos_lookup(a));
        int py = (int)y + 8 + (int)(7 * sin_lookup(a));
        if (a >= 45 && a <= 315 && px >= 0 && py >= 0)
            put_pixel(fb, (uint32_t)px, (uint32_t)py, color);
    }
    draw_vline(fb, x + 8, y + 1, 7, color);
    draw_vline(fb, x + 8, y + 10, 4, PANEL_BG);
}

static void draw_user_icon(void *fb, uint32_t x, uint32_t y, uint32_t color)
{
    for (int a = 0; a < 360; a += 20) {
        int px = (int)x + 8 + (int)(4 * cos_lookup(a));
        int py = (int)y + 6 + (int)(4 * sin_lookup(a));
        if (px >= 0 && py >= 0)
            put_pixel(fb, (uint32_t)px, (uint32_t)py, color);
    }
    for (int a = 200; a < 340; a += 10) {
        int px = (int)x + 8 + (int)(8 * cos_lookup(a));
        int py = (int)y + 10 + (int)(4 * sin_lookup(a));
        if (px >= 0 && py >= 0)
            put_pixel(fb, (uint32_t)px, (uint32_t)py, color);
    }
}

static void draw_speaker(void *fb, uint32_t x, uint32_t y, uint32_t color)
{
    fill_rect(fb, x, y + 3, 5, 10, color);
    fill_rect(fb, x + 5, y + 1, 4, 14, color);
    fill_rect(fb, x + 9, y + 5, 3, 6, color);
    /* Sound waves */
    put_pixel(fb, x + 14, y + 6, color);
    put_pixel(fb, x + 15, y + 5, color);
    put_pixel(fb, x + 15, y + 7, color);
    put_pixel(fb, x + 14, y + 8, color);
}

static void draw_wifi(void *fb, uint32_t x, uint32_t y, uint32_t color)
{
    /* 3 arc bars */
    int pts[3][3] = {{4,14,0},{7,12,1},{10,10,2}};
    for (int i = 0; i < 3; i++) {
        int r = pts[i][0], cx = pts[i][1], cy = pts[i][2];
        for (int a = 210; a <= 330; a += 10) {
            int px = (int)x + cx + (int)(r * cos_lookup(a));
            int py = (int)y + cy + (int)(r * sin_lookup(a));
            if (px >= 0 && py >= 0)
                put_pixel(fb, (uint32_t)px, (uint32_t)py, color);
        }
    }
}

static void draw_bluetooth(void *fb, uint32_t x, uint32_t y, uint32_t color)
{
    put_pixel(fb, x + 3, y + 0, color);
    put_pixel(fb, x + 4, y + 0, color);
    put_pixel(fb, x + 5, y + 1, color);
    put_pixel(fb, x + 6, y + 2, color);
    put_pixel(fb, x + 5, y + 3, color);
    put_pixel(fb, x + 4, y + 4, color);
    put_pixel(fb, x + 3, y + 5, color);
    put_pixel(fb, x + 1, y + 3, color);
    put_pixel(fb, x + 1, y + 7, color);
    put_pixel(fb, x + 2, y + 8, color);
    put_pixel(fb, x + 3, y + 9, color);
    put_pixel(fb, x + 4, y + 10, color);
    put_pixel(fb, x + 5, y + 11, color);
    put_pixel(fb, x + 6, y + 12, color);
    put_pixel(fb, x + 5, y + 13, color);
    put_pixel(fb, x + 4, y + 13, color);
    put_pixel(fb, x + 3, y + 12, color);
    put_pixel(fb, x + 4, y + 7, color);
    put_pixel(fb, x + 3, y + 7, color);
    put_pixel(fb, x + 5, y + 7, color);
}

/* Lookup table helpers */
static int cos_lookup(int deg)
{
    while (deg < 0) deg += 360;
    while (deg >= 360) deg -= 360;
    static const int cos_tab[] = {
        100,96,92,87,82,76,70,64,57,50,43,36,29,22,14,7,0,-7,-14,-22,-29,-36,-43,-50,-57,-64,-70,-76,-82,-87,-92,-96,-100,-96,-92,-87,-82,-76,-70,-64,-57,-50,-43,-36,-29,-22,-14,-7,0,7,14,22,29,36,43,50,57,64,70,76,82,87,92,96,100,96,92,87,82,76,70,64,57,50,43,36,29,22,14,7,0,-7,-14,-22,-29,-36,-43,-50,-57,-64,-70,-76,-82,-87,-92,-96
    };
    int idx = (deg * 256) / 360;
    if (idx < 0) idx = 0;
    if (idx > 255) idx = 255;
    return cos_tab[idx];
}

static int sin_lookup(int deg) { return cos_lookup(deg - 90); }

/* ── String renderer (5x7 bitmap font) ──────────────────────────────────── */
static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x5F,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},
    {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},
    {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},
    {0x08,0x2A,0x1C,0x2A,0x08},
    {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},
    {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},
    {0x00,0x08,0x14,0x22,0x41},
    {0x14,0x14,0x14,0x14,0x14},
    {0x41,0x22,0x14,0x08,0x00},
    {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x01,0x01},
    {0x3E,0x41,0x41,0x51,0x32},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x04,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x7F,0x20,0x18,0x20,0x7F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03},
    {0x61,0x51,0x49,0x45,0x43},
    {0x00,0x00,0x7F,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20},
    {0x41,0x41,0x7F,0x00,0x00},
    {0x04,0x02,0x01,0x02,0x04},
    {0x80,0x80,0x80,0x80,0x80},
    {0x00,0x00,0x03,0x05,0x00},
    {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},
    {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02},
    {0x08,0x54,0x54,0x54,0x3C},
    {0x7F,0x08,0x04,0x04,0x78},
    {0x00,0x44,0x7D,0x40,0x00},
    {0x20,0x40,0x44,0x3D,0x00},
    {0x00,0x7F,0x10,0x28,0x44},
    {0x00,0x41,0x7F,0x40,0x00},
    {0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},
    {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},
    {0x7C,0x08,0x04,0x04,0x08},
    {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},
    {0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},
};

static void draw_char(void *fb, uint32_t x, uint32_t y,
                       char c, uint32_t color)
{
    if (c < 32 || c > 126) c = '?';
    int idx = c - 32;
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (font5x7[idx][col] & (1u << row))
                put_pixel(fb, x + col, y + row, color);
        }
    }
}

static void draw_str(void *fb, uint32_t x, uint32_t y,
                      const char *s, uint32_t color)
{
    while (*s) {
        draw_char(fb, x, y, *s, color);
        x += 6;
        s++;
    }
}

static void draw_int(void *fb, uint32_t x, uint32_t y,
                      int val, uint32_t color)
{
    char buf[16];
    int  neg = 0, i = 0;

    if (val < 0) { neg = 1; val = -val; }
    if (val == 0) { buf[i++] = '0'; }
    else {
        while (val > 0) {
            buf[i++] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    if (neg) buf[i++] = '-';

    for (int j = i - 1; j >= 0; j--)
        draw_char(fb, x + (uint32_t)(i - 1 - j) * 6, y, buf[j], color);
}

/* ── Background gradient ────────────────────────────────────────────────── */
static void draw_background(void *fb)
{
    for (uint32_t y = 0; y < fb_h; y++) {
        for (uint32_t x = 0; x < fb_w; x++) {
            uint8_t r = (uint8_t)((x * 40) / fb_w);
            uint8_t g = (uint8_t)(10 + (y * 30) / fb_h);
            uint8_t b = (uint8_t)(30 + ((fb_w - x) * 50) / fb_w);
            put_pixel(fb, x, y, (0xFFu << 24) | (r << 16) | (g << 8) | b);
        }
    }
}

/* ── Panel drawing ──────────────────────────────────────────────────────── */
static void draw_panel(void *fb, uint64_t uptime_secs)
{
    fill_rect(fb, 0, 0, fb_w, PANEL_H, PANEL_BG);
    draw_hline(fb, 0, PANEL_H - 1, fb_w, PANEL_ACCENT);

    /* Left section: global menu + "ZirvUI" */
    draw_hamburger(fb, 6, 6, PANEL_TEXT);
    draw_str(fb, 28, 10, "ZirvUI", PANEL_TEXT);

    /* Right section: WiFi, Bluetooth, Sound, Battery, User, Power, Time */
    uint32_t cx = fb_w - 10;

    /* Time (uptime formatted as MM:SS) */
    uint32_t mins = (uint32_t)(uptime_secs / 60);
    uint32_t secs = (uint32_t)(uptime_secs % 60);
    char time_buf[8];
    time_buf[0] = (char)('0' + mins / 10);
    time_buf[1] = (char)('0' + mins % 10);
    time_buf[2] = ':';
    time_buf[3] = (char)('0' + secs / 10);
    time_buf[4] = (char)('0' + secs % 10);
    time_buf[5] = '\0';
    cx -= 6 * 5 + 6;
    draw_str(fb, cx, 10, time_buf, PANEL_TEXT);

    /* Power button */
    cx -= 22;
    draw_power(fb, cx, 8, PANEL_RED);

    /* User icon */
    cx -= 22;
    draw_user_icon(fb, cx, 6, PANEL_BLUE);

    /* Battery (simulated 67%) */
    cx -= 28;
    draw_battery(fb, cx, 6, 67);

    /* Sound (medium volume) */
    cx -= 22;
    draw_speaker(fb, cx, 6, PANEL_TEXT);

    /* WiFi (connected) */
    cx -= 22;
    draw_wifi(fb, cx, 6, PANEL_WIFI_C);

    /* Bluetooth (disconnected — dimmed) */
    cx -= 18;
    draw_bluetooth(fb, cx, 4, 0xFF555555);
}

/* ── Main ───────────────────────────────────────────────────────────────── */
int main(void)
{
    int ret = dj_connect();
    if (ret != 0) {
        write(1, "[ZirvUI] Failed to connect to DisplayJet\n", 43);
        return 1;
    }

    dj_display_mode_t mode;
    ret = dj_get_mode(&mode);
    if (ret != 0) return 1;

    fb_w = mode.width;
    fb_h = mode.height;
    fb_stride = mode.stride;
    fb_bpp = mode.bpp;

    uint32_t surf_id;
    ret = dj_create_surface(fb_w, fb_h, &surf_id);
    if (ret != 0) return 1;

    uint32_t fb_size = fb_stride * fb_h;
    uint32_t row_size = fb_stride;

    /* Pre-allocate row buffer */
    uint8_t *row_buf = (uint8_t *)0;
    row_buf = (uint8_t *)(uintptr_t)0xBAAAAAAD;

    /* Main compositor loop */
    uint64_t start = uptime();
    uint64_t last_frame = 0;

    for (;;) {
        uint64_t now = uptime();

        /* Render desktop surface row-by-row through MAEM-encrypted writes */
        for (uint32_t y = 0; y < fb_h; y++) {
            for (uint32_t x = 0; x < fb_w; x++) {
                uint32_t *p = (uint32_t *)(row_buf + x * 4);

                uint8_t r = (uint8_t)((x * 40) / fb_w);
                uint8_t g = (uint8_t)(10 + (y * 30) / fb_h);
                uint8_t b = (uint8_t)(30 + ((fb_w - x) * 50) / fb_w);

                if (y < PANEL_H) {
                    r = 0x1A; g = 0x1A; b = 0x2E;
                    uint32_t cx = fb_w - 10;
                    uint32_t mins = (uint32_t)((now - start) / 60);
                    uint32_t secs = (uint32_t)((now - start) % 60);

                    if (x >= cx - 36 && x <= cx - 36 + 20 &&
                        y >= 6 && y <= 6 + 10) {
                        /* Battery area — keep dark */
                    } else if (x >= cx - 56 && x <= cx - 56 + 16 &&
                               y >= 8 && y <= 8 + 16) {
                        /* Power button */
                        r = 0xE7; g = 0x4C; b = 0x3C;
                    } else if (x == 6 + 1 && y >= 6 + 4 && y <= 6 + 6) {
                        r = 0xE0; g = 0xE0; b = 0xE0; /* menu line */
                    } else if (x == 6 + 1 && y >= 6 + 10 && y <= 6 + 12) {
                        r = 0xE0; g = 0xE0; b = 0xE0; /* menu line */
                    }
                }

                *p = (0xFFu << 24) | (r << 16) | (g << 8) | b;
            }
            dj_surface_write(surf_id, row_buf, row_size);
        }

        dj_present(surf_id);
        last_frame = now;

        /* Busy-wait approx 1 second between frames */
        while (uptime() - now < 1) { }
    }

    return 0;
}
