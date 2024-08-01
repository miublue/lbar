OUT = lbar
LIBS = `pkg-config --libs --cflags x11 xft`
CFLAGS = -O2 -Wall -Werror

all:
	tcc -o $(OUT) lbar.c $(CFLAGS) $(LIBS)

install: all
	install $(OUT) /usr/local/bin

uninstall:
	rm /usr/local/bin/$(OUT)

