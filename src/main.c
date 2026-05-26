#include <zirvflux.h>
#include <zirvtk.h>
#include <unistd.h>
#include <stdio.h>
#include <datetime.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

typedef zf_mouse_event_t mouse_event_t;

/* Freestanding abs (not in zirvlibc) */
static inline int iabs(int x) { return x < 0 ? -x : x; }

#define MAX_W      1920
#define MAX_H      1080
#define FONT_W     8
#define FONT_H     13

/* ── UI26 design language ─────────────────────────────────────────── */
#define UI26_BG         0xFF0D0D1A
#define UI26_BG2        0xFF16162A
#define UI26_SURFACE    0xFF1A1A30
#define UI26_ACCENT     0xFF00D4FF
#define UI26_TEXT        0xFFFFFFFF
#define UI26_TEXT_DIM    0xFF8888BB
#define UI26_TILE_TERM  0xFF0088FF
#define UI26_TILE_CALC  0xFF00CC88
#define UI26_TILE_SETT  0xFF607D8B
#define UI26_TILE_CLOCK 0xFF34495E
#define UI26_TILE_EDITR 0xFF9B59B6
#define UI26_TILE_SNAKE 0xFF2ECC71
#define UI26_TILE_PONG  0xFF3498DB
#define UI26_TILE_TETRS 0xFFE74C3C
#define UI26_TILE_DEMO  0xFF8E44AD
#define UI26_BACK_BTN   0xFF2A2A4A
#define UI26_TILE_HOVER 0x33FFFFFF
#define UI26_DIALOG_BG  0xFF16162A
#define UI26_TILE_RADIUS 8

/* ── Wallpaper gradient presets (6 schemes) ──────────────────────────── */
#define NUM_WALLPAPERS 6
static const char *wallpaper_names[NUM_WALLPAPERS] = {
    "Deep Blue", "Twilight", "Forest", "Sunset", "Ocean", "Midnight",
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
#define TITLEBAR_H   36
#define PANEL_H     TITLEBAR_H

/* ── App menu constants ──────────────────────────────────────────────── */
#define MENU_W       180
#define MENU_ITEM_H  24

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
    ICON_ZIRVIUM,
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
    {"Terminal",   UI26_TILE_TERM,  ICON_TERMINAL},
    {"Calculator", UI26_TILE_CALC,  ICON_CALC},
    {"Settings",   UI26_TILE_SETT,  ICON_SETTINGS},
    {"Clock",      UI26_TILE_CLOCK, ICON_CLOCK},
    {"Editor",     UI26_TILE_EDITR, ICON_EDITOR},
    {"Snake",      UI26_TILE_SNAKE, ICON_SNAKE},
    {"Pong",       UI26_TILE_PONG,  ICON_PONG},
    {"Tetris",     UI26_TILE_TETRS, ICON_TETRIS},
    {"Demo",       UI26_TILE_DEMO,  ICON_DEMO},
};



/* ── Framebuffer state ────────────────────────────────────────────────── */
static uint8_t compositor_fb[MAX_W * MAX_H * 4];
static zf_buffer_t g_buf;
static zf_display_info_t g_info;
static int cursor_x = 0, cursor_y = 0;
static int prev_buttons = 0;
static int g_dirty = 1;

/* ── UI state ─────────────────────────────────────────────────────────── */
static int g_active_app = -1;
static int g_back_hover = 0;
static int g_font_scale = 1;
static int g_dark_theme = 1;    /* 1=dark, 0=light */
static int g_brightness = 80;   /* 0-100 */
static int g_frame_rate = 60;   /* 30 or 60 */
static int g_wifi_on = 1;
static int g_bt_on = 0;
static int g_volume = 75;       /* 0-100 */
static int g_power_hover = 0;
static int g_wallpaper_style = 0; /* 0-5 */
static int g_accent_color = 0;    /* 0-7 */
static int g_font_style = 1;      /* 0=Regular, 1=Bold */
static int g_settings_tab = 0;    /* 0=Display, 1=Network, 2=Audio, 3=About */

/* ── GNOME 2 App menu state ────────────────────────────────────────── */
static int g_menu_active = 0;
static int g_menu_hover = -1;

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



/* ── Dropdown menu state ────────────────────────────────────────────── */
#define DROPDOWN_MAX_ITEMS 12
static int g_dropdown_active = 0;
static int g_dropdown_x, g_dropdown_y, g_dropdown_w, g_dropdown_h;
static int g_dropdown_hover = -1;
static int g_dropdown_count = 0;
static const char *g_dropdown_items[DROPDOWN_MAX_ITEMS];
static int g_dropdown_callback = 0;
static int g_dropdown_item_h = 24;

