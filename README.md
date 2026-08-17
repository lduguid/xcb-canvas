# xcb-canvas

A small 2D game canvas. You write one C file, include only `include/canvas.h`, and fill in a `Game` table. The same game runs on Linux (X11) and Windows.

Shipped examples: `wander` (top-down), `bounce` (camera + sprites), `pacman`, `plat` (platformer).

## Build and run

Linux:

```bash
make
./bounce
```

Windows (from WSL, or native MinGW):

```bash
make -f Makefile.win32
plat.exe
```

`./canvas bounce` and `canvas.exe bounce` also work. Each named binary (`./plat`, `plat.exe`) is a host that loads `plat.so` / `plat.dll`.

| Key | Action |
|-----|--------|
| F5 | Rebuild the game plugin and keep heap state |
| F6 | Rebuild and call `shutdown` + `init` again |
| F1 | Hide / show the overlay |

On Windows, F5 uses MinGW-w64 by default. To use Visual Studio's `cl` instead:

```bat
set CANVAS_CC=msvc
```

or put `msvc` or `mingw` in a `canvas-cc` file next to the `.exe`. After switching compilers, press F6 once.

## The one rule

Game code includes **only** `canvas.h`. Do not include X11, Win32, OpenGL, or anything under `src/`. The host owns the window, GL, input, and audio.

## What a game is

You export one `Game` named `canvas_game`:

```c
CANVAS_EXPORT const Game canvas_game = {
    .name = "My game",
    .width = 800,
    .height = 560,
    .init = init,
    .update = update,
    .render = render,
    .shutdown = shutdown,
};
```

The host calls those functions for you.

| Function | When | What you do |
|----------|------|-------------|
| `init` | Once at start (and after F6) | `calloc` your state, load sounds/textures, return the pointer |
| `update` | Every frame | Read input, move things. `dt` is seconds since last frame (capped) |
| `render` | Every frame after update | Draw. Do not change gameplay state here |
| `shutdown` | Quit or F6 | `free` what `init` allocated |
| `hot_reload` | Optional, after F5 | Fix pointers if you added fields; state is the old heap |

`width` / `height` are the starting window size. The player can resize; use `canvas_width` / `canvas_height` if you care.

## A first game

Put this in `games/catch.c`. A square you steer with the arrows; collect a coin.

```c
#include "canvas.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    float x, y;
    float coin_x, coin_y;
    int score;
    unsigned blip;
} Catch;

static void *init(Canvas *c)
{
    Catch *g = calloc(1, sizeof(*g));
    g->x = 380.0f;
    g->y = 260.0f;
    g->coin_x = 120.0f;
    g->coin_y = 90.0f;
    g->blip = canvas_sound_tone(c, 880.0f, 80.0f, 0.4f);
    canvas_set_title(c, "Catch");
    return g;
}

static void update(void *state, Canvas *c, float dt)
{
    Catch *g = state;
    float speed = 260.0f;

    if (canvas_key_down(c, KEY_LEFT))  g->x -= speed * dt;
    if (canvas_key_down(c, KEY_RIGHT)) g->x += speed * dt;
    if (canvas_key_down(c, KEY_UP))    g->y -= speed * dt;
    if (canvas_key_down(c, KEY_DOWN))  g->y += speed * dt;
    if (canvas_key_pressed(c, KEY_ESCAPE))
        canvas_quit(c);

    if (g->x < g->coin_x + 16 && g->x + 32 > g->coin_x &&
        g->y < g->coin_y + 16 && g->y + 32 > g->coin_y) {
        g->score++;
        g->coin_x = 40.0f + (float)(g->score * 97 % 700);
        g->coin_y = 40.0f + (float)(g->score * 53 % 480);
        canvas_sound_play(c, g->blip, 0.5f);
    }
}

static void render(void *state, Canvas *c)
{
    Catch *g = state;
    char line[32];

    canvas_clear(c, 0.08f, 0.10f, 0.14f);
    canvas_fill_rect(c, g->coin_x, g->coin_y, 16, 16, 1.0f, 0.85f, 0.2f, 1.0f);
    canvas_fill_rect(c, g->x, g->y, 32, 32, 0.3f, 0.7f, 1.0f, 1.0f);

    canvas_begin_hud(c);
    snprintf(line, sizeof(line), "score %d", g->score);
    canvas_draw_text(c, 10, 20, line, 1, 1, 1);
    canvas_end_hud(c);
}

static void shutdown(void *state, Canvas *c)
{
    (void)c;
    free(state);
}

CANVAS_EXPORT const Game canvas_game = {
    .name = "Catch",
    .width = 800,
    .height = 560,
    .init = init,
    .update = update,
    .render = render,
    .shutdown = shutdown,
};
```

