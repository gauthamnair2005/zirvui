#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>

#define CANVAS_FONT_W 8
#define CANVAS_FONT_H 13
#define CANVAS_FONT_STEP 9

uint32_t canvas_blend(uint32_t fg, uint32_t bg, uint8_t alpha);
void canvas_set_pixel(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t color);
void canvas_fill_rect(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t rw, uint32_t rh, uint32_t color);
void canvas_fill_gradient_v(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t gw, uint32_t gh, uint32_t top, uint32_t bot);
void canvas_draw_char(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint8_t c, uint32_t color);
void canvas_draw_char_aa(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint8_t c, uint32_t color, uint8_t aa_level);
void canvas_draw_text(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, const uint8_t *text, uint32_t color);
void canvas_hline(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t line_w, uint32_t color);
void canvas_fill_circle(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int cx, int cy, uint32_t r, uint32_t color);
void canvas_fill_round_rect(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint32_t rw, uint32_t rh, uint32_t radius, uint32_t color);
void canvas_draw_line(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x0, int y0, int x1, int y1, uint32_t color);
void canvas_draw_char_scaled(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, uint8_t c, uint32_t color, uint32_t scale);
void canvas_draw_text_large(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, const uint8_t *text, uint32_t color, uint32_t scale);

#endif
