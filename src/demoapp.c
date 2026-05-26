#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <zirvtk.h>

/* ── Constants shared with main.c ──────────────────────────────── */
#define TITLEBAR_H 36

extern uint32_t get_accent_color(void);

/* ── Demo app state ────────────────────────────────────────────── */
#define DEMO_TAB_SHAPES  0
#define DEMO_TAB_3D      1
#define DEMO_TAB_WIDGETS 2
#define DEMO_TAB_ICONS   3

static int g_demo_tab      = 0;
static int g_demo_angle    = 0;
static int g_demo_toggle1  = 1;
static int g_demo_toggle2  = 0;
static int g_demo_slider   = 65;
static int g_demo_btn_pressed = -1;

/* ── Fixed-point math (16.16) for 3D ──────────────────────────── */
#define FP_SHIFT 16
#define FP_ONE   (1 << FP_SHIFT)
#define FP_MUL(a,b)  ((int)((((int64_t)(a)) * (b)) >> FP_SHIFT))
#define FP_DIV(a,b)  ((int)((((int64_t)(a)) << FP_SHIFT) / (b)))

/* sin/cos lookup table, 0-359 degrees */
static int16_t g_sin_tab[360];
static int g_math_inited = 0;

static void init_math(void) {
    if (g_math_inited) return;
    g_math_inited = 1;
    for (int i = 0; i < 360; i++) {
        double rad = i * 3.14159265358979323846 / 180.0;
        double s;
        __asm__("fsin" : "=t"(s) : "0"(rad));
        g_sin_tab[i] = (int16_t)(s * FP_ONE);
    }
}
static int fpsin(int deg) { return g_sin_tab[((deg % 360) + 360) % 360]; }
static int fpcos(int deg) { return g_sin_tab[((90 - (deg % 360)) + 360) % 360]; }

typedef struct { int m[16]; } fp_mat4;

static void fp_identity(fp_mat4 *m) {
    memset(m, 0, sizeof(*m));
    m->m[0] = m->m[5] = m->m[10] = m->m[15] = FP_ONE;
}
static void fp_mul(fp_mat4 *out, const fp_mat4 *a, const fp_mat4 *b) {
    fp_mat4 t;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            int64_t sum = 0;
            for (int k = 0; k < 4; k++)
                sum += (int64_t)a->m[i*4+k] * b->m[k*4+j];
            t.m[i*4+j] = (int)(sum >> FP_SHIFT);
        }
    *out = t;
}
static void fp_rotate_y(fp_mat4 *m, int deg) {
    int c = fpcos(deg), s = fpsin(deg);
    fp_mat4 r; fp_identity(&r);
    r.m[0] = c;        r.m[2] = s;
    r.m[8] = -s;       r.m[10] = c;
    fp_mat4 t; fp_mul(&t, m, &r); *m = t;
}
static void fp_rotate_x(fp_mat4 *m, int deg) {
    int c = fpcos(deg), s = fpsin(deg);
    fp_mat4 r; fp_identity(&r);
    r.m[5] = c;        r.m[6] = -s;
    r.m[9] = s;        r.m[10] = c;
    fp_mat4 t; fp_mul(&t, m, &r); *m = t;
}
static void fp_perspective(fp_mat4 *m, int fov_int, int aspect, int near, int far) {
    int t = FP_MUL(near, fpsin(fov_int / 2));
    int r = FP_MUL(t, aspect);
    memset(m, 0, sizeof(*m));
    m->m[0]  = FP_DIV(FP_MUL(near, FP_ONE), r);
    m->m[5]  = FP_DIV(FP_MUL(near, FP_ONE), t);
    m->m[10] = -FP_DIV(far + near, far - near);
    m->m[11] = -FP_ONE;
    m->m[14] = -FP_DIV(FP_MUL(2 * FP_ONE, FP_MUL(far, near)), far - near);
}
static void fp_look_at(fp_mat4 *m, int ex, int ey, int ez, int cx, int cy, int cz) {
    (void)cx; (void)cy; (void)cz;
    memset(m, 0, sizeof(*m));
    m->m[0] = FP_ONE; m->m[5] = FP_ONE; m->m[10] = FP_ONE; m->m[15] = FP_ONE;
    m->m[12] = -ex; m->m[13] = -ey; m->m[14] = -ez;
}
static void fp_transform(const fp_mat4 *m, int x, int y, int z, int *ox, int *oy) {
    int w = FP_MUL(m->m[3], x) + FP_MUL(m->m[7], y) + FP_MUL(m->m[11], z) + m->m[15];
    if (w == 0) w = 1;
    *ox = FP_DIV(FP_MUL(m->m[0], x) + FP_MUL(m->m[4], y) + FP_MUL(m->m[8], z) + m->m[12], w);
    *oy = FP_DIV(FP_MUL(m->m[1], x) + FP_MUL(m->m[5], y) + FP_MUL(m->m[9], z) + m->m[13], w);
}

