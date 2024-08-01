#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdint.h>
#include <string.h>
#include "config.h"

Display *display;
Window root;
int screen;
uint32_t screen_w, screen_h;
Visual *visual;
Window window;
Drawable buffer;
XftDraw *draw;
XftFont *font;
XftColor col_bg, col_fg;
uint32_t text_y = BARHEIGHT;
GC gc;

enum {
    LEFT,
    RIGHT,
    CENTER,
};

XftColor alloc_color(char *col) {
    Colormap map = DefaultColormap(display, screen);
    XftColor ret;
    XftColorAllocName(display, visual, map, col, &ret);
    return ret;
}

void draw_text(char *text, size_t size, XftColor *color, int pos) {
    XGlyphInfo extents;
    XftTextExtents8(display, font, text, size, &extents);
    int x = 0;
    switch (pos) {
    case RIGHT:
        x = screen_w - extents.width; break;
    case CENTER:
        x = (screen_w - extents.width) / 2; break;
    default:
        x = 0; break;
    }

    XftDrawString8(draw, color, font, x, text_y, text, size);
}

void parse_status(char *status, size_t status_sz) {
    // L code
    int text_pos = LEFT;
    char left_buf[status_sz];
    char right_buf[status_sz];
    char center_buf[status_sz];
    size_t left_sz = 0, right_sz = 0, center_sz = 0;

    for (int i = 0; i < status_sz; ++i) {
        if (status[i] == '&') {
            if (status[i+1] == 'R') text_pos = RIGHT;
            else if (status[i+1] == 'L') text_pos = LEFT;
            else if (status[i+1] == 'C') text_pos = CENTER;
            i += 1;
        }
        else {
            switch (text_pos) {
            case RIGHT:
                right_buf[right_sz++] = status[i]; break;
            case CENTER:
                center_buf[center_sz++] = status[i]; break;
            default:
                left_buf[left_sz++] = status[i]; break;
            }
        }
    }

    // so the text doesn't go outside the screen
    right_buf[right_sz++] = ' ';
    draw_text(left_buf, left_sz, &col_fg, LEFT);
    draw_text(center_buf, center_sz, &col_fg, CENTER);
    draw_text(right_buf, right_sz, &col_fg, RIGHT);
    XCopyArea(display, buffer, window, gc, 0, 0, screen_w, BARHEIGHT, 0, 0);
}

int main() {
    if (!(display = XOpenDisplay(0))) return 1;
    XSetWindowAttributes attr;

    screen = DefaultScreen(display);
    visual = DefaultVisual(display, screen);
    root = RootWindow(display, screen);
    screen_w = XDisplayWidth(display, screen);
    screen_h = XDisplayHeight(display, screen);

    col_bg = alloc_color(BACKGROUND);
    col_fg = alloc_color(FOREGROUND);

    window = XCreateSimpleWindow(display, root, 0, 0, screen_w, BARHEIGHT, 0, 0, col_bg.pixel);
    buffer = XCreatePixmap(display, root, screen_w, BARHEIGHT, DefaultDepth(display, screen));

    XGCValues gc_value;
    gc_value.background = col_bg.pixel;
    gc_value.foreground = col_fg.pixel;
    gc_value.line_style = LineSolid;
    gc_value.fill_style = FillSolid;
    uint64_t gc_mask = GCBackground|GCForeground|GCLineWidth|GCLineStyle;
    gc = XCreateGC(display, window, gc_mask, &gc_value);

    draw = XftDrawCreate(display, buffer, visual, DefaultColormap(display, screen));
    font = XftFontOpenName(display, screen, FONT);

    attr.override_redirect = True;
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attr);
    XSelectInput(display, window, ExposureMask);
    XSelectInput(display, root, PropertyChangeMask);
    XMapWindow(display, window);

    // get y position to draw text in
    {
        XGlyphInfo extents;
        // if characters have different heights, it'll pick whichever is taller (hopefully)
        XftTextExtents8(display, font, "L1O0Tt", 6, &extents);
        text_y = extents.height + (BARHEIGHT-extents.height)/2;
    }

    char text[2048] = {0};
    int text_sz = 0;

    XEvent event;
    for (;;) {
        XftDrawRect(draw, &col_bg, 0, 0, screen_w, BARHEIGHT);

        int ch = getc(stdin);
        if (ch == '\n' || !ch) {
            parse_status(text, text_sz);
            text_sz = 0;
        }
        else {
            text[text_sz++] = ch;
        }

        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == Expose) {
                XCopyArea(display, buffer, window, gc, 0, 0, screen_w, BARHEIGHT, 0, 0);
                XSync(display, False);
            }
            if (event.type == PropertyNotify && event.xproperty.window == root) {
                parse_status(text, text_sz);
            }
        }
    }

    XftDrawDestroy(draw);
    XFreeGC(display, gc);
    XCloseDisplay(display);
    return 0;
}
