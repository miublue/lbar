#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdint.h>
#include <string.h>
#include "config.h"

#define TEXT_MAX 2048

static Display *display;
static uint32_t screen_w, screen_h;
static Window root, window;
static Drawable buffer;
static Visual *visual;
static XftDraw *draw;
static XftFont *font;
static XftColor col_bg, col_fg;
static uint32_t text_y = BAR_HEIGHT;
static GC gc;
static char *opt_font = FONT, *opt_fg = FOREGROUND, *opt_bg = BACKGROUND;
static int opt_height = BAR_HEIGHT, opt_bottom = BOTTOM_BAR;

enum { LEFT, RIGHT, CENTER };

XftColor alloc_color(char *col) {
    Colormap map = DefaultColormap(display, DefaultScreen(display));
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
            ++i;
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

    // extra space so text don't go outside the window (lmfao)
    right_buf[right_sz++] = ' ';
    draw_text(left_buf, left_sz, &col_fg, LEFT);
    draw_text(center_buf, center_sz, &col_fg, CENTER);
    draw_text(right_buf, right_sz, &col_fg, RIGHT);
    XCopyArea(display, buffer, window, gc, 0, 0, screen_w, opt_height, 0, 0);
}

void usage(char *name) {
    printf("usage: %s [-h|-b|-f font|-F foreground|-B background|-H height]\n", name);
}

int main(int argc, char **argv) {
    if (!(display = XOpenDisplay(0))) return 1;
    XSetWindowAttributes attr;
    int screen, text_sz = 0;
    char text[TEXT_MAX];
    XEvent event;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h")) {
            usage(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "-b")) {
            opt_bottom = 1;
        } else if (!strcmp(argv[i], "-f")) {
            opt_font = argv[++i];
        } else if (!strcmp(argv[i], "-F")) {
            opt_fg = argv[++i];
        } else if (!strcmp(argv[i], "-B")) {
            opt_bg = argv[++i];
        } else if (!strcmp(argv[i], "-H")) {
            opt_height = strtol(argv[++i], NULL, 0);
        } else {
            printf("invalid option '%s'\n", argv[i]);
        }
    }

    screen = DefaultScreen(display);
    visual = DefaultVisual(display, screen);
    screen_w = XDisplayWidth(display, screen);
    screen_h = XDisplayHeight(display, screen);
    root = RootWindow(display, screen);
    col_bg = alloc_color(opt_bg);
    col_fg = alloc_color(opt_fg);

    window = XCreateSimpleWindow(display, root,
            0, opt_bottom? screen_h-opt_height : 0,
            screen_w, opt_height, 0, 0, col_bg.pixel);
    buffer = XCreatePixmap(display, root,
            screen_w, opt_height, DefaultDepth(display, screen));

    XGCValues gc_value = {
        .background = col_bg.pixel,
        .foreground = col_fg.pixel,
        .line_style = LineSolid,
        .fill_style = FillSolid,
    };
    int gc_mask = GCBackground|GCForeground|GCLineWidth|GCLineStyle;
    gc = XCreateGC(display, window, gc_mask, &gc_value);
    draw = XftDrawCreate(display, buffer, visual, DefaultColormap(display, screen));
    font = XftFontOpenName(display, screen, opt_font);

    attr.override_redirect = True;
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attr);
    XSelectInput(display, window, ExposureMask);
    XSelectInput(display, root, PropertyChangeMask);
    XMapWindow(display, window);

    { // get y position to draw text in
        XGlyphInfo extents;
        // if characters have different heights, it'll pick whichever is taller (hopefully)
        XftTextExtents8(display, font, "L1O0Tt", 6, &extents);
        text_y = extents.height + (opt_height-extents.height)/2;
    }

    for (;;) {
        XftDrawRect(draw, &col_bg, 0, 0, screen_w, opt_height);
        int ch = getc(stdin);
        if (ch == '\n' || !ch) {
            parse_status(text, text_sz);
            text_sz = 0;
        } else {
            text[text_sz++] = ch;
        }

        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == Expose) {
                XCopyArea(display, buffer, window, gc, 0, 0, screen_w, opt_height, 0, 0);
                XSync(display, False);
            } else if (event.type == PropertyNotify && event.xproperty.window == root) {
                parse_status(text, text_sz);
            }
        }
    }

    XftDrawDestroy(draw);
    XFreeGC(display, gc);
    XCloseDisplay(display);
    return 0;
}
