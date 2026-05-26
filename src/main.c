#include <zirvflux.h>
#include <zirvtk.h>
#include <unistd.h>
#include <stdio.h>
#include <datetime.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

typedef zf_mouse_event_t mouse_event_t;

#define MAX_W      1920
#define MAX_H      1080
#define FONT_W     8
#define FONT_H     13

/* ── Metro color scheme (Windows 8 style) ─────────────────────────────── */
#define METRO_BG         0xFF1A1A2E
#define METRO_BG2        0xFF16213E
#define METRO_TASKBAR    0xFF0F0F1A
#define METRO_ACCENT     0xFF00B7C3
#define METRO_TEXT        0xFFFFFFFF
#define METRO_TEXT_DIM    0xFF8888AA
#define METRO_TILE_TERM  0xFF1E90FF
#define METRO_TILE_CALC  0xFF50C878
#define METRO_TILE_SETT  0xFF6C7A89
#define METRO_TILE_CLOCK 0xFF2C3E50
#define METRO_TILE_EDITR 0xFF8E44AD
#define METRO_TILE_SNAKE 0xFF27AE60
#define METRO_TILE_PONG  0xFF2980B9
#define METRO_TILE_TETRS 0xFFC0392B
#define METRO_TILE_DEMO 0xFF663399
#define METRO_BACK_BTN   0xFF444466
#define METRO_TILE_HOVER 0x22FFFFFF
#define METRO_DIALOG_BG  0xFF1A1A3E

/* ── Wallpaper gradient presets (6 schemes) ──────────────────────────── */
#define NUM_WALLPAPERS 6
static const char *wallpaper_names[NUM_WALLPAPERS] = {
    "Deep Blue", "Twilight", "Forest", "Sunset", "Ocean", "Midnight",
};
/* Each preset: top_r,top_g,top_b, mid_r,mid_g,mid_b, bot_r,bot_g,bot_b */
static const uint8_t wallpaper_colors[NUM_WALLPAPERS][9] = {
    {0x1A,0x1A,0x2E, 0x16,0x21,0x3E, 0x12,0x1A,0x2A}, /* Deep Blue */
    {0x2E,0x1A,0x3E, 0x21,0x16,0x2E, 0x1A,0x12,0x1A}, /* Twilight */
    {0x1A,0x2E,0x1A, 0x16,0x21,0x16, 0x0A,0x1A,0x0A}, /* Forest */
    {0x3E,0x1A,0x16, 0x2E,0x1A,0x12, 0x1A,0x12,0x0A}, /* Sunset */
    {0x16,0x2E,0x3E, 0x12,0x21,0x2E, 0x0A,0x1A,0x1A}, /* Ocean */
    {0x0F,0x0F,0x1A, 0x0A,0x0A,0x14, 0x05,0x05,0x0A}, /* Midnight */
};

/* ── Accent color presets (8 colors) ─────────────────────────────────── */
#define NUM_ACCENTS 8
static const uint32_t accent_colors[NUM_ACCENTS] = {
    0xFF00B7C3, 0xFF0078D4, 0xFFE74C3C, 0xFF2ECC71,
    0xFFF39C12, 0xFF9B59B6, 0xFF1ABC9C, 0xFFE91E63,
};
static const char *accent_names[NUM_ACCENTS] = {
    "Teal", "Blue", "Red", "Green", "Orange", "Purple", "Mint", "Pink",
};

/* ── Layout constants ─────────────────────────────────────────────────── */
#define TILE_SIZE    120
#define TILE_GAP     16
#define TILE_MARGIN  48
#define TILE_COLS    6
#define TASKBAR_H    40
#define TITLEBAR_H   36

/* ── App icons ─────────────────────────────────────────────────────────── */
enum {
    ICON_TERMINAL,
    ICON_CALC,
    ICON_SETTINGS,
    ICON_CLOCK,
    ICON_EDITOR,
    ICON_SNAKE,
    ICON_PONG,
    ICON_TETRIS,
    ICON_DEMO,
    NUM_ICONS,
};

/* ── App definitions ──────────────────────────────────────────────────── */
enum {
    APP_TERMINAL,
    APP_CALCULATOR,
    APP_SETTINGS,
    APP_CLOCK,
    APP_EDITOR,
    APP_SNAKE,
    APP_PONG,
    APP_TETRIS,
    APP_DEMO,
    NUM_APPS,
};

typedef struct {
    const char *name;
    uint32_t color;
    int icon;
} AppDef;

static const AppDef g_apps[NUM_APPS] = {
    {"Terminal",   METRO_TILE_TERM,  ICON_TERMINAL},
    {"Calculator", METRO_TILE_CALC,  ICON_CALC},
    {"Settings",   METRO_TILE_SETT,  ICON_SETTINGS},
    {"Clock",      METRO_TILE_CLOCK, ICON_CLOCK},
    {"Editor",     METRO_TILE_EDITR, ICON_EDITOR},
    {"Snake",      METRO_TILE_SNAKE, ICON_SNAKE},
    {"Pong",       METRO_TILE_PONG,  ICON_PONG},
    {"Tetris",     METRO_TILE_TETRS, ICON_TETRIS},
    {"Demo",       METRO_TILE_DEMO,  ICON_DEMO},
};

/* ── Framebuffer state ────────────────────────────────────────────────── */
static uint8_t compositor_fb[MAX_W * MAX_H * 4];
static uint8_t bg_cache[MAX_W * MAX_H * 4];
static int bg_valid = 0;
static zf_buffer_t g_buf;
static zf_display_info_t g_info;
static int cursor_x = 0, cursor_y = 0;
static int prev_buttons = 0;
static int g_dirty = 1;

/* ── UI state ─────────────────────────────────────────────────────────── */
static int g_active_app = -1;
static int g_tile_hover = -1;
static int g_back_hover = 0;
static int g_font_scale = 1;
static int g_dark_theme = 1;    /* 1=dark, 0=light */
static int g_brightness = 80;   /* 0-100 */
static int g_frame_rate = 60;   /* 30 or 60 */
static int g_wifi_on = 1;
static int g_bt_on = 0;
static int g_volume = 75;       /* 0-100 */
static int g_exit_hover = 0;
static int g_power_hover = 0;
static int g_wallpaper_style = 0; /* 0-5 */
static int g_accent_color = 0;    /* 0-7 */
static int g_anim_frames = 10;    /* replaces g_anim_frames */
static int g_font_style = 0;      /* 0=Regular, 1=Bold */
static int g_settings_tab = 0;    /* 0=Display, 1=Network, 2=Audio, 3=About */

/* ── Tile order, sizes, and layout cache ──────────────────────────── */
static int g_tile_order[NUM_APPS];
static int g_tile_sizes[NUM_APPS]; /* 0=std(1x1), 1=wide(2x1) */
static int g_tile_pos_x[NUM_APPS], g_tile_pos_y[NUM_APPS];
static int g_tile_pos_w[NUM_APPS], g_tile_pos_h[NUM_APPS];
static int g_tile_layout_dirty = 1;

/* ── Tile drag state ────────────────────────────────────────────────── */
static int g_drag_active = 0;
static int g_drag_slot = -1;   /* visual slot being dragged */
static int g_drag_app = -1;    /* app index being dragged */
static int g_drag_cur_x = 0, g_drag_cur_y = 0;
static int g_drag_dst_slot = -1;

/* ── Context menu state ─────────────────────────────────────────────── */
#define CTX_MAX 8
static int g_ctx_active = 0;
static int g_ctx_x, g_ctx_y;
static int g_ctx_count;
static int g_ctx_hover = -1;
static char g_ctx_labels[CTX_MAX][24];
static int g_ctx_app; /* -1 = desktop */

/* ── Shutdown confirmation / animation state ──────────────────────────── */
static int g_confirm_shutdown = 0;   /* 0=off, 1=confirm dialog, 2=animating */
static int g_shutdown_dot = 0;
static uint64_t g_shutdown_start_s = 0;
static int g_shutdown_yes_hover = 0;
static int g_shutdown_no_hover = 0;

/* ── Clock state ──────────────────────────────────────────────────────── */
static int last_minute = -1;
static int last_second = -1;

/* ── Keycode defines (MOSIX/HID usage) ────────────────────────────────── */
#define KEY_ENTER   0x28
#define KEY_BSPACE  0x2A
#define KEY_TAB     0x2B
#define KEY_SPACE   0x2C
#define KEY_UP      0x52
#define KEY_DOWN    0x51
#define KEY_LEFT    0x50
#define KEY_RIGHT   0x4F
#define KEY_V       0x19
#define MOD_LSHIFT  1
#define MOD_RSHIFT  2
#define MOD_LCTRL   4
#define MOD_RCTRL   8
#define MOD_CAPS    64

/* ── Calculator state ─────────────────────────────────────────────────── */
typedef struct {
    int accum;
    int display;
    char op;
    int new_input;
    int has_op;
    char disp_str[16];
} CalcState;

static CalcState g_calc;
static int g_calc_btn_hover = -1;

/* ── Terminal state ───────────────────────────────────────────────────── */
#define TERM_SCROLL 64
#define TERM_LINE_LEN 128

typedef struct {
    char scroll[TERM_SCROLL][TERM_LINE_LEN];
    int scroll_count;
    char input[256];
    int input_len;
} TermState;

static TermState g_term;

/* ── Editor state ─────────────────────────────────────────────────────── */
#define EDITOR_ROWS 32
#define EDITOR_COLS 80
typedef struct {
    char lines[EDITOR_ROWS][EDITOR_COLS + 1];
    int row;
    int col;
    int num_lines;
    int scroll_row;
} EditorState;
static EditorState g_editor;

/* ── Snake state ──────────────────────────────────────────────────────── */
#define SNAKE_MAX 256
#define SNAKE_COLS 40
#define SNAKE_ROWS 30
typedef struct {
    int seg_x[SNAKE_MAX];
    int seg_y[SNAKE_MAX];
    int seg_len;
    int dir; /* 0=up,1=down,2=left,3=right */
    int next_dir;
    int food_x, food_y;
    int score;
    int gameover;
    int tick;
} SnakeState;
static SnakeState g_snake;

/* ── Pong state ────────────────────────────────────────────────────────── */
typedef struct {
    float ball_x, ball_y, ball_dx, ball_dy;
    float paddle_y;
    int player_score;
    int ai_score;
    int gameover;
    int tick;
} PongState;
static PongState g_pong;

/* ── Tetris state ──────────────────────────────────────────────────────── */
#define TETRIS_COLS 10
#define TETRIS_ROWS 20
typedef struct {
    char grid[TETRIS_ROWS][TETRIS_COLS];
    int piece_type;   /* 0-6 */
    int piece_rot;    /* 0-3 */
    int piece_x, piece_y;
    int next_piece;
    int score;
    int lines;
    int gameover;
    int tick;
} TetrisState;
static TetrisState g_tetris;

static void term_add_scroll(const char *s) {
    if (g_term.scroll_count < TERM_SCROLL) {
        strcpy(g_term.scroll[g_term.scroll_count], s);
        g_term.scroll_count++;
    } else {
        memmove(g_term.scroll, g_term.scroll + 1,
                (size_t)(TERM_SCROLL - 1) * TERM_LINE_LEN);
        strcpy(g_term.scroll[TERM_SCROLL - 1], s);
    }
}

static int keycode_to_ascii(uint16_t kc, uint8_t mods) {
    int shift = (mods & (MOD_LSHIFT | MOD_RSHIFT)) ? 1 : 0;
    int caps  = (mods & MOD_CAPS) ? 1 : 0;

    if (kc >= 0x04 && kc <= 0x1D) {
        char c = 'a' + (int)(kc - 0x04);
        if (shift ^ caps) c -= 0x20;
        return c;
    }
    if (kc >= 0x1E && kc <= 0x26) {
        const char shifted[] = "!@#$%^&*(";
        if (shift) return shifted[kc - 0x1E];
        return '1' + (int)(kc - 0x1E);
    }
    if (kc == 0x27) return shift ? ')' : '0';
    if (kc == KEY_ENTER) return '\n';
    if (kc == KEY_BSPACE) return '\b';
    if (kc == KEY_TAB) return '\t';
    if (kc == KEY_SPACE) return ' ';

    switch (kc) {
    case 0x2D: return shift ? '_' : '-';
    case 0x2E: return shift ? '+' : '=';
    case 0x2F: return shift ? '{' : '[';
    case 0x30: return shift ? '}' : ']';
    case 0x31: return shift ? '|' : '\\';
    case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\'';
    case 0x35: return shift ? '~' : '`';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    }
    return 0;
}

