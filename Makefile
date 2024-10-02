LIBS = `pkg-config --libs --cflags x11 xft`
CFLAGS = -O2 -Wall -Werror

all:
	tcc -o lbar lbar.c ${CFLAGS} ${LIBS}

install: all
	install lbar /usr/local/bin

uninstall:
	rm /usr/local/bin/lbar
