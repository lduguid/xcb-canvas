CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -pthread -Iinclude -Isrc $(shell pkg-config --cflags gl glx x11 xcb alsa)
MACHINE = $(shell gcc -dumpmachine)
X11XCB = $(shell test -e /usr/lib/$(MACHINE)/libX11-xcb.so && echo -lX11-xcb || echo /usr/lib/$(MACHINE)/libX11-xcb.so.1)
LIBS = $(shell pkg-config --libs gl glx x11 xcb alsa) $(X11XCB) -lm -pthread

CORE = src/canvas_core.o
PLAT = src/canvas_x11.o
CANVAS_OBJ = $(CORE) $(PLAT)

all: wander bounce pacman plat

src/canvas_core.o: src/canvas_core.c src/canvas_internal.h include/canvas.h
	$(CC) $(CFLAGS) -c -o $@ src/canvas_core.c

src/canvas_x11.o: src/canvas_x11.c src/canvas_internal.h include/canvas.h
	$(CC) $(CFLAGS) -c -o $@ src/canvas_x11.c

wander: games/wander.c $(CANVAS_OBJ) include/canvas.h
	$(CC) $(CFLAGS) -o $@ games/wander.c $(CANVAS_OBJ) $(LIBS)

bounce: games/bounce.c $(CANVAS_OBJ) include/canvas.h
	$(CC) $(CFLAGS) -o $@ games/bounce.c $(CANVAS_OBJ) $(LIBS)

pacman: games/pacman.c $(CANVAS_OBJ) include/canvas.h
	$(CC) $(CFLAGS) -o $@ games/pacman.c $(CANVAS_OBJ) $(LIBS)

plat: games/plat.c $(CANVAS_OBJ) include/canvas.h
	$(CC) $(CFLAGS) -o $@ games/plat.c $(CANVAS_OBJ) $(LIBS)

clean:
	rm -f wander bounce pacman plat src/canvas_core.o src/canvas_x11.o
	rm -f wander.exe bounce.exe pacman.exe plat.exe src/*.win32.o