static void term_run_shell(void) {
    if (g_term.input_len == 0) return;

    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) return;

    int old_stdout = stdout_pipe[0];

    char cmd[256];
    int cmd_len = g_term.input_len;
    memcpy(cmd, g_term.input, (size_t)cmd_len);
    cmd[cmd_len] = '\0';

    term_add_scroll(cmd);
    g_term.input[0] = '\0';
    g_term.input_len = 0;

    write(stdin_pipe[1], cmd, (size_t)cmd_len);
    write(stdin_pipe[1], "\n", 1);
    write(stdin_pipe[1], "exit\n", 5);
    close(stdin_pipe[1]);

    dup2(stdin_pipe[0], 0);
    close(stdin_pipe[0]);
    dup2(stdout_pipe[1], 1);
    close(stdout_pipe[1]);

    char *argv[] = {"shell", NULL};
    g_dirty = 1;
    int ret = execve("/bin/shell", argv, NULL);

    if (ret < 0) {
        close(1);
        close(old_stdout);
        close(0);
        term_add_scroll("error: shell not found");
        return;
    }

    /* Shell exited — read output from stdout pipe */
    close(1);

    char buf[4096];
    int total = 0, n;
    while ((n = read(old_stdout, buf + total, (int)sizeof(buf) - 1 - total)) > 0) {
        total += n;
        if (total >= (int)sizeof(buf) - 1) break;
    }

    if (total > 0) {
        buf[total] = '\0';
        char *line = buf;
        char *nl;
        while ((nl = strchr(line, '\n')) != NULL) {
            *nl = '\0';
            term_add_scroll(line);
            line = nl + 1;
        }
        if (*line) term_add_scroll(line);
    }
    close(old_stdout);
    close(0);
}

/* ── Tetris piece shapes (4 rotations each) ──────────────────────────── */
static const int tetris_pieces[7][4][4] = {
    {{1,1,1,1},{0,0,0,0},{0,0,0,0},{0,0,0,0}}, /* I */
    {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}, /* O */
    {{1,1,1,0},{0,1,0,0},{0,0,0,0},{0,0,0,0}}, /* T */
    {{1,1,1,0},{1,0,0,0},{0,0,0,0},{0,0,0,0}}, /* L */
    {{1,1,1,0},{0,0,1,0},{0,0,0,0},{0,0,0,0}}, /* J */
    {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}, /* S */
    {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}, /* Z */
};
static const int tetris_colors[] = {
    0xFF00FFFF, 0xFFFFFF00, 0xFFAA00FF, 0xFFFF8800,
    0xFF0000FF, 0xFF00FF00, 0xFFFF0000,
};

/* ── Game initialization helpers ──────────────────────────────────────── */
static void editor_reset(void) {
    memset(&g_editor, 0, sizeof(g_editor));
    g_editor.num_lines = 1;
}

static void snake_reset(void) {
    g_snake.seg_len = 3;
    g_snake.seg_x[0] = SNAKE_COLS / 2; g_snake.seg_y[0] = SNAKE_ROWS / 2;
    g_snake.seg_x[1] = SNAKE_COLS / 2 - 1; g_snake.seg_y[1] = SNAKE_ROWS / 2;
    g_snake.seg_x[2] = SNAKE_COLS / 2 - 2; g_snake.seg_y[2] = SNAKE_ROWS / 2;
    g_snake.dir = 3; /* right */
    g_snake.next_dir = 3;
    g_snake.score = 0;
    g_snake.gameover = 0;
    g_snake.tick = 0;
    /* place food */
    g_snake.food_x = SNAKE_COLS * 3 / 4;
    g_snake.food_y = SNAKE_ROWS / 4;
}

static void pong_reset(void) {
    g_pong.ball_x = 0.5f; g_pong.ball_y = 0.5f;
    g_pong.ball_dx = 0.02f; g_pong.ball_dy = 0.015f;
    g_pong.paddle_y = 0.4f;
    g_pong.player_score = 0;
    g_pong.ai_score = 0;
    g_pong.gameover = 0;
    g_pong.tick = 0;
}

static void tetris_reset(void) {
    memset(&g_tetris, 0, sizeof(g_tetris));
    g_tetris.piece_type = 0;
    g_tetris.piece_rot = 0;
    g_tetris.piece_x = 3;
    g_tetris.piece_y = 0;
    g_tetris.next_piece = 1;
    g_tetris.score = 0;
    g_tetris.lines = 0;
    g_tetris.gameover = 0;
    g_tetris.tick = 0;
}

/* ── Animation state ──────────────────────────────────────────────────── */
typedef struct {
    int active;
    int opening;
    int app_idx;
    int frame;
    int tile_x, tile_y, tile_w, tile_h;
    int scr_w, scr_h;
} AnimState;

static AnimState g_anim;

/* ── Smoothstep table (0..256) ────────────────────────────────────────── */
static int smoothstep(int t) {
    if (t <= 0) return 0;
    if (t >= 256) return 256;
    return (t * t * (768 - (t * 2))) / (256 * 256);
}

uint32_t get_accent_color(void) {
    return accent_colors[g_accent_color];
}

/* ── Drawing helpers ──────────────────────────────────────────────────── */
static uint32_t blend(uint32_t fg, uint32_t bg, uint8_t alpha) {
    return ztk_fb_blend(fg, bg, alpha);
}

static void fill_rect(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x, int y, int w, int h, uint32_t color) {
    ztk_fb_fill_rect(fb, fb_w, fb_h, x, y, (uint32_t)(w > 0 ? w : 0), (uint32_t)(h > 0 ? h : 0), color);
}

/* ── 8x13 bitmap font ─────────────────────────────────────────────────── */
#include "font_8x13.h"

static uint8_t font_8x13_bold[95][13];
static int font_bold_inited = 0;

static void font_init_bold(void) {
    if (font_bold_inited) return;
    font_bold_inited = 1;
    for (int i = 0; i < 95; i++) {
        for (int r = 0; r < 13; r++) {
            uint8_t b = font_8x13[i][r];
            font_8x13_bold[i][r] = b | (b >> 1) | ((b & 1) ? 0x80 : 0);
        }
    }
}

static const uint8_t *font_get(char c) {
    int idx = (c >= 32 && c <= 126) ? (c - 32) : 0;
    if (g_font_style == 1) {
        font_init_bold();
        return font_8x13_bold[idx];
    }
    return font_8x13[idx];
}

static void draw_char(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x, int y, char c, uint32_t color) {
    const uint8_t *bm = font_get(c);
    for (int row = 0; row < FONT_H && (y + row) < (int)fb_h; row++) {
        uint8_t bits = bm[row];
        if (bits == 0) continue;
        for (int col = 0; col < FONT_W && (x + col) < (int)fb_w; col++) {
            if (bits & (1 << (7 - col))) {
                int px = x + col, py = y + row;
                if (px < 0 || py < 0) continue;
                uint32_t off = (uint32_t)py * fb_w + (uint32_t)px;
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

static void draw_char_scaled(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                             int x, int y, char c, uint32_t color) {
    ztk_fb_draw_char_scaled(fb, fb_w, fb_h, x, y, (uint8_t)c, color, (uint32_t)g_font_scale);
}

static void draw_text(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x, int y, const char *s, uint32_t color) {
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

static void draw_clock_text(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                            int x, int y, uint32_t color) {
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

/* ── Tile geometry ────────────────────────────────────────────────────── */
static void recompute_tile_layout(void) {
    if (!g_tile_layout_dirty) return;
    int col = 0, row = 0;
    for (int slot = 0; slot < NUM_APPS; slot++) {
        int app = g_tile_order[slot];
        int size = g_tile_sizes[app];
        int tw = (size == 0) ? TILE_SIZE : TILE_SIZE * 2 + TILE_GAP;
        int th = TILE_SIZE;
        if (col + (tw / TILE_SIZE) > TILE_COLS) {
            col = 0;
            row++;
        }
        g_tile_pos_x[slot] = TILE_MARGIN + col * (TILE_SIZE + TILE_GAP);
        g_tile_pos_y[slot] = TILE_MARGIN + row * (TILE_SIZE + TILE_GAP);
        g_tile_pos_w[slot] = tw;
        g_tile_pos_h[slot] = th;
        col += tw / TILE_SIZE;
    }
    g_tile_layout_dirty = 0;
}

static void get_tile_rect(int idx, int *x, int *y, int *w, int *h) {
    recompute_tile_layout();
    *x = g_tile_pos_x[idx];
    *y = g_tile_pos_y[idx];
    *w = g_tile_pos_w[idx];
    *h = g_tile_pos_h[idx];
}

static int point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static int app_slot(int app_idx) {
    for (int i = 0; i < NUM_APPS; i++)
        if (g_tile_order[i] == app_idx) return i;
    return -1;
}

/* ── Context menu ──────────────────────────────────────────────────────── */
static void ctx_add(const char *label) {
    if (g_ctx_count >= CTX_MAX) return;
    int n = 0;
    while (label[n] && n < 23) {
        g_ctx_labels[g_ctx_count][n] = label[n];
        n++;
    }
    g_ctx_labels[g_ctx_count][n] = '\0';
    g_ctx_count++;
}

static void start_context_menu(int mx, int my, int app_idx) {
    g_ctx_app = app_idx;
    g_ctx_x = mx;
    g_ctx_y = my;
    g_ctx_count = 0;
    g_ctx_hover = -1;

    if (app_idx >= 0) {
        ctx_add("Open");
        ctx_add("Resize");
        ctx_add("Move Left");
        ctx_add("Move Right");
        ctx_add("Send to Front");
        ctx_add("Send to End");
    } else {
        ctx_add("Settings");
        ctx_add("Refresh Wallpaper");
    }
    g_ctx_active = 1;
    g_dirty = 1;
}

static void ctx_execute(int item) {
    if (item < 0 || item >= g_ctx_count) return;
    const char *label = g_ctx_labels[item];
    int slot = (g_ctx_app >= 0) ? app_slot(g_ctx_app) : -1;

    if (g_ctx_app >= 0) {
        if (strcmp(label, "Open") == 0) {
            g_active_app = g_ctx_app;
            g_dirty = 1;
        } else if (strcmp(label, "Resize") == 0) {
            g_tile_sizes[g_ctx_app] = g_tile_sizes[g_ctx_app] ? 0 : 1;
            g_tile_layout_dirty = 1;
            g_dirty = 1;
        } else if (strcmp(label, "Move Left") == 0 && slot > 0) {
            int tmp = g_tile_order[slot];
            g_tile_order[slot] = g_tile_order[slot - 1];
            g_tile_order[slot - 1] = tmp;
            g_tile_layout_dirty = 1;
            g_dirty = 1;
        } else if (strcmp(label, "Move Right") == 0 && slot < NUM_APPS - 1) {
            int tmp = g_tile_order[slot];
            g_tile_order[slot] = g_tile_order[slot + 1];
            g_tile_order[slot + 1] = tmp;
            g_tile_layout_dirty = 1;
            g_dirty = 1;
        } else if (strcmp(label, "Send to Front") == 0) {
            int tmp = g_tile_order[slot];
            for (int i = slot; i > 0; i--)
                g_tile_order[i] = g_tile_order[i - 1];
            g_tile_order[0] = tmp;
            g_tile_layout_dirty = 1;
            g_dirty = 1;
        } else if (strcmp(label, "Send to End") == 0) {
            int tmp = g_tile_order[slot];
            for (int i = slot; i < NUM_APPS - 1; i++)
                g_tile_order[i] = g_tile_order[i + 1];
            g_tile_order[NUM_APPS - 1] = tmp;
            g_tile_layout_dirty = 1;
            g_dirty = 1;
        }
    } else {
        if (strcmp(label, "Settings") == 0) {
            g_active_app = APP_SETTINGS;
            g_dirty = 1;
        } else if (strcmp(label, "Refresh Wallpaper") == 0) {
            g_wallpaper_style = (g_wallpaper_style + 1) % NUM_WALLPAPERS;
            bg_valid = 0;
            g_dirty = 1;
        }
    }
    g_ctx_active = 0;
}

static void draw_context_menu(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_ctx_active) return;
    int mw = 160, mh = g_ctx_count * 24 + 8;
    int mx = g_ctx_x;
    int my = g_ctx_y;
    if (mx + mw > fb_w) mx = fb_w - mw - 4;
    if (my + mh > fb_h) my = fb_h - mh - 4;
    uint32_t col_accent = get_accent_color();
    uint32_t bg = 0xE0202020;
    uint32_t border = col_accent;
    uint32_t text_col = 0xFFE0E0E0;
    uint32_t hover_bg = (col_accent & 0x00FFFFFF) | 0x40000000;

    fill_rect(fb, fb_w, fb_h, mx, my, mw, mh, bg);
    fill_rect(fb, fb_w, fb_h, mx, my, mw, 1, border);
    fill_rect(fb, fb_w, fb_h, mx, my + mh - 1, mw, 1, border);
    fill_rect(fb, fb_w, fb_h, mx, my, 1, mh, border);
    fill_rect(fb, fb_w, fb_h, mx + mw - 1, my, 1, mh, border);

    for (int i = 0; i < g_ctx_count; i++) {
        int iy = my + 4 + i * 24;
        if (i == g_ctx_hover)
            fill_rect(fb, fb_w, fb_h, mx + 2, iy, mw - 4, 24, hover_bg);
        draw_text(fb, fb_w, fb_h, mx + 8, iy + 4, g_ctx_labels[i], text_col);
    }
}

/* ── Forward declarations ─────────────────────────────────────────────── */
static void draw_line(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x0, int y0, int x1, int y1, uint32_t color);
extern void draw_demoapp(uint32_t *fb, uint32_t w, uint32_t h);
extern int demo_hit_test(int mx, int my, int w);

/* ── Procedural app icons ─────────────────────────────────────────────── */
static void draw_icon(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int cx, int cy, int size, int icon_type, uint32_t color) {
    switch (icon_type) {
        case ICON_TERMINAL: {
            int s = size / 4;
            fill_rect(fb, fb_w, fb_h, cx - s, cy - s, s * 2, s * 2, 0xFF1A1A2E);
            draw_char(fb, fb_w, fb_h, cx - 4, cy - 4, '>', color);
            draw_char(fb, fb_w, fb_h, cx + 4, cy - 4, '_', color);
            break;
        }
        case ICON_CALC: {
            int dot = size / 8;
            if (dot < 2) dot = 2;
            int gap = dot + 2;
            int start_x = cx - gap;
            int start_y = cy - gap;
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    int px = start_x + c * gap;
                    int py = start_y + r * gap;
                    fill_rect(fb, fb_w, fb_h, px, py, dot, dot, color);
                }
            }
            break;
        }
        case ICON_SETTINGS: {
            int r = size / 3;
            for (int i = 0; i < 4; i++) {
                int angle = i * 90;
                (void)angle;
            }
            fill_rect(fb, fb_w, fb_h, cx - r / 2, cy - r/6, r, r/3, color);
            fill_rect(fb, fb_w, fb_h, cx - r/6, cy - r / 2, r/3, r, color);
            int cir = size / 5;
            int cir2 = size / 8;
            for (int dy = -cir; dy <= cir; dy++) {
                for (int dx = -cir; dx <= cir; dx++) {
                    if (dx * dx + dy * dy <= cir * cir && dx * dx + dy * dy >= cir2 * cir2) {
                        int px = cx + dx, py = cy + dy;
                        if (px >= 0 && py >= 0 && px < (int)fb_w && py < (int)fb_h)
                            fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
                    }
                }
            }
            break;
        }
        case ICON_CLOCK: {
            int r = size / 3;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (dx * dx + dy * dy <= r * r && dx * dx + dy * dy >= (r - 2) * (r - 2)) {
                        int px = cx + dx, py = cy + dy;
                        if (px >= 0 && py >= 0 && px < (int)fb_w && py < (int)fb_h)
                            fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
                    }
                }
            }
            fill_rect(fb, fb_w, fb_h, cx - 1, cy - r / 2, 2, r / 2 + 2, color);
            fill_rect(fb, fb_w, fb_h, cx, cy, r / 3, 2, color);
            break;
        }
        case ICON_EDITOR: {
            int s = size / 3;
            draw_line(fb, fb_w, fb_h, cx - s, cy - s, cx - s, cy + s, color);
            draw_line(fb, fb_w, fb_h, cx + s, cy - s, cx + s, cy + s, color);
            for (int i = -1; i <= 1; i++)
                draw_line(fb, fb_w, fb_h, cx - s + 1, cy + i * s / 2, cx + s - 1, cy + i * s / 2, color);
            break;
        }
        case ICON_SNAKE: {
            int s = size / 4;
            fill_rect(fb, fb_w, fb_h, cx - s, cy, s * 2, s, color);
            fill_rect(fb, fb_w, fb_h, cx - s, cy - s * 2, s * 2, s, color);
            fill_rect(fb, fb_w, fb_h, cx + s, cy - s * 2, s, s * 3, color);
            fill_rect(fb, fb_w, fb_h, cx - s, cy - s, s, s, color);
            break;
        }
        case ICON_PONG: {
            int s = size / 4;
            fill_rect(fb, fb_w, fb_h, cx - s * 3, cy - s * 2, s / 2, s * 4, color);
            fill_rect(fb, fb_w, fb_h, cx + s * 3 - s / 2, cy - s * 2, s / 2, s * 4, color);
            fill_rect(fb, fb_w, fb_h, cx - s / 4, cy - s / 4, s / 2, s / 2, color);
            break;
        }
        case ICON_TETRIS: {
            int s = size / 5;
            fill_rect(fb, fb_w, fb_h, cx - s, cy - s * 2, s * 2, s, color);
            fill_rect(fb, fb_w, fb_h, cx - s, cy - s, s, s * 3, color);
            fill_rect(fb, fb_w, fb_h, cx, cy, s * 2, s, color);
            fill_rect(fb, fb_w, fb_h, cx + s, cy + s, s * 2, s, color);
            break;
        }
        case ICON_DEMO: {
            int r = size / 3;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (dx * dx + dy * dy <= r * r) {
                        int px = cx + dx, py = cy + dy;
                        if (px >= 0 && py >= 0 && px < (int)fb_w && py < (int)fb_h)
                            fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
                    }
                }
            }
            draw_line(fb, fb_w, fb_h, cx - r/2, cy - r/2, cx + r/2, cy + r/2, 0xFF1A1A2E);
            draw_line(fb, fb_w, fb_h, cx + r/2, cy - r/2, cx - r/2, cy + r/2, 0xFF1A1A2E);
            break;
        }
        default:
            break;
    }
}

