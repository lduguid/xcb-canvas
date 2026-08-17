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
    unsigned tex_player;
    unsigned tex_orb;
    unsigned tex_grass;
    unsigned tex_dirt;
    unsigned snd_step, snd_place;
    float bob, step_t;
} Wander;

static void put_px(unsigned char *p, int w, int x, int y, int r, int g, int b, int a)
{
    int i = (y * w + x) * 4;
    p[i] = (unsigned char)r;
    p[i + 1] = (unsigned char)g;
    p[i + 2] = (unsigned char)b;
    p[i + 3] = (unsigned char)a;
}

static unsigned make_grass(Canvas *c)
{
    unsigned char px[16 * 16 * 4];
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int n = ((x * 13 + y * 7) & 7);
            put_px(px, 16, x, y, 46 + n * 4, 110 + n * 3, 52 + n, 255);
        }
    }
    return canvas_texture_rgba(c, 16, 16, px);
}

static unsigned make_dirt(Canvas *c)
{
    unsigned char px[16 * 16 * 4];
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int n = ((x * 5 + y * 11) & 7);
            put_px(px, 16, x, y, 92 + n * 3, 70 + n * 2, 42 + n, 255);
        }
    }
    return canvas_texture_rgba(c, 16, 16, px);
}

static unsigned make_player(Canvas *c)
{
    /* 4-frame 16x20 strip */
    const int fw = 16, fh = 20, frames = 4;
    unsigned char *px = calloc((size_t)fw * frames * fh, 4);
    int f, x, y;

    for (f = 0; f < frames; f++) {
        int ox = f * fw;
        int leg = (f == 1) ? -1 : (f == 3) ? 1 : 0;
        for (y = 0; y < fh; y++) {
            for (x = 0; x < fw; x++)
                put_px(px, fw * frames, ox + x, y, 0, 0, 0, 0);
        }
        for (y = 3; y <= 8; y++)
            for (x = 5; x <= 10; x++)
                put_px(px, fw * frames, ox + x, y, 240, 210, 170, 255);
        for (y = 9; y <= 14; y++)
            for (x = 4; x <= 11; x++)
                put_px(px, fw * frames, ox + x, y, 40, 90, 180, 255);
        for (y = 15; y <= 19; y++) {
            put_px(px, fw * frames, ox + 6 + leg, y, 40, 40, 50, 255);
            put_px(px, fw * frames, ox + 9 - leg, y, 40, 40, 50, 255);
        }
        for (x = 5; x <= 10; x++)
            put_px(px, fw * frames, ox + x, 2, 30, 30, 35, 255);
    }
    {
        unsigned tex = canvas_texture_rgba(c, fw * frames, fh, px);
        free(px);
        return tex;
    }
}

static unsigned make_orb(Canvas *c)
{
    unsigned char px[12 * 12 * 4];
    int x, y;
    for (y = 0; y < 12; y++) {
        for (x = 0; x < 12; x++) {
            float dx = x - 5.5f, dy = y - 5.5f;
            float d = dx * dx + dy * dy;
            if (d < 22.0f)
                put_px(px, 12, x, y, 255, 210, 70, 255);
            else if (d < 28.0f)
                put_px(px, 12, x, y, 220, 140, 30, 220);
            else
                put_px(px, 12, x, y, 0, 0, 0, 0);
        }
    }
    return canvas_texture_rgba(c, 12, 12, px);
}

static void *init(Canvas *c)
{
    Wander *w = calloc(1, sizeof(*w));
    int i;

    w->tex_grass = make_grass(c);
    w->tex_dirt = make_dirt(c);
    w->tex_player = make_player(c);
    w->tex_orb = make_orb(c);

    sprite_init(&w->player, WORLD_W * 0.5f, WORLD_H * 0.5f, 32, 40, w->tex_player);
    w->player.frames = 4;
    w->player.frame_w = 16;
    w->player.frame_h = 20;
    w->player.fps = 8.0f;
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
            unsigned tex = ((tx + ty) & 7) == 0 ? w->tex_dirt : w->tex_grass;
            canvas_blit(c, tex, tx * TILE, ty * TILE, TILE, TILE, 0, 0, 1, 1, 1, 1, 1, 1);
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
    free(state);
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
