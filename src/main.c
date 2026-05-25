#include <zirvflux.h>
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
#define METRO_TILE_FILES 0xFFFFD700
#define METRO_TILE_SETT  0xFF6C7A89
#define METRO_TILE_ABOUT 0xFF9B59B6
#define METRO_TILE_CLOCK 0xFF2C3E50
#define METRO_TILE_WTHR  0xFF3498DB
#define METRO_TILE_MUSIC 0xFFE74C3C
#define METRO_TILE_MAIL  0xFFE67E22
#define METRO_TILE_PHOTO 0xFF1ABC9C
#define METRO_TILE_EDITR 0xFF8E44AD
#define METRO_TILE_SNAKE 0xFF27AE60
#define METRO_TILE_PONG  0xFF2980B9
#define METRO_TILE_TETRS 0xFFC0392B
#define METRO_BACK_BTN   0xFF444466
#define METRO_TILE_HOVER 0x22FFFFFF
#define METRO_DIALOG_BG  0xFF1A1A3E

/* ── Layout constants ─────────────────────────────────────────────────── */
#define TILE_SIZE    120
#define TILE_GAP     16
#define TILE_MARGIN  48
#define TILE_COLS    6
#define TASKBAR_H    40
#define TITLEBAR_H   36
#define ANIM_FRAMES  10

/* ── App icons ─────────────────────────────────────────────────────────── */
enum {
    ICON_TERMINAL,
    ICON_CALC,
    ICON_FILES,
    ICON_SETTINGS,
    ICON_ABOUT,
    ICON_CLOCK,
    ICON_WEATHER,
    ICON_MUSIC,
    ICON_MAIL,
    ICON_PHOTOS,
    ICON_STORE,
    ICON_MAPS,
    ICON_EDITOR,
    ICON_SNAKE,
    ICON_PONG,
    ICON_TETRIS,
    NUM_ICONS,
};

/* ── App definitions ──────────────────────────────────────────────────── */
enum {
    APP_TERMINAL,
    APP_CALCULATOR,
    APP_FILES,
    APP_SETTINGS,
    APP_ABOUT,
    APP_CLOCK,
    APP_WEATHER,
    APP_MUSIC,
    APP_MAIL,
    APP_PHOTOS,
    APP_STORE,
    APP_MAPS,
    APP_EDITOR,
    APP_SNAKE,
    APP_PONG,
    APP_TETRIS,
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
    {"Files",      METRO_TILE_FILES, ICON_FILES},
    {"Settings",   METRO_TILE_SETT,  ICON_SETTINGS},
    {"About",      METRO_TILE_ABOUT, ICON_ABOUT},
    {"Clock",      METRO_TILE_CLOCK, ICON_CLOCK},
    {"Weather",    METRO_TILE_WTHR,  ICON_WEATHER},
    {"Music",      METRO_TILE_MUSIC, ICON_MUSIC},
    {"Mail",       METRO_TILE_MAIL,  ICON_MAIL},
    {"Photos",     METRO_TILE_PHOTO, ICON_PHOTOS},
    {"Store",      METRO_TILE_SETT,  ICON_STORE},
    {"Maps",       METRO_TILE_TERM,  ICON_MAPS},
    {"Editor",     METRO_TILE_EDITR, ICON_EDITOR},
    {"Snake",      METRO_TILE_SNAKE, ICON_SNAKE},
    {"Pong",       METRO_TILE_PONG,  ICON_PONG},
    {"Tetris",     METRO_TILE_TETRS, ICON_TETRIS},
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
static int g_exit_hover = 0;
static int g_power_hover = 0;

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
    double accum;
    double display;
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

/* ── Drawing helpers ──────────────────────────────────────────────────── */
static uint32_t blend(uint32_t fg, uint32_t bg, uint8_t alpha) {
    uint8_t fr = (fg >> 16) & 0xFF, fg_ = (fg >> 8) & 0xFF, fb = fg & 0xFF;
    uint8_t br = (bg >> 16) & 0xFF, bg_ = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    uint8_t r = (uint8_t)(((uint32_t)fr * alpha + (uint32_t)br * (255 - alpha)) / 255);
    uint8_t g = (uint8_t)(((uint32_t)fg_ * alpha + (uint32_t)bg_ * (255 - alpha)) / 255);
    uint8_t b = (uint8_t)(((uint32_t)fb * alpha + (uint32_t)bb * (255 - alpha)) / 255);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void fill_rect(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x, int y, int w, int h, uint32_t color) {
    for (int row = 0; row < h; row++) {
        int py = y + row;
        if (py < 0 || py >= (int)fb_h) continue;
        uint32_t base = (uint32_t)py * fb_w;
        for (int col = 0; col < w; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)fb_w) continue;
            fb[base + (uint32_t)px] = color;
        }
    }
}