static void draw_line(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x0, int y0, int x1, int y1, uint32_t color) {
    ztk_fb_draw_line(fb, fb_w, fb_h, x0, y0, x1, y1, color);
}

/* ── Background rendering ─────────────────────────────────────────────── */
static void render_bg(uint32_t *fb, uint32_t w, uint32_t h) {
    if (bg_valid) {
        memcpy(fb, bg_cache, w * h * 4);
        return;
    }
    int wp = g_wallpaper_style;
    if (wp < 0) wp = 0;
    if (wp >= NUM_WALLPAPERS) wp = 0;
    const uint8_t *wc = wallpaper_colors[wp];
    for (uint32_t y = 0; y < h; y++) {
        if (g_dark_theme) {
            uint32_t top_h = h * 2 / 3;
            uint32_t c;
            if (y < top_h) {
                uint8_t r = (uint8_t)(wc[0] + ((int)(wc[3] - wc[0]) * (int)y) / (int)top_h);
                uint8_t g = (uint8_t)(wc[1] + ((int)(wc[4] - wc[1]) * (int)y) / (int)top_h);
                uint8_t b = (uint8_t)(wc[2] + ((int)(wc[5] - wc[2]) * (int)y) / (int)top_h);
                c = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            } else {
                uint32_t dy = y - top_h;
                uint32_t bh = h - top_h;
                uint8_t r = (uint8_t)(wc[3] + ((int)(wc[6] - wc[3]) * (int)dy) / (int)bh);
                uint8_t g = (uint8_t)(wc[4] + ((int)(wc[7] - wc[4]) * (int)dy) / (int)bh);
                uint8_t b = (uint8_t)(wc[5] + ((int)(wc[8] - wc[5]) * (int)dy) / (int)bh);
                c = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
            for (uint32_t x = 0; x < w; x++)
                fb[y * w + x] = c;
        } else {
            uint8_t v = (uint8_t)(0xD0 + ((0xF0 - 0xD0) * y) / h);
            uint32_t c = 0xFF000000u | ((uint32_t)v << 16) | ((uint32_t)v << 8) | (uint32_t)v;
            for (uint32_t x = 0; x < w; x++)
                fb[y * w + x] = c;
        }
    }
    if (!bg_valid) {
        memcpy(bg_cache, fb, w * h * 4);
        bg_valid = 1;
    }
}

/* ── Start screen tiles ────────────────────────────────────────────────── */
static void draw_tile(uint32_t *fb, uint32_t w, uint32_t h,
                      int slot, int hovered) {
    int app = g_tile_order[slot];
    int tx, ty, tw, th;
    get_tile_rect(slot, &tx, &ty, &tw, &th);
    uint32_t color = g_apps[app].color;
    if (hovered) color = blend(0x44FFFFFF, color, 40);
    fill_rect(fb, w, h, tx, ty, tw, th, color);

    int icon_size = tw / 3;
    int icon_cx = tx + tw / 2;
    int icon_cy = ty + th / 2 - 6;
    draw_icon(fb, w, h, icon_cx, icon_cy, icon_size, g_apps[app].icon, METRO_TEXT);

    int name_y = ty + th - FONT_H - 6;
    const char *name = g_apps[app].name;
    int name_w = (int)strlen(name) * (FONT_W + 1);
    draw_text(fb, w, h, tx + (tw - name_w * g_font_scale) / 2, name_y, name, METRO_TEXT);
}

static void draw_start_screen(uint32_t *fb, uint32_t w, uint32_t h) {
    render_bg(fb, w, h);

    for (int slot = 0; slot < NUM_APPS; slot++) {
        int app = g_tile_order[slot];
        if (g_anim.active && g_anim.opening && g_anim.app_idx == app) continue;
        if (g_anim.active && !g_anim.opening && g_anim.app_idx == app) {
            int anim_slot = app_slot(app);
            if (anim_slot < 0) { draw_tile(fb, w, h, slot, 0); continue; }
            int tw = (int)w, th = (int)h;
            int tx = 0, ty = 0;
            int progress = (g_anim.frame * 256) / g_anim_frames;
            int eased = smoothstep(256 - progress);
            int cx = g_anim.tile_x + (tx - g_anim.tile_x) * eased / 256;
            int cy = g_anim.tile_y + (ty - g_anim.tile_y) * eased / 256;
            int cw = g_anim.tile_w + (tw - g_anim.tile_w) * eased / 256;
            int ch = g_anim.tile_h + (th - g_anim.tile_h) * eased / 256;
            uint32_t color = g_apps[app].color;
            fill_rect(fb, w, h, cx, cy, cw, ch, color);
            int icon_size = cw / 3;
            draw_icon(fb, w, h, cx + cw / 2, cy + ch / 2 - 8, icon_size, g_apps[app].icon, METRO_TEXT);
            int name_y = cy + ch - FONT_H - 8;
            const char *name = g_apps[app].name;
            int name_w = (int)strlen(name) * (FONT_W + 1);
            draw_text(fb, w, h, cx + (cw - name_w * g_font_scale) / 2, name_y, name, METRO_TEXT);
        } else {
            draw_tile(fb, w, h, slot, g_tile_hover == slot);
        }
    }
}

/* ── App renderers ────────────────────────────────────────────────────── */
static void draw_terminal(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, 0, (int)w, (int)h, 0xFF0D0D1A);
    int y = TITLEBAR_H + 10;
    int max_lines = ((int)h - TITLEBAR_H - 10) / (FONT_H + 2);
    int start = (g_term.scroll_count > max_lines)
                ? g_term.scroll_count - max_lines : 0;
    for (int i = start; i < g_term.scroll_count; i++) {
        draw_text(fb, w, h, 12, y, g_term.scroll[i], 0xFF00FF00);
        y += FONT_H + 2;
    }

    /* Prompt + input line */
    char prompt[280];
    int plen = snprintf(prompt, sizeof(prompt), "zirvium:~$ %s",
                        g_term.input);
    if (plen < 0 || plen >= (int)sizeof(prompt)) plen = (int)sizeof(prompt) - 1;
    prompt[plen] = '\0';
    draw_text(fb, w, h, 12, y, prompt, 0xFF00FF00);

    /* Cursor */
    int cw = (int)strlen("zirvium:~$ ") * (FONT_W + 1);
    int ci = (int)strlen(g_term.input);
    int cx = 12 + (cw + ci * (FONT_W + 1)) * g_font_scale;
    fill_rect(fb, w, h, cx, y, 6 * g_font_scale, FONT_H * g_font_scale, 0xFF00FF00);
}

static void calc_reset(void) {
    g_calc.accum = 0;
    g_calc.display = 0;
    g_calc.op = 0;
    g_calc.new_input = 1;
    g_calc.has_op = 0;
    strcpy(g_calc.disp_str, "0");
}

static void calc_press(char btn) {
    if (btn >= '0' && btn <= '9') {
        if (g_calc.new_input) {
            g_calc.display = btn - '0';
            g_calc.new_input = 0;
        } else {
            g_calc.display = g_calc.display * 10 + (btn - '0');
        }
        if (g_calc.display > 99999999) g_calc.display = 99999999;
    } else if (btn == 'C') {
        calc_reset();
    } else if (btn == '=' || btn == '+' || btn == '-' || btn == '*' || btn == '/') {
        if (g_calc.has_op) {
            if (g_calc.op == '+') g_calc.accum += g_calc.display;
            else if (g_calc.op == '-') g_calc.accum -= g_calc.display;
            else if (g_calc.op == '*') g_calc.accum *= g_calc.display;
            else if (g_calc.op == '/') {
                if (g_calc.display != 0) g_calc.accum /= g_calc.display;
                else g_calc.accum = 0;
            }
            g_calc.display = g_calc.accum;
        } else {
            g_calc.accum = g_calc.display;
        }
        g_calc.op = btn;
        g_calc.has_op = 1;
        g_calc.new_input = 1;
    }
    {
        char buf[20];
        int n = snprintf(buf, 20, "%d", g_calc.display);
        if (n > 0 && n < 20) strcpy(g_calc.disp_str, buf);
        else strcpy(g_calc.disp_str, "0");
    }
}

