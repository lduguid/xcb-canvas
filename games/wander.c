#include "canvas.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORLD_W 3200.0f
#define WORLD_H 2200.0f
#define TILE 32.0f
#define ORB_N 18

typedef struct {
    Sprite player;
    Sprite orbs[ORB_N];
    Sheet tiles;
    Sheet walk;
    unsigned tex_orb;
    unsigned snd_step, snd_place;
    float bob, step_t;
} Wander;

static void load_or_solid(Canvas *c, Sheet *s, const char *path, int cw, int ch)
{
    if (!canvas_sheet_load(c, s, path, cw, ch)) {
        unsigned t = canvas_texture_solid(c, 1.0f, 0.0f, 1.0f);
        canvas_sheet_init(s, t, cw, ch, cw, ch);
    }
}

static void *init(Canvas *c)
{
    Wander *w = canvas_calloc(1, sizeof(*w));
    int i;

    load_or_solid(c, &w->tiles, "assets/wander/tiles.png", 16, 16);
    load_or_solid(c, &w->walk, "assets/wander/hero.png", 16, 20);
    w->tex_orb = canvas_texture_file(c, "assets/wander/orb.png");
    if (!w->tex_orb)
        w->tex_orb = canvas_texture_solid(c, 1.0f, 0.82f, 0.27f);

    sprite_from_sheet(&w->player, &w->walk, 0, WORLD_W * 0.5f, WORLD_H * 0.5f);
    sprite_anim(&w->player, &w->walk, 0, 4, 8.0f);
    w->player.w = 32.0f;
    w->player.h = 40.0f;
    w->player.origin_x = 0.5f;
    w->player.origin_y = 1.0f;

    for (i = 0; i < ORB_N; i++) {
        sprite_init(&w->orbs[i], 80.0f + (float)((i * 197) % (int)WORLD_W),
                    80.0f + (float)((i * 311) % (int)WORLD_H), 18, 18, w->tex_orb);
        w->orbs[i].origin_x = 0.5f;
        w->orbs[i].origin_y = 0.5f;
    }

    w->snd_step = canvas_sound_noise(c, 70.0f, 0.35f);
    w->snd_place = canvas_sound_tone(c, 520.0f, 90.0f, 0.4f);
    canvas_cam_bounds(c, 0, 0, WORLD_W, WORLD_H);
    canvas_cam_set(c, w->player.x - canvas_width(c) * 0.5f, w->player.y - canvas_height(c) * 0.5f);
    canvas_set_title(c, "Wander — arrows/WASD move, click to place, wheel zoom");
    return w;
}