static void dropdown_open(int dx, int dy, int count, const char **items, int callback_id) {
    g_dropdown_count = count < DROPDOWN_MAX_ITEMS ? count : DROPDOWN_MAX_ITEMS;
    for (int i = 0; i < g_dropdown_count; i++) g_dropdown_items[i] = items[i];
    g_dropdown_callback = callback_id;
    g_dropdown_x = dx;
    g_dropdown_y = dy;
    g_dropdown_w = 160;
    g_dropdown_h = g_dropdown_count * g_dropdown_item_h + 8;
    g_dropdown_hover = -1;
    g_dropdown_active = 1;
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

/* ── Rounded rectangle (simple AA corners, no sub-pixel sampling) ───── */
static void fill_round_rect(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                            int x, int y, int w, int h, int r, uint32_t color) {
    if (r < 1) r = 1;
    int r2 = r * r;
    for (int py = y; py < y + h && py < (int)fb_h; py++) {
        for (int px = x; px < x + w && px < (int)fb_w; px++) {
            float dx = 0.0f, dy = 0.0f;
            int corner = 0;
            if (px < x + r && py < y + r) {
                dx = (float)(px - (x + r)) + 0.5f;
                dy = (float)(py - (y + r)) + 0.5f;
                corner = 1;
            } else if (px >= x + w - r && py < y + r) {
                dx = (float)(px - (x + w - r - 1)) - 0.5f;
                dy = (float)(py - (y + r)) + 0.5f;
                corner = 1;
            } else if (px < x + r && py >= y + h - r) {
                dx = (float)(px - (x + r)) + 0.5f;
                dy = (float)(py - (y + h - r - 1)) - 0.5f;
                corner = 1;
            } else if (px >= x + w - r && py >= y + h - r) {
                dx = (float)(px - (x + w - r - 1)) - 0.5f;
                dy = (float)(py - (y + h - r - 1)) - 0.5f;
                corner = 1;
            }
            if (corner) {
                float dist2 = dx * dx + dy * dy;
                if (dist2 >= (float)(r2 + r)) continue;  /* hard edge for perf */
                fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
                continue;
            }
            fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
        }
    }
}

/* ── Glassmorphism panel: semi-transparent with rounded corners and border highlight ── */
static void fill_glass_panel(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                             int x, int y, int w, int h, int r, uint32_t tint, uint32_t border) {
    fill_round_rect(fb, fb_w, fb_h, x, y, w, h, r, tint);
    if (border) {
        /* top edge */
        uint32_t b2 = blend(border, tint, 100);
        for (int px = x + r; px < x + w - r; px++) {
            int py = y;
            if (py >= 0 && py < (int)fb_h && px >= 0 && px < (int)fb_w)
                fb[(uint32_t)py * fb_w + (uint32_t)px] = b2;
        }
        /* bottom edge */
        for (int px = x + r; px < x + w - r; px++) {
            int py = y + h - 1;
            if (py >= 0 && py < (int)fb_h && px >= 0 && px < (int)fb_w)
                fb[(uint32_t)py * fb_w + (uint32_t)px] = b2;
        }
    }
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
    static const uint8_t cov_alpha[9] = {80, 110, 130, 150, 170, 195, 215, 240, 255};
    for (int row = 0; row < FONT_H && (y + row) < (int)fb_h; row++) {
        uint8_t bits = bm[row];
        if (bits == 0) continue;
        for (int col = 0; col < FONT_W && (x + col) < (int)fb_w; col++) {
            if (bits & (1 << (7 - col))) {
                int px = x + col, py = y + row;
                if (px < 0 || py < 0) continue;
                uint32_t off = (uint32_t)py * fb_w + (uint32_t)px;
                int count = 0;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = row + dr, nc = col + dc;
                        if (nr >= 0 && nr < FONT_H && nc >= 0 && nc < FONT_W) {
                            if (bm[nr] & (1 << (7 - nc))) count++;
                        }
                    }
                }
                uint8_t alpha = cov_alpha[count];
                if (alpha >= 255) fb[off] = color;
                else fb[off] = blend(color, fb[off], alpha);
            }
        }
    }
}

/* ── Dropdown menu drawing ──────────────────────────────────────────────── */
static void draw_text(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x, int y, const char *str, uint32_t color);
static void draw_dropdown(uint32_t *fb, uint32_t fb_w, uint32_t fb_h) {
    if (!g_dropdown_active) return;
    uint32_t accent = get_accent_color();
    uint32_t glass_bg = blend(0xE0202020, 0x00000000, 200);
    uint32_t hover_bg = blend(accent, 0x00000000, 60);
    int ih = g_dropdown_item_h;

    fill_glass_panel(fb, fb_w, fb_h, g_dropdown_x, g_dropdown_y,
                     g_dropdown_w, g_dropdown_h, 6, glass_bg, accent);

    for (int i = 0; i < g_dropdown_count; i++) {
        int iy = g_dropdown_y + 4 + i * ih;
        if (i == g_dropdown_hover)
            fill_round_rect(fb, fb_w, fb_h, g_dropdown_x + 2, iy,
                            g_dropdown_w - 4, ih, 4, hover_bg);
        draw_text(fb, fb_w, fb_h, g_dropdown_x + 10, iy + (ih - FONT_H) / 2,
                  g_dropdown_items[i], 0xFFE0E0E0);
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


static int point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
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
    } else {
        ctx_add("Settings");
    }
    g_ctx_active = 1;
    g_dirty = 1;
}