static void draw_calculator(uint32_t *fb, uint32_t w, uint32_t h) {
    uint32_t bg = 0xFF1A1A2E;
    uint32_t display_bg = 0xFF0D0D1A;
    uint32_t btn_bg = 0xFF2A2A4E;
    uint32_t btn_op = 0xFF1E3A5F;
    uint32_t btn_eq = 0xFF0078D4;
    uint32_t btn_clr = 0xFF8B0000;
    uint32_t btn_hov = 0xFF3A3A6E;

    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, bg);

    int bw = (int)w / 4;
    int bh = 50;
    int start_x = 0;
    int start_y = TITLEBAR_H + 80;

    struct { const char *label; int row; int col; int wide; uint32_t color; } buttons[] = {
        {"C",  0, 0, 0, btn_clr},
        {"(",  0, 1, 0, btn_bg},
        {")",  0, 2, 0, btn_bg},
        {"/",  0, 3, 0, btn_op},
        {"7",  1, 0, 0, btn_bg},
        {"8",  1, 1, 0, btn_bg},
        {"9",  1, 2, 0, btn_bg},
        {"*",  1, 3, 0, btn_op},
        {"4",  2, 0, 0, btn_bg},
        {"5",  2, 1, 0, btn_bg},
        {"6",  2, 2, 0, btn_bg},
        {"-",  2, 3, 0, btn_op},
        {"1",  3, 0, 0, btn_bg},
        {"2",  3, 1, 0, btn_bg},
        {"3",  3, 2, 0, btn_bg},
        {"+",  3, 3, 0, btn_op},
        {"0",  4, 0, 0, btn_bg},
        {".",  4, 1, 0, btn_bg},
        {"=",  4, 2, 1, btn_eq},
        {"",   0, 0, 0, 0},
    };

    fill_rect(fb, w, h, 4, TITLEBAR_H + 10, (int)w - 8, 60, display_bg);
    draw_text(fb, w, h, 16, TITLEBAR_H + 28, g_calc.disp_str, 0xFF00FF88);

    int num_btns = (int)(sizeof(buttons) / sizeof(buttons[0])) - 1;
    for (int i = 0; i < num_btns; i++) {
        int bx = start_x + buttons[i].col * bw;
        int by = start_y + buttons[i].row * bh;
        int bww = buttons[i].wide ? bw * 2 : bw;
        int hovered = (g_calc_btn_hover == i);
        uint32_t bc = hovered ? btn_hov : buttons[i].color;
        fill_rect(fb, w, h, bx + 2, by + 2, bww - 4, bh - 4, bc);
        int lx = bx + (bww - (int)strlen(buttons[i].label) * (FONT_W + 1)) / 2;
        int ly = by + (bh - FONT_H) / 2;
        draw_text(fb, w, h, lx, ly, buttons[i].label, METRO_TEXT);
    }
}

/* ── Clock App ──────────────────────────────────────────────────────────── */
static void draw_clock_app(uint32_t *fb, uint32_t w, uint32_t h) {
    uint32_t bg = g_dark_theme ? 0xFF1A1A2E : 0xFFE8E8F0;
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, bg);
    struct datetime dt;
    if (getdatetime(&dt) == 0) {
        char time_str[9];
        time_str[0] = (char)('0' + dt.hour / 10);
        time_str[1] = (char)('0' + dt.hour % 10);
        time_str[2] = ':';
        time_str[3] = (char)('0' + dt.minute / 10);
        time_str[4] = (char)('0' + dt.minute % 10);
        time_str[5] = ':';
        time_str[6] = (char)('0' + dt.second / 10);
        time_str[7] = (char)('0' + dt.second % 10);
        time_str[8] = '\0';
        int saved_scale = g_font_scale;
        g_font_scale = 4;
        draw_text(fb, w, h, 24, TITLEBAR_H + 60, time_str, get_accent_color());
        g_font_scale = saved_scale;
        char date_str[32];
        snprintf(date_str, 32, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
        draw_text(fb, w, h, 24, TITLEBAR_H + 160, date_str,
                  g_dark_theme ? METRO_TEXT_DIM : 0xFF666666);
    } else {
        draw_text(fb, w, h, 24, TITLEBAR_H + 60, "No RTC", METRO_TEXT);
    }
}

/* ── Settings app with functional controls and toggles ──────────────────── */
#define SETTING_ROW_H  36
#define SETTING_GAP    4
#define SETTINGS_TAB_H 28
#define SETTINGS_TABS_Y (TITLEBAR_H + 4)
#define SETTINGS_CONTENT_Y (SETTINGS_TABS_Y + SETTINGS_TAB_H + 8)

static int setting_row_y(int row) {
    return SETTINGS_CONTENT_Y + row * (SETTING_ROW_H + SETTING_GAP);
}

static void draw_toggle_row(uint32_t *fb, uint32_t w, uint32_t row,
                            const char *label, const char *val, uint32_t val_color) {
    int y = setting_row_y(row);
    fill_rect(fb, w, g_info.height, 24, y, (int)w - 48, SETTING_ROW_H, 0xFF222244);
    draw_text(fb, w, g_info.height, 36, y + (SETTING_ROW_H - FONT_H) / 2, label, METRO_TEXT);
    draw_text(fb, w, g_info.height, (int)w - 120, y + (SETTING_ROW_H - FONT_H) / 2, val, val_color);
}

static const char *g_settings_tab_names[4] = {"Display", "Network", "Audio", "About"};

static void draw_settings_tabs(uint32_t *fb, uint32_t w, uint32_t h) {
    int tw = w / 4;
    for (int i = 0; i < 4; i++) {
        int x = i * tw;
        uint32_t col = (i == g_settings_tab) ? 0xFF3388CC : 0xFF222244;
        ztk_fb_fill_rect(fb, w, h, x, SETTINGS_TABS_Y, tw, SETTINGS_TAB_H, col);
        draw_text(fb, w, h, x + (tw - (int)strlen(g_settings_tab_names[i]) * (FONT_W + 1)) / 2,
                  SETTINGS_TABS_Y + (SETTINGS_TAB_H - FONT_H) / 2,
                  g_settings_tab_names[i], METRO_TEXT);
    }
    ztk_fb_hline(fb, w, h, 0, SETTINGS_TABS_Y + SETTINGS_TAB_H, w, 0xFF445566);
}

static void draw_settings(uint32_t *fb, uint32_t w, uint32_t h) {
    uint32_t bg = g_dark_theme ? 0xFF1A1A2E : 0xFFE8E8F0;
    uint32_t text_c   = g_dark_theme ? METRO_TEXT : 0xFF222222;
    uint32_t dim_c    = g_dark_theme ? METRO_TEXT_DIM : 0xFF666666;
    uint32_t accent_c = get_accent_color();
    uint32_t green_c  = 0xFF44CC44;
    uint32_t bar_bg   = g_dark_theme ? 0xFF333355 : 0xFFCCCCDD;

    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, bg);
    draw_settings_tabs(fb, w, h);

    int bar_x = 160, bar_w = (int)w - 200;
    int slid_y, fill;

    switch (g_settings_tab) {
    case 0: { /* Display tab */
        int r = 0;

        /* Row 0: Wallpaper */
        draw_toggle_row(fb, w, r, "Wallpaper",
                        wallpaper_names[g_wallpaper_style], accent_c);

        /* Row 1: Accent Color */
        r = 1;
        int ay = setting_row_y(r);
        fill_rect(fb, w, g_info.height, 24, ay, (int)w - 48, SETTING_ROW_H, 0xFF222244);
        draw_text(fb, w, g_info.height, 36, ay + (SETTING_ROW_H - FONT_H) / 2, "Accent Color", text_c);
        int swatch_x = (int)w - 140;
        int swatch_y = ay + (SETTING_ROW_H - 14) / 2;
        fill_rect(fb, w, g_info.height, swatch_x, swatch_y, 14, 14, accent_colors[g_accent_color]);
        fill_rect(fb, w, g_info.height, swatch_x, swatch_y, 14, 1, 0x88FFFFFF);
        fill_rect(fb, w, g_info.height, swatch_x, swatch_y, 1, 14, 0x88FFFFFF);
        fill_rect(fb, w, g_info.height, swatch_x + 13, swatch_y, 1, 14, 0x44000000);
        fill_rect(fb, w, g_info.height, swatch_x, swatch_y + 13, 14, 1, 0x44000000);
        draw_text(fb, w, g_info.height, swatch_x + 20, ay + (SETTING_ROW_H - FONT_H) / 2,
                  accent_names[g_accent_color], accent_c);

        /* Row 2: Animation Speed */
        r = 2;
        {
            const char *speed_name;
            if (g_anim_frames <= 5) speed_name = "Fast";
            else if (g_anim_frames >= 20) speed_name = "Smooth";
            else speed_name = "Medium";
            draw_toggle_row(fb, w, r, "Animation", speed_name, accent_c);
        }

        /* Row 3: Font Style */
        r = 3;
        draw_toggle_row(fb, w, r, "Font Style",
                        g_font_style ? "Bold" : "Regular", accent_c);

        /* Row 4: Font Size */
        r = 4;
        draw_toggle_row(fb, w, r, "Font Size",
                        g_font_scale > 1 ? "Large" : "Small", accent_c);

        /* Row 5: Dark Theme */
        r = 5;
        draw_toggle_row(fb, w, r, "Dark Theme",
                        g_dark_theme ? "On" : "Off", g_dark_theme ? green_c : dim_c);

        /* Row 6: Brightness slider */
        r = 6;
        slid_y = setting_row_y(r) + (SETTING_ROW_H - 16) / 2;
        draw_text(fb, w, h, 36, slid_y + (16 - FONT_H) / 2, "Brightness", text_c);
        fill_rect(fb, w, h, bar_x, slid_y, bar_w, 16, bar_bg);
        fill = bar_w * g_brightness / 100;
        if (fill > 0) fill_rect(fb, w, h, bar_x, slid_y, fill, 16, accent_c);
        char bri[8];
        snprintf(bri, sizeof(bri), "%d%%", g_brightness);
        draw_text(fb, w, h, bar_x + fill - 16, slid_y + (16 - FONT_H) / 2, bri, METRO_TEXT);

        /* Row 7: Frame Rate */
        r = 7;
        draw_toggle_row(fb, w, r, "Frame Rate",
                        g_frame_rate == 60 ? "60 FPS" : "30 FPS", accent_c);
        break;
    }
    case 1: { /* Network tab */
        int r = 0;
        draw_toggle_row(fb, w, r, "Wi-Fi",
                        g_wifi_on ? "On" : "Off", g_wifi_on ? green_c : dim_c);
        r = 1;
        draw_toggle_row(fb, w, r, "Bluetooth",
                        g_bt_on ? "On" : "Off", g_bt_on ? green_c : dim_c);
        break;
    }
    case 2: { /* Audio tab */
        int r = 0;
        slid_y = setting_row_y(r) + (SETTING_ROW_H - 16) / 2;
        draw_text(fb, w, h, 36, slid_y + (16 - FONT_H) / 2, "Volume", text_c);
        fill_rect(fb, w, h, bar_x, slid_y, bar_w, 16, bar_bg);
        fill = bar_w * g_volume / 100;
        if (fill > 0) fill_rect(fb, w, h, bar_x, slid_y, fill, 16, accent_c);
        char vol[8];
        snprintf(vol, sizeof(vol), "%d%%", g_volume);
        draw_text(fb, w, h, bar_x + fill - 16, slid_y + (16 - FONT_H) / 2, vol, METRO_TEXT);
        break;
    }
    case 3: { /* About tab */
        int r = 0;
        draw_text(fb, w, h, 36, setting_row_y(r) + (SETTING_ROW_H - FONT_H) / 2,
                  "Zirvium OS v0.1.0", dim_c);
        r = 1;
        draw_text(fb, w, h, 36, setting_row_y(r) + (SETTING_ROW_H - FONT_H) / 2,
                  "DisplayJet MAEM compositor", dim_c);
        r = 2;
        draw_text(fb, w, h, 36, setting_row_y(r) + (SETTING_ROW_H - FONT_H) / 2,
                  "Kernel: MOSIX x86_64", dim_c);
        r = 3;
        draw_text(fb, w, h, 36, setting_row_y(r) + (SETTING_ROW_H - FONT_H) / 2,
                  "Font: 8x13 bitmap (Regular / Bold)", dim_c);
        break;
    }
    }
}

/* ── Text Editor ───────────────────────────────────────────────────────── */
static void draw_editor(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    int rows = ((int)h - TITLEBAR_H - 20) / (FONT_H + 4);
    int start_row = g_editor.scroll_row;
    int end_row = start_row + rows;
    if (end_row > g_editor.num_lines) end_row = g_editor.num_lines;
    int y = TITLEBAR_H + 10;
    for (int r = start_row; r < end_row; r++) {
        uint32_t c = (r == g_editor.row) ? 0xFF2A2A4E : 0xFF222244;
        fill_rect(fb, w, h, 8, y, (int)w - 16, FONT_H + 4, c);
        draw_text(fb, w, h, 12, y + 1, g_editor.lines[r], METRO_TEXT);
        /* cursor on active row */
        if (r == g_editor.row) {
            int cx = 12 + g_editor.col * (FONT_W + 1);
            fill_rect(fb, w, h, cx, y + 1, 2, FONT_H, 0xFF88CCFF);
        }
        y += FONT_H + 5;
    }
}

