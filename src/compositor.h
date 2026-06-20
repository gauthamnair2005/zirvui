#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <stdint.h>

#define MAX_WINDOWS 16
#define MAX_APPS    8
#define TITLE_H     24
#define BORDER      1
#define CLOSE_SIZE  16
#define CLOSE_PAD   4
#define TASKBAR_H   40
#define START_BTN_W 44
#define MENU_W      200
#define MENU_H      260

typedef struct {
    char name[32];
    uint32_t color;
    char icon[4];
    int def_w, def_h;
    void (*draw)(int id, uint32_t *fb, uint32_t fw, uint32_t fh, int x, int y, int w, int h);
    int  (*click)(int id, int mx, int my, int btn);
    void (*tick)(int id);
} AppDef;

typedef struct {
    int x, y, content_w, content_h, visible, app_id, active;
    int drag_ox, drag_oy;
    char title[48];
    union {
        struct { double acc, cur; char op; int fresh; char dbuf[32]; int db_len; } calc;
        struct { int h, m, s; } clk;
        struct { char lines[80][60]; int n, scroll; char ibuf[60]; int ipos; int hist[10]; int hpos; } term;
        struct { char items[64][48]; int n, scroll; } list;
    } u;
} Window;

int compositor_run(void);

#endif
