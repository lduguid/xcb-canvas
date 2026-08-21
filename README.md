# xcb-canvas

You are writing a 2D game. The host owns the window, OpenGL, input, and audio. Most games are **one C file**: include only `include/canvas.h` and export a `Game` table. The same file runs on Linux (X11) and Windows.

This is not an emulator of any particular machine — just a small subset of 2D graphics so games share one platform API.

Do not include X11, Win32, OpenGL, or anything under `src/`.

Shipped games: `wander`, `bounce`, `pacman`, `plat`, `vector`, `crate`, and `arpg` (Crypt). Steal from those after the catch sketch below. Crypt is the exception to “one file”. Layers:

- **canvas** — window, input, draw, audio (`canvas.h`). Shared by every game.
- **rpg/** — ARPG engine API: stat bags, items, inventory, equipment, gold/XP, ground loot, melee dispatch, zone graph. No `canvas.h`. A game binds `RpgRules` (which stats exist, bag size, formulas, item text).
- **this game** — `games/crypt_tune.c` (all balance numbers), `games/crypt.c` (Haven, items, rules callbacks), `games/arpg.c` (draw / input). Edit tunables, F5; F6 for a new hero.

List extra `.c` files in `games/<name>.files` so F5 rebuilds them too.

## Build and run

Linux:

```bash
make
./bounce
```

Windows (from this directory, or `make mingw` / `make w64` / `make msvc` in the parent `xcb-examples` tree):

```bash
make -f Makefile.win32                 # this host's MinGW, unsuffixed names
make -f Makefile.win32 TAG=-mingw      # bounce-mingw.exe + canvas-mingw.dll
make -f Makefile.win32 TOOLSET=msvc TAG=-msvc
```

A tagged host loads matching tagged DLLs: `bounce-msvc.exe` wants `canvas-msvc.dll` and `bounce-msvc.dll` next to it. Keep `assets/` beside the exe.

`./canvas bounce` (or `canvas.exe bounce`) also works. F5 rebuilds the plugin and keeps heap state; F6 does a full `shutdown` + `init`. F1 hides the overlay.

On Windows, F5 uses MinGW unless you set the environment variable `CANVAS_CC` to `msvc` or `mingw`, or put that word in a `canvas-cc` file next to the exe. Use the same compiler the host was built with.

Linux runtime: X11, XCB, OpenGL/GLX, ALSA. Windows: GDI, OpenGL, waveOut (no extra install).

## What a game is

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

| Function | When | What you do |
|----------|------|-------------|
| `init` | Start and after F6 | Allocate state, load textures/sounds, return the pointer |
| `update` | Every frame | Input and movement. `dt` is seconds (capped) |
| `render` | After update | Draw only. Do not change gameplay here |
| `shutdown` | Quit or F6 | Free what `init` allocated |
| `hot_reload` | Optional, after F5 | Fix pointers if you added struct fields |

`width` / `height` are the starting window. The player can resize; read `canvas_width` / `canvas_height` if you care.

On Windows, allocate game state with `canvas_calloc` / `canvas_free` (not libc `malloc`) so F5 can unload the plugin without freeing across CRTs.

## Play logs

Set `CANVAS_LOG` to a file (or `-` for stdout). Each line is one JSON object: `session`, `input`, `state` (from optional `Game.observe`), `trace` (named events), `end`. `state` is written when the snapshot changes, and at least once per second (stuck detection). `CANVAS_SEED` makes a run repeatable. Recording locks the sim to 60 Hz so the log can be watched later.

```bash
CANVAS_LOG=run.jsonl CANVAS_SEED=1 ./arpg
```

Play the same log back through the game (seed and timed input, not a second renderer). The engine runs as normal; you watch it like a recording. Esc or Q quits.

```bash
CANVAS_REPLAY=run.jsonl ./arpg
```

A later script or model can also diff two logs for HP/gold jumps, town damage, death, or a `state` line whose `x,y` never moves. Games must not change gameplay in `observe`. Optional `canvas_trace` is a no-op without a log. `canvas_inject_key` / `canvas_inject_mouse` / `canvas_inject_click` are the same path replay uses.

## Crypt (`./arpg`)

Diablo-like loop on the canvas host. RPG rules stay in `rpg/` (no `canvas.h`). Crypt is content + presentation.

```bash
make arpg && ./arpg
```

Keep `assets/` next to the exe. Extra F5 sources: `games/arpg.files`. Details of the engine: `rpg/README.md`.

**Play.** Click to walk, attack, or take loot. WASD also moves. **I**, **C**, or **Tab** toggles the character sheet (same key closes it; Esc quits). Space uses / talks to a place. 1 / 2 drink potions. Haven is safe (vendor, stash, gate). The Wilds has imps and four dungeon portals. Death returns to Haven with a gold tax.

**Tune.** `games/crypt_tune.c` is all the numbers. F5 applies live formulas (HP, melee, walk). F6 starts a new hero, kit, and world.

**Logs.** `CANVAS_LOG=run.jsonl CANVAS_SEED=1 ./arpg` then `CANVAS_REPLAY=run.jsonl ./arpg`. Replay feeds recorded input through the same engine so you can watch a run.

### Flare art

Icons, a portrait, floor/wall crops, and a portal rune in `assets/crypt/` come from **Flare: Empyrean Campaign**, not from our own pixel sheets. They are **CC-BY-SA 3.0** (later versions allowed). File list and artists: `assets/crypt/CREDITS`.

- Game data / art we copied: [flareteam/flare-game](https://github.com/flareteam/flare-game)
- Engine those assets were built for: [flareteam/flare-engine](https://github.com/flareteam/flare-engine)

This project is not affiliated with Flare. Share-alike applies to those assets and any edits of them; it does not GPL the canvas host.

## A first game

Put this in `games/catch.c`. A square you steer; collect a coin.

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
    Catch *g = canvas_calloc(1, sizeof(*g));
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
    canvas_free(state);
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

Always multiply movement by `dt`.

Add `catch` to `GAMES` in `Makefile` and `Makefile.win32`, then `make catch` / `./catch`.

## Coordinates, color, input

Origin is **top-left**. `y` grows down. Colors are floats **0–1**.

```c
if (canvas_key_down(c, KEY_SPACE))     { /* while held */ }
if (canvas_key_pressed(c, KEY_SPACE))  { /* this frame only */ }
if (canvas_key_released(c, KEY_SPACE)) { /* just let go */ }
```

Mouse `1/2/3` is left/right/middle. `canvas_mouse_x` / `canvas_mouse_y` are **screen** pixels. Convert with `canvas_screen_to_world` if the camera has moved.

## Drawing, camera, sprites

From `render`: `canvas_clear`, `canvas_fill_rect`, `canvas_stroke_rect`, `canvas_draw_line`, `canvas_draw_pixel`, `canvas_fill_triangle`, `canvas_stroke_triangle`, `canvas_fill_triangle_tex`, `canvas_draw_text`. Wrap HUD in `canvas_begin_hud` / `canvas_end_hud` so score ignores the camera.

World larger than the window:

```c
canvas_cam_bounds(c, 0, 0, 3200, 2200);
canvas_cam_follow(c, player_x, player_y, 8.0f);
```

PNG/JPEG/BMP via `canvas_texture_file` (stb_image, path relative to the exe). `sprite_init` / `sprite_update` / `canvas_draw_sprite`. Pack a grid with `canvas_sheet_load` and `sprite_from_sheet` / `sprite_anim` — see `games/bounce.c` and `games/wander.c`.

## Sound

No audio files. In `init`, `canvas_sound_tone` / `canvas_sound_noise` / `canvas_sound_pcm`. Later `canvas_sound_play`, `canvas_sound_loop`, `canvas_sound_stop`. Silent if there is no device.

## Hot reload

Edit `games/yourgame.c`, press **F5**. The heap from `init` is **not** freed. Changing `update`/`render` is safe. Adding a field to your state struct is not, unless you handle it in `hot_reload` or press **F6**. Do not store function pointers from the old plugin in state.

If the plugin is more than one `.c` file, list the extras (one path per line) in `games/<name>.files`. F5 compiles `games/<name>.c` plus those. See `games/arpg.files`.

## Credits

PNG/JPEG/BMP: **stb_image** v2.30, Sean Barrett, public domain (`src/stb_image.h`).

Crypt art sample: **Flare: Empyrean Campaign** (Clint Bellanger and contributors), [CC-BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/). Repos: [flare-game](https://github.com/flareteam/flare-game) (art we used) and [flare-engine](https://github.com/flareteam/flare-engine). See `assets/crypt/CREDITS`.