/* ── Snake ──────────────────────────────────────────────────────────────── */
#define SNAKE_CELL 10
static void draw_snake(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF0A0A1E);
    int ox = ((int)w - SNAKE_COLS * SNAKE_CELL) / 2;
    int oy = TITLEBAR_H + ((int)h - TITLEBAR_H - SNAKE_ROWS * SNAKE_CELL) / 2;
    /* grid */
    for (int y = 0; y < SNAKE_ROWS; y++) {
        for (int x = 0; x < SNAKE_COLS; x++) {
            fill_rect(fb, w, h, ox + x * SNAKE_CELL, oy + y * SNAKE_CELL,
                      SNAKE_CELL - 1, SNAKE_CELL - 1, 0xFF111133);
        }
    }
    /* food */
    fill_rect(fb, w, h, ox + g_snake.food_x * SNAKE_CELL, oy + g_snake.food_y * SNAKE_CELL,
              SNAKE_CELL - 1, SNAKE_CELL - 1, 0xFFE74C3C);
    /* snake */
    for (int i = 0; i < g_snake.seg_len; i++) {
        uint32_t sc = (i == 0) ? 0xFF2ECC71 : 0xFF27AE60;
        fill_rect(fb, w, h, ox + g_snake.seg_x[i] * SNAKE_CELL, oy + g_snake.seg_y[i] * SNAKE_CELL,
                  SNAKE_CELL - 1, SNAKE_CELL - 1, sc);
    }
    /* score */
    char buf[32];
    snprintf(buf, 32, "Score: %d", g_snake.score);
    draw_text(fb, w, h, 8, TITLEBAR_H + 6, buf, METRO_TEXT);
    if (g_snake.gameover) {
        draw_text(fb, w, h, (int)w / 2 - 40, (int)h / 2 - 6, "GAME OVER", 0xFFE74C3C);
        draw_text(fb, w, h, (int)w / 2 - 48, (int)h / 2 + 12, "Press R to restart", METRO_TEXT_DIM);
    }
}

/* ── Pong ───────────────────────────────────────────────────────────────── */
static void draw_pong(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF0A0A1E);
    int cw = (int)w;
    int ch = (int)h - TITLEBAR_H;
    int mid_x = cw / 2;
    int mid_y = TITLEBAR_H + ch / 2;
    /* center line */
    fill_rect(fb, w, h, mid_x - 1, TITLEBAR_H, 2, ch, 0xFF222244);
    /* ball */
    int bx = (int)(g_pong.ball_x * cw);
    int by = (int)(g_pong.ball_y * ch) + TITLEBAR_H;
    fill_rect(fb, w, h, bx - 4, by - 4, 8, 8, 0xFFFFFFFF);
    /* player paddle */
    int ppy = (int)(g_pong.paddle_y * ch) + TITLEBAR_H;
    fill_rect(fb, w, h, 12, ppy - 25, 6, 50, 0xFF3498DB);
    /* AI paddle */
    int apy = (int)(g_pong.ball_y * ch) + TITLEBAR_H - 25;
    if (apy < TITLEBAR_H) apy = TITLEBAR_H;
    int amax = (int)h - 50;
    if (apy > amax) apy = amax;
    fill_rect(fb, w, h, cw - 18, apy, 6, 50, 0xFFE74C3C);
    /* score */
    char buf[16];
    snprintf(buf, 16, "%d  %d", g_pong.player_score, g_pong.ai_score);
    draw_text(fb, w, h, mid_x - 20, TITLEBAR_H + 10, buf, METRO_TEXT);
    if (g_pong.gameover) {
        draw_text(fb, w, h, mid_x - 40, mid_y - 10, "GAME OVER", 0xFFE74C3C);
        draw_text(fb, w, h, mid_x - 48, mid_y + 12, "Press R to restart", METRO_TEXT_DIM);
    }
}

/* ── Tetris ─────────────────────────────────────────────────────────────── */
#define TETRIS_CELL 18
static void tetris_get_piece(int type, int rot, int out[4][4]) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[r][c] = tetris_pieces[type][r][c];
    for (int k = 0; k < rot; k++) {
        int tmp[4][4];
        memcpy(tmp, out, sizeof(tmp));
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                out[r][c] = tmp[3 - c][r];
    }
}
static int tetris_collide(int type, int rot, int px, int py) {
    int shape[4][4];
    tetris_get_piece(type, rot, shape);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!shape[r][c]) continue;
            int gx = px + c, gy = py + r;
            if (gx < 0 || gx >= TETRIS_COLS || gy >= TETRIS_ROWS) return 1;
            if (gy >= 0 && g_tetris.grid[gy][gx]) return 1;
        }
    }
    return 0;
}
static void tetris_lock(void) {
    int shape[4][4];
    tetris_get_piece(g_tetris.piece_type, g_tetris.piece_rot, shape);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!shape[r][c]) continue;
            int gx = g_tetris.piece_x + c, gy = g_tetris.piece_y + r;
            if (gy >= 0 && gy < TETRIS_ROWS && gx >= 0 && gx < TETRIS_COLS)
                g_tetris.grid[gy][gx] = (char)(g_tetris.piece_type + 1);
        }
    }
    /* clear lines */
    int cleared = 0;
    for (int r = TETRIS_ROWS - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < TETRIS_COLS; c++) {
            if (!g_tetris.grid[r][c]) { full = 0; break; }
        }
        if (full) {
            for (int r2 = r; r2 > 0; r2--)
                memcpy(g_tetris.grid[r2], g_tetris.grid[r2 - 1], TETRIS_COLS);
            memset(g_tetris.grid[0], 0, TETRIS_COLS);
            cleared++;
            r++; /* recheck same row */
        }
    }
    if (cleared) {
        g_tetris.lines += cleared;
        g_tetris.score += cleared * 100;
    }
    /* spawn next */
    g_tetris.piece_type = g_tetris.next_piece;
    g_tetris.piece_rot = 0;
    g_tetris.piece_x = 3;
    g_tetris.piece_y = 0;
    g_tetris.next_piece = (int)((uint64_t)uptime() % 7);
    if (tetris_collide(g_tetris.piece_type, g_tetris.piece_rot,
                       g_tetris.piece_x, g_tetris.piece_y))
        g_tetris.gameover = 1;
}
static void tetris_drop(void) {
    if (g_tetris.gameover) return;
    if (!tetris_collide(g_tetris.piece_type, g_tetris.piece_rot,
                        g_tetris.piece_x, g_tetris.piece_y + 1)) {
        g_tetris.piece_y++;
    } else {
        tetris_lock();
    }
}

static void draw_tetris(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF0A0A1E);
    int ox = ((int)w - TETRIS_COLS * TETRIS_CELL) / 2;
    int oy = TITLEBAR_H + ((int)h - TITLEBAR_H - TETRIS_ROWS * TETRIS_CELL) / 2;
    /* grid */
    for (int r = 0; r < TETRIS_ROWS; r++) {
        for (int c = 0; c < TETRIS_COLS; c++) {
            int idx = (int)g_tetris.grid[r][c] - 1;
            uint32_t col = (idx >= 0 && idx < 7) ? (uint32_t)tetris_colors[idx] : 0xFF111133;
            fill_rect(fb, w, h, ox + c * TETRIS_CELL, oy + r * TETRIS_CELL,
                      TETRIS_CELL - 1, TETRIS_CELL - 1, col);
        }
    }
    /* current piece */
    if (!g_tetris.gameover) {
        int shape[4][4];
        tetris_get_piece(g_tetris.piece_type, g_tetris.piece_rot, shape);
        uint32_t pc = tetris_colors[g_tetris.piece_type];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (!shape[r][c]) continue;
                int gx = g_tetris.piece_x + c, gy = g_tetris.piece_y + r;
                if (gy >= 0 && gy < TETRIS_ROWS && gx >= 0 && gx < TETRIS_COLS)
                    fill_rect(fb, w, h, ox + gx * TETRIS_CELL, oy + gy * TETRIS_CELL,
                              TETRIS_CELL - 1, TETRIS_CELL - 1, pc);
            }
        }
    }
    /* info */
    char buf[32];
    snprintf(buf, 32, "Score: %d", g_tetris.score);
    draw_text(fb, w, h, 8, TITLEBAR_H + 6, buf, METRO_TEXT);
    snprintf(buf, 32, "Lines: %d", g_tetris.lines);
    draw_text(fb, w, h, 8, TITLEBAR_H + 22, buf, METRO_TEXT_DIM);
    /* next piece preview */
    draw_text(fb, w, h, (int)w - 80, TITLEBAR_H + 6, "Next:", METRO_TEXT_DIM);
    int nshape[4][4];
    tetris_get_piece(g_tetris.next_piece, 0, nshape);
    uint32_t npc = tetris_colors[g_tetris.next_piece];
    int nx = (int)w - 72, ny = TITLEBAR_H + 22;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (nshape[r][c])
                fill_rect(fb, w, h, nx + c * 10, ny + r * 10, 9, 9, npc);

    if (g_tetris.gameover) {
        draw_text(fb, w, h, (int)w / 2 - 40, (int)h / 2 - 6, "GAME OVER", 0xFFE74C3C);
        draw_text(fb, w, h, (int)w / 2 - 48, (int)h / 2 + 12, "Press R to restart", METRO_TEXT_DIM);
    }
}

/* ── App dispatch table ──────────────────────────────────────────────────── */
typedef void (*app_draw_fn)(uint32_t *, uint32_t, uint32_t);
static app_draw_fn g_app_draw[NUM_APPS] = {
    draw_terminal, draw_calculator, draw_settings,
    draw_clock_app, draw_editor, draw_snake, draw_pong, draw_tetris, draw_demoapp,
};

/* ── Title bar rendering (Metro thin bar at top) ──────────────────────────── */
static void draw_title_bar(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, 0, (int)w, TITLEBAR_H, METRO_BG);
    fill_rect(fb, w, h, 0, TITLEBAR_H - 1, (int)w, 1, 0xFF333355);
    uint32_t bc = g_back_hover ? 0xFF555577 : METRO_BACK_BTN;
    fill_rect(fb, w, h, 4, 4, 60, TITLEBAR_H - 8, bc);
    draw_text(fb, w, h, 10, (TITLEBAR_H - FONT_H) / 2, "< Back", METRO_TEXT);
    if (g_active_app >= 0) {
        draw_text(fb, w, h, (int)w / 2 - ((int)strlen(g_apps[g_active_app].name) * (FONT_W + 1)) / 2,
                  (TITLEBAR_H - FONT_H) / 2, g_apps[g_active_app].name, METRO_TEXT);
    }
}

/* ── Taskbar (visible on start screen) ───────────────────────────────────── */
static void draw_taskbar(uint32_t *fb, uint32_t w, uint32_t h) {
    int tb_y = (int)h - TASKBAR_H;
    fill_rect(fb, w, h, 0, tb_y, (int)w, TASKBAR_H, METRO_TASKBAR);
    fill_rect(fb, w, h, 0, tb_y, (int)w, 1, 0xFF333355);
    draw_clock_text(fb, w, h, (int)w - 70, tb_y + (TASKBAR_H - FONT_H) / 2, METRO_TEXT);
    draw_text(fb, w, h, 10, tb_y + (TASKBAR_H - FONT_H) / 2, "Zirvium", get_accent_color());

    int btn_y = tb_y + 4;
    int btn_h = TASKBAR_H - 8;

    /* Exit button */
    int exit_x = (int)w - 195;
    int exit_w = 55;
    uint32_t exit_c = g_exit_hover ? 0xFF555577 : 0xFF333355;
    fill_rect(fb, w, h, exit_x, btn_y, exit_w, btn_h, exit_c);
    draw_text(fb, w, h, exit_x + (exit_w - 4 * (FONT_W + 1)) / 2,
              btn_y + (btn_h - FONT_H) / 2, "Exit", METRO_TEXT);

    /* Power button */
    int pwr_x = (int)w - 135;
    int pwr_w = 60;
    uint32_t pwr_c = g_power_hover ? 0xFF553333 : 0xFF333355;
    fill_rect(fb, w, h, pwr_x, btn_y, pwr_w, btn_h, pwr_c);
    draw_text(fb, w, h, pwr_x + (pwr_w - 5 * (FONT_W + 1)) / 2,
              btn_y + (btn_h - FONT_H) / 2, "Power", METRO_TEXT);
}