static void update(void *state, Canvas *c, float dt)
{
    Wander *w = state;
    float sp = 220.0f;
    float vx = 0, vy = 0;
    int i, moving;

    if (canvas_key_down(c, KEY_LEFT) || canvas_key_down(c, KEY_A))
        vx -= 1;
    if (canvas_key_down(c, KEY_RIGHT) || canvas_key_down(c, KEY_D))
        vx += 1;
    if (canvas_key_down(c, KEY_UP) || canvas_key_down(c, KEY_W))
        vy -= 1;
    if (canvas_key_down(c, KEY_DOWN) || canvas_key_down(c, KEY_S))
        vy += 1;
    if (canvas_key_pressed(c, KEY_ESCAPE) || canvas_key_pressed(c, KEY_Q))
        canvas_quit(c);
    if (canvas_mouse_pressed(c, 1)) {
        float wx, wy;
        canvas_screen_to_world(c, (float)canvas_mouse_x(c), (float)canvas_mouse_y(c), &wx, &wy);
        w->player.x = wx;
        w->player.y = wy;
        canvas_sound_play(c, w->snd_place, 0.7f);
    }

    if (vx != 0 && vy != 0) {
        vx *= 0.7071f;
        vy *= 0.7071f;
    }
    w->player.vx = vx * sp;
    w->player.vy = vy * sp;
    moving = (vx != 0 || vy != 0);
    w->player.fps = moving ? 10.0f : 0.0f;
    if (!moving)
        w->player.frame = 0;
    if (vx < 0)
        w->player.flip_x = 1;
    if (vx > 0)
        w->player.flip_x = 0;

    if (moving) {
        w->step_t -= dt;
        if (w->step_t <= 0.0f) {
            canvas_sound_play(c, w->snd_step, 0.35f);
            w->step_t = 0.22f;
        }
    } else {
        w->step_t = 0.0f;
    }

    sprite_update(&w->player, dt);
    if (w->player.x < 16)
        w->player.x = 16;
    if (w->player.y < 16)
        w->player.y = 16;
    if (w->player.x > WORLD_W - 16)
        w->player.x = WORLD_W - 16;
    if (w->player.y > WORLD_H - 16)
        w->player.y = WORLD_H - 16;

    w->bob += dt;
    for (i = 0; i < ORB_N; i++) {
        w->orbs[i].angle += 40.0f * dt;
        w->orbs[i].y += sinf(w->bob * 2.0f + (float)i) * 12.0f * dt;
    }

    if (canvas_wheel(c) != 0.0f) {
        float z = canvas_cam_zoom_get(c) + canvas_wheel(c) * 0.1f;
        if (z < 0.5f)
            z = 0.5f;
        if (z > 2.5f)
            z = 2.5f;
        canvas_cam_zoom(c, z);
    }

    canvas_cam_follow(c, w->player.x - canvas_width(c) / (2.0f * canvas_cam_zoom_get(c)),
                      w->player.y - canvas_height(c) / (2.0f * canvas_cam_zoom_get(c)), 6.0f);
}

static void render(void *state, Canvas *c)
{
    Wander *w = state;
    float vx, vy, vw, vh;
    int x0, y0, x1, y1, tx, ty, i;
    char hud[96];

    canvas_clear(c, 0.10f, 0.14f, 0.12f);
    canvas_view(c, &vx, &vy, &vw, &vh);
    x0 = (int)(vx / TILE) - 1;
    y0 = (int)(vy / TILE) - 1;
    x1 = (int)((vx + vw) / TILE) + 1;
    y1 = (int)((vy + vh) / TILE) + 1;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;

    for (ty = y0; ty <= y1; ty++) {
        for (tx = x0; tx <= x1; tx++) {
            int cell = ((tx + ty) & 7) == 0 ? 1 : 0;
            canvas_draw_sheet(c, &w->tiles, cell, tx * TILE, ty * TILE, TILE, TILE);
        }
    }

    canvas_stroke_rect(c, 0, 0, WORLD_W, WORLD_H, 0.2f, 0.9f, 0.4f, 0.4f);

    for (i = 0; i < ORB_N; i++)
        canvas_draw_sprite(c, &w->orbs[i]);
    canvas_draw_sprite(c, &w->player);

    canvas_begin_hud(c);
    canvas_fill_rect(c, 0, 0, (float)canvas_width(c), 28, 0, 0, 0, 0.45f);
    snprintf(hud, sizeof(hud), "Wander   world %.0fx%.0f   pos %.0f,%.0f   zoom %.2f", WORLD_W, WORLD_H,
             w->player.x, w->player.y, canvas_cam_zoom_get(c));
    canvas_draw_text(c, 10, 18, hud, 0.9f, 0.92f, 0.95f);
    canvas_end_hud(c);
}

static void shutdown(void *state, Canvas *c)
{
    (void)c;
    canvas_free(state);
}

CANVAS_EXPORT const Game canvas_game = {
    .name = "Wander",
    .width = 960,
    .height = 640,
    .init = init,
    .update = update,
    .render = render,
    .shutdown = shutdown,
};

#ifndef CANVAS_PLUGIN
int main(void)
{
    return canvas_run(&canvas_game);
}
#endif
