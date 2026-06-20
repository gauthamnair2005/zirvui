#include "canvas.h"
#include "font_8x13.h"
#include <stddef.h>

static int cnv_min(int a, int b) { return a < b ? a : b; }
static int cnv_max(int a, int b) { return a > b ? a : b; }
static int cnv_abs(int x) { return x < 0 ? -x : x; }

uint32_t canvas_blend(uint32_t fg, uint32_t bg, uint8_t alpha)
{
    uint32_t fa = alpha;
    uint32_t fga = 255 - fa;
    uint32_t r = ((fg >> 16) & 0xFF) * fa + ((bg >> 16) & 0xFF) * fga;
    uint32_t g = ((fg >> 8) & 0xFF) * fa + ((bg >> 8) & 0xFF) * fga;
    uint32_t b = ((fg) & 0xFF) * fa + ((bg) & 0xFF) * fga;
    return 0xFF000000 | ((r / 255) << 16) | ((g / 255) << 8) | (b / 255);
}

void canvas_set_pixel(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t color)
{
    (void)fb_h;
    if (x < 0 || (uint32_t)x >= fb_w) return;
    fb[(uint32_t)y * fb_w + (uint32_t)x] = color;
}

void canvas_fill_rect(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t rw, uint32_t rh, uint32_t color)
{
    int x0 = cnv_max(x, 0);
    int y0 = cnv_max(y, 0);
    int x1 = cnv_min(x + (int)rw - 1, (int)fb_w - 1);
    int y1 = cnv_min(y + (int)rh - 1, (int)fb_h - 1);
    for (int py = y0; py <= y1; py++) {
        uint32_t *row = fb + (uint32_t)py * fb_w;
        for (int px = x0; px <= x1; px++)
            row[px] = color;
    }
}

void canvas_fill_gradient_v(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t gw, uint32_t gh, uint32_t top, uint32_t bot)
{
    int x0 = cnv_max(x, 0);
    int y0 = cnv_max(y, 0);
    int x1 = cnv_min(x + (int)gw - 1, (int)fb_w - 1);
    int y1 = cnv_min(y + (int)gh - 1, (int)fb_h - 1);
    uint8_t tr = (uint8_t)(top >> 16), tg = (uint8_t)(top >> 8), tb = (uint8_t)(top);
    uint8_t br = (uint8_t)(bot >> 16), bg2 = (uint8_t)(bot >> 8), bb = (uint8_t)(bot);
    for (int py = y0; py <= y1; py++) {
        uint32_t t = (uint32_t)(py - y) * 255 / (gh > 0 ? gh : 1);
        if (t > 255) t = 255;
        uint32_t inv = 255 - t;
        uint8_t r = (uint8_t)(((uint32_t)tr * inv + (uint32_t)br * t) / 255);
        uint8_t g = (uint8_t)(((uint32_t)tg * inv + (uint32_t)bg2 * t) / 255);
        uint8_t b = (uint8_t)(((uint32_t)tb * inv + (uint32_t)bb * t) / 255);
        uint32_t c = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        uint32_t *row = fb + (uint32_t)py * fb_w;
        for (int px = x0; px <= x1; px++)
            row[px] = c;
    }
}

void canvas_draw_char(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint8_t c, uint32_t color)
{
    int idx = (int)c - FONT_FIRST_CHAR;
    if (idx < 0 || idx >= FONT_NUM_GLYPHS) idx = 0;
    const uint8_t *glyph = font_8x13[idx];
    for (int row = 0; row < FONT_H; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= fb_h) continue;
        uint8_t bits = glyph[row];
        if (bits == 0) continue;
        uint32_t *row_p = fb + (uint32_t)py * fb_w;
        for (int col = 0; col < FONT_W; col++) {
            int px = x + col;
            if (px < 0 || (uint32_t)px >= fb_w) continue;
            if (bits & (1 << (7 - col))) {
                int tl = col > 0 && (bits & (1 << (7 - col + 1)));
                int tr = col < (FONT_W - 1) && (bits & (1 << (7 - col - 1)));
                int tu = row > 0 && (glyph[row - 1] & (1 << (7 - col)));
                int td = row < (FONT_H - 1) && (glyph[row + 1] & (1 << (7 - col)));
                if (tl && tr && tu && td) {
                    row_p[px] = color;
                } else {
                    uint32_t bg = row_p[px];
                    row_p[px] = canvas_blend(color, bg, 200);
                }
            }
        }
    }
}