static void put_px(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                   int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= (int)fb_w || y >= (int)fb_h) return;
    fb[(uint32_t)y * fb_w + (uint32_t)x] = color;
}

/* ── 8x13 bitmap font ─────────────────────────────────────────────────── */
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

static const uint8_t *font_get(char c) {
    if (c >= 32 && c <= 126) return font_8x13[c - 32];
    return font_8x13[0];
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
    const uint8_t *bm = font_get(c);
    int s = g_font_scale;
    for (int row = 0; row < FONT_H && (y + row * s) < (int)fb_h; row++) {
        uint8_t bits = bm[row];
        if (bits == 0) continue;
        for (int col = 0; col < FONT_W && (x + col * s) < (int)fb_w; col++) {
            if (bits & (1 << (7 - col))) {
                for (int dy = 0; dy < s; dy++) {
                    for (int dx = 0; dx < s; dx++) {
                        int px = x + col * s + dx, py = y + row * s + dy;
                        if (px >= 0 && py >= 0 && px < (int)fb_w && py < (int)fb_h)
                            fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
                    }
                }
            }
        }
    }
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
static void get_tile_rect(int idx, int *x, int *y, int *w, int *h) {
    int col = idx % TILE_COLS;
    int row = idx / TILE_COLS;
    *x = TILE_MARGIN + col * (TILE_SIZE + TILE_GAP);
    *y = TILE_MARGIN + row * (TILE_SIZE + TILE_GAP);
    *w = TILE_SIZE;
    *h = TILE_SIZE;
}

static int point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

/* ── Forward declarations ─────────────────────────────────────────────── */
static void draw_line(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x0, int y0, int x1, int y1, uint32_t color);

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
        case ICON_FILES: {
            int s = size / 3;
            fill_rect(fb, fb_w, fb_h, cx - s, cy - s + 2, s * 2, s * 2 - 2, color);
            fill_rect(fb, fb_w, fb_h, cx - s, cy - s - 2, s, s / 2, color);
            break;
        }
        case ICON_SETTINGS: {
            int r = size / 4;
            int sr = size / 10;
            if (sr < 1) sr = 1;
            for (int i = 0; i < 4; i++) {
                int angle = i * 2;
                int lx = cx + (r * (angle == 0 ? 1 : (angle == 2 ? -1 : 0))) / 2;
                int ly = cy + (r * (angle == 1 ? 1 : (angle == 3 ? -1 : 0))) / 2;
                (void)lx; (void)ly;
            }
            fill_rect(fb, fb_w, fb_h, cx - r / 2, cy - sr, r, sr * 2, color);
            fill_rect(fb, fb_w, fb_h, cx - sr, cy - r / 2, sr * 2, r, color);
            int cir = size / 6;
            int cir2 = size / 10;
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
        case ICON_ABOUT: {
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
            fill_rect(fb, fb_w, fb_h, cx - 1, cy - r / 2, 3, 2, METRO_BG);
            fill_rect(fb, fb_w, fb_h, cx - 1, cy + 1, 3, r / 2, METRO_BG);
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
        case ICON_WEATHER: {
            int r = size / 4;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (dx * dx + dy * dy <= r * r) {
                        int px = cx + dx, py = cy + dy;
                        if (px >= 0 && py >= 0 && px < (int)fb_w && py < (int)fb_h)
                            fb[(uint32_t)py * fb_w + (uint32_t)px] = color;
                    }
                }
            }
            int ray_len = r + 5;
            draw_line(fb, fb_w, fb_h, cx, cy - r, cx, cy - ray_len, color);
            draw_line(fb, fb_w, fb_h, cx, cy + r, cx, cy + ray_len, color);
            draw_line(fb, fb_w, fb_h, cx - r, cy, cx - ray_len, cy, color);
            draw_line(fb, fb_w, fb_h, cx + r, cy, cx + ray_len, cy, color);
            draw_line(fb, fb_w, fb_h, cx - r * 3 / 4, cy - r * 3 / 4,
                      cx - ray_len * 3 / 4, cy - ray_len * 3 / 4, color);
            draw_line(fb, fb_w, fb_h, cx + r * 3 / 4, cy + r * 3 / 4,
                      cx + ray_len * 3 / 4, cy + ray_len * 3 / 4, color);
            break;
        }
        case ICON_MUSIC: {
            draw_char(fb, fb_w, fb_h, cx - 4, cy - 5, '~', color);
            draw_char(fb, fb_w, fb_h, cx + 2, cy - 5, '~', color);
            draw_char(fb, fb_w, fb_h, cx - 4, cy + 1, '~', color);
            draw_char(fb, fb_w, fb_h, cx + 2, cy + 1, '~', color);
            break;
        }
        case ICON_MAIL: {
            int s = size / 3;
            fill_rect(fb, fb_w, fb_h, cx - s, cy - s / 2, s * 2, s, color);
            draw_line(fb, fb_w, fb_h, cx - s, cy - s / 2, cx, cy + s / 4, color);
            draw_line(fb, fb_w, fb_h, cx + s, cy - s / 2, cx, cy + s / 4, color);
            break;
        }
        case ICON_PHOTOS: {
            int s = size / 4;
            for (int dy = -s; dy <= s; dy++) {
                for (int dx = -s; dx <= s; dx++) {
                    int px = cx + dx, py = cy + dy;
                    if (px >= 0 && py >= 0 && px < (int)fb_w && py < (int)fb_h) {
                        int v = (dx * 3 + dy * 5) % 256;
                        fb[(uint32_t)py * fb_w + (uint32_t)px] = 0xFF000000u | ((uint32_t)(128 + v / 2) << 16) | ((uint32_t)(100 + v / 3) << 8) | (uint32_t)(200 - v / 3);
                    }
                }
            }
            break;
        }
        case ICON_STORE: {
            draw_char(fb, fb_w, fb_h, cx - 7, cy - 4, 'S', color);
            draw_char(fb, fb_w, fb_h, cx + 1, cy - 4, 't', color);
            draw_char(fb, fb_w, fb_h, cx - 3, cy + 4, 'o', color);
            draw_char(fb, fb_w, fb_h, cx + 5, cy + 4, 'r', color);
            break;
        }
        case ICON_MAPS: {
            draw_line(fb, fb_w, fb_h, cx - size / 3, cy + size / 4, cx, cy - size / 4, color);
            draw_line(fb, fb_w, fb_h, cx + size / 3, cy + size / 4, cx, cy - size / 4, color);
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
        default:
            break;
    }
}

