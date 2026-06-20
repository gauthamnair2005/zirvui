#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <stdint.h>

#define MAX_WINDOWS 16
#define TITLE_H 24
#define BORDER 1
#define CLOSE_SIZE 16
#define CLOSE_PAD 4

typedef struct {
    int x, y;
    int content_w, content_h;
    int visible;
    char title[48];
    void (*draw_content)(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, int w, int h);
    int drag_grab_x;
    int drag_grab_y;
} Window;

int compositor_run(void);

#endif
