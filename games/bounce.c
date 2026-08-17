#include "canvas.h"

#include <stdio.h>
#include <stdlib.h>

#define BALL_N 12
#define WORLD_W 1600.0f
#define WORLD_H 1000.0f

typedef struct {
    Sprite balls[BALL_N];
    Sheet balls_sheet;
    unsigned snd_hit;
    int drag_x, drag_y;
    int dragging;
    float hit_cool;
} Bounce;

static void *init(Canvas *c)
{
    Bounce *b = calloc(1, sizeof(*b));
    int i;

    if (!canvas_sheet_load(c, &b->balls_sheet, "assets/bounce/balls.png", 16, 16)) {
        unsigned t = canvas_texture_solid(c, 0.3f, 0.75f, 1.0f);
        canvas_sheet_init(&b->balls_sheet, t, 16, 16, 16, 16);
    }
    for (i = 0; i < BALL_N; i++) {
        sprite_from_sheet(&b->balls[i], &b->balls_sheet, i % canvas_sheet_count(&b->balls_sheet),
                          80.0f + (float)(i * 90), 80.0f + (float)((i * 47) % 400));
        b->balls[i].w = b->balls[i].h = 28.0f;
        b->balls[i].vx = 80.0f + (float)((i * 37) % 160);
        b->balls[i].vy = 60.0f + (float)((i * 53) % 140);
        if (i & 1)
            b->balls[i].vx = -b->balls[i].vx;
        b->balls[i].origin_x = 0.5f;
        b->balls[i].origin_y = 0.5f;
    }
    b->snd_hit = canvas_sound_tone(c, 340.0f, 40.0f, 0.35f);
    canvas_cam_bounds(c, 0, 0, WORLD_W, WORLD_H);
    canvas_cam_set(c, 200, 140);
    canvas_set_title(c, "Bounce — arrows pan camera, drag pans, wheel zoom");
    return b;
}

static void update(void *state, Canvas *c, float dt)
{
    Bounce *b = state;
    float cx, cy;
    int i;

    if (canvas_key_pressed(c, KEY_ESCAPE) || canvas_key_pressed(c, KEY_Q))
        canvas_quit(c);

    canvas_cam_get(c, &cx, &cy);
    if (canvas_mouse_pressed(c, 1)) {
        b->dragging = 1;
        b->drag_x = canvas_mouse_x(c);
        b->drag_y = canvas_mouse_y(c);
    }
    if (!canvas_mouse_down(c, 1))
        b->dragging = 0;
    if (b->dragging) {
        float z = canvas_cam_zoom_get(c);
        cx -= (float)(canvas_mouse_x(c) - b->drag_x) / z;
        cy -= (float)(canvas_mouse_y(c) - b->drag_y) / z;
        b->drag_x = canvas_mouse_x(c);
        b->drag_y = canvas_mouse_y(c);
    }
    if (canvas_key_down(c, KEY_LEFT) || canvas_key_down(c, KEY_A))
        cx -= 720.0f * dt;
    if (canvas_key_down(c, KEY_RIGHT) || canvas_key_down(c, KEY_D))
        cx += 720.0f * dt;
    if (canvas_key_down(c, KEY_UP) || canvas_key_down(c, KEY_W))
        cy -= 720.0f * dt;
    if (canvas_key_down(c, KEY_DOWN) || canvas_key_down(c, KEY_S))
        cy += 720.0f * dt;
    canvas_cam_set(c, cx, cy);

    if (canvas_wheel(c) != 0.0f) {
        float z = canvas_cam_zoom_get(c) + canvas_wheel(c) * 0.08f;
        if (z < 0.4f)
            z = 0.4f;
        if (z > 2.0f)
            z = 2.0f;
        canvas_cam_zoom(c, z);
    }

    b->hit_cool -= dt;
    for (i = 0; i < BALL_N; i++) {
        Sprite *s = &b->balls[i];
        int hit = 0;
        sprite_update(s, dt);
        s->angle += 90.0f * dt;
        if (s->x < 14) {
            s->x = 14;
            s->vx = -s->vx;
            hit = 1;
        }
        if (s->y < 14) {
            s->y = 14;
            s->vy = -s->vy;
            hit = 1;
        }
        if (s->x > WORLD_W - 14) {
            s->x = WORLD_W - 14;
            s->vx = -s->vx;
            hit = 1;
        }
        if (s->y > WORLD_H - 14) {
            s->y = WORLD_H - 14;
            s->vy = -s->vy;
            hit = 1;
        }
        if (hit && b->hit_cool <= 0.0f) {
            canvas_sound_play(c, b->snd_hit, 0.28f);
            b->hit_cool = 0.05f;
        }
    }
}

static void render(void *state, Canvas *c)
{
    Bounce *b = state;
    int i;

    canvas_clear(c, 0.07f, 0.08f, 0.12f);
    canvas_fill_rect(c, 0, 0, WORLD_W, WORLD_H, 0.12f, 0.14f, 0.20f, 1);
    for (i = 0; i <= (int)WORLD_W; i += 80)
        canvas_draw_line(c, (float)i, 0, (float)i, WORLD_H, 0.18f, 0.22f, 0.30f, 1);
    for (i = 0; i <= (int)WORLD_H; i += 80)
        canvas_draw_line(c, 0, (float)i, WORLD_W, (float)i, 0.18f, 0.22f, 0.30f, 1);
    canvas_stroke_rect(c, 0, 0, WORLD_W, WORLD_H, 0.4f, 0.7f, 1, 0.6f);
    for (i = 0; i < BALL_N; i++)
        canvas_draw_sprite(c, &b->balls[i]);

    {
        float cx, cy;
        char hud[96];
        canvas_cam_get(c, &cx, &cy);
        canvas_begin_hud(c);
        snprintf(hud, sizeof(hud), "Arrows/WASD pan camera   drag pans   wheel zoom   cam %.0f,%.0f",
                 cx, cy);
        canvas_draw_text(c, 10, 20, hud, 0.85f, 0.9f, 1.0f);
        canvas_end_hud(c);
    }
}

static void shutdown(void *state, Canvas *c)
{
    (void)c;
    free(state);
}

CANVAS_EXPORT const Game canvas_game = {
    .name = "Bounce",
    .width = 800,
    .height = 560,
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
