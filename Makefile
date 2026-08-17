CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -pthread -Iinclude -Isrc $(shell pkg-config --cflags gl glx x11 xcb alsa)
MACHINE = $(shell gcc -dumpmachine)
X11XCB = $(shell test -e /usr/lib/$(MACHINE)/libX11-xcb.so && echo -lX11-xcb || echo /usr/lib/$(MACHINE)/libX11-xcb.so.1)
LIBS = $(shell pkg-config --libs gl glx x11 xcb alsa) $(X11XCB) -lm -pthread

CORE = src/canvas_core.o
PLAT = src/canvas_x11.o
CANVAS_OBJ = $(CORE) $(PLAT)

GAMES = wander bounce pacman plat vector crate
PLUGINS = $(addsuffix .so,$(GAMES))

all: $(GAMES) canvas $(PLUGINS)

src/canvas_core.o: src/canvas_core.c src/canvas_internal.h include/canvas.h src/stb_image.h
	$(CC) $(CFLAGS) -c -o $@ src/canvas_core.c

src/canvas_x11.o: src/canvas_x11.c src/canvas_internal.h include/canvas.h
	$(CC) $(CFLAGS) -c -o $@ src/canvas_x11.c

HOST_SRC = src/canvas_host.c

canvas: $(HOST_SRC) $(CANVAS_OBJ) include/canvas.h
	$(CC) $(CFLAGS) -rdynamic -o $@ $(HOST_SRC) $(CANVAS_OBJ) $(LIBS) -ldl

wander: $(HOST_SRC) $(CANVAS_OBJ) wander.so include/canvas.h
	$(CC) $(CFLAGS) -rdynamic -DCANVAS_LAUNCH=\"wander\" -o $@ $(HOST_SRC) $(CANVAS_OBJ) $(LIBS) -ldl

bounce: $(HOST_SRC) $(CANVAS_OBJ) bounce.so include/canvas.h
	$(CC) $(CFLAGS) -rdynamic -DCANVAS_LAUNCH=\"bounce\" -o $@ $(HOST_SRC) $(CANVAS_OBJ) $(LIBS) -ldl

pacman: $(HOST_SRC) $(CANVAS_OBJ) pacman.so include/canvas.h
	$(CC) $(CFLAGS) -rdynamic -DCANVAS_LAUNCH=\"pacman\" -o $@ $(HOST_SRC) $(CANVAS_OBJ) $(LIBS) -ldl

plat: $(HOST_SRC) $(CANVAS_OBJ) plat.so include/canvas.h
	$(CC) $(CFLAGS) -rdynamic -DCANVAS_LAUNCH=\"plat\" -o $@ $(HOST_SRC) $(CANVAS_OBJ) $(LIBS) -ldl

vector: $(HOST_SRC) $(CANVAS_OBJ) vector.so include/canvas.h
	$(CC) $(CFLAGS) -rdynamic -DCANVAS_LAUNCH=\"vector\" -o $@ $(HOST_SRC) $(CANVAS_OBJ) $(LIBS) -ldl

crate: $(HOST_SRC) $(CANVAS_OBJ) crate.so include/canvas.h
	$(CC) $(CFLAGS) -rdynamic -DCANVAS_LAUNCH=\"crate\" -o $@ $(HOST_SRC) $(CANVAS_OBJ) $(LIBS) -ldl

%.so: games/%.c include/canvas.h
	$(CC) $(CFLAGS) -shared -fPIC -DCANVAS_PLUGIN -o $@ $< -lm

clean:
	rm -f wander bounce pacman plat vector crate canvas src/canvas_core.o src/canvas_x11.o
	rm -f wander.exe bounce.exe pacman.exe plat.exe vector.exe crate.exe canvas.exe src/*.win32.o
	rm -f *.so *.dll libcanvas.a libcanvas_host.a canvas.lib canvas.exp *.obj *.pdb *.ilk
	rm -rf .hot