static void ctx_execute(int item) {
    if (item < 0 || item >= g_ctx_count) return;
    const char *label = g_ctx_labels[item];
    if (g_ctx_app >= 0) {
        if (strcmp(label, "Open") == 0) {
            g_active_app = g_ctx_app;
            g_dirty = 1;
        }
    } else {
        if (strcmp(label, "Settings") == 0) {
            g_active_app = APP_SETTINGS;
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
    uint32_t text_col = 0xFFE0E0E0;
    uint32_t hover_bg = blend(col_accent, 0x00000000, 60);

    /* Glass panel background with rounded corners */
    uint32_t glass_bg = blend(0xE0202020, 0x00000000, 200);
    fill_glass_panel(fb, fb_w, fb_h, mx, my, mw, mh, 6, glass_bg, col_accent);

    for (int i = 0; i < g_ctx_count; i++) {
        int iy = my + 4 + i * 24;
        if (i == g_ctx_hover)
            fill_round_rect(fb, fb_w, fb_h, mx + 2, iy, mw - 4, 24, 4, hover_bg);
        draw_text(fb, fb_w, fb_h, mx + 8, iy + 4, g_ctx_labels[i], text_col);
    }
}

/* ── Forward declarations ─────────────────────────────────────────────── */
extern void draw_demoapp(uint32_t *fb, uint32_t w, uint32_t h);
extern int demo_hit_test(int mx, int my, int w);

/* ── Zirvium logo (procedural from logo.svg) ──────────────────────────── */
static void draw_zirvium_logo(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                              int cx, int cy, int size, uint32_t color) {
    int bar_h = size / 12;
    if (bar_h < 1) bar_h = 1;
    struct { int y_off; int half_w; } bars[] = {
        {-24, 4}, {-16, 10}, {-8, 16}, {0, 22}, {8, 16}, {16, 10}, {24, 4},
    };
    for (int i = 0; i < 7; i++) {
        int y = cy + bars[i].y_off * size / 64;
        int hw = bars[i].half_w * size / 64;
        if (hw < 1) hw = 1;
        uint32_t c = (i == 2 || i == 4) ? 0xFF4488FF : color;
        fill_rect(fb, fb_w, fb_h, cx - hw, y - bar_h / 2, hw * 2, bar_h, c);
    }
}

/* ── Procedural app icons ─────────────────────────────────────────────── */
/* draw_icon wrapper removed — call ztk_draw_icon directly */

/* ── Global top panel (always visible) ──────────────────────────────── */
static void draw_top_panel(uint32_t *fb, uint32_t w, uint32_t h, int app_active) {
    fill_rect(fb, w, h, 0, 0, (int)w, PANEL_H, UI26_BG2);
    uint32_t accent = get_accent_color();

    if (app_active && g_active_app >= 0) {
        int bs = 18;
        int bx = 8, by = (PANEL_H - bs) / 2;
        if (g_back_hover)
            fill_rect(fb, w, h, bx - 2, by - 2, bs + 4, bs + 4, blend(accent, 0x00000000, 100));
        const char *an = g_apps[g_active_app].name;
        int nw = (int)strlen(an) * (FONT_W + 1);
        draw_text(fb, w, h, (int)w / 2 - nw / 2, (PANEL_H - FONT_H) / 2, an, UI26_TEXT);
    } else {
        uint32_t mc = g_menu_active ? blend(accent, 0x00000000, 120) : 0;
        if (g_menu_active)
            fill_rect(fb, w, h, 8, 0, MENU_W - 16, PANEL_H, mc);
        draw_text(fb, w, h, 12, (PANEL_H - FONT_H) / 2, "Zirvium", accent);
    }

    struct datetime dt;
    char tb[6];
    if (getdatetime(&dt) == 0) {
        tb[0] = '0' + dt.hour / 10;
        tb[1] = '0' + dt.hour % 10;
        tb[2] = ':';
        tb[3] = '0' + dt.minute / 10;
        tb[4] = '0' + dt.minute % 10;
        tb[5] = '\0';
    } else { strcpy(tb, "--:--"); }
    int cx = (int)w - 20 - 5 * (FONT_W + 1);
    draw_text(fb, w, h, cx, (PANEL_H - FONT_H) / 2, tb, UI26_TEXT_DIM);
    uint32_t pwr_c = g_power_hover ? 0xFF774444 : 0xFF553333;
    fill_rect(fb, w, h, cx - 22, (PANEL_H - 14) / 2, 16, 14, pwr_c);
    draw_text(fb, w, h, cx - 20, (PANEL_H - FONT_H) / 2, "x", UI26_TEXT_DIM);
}

/* ── GNOME 2 app menu (dropdown below panel) ────────────────────────── */
static void draw_app_menu(uint32_t *fb, uint32_t w, uint32_t h) {
    if (!g_menu_active) return;
    uint32_t accent = get_accent_color();
    int mh = NUM_APPS * MENU_ITEM_H + 8;
    int mx = 8, my = PANEL_H;
    int mw = MENU_W;
    if (my + mh > (int)h) my = (int)h - mh;
    uint32_t mcol = blend(0xE016162A, 0x00000000, 200);
    fill_round_rect(fb, w, h, mx, my, mw, mh, 6, mcol);
    for (int i = 0; i < NUM_APPS; i++) {
        int iy = my + 4 + i * MENU_ITEM_H;
        if (i == g_menu_hover)
            fill_rect(fb, w, h, mx + 2, iy, mw - 4, MENU_ITEM_H, blend(accent, 0x00000000, 60));
        uint32_t ic = (i == g_menu_hover) ? UI26_TEXT : UI26_TEXT_DIM;
        draw_text(fb, w, h, mx + 10, iy + (MENU_ITEM_H - FONT_H) / 2, g_apps[i].name, ic);
    }
}

static void draw_start_screen(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, 0, (int)w, (int)h, UI26_BG);
    draw_top_panel(fb, w, h, 0);
    draw_app_menu(fb, w, h);
}

/* ── App renderers ────────────────────────────────────────────────────── */
static void draw_terminal(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, 0, (int)w, (int)h, UI26_BG);
    /* Accent sweep line below title bar */
    uint32_t accent = get_accent_color();
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, 1, blend(accent, 0x00000000, 80));
    int y = TITLEBAR_H + 12;
    int max_lines = ((int)h - TITLEBAR_H - 14) / (FONT_H + 2);
    int start = (g_term.scroll_count > max_lines)
                ? g_term.scroll_count - max_lines : 0;
    for (int i = start; i < g_term.scroll_count; i++) {
        uint32_t tc = (g_term.scroll[i][0] == '>') ? accent : 0xFF00CC66;
        draw_text(fb, w, h, 12, y, g_term.scroll[i], tc);
        y += FONT_H + 2;
    }

    /* Prompt + input line with rounded input area */
    char prompt[280];
    int plen = snprintf(prompt, sizeof(prompt), "zirvium:~$ %s",
                        g_term.input);
    if (plen < 0 || plen >= (int)sizeof(prompt)) plen = (int)sizeof(prompt) - 1;
    prompt[plen] = '\0';
    int input_h = FONT_H + 4;
    fill_round_rect(fb, w, h, 8, y - 2, (int)w - 16, input_h, 4, 0xFF1A1A30);
    draw_text(fb, w, h, 14, y, prompt, 0xFF00CC66);

    /* Cursor */
    int cw = (int)strlen("zirvium:~$ ") * (FONT_W + 1);
    int ci = (int)strlen(g_term.input);
    int cx = 14 + (cw + ci * (FONT_W + 1)) * g_font_scale;
    fill_rect(fb, w, h, cx, y, 6 * g_font_scale, FONT_H * g_font_scale, accent);
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
    uint32_t bg = UI26_SURFACE;
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

    fill_round_rect(fb, w, h, 4, TITLEBAR_H + 10, (int)w - 8, 60, 8, display_bg);
    fill_rect(fb, w, h, 4, TITLEBAR_H + 68, (int)w - 8, 1, 0xFF333355);
    draw_text(fb, w, h, 16, TITLEBAR_H + 28, g_calc.disp_str, 0xFF00FF88);

    int num_btns = (int)(sizeof(buttons) / sizeof(buttons[0])) - 1;
    for (int i = 0; i < num_btns; i++) {
        int bx = start_x + buttons[i].col * bw;
        int by = start_y + buttons[i].row * bh;
        int bww = buttons[i].wide ? bw * 2 : bw;
        int hovered = (g_calc_btn_hover == i);
        uint32_t bc = hovered ? btn_hov : buttons[i].color;
        fill_round_rect(fb, w, h, bx + 2, by + 2, bww - 4, bh - 4, 6, bc);
        int lx = bx + (bww - (int)strlen(buttons[i].label) * (FONT_W + 1)) / 2;
        int ly = by + (bh - FONT_H) / 2;
        draw_text(fb, w, h, lx, ly, buttons[i].label, UI26_TEXT);
    }
}

