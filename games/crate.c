#include "canvas.h"

#include <math.h>
#include <stdio.h>

/* Textured-triangle example. The game projects a cube; the host only fills
 * 2D triangles with UVs (affine, like a simple hardware rasterizer). */

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    unsigned tex;
    float yaw, pitch;
    int auto_spin;
} Crate;

static Vec3 v3(float x, float y, float z)
{
    Vec3 p;
    p.x = x;
    p.y = y;
    p.z = z;
    return p;
}

static Vec3 rot(Vec3 p, float yaw, float pitch)
{
    float cy = cosf(yaw), sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);
    float x = p.x * cy + p.z * sy;
    float z = -p.x * sy + p.z * cy;
    float y = p.y * cp - z * sp;
    z = p.y * sp + z * cp;
    return v3(x, y, z);
}

static int project(Vec3 p, float w, float h, float *sx, float *sy)
{
    float z = p.z + 3.2f;
    if (z < 0.2f)
        return 0;
    *sx = w * 0.5f + (p.x / z) * (0.7f * h);
    *sy = h * 0.5f - (p.y / z) * (0.7f * h);
    return 1;
}

static int front(Vec3 a, Vec3 b, Vec3 c)
{
    float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    return (uy * vz - uz * vy) * (-a.x) + (uz * vx - ux * vz) * (-a.y) +
               (ux * vy - uy * vx) * (-(a.z + 3.2f)) >
           0.0f;
}

static unsigned make_crate_tex(Canvas *c)
{
    unsigned char px[32 * 32 * 4];
    int y, x;
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            int i = (y * 32 + x) * 4;
            int edge = x < 2 || y < 2 || x > 29 || y > 29;
            int plank = ((x / 8) + (y / 8)) & 1;
            int cross = (x > 14 && x < 17) || (y > 14 && y < 17);
            px[i + 0] = (unsigned char)(edge ? 40 : cross ? 210 : plank ? 170 : 120);
            px[i + 1] = (unsigned char)(edge ? 28 : cross ? 140 : plank ? 95 : 70);
            px[i + 2] = (unsigned char)(edge ? 18 : cross ? 50 : plank ? 40 : 28);
            px[i + 3] = 255;
        }
    }
    return canvas_texture_rgba(c, 32, 32, px);
}

static void draw_face(Canvas *c, const Crate *g, Vec3 a, Vec3 b, Vec3 d, Vec3 e, float shade)
{
    float w = (float)canvas_width(c), h = (float)canvas_height(c);
    float ax, ay, bx, by, dx, dy, ex, ey;
    a = rot(a, g->yaw, g->pitch);
    b = rot(b, g->yaw, g->pitch);
    d = rot(d, g->yaw, g->pitch);
    e = rot(e, g->yaw, g->pitch);
    if (!front(a, b, d))
        return;
    if (!project(a, w, h, &ax, &ay) || !project(b, w, h, &bx, &by) || !project(d, w, h, &dx, &dy) ||
        !project(e, w, h, &ex, &ey))
        return;
    canvas_fill_triangle_tex(c, g->tex, ax, ay, 0, 0, bx, by, 1, 0, dx, dy, 1, 1, shade, shade, shade,
                             1);
    canvas_fill_triangle_tex(c, g->tex, ax, ay, 0, 0, dx, dy, 1, 1, ex, ey, 0, 1, shade, shade, shade,
                             1);
}

static void *init(Canvas *c)
{
    Crate *g = canvas_calloc(1, sizeof(*g));
    g->tex = canvas_texture_file(c, "assets/plat/tiles.png");
    if (!g->tex)
        g->tex = make_crate_tex(c);
    canvas_texture_nearest(g->tex, 1);
    g->auto_spin = 1;
    canvas_set_title(c, "Crate - textured triangles  arrows rotate  space toggle spin");
    return g;
}

static void update(void *state, Canvas *c, float dt)
{
    Crate *g = state;
    if (canvas_key_pressed(c, KEY_ESCAPE) || canvas_key_pressed(c, KEY_Q))
        canvas_quit(c);
    if (canvas_key_pressed(c, KEY_SPACE))
        g->auto_spin = !g->auto_spin;
    if (canvas_key_down(c, KEY_LEFT) || canvas_key_down(c, KEY_A))
        g->yaw -= 1.6f * dt;
    if (canvas_key_down(c, KEY_RIGHT) || canvas_key_down(c, KEY_D))
        g->yaw += 1.6f * dt;
    if (canvas_key_down(c, KEY_UP) || canvas_key_down(c, KEY_W))
        g->pitch -= 1.4f * dt;
    if (canvas_key_down(c, KEY_DOWN) || canvas_key_down(c, KEY_S))
        g->pitch += 1.4f * dt;
    if (g->auto_spin) {
        g->yaw += 0.55f * dt;
        g->pitch += 0.22f * dt;
    }
}

static void render(void *state, Canvas *c)
{
    Crate *g = state;
    /* Each face: a--b
     *            |  |
     *            e--d   then two triangles a-b-d and a-d-e. */
    static const float F[6][4][3] = {
        {{1, 1, 1}, {1, 1, -1}, {1, -1, -1}, {1, -1, 1}},
        {{-1, 1, -1}, {-1, 1, 1}, {-1, -1, 1}, {-1, -1, -1}},
        {{-1, 1, 1}, {1, 1, 1}, {1, -1, 1}, {-1, -1, 1}},
        {{1, 1, -1}, {-1, 1, -1}, {-1, -1, -1}, {1, -1, -1}},
        {{-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1}},
        {{-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}},
    };
    static const float shade[6] = {1.0f, 0.55f, 0.85f, 0.7f, 0.95f, 0.4f};
    int i;

    canvas_clear(c, 0.08f, 0.09f, 0.12f);
    canvas_begin_hud(c);
    for (i = 0; i < 6; i++)
        draw_face(c, g, v3(F[i][0][0], F[i][0][1], F[i][0][2]), v3(F[i][1][0], F[i][1][1], F[i][1][2]),
                  v3(F[i][2][0], F[i][2][1], F[i][2][2]), v3(F[i][3][0], F[i][3][1], F[i][3][2]),
                  shade[i]);
    canvas_draw_text(c, 10, 22, "CRATE   canvas_fill_triangle_tex   arrows rotate   space spin", 0.9f,
                     0.92f, 0.85f);
    canvas_end_hud(c);
}

static void shutdown(void *state, Canvas *c)
{
    (void)c;
    canvas_free(state);
}

CANVAS_EXPORT const Game canvas_game = {
    .name = "Crate",
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
