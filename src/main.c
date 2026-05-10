#include <displayjet.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define PANEL_H  28

static uint32_t fb_w, fb_h, fb_stride;

/* Full-screen pixel buffer (3 MB for 1024×768×32bpp) */
static uint8_t compositor_fb[1024 * 768 * 4];

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

    uint32_t surf_id;
    ret = dj_create_surface(fb_w, fb_h, &surf_id);
    if (ret != 0) return 1;

    /* Render full desktop into compositor_fb */
    uint32_t *fb32 = (uint32_t *)compositor_fb;
    for (uint32_t y = 0; y < fb_h; y++) {
        for (uint32_t x = 0; x < fb_w; x++) {
            uint32_t off = y * fb_w + x;

            uint8_t r = (uint8_t)((x * 40) / fb_w);
            uint8_t g = (uint8_t)(10 + (y * 30) / fb_h);
            uint8_t b = (uint8_t)(30 + ((fb_w - x) * 50) / fb_w);

            if (y < PANEL_H) {
                r = 0x1A; g = 0x1A; b = 0x2E;
                uint32_t cx = fb_w - 10;
                if (x >= cx - 56 && x <= cx - 56 + 16 &&
                    y >= 8 && y <= 8 + 16) {
                    r = 0xE7; g = 0x4C; b = 0x3C;
                } else if (x == 7 && y >= 10 && y <= 12) {
                    r = 0xE0; g = 0xE0; b = 0xE0;
                } else if (x == 7 && y >= 16 && y <= 18) {
                    r = 0xE0; g = 0xE0; b = 0xE0;
                }
            }

            fb32[off] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }

    /* Write full framebuffer to surface */
    dj_surface_write(surf_id, compositor_fb, fb_stride * fb_h);

    /* Main compositor loop: re-present periodically */
    uint64_t start = uptime();
    for (;;) {
        dj_present(surf_id);
        while (uptime() - start < 1) { }
        start = uptime();
    }

    return 0;
}