/* ── Clock App ──────────────────────────────────────────────────────────── */
static void draw_clock_app(uint32_t *fb, uint32_t w, uint32_t h) {
    uint32_t bg = g_dark_theme ? UI26_BG : 0xFFE8E8F0;
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
        uint32_t accent = get_accent_color();
        draw_text(fb, w, h, 24, TITLEBAR_H + 60, time_str, accent);
        g_font_scale = saved_scale;
        char date_str[32];
        snprintf(date_str, 32, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
        fill_round_rect(fb, w, h, 24, TITLEBAR_H + 130, 160, 40, 8, blend(accent, 0x00000000, 100));
        draw_text(fb, w, h, 36, TITLEBAR_H + 140, date_str,
                  UI26_TEXT);
    } else {
        draw_text(fb, w, h, 24, TITLEBAR_H + 60, "No RTC", UI26_TEXT);
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
    fill_round_rect(fb, w, g_info.height, 24, y, (int)w - 48, SETTING_ROW_H, 6, 0xFF222244);
    draw_text(fb, w, g_info.height, 36, y + (SETTING_ROW_H - FONT_H) / 2, label, UI26_TEXT);
    draw_text(fb, w, g_info.height, (int)w - 120, y + (SETTING_ROW_H - FONT_H) / 2, val, val_color);
    /* Subtle right arrow indicator */
    draw_text(fb, w, g_info.height, (int)w - 44, y + (SETTING_ROW_H - FONT_H) / 2, ">", 0xFF555577);
}

static const char *g_settings_tab_names[4] = {"Display", "Network", "Audio", "About"};

static void draw_settings_tabs(uint32_t *fb, uint32_t w, uint32_t h) {
    int tw = (int)w / 4;
    int tab_pad = 6;
    for (int i = 0; i < 4; i++) {
        int x = i * tw + tab_pad;
        int tw_inner = tw - tab_pad * 2;
        uint32_t col = (i == g_settings_tab) ? blend(0x44FFFFFF, 0xFF3388CC, 100) : 0xFF1E1E38;
        fill_round_rect(fb, w, h, x, SETTINGS_TABS_Y, tw_inner, SETTINGS_TAB_H, 8, col);
        draw_text(fb, w, h, x + (tw_inner - (int)strlen(g_settings_tab_names[i]) * (FONT_W + 1)) / 2,
                  SETTINGS_TABS_Y + (SETTINGS_TAB_H - FONT_H) / 2,
                  g_settings_tab_names[i], UI26_TEXT);
    }
}

static void draw_settings(uint32_t *fb, uint32_t w, uint32_t h) {
    uint32_t bg = g_dark_theme ? UI26_SURFACE : 0xFFE8E8F0;
    uint32_t text_c   = g_dark_theme ? UI26_TEXT : 0xFF222222;
    uint32_t dim_c    = g_dark_theme ? UI26_TEXT_DIM : 0xFF666666;
    uint32_t accent_c = get_accent_color();
    uint32_t green_c  = 0xFF44CC44;
    uint32_t bar_bg   = g_dark_theme ? 0xFF2A2A48 : 0xFFCCCCDD;

    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, bg);
    draw_settings_tabs(fb, w, h);

    int bar_x = 160, bar_w = (int)w - 200;
    int slid_y, fill;

    /* Section header */
    uint32_t section_col = blend(accent_c, 0x00000000, 80);
    fill_rect(fb, w, h, 24, SETTINGS_TABS_Y + SETTINGS_TAB_H + 4, 4, 20, section_col);

    switch (g_settings_tab) {
    case 0: { /* Display tab */
        int r = 0;

        /* Row 0: Wallpaper */
        draw_toggle_row(fb, w, r, "Wallpaper",
                        wallpaper_names[g_wallpaper_style], accent_c);

        /* Row 1: Accent Color */
        r = 1;
        {
            int ay = setting_row_y(r);
            fill_round_rect(fb, w, g_info.height, 24, ay, (int)w - 48, SETTING_ROW_H, 6, 0xFF222244);
            draw_text(fb, w, g_info.height, 36, ay + (SETTING_ROW_H - FONT_H) / 2, "Accent Color", text_c);
            int swatch_x = (int)w - 120;
            int swatch_y = ay + (SETTING_ROW_H - 14) / 2;
            fill_rect(fb, w, g_info.height, swatch_x, swatch_y, 14, 14, accent_colors[g_accent_color]);
            fill_rect(fb, w, g_info.height, swatch_x, swatch_y, 14, 1, 0xAAFFFFFF);
            fill_rect(fb, w, g_info.height, swatch_x, swatch_y, 1, 14, 0xAAFFFFFF);
            draw_text(fb, w, g_info.height, swatch_x + 20, ay + (SETTING_ROW_H - FONT_H) / 2,
                      accent_names[g_accent_color], accent_c);
            draw_text(fb, w, g_info.height, (int)w - 44, ay + (SETTING_ROW_H - FONT_H) / 2, ">", 0xFF555577);
        }

        /* Row 2: Font Style */
        r = 2;
        draw_toggle_row(fb, w, r, "Font Style",
                        g_font_style ? "Bold" : "Regular", accent_c);

        /* Row 3: Font Size */
        r = 3;
        draw_toggle_row(fb, w, r, "Font Size",
                        g_font_scale > 1 ? "Large" : "Small", accent_c);

        /* Row 4: Dark Theme */
        r = 4;
        draw_toggle_row(fb, w, r, "Dark Theme",
                        g_dark_theme ? "On" : "Off", g_dark_theme ? green_c : dim_c);

        /* Row 6: Brightness slider */
        r = 6;
        slid_y = setting_row_y(r) + (SETTING_ROW_H - 16) / 2;
        fill_round_rect(fb, w, h, 24, setting_row_y(r), (int)w - 48, SETTING_ROW_H, 6, 0xFF222244);
        draw_text(fb, w, h, 36, slid_y + (16 - FONT_H) / 2, "Brightness", text_c);
        fill_round_rect(fb, w, h, bar_x, slid_y, bar_w, 16, 4, bar_bg);
        fill = bar_w * g_brightness / 100;
        if (fill > 0) fill_round_rect(fb, w, h, bar_x, slid_y, fill, 16, 4, accent_c);
        char bri[8];
        snprintf(bri, sizeof(bri), "%d%%", g_brightness);
        draw_text(fb, w, h, bar_x + 4, slid_y + (16 - FONT_H) / 2, bri, UI26_TEXT);

        /* Row 7: Frame Rate */
        r = 7;
        draw_toggle_row(fb, w, r, "Frame Rate",
                        g_frame_rate == 60 ? "60 FPS" : "30 FPS", accent_c);
        break;
    }
    case 1: { /* Network tab */
        int r = 0;
        fill_round_rect(fb, w, h, 24, setting_row_y(r), (int)w - 48, SETTING_ROW_H, 6, 0xFF222244);
        draw_text(fb, w, h, 36, setting_row_y(r) + (SETTING_ROW_H - FONT_H) / 2, "Wi-Fi", text_c);
        draw_text(fb, w, h, (int)w - 120, setting_row_y(r) + (SETTING_ROW_H - FONT_H) / 2,
                  g_wifi_on ? "On" : "Off", g_wifi_on ? green_c : dim_c);
        r = 1;
        fill_round_rect(fb, w, h, 24, setting_row_y(r), (int)w - 48, SETTING_ROW_H, 6, 0xFF222244);
        draw_text(fb, w, h, 36, setting_row_y(r) + (SETTING_ROW_H - FONT_H) / 2, "Bluetooth", text_c);
        draw_text(fb, w, h, (int)w - 120, setting_row_y(r) + (SETTING_ROW_H - FONT_H) / 2,
                  g_bt_on ? "On" : "Off", g_bt_on ? green_c : dim_c);
        break;
    }
    case 2: { /* Audio tab */
        int r = 0;
        fill_round_rect(fb, w, h, 24, setting_row_y(r), (int)w - 48, SETTING_ROW_H, 6, 0xFF222244);
        slid_y = setting_row_y(r) + (SETTING_ROW_H - 16) / 2;
        draw_text(fb, w, h, 36, slid_y + (16 - FONT_H) / 2, "Volume", text_c);
        fill_round_rect(fb, w, h, bar_x, slid_y, bar_w, 16, 4, bar_bg);
        fill = bar_w * g_volume / 100;
        if (fill > 0) fill_round_rect(fb, w, h, bar_x, slid_y, fill, 16, 4, accent_c);
        char vol[8];
        snprintf(vol, sizeof(vol), "%d%%", g_volume);
        draw_text(fb, w, h, bar_x + 4, slid_y + (16 - FONT_H) / 2, vol, UI26_TEXT);
        break;
    }
    case 3: { /* About tab */
        int logo_size = 80;
        int logo_cx = 56 + logo_size / 2;
        int logo_cy = setting_row_y(0) + SETTING_ROW_H / 2;
        fill_round_rect(fb, w, h, 24, setting_row_y(0), (int)w - 48, 100, 8, 0xFF1A1A30);
        draw_zirvium_logo(fb, w, h, logo_cx, logo_cy, logo_size, accent_c);
        int r = 0;
        draw_text(fb, w, h, logo_cx + logo_size / 2 + 12,
                  setting_row_y(r) + (10),
                  "Zirvium OS v0.1.0", text_c);
        r = 1;
        draw_text(fb, w, h, logo_cx + logo_size / 2 + 12,
                  setting_row_y(r) + (10),
                  "DisplayJet MAEM compositor", dim_c);
        r = 2;
        draw_text(fb, w, h, logo_cx + logo_size / 2 + 12,
                  setting_row_y(r) + (10),
                  "Kernel: MOSIX x86_64", dim_c);
        r = 3;
        draw_text(fb, w, h, logo_cx + logo_size / 2 + 12,
                  setting_row_y(r) + (10),
                  "UI26 design language", dim_c);
        break;
    }
    }
}