/* ── Tab bar ───────────────────────────────────────────────────── */
#define TAB_H 32
#define TABS_Y (TITLEBAR_H + 4)

static const char *g_demo_tab_names[4] = {"Shapes", "3D", "Widgets", "Icons"};

static void draw_tabs(uint32_t *fb, uint32_t w, uint32_t h) {
    int tw = (int)w / 4;
    int pad = 4;
    for (int i = 0; i < 4; i++) {
        int x = i * tw + pad;
        int tw_inner = tw - pad * 2;
        uint32_t col = (i == g_demo_tab) ? 0xFF2A4A6E : 0xFF1E1E38;
        ztk_fb_fill_round_rect(fb, w, h, x, TABS_Y, tw_inner, TAB_H, 8, col);
        ztk_fb_draw_text(fb, w, h, x + (tw_inner - (int)strlen(g_demo_tab_names[i]) * 9) / 2,
                         TABS_Y + (TAB_H - 13) / 2,
                         (const uint8_t *)g_demo_tab_names[i], 0xFFFFFFFF);
    }
}

#define CONTENT_Y (TABS_Y + TAB_H + 6)

/* ── Tab 1: 2D Shapes / Primitives ────────────────────────────── */
static void draw_shapes_tab(uint32_t *fb, uint32_t w, uint32_t h) {
    int y = CONTENT_Y;
    int lx = 120;

    /* 1. fill_rect */
    for (int i = 0; i < 3; i++)
        ztk_fb_fill_rect(fb, w, h, 24 + i * 28, y + i * 12, 24 + i * 8, 40 - i * 10, 0xFFFF4444);
    ztk_fb_draw_text(fb, w, h, lx, y + 2,     (const uint8_t *)"ztk_fb_fill_rect", 0xFF88CCFF);
    ztk_fb_draw_text(fb, w, h, lx, y + 18,    (const uint8_t *)"solid colored rectangles", 0xFF888888);
    y += 50;

    /* 2. fill_circle */
    for (int i = 0; i < 4; i++)
        ztk_fb_fill_circle(fb, w, h, 40 + i * 32, y + 20, 12 + i * 3, 0xFF44FF44);
    ztk_fb_draw_text(fb, w, h, lx, y + 2,     (const uint8_t *)"ztk_fb_fill_circle", 0xFF88CCFF);
    ztk_fb_draw_text(fb, w, h, lx, y + 18,    (const uint8_t *)"filled circles (varying radius)", 0xFF888888);
    y += 50;

    /* 3. fill_round_rect */
    ztk_fb_fill_round_rect(fb, w, h, 24, y, 80, 36, 4, 0xFF4488FF);
    ztk_fb_fill_round_rect(fb, w, h, 110, y, 80, 36, 12, 0xFF8844FF);
    ztk_fb_fill_round_rect(fb, w, h, 196, y, 80, 36, 18, 0xFFFF4488);
    ztk_fb_draw_text(fb, w, h, lx, y + 2,     (const uint8_t *)"ztk_fb_fill_round_rect", 0xFF88CCFF);
    ztk_fb_draw_text(fb, w, h, lx, y + 18,    (const uint8_t *)"rounded corners: r=4, r=12, r=18", 0xFF888888);
    y += 50;

    /* 4. draw_line */
    for (int i = 0; i < 6; i++)
        ztk_fb_draw_line(fb, w, h, 40, y + 20, 40 + i * 12, y + 40 - i * 6, 0xFFFFFF44);
    ztk_fb_draw_text(fb, w, h, lx, y + 2,     (const uint8_t *)"ztk_fb_draw_line", 0xFF88CCFF);
    ztk_fb_draw_text(fb, w, h, lx, y + 18,    (const uint8_t *)"Bresenham line (various slopes)", 0xFF888888);
    y += 50;

    /* 5. fill_gradient_v */
    ztk_fb_fill_gradient_v(fb, w, h, 24, y, 80, 36, 0xFFFF44FF, 0xFF44FFFF);
    ztk_fb_fill_gradient_v(fb, w, h, 110, y, 80, 36, 0xFFFFFF44, 0xFF4444FF);
    ztk_fb_draw_text(fb, w, h, lx, y + 2,     (const uint8_t *)"ztk_fb_fill_gradient_v", 0xFF88CCFF);
    ztk_fb_draw_text(fb, w, h, lx, y + 18,    (const uint8_t *)"vertical color gradients", 0xFF888888);
    y += 50;

    /* 6. draw_text, draw_text_large */
    ztk_fb_draw_text(fb, w, h, 24, y,          (const uint8_t *)"Regular 8x13 text", 0xFFFFFFFF);
    ztk_fb_draw_text_large(fb, w, h, 24, y + 18, (const uint8_t *)"Scale x2", 0xFFFFAA00, 2);
    ztk_fb_draw_text_large(fb, w, h, 24, y + 48, (const uint8_t *)"Scale x3", 0xFF44FFAA, 3);
    ztk_fb_draw_text(fb, w, h, lx, y + 4,     (const uint8_t *)"ztk_fb_draw_text / _large", 0xFF88CCFF);
    ztk_fb_draw_text(fb, w, h, lx, y + 20,    (const uint8_t *)"bitmap font at scale=1,2,3", 0xFF888888);
}

