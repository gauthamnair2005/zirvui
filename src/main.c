#include <displayjet.h>
#include <stdint.h>
#include <stddef.h>

/* Simple pixel fill helpers */
static void fill_rect(void *fb, uint32_t width, uint32_t height,
                       uint32_t stride, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t py = y; py < y + h && py < height; py++) {
        for (uint32_t px = x; px < x + w && px < width; px++) {
            uint32_t *p = (uint32_t *)((uint8_t *)fb + py * stride + px * 4);
            *p = color;
        }
    }
}

static void draw_gradient(void *fb, uint32_t width, uint32_t height,
                           uint32_t stride)
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t *p = (uint32_t *)((uint8_t *)fb + y * stride + x * 4);
            uint8_t r = (uint8_t)((x * 255) / width);
            uint8_t g = (uint8_t)((y * 255) / height);
            uint8_t b = (uint8_t)(((x + y) * 128) / (width + height));
            *p = (uint32_t)(0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

static void draw_status_bar(void *fb, uint32_t width, uint32_t stride)
{
    uint32_t bar_color = 0xFF202020;
    uint32_t text_color = 0xFFCCCCCC;
    fill_rect(fb, width, 24, stride, 0, 0, width, 24, bar_color);

    /* Simple pixel row for "ZirvUI" title */
    for (uint32_t x = 8; x < 80 && x < width; x++) {
        uint32_t *p = (uint32_t *)((uint8_t *)fb + 6 * stride + x * 4);
        *p = text_color;
    }
    for (uint32_t x = 8; x < 80 && x < width; x++) {
        uint32_t *p = (uint32_t *)((uint8_t *)fb + 8 * stride + x * 4);
        *p = text_color;
    }
}

int main(void)
{
    /* Connect to DisplayJet compositor */
    int ret = dj_connect();

    dj_display_mode_t mode;
    ret = dj_get_mode(&mode);

    /* Create a full-screen desktop surface */
    uint32_t surf_id = 0;
    ret = dj_create_surface(mode.width, mode.height, &surf_id);

    /* Write desktop pixels: gradient + status bar */
    uint32_t stride = mode.width * 4;
    uint32_t fb_size = stride * mode.height;

    /* We'll use a stack buffer for small displays, or multiple writes */
    /* For simplicity, write in row chunks */
    /* First create a simple gradient in a temp buffer row by row */
    uint8_t row_buf[4096 * 4]; /* max 4096 pixels wide */

    for (uint32_t y = 0; y < mode.height; y++) {
        uint32_t row_size = stride;
        if (row_size > sizeof(row_buf)) row_size = sizeof(row_buf);

        for (uint32_t x = 0; x < mode.width && x * 4 + 4 <= sizeof(row_buf); x++) {
            uint32_t *p = (uint32_t *)(row_buf + x * 4);
            uint8_t r = (uint8_t)((x * 255) / mode.width);
            uint8_t g = (uint8_t)((y * 255) / mode.height);
            uint8_t b = (uint8_t)(((x + y) * 128) / (mode.width + mode.height));

            if (y < 24) {
                /* Status bar: dark background */
                if (y == 6 || y == 8) {
                    r = 0xCC; g = 0xCC; b = 0xCC; /* Title text */
                } else {
                    r = 0x20; g = 0x20; b = 0x20;
                }
            }
            *p = (uint32_t)(0xFF << 24) | (r << 16) | (g << 8) | b;
        }

        dj_surface_write(surf_id, row_buf, row_size);
    }

    /* Present the desktop */
    dj_present(surf_id);

    /* Main event loop: keep presenting */
    for (;;) {
        /* Re-present periodically (no scheduler, so this runs once) */
        dj_present(surf_id);
    }

    return 0;
}