/* ── Shutdown confirmation dialog / animation overlay ─────────────────── */
static void draw_shutdown_overlay(uint32_t *fb, uint32_t w, uint32_t h) {
    /* Semi-transparent dark overlay */
    fill_rect(fb, w, h, 0, 0, (int)w, (int)h, 0xAA000000);

    int cx = (int)w / 2;
    int cy = (int)h / 2;
    int dw = 340, dh = 140;
    int dx = cx - dw / 2, dy = cy - dh / 2;

    /* Dialog box background */
    fill_rect(fb, w, h, dx, dy, dw, dh, METRO_DIALOG_BG);
    fill_rect(fb, w, h, dx, dy, dw, 2, 0xFF6666AA);

    if (g_confirm_shutdown == 1) {
        /* ── Confirmation dialog ─────────────────────────────────────── */
        draw_text(fb, w, h, cx - 36, dy + 24, "Shut down?", METRO_TEXT);

        int btn_w = 90, btn_h = 34;
        int btn_y = dy + dh - 52;

        /* Yes button */
        int yes_x = dx + 50;
        uint32_t yes_c = g_shutdown_yes_hover ? 0xFF553333 : 0xFF444466;
        fill_rect(fb, w, h, yes_x, btn_y, btn_w, btn_h, yes_c);
        draw_text(fb, w, h, yes_x + (btn_w - 3 * (FONT_W + 1)) / 2,
                  btn_y + (btn_h - FONT_H) / 2, "Yes", METRO_TEXT);

        /* No button */
        int no_x = dx + dw - 50 - btn_w;
        uint32_t no_c = g_shutdown_no_hover ? 0xFF555577 : 0xFF444466;
        fill_rect(fb, w, h, no_x, btn_y, btn_w, btn_h, no_c);
        draw_text(fb, w, h, no_x + (btn_w - 2 * (FONT_W + 1)) / 2,
                  btn_y + (btn_h - FONT_H) / 2, "No", METRO_TEXT);
    } else {
        /* ── Shutting down animation ─────────────────────────────────── */
        const char *dots[] = {"", ".", "..", "..."};
        char msg[32];
        snprintf(msg, sizeof(msg), "Shutting down%s", dots[g_shutdown_dot]);
        draw_text(fb, w, h, cx - 72, dy + 30, msg, 0xFFFF6666);

        /* Progress bar */
        int bar_x = dx + 30, bar_y = dy + 70;
        int bar_w = dw - 60, bar_h = 8;
        fill_rect(fb, w, h, bar_x, bar_y, bar_w, bar_h, 0xFF333355);

        uint64_t elapsed = uptime() - g_shutdown_start_s;
        if (elapsed > 5) elapsed = 5;
        int fill = (int)((uint64_t)bar_w * elapsed / 5);
        if (fill > 0)
            fill_rect(fb, w, h, bar_x, bar_y, fill, bar_h, 0xFFCC4444);

        char pct[8];
        g_font_scale = 1;
        snprintf(pct, sizeof(pct), "%lu%%", (unsigned long)(elapsed * 100 / 5));
        draw_text(fb, w, h, cx - 12, bar_y + 14, pct, METRO_TEXT_DIM);
    }
}

/* ── Boot splash with Zirvium SVG logo ──────────────────────────────────── */
static void draw_boot_splash(uint32_t *fb, uint32_t w, uint32_t h) {
    /* Dark gradient background */
    for (uint32_t y = 0; y < h; y++) {
        uint8_t r = (uint8_t)(0x0A + ((0x14 - 0x0A) * y) / (h ? h : 1));
        uint8_t g = (uint8_t)(0x0A + ((0x14 - 0x0A) * y) / (h ? h : 1));
        uint8_t b = (uint8_t)(0x1E + ((0x3A - 0x1E) * y) / (h ? h : 1));
        uint32_t c = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        for (uint32_t x = 0; x < w; x++)
            fb[y * w + x] = c;
    }

    /* SVG logo: 7 horizontal lines from zirvworld logo.svg */
    int logo_data[7][4] = {
        {28, 36, 8,  0}, {22, 42, 16, 0}, {16, 48, 24, 1},
        {10, 54, 32, 0}, {16, 48, 40, 1}, {22, 42, 48, 0}, {28, 36, 56, 0},
    };
    int logo_size = (w < h ? w : h) / 5;
    if (logo_size > 480) logo_size = 480;
    if (logo_size < 160) logo_size = 160;
    int scale = logo_size / 64;

    int ox = (int)w / 2 - 32 * scale;
    int oy = (int)h / 3 - 32 * scale;
    uint32_t light = 0xFFCCCCCCu;
    uint32_t blue  = 0xFF4488FFu;

    for (int i = 0; i < 7; i++) {
        int x1 = ox + logo_data[i][0] * scale;
        int x2 = ox + logo_data[i][1] * scale;
        int y  = oy + logo_data[i][2] * scale;
        uint32_t color = logo_data[i][3] ? blue : light;
        int thick = scale > 2 ? scale * 2 : scale;
        for (int dy = 0; dy < thick; dy++) {
            int py = y + dy;
            if (py < 0 || py >= (int)h) continue;
            for (int px = x1; px <= x2; px++) {
                if (px >= 0 && px < (int)w)
                    fb[(uint32_t)py * w + (uint32_t)px] = color;
            }
        }
    }

    /* Title: "Zirvium" scaled */
    int saved_scale = g_font_scale;
    g_font_scale = 3;
    const char *title = "Zirvium";
    int tlen = 0;
    while (title[tlen]) tlen++;
    int tstep = (FONT_W + 1) * g_font_scale;
    int tx = (int)w / 2 - (tlen * tstep) / 2;
    int ty = oy + 64 * scale + 20;
    for (const char *p = title; *p; p++) {
        draw_char_scaled(fb, w, h, tx, ty, *p, 0xFF4488FFu);
        tx += tstep;
    }

    /* Subtitle */
    g_font_scale = 1;
    const char *sub = "MOSIX Operating System";
    int slen = 0;
    while (sub[slen]) slen++;
    int sstep = (FONT_W + 1) * g_font_scale;
    int sx = (int)w / 2 - (slen * sstep) / 2;
    int sy = ty + (FONT_H + 1) * 3 + 8;
    for (const char *p = sub; *p; p++) {
        draw_char(fb, w, h, sx, sy, *p, 0xFF8888CCu);
        sx += sstep;
    }
    g_font_scale = saved_scale;

    draw_text(fb, w, h, (int)w / 2 - 80, sy + 24,
              "Loading...", 0xFF666688u);
}

/* ── Render one frame ──────────────────────────────────────────────────── */
static void render_frame(void) {
    uint32_t w = g_info.width;
    uint32_t h = g_info.height;
    uint32_t *fb32 = (uint32_t *)compositor_fb;

    if (g_anim.active) {
        if (g_anim.opening) {
            draw_start_screen(fb32, w, h);
            int progress = (g_anim.frame * 256) / g_anim_frames;
            int eased = smoothstep(progress);
            int cw = g_anim.tile_w + ((int)w - g_anim.tile_w) * eased / 256;
            int ch = g_anim.tile_h + ((int)h - g_anim.tile_h) * eased / 256;
            int cx = g_anim.tile_x + (0 - g_anim.tile_x) * eased / 256;
            int cy = g_anim.tile_y + (0 - g_anim.tile_y) * eased / 256;
            uint32_t color = g_apps[g_anim.app_idx].color;
            fill_rect(fb32, w, h, cx, cy, cw, ch, color);
            int icon_size = cw / 4;
            draw_icon(fb32, w, h, cx + cw / 2, cy + ch / 2 - 12, icon_size,
                      g_apps[g_anim.app_idx].icon, METRO_TEXT);
            int name_y = cy + ch - FONT_H - 12;
            const char *name = g_apps[g_anim.app_idx].name;
            int name_w = (int)strlen(name) * (FONT_W + 1);
            draw_text(fb32, w, h, cx + (cw - name_w * g_font_scale) / 2, name_y, name, METRO_TEXT);
            draw_taskbar(fb32, w, h);
        } else {
            render_bg(fb32, w, h);
            int progress = (g_anim.frame * 256) / g_anim_frames;
            int eased = smoothstep(256 - progress);
            int cw = g_anim.tile_w + ((int)w - g_anim.tile_w) * eased / 256;
            int ch = g_anim.tile_h + ((int)h - g_anim.tile_h) * eased / 256;
            int cx = g_anim.tile_x + (0 - g_anim.tile_x) * eased / 256;
            int cy = g_anim.tile_y + (0 - g_anim.tile_y) * eased / 256;
            uint32_t color = g_apps[g_anim.app_idx].color;
            fill_rect(fb32, w, h, cx, cy, cw, ch, color);
            int icon_size = cw / 4;
            draw_icon(fb32, w, h, cx + cw / 2, cy + ch / 2 - 12, icon_size,
                      g_apps[g_anim.app_idx].icon, METRO_TEXT);
            int name_y = cy + ch - FONT_H - 12;
            const char *name = g_apps[g_anim.app_idx].name;
            int name_w = (int)strlen(name) * (FONT_W + 1);
            draw_text(fb32, w, h, cx + (cw - name_w * g_font_scale) / 2, name_y, name, METRO_TEXT);
            draw_taskbar(fb32, w, h);
        }
        return;
    }

    if (g_active_app >= 0) {
        render_bg(fb32, w, h);
        draw_title_bar(fb32, w, h);
        g_app_draw[g_active_app](fb32, w, h);
    } else {
        draw_start_screen(fb32, w, h);
        draw_taskbar(fb32, w, h);
    }

    /* Shutdown confirmation / animation overlay */
    if (g_confirm_shutdown)
        draw_shutdown_overlay(fb32, w, h);

    /* Context menu overlay */
    if (g_ctx_active)
        draw_context_menu(fb32, w, h);

    /* Drag overlay: draw dragged tile at cursor */
    if (g_drag_active == 2 && g_drag_app >= 0) {
        int dw = g_tile_pos_w[g_drag_slot];
        int dh = g_tile_pos_h[g_drag_slot];
        int dx = g_drag_cur_x - (dw / 2);
        int dy = g_drag_cur_y - (dh / 2);
        uint32_t drag_col = g_apps[g_drag_app].color;
        drag_col = blend(0x80FFFFFF, drag_col, 40);
        fill_rect(fb32, w, h, dx, dy, dw, dh, drag_col);
        int icon_size = dw / 3;
        draw_icon(fb32, w, h, dx + dw / 2, dy + dh / 2 - 6, icon_size,
                  g_apps[g_drag_app].icon, METRO_TEXT);
        int name_y = dy + dh - FONT_H - 6;
        const char *name = g_apps[g_drag_app].name;
        int name_w = (int)strlen(name) * (FONT_W + 1);
        draw_text(fb32, w, h, dx + (dw - name_w * g_font_scale) / 2, name_y, name, METRO_TEXT);
    }
}

/* ── Process mouse events ───────────────────────────────────────────────── */
static void calc_hit_test(int mx, int my) {
    g_calc_btn_hover = -1;
    int bw = (int)g_info.width / 4;
    int bh = 50;
    int start_y = TITLEBAR_H + 80;
    struct { int row, col, wide; } btns[] = {
        {0,0,0}, {0,1,0}, {0,2,0}, {0,3,0},
        {1,0,0}, {1,1,0}, {1,2,0}, {1,3,0},
        {2,0,0}, {2,1,0}, {2,2,0}, {2,3,0},
        {3,0,0}, {3,1,0}, {3,2,0}, {3,3,0},
        {4,0,0}, {4,1,0}, {4,2,1},
    };
    int n = (int)(sizeof(btns) / sizeof(btns[0]));
    for (int i = 0; i < n; i++) {
        int bx = btns[i].col * bw;
        int by = start_y + btns[i].row * bh;
        int bww = btns[i].wide ? bw * 2 : bw;
        if (point_in(mx, my, bx + 2, by + 2, bww - 4, bh - 4)) {
            g_calc_btn_hover = i;
            return;
        }
    }
}

static const char *calc_btn_labels[] = {
    "C", "(", ")", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "0", ".", "=",
};