/* ── Tab 2: 3D Wireframe Objects ───────────────────────────────── */
static const int8_t cube_verts[8][3] = {
    {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
    {-1,-1,1},  {1,-1,1},  {1,1,1},  {-1,1,1},
};
static const int cube_edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
};
static const uint32_t cube_edge_cols[12] = {
    0xFFFF4444,0xFFFF8844,0xFFFFFF44,0xFF44FF44,
    0xFF44CCFF,0xFF4444FF,0xFF8844FF,0xFFFF44FF,
    0xFFFF6644,0xFFCCFF44,0xFF44FFAA,0xFF44AAFF
};
static const int8_t tetra_verts[4][3] = {
    {12,12,12}, {12,-12,-12}, {-12,12,-12}, {-12,-12,12}
};
static const int tetra_edges[6][2] = {
    {0,1},{0,2},{0,3},{1,2},{1,3},{2,3}
};

static void draw_3d_shape(uint32_t *fb, uint32_t w, uint32_t h,
                          const fp_mat4 *mvp,
                          const int8_t verts[][3], int nv,
                          const int edges[][2], int ne,
                          const uint32_t *ecol, int cx, int cy, int sz)
{
    int px[16], py[16];
    for (int i = 0; i < nv && i < 16; i++) {
        int vx = verts[i][0] * sz;
        int vy = verts[i][1] * sz;
        int vz = verts[i][2] * sz;
        int ox, oy;
        fp_transform(mvp, vx, vy, vz, &ox, &oy);
        px[i] = cx + ox;
        py[i] = cy - oy;
    }
    for (int i = 0; i < ne; i++) {
        int a = edges[i][0], b = edges[i][1];
        uint32_t col = ecol ? ecol[i] : 0xFFFFAA00;
        ztk_fb_draw_line(fb, w, h, px[a], py[a], px[b], py[b], col);
    }
}