/* ── Text Editor ───────────────────────────────────────────────────────── */
static void draw_editor(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, UI26_BG);
    int rows = ((int)h - TITLEBAR_H - 20) / (FONT_H + 4);
    int start_row = g_editor.scroll_row;
    int end_row = start_row + rows;
    if (end_row > g_editor.num_lines) end_row = g_editor.num_lines;
    int y = TITLEBAR_H + 10;
    for (int r = start_row; r < end_row; r++) {
        uint32_t c = (r == g_editor.row) ? 0xFF2A2A4E : 0xFF1A1A30;
        fill_round_rect(fb, w, h, 8, y, (int)w - 16, FONT_H + 4, 4, c);
        /* Line number gutter */
        char line_no[8];
        snprintf(line_no, 8, "%2d", r + 1);
        draw_text(fb, w, h, 12, y + 1, line_no, 0xFF555588);
        draw_text(fb, w, h, 36, y + 1, g_editor.lines[r], UI26_TEXT);
        /* cursor on active row */
        if (r == g_editor.row) {
            int cx = 36 + g_editor.col * (FONT_W + 1);
            fill_rect(fb, w, h, cx, y + 1, 2, FONT_H, 0xFF88CCFF);
        }
        y += FONT_H + 5;
    }
}

/* ── Snake ──────────────────────────────────────────────────────────────── */
#define SNAKE_CELL 10
static void draw_snake(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, UI26_BG);
    int ox = ((int)w - SNAKE_COLS * SNAKE_CELL) / 2;
    int oy = TITLEBAR_H + ((int)h - TITLEBAR_H - SNAKE_ROWS * SNAKE_CELL) / 2;
    /* Grid background */
    fill_round_rect(fb, w, h, ox - 4, oy - 4, SNAKE_COLS * SNAKE_CELL + 8,
                    SNAKE_ROWS * SNAKE_CELL + 8, 6, 0xFF111133);
    for (int y = 0; y < SNAKE_ROWS; y++) {
        for (int x = 0; x < SNAKE_COLS; x++) {
            fill_rect(fb, w, h, ox + x * SNAKE_CELL + 1, oy + y * SNAKE_CELL + 1,
                      SNAKE_CELL - 2, SNAKE_CELL - 2, 0xFF181840);
        }
    }
    /* food */
    fill_round_rect(fb, w, h, ox + g_snake.food_x * SNAKE_CELL + 1,
                    oy + g_snake.food_y * SNAKE_CELL + 1,
                    SNAKE_CELL - 2, SNAKE_CELL - 2, 3, 0xFFE74C3C);
    /* snake */
    for (int i = 0; i < g_snake.seg_len; i++) {
        uint32_t sc = (i == 0) ? 0xFF2ECC71 : 0xFF1A8C4A;
        fill_round_rect(fb, w, h, ox + g_snake.seg_x[i] * SNAKE_CELL + 1,
                        oy + g_snake.seg_y[i] * SNAKE_CELL + 1,
                        SNAKE_CELL - 2, SNAKE_CELL - 2, 3, sc);
    }
    /* score */
    char buf[32];
    snprintf(buf, 32, "Score: %d", g_snake.score);
    draw_text(fb, w, h, 8, TITLEBAR_H + 6, buf, UI26_TEXT);
    if (g_snake.gameover) {
        fill_glass_panel(fb, w, h, (int)w / 2 - 70, (int)h / 2 - 30, 140, 60, 8, 0xCC111122, 0xFF4488FF);
        draw_text(fb, w, h, (int)w / 2 - 40, (int)h / 2 - 12, "GAME OVER", 0xFFE74C3C);
        draw_text(fb, w, h, (int)w / 2 - 56, (int)h / 2 + 8, "Press R to restart", UI26_TEXT);
    }
}