static void process_mouse(void) {
    int prev_th = g_tile_hover;
    int prev_bh = g_back_hover;
    int prev_cbh = g_calc_btn_hover;
    int prev_eh = g_exit_hover;
    int prev_ph = g_power_hover;
    int prev_syh = g_shutdown_yes_hover;
    int prev_snh = g_shutdown_no_hover;

    uint32_t ww = g_info.width;
    uint32_t wh = g_info.height;

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

        int left_down = (ev.buttons & 1) && !(prev_buttons & 1);
        int left_up = !(ev.buttons & 1) && (prev_buttons & 1);
        int right_down = (ev.buttons & 2) && !(prev_buttons & 2);
        prev_buttons = ev.buttons;

        g_tile_hover = -1;
        g_back_hover = 0;
        g_exit_hover = 0;
        g_power_hover = 0;

        if (g_anim.active) {
            if (left_down) {
                g_anim.active = 0;
                g_anim.frame = g_anim_frames;
                g_dirty = 1;
            }
            continue;
        }

        /* ── Shutdown confirmation dialog ───────────────────────────── */
        if (g_confirm_shutdown == 2) {
            continue; /* ignore all input during shutdown animation */
        }
        if (g_confirm_shutdown == 1) {
            g_shutdown_yes_hover = 0;
            g_shutdown_no_hover = 0;

            int cx = (int)ww / 2, cy = (int)wh / 2;
            int dw = 340, dh = 140;
            int dx = cx - dw / 2, dy = cy - dh / 2;

            int btn_x = dx + 50, btn_y = dy + dh - 52;
            int btn_w = 90, btn_h = 34;
            if (point_in(nx, ny, btn_x, btn_y, btn_w, btn_h))
                g_shutdown_yes_hover = 1;

            int no_x = dx + dw - 50 - btn_w;
            if (point_in(nx, ny, no_x, btn_y, btn_w, btn_h))
                g_shutdown_no_hover = 1;

            if (left_down && g_shutdown_yes_hover) {
                g_confirm_shutdown = 2;
                g_shutdown_start_s = uptime();
                g_dirty = 1;
            }
            if (left_down && g_shutdown_no_hover) {
                g_confirm_shutdown = 0;
                g_dirty = 1;
            }
            continue;
        }

        if (g_active_app >= 0) {
            g_back_hover = point_in(nx, ny, 4, 4, 60, TITLEBAR_H - 8);
            if (g_active_app == APP_CALCULATOR) {
                calc_hit_test(nx, ny);
            }
            if (left_down && g_active_app == APP_DEMO) {
                if (demo_hit_test(nx, ny, (int)ww))
                    g_dirty = 1;
            }
            if (left_down) {
                if (g_back_hover) {
                    if (g_active_app >= 0) {
                        int tx, ty, tw, th;
                        get_tile_rect(app_slot(g_active_app), &tx, &ty, &tw, &th);
                        g_anim.active = 1;
                        g_anim.opening = 0;
                        g_anim.app_idx = g_active_app;
                        g_anim.frame = 0;
                        g_anim.tile_x = tx; g_anim.tile_y = ty;
                        g_anim.tile_w = tw; g_anim.tile_h = th;
                        g_anim.scr_w = (int)ww; g_anim.scr_h = (int)wh;
                        g_dirty = 1;
                    }
                } else if (g_active_app == APP_CALCULATOR && g_calc_btn_hover >= 0) {
                    const char *label = calc_btn_labels[g_calc_btn_hover];
                    calc_press(label[0]);
                    g_dirty = 1;
                } else if (g_active_app == APP_SETTINGS) {
                    int rw = (int)ww;
                    int bar_x = 160, bar_w = rw - 200;

                    /* Tab bar hit-test */
                    if (ny >= SETTINGS_TABS_Y && ny < SETTINGS_TABS_Y + SETTINGS_TAB_H) {
                        int tw = rw / 4;
                        for (int i = 0; i < 4; i++) {
                            if (nx >= i * tw && nx < (i + 1) * tw) {
                                g_settings_tab = i;
                                g_dirty = 1;
                            }
                        }
                    }

                    switch (g_settings_tab) {
                    case 0: { /* Display tab */
                        /* Row 0: Wallpaper (cycle) */
                        if (point_in(nx, ny, 24, setting_row_y(0), rw - 48, SETTING_ROW_H)) {
                            g_wallpaper_style = (g_wallpaper_style + 1) % NUM_WALLPAPERS;
                            bg_valid = 0;
                            g_dirty = 1;
                        }
                        /* Row 1: Accent Color (cycle) */
                        if (point_in(nx, ny, 24, setting_row_y(1), rw - 48, SETTING_ROW_H)) {
                            g_accent_color = (g_accent_color + 1) % NUM_ACCENTS;
                            g_dirty = 1;
                        }
                        /* Row 2: Animation Speed */
                        if (point_in(nx, ny, 24, setting_row_y(2), rw - 48, SETTING_ROW_H)) {
                            if (g_anim_frames <= 5) g_anim_frames = 10;
                            else if (g_anim_frames >= 20) g_anim_frames = 5;
                            else g_anim_frames = 20;
                            g_dirty = 1;
                        }
                        /* Row 3: Font Style */
                        if (point_in(nx, ny, 24, setting_row_y(3), rw - 48, SETTING_ROW_H)) {
                            g_font_style = g_font_style ? 0 : 1;
                            g_dirty = 1;
                        }
                        /* Row 4: Font Size */
                        if (point_in(nx, ny, 24, setting_row_y(4), rw - 48, SETTING_ROW_H)) {
                            g_font_scale = (g_font_scale > 1) ? 1 : 2;
                            g_dirty = 1;
                        }
                        /* Row 5: Dark Theme */
                        if (point_in(nx, ny, 24, setting_row_y(5), rw - 48, SETTING_ROW_H)) {
                            g_dark_theme = !g_dark_theme;
                            bg_valid = 0;
                            g_dirty = 1;
                        }
                        /* Row 6: Brightness slider */
                        if (point_in(nx, ny, bar_x, setting_row_y(6), bar_w, SETTING_ROW_H)) {
                            int pct = (nx - bar_x) * 100 / bar_w;
                            if (pct < 0) pct = 0;
                            if (pct > 100) pct = 100;
                            g_brightness = pct;
                            g_dirty = 1;
                        }
                        /* Row 7: Frame Rate */
                        if (point_in(nx, ny, 24, setting_row_y(7), rw - 48, SETTING_ROW_H)) {
                            g_frame_rate = (g_frame_rate == 60) ? 30 : 60;
                            g_dirty = 1;
                        }
                        break;
                    }
                    case 1: { /* Network tab */
                        /* Row 0: Wi-Fi */
                        if (point_in(nx, ny, 24, setting_row_y(0), rw - 48, SETTING_ROW_H)) {
                            g_wifi_on = !g_wifi_on;
                            g_dirty = 1;
                        }
                        /* Row 1: Bluetooth */
                        if (point_in(nx, ny, 24, setting_row_y(1), rw - 48, SETTING_ROW_H)) {
                            g_bt_on = !g_bt_on;
                            g_dirty = 1;
                        }
                        break;
                    }
                    case 2: { /* Audio tab */
                        /* Row 0: Volume slider */
                        if (point_in(nx, ny, bar_x, setting_row_y(0), bar_w, SETTING_ROW_H)) {
                            int pct = (nx - bar_x) * 100 / bar_w;
                            if (pct < 0) pct = 0;
                            if (pct > 100) pct = 100;
                            g_volume = pct;
                            g_dirty = 1;
                        }
                        break;
                    }
                    case 3: /* About tab — no interactive rows */
                        break;
                    }
                }
            }
            continue;
        }

        /* ── Context menu interaction ────────────────────────────────────── */
        if (g_ctx_active) {
            int mw = 160, mh = g_ctx_count * 24 + 8;
            int mx = g_ctx_x;
            int my = g_ctx_y;
            if (mx + mw > (int)ww) mx = (int)ww - mw - 4;
            if (my + mh > (int)wh) my = (int)wh - mh - 4;
            if (point_in(nx, ny, mx, my, mw, mh)) {
                int item = (ny - my - 4) / 24;
                g_ctx_hover = (item >= 0 && item < g_ctx_count) ? item : -1;
                if (left_down && g_ctx_hover >= 0) {
                    ctx_execute(g_ctx_hover);
                    g_dirty = 1;
                }
            } else {
                g_ctx_hover = -1;
                if (left_down || right_down) {
                    g_ctx_active = 0;
                    g_dirty = 1;
                }
            }
            continue;
        }

        /* ── Tile hover ───────────────────────────────────────────────── */
        for (int slot = 0; slot < NUM_APPS; slot++) {
            int tx, ty, tw, th;
            get_tile_rect(slot, &tx, &ty, &tw, &th);
            if (point_in(nx, ny, tx, ty, tw, th)) {
                g_tile_hover = slot;
                break;
            }
        }

        /* ── Right-click → context menu ────────────────────────────── */
        if (right_down) {
            int slot = g_tile_hover;
            start_context_menu(nx, ny, (slot >= 0) ? g_tile_order[slot] : -1);
        }

        /* ── Tile drag / open ─────────────────────────────────────────── */
        if (g_drag_active) {
            if (left_up) {
                if (g_drag_active == 1) {
                    /* Click (no drag) → open app */
                    int app = g_drag_app;
                    int slot = app_slot(app);
                    if (slot >= 0) {
                        int tx, ty, tw, th;
                        get_tile_rect(slot, &tx, &ty, &tw, &th);
                        g_anim.active = 1;
                        g_anim.opening = 1;
                        g_anim.app_idx = app;
                        g_anim.frame = 0;
                        g_anim.tile_x = tx; g_anim.tile_y = ty;
                        g_anim.tile_w = tw; g_anim.tile_h = th;
                        g_anim.scr_w = (int)ww; g_anim.scr_h = (int)wh;
                        g_active_app = app;
                        g_dirty = 1;
                    }
                } else {
                    /* Drag complete → swap tiles */
                    if (g_drag_dst_slot >= 0 && g_drag_dst_slot != g_drag_slot) {
                        int tmp = g_tile_order[g_drag_slot];
                        g_tile_order[g_drag_slot] = g_tile_order[g_drag_dst_slot];
                        g_tile_order[g_drag_dst_slot] = tmp;
                        g_tile_layout_dirty = 1;
                        g_dirty = 1;
                    }
                }
                g_drag_active = 0;
                g_drag_slot = -1;
                g_drag_dst_slot = -1;
            } else if (g_drag_active == 1) {
                int dx = nx - g_drag_cur_x;
                int dy = ny - g_drag_cur_y;
                if (dx * dx + dy * dy > 64) {
                    g_drag_active = 2;
                    g_dirty = 1;
                }
                g_drag_cur_x = nx;
                g_drag_cur_y = ny;
            } else if (g_drag_active == 2) {
                g_drag_cur_x = nx;
                g_drag_cur_y = ny;
                g_drag_dst_slot = -1;
                for (int slot = 0; slot < NUM_APPS; slot++) {
                    if (slot == g_drag_slot) continue;
                    int tx, ty, tw, th;
                    get_tile_rect(slot, &tx, &ty, &tw, &th);
                    if (point_in(nx, ny, tx, ty, tw, th)) {
                        g_drag_dst_slot = slot;
                        break;
                    }
                }
                g_dirty = 1;
            }
        } else if (left_down && g_tile_hover >= 0) {
            /* Start potential drag */
            g_drag_active = 1;
            g_drag_slot = g_tile_hover;
            g_drag_app = g_tile_order[g_tile_hover];
            g_drag_cur_x = nx;
            g_drag_cur_y = ny;
            g_drag_dst_slot = -1;
        }

        /* ── Taskbar buttons ─────────────────────────────────────────── */
        int tb_y = (int)wh - TASKBAR_H;
        int btn_y = tb_y + 4;
        int btn_h = TASKBAR_H - 8;
        int exit_x = (int)ww - 195;
        int exit_w = 55;
        if (point_in(nx, ny, exit_x, btn_y, exit_w, btn_h))
            g_exit_hover = 1;
        int pwr_x = (int)ww - 135;
        int pwr_w = 60;
        if (point_in(nx, ny, pwr_x, btn_y, pwr_w, btn_h))
            g_power_hover = 1;

        if (left_down && g_exit_hover) {
            zf_disconnect();
            char *argv[] = { "/bin/shell", NULL };
            char *envp[] = { NULL };
            execve("/bin/shell", argv, envp);
            _exit(0);
        }
        if (left_down && g_power_hover) {
            g_confirm_shutdown = 1;
            g_dirty = 1;
        }
    }

    if (g_tile_hover != prev_th || g_back_hover != prev_bh ||
        g_calc_btn_hover != prev_cbh || g_exit_hover != prev_eh ||
        g_power_hover != prev_ph || g_shutdown_yes_hover != prev_syh ||
        g_shutdown_no_hover != prev_snh)
        g_dirty = 1;
}

/* ── Game logic updates (called each frame) ───────────────────────────── */
static void update_snake(void) {
    if (g_snake.gameover) return;
    g_snake.tick++;
    if (g_snake.tick < 8) return;
    g_snake.tick = 0;
    g_dirty = 1;
    
    g_snake.dir = g_snake.next_dir;
    int dx = 0, dy = 0;
    if (g_snake.dir == 0) dy = -1;
    else if (g_snake.dir == 1) dy = 1;
    else if (g_snake.dir == 2) dx = -1;
    else if (g_snake.dir == 3) dx = 1;
    
    int nx = g_snake.seg_x[0] + dx;
    int ny = g_snake.seg_y[0] + dy;
    
    /* wall collision */
    if (nx < 0 || nx >= SNAKE_COLS || ny < 0 || ny >= SNAKE_ROWS) {
        g_snake.gameover = 1; return;
    }
    /* self collision */
    for (int i = 0; i < g_snake.seg_len; i++) {
        if (g_snake.seg_x[i] == nx && g_snake.seg_y[i] == ny) {
            g_snake.gameover = 1; return;
        }
    }
    /* move */
    for (int i = g_snake.seg_len - 1; i > 0; i--) {
        g_snake.seg_x[i] = g_snake.seg_x[i - 1];
        g_snake.seg_y[i] = g_snake.seg_y[i - 1];
    }
    g_snake.seg_x[0] = nx; g_snake.seg_y[0] = ny;
    
    /* food */
    if (nx == g_snake.food_x && ny == g_snake.food_y) {
        g_snake.seg_len++;
        if (g_snake.seg_len > SNAKE_MAX) g_snake.seg_len = SNAKE_MAX;
        g_snake.score += 10;
        g_snake.seg_x[g_snake.seg_len - 1] = g_snake.seg_x[g_snake.seg_len - 2];
        g_snake.seg_y[g_snake.seg_len - 1] = g_snake.seg_y[g_snake.seg_len - 2];
        /* new food */
        g_snake.food_x = (g_snake.food_x + 7) % SNAKE_COLS;
        g_snake.food_y = (g_snake.food_y + 13) % SNAKE_ROWS;
    }
}