Always multiply movement by `dt`. If you write `g->x += 260` with no `dt`, the square teleports and the game runs at different speeds on different machines.

## Coordinates and color

- Origin is the **top-left**. `x` grows right, `y` grows **down**.
- Units are pixels in world space (until you zoom the camera).
- Colors are floats **0 to 1**, not 0–255. `(1, 0, 0)` is red. The last value on rects is alpha (1 = opaque).

## Input

Held this frame vs just pressed this frame:

```c
if (canvas_key_down(c, KEY_SPACE))    { /* jump height while held */ }
if (canvas_key_pressed(c, KEY_SPACE)) { /* start a jump once */ }
if (canvas_key_released(c, KEY_SPACE)) { /* cut the jump */ }
```

Keys: `KEY_LEFT`…`KEY_DOWN`, `KEY_A`…`KEY_Z`, `KEY_0`…`KEY_9`, `KEY_SPACE`, `KEY_ENTER`, `KEY_ESCAPE`, `KEY_SHIFT`, `KEY_F1`, `KEY_F5`, `KEY_F6`, …

Mouse: buttons `1` (left), `2` (right), `3` (middle). `canvas_mouse_x` / `canvas_mouse_y` are **screen** pixels. Convert with `canvas_screen_to_world` if the camera has moved.

```c
float wx, wy;
canvas_screen_to_world(c, (float)canvas_mouse_x(c), (float)canvas_mouse_y(c), &wx, &wy);
```

## Drawing

Call these from `render`:

```c
canvas_clear(c, r, g, b);
canvas_fill_rect(c, x, y, w, h, r, g, b, a);
canvas_stroke_rect(c, x, y, w, h, r, g, b, a);
canvas_draw_line(c, x1, y1, x2, y2, r, g, b, a);
canvas_draw_text(c, x, y, "hello", r, g, b);
```

Text is a small built-in bitmap font. It is drawn in the current space (world or HUD).

**HUD** (score, lives) should ignore the camera:

```c
canvas_begin_hud(c);
canvas_draw_text(c, 10, 20, "lives 3", 1, 1, 1);
canvas_end_hud(c);
```

## Camera

Useful when the world is larger than the window (`bounce`, `wander`, `plat`).

```c
canvas_cam_bounds(c, 0, 0, 3200, 2200);   /* clamp, optional */
canvas_cam_set(c, player_x, player_y);    /* jump */
canvas_cam_follow(c, player_x, player_y, 8.0f); /* smooth; stiffness ~ 4–12 */
canvas_cam_zoom(c, 1.0f);
```

`canvas_cam_follow` aims at a point; the host eases toward it each frame.

## Sprites and textures

A `Sprite` is a rectangle plus velocity, tint, and optional animation.

```c
Sprite s;
unsigned tex = canvas_texture_solid(c, 1.0f, 0.4f, 0.2f);
sprite_init(&s, 100, 80, 32, 32, tex);
s.vx = 120.0f;
s.origin_x = 0.5f;  /* rotate/flip around center */
s.origin_y = 0.5f;

/* in update */
sprite_update(&s, dt);   /* x += vx * dt, advances frames */

/* in render */
canvas_draw_sprite(c, &s);
```