/* ── Pong ───────────────────────────────────────────────────────────────── */
static void draw_pong(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, UI26_BG);
    int cw = (int)w;
    int ch = (int)h - TITLEBAR_H;
    int mid_x = cw / 2;
    int mid_y = TITLEBAR_H + ch / 2;
    /* dashed center line */
    for (int dy = TITLEBAR_H + 10; dy < (int)h - 10; dy += 20) {
        fill_rect(fb, w, h, mid_x - 1, dy, 2, 10, 0xFF333355);
    }
    /* ball */
    int bx = (int)(g_pong.ball_x * cw);
    int by = (int)(g_pong.ball_y * ch) + TITLEBAR_H;
    fill_round_rect(fb, w, h, bx - 4, by - 4, 8, 8, 4, 0xFFFFFFFF);
    /* player paddle */
    int ppy = (int)(g_pong.paddle_y * ch) + TITLEBAR_H;
    fill_round_rect(fb, w, h, 10, ppy - 28, 8, 56, 4, 0xFF3498DB);
    /* AI paddle */
    int apy = (int)(g_pong.ball_y * ch) + TITLEBAR_H - 28;
    if (apy < TITLEBAR_H) apy = TITLEBAR_H;
    int amax = (int)h - 56;
    if (apy > amax) apy = amax;
    fill_round_rect(fb, w, h, cw - 18, apy, 8, 56, 4, 0xFFE74C3C);
    /* score */
    char buf[16];
    snprintf(buf, 16, "%d", g_pong.player_score);
    draw_text(fb, w, h, mid_x - 60, TITLEBAR_H + 15, buf, 0xFF3498DB);
    snprintf(buf, 16, "%d", g_pong.ai_score);
    draw_text(fb, w, h, mid_x + 44, TITLEBAR_H + 15, buf, 0xFFE74C3C);
    if (g_pong.gameover) {
        fill_glass_panel(fb, w, h, mid_x - 70, mid_y - 30, 140, 60, 8, 0xCC111122, 0xFF4488FF);
        draw_text(fb, w, h, mid_x - 40, mid_y - 12, "GAME OVER", 0xFFE74C3C);
        draw_text(fb, w, h, mid_x - 56, mid_y + 8, "Press R to restart", UI26_TEXT);
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
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, UI26_BG);
    int ox = ((int)w - TETRIS_COLS * TETRIS_CELL) / 2;
    int oy = TITLEBAR_H + ((int)h - TITLEBAR_H - TETRIS_ROWS * TETRIS_CELL) / 2;
    int rr = TETRIS_CELL / 4;
    if (rr < 2) rr = 2;
    if (rr > 4) rr = 4;
    /* Game board background */
    fill_round_rect(fb, w, h, ox - 4, oy - 4, TETRIS_COLS * TETRIS_CELL + 8,
                    TETRIS_ROWS * TETRIS_CELL + 8, 6, 0xFF111133);
    for (int r = 0; r < TETRIS_ROWS; r++) {
        for (int c = 0; c < TETRIS_COLS; c++) {
            int idx = (int)g_tetris.grid[r][c] - 1;
            uint32_t col = (idx >= 0 && idx < 7) ? (uint32_t)tetris_colors[idx] : 0xFF181840;
            fill_round_rect(fb, w, h, ox + c * TETRIS_CELL + 1, oy + r * TETRIS_CELL + 1,
                            TETRIS_CELL - 2, TETRIS_CELL - 2, rr, col);
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
                    fill_round_rect(fb, w, h, ox + gx * TETRIS_CELL + 1, oy + gy * TETRIS_CELL + 1,
                                    TETRIS_CELL - 2, TETRIS_CELL - 2, rr, pc);
            }
        }
    }
    /* info panel */
    fill_round_rect(fb, w, h, 8, TITLEBAR_H + 4, 100, 52, 6, 0xFF1A1A30);
    char buf[32];
    snprintf(buf, 32, "Score: %d", g_tetris.score);
    draw_text(fb, w, h, 16, TITLEBAR_H + 10, buf, UI26_TEXT);
    snprintf(buf, 32, "Lines: %d", g_tetris.lines);
    draw_text(fb, w, h, 16, TITLEBAR_H + 28, buf, 0xFF8888CC);
    /* next piece preview with card */
    fill_round_rect(fb, w, h, (int)w - 78, TITLEBAR_H + 4, 64, 64, 6, 0xFF1A1A30);
    draw_text(fb, w, h, (int)w - 70, TITLEBAR_H + 8, "Next", 0xFF555577);
    int nshape[4][4];
    tetris_get_piece(g_tetris.next_piece, 0, nshape);
    uint32_t npc = tetris_colors[g_tetris.next_piece];
    int nx = (int)w - 66, ny = TITLEBAR_H + 24;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (nshape[r][c])
                fill_round_rect(fb, w, h, nx + c * 12, ny + r * 12, 10, 10, 2, npc);

    if (g_tetris.gameover) {
        fill_glass_panel(fb, w, h, (int)w / 2 - 70, (int)h / 2 - 30, 140, 60, 8, 0xCC111122, 0xFF4488FF);
        draw_text(fb, w, h, (int)w / 2 - 40, (int)h / 2 - 12, "GAME OVER", 0xFFE74C3C);
        draw_text(fb, w, h, (int)w / 2 - 56, (int)h / 2 + 8, "Press R to restart", UI26_TEXT);
    }
}

/* ── App dispatch table ──────────────────────────────────────────────────── */
typedef void (*app_draw_fn)(uint32_t *, uint32_t, uint32_t);
static app_draw_fn g_app_draw[NUM_APPS] = {
    draw_terminal, draw_calculator, draw_settings,
    draw_clock_app, draw_editor, draw_snake, draw_pong, draw_tetris, draw_demoapp,
};



/* ── Shutdown confirmation dialog ──────────────────────────────────── */
static void draw_shutdown_overlay(uint32_t *fb, uint32_t w, uint32_t h) {
    int cx = (int)w / 2, cy = (int)h / 2;
    int dw = 340, dh = 140;
    int dx = cx - dw / 2, dy = cy - dh / 2;
    uint32_t accent = get_accent_color();
    fill_glass_panel(fb, w, h, dx, dy, dw, dh, 12, 0xCC151530, accent);

    draw_text(fb, w, h, cx - 44, dy + 30, "Shut down?", UI26_TEXT);
    if (g_confirm_shutdown == 2) {
        char dots[8];
        int n = g_shutdown_dot % 4;
        for (int i = 0; i < n; i++) dots[i] = '.';
        dots[n] = '\0';
        draw_text(fb, w, h, cx - 20, dy + 60, dots, UI26_TEXT_DIM);
    }

    int btn_x = dx + 50, btn_y = dy + dh - 52;
    int btn_w = 90, btn_h = 34;
    uint32_t yes_c = g_shutdown_yes_hover ? blend(0xFF4488FF, accent, 60) : blend(accent, 0x00000000, 80);
    fill_round_rect(fb, w, h, btn_x, btn_y, btn_w, btn_h, 8, yes_c);
    draw_text(fb, w, h, btn_x + (btn_w - 3 * (FONT_W + 1)) / 2, btn_y + (btn_h - FONT_H) / 2, "Yes", UI26_TEXT);

    int no_x = dx + dw - 50 - btn_w;
    uint32_t no_c = g_shutdown_no_hover ? 0xFF444466 : 0xFF2A2A44;
    fill_round_rect(fb, w, h, no_x, btn_y, btn_w, btn_h, 8, no_c);
    draw_text(fb, w, h, no_x + (btn_w - 2 * (FONT_W + 1)) / 2, btn_y + (btn_h - FONT_H) / 2, "No", UI26_TEXT);
}