static void update_pong(void) {
    if (g_pong.gameover) return;
    g_pong.tick++;
    if (g_pong.tick < 3) return;
    g_pong.tick = 0;
    g_dirty = 1;
    
    g_pong.ball_x += g_pong.ball_dx;
    g_pong.ball_y += g_pong.ball_dy;
    
    /* top/bottom bounce */
    if (g_pong.ball_y < 0.02f || g_pong.ball_y > 0.98f)
        g_pong.ball_dy = -g_pong.ball_dy;
    
    /* player paddle */
    int cw = (int)g_info.width;
    int ch = (int)g_info.height - TITLEBAR_H;
    int bx = (int)(g_pong.ball_x * cw);
    int by = (int)(g_pong.ball_y * ch) + TITLEBAR_H;
    int ppy = (int)(g_pong.paddle_y * ch) + TITLEBAR_H;
    
    if (bx < 24 && bx > 6 && by >= ppy - 25 && by <= ppy + 25) {
        g_pong.ball_dx = -g_pong.ball_dx;
        g_pong.ball_x += g_pong.ball_dx * 2;
    }
    
    /* AI paddle follows ball */
    float target = g_pong.ball_y;
    if (g_pong.paddle_y < target - 0.03f) g_pong.paddle_y += 0.02f;
    if (g_pong.paddle_y > target + 0.03f) g_pong.paddle_y -= 0.02f;
    
    /* scoring */
    if (g_pong.ball_x < -0.02f) {
        g_pong.ai_score++;
        if (g_pong.ai_score >= 10) g_pong.gameover = 1;
        else { g_pong.ball_x = 0.5f; g_pong.ball_y = 0.5f; }
    }
    if (g_pong.ball_x > 1.02f) {
        g_pong.player_score++;
        if (g_pong.player_score >= 10) g_pong.gameover = 1;
        else { g_pong.ball_x = 0.5f; g_pong.ball_y = 0.5f; }
    }
}

static void update_tetris(void) {
    if (g_tetris.gameover) return;
    g_tetris.tick++;
    if (g_tetris.tick < 30) return;
    g_tetris.tick = 0;
    g_dirty = 1;
    tetris_drop();
}

/* ── Process keyboard events ──────────────────────────────────────────── */
static void process_keys(void) {
    key_event_t ev;
    while (read_keys(&ev) == 0) {
        if (!ev.pressed) continue;

        if (g_active_app == APP_TERMINAL) {
            int c = keycode_to_ascii(ev.keycode, ev.mods);
            if (c == '\n') {
                term_run_shell();
                g_dirty = 1;
            } else if (c == '\b') {
                if (g_term.input_len > 0) {
                    g_term.input[--g_term.input_len] = '\0';
                    g_dirty = 1;
                }
            } else if (c > 0 && c < 128 && g_term.input_len < 255) {
                g_term.input[g_term.input_len++] = (char)c;
                g_term.input[g_term.input_len] = '\0';
                g_dirty = 1;
            }
        } else if (g_active_app == APP_EDITOR) {
            int c = keycode_to_ascii(ev.keycode, ev.mods);
            if (c == '\n') {
                if (g_editor.num_lines < EDITOR_ROWS) {
                    for (int r = g_editor.num_lines; r > g_editor.row; r--)
                        strcpy(g_editor.lines[r], g_editor.lines[r - 1]);
                    int rest = (int)strlen(g_editor.lines[g_editor.row] + g_editor.col);
                    memmove(g_editor.lines[g_editor.row + 1],
                            g_editor.lines[g_editor.row] + g_editor.col, (size_t)rest + 1);
                    g_editor.lines[g_editor.row][g_editor.col] = '\0';
                    g_editor.num_lines++;
                    g_editor.row++;
                    g_editor.col = 0;
                    if (g_editor.row >= g_editor.scroll_row + ((int)g_info.height - TITLEBAR_H - 20) / (FONT_H + 4))
                        g_editor.scroll_row++;
                    g_dirty = 1;
                }
            } else if (c == '\b') {
                if (g_editor.col > 0) {
                    int len = (int)strlen(g_editor.lines[g_editor.row]);
                    if (g_editor.col <= len) {
                        memmove(g_editor.lines[g_editor.row] + g_editor.col - 1,
                                g_editor.lines[g_editor.row] + g_editor.col,
                                (size_t)(len - g_editor.col + 1));
                    }
                    g_editor.col--;
                    g_dirty = 1;
                } else if (g_editor.row > 0) {
                    int prev_len = (int)strlen(g_editor.lines[g_editor.row - 1]);
                    g_editor.col = prev_len;
                    char *dst = g_editor.lines[g_editor.row - 1] + prev_len;
                    char *src = g_editor.lines[g_editor.row];
                    while ((*dst++ = *src++));
                    for (int r = g_editor.row; r < g_editor.num_lines - 1; r++)
                        strcpy(g_editor.lines[r], g_editor.lines[r + 1]);
                    memset(g_editor.lines[g_editor.num_lines - 1], 0, EDITOR_COLS + 1);
                    g_editor.num_lines--;
                    g_editor.row--;
                    if (g_editor.row < g_editor.scroll_row)
                        g_editor.scroll_row--;
                    g_dirty = 1;
                }
            } else if (c > 0 && c < 128) {
                int len = (int)strlen(g_editor.lines[g_editor.row]);
                if (len < EDITOR_COLS) {
                    memmove(g_editor.lines[g_editor.row] + g_editor.col + 1,
                            g_editor.lines[g_editor.row] + g_editor.col,
                            (size_t)(len - g_editor.col + 1));
                    g_editor.lines[g_editor.row][g_editor.col] = (char)c;
                    g_editor.col++;
                    g_dirty = 1;
                }
            } else if (ev.keycode == KEY_LEFT) {
                if (g_editor.col > 0) g_editor.col--;
                g_dirty = 1;
            } else if (ev.keycode == KEY_RIGHT) {
                int len = (int)strlen(g_editor.lines[g_editor.row]);
                if (g_editor.col < len) g_editor.col++;
                g_dirty = 1;
            } else if (ev.keycode == KEY_UP) {
                if (g_editor.row > 0) {
                    g_editor.row--;
                    int len = (int)strlen(g_editor.lines[g_editor.row]);
                    if (g_editor.col > len) g_editor.col = len;
                    if (g_editor.row < g_editor.scroll_row)
                        g_editor.scroll_row--;
                    g_dirty = 1;
                }
            } else if (ev.keycode == KEY_DOWN) {
                if (g_editor.row < g_editor.num_lines - 1) {
                    g_editor.row++;
                    int len = (int)strlen(g_editor.lines[g_editor.row]);
                    if (g_editor.col > len) g_editor.col = len;
                    int rows = ((int)g_info.height - TITLEBAR_H - 20) / (FONT_H + 4);
                    if (g_editor.row >= g_editor.scroll_row + rows)
                        g_editor.scroll_row++;
                    g_dirty = 1;
                }
            }
        } else if (g_active_app == APP_SNAKE) {
            if (g_snake.gameover && ev.keycode == 0x15) { /* R */
                snake_reset(); g_dirty = 1;
            } else if (!g_snake.gameover) {
                if (ev.keycode == KEY_UP || ev.keycode == 0x1A) g_snake.next_dir = 0; /* W */
                else if (ev.keycode == KEY_DOWN || ev.keycode == 0x16) g_snake.next_dir = 1; /* S */
                else if (ev.keycode == KEY_LEFT || ev.keycode == 0x04) g_snake.next_dir = 2; /* A */
                else if (ev.keycode == KEY_RIGHT || ev.keycode == 0x07) g_snake.next_dir = 3; /* D */
            }
        } else if (g_active_app == APP_PONG) {
            if (g_pong.gameover && ev.keycode == 0x15) { /* R */
                pong_reset(); g_dirty = 1;
            } else if (!g_pong.gameover) {
                if (ev.keycode == KEY_UP || ev.keycode == 0x1A) { /* W */
                    g_pong.paddle_y -= 0.06f;
                    if (g_pong.paddle_y < 0.05f) g_pong.paddle_y = 0.05f;
                }
                if (ev.keycode == KEY_DOWN || ev.keycode == 0x16) { /* S */
                    g_pong.paddle_y += 0.06f;
                    if (g_pong.paddle_y > 0.95f) g_pong.paddle_y = 0.95f;
                }
            }
        } else if (ev.keycode == KEY_V) {
            if (g_active_app == APP_TERMINAL && !g_anim.active) {
                g_active_app = -1;
            } else if (g_active_app < 0) {
                g_active_app = APP_TERMINAL;
            }
            g_dirty = 1;
        } else if (g_active_app == APP_TETRIS) {
            if (g_tetris.gameover && ev.keycode == 0x15) { /* R */
                tetris_reset(); g_dirty = 1;
            } else if (!g_tetris.gameover) {
                if (ev.keycode == KEY_LEFT || ev.keycode == 0x04) { /* A */
                    if (!tetris_collide(g_tetris.piece_type, g_tetris.piece_rot,
                                        g_tetris.piece_x - 1, g_tetris.piece_y))
                        g_tetris.piece_x--;
                    g_dirty = 1;
                } else if (ev.keycode == KEY_RIGHT || ev.keycode == 0x07) { /* D */
                    if (!tetris_collide(g_tetris.piece_type, g_tetris.piece_rot,
                                        g_tetris.piece_x + 1, g_tetris.piece_y))
                        g_tetris.piece_x++;
                    g_dirty = 1;
                } else if (ev.keycode == KEY_DOWN || ev.keycode == 0x16) { /* S */
                    tetris_drop();
                    g_dirty = 1;
                } else if (ev.keycode == KEY_UP || ev.keycode == 0x1A) { /* W */
                    int new_rot = (g_tetris.piece_rot + 1) % 4;
                    if (!tetris_collide(g_tetris.piece_type, new_rot,
                                        g_tetris.piece_x, g_tetris.piece_y))
                        g_tetris.piece_rot = new_rot;
                    g_dirty = 1;
                }
            }
        }
    }
}

/* ── Main loop ──────────────────────────────────────────────────────────── */
int main(void) {
    if (zf_connect() != 0) return 1;
    if (zf_get_info(&g_info) != 0 || !g_info.connected) return 1;
    if (zf_create_buffer(g_info.width, g_info.height, &g_buf) != 0) return 1;
    if (g_info.width > MAX_W || g_info.height > MAX_H) return 1;

    cursor_x = (int)g_info.width / 2;
    cursor_y = (int)g_info.height / 2;
    zf_set_cursor(cursor_x, cursor_y);

    calc_reset();

    /* Initialize apps */
    memset(&g_term, 0, sizeof(g_term));
    term_add_scroll("Zirvium OS Terminal v0.1");
    term_add_scroll("");
    editor_reset();
    snake_reset();
    pong_reset();
    tetris_reset();

    /* Initialize tile order and sizes */
    for (int i = 0; i < NUM_APPS; i++) {
        g_tile_order[i] = i;
        g_tile_sizes[i] = 0;
    }
    g_tile_layout_dirty = 1;

    size_t fb_size = (size_t)g_buf.stride * g_buf.height;

    /* ── Boot splash ──────────────────────────────────────────────────────── */
    draw_boot_splash((uint32_t *)compositor_fb, g_info.width, g_info.height);
    zf_write_buffer(&g_buf, compositor_fb, fb_size);
    zf_present(&g_buf);
    msleep(2500);

    render_frame();

    if (zf_write_buffer(&g_buf, compositor_fb, fb_size) < 0) return 1;
    if (zf_present(&g_buf) != 0) return 1;

    for (;;) {
        process_mouse();
        process_keys();

        /* Update active game */
        if (g_active_app == APP_SNAKE)  update_snake();
        if (g_active_app == APP_PONG)   update_pong();
        if (g_active_app == APP_TETRIS) update_tetris();

        if (g_anim.active) {
            g_anim.frame++;
            if (g_anim.frame >= g_anim_frames) {
                g_anim.active = 0;
                if (g_anim.opening) {
                    g_active_app = g_anim.app_idx;
                } else {
                    g_active_app = -1;
                }
            }
            g_dirty = 1;
        }

        /* ── Shutdown animation timing ──────────────────────────────── */
        if (g_confirm_shutdown == 2) {
            uint64_t now = uptime();
            uint64_t elapsed = now - g_shutdown_start_s;
            int new_dot = (int)(elapsed % 4);
            if (new_dot != g_shutdown_dot) {
                g_shutdown_dot = new_dot;
                g_dirty = 1;
            }
            if (elapsed >= 5) {
                zf_disconnect();
                shutdown();
            }
            g_dirty = 1;
        }

        struct datetime dt;
        if (getdatetime(&dt) == 0) {
            if (dt.minute != last_minute || dt.second != last_second) {
                last_minute = dt.minute;
                last_second = dt.second;
                g_dirty = 1;
            }
        }

        if (g_dirty) {
            render_frame();
            zf_write_buffer(&g_buf, compositor_fb, fb_size);
            g_dirty = 0;
        }

        if (zf_present(&g_buf) != 0) return 1;
        msleep(g_frame_rate == 60 ? 16 : 33);
    }
}