static void draw_3d_tab(uint32_t *fb, uint32_t w, uint32_t h) {
    init_math();

    /* Fill canvas with dark background that incorporates accent color */
    uint32_t accent = get_accent_color();
    /* Extract R/G from accent, dim them heavily for a dark canvas tone */
    uint8_t ar = (uint8_t)(accent >> 16) >> 3;
    uint8_t ag = (uint8_t)(accent >> 8)  >> 3;
    uint8_t ab = (uint8_t)(accent)       >> 3;
    uint32_t canvas = 0xFF000000u | ((uint32_t)(ar > 8 ? ar : 8) << 16)
                     | ((uint32_t)(ag > 8 ? ag : 8) << 8)
                     | (uint32_t)(ab > 8 ? ab : 8);
    /* Also draw a thin accent glow line */
    ztk_fb_fill_rect(fb, w, h, 0, CONTENT_Y, w, h - CONTENT_Y - 50, canvas);
    ztk_fb_fill_rect(fb, w, h, 0, CONTENT_Y, w, 2, accent);

    int cy = CONTENT_Y + 140;
    int cx_l = w / 4;
    int cx_r = w * 3 / 4;

    fp_mat4 proj, view, model, mvp;
    fp_perspective(&proj, 60, 1, 64, 1024);
    fp_look_at(&view, 0, 0, 384, 0, 0, 0);

    /* Cube — use accent-tinted edges */
    fp_identity(&model);
    fp_rotate_y(&model, g_demo_angle);
    fp_rotate_x(&model, (g_demo_angle * 7) / 10);
    fp_mul(&mvp, &view, &model);
    fp_mul(&mvp, &proj, &mvp);

    ztk_fb_draw_text(fb, w, h, cx_l - 24, CONTENT_Y + 6,
                     (const uint8_t *)"Cube (8v 12e 12t)", 0xFF88CCFF);
    draw_3d_shape(fb, w, h, &mvp, cube_verts, 8, cube_edges, 12, cube_edge_cols, cx_l, cy, 100);

    /* Tetrahedron — use accent color for edges */
    fp_identity(&model);
    fp_rotate_y(&model, (g_demo_angle * 13) / 10);
    fp_rotate_x(&model, (g_demo_angle * 5) / 10 + 30);
    fp_mul(&mvp, &view, &model);
    fp_mul(&mvp, &proj, &mvp);

    ztk_fb_draw_text(fb, w, h, cx_r - 32, CONTENT_Y + 6,
                     (const uint8_t *)"Tetrahedron (4v 6e 4t)", 0xFF88CCFF);
    /* Make tetrahedron use bright accent edges, visible against the dark canvas */
    uint32_t tetra_cols[6];
    for (int i = 0; i < 6; i++) {
        uint32_t c = accent;
        /* brighten */
        uint8_t tr = (uint8_t)(c >> 16);
        uint8_t tg = (uint8_t)(c >> 8);
        uint8_t tb = (uint8_t)(c);
        tr = tr + (uint8_t)((255 - tr) * 3 / 4);
        tg = tg + (uint8_t)((255 - tg) * 3 / 4);
        tb = tb + (uint8_t)((255 - tb) * 3 / 4);
        tetra_cols[i] = 0xFF000000u | ((uint32_t)tr << 16) | ((uint32_t)tg << 8) | tb;
    }
    draw_3d_shape(fb, w, h, &mvp, tetra_verts, 4, tetra_edges, 6, tetra_cols, cx_r, cy, 8);

    char buf[64];
    snprintf(buf, sizeof(buf), "Angle: %d  |  auto-rotate enabled", g_demo_angle);
    ztk_fb_draw_text(fb, w, h, 20, h - 28, (const uint8_t *)buf, 0xFF888888);
    ztk_fb_draw_text(fb, w, h, 20, h - 14, (const uint8_t *)"Fixed-point 3D pipeline  |  accent canvas", 0xFF555555);
}