/* ── Boot splash with Zirvium vector icon ──────────────────────────────────── */
static void draw_boot_splash(uint32_t *fb, uint32_t w, uint32_t h) {
    for (uint32_t y = 0; y < h; y++) {
        uint8_t val = (uint8_t)(0x0A + ((0x14 - 0x0A) * y) / (h ? h : 1));
        uint32_t c = 0xFF000000u | ((uint32_t)val << 16) | ((uint32_t)val << 8) | (uint32_t)(val + 16);
        for (uint32_t x = 0; x < w; x++)
            fb[y * w + x] = c;
    }

    int icon_size = (w < h ? w : h) / 4;
    if (icon_size > 300) icon_size = 300;
    if (icon_size < 80) icon_size = 80;
    int cx = (int)w / 2;
    int cy = (int)h / 3 - icon_size / 6;

    for (int r = icon_size / 2 + 10; r > icon_size / 2; r--) {
        int alpha = 30 - (icon_size / 2 + 10 - r) * 3;
        if (alpha < 0) alpha = 0;
        fill_round_rect(fb, w, h, cx - r, cy - r, r * 2, r * 2, r,
                        blend(0xFF4488FF, 0x00000000, (uint8_t)alpha));
    }
    draw_zirvium_logo(fb, w, h, cx, cy, icon_size, 0xFF4488FF);

    int saved_scale = g_font_scale;
    g_font_scale = 3;
    const char *title = "Zirvium";
    int tlen = (int)strlen(title);
    int tstep = (FONT_W + 1) * g_font_scale;
    int tx = (int)w / 2 - (tlen * tstep) / 2;
    int ty = cy + icon_size / 2 + 20;
    for (const char *p = title; *p; p++) {
        draw_char_scaled(fb, w, h, tx, ty, *p, 0xFF4488FFu);
        tx += tstep;
    }

    g_font_scale = 1;
    const char *sub = "MOSIX Operating System";
    int slen = (int)strlen(sub);
    int sstep = (FONT_W + 1) * g_font_scale;
    int sx = (int)w / 2 - (slen * sstep) / 2;
    int sy = ty + (FONT_H + 1) * 3 + 8;
    for (const char *p = sub; *p; p++) {
        draw_char(fb, w, h, sx, sy, *p, 0xFF8888CCu);
        sx += sstep;
    }
    g_font_scale = saved_scale;

    draw_text(fb, w, h, (int)w / 2 - 80, sy + 24, "Loading...", 0xFF666688u);
}

