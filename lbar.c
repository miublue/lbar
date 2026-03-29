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
static int opt_bottom = BOTTOM_BAR, opt_line = LINE_HEIGHT;
static struct { int x, y, w, h; } geom;

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
    char buf[size];
    memcpy(buf, text, size);
    for (int i = 0; i < size; ++i) {
        // replace spaces with some wide character
        if (buf[i] == ' ') buf[i] = 'W';
    }
    XftTextExtentsUtf8(display, font, (const unsigned char*)buf, size, &extents);
    return extents;
}

static void draw_text(char *text, size_t size, XftColor *color, int pos, int off, int line) {
    XGlyphInfo ex = get_text_extents(text, size);
    int x = pos == RIGHT? (geom.w-ex.width) : pos == CENTER? ((geom.w-ex.width)/2) : 0;
    if (line) XftDrawRect(draw, &col_ul, x+off, geom.h-opt_line, ex.width, opt_line);
    XftDrawStringUtf8(draw, color, font, x+off, text_y, (const unsigned char*)text, size);
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
        if (status[i] == '\r') {
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
    XCopyArea(display, buffer, window, gc, 0, 0, geom.w, geom.h, 0, 0);
}

static void usage(char *name) {
    printf("usage: %s [-h|-b|-f font|-u size|-g geom|-F foreground|-B background|-U underline]\n", name);
    printf("    -h          show help\n");
    printf("    -b          place bar at the bottom of the screen\n");
    printf("    -f font     set bar font\n");
    printf("    -u size     set bar underline height in pixels\n");
    printf("    -g geom     set bar geometry {width}x{height}+{xoffset}+{yoffset}\n");
    printf("    -F #RRGGBB  set bar text color\n");
    printf("    -B #RRGGBB  set bar background color\n");
    printf("    -U #RRGGBB  set bar underline color\n");
}

int main(int argc, char **argv) {
    if (!(display = XOpenDisplay(0))) return 1;
    XSetWindowAttributes attr;
    int screen, text_sz = 0;
    char text[TEXT_MAX];
    XEvent event;

    screen = DefaultScreen(display);
    visual = DefaultVisual(display, screen);
    screen_w = XDisplayWidth(display, screen);
    screen_h = XDisplayHeight(display, screen);
    root = RootWindow(display, screen);

    geom.x = 0, geom.y = 0, geom.w = screen_w, geom.h = BAR_HEIGHT;

    for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "-b")) {
            opt_bottom = 1;
        } else if (!strcmp(argv[i], "-h") || i + 1 == argc) {
            usage(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "-f")) { // options that require an arg
            opt_font = argv[++i];
        } else if (!strcmp(argv[i], "-u")) {
            opt_line = strtol(argv[++i], NULL, 0);
        } else if (!strcmp(argv[i], "-g")) {
            sscanf(argv[++i], "%dx%d+%d+%d", &geom.w, &geom.h, &geom.x, &geom.y);
        } else if (!strcmp(argv[i], "-F")) {
            opt_fg = argv[++i];
        } else if (!strcmp(argv[i], "-B")) {
            opt_bg = argv[++i];
        } else if (!strcmp(argv[i], "-U")) {
            opt_ul = argv[++i];
        } else {
            printf("invalid option '%s'\n", argv[i]);
        }
    }

    if (geom.w <= 0) geom.w = screen_w+geom.w;
    if (geom.h <= 0) geom.h = BAR_HEIGHT;
    col_bg = alloc_color(opt_bg);
    col_fg = alloc_color(opt_fg);
    col_ul = alloc_color(opt_ul);

    window = XCreateSimpleWindow(display, root,
            geom.x, opt_bottom? screen_h-geom.h-geom.y : geom.y,
            geom.w, geom.h, 0, 0, col_bg.pixel);
    buffer = XCreatePixmap(display, root,
            geom.w, geom.h, DefaultDepth(display, screen));

    attr.override_redirect = True;
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attr);
    XSelectInput(display, window, ExposureMask);
    XSelectInput(display, root, PropertyChangeMask);
    XMapWindow(display, window);

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

    { // get y position to draw text in
        XGlyphInfo extents;
        // if characters have different heights, it'll pick whichever is taller (hopefully)
        XftTextExtentsUtf8(display, font, (const unsigned char*)"L1O0Tt", 6, &extents);
        text_y = extents.height + (geom.h-extents.height)/2;
    }

    for (;;) {
        XftDrawRect(draw, &col_bg, 0, 0, geom.w, geom.h);
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
                XCopyArea(display, buffer, window, gc, 0, 0, geom.w, geom.h, 0, 0);
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