/* ── Tab 3: Widgets / Interactive Controls ─────────────────────── */
static void draw_widgets_tab(uint32_t *fb, uint32_t w, uint32_t h) {
    int y = CONTENT_Y;

    ztk_fb_draw_text(fb, w, h, 20, y, (const uint8_t *)"Interactive UI Widgets", 0xFFFFAA00);
    y += 24;

    /* Toggle 1 */
    ztk_fb_fill_rect(fb, w, h, 20, y, w - 40, 36, 0xFF1A1A3A);
    ztk_fb_draw_text(fb, w, h, 36, y + (36-13)/2, (const uint8_t *)"Toggle Switch A", 0xFFCCCCCC);
    int tog_x = w - 90;
    uint32_t tc = g_demo_toggle1 ? 0xFF339933 : 0xFF772222;
    ztk_fb_fill_round_rect(fb, w, h, tog_x, y + 6, 50, 24, 12, tc);
    ztk_fb_fill_circle(fb, w, h, g_demo_toggle1 ? tog_x + 38 : tog_x + 12, y + 18, 10, 0xFFFFFFFF);
    ztk_fb_draw_text(fb, w, h, tog_x - 40, y + (36-13)/2,
                     g_demo_toggle1 ? (const uint8_t *)"ON" : (const uint8_t *)"OFF",
                     g_demo_toggle1 ? 0xFF44FF44 : 0xFFFF4444);
    y += 44;

    /* Toggle 2 */
    ztk_fb_fill_rect(fb, w, h, 20, y, w - 40, 36, 0xFF1A1A3A);
    ztk_fb_draw_text(fb, w, h, 36, y + (36-13)/2, (const uint8_t *)"Toggle Switch B", 0xFFCCCCCC);
    tc = g_demo_toggle2 ? 0xFF339933 : 0xFF772222;
    ztk_fb_fill_round_rect(fb, w, h, tog_x, y + 6, 50, 24, 12, tc);
    ztk_fb_fill_circle(fb, w, h, g_demo_toggle2 ? tog_x + 38 : tog_x + 12, y + 18, 10, 0xFFFFFFFF);
    ztk_fb_draw_text(fb, w, h, tog_x - 40, y + (36-13)/2,
                     g_demo_toggle2 ? (const uint8_t *)"ON" : (const uint8_t *)"OFF",
                     g_demo_toggle2 ? 0xFF44FF44 : 0xFFFF4444);
    y += 44;

    /* Buttons */
    ztk_fb_draw_text(fb, w, h, 20, y, (const uint8_t *)"Buttons (clickable)", 0xFFFFAA00);
    y += 24;
    const char *btn_labels[3] = {"Action A", "Action B", "Action C"};
    uint32_t btn_cols[3] = {0xFF4488CC, 0xFF44AA66, 0xFFCC6644};
    for (int i = 0; i < 3; i++) {
        int bx = 20 + i * 120;
        uint32_t bc = btn_cols[i];
        if (i == g_demo_btn_pressed) bc = 0xFFFFFFFF;
        ztk_fb_fill_round_rect(fb, w, h, bx, y, 100, 36, 6, bc);
        uint32_t tc2 = (i == g_demo_btn_pressed) ? 0xFF000000 : 0xFFFFFFFF;
        ztk_fb_draw_text(fb, w, h, bx + (100 - (int)strlen(btn_labels[i]) * 8) / 2,
                         y + (36-13)/2, (const uint8_t *)btn_labels[i], tc2);
    }
    y += 50;

    /* Slider */
    ztk_fb_draw_text(fb, w, h, 20, y, (const uint8_t *)"Slider Control", 0xFFFFAA00);
    y += 24;
    int bar_x = 20, bar_w = w - 40;
    ztk_fb_fill_round_rect(fb, w, h, bar_x, y, bar_w, 16, 8, 0xFF222255);
    int fill_w = bar_w * g_demo_slider / 100;
    ztk_fb_fill_round_rect(fb, w, h, bar_x, y, fill_w, 16, 8, 0xFF4488CC);
    ztk_fb_fill_circle(fb, w, h, bar_x + fill_w, y + 8, 10, 0xFFFFFFFF);
    char sval[16];
    snprintf(sval, sizeof(sval), "%d%%", g_demo_slider);
    ztk_fb_draw_text(fb, w, h, bar_x + fill_w - 16, y + (16-13)/2, (const uint8_t *)sval, 0xFFFFFFFF);
    y += 32;

    /* Status / Description */
    ztk_fb_hline(fb, w, h, 20, y, w - 40, 0xFF334455);
    y += 10;
    char status[160];
    snprintf(status, sizeof(status),
             "Status: ToggleA=%s  ToggleB=%s  Slider=%d%%  LastBtn=%d",
             g_demo_toggle1 ? "ON" : "OFF", g_demo_toggle2 ? "ON" : "OFF",
             g_demo_slider, g_demo_btn_pressed);
    ztk_fb_draw_text(fb, w, h, 20, y, (const uint8_t *)status, 0xFF88BB88);
    y += 18;
    ztk_fb_draw_text(fb, w, h, 20, y, (const uint8_t *)"Tab: ZirvTK demo showing interactive widgets rendered with 2D primitives", 0xFF666666);
}

