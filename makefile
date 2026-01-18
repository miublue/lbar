CC = tcc
LIBS = `pkg-config --libs --cflags x11 xft`
CFLAGS = -O2 -Wall -Werror
PREFIX = /usr/local

all:
	${CC} -o lbar lbar.c ${CFLAGS} ${LIBS}

install: all
	mkdir -p ${PREFIX}/bin
	install -s lbar ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/lbar

.PHONY: all install uninstall
