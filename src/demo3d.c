#include <zirvflux.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define W 1024
#define H 768

static uint32_t fb[W * H];
static zf_buffer_t g_buf;
static zf_display_info_t g_info;
static zf_renderer_t g_r;

static int g_angle = 0;
static uint32_t g_colors[8] = {
    0xFFFF4444, 0xFF44FF44, 0xFF4444FF,
    0xFFFFFF44, 0xFFFF44FF, 0xFF44FFFF,
    0xFF888888, 0xFFFFFFFF,
};

static void draw_ui(void)
{
    /* Simple terminal overlay in top-left corner */
    char *lines[] = {
        "Zirvium 3D Demo",
        "Angle: 000",
        "[click to toggle spin]",
    };
    int line_y = 10;
    for (int i = 0; i < 3; i++) {
        lines[1][7] = '0' + (g_angle / 100) % 10;
        lines[1][8] = '0' + (g_angle / 10) % 10;
        lines[1][9] = '0' + g_angle % 10;
        /* Draw bg bar */
        for (int py = line_y; py < line_y + 14; py++)
            for (int px = 10; px < 210; px++)
                if (px < W && py < H)
                    fb[py * W + px] = 0xCC111111u;
        for (int c = 0; lines[i][c]; c++) {
            uint32_t col = (i == 0) ? 0xFFFFAA00u : 0xFF88CCFFu;
            /* 8x13 font approximation - just render colored rects as pixels */
            for (int my = 0; my < 13; my++)
                for (int mx = 0; mx < 8; mx++) {
                    int px = 12 + c * 9 + mx;
                    int py2 = line_y + my;
                    if (px < W && py2 < H)
                        fb[py2 * W + px] = col;
                }
        }
        line_y += 18;
    }
}

int main(void)
{
    if (zf_connect() != 0) return 1;
    if (zf_get_info(&g_info) != 0 || !g_info.connected) return 1;

    if (zf_create_buffer(W, H, &g_buf) != 0) return 1;

    /* Init 3D renderer */
    zf_renderer_init(&g_r, W, H, fb);
    zf_renderer_set_viewport(&g_r, 0, 0, W, H);

    /* Build cube geometry */
    zf_vertex_t verts[24];
    uint16_t indices[36];
    uint32_t cube_colors[24];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 3; j++) {
            int idx = i * 3 + j;
            cube_colors[idx] = g_colors[i];
        }
    }
    zf_make_cube_verts(verts, 0xFFFF8844);
    int ntri = zf_make_cube_indices(indices);

    zf_present(&g_buf);

    int frames = 0;
    for (;;) {
        /* Clear buffers */
        zf_renderer_clear(&g_r);
        zf_renderer_clear_depth(&g_r);

        /* Update rotation */
        g_angle = (g_angle + 1) % 360;

        /* Build MVP */
        zf_mat4_identity(&g_r.model);
        zf_mat4_translate(&g_r.model, 0, 0, ZF_INT_TO_FP(4));
        zf_mat4_rotate_x(&g_r.model, ZF_INT_TO_FP((g_angle * 2) % 360));
        zf_mat4_rotate_y(&g_r.model, ZF_INT_TO_FP((g_angle * 3) % 360));

        for (int i = 0; i < 24; i++)
            verts[i].color = cube_colors[i];

        zf_renderer_update_mvp(&g_r);
        zf_renderer_tris(&g_r, verts, indices, ntri);

        /* Draw debug grid */
        zf_renderer_draw_grid(&g_r, 40, 0x44000000u);

        /* Overlay UI */
        draw_ui();

        /* Present */
        zf_write_buffer(&g_buf, fb, W * H * 4);
        zf_present(&g_buf);

        frames++;
        __asm__ volatile("pause");
    }
}