void canvas_draw_char_aa(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint8_t c, uint32_t color, uint8_t aa_level)
{
    (void)aa_level;
    int idx = (int)c - FONT_FIRST_CHAR;
    if (idx < 0 || idx >= FONT_NUM_GLYPHS) idx = 0;
    const uint8_t *glyph = font_8x13[idx];
    for (int row = 0; row < FONT_H; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= fb_h) continue;
        uint32_t *row_p = fb + (uint32_t)py * fb_w;
        for (int col = 0; col < FONT_W; col++) {
            int px = x + col;
            if (px < 0 || (uint32_t)px >= fb_w) continue;
            if (!(glyph[row] & (1 << (7 - col)))) continue;
            int covered = 0, total = 0;
            for (int dr = -1; dr <= 1; dr++) {
                int nr = row + dr;
                if (nr < 0 || nr >= FONT_H) continue;
                uint8_t nb = glyph[nr];
                for (int dc = -1; dc <= 1; dc++) {
                    int nc = col + dc;
                    if (nc < 0 || nc >= FONT_W) continue;
                    if (nb & (1 << (7 - nc))) covered++;
                    total++;
                }
            }
            if (total > 0 && covered < total) {
                uint8_t a = (uint8_t)((covered * 255) / total);
                row_p[px] = canvas_blend(color, row_p[px], a);
            } else {
                row_p[px] = color;
            }
        }
    }
}

void canvas_draw_text(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, const uint8_t *text, uint32_t color)
{
    int cx = x;
    while (*text) {
        canvas_draw_char_aa(fb, fb_w, fb_h, cx, y, *text, color, 3);
        cx += CANVAS_FONT_STEP;
        text++;
    }
}

void canvas_hline(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t line_w, uint32_t color)
{
    if (y < 0 || (uint32_t)y >= fb_h) return;
    int x0 = cnv_max(x, 0);
    int x1 = cnv_min(x + (int)line_w - 1, (int)fb_w - 1);
    uint32_t *row = fb + (uint32_t)y * fb_w;
    for (int px = x0; px <= x1; px++)
        row[px] = color;
}

void canvas_fill_circle(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int cx, int cy, uint32_t r, uint32_t color)
{
    int rr = (int)(r * r);
    for (int dy = -(int)r; dy <= (int)r; dy++) {
        int py = cy + dy;
        if (py < 0 || (uint32_t)py >= fb_h) continue;
        int dx = 0;
        while (dx * dx + dy * dy <= rr) dx++;
        dx--;
        if (dx < 0) continue;
        int x0 = cnv_max(cx - dx, 0);
        int x1 = cnv_min(cx + dx, (int)fb_w - 1);
        if (x0 <= x1) {
            uint32_t *row = fb + (uint32_t)py * fb_w;
            for (int px = x0; px <= x1; px++)
                row[px] = color;
        }
    }
}

