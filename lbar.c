#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdint.h>
#include <string.h>
#include "config.h"

#define TEXT_MAX 2048
#define BLOCKS_MAX 32

static Display *display;
static uint32_t screen_w, screen_h;
static Window root, window;
static Drawable buffer;
static Visual *visual;
static XftDraw *draw;
static XftFont *font;
static XftColor col_bg, col_fg, col_ul;
static uint32_t text_y = BAR_HEIGHT;
static GC gc;
static char *opt_font = FONT, *opt_fg = FOREGROUND, *opt_bg = BACKGROUND, *opt_ul = UNDERLINE;
static int opt_height = BAR_HEIGHT, opt_bottom = BOTTOM_BAR, opt_line = LINE_HEIGHT;

enum { LEFT, RIGHT, CENTER };
struct block {
    int start, size, line;
};

struct blocks {
    struct block blocks[BLOCKS_MAX];
    int off, num;
};

static struct blocks blocks[3];

static XftColor alloc_color(char *col) {
    Colormap map = DefaultColormap(display, DefaultScreen(display));
    XftColor ret;
    XftColorAllocName(display, visual, map, col, &ret);
    return ret;
}

static XGlyphInfo get_text_extents(char *text, size_t size) {
    XGlyphInfo extents;
    XftTextExtents8(display, font, text, size, &extents);
    return extents;
}

static void draw_text(char *text, size_t size, XftColor *color, int pos, int off, int line) {
    XGlyphInfo ex = get_text_extents(text, size);
    int x = pos == RIGHT? (screen_w-ex.width) : pos == CENTER? ((screen_w-ex.width)/2) : 0;
    if (line) XftDrawRect(draw, &col_ul, x+off, opt_height-opt_line, ex.width, opt_line);
    XftDrawString8(draw, color, font, x+off, text_y, text, size);
}

static void draw_right_block(char *text) {
    for (int i = blocks[RIGHT].num-1; i >= 0; --i) {
        struct block blk = blocks[RIGHT].blocks[i];
        draw_text(text+blk.start, blk.size, &col_fg, RIGHT, blocks[RIGHT].off, blk.line);
        XGlyphInfo ex = get_text_extents(text+blk.start, blk.size);
        blocks[RIGHT].off -= ex.width;
    }
}

static void draw_block(int n, char *text) {
    // XXX: center blocks are not being offset properly
    if (n == RIGHT) return draw_right_block(text);
    for (int i = 0; i < blocks[n].num; ++i) {
        struct block blk = blocks[n].blocks[i];
        draw_text(text+blk.start, blk.size, &col_fg, n, blocks[n].off, blk.line);
        XGlyphInfo ex = get_text_extents(text+blk.start, blk.size);
        blocks[n].off += ex.width;
    }
}

static void parse_status(char *status, size_t status_sz) {
    for (int i = 0; i < 3; ++i) blocks[i].off = blocks[i].num = 0;
    struct block blk = {0};
    int pos = LEFT;
    for (int i = 0; i < status_sz; ++i) {
        if (status[i] == '&') {
            blocks[pos].blocks[blocks[pos].num++] = blk;
            if (status[++i] == 'U') blk.line = !blk.line;
            else pos = status[i] == 'R'? RIGHT : status[i] == 'C'? CENTER : LEFT;
            blk.start = i+1;
            blk.size = 0;
        } else {
            ++blk.size;
        }
    }

    if (blk.size) blocks[pos].blocks[blocks[pos].num++] = blk;
    for (int i = 0; i < 3; ++i) draw_block(i, status);
    XCopyArea(display, buffer, window, gc, 0, 0, screen_w, opt_height, 0, 0);
}

static void usage(char *name) {
    printf("usage: %s [-h|-b|-f font|-F foreground|-B background|-U underline|-u size|-H height]\n", name);
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
        } else if (!strcmp(argv[i], "-U")) {
            opt_ul = argv[++i];
        } else if (!strcmp(argv[i], "-H")) {
            opt_height = strtol(argv[++i], NULL, 0);
        } else if (!strcmp(argv[i], "-u")) {
            opt_line = strtol(argv[++i], NULL, 0);
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
    col_ul = alloc_color(opt_ul);

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