/* ── Tab 4: All Procedural Icons ───────────────────────────────── */
static void draw_icons_tab(uint32_t *fb, uint32_t w, uint32_t h) {
    static const char *icon_names[16] = {
        "Terminal", "Calculator", "Files", "Clock",
        "Settings", "Weather",   "Photos", "Editor",
        "Snake",    "Pong",      "Tetris", "Music",
        "Mail",     "Store",     "Maps",   "About"
    };
    int cols = 4;
    int cell_w = w / cols;
    int cell_h = 100;
    int start_y = CONTENT_Y + 10;
    for (int i = 0; i < 16; i++) {
        int col = i % cols;
        int row = i / cols;
        int cx = col * cell_w + cell_w / 2;
        int cy = start_y + row * cell_h + cell_h / 3;
        ztk_draw_icon(fb, w, h, cx, cy, 36, i, 0xFFFFFFFF);
        int tw = (int)strlen(icon_names[i]) * 8 / 2;
        ztk_fb_draw_text(fb, w, h, cx - tw, cy + 28,
                         (const uint8_t *)icon_names[i], 0xFFAAAAAA);
    }
}

/* ── Main draw function ────────────────────────────────────────── */
void draw_demoapp(uint32_t *fb, uint32_t w, uint32_t h) {
    draw_tabs(fb, w, h);
    switch (g_demo_tab) {
    case DEMO_TAB_SHAPES:  draw_shapes_tab(fb, w, h);  break;
    case DEMO_TAB_3D:      draw_3d_tab(fb, w, h);      break;
    case DEMO_TAB_WIDGETS: draw_widgets_tab(fb, w, h); break;
    case DEMO_TAB_ICONS:   draw_icons_tab(fb, w, h);   break;
    }
    g_demo_angle = (g_demo_angle + 1) % 360;
}

/* ── Hit-testing (called from main.c process_mouse) ───────────── */
int demo_hit_test(int mx, int my, int w) {
    if (my >= TABS_Y && my < TABS_Y + TAB_H) {
        int tw = w / 4;
        for (int i = 0; i < 4; i++) {
            if (mx >= i * tw && mx < (i + 1) * tw) {
                g_demo_tab = i;
                return 1;
            }
        }
    }
    if (g_demo_tab != DEMO_TAB_WIDGETS) return 0;

    int y = CONTENT_Y + 24;

    /* Toggle 1 */
    if (my >= y && my < y + 36) { g_demo_toggle1 = !g_demo_toggle1; return 1; }
    y += 44;

    /* Toggle 2 */
    if (my >= y && my < y + 36) { g_demo_toggle2 = !g_demo_toggle2; return 1; }
    y += 44;

    /* Buttons section header */
    y += 24; y += 24;

    /* Buttons */
    for (int i = 0; i < 3; i++) {
        int bx = 20 + i * 120;
        if (mx >= bx && mx < bx + 100 && my >= y && my < y + 36) {
            g_demo_btn_pressed = i;
            return 1;
        }
    }
    y += 50;

    /* Slider */
    y += 24;
    int bar_x = 20, bar_w = w - 40;
    if (my >= y && my < y + 16) {
        int pct = (mx - bar_x) * 100 / bar_w;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        g_demo_slider = pct;
        return 1;
    }
    return 0;
}