void canvas_fill_round_rect(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t rw, uint32_t rh, uint32_t radius, uint32_t color)
{
    if (rw == 0 || rh == 0) return;
    uint32_t ru = radius < rw / 2u && radius < rh / 2u ? radius : (uint32_t)cnv_min((int)rw, (int)rh) / 2u;
    int r = (int)ru;
    if (r <= 0) { canvas_fill_rect(fb, fb_w, fb_h, x, y, rw, rh, color); return; }
    int r2 = r * r;
    for (int row = 0; row < (int)rh; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= fb_h) continue;
        int x0, x1;
        if (row < r) {
            int d = r - row;
            int ins = 0;
            int ds = d * d;
            while ((ins + 1) * (ins + 1) + ds <= r2) ins++;
            x0 = x + r - ins;
            x1 = x + (int)rw - r + ins - 1;
        } else if (row > (int)rh - r - 1) {
            int d = row - ((int)rh - r - 1);
            int ins = 0;
            int ds = d * d;
            while ((ins + 1) * (ins + 1) + ds <= r2) ins++;
            x0 = x + r - ins;
            x1 = x + (int)rw - r + ins - 1;
        } else {
            x0 = x;
            x1 = x + (int)rw - 1;
        }
        if (x0 < 0 || x1 >= (int)fb_w) {
            uint32_t *row_p = fb + (uint32_t)py * fb_w;
            if (x0 < 0 && x0 <= x1) {
                int dist = -x0;
                uint8_t a = (uint8_t)(dist < r ? 128 : 255);
                row_p[0] = canvas_blend(color, row_p[0], a);
                x0 = 1;
            }
            if (x1 >= (int)fb_w && x0 <= x1) {
                int dist = x1 - ((int)fb_w - 1);
                uint8_t a = (uint8_t)(dist < r ? 128 : 255);
                row_p[x1] = canvas_blend(color, row_p[x1], a);
                x1 = (int)fb_w - 2;
            }
        }
        x0 = cnv_max(x0, 0);
        x1 = cnv_min(x1, (int)fb_w - 1);
        if (x0 <= x1) {
            uint32_t *row_p = fb + (uint32_t)py * fb_w;
            for (int px = x0; px <= x1; px++)
                row_p[px] = color;
        }
    }
    /* Smooth corner edges with coverage alpha using distance from circle */
    for (int row = 0; row < r && row < (int)rh; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= fb_h) continue;
        int d = r - row;
        int ds = d * d;
        int ins = 0;
        while ((ins + 1) * (ins + 1) + ds <= r2) ins++;
        /* Left corner edge */
        int cx0 = x + r - ins - 1;
        if (cx0 >= 0 && (uint32_t)cx0 < fb_w) {
            int dx = r - ins;
            int dist2 = dx * dx + ds;
            if (dist2 < r2 + r) {
                uint32_t *p = &fb[(uint32_t)py * fb_w + (uint32_t)cx0];
                int excess = r2 - dist2;
                uint8_t a = (uint8_t)((excess * 255) / (2 * r + 1));
                if (a < 255) *p = canvas_blend(color, *p, a > 0 ? a : 0);
            }
        }
        /* Right corner edge */
        int cx1 = x + (int)rw - r + ins;
        if (cx1 >= 0 && (uint32_t)cx1 < fb_w) {
            int dx = r - ins;
            int dist2 = dx * dx + ds;
            if (dist2 < r2 + r) {
                uint32_t *p = &fb[(uint32_t)py * fb_w + (uint32_t)cx1];
                int excess = r2 - dist2;
                uint8_t a = (uint8_t)((excess * 255) / (2 * r + 1));
                if (a < 255) *p = canvas_blend(color, *p, a > 0 ? a : 0);
            }
        }
    }
    for (int row = (int)rh - r; row < (int)rh && row >= 0; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= fb_h) continue;
        int d = row - ((int)rh - r - 1);
        int ds = d * d;
        int ins = 0;
        while ((ins + 1) * (ins + 1) + ds <= r2) ins++;
        int cx0 = x + r - ins - 1;
        if (cx0 >= 0 && (uint32_t)cx0 < fb_w) {
            int dx = r - ins;
            int dist2 = dx * dx + ds;
            if (dist2 < r2 + r) {
                uint32_t *p = &fb[(uint32_t)py * fb_w + (uint32_t)cx0];
                int excess = r2 - dist2;
                uint8_t a = (uint8_t)((excess * 255) / (2 * r + 1));
                if (a < 255) *p = canvas_blend(color, *p, a > 0 ? a : 0);
            }
        }
        int cx1 = x + (int)rw - r + ins;
        if (cx1 >= 0 && (uint32_t)cx1 < fb_w) {
            int dx = r - ins;
            int dist2 = dx * dx + ds;
            if (dist2 < r2 + r) {
                uint32_t *p = &fb[(uint32_t)py * fb_w + (uint32_t)cx1];
                int excess = r2 - dist2;
                uint8_t a = (uint8_t)((excess * 255) / (2 * r + 1));
                if (a < 255) *p = canvas_blend(color, *p, a > 0 ? a : 0);
            }
        }
    }
}

void canvas_draw_line(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = cnv_abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -cnv_abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && (uint32_t)x0 < fb_w && y0 >= 0 && (uint32_t)y0 < fb_h)
            fb[(uint32_t)y0 * fb_w + (uint32_t)x0] = color;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static const uint8_t *font_glyph(uint8_t c)
{
    int idx = (int)c - FONT_FIRST_CHAR;
    if (idx < 0 || idx >= FONT_NUM_GLYPHS) idx = 0;
    return font_8x13[idx];
}

void canvas_draw_char_scaled(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint8_t c, uint32_t color, uint32_t scale)
{
    if (scale == 0) return;
    const uint8_t *glyph = font_glyph(c);
    if (scale == 1) {
        canvas_draw_char(fb, fb_w, fb_h, x, y, c, color);
        return;
    }
    int sw = (int)scale;
    int sh = (int)scale;
    int fw = FONT_W;
    int fh = FONT_H;
    for (int row = 0; row < fh * sh; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= fb_h) continue;
        uint32_t *row_p = fb + (uint32_t)py * fb_w;
        for (int col = 0; col < fw * sw; col++) {
            int px = x + col;
            if (px < 0 || (uint32_t)px >= fb_w) continue;
            int sx = col / sw;
            int sy = row / sh;
            if (sx >= fw) sx = fw - 1;
            if (sy >= fh) sy = fh - 1;
            if (glyph[sy] & (1 << (7 - sx)))
                row_p[px] = color;
        }
    }
}

void canvas_draw_text_large(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, const uint8_t *text, uint32_t color, uint32_t scale)
{
    int cx = x;
    int step = (CANVAS_FONT_W + 1) * (int)scale;
    while (*text) {
        canvas_draw_char_scaled(fb, fb_w, fb_h, cx, y, *text, color, scale);
        cx += step;
        text++;
    }
}
