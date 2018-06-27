#ifndef VSCREEN_H
#define VSCREEN_H

/*
 * Data structure maintaining information about a virtual screen.
 */

#include <ncurses.h>

struct vscreen {
    int num_lines;
    int num_cols;
    int cur_line;
    int cur_col;
    char **lines;
    char *line_changed;
};
typedef struct vscreen VSCREEN;

extern WINDOW *main_screen;
extern WINDOW *status_line;

VSCREEN *vscreen_init(void);
void set_status(char *status);
void vscreen_show(VSCREEN *vscreen);
void vscreen_sync(VSCREEN *vscreen);
void vscreen_putc(VSCREEN *vscreen, char c);
void vscreen_fini(VSCREEN *vscreen);

#endif