static void draw_line(uint32_t *fb, uint32_t fb_w, uint32_t fb_h,
                      int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sy = (y0 < y1) ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;
    for (;;) {
        put_px(fb, fb_w, fb_h, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

/* ── Background rendering ─────────────────────────────────────────────── */
static void render_bg(uint32_t *fb, uint32_t w, uint32_t h) {
    if (bg_valid) {
        memcpy(fb, bg_cache, w * h * 4);
        return;
    }
    for (uint32_t y = 0; y < h; y++) {
        uint32_t top_h = h * 2 / 3;
        uint32_t c;
        if (y < top_h) {
            uint8_t r = (uint8_t)(0x1A + ((0x16 - 0x1A) * y) / top_h);
            uint8_t g = (uint8_t)(0x1A + ((0x21 - 0x1A) * y) / top_h);
            uint8_t b = (uint8_t)(0x2E + ((0x3E - 0x2E) * y) / top_h);
            c = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        } else {
            uint32_t dy = y - top_h;
            uint32_t bh = h - top_h;
            uint8_t r = (uint8_t)(0x16 + ((0x12 - 0x16) * dy) / bh);
            uint8_t g = (uint8_t)(0x21 + ((0x1A - 0x21) * dy) / bh);
            uint8_t b = (uint8_t)(0x3E + ((0x2A - 0x3E) * dy) / bh);
            c = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        for (uint32_t x = 0; x < w; x++)
            fb[y * w + x] = c;
    }
    if (!bg_valid) {
        memcpy(bg_cache, fb, w * h * 4);
        bg_valid = 1;
    }
}

/* ── Start screen tiles ────────────────────────────────────────────────── */
static void draw_tile(uint32_t *fb, uint32_t w, uint32_t h,
                      int idx, int hovered) {
    int tx, ty, tw, th;
    get_tile_rect(idx, &tx, &ty, &tw, &th);
    uint32_t color = g_apps[idx].color;
    if (hovered) color = blend(0x44FFFFFF, color, 40);
    fill_rect(fb, w, h, tx, ty, tw, th, color);

    int icon_size = tw / 3;
    int icon_cx = tx + tw / 2;
    int icon_cy = ty + th / 2 - 6;
    draw_icon(fb, w, h, icon_cx, icon_cy, icon_size, g_apps[idx].icon, METRO_TEXT);

    int name_y = ty + th - FONT_H - 6;
    const char *name = g_apps[idx].name;
    int name_w = (int)strlen(name) * (FONT_W + 1);
    draw_text(fb, w, h, tx + (tw - name_w * g_font_scale) / 2, name_y, name, METRO_TEXT);
}

static void draw_start_screen(uint32_t *fb, uint32_t w, uint32_t h) {
    render_bg(fb, w, h);

    int num_tiles = NUM_APPS;
    for (int i = 0; i < num_tiles; i++) {
        if (g_anim.active && g_anim.opening && g_anim.app_idx == i) continue;
        if (g_anim.active && !g_anim.opening && g_anim.app_idx == i) {
            int tw = (int)w, th = (int)h;
            int tx = 0, ty = 0;
            int progress = (g_anim.frame * 256) / ANIM_FRAMES;
            int eased = smoothstep(256 - progress);
            int cx = g_anim.tile_x + (tx - g_anim.tile_x) * eased / 256;
            int cy = g_anim.tile_y + (ty - g_anim.tile_y) * eased / 256;
            int cw = g_anim.tile_w + (tw - g_anim.tile_w) * eased / 256;
            int ch = g_anim.tile_h + (th - g_anim.tile_h) * eased / 256;
            uint32_t color = g_apps[i].color;
            fill_rect(fb, w, h, cx, cy, cw, ch, color);
            int icon_size = cw / 3;
            draw_icon(fb, w, h, cx + cw / 2, cy + ch / 2 - 8, icon_size, g_apps[i].icon, METRO_TEXT);
            int name_y = cy + ch - FONT_H - 8;
            const char *name = g_apps[i].name;
            int name_w = (int)strlen(name) * (FONT_W + 1);
            draw_text(fb, w, h, cx + (cw - name_w * g_font_scale) / 2, name_y, name, METRO_TEXT);
        } else {
            draw_tile(fb, w, h, i, g_tile_hover == i);
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
            g_calc.display = (double)(btn - '0');
            g_calc.new_input = 0;
        } else {
            g_calc.display = g_calc.display * 10.0 + (double)(btn - '0');
        }
        if (g_calc.display > 99999999) g_calc.display = 99999999;
    } else if (btn == '.') {
        if (g_calc.new_input) { g_calc.display = 0.0; g_calc.new_input = 0; }
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
        int n = snprintf(buf, 20, "%.0f", g_calc.display);
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

static void draw_files(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    const char *entries[] = {
        "[DIR]  ..",
        "[DIR]  usr",
        "[DIR]  etc",
        "[DIR]  home",
        "[DIR]  mnt",
        "[DIR]  tmp",
        "[DIR]  bin",
        "[DIR]  lib",
        "[FILE] README.md",
        "[FILE] LICENSE",
        "[FILE] init.bin",
        "[FILE] shell.elf",
        NULL,
    };
    int y = TITLEBAR_H + 10;
    for (int i = 0; entries[i]; i++) {
        uint32_t c = (entries[i][0] == '[' && entries[i][1] == 'D') ? 0xFF66CCFF : 0xFFCCCCDD;
        fill_rect(fb, w, h, 8, y, (int)w - 16, FONT_H + 6, 0xFF222244);
        draw_text(fb, w, h, 16, y + 2, entries[i], c);
        y += FONT_H + 10;
    }
}

static void draw_settings(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    draw_text(fb, w, h, 24, TITLEBAR_H + 20, "Display Settings", METRO_TEXT);
    fill_rect(fb, w, h, 24, TITLEBAR_H + 50, (int)w - 48, 40, 0xFF222244);
    draw_text(fb, w, h, 40, TITLEBAR_H + 58, "Font size:", METRO_TEXT);
    draw_text(fb, w, h, (int)w - 120, TITLEBAR_H + 58,
              g_font_scale > 1 ? "Large" : "Small", METRO_ACCENT);
    fill_rect(fb, w, h, 24, TITLEBAR_H + 96, (int)w - 48, 40, 0xFF222244);
    draw_text(fb, w, h, 40, TITLEBAR_H + 104, "Frame rate:", METRO_TEXT);
    draw_text(fb, w, h, (int)w - 120, TITLEBAR_H + 104,
              "60 FPS", METRO_ACCENT);
    draw_text(fb, w, h, 24, TITLEBAR_H + 160, "Click an option to toggle", METRO_TEXT_DIM);
}

static void draw_about(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    draw_text(fb, w, h, 24, TITLEBAR_H + 30, "Zirvium OS", 0xFF9B59B6);
    draw_text(fb, w, h, 24, TITLEBAR_H + 56, "Version: 0.1.0", METRO_TEXT);
    char res[32];
    int rl = 0;
    uint32_t rw = g_info.width, rh = g_info.height;
    if (rw >= 100) { res[rl++] = (char)('0' + rw / 100); rw %= 100; }
    res[rl++] = (char)('0' + rw / 10); rw %= 10;
    res[rl++] = (char)('0' + rw); res[rl++] = 'x';
    if (rh >= 100) { res[rl++] = (char)('0' + rh / 100); rh %= 100; }
    res[rl++] = (char)('0' + rh / 10); rh %= 10;
    res[rl++] = (char)('0' + rh); res[rl] = '\0';
    draw_text(fb, w, h, 24, TITLEBAR_H + 82, "Resolution:", METRO_TEXT_DIM);
    draw_text(fb, w, h, 140, TITLEBAR_H + 82, res, METRO_ACCENT);
    draw_text(fb, w, h, 24, TITLEBAR_H + 108, "DisplayJet MAEM encrypted compositor", METRO_TEXT_DIM);
    draw_text(fb, w, h, 24, TITLEBAR_H + 134, "ZirvTK: Rust GUI toolkit available", METRO_TEXT_DIM);
    draw_text(fb, w, h, 24, TITLEBAR_H + 160, "Windows 8 Metro UI", 0xFF00B7C3);
    draw_text(fb, w, h, 24, TITLEBAR_H + 200, "Zirvium", 0xFF8888AA);
}

static void draw_clock_app(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
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
        draw_text(fb, w, h, 24, TITLEBAR_H + 60, time_str, 0xFF2C3E50);
        g_font_scale = saved_scale;
        char date_str[32];
        snprintf(date_str, 32, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
        draw_text(fb, w, h, 24, TITLEBAR_H + 160, date_str, METRO_TEXT_DIM);
    } else {
        draw_text(fb, w, h, 24, TITLEBAR_H + 60, "No RTC", METRO_TEXT);
    }
}

static void draw_weather(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    draw_text(fb, w, h, 24, TITLEBAR_H + 30, "Seattle, WA", METRO_TEXT);
    int saved = g_font_scale;
    g_font_scale = 3;
    draw_text(fb, w, h, 24, TITLEBAR_H + 70, "72", 0xFF3498DB);
    g_font_scale = 1;
    draw_char(fb, w, h, 24 + 3 * (FONT_W + 1) * 3 + 4, TITLEBAR_H + 70, 'o', METRO_TEXT);
    draw_text(fb, w, h, 24 + 3 * (FONT_W + 1) * 3 + 12, TITLEBAR_H + 78, "F", METRO_TEXT);
    draw_text(fb, w, h, 24, TITLEBAR_H + 140, "Partly Cloudy", METRO_TEXT_DIM);
    draw_text(fb, w, h, 24, TITLEBAR_H + 160, "H: 78  L: 62", METRO_TEXT_DIM);
    draw_text(fb, w, h, 24, TITLEBAR_H + 180, "Humidity: 45%  Wind: 8 mph SW", METRO_TEXT_DIM);
    g_font_scale = saved;
}

static void draw_music(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    draw_text(fb, w, h, 24, TITLEBAR_H + 30, "Now Playing", METRO_TEXT_DIM);
    int saved = g_font_scale;
    g_font_scale = 2;
    draw_text(fb, w, h, 24, TITLEBAR_H + 56, "Bohemian Rhapsody", 0xFFE74C3C);
    g_font_scale = 1;
    draw_text(fb, w, h, 24, TITLEBAR_H + 90, "Queen - A Night at the Opera", METRO_TEXT_DIM);
    uint32_t prog = 0xFF222244;
    fill_rect(fb, w, h, 24, TITLEBAR_H + 120, (int)w - 48, 6, prog);
    fill_rect(fb, w, h, 24, TITLEBAR_H + 120, ((int)w - 48) * 3 / 10, 6, 0xFFE74C3C);
    draw_text(fb, w, h, 24, TITLEBAR_H + 132, "1:23", METRO_TEXT_DIM);
    draw_text(fb, w, h, (int)w - 80, TITLEBAR_H + 132, "5:55", METRO_TEXT_DIM);
    struct { const char *l; int x; } ctrls[] = {
        {"<<", (int)w / 2 - 80},
        {"||", (int)w / 2 - 16},
        {">>", (int)w / 2 + 48},
        {NULL, 0},
    };
    for (int i = 0; ctrls[i].l; i++) {
        fill_rect(fb, w, h, ctrls[i].x, TITLEBAR_H + 160, 48, 32, 0xFF2A2A4E);
        draw_text(fb, w, h, ctrls[i].x + 8, TITLEBAR_H + 166, ctrls[i].l, METRO_TEXT);
    }
    g_font_scale = saved;
}

static void draw_mail(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    struct { const char *from, *subj; } mails[] = {
        {"System", "Welcome to Zirvium OS"},
        {"Admin", "Kernel build completed"},
        {"GitHub", "New release: v0.1.0"},
        {" noreply@zirv ", "Your compositor is running"},
        {NULL, NULL},
    };
    int y = TITLEBAR_H + 16;
    for (int i = 0; mails[i].from; i++) {
        fill_rect(fb, w, h, 16, y, (int)w - 32, 40, 0xFF222244);
        draw_text(fb, w, h, 24, y + 4, mails[i].from, METRO_ACCENT);
        draw_text(fb, w, h, 24, y + 20, mails[i].subj, METRO_TEXT);
        y += 46;
    }
}

static void draw_photos(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    int grid = 100, gap = 8, start = 24;
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 4; col++) {
            int px = start + col * (grid + gap);
            int py = TITLEBAR_H + 16 + row * (grid + gap);
            uint32_t c = 0xFF000000 | ((uint32_t)(64 + row * 32 + col * 16) << 16)
                        | ((uint32_t)(32 + col * 24 + row * 16) << 8)
                        | (uint32_t)(128 + row * 20 + col * 10);
            fill_rect(fb, w, h, px, py, grid - 4, grid - 4, c);
            draw_char(fb, w, h, px + grid / 2 - 4, py + grid / 2 - 6, '~', METRO_TEXT);
        }
    }
}

static void draw_store(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    draw_text(fb, w, h, 24, TITLEBAR_H + 20, "Zirvium Store", METRO_TEXT);
    struct { const char *name, *desc; uint32_t c; } items[] = {
        {"ZirvTerm", "Terminal emulator", METRO_TILE_TERM},
        {"ZirvCalc", "Scientific calculator", METRO_TILE_CALC},
        {"ZirvFiles", "File manager", METRO_TILE_FILES},
        {"ZirvMusic", "Music player", METRO_TILE_MUSIC},
        {"ZirvWeather", "Weather forecast", METRO_TILE_WTHR},
        {"ZirvMail", "Email client", METRO_TILE_MAIL},
        {NULL, NULL, 0},
    };
    int y = TITLEBAR_H + 50;
    for (int i = 0; items[i].name; i++) {
        fill_rect(fb, w, h, 24, y, (int)w - 48, 48, items[i].c);
        draw_text(fb, w, h, 36, y + 6, items[i].name, METRO_TEXT);
        draw_text(fb, w, h, 36, y + 26, items[i].desc, METRO_TEXT_DIM);
        y += 54;
    }
}

static void draw_maps(uint32_t *fb, uint32_t w, uint32_t h) {
    fill_rect(fb, w, h, 0, TITLEBAR_H, (int)w, (int)h - TITLEBAR_H, 0xFF1A1A2E);
    draw_text(fb, w, h, 24, TITLEBAR_H + 20, "Zirvium Maps", METRO_TEXT);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 12; x++) {
            int px = 24 + x * 80;
            int py = TITLEBAR_H + 50 + y * 80;
            uint32_t c = ((x + y) % 2 == 0) ? 0xFF2A3A4E : 0xFF1A2A3E;
            fill_rect(fb, w, h, px, py, 78, 78, c);
        }
    }
    draw_line(fb, w, h, 24, TITLEBAR_H + 50 + 4 * 80 + 40,
              24 + 6 * 80, TITLEBAR_H + 50 + 2 * 80 + 40, 0xFFE74C3C);
    draw_line(fb, w, h, 24 + 6 * 80, TITLEBAR_H + 50 + 2 * 80 + 40,
              24 + 10 * 80, TITLEBAR_H + 50 + 5 * 80 + 40, 0xFFE74C3C);
    fill_rect(fb, w, h, 24 + 7 * 80 - 4, TITLEBAR_H + 50 + 3 * 80 - 4, 8, 8, 0xFFE74C3C);
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
    draw_terminal, draw_calculator, draw_files, draw_settings,
    draw_about, draw_clock_app, draw_weather, draw_music,
    draw_mail, draw_photos, draw_store, draw_maps,
    draw_editor, draw_snake, draw_pong, draw_tetris,
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
    draw_text(fb, w, h, 10, tb_y + (TASKBAR_H - FONT_H) / 2, "Zirvium", METRO_ACCENT);

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
            int progress = (g_anim.frame * 256) / ANIM_FRAMES;
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
            int progress = (g_anim.frame * 256) / ANIM_FRAMES;
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
        prev_buttons = ev.buttons;

        g_tile_hover = -1;
        g_back_hover = 0;
        g_exit_hover = 0;
        g_power_hover = 0;

        if (g_anim.active) {
            if (left_down) {
                g_anim.active = 0;
                g_anim.frame = ANIM_FRAMES;
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
            if (left_down) {
                if (g_back_hover) {
                    if (g_active_app >= 0) {
                        int tx, ty, tw, th;
                        get_tile_rect(g_active_app, &tx, &ty, &tw, &th);
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
                    int sy1 = TITLEBAR_H + 50;
                    if (point_in(nx, ny, 24, sy1, (int)ww - 48, 40)) {
                        g_font_scale = (g_font_scale > 1) ? 1 : 2;
                        g_dirty = 1;
                    }
                    int sy2 = TITLEBAR_H + 96;
                    if (point_in(nx, ny, 24, sy2, (int)ww - 48, 40)) {
                        g_dirty = 1;
                    }
                }
            }
            continue;
        }

        for (int i = 0; i < NUM_APPS; i++) {
            int tx, ty, tw, th;
            get_tile_rect(i, &tx, &ty, &tw, &th);
            if (point_in(nx, ny, tx, ty, tw, th)) {
                g_tile_hover = i;
                break;
            }
        }

        /* Taskbar buttons hover */
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

        if (left_down && g_tile_hover >= 0) {
            int tx, ty, tw, th;
            get_tile_rect(g_tile_hover, &tx, &ty, &tw, &th);
            g_anim.active = 1;
            g_anim.opening = 1;
            g_anim.app_idx = g_tile_hover;
            g_anim.frame = 0;
            g_anim.tile_x = tx; g_anim.tile_y = ty;
            g_anim.tile_w = tw; g_anim.tile_h = th;
            g_anim.scr_w = (int)ww; g_anim.scr_h = (int)wh;
            g_dirty = 1;
        }

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

    size_t fb_size = (size_t)g_buf.stride * g_buf.height;

    /* ── Boot splash ──────────────────────────────────────────────────────── */
    draw_boot_splash((uint32_t *)compositor_fb, g_info.width, g_info.height);
    zf_write_buffer(&g_buf, compositor_fb, fb_size);
    zf_present(&g_buf);
    msleep(2500);

    render_frame();

    if (zf_write_buffer(&g_buf, compositor_fb, fb_size) < 0) return 1;
    if (zf_present(&g_buf) != 0) return 1;

    zf_suppress_dbg();

    for (;;) {
        process_mouse();
        process_keys();

        /* Update active game */
        if (g_active_app == APP_SNAKE)  update_snake();
        if (g_active_app == APP_PONG)   update_pong();
        if (g_active_app == APP_TETRIS) update_tetris();

        if (g_anim.active) {
            g_anim.frame++;
            if (g_anim.frame >= ANIM_FRAMES) {
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
        msleep(16);
    }
}