Make a texture from raw RGBA bytes (one pixel = 4 bytes, red first):

```c
unsigned char px[16 * 16 * 4];
/* fill px ... */
unsigned tex = canvas_texture_rgba(c, 16, 16, px);
canvas_texture_nearest(tex, 1);  /* chunky pixels */
```

For a walk cycle, pack frames in a **horizontal strip**. Set `s.frames`, `s.frame_w`, `s.frame_h`, and `s.fps`. See `games/wander.c`.

`canvas_blit` draws a sub-rectangle of a texture (UV 0–1) if you need more control than `Sprite`.

## Sound

No audio files. You generate clips once in `init`, then play them.

```c
unsigned hop  = canvas_sound_tone(c, 520.0f, 90.0f, 0.4f);   /* square beep */
unsigned land = canvas_sound_noise(c, 40.0f, 0.3f);          /* noise burst */
canvas_sound_play(c, hop, 0.5f);   /* vol 0–1 */
canvas_sound_loop(c, land, 0.2f);
canvas_sound_stop(c, land);
```

`canvas_sound_pcm` copies your own `short` samples if you want a custom waveform. If the machine has no audio device, play calls are silent.

## Hot reload

Edit `games/yourgame.c`, press **F5**. The host rebuilds the `.so` / `.dll` and swaps the function table. Your `init` heap is **not** freed.

That means:

- Changing code in `update` / `render` is safe.
- Adding a field to your state struct is not safe unless you handle it in `hot_reload`, or you press **F6** (full reset).
- Do not store function pointers from the old plugin inside your state.

## Add your game to the build

1. Create `games/catch.c` as above (the `CANVAS_EXPORT const Game canvas_game` block is required).
2. Linux — add `catch` to `GAMES` in `Makefile`. The `%.so` rule builds the plugin; copy a `plat:` host line for `catch:`.
3. Windows — add `catch` to `GAMES` in `Makefile.win32` the same way, or copy the `plat.exe` / `plat.dll` rules.

Then:

```bash
make catch
./catch
```

On Windows, `catch.exe` loads `catch.dll`. Keep `canvas.dll` next to the `.exe`.

## Example games

| File | Ideas to steal |
|------|----------------|
| `games/bounce.c` | Sprites, camera pan/zoom, generated texture, hit beep |
| `games/wander.c` | Tile world, follow-cam, walk frames, mouse in world space |
| `games/plat.c` | Gravity, jumps, tile collisions, enemies |
| `games/pacman.c` | Grid movement, AI, frightened mode |

Read those after the catch example. They are still just `canvas.h` plus your own structs.

## API list

From `include/canvas.h`:

**Window / time:** `canvas_width`, `canvas_height`, `canvas_set_title`, `canvas_quit`, `canvas_time`, `canvas_resized`

**Keys / mouse:** `canvas_key_down`, `canvas_key_pressed`, `canvas_key_released`, `canvas_mouse_x`, `canvas_mouse_y`, `canvas_mouse_down`, `canvas_mouse_pressed`, `canvas_wheel`, `canvas_wheel_h`

**Camera:** `canvas_cam_set`, `canvas_cam_get`, `canvas_cam_follow`, `canvas_cam_bounds`, `canvas_cam_zoom`, `canvas_cam_zoom_get`, `canvas_view`, `canvas_screen_to_world`

**Draw:** `canvas_clear`, `canvas_fill_rect`, `canvas_stroke_rect`, `canvas_draw_line`, `canvas_draw_text`, `canvas_begin_hud`, `canvas_end_hud`

**Images:** `canvas_texture_rgba`, `canvas_texture_solid`, `canvas_texture_nearest`, `sprite_init`, `sprite_update`, `canvas_draw_sprite`, `canvas_blit`

**Sound:** `canvas_sound_pcm`, `canvas_sound_tone`, `canvas_sound_noise`, `canvas_sound_play`, `canvas_sound_loop`, `canvas_sound_stop`