/* ── Render one frame ──────────────────────────────────────────────────── */
static void render_frame(void) {
    uint32_t w = g_info.width;
    uint32_t h = g_info.height;
    uint32_t *fb32 = (uint32_t *)compositor_fb;

    if (g_active_app >= 0) {
        fill_rect(fb32, w, h, 0, 0, (int)w, (int)h, UI26_BG);
        draw_top_panel(fb32, w, h, 1);
        g_app_draw[g_active_app](fb32, w, h);
    } else {
        draw_start_screen(fb32, w, h);
    }

    if (g_confirm_shutdown)
        draw_shutdown_overlay(fb32, w, h);
    if (g_ctx_active)
        draw_context_menu(fb32, w, h);
    draw_dropdown(fb32, w, h);
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
    int prev_mh = g_menu_hover;
    int prev_bh = g_back_hover;
    int prev_cbh = g_calc_btn_hover;
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
        int right_down = (ev.buttons & 2) && !(prev_buttons & 2);
        prev_buttons = ev.buttons;

        g_back_hover = 0;
        g_power_hover = 0;
        g_menu_hover = -1;

        /* ── Shutdown confirmation dialog ───────────────────────────── */
        if (g_confirm_shutdown == 2) continue;
        if (g_confirm_shutdown == 1) {
            g_shutdown_yes_hover = 0;
            g_shutdown_no_hover = 0;
            int cx = (int)ww / 2, cy = (int)wh / 2;
            int dw = 340, dh = 140;
            int dx = cx - dw / 2, dy = cy - dh / 2;
            int btn_x = dx + 50, btn_y = dy + dh - 52;
            int btn_w = 90, btn_h = 34;
            if (point_in(nx, ny, btn_x, btn_y, btn_w, btn_h)) g_shutdown_yes_hover = 1;
            int no_x = dx + dw - 50 - btn_w;
            if (point_in(nx, ny, no_x, btn_y, btn_w, btn_h)) g_shutdown_no_hover = 1;
            if (left_down && g_shutdown_yes_hover) { g_confirm_shutdown = 2; g_shutdown_start_s = uptime(); g_dirty = 1; }
            if (left_down && g_shutdown_no_hover) { g_confirm_shutdown = 0; g_dirty = 1; }
            continue;
        }

        /* ── App menu on start screen ─────────────────────────────────-- */
        if (g_active_app < 0 && g_menu_active) {
            int mh = NUM_APPS * MENU_ITEM_H + 8;
            int mx = 8, my = PANEL_H;
            int mw = MENU_W;
            if (point_in(nx, ny, mx, my, mw, mh)) {
                int item = (ny - my - 4) / MENU_ITEM_H;
                g_menu_hover = (item >= 0 && item < NUM_APPS) ? item : -1;
                if (left_down && g_menu_hover >= 0) {
                    g_active_app = g_menu_hover;
                    g_menu_active = 0;
                    g_dirty = 1;
                }
            } else {
                if (left_down || right_down) {
                    g_menu_active = 0;
                    g_dirty = 1;
                }
            }
        }

        /* ── When an app is active ───────────────────────────────────── */
        if (g_active_app >= 0) {
            g_back_hover = point_in(nx, ny, 4, 4, 36, PANEL_H - 8);
            if (g_active_app == APP_CALCULATOR) calc_hit_test(nx, ny);
            if (left_down) {
                if (g_back_hover) {
                    g_active_app = -1;
                    g_dirty = 1;
                } else if (g_active_app == APP_CALCULATOR && g_calc_btn_hover >= 0) {
                    calc_press(calc_btn_labels[g_calc_btn_hover][0]);
                    g_dirty = 1;
                } else if (g_active_app == APP_SETTINGS) {
                    int rw = (int)ww;
                    int bar_x = 160, bar_w = rw - 200;
                    if (ny >= SETTINGS_TABS_Y && ny < SETTINGS_TABS_Y + SETTINGS_TAB_H) {
                        int stw = rw / 4;
                        for (int i = 0; i < 4; i++)
                            if (nx >= i * stw && nx < (i + 1) * stw) { g_settings_tab = i; g_dirty = 1; }
                    }
                    if (g_dropdown_active) {
                        int ddx = nx - g_dropdown_x, ddy = ny - g_dropdown_y;
                        if (ddx >= 0 && ddx < g_dropdown_w && ddy >= 4 && ddy < g_dropdown_h - 4) {
                            int sel = (ddy - 4) / g_dropdown_item_h;
                            if (sel >= 0 && sel < g_dropdown_count) {
                                switch (g_dropdown_callback) {
                                case 1: g_wallpaper_style = sel; break;
                                case 2: g_accent_color = sel; break;
                                case 3: break;
                                case 4: g_font_style = sel; break;
                                case 5: g_font_scale = (sel == 0) ? 1 : 2; break;
                                }
                                g_dirty = 1;
                            }
                        }
                        g_dropdown_active = 0;
                    } else {
                        switch (g_settings_tab) {
                        case 0:
                            if (point_in(nx, ny, 24, setting_row_y(0), rw - 48, SETTING_ROW_H)) {
                                static const char *items[] = {"Wave","Sunset","Midnight","Forest","Aurora","Neon"};
                                dropdown_open(nx, ny, NUM_WALLPAPERS, items, 1); g_dirty = 1;
                            }
                            if (point_in(nx, ny, 24, setting_row_y(1), rw - 48, SETTING_ROW_H)) {
                                dropdown_open(nx, ny, NUM_ACCENTS, accent_names, 2); g_dirty = 1;
                            }
                            if (point_in(nx, ny, 24, setting_row_y(3), rw - 48, SETTING_ROW_H)) {
                                static const char *items[] = {"Regular","Bold"};
                                dropdown_open(nx, ny, 2, items, 4); g_dirty = 1;
                            }
                            if (point_in(nx, ny, 24, setting_row_y(4), rw - 48, SETTING_ROW_H)) {
                                static const char *items[] = {"Small","Large"};
                                dropdown_open(nx, ny, 2, items, 5); g_dirty = 1;
                            }
                            if (point_in(nx, ny, 24, setting_row_y(5), rw - 48, SETTING_ROW_H)) {
                                g_dark_theme = !g_dark_theme; g_dirty = 1;
                            }
                            if (point_in(nx, ny, bar_x, setting_row_y(6), bar_w, SETTING_ROW_H)) {
                                int pct = (nx - bar_x) * 100 / bar_w;
                                if (pct < 0) pct = 0;
                                if (pct > 100) pct = 100;
                                g_brightness = pct; g_dirty = 1;
                            }
                            if (point_in(nx, ny, 24, setting_row_y(7), rw - 48, SETTING_ROW_H)) {
                                g_frame_rate = (g_frame_rate == 60) ? 30 : 60; g_dirty = 1;
                            }
                            break;
                        case 1:
                            if (point_in(nx, ny, 24, setting_row_y(0), rw - 48, SETTING_ROW_H)) { g_wifi_on = !g_wifi_on; g_dirty = 1; }
                            if (point_in(nx, ny, 24, setting_row_y(1), rw - 48, SETTING_ROW_H)) { g_bt_on = !g_bt_on; g_dirty = 1; }
                            break;
                        case 2:
                            if (point_in(nx, ny, bar_x, setting_row_y(0), bar_w, SETTING_ROW_H)) {
                                int pct = (nx - bar_x) * 100 / bar_w;
                                if (pct < 0) pct = 0;
                                if (pct > 100) pct = 100;
                                g_volume = pct; g_dirty = 1;
                            }
                            break;
                        }
                    }
                }
            }
            if (g_dropdown_active) {
                int ddx = nx - g_dropdown_x, ddy = ny - g_dropdown_y;
                if (ddx >= 0 && ddx < g_dropdown_w && ddy >= 4 && ddy < g_dropdown_h - 4) {
                    int item = (ddy - 4) / g_dropdown_item_h;
                    g_dropdown_hover = (item >= 0 && item < g_dropdown_count) ? item : -1;
                } else {
                    g_dropdown_hover = -1;
                    if (right_down) { g_dropdown_active = 0; g_dirty = 1; }
                }
            }
            continue;
        }

        /* ── Start screen: context menu ──────────────────────────────── */
        if (g_ctx_active) {
            int mw = 160, mh = g_ctx_count * 24 + 8;
            int mx = g_ctx_x, my = g_ctx_y;
            if (mx + mw > (int)ww) mx = (int)ww - mw - 4;
            if (my + mh > (int)wh) my = (int)wh - mh - 4;
            if (point_in(nx, ny, mx, my, mw, mh)) {
                int item = (ny - my - 4) / 24;
                g_ctx_hover = (item >= 0 && item < g_ctx_count) ? item : -1;
                if (left_down && g_ctx_hover >= 0) { ctx_execute(g_ctx_hover); g_dirty = 1; }
            } else {
                g_ctx_hover = -1;
                if (left_down || right_down) { g_ctx_active = 0; g_dirty = 1; }
            }
            continue;
        }
        if (right_down) {
            start_context_menu(nx, ny, -1);
        }

        /* ── "Zirvium" label → toggle app menu ──────────────────────── */
        if (left_down && point_in(nx, ny, 8, 0, 80, PANEL_H)) {
            g_menu_active = !g_menu_active;
            g_dirty = 1;
        }

        /* ── Power button (top panel, right side) ──────────────────────── */
        int pwr_x = (int)ww - 20 - 5 * (FONT_W + 1) - 22;
        if (point_in(nx, ny, pwr_x, (PANEL_H - 14) / 2, 16, 14))
            g_power_hover = 1;
        if (left_down && g_power_hover) {
            g_confirm_shutdown = 1;
            g_dirty = 1;
        }
    }

    if (g_menu_hover != prev_mh || g_back_hover != prev_bh ||
        g_calc_btn_hover != prev_cbh || g_power_hover != prev_ph ||
        g_shutdown_yes_hover != prev_syh ||
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
            if (g_active_app == APP_TERMINAL) {
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

    /* No tile grid initialization needed */

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
