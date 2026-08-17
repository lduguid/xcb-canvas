#include "canvas.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Software 3D with only the 2D canvas primitives. Projection, back-face
 * cull, and meshes live in this file — the host never sees a 3D API.
 * Play is a Battlezone / Elite-style wireframe tank range. */

#define PI 3.14159265f
#define NEAR_Z 4.0f
#define ENEMY_N 4
#define SHOT_N 12
#define STAR_N 80
#define MTN_N 10

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float x, z, yaw;
    int alive;
    float cool;
    int hit;
} Tank;

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life;
    int friendly;
} Shot;

typedef struct {
    Tank you;
    Tank foe[ENEMY_N];
    Shot shot[SHOT_N];
    float star[STAR_N][2];
    float mtn_x[MTN_N], mtn_z[MTN_N], mtn_s[MTN_N];
    int score, lives, won;
    unsigned snd_shot, snd_hit, snd_boom;
} Vector;

static float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static float wrap_pi(float a)
{
    while (a > PI)
        a -= 2.0f * PI;
    while (a < -PI)
        a += 2.0f * PI;
    return a;
}

static float ang_to(float from, float to, float max_step)
{
    float d = wrap_pi(to - from);
    if (d > max_step)
        d = max_step;
    if (d < -max_step)
        d = -max_step;
    return from + d;
}

static Vec3 v3(float x, float y, float z)
{
    Vec3 p;
    p.x = x;
    p.y = y;
    p.z = z;
    return p;
}

/* Local mesh -> world (yaw about Y, then translate on XZ). */
static Vec3 place(Vec3 local, float x, float z, float yaw)
{
    float c = cosf(yaw), s = sinf(yaw);
    Vec3 w;
    w.x = x + local.x * c + local.z * s;
    w.y = local.y;
    w.z = z - local.x * s + local.z * c;
    return w;
}

static int project(const Vector *g, float width, float height, Vec3 world, float *sx, float *sy,
                   float *camz)
{
    float dx = world.x - g->you.x;
    float dy = world.y - 12.0f;
    float dz = world.z - g->you.z;
    /* Same basis as drive/fire: forward = (sin yaw, cos yaw) in XZ. */
    float c = cosf(g->you.yaw), s = sinf(g->you.yaw);
    float cx = dx * c - dz * s;
    float cz = dx * s + dz * c;
    float focal;

    *camz = cz;
    if (cz < NEAR_Z)
        return 0;
    focal = 0.62f * height;
    *sx = width * 0.5f + (cx / cz) * focal;
    *sy = height * 0.48f - (dy / cz) * focal;
    return 1;
}

static int face_front(Vec3 a, Vec3 b, Vec3 c, const Vector *g)
{
    float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float ex = g->you.x - a.x;
    float ey = 12.0f - a.y;
    float ez = g->you.z - a.z;
    return nx * ex + ny * ey + nz * ez > 0.0f;
}

static void draw_tri(Canvas *cv, const Vector *g, Vec3 a, Vec3 b, Vec3 c, float r, float gr, float bl,
                     int fill)
{
    float ax, ay, az, bx, by, bz, cx, cy, cz;
    float w = (float)canvas_width(cv), h = (float)canvas_height(cv);
    if (!face_front(a, b, c, g))
        return;
    if (!project(g, w, h, a, &ax, &ay, &az) || !project(g, w, h, b, &bx, &by, &bz) ||
        !project(g, w, h, c, &cx, &cy, &cz))
        return;
    if (fill)
        canvas_fill_triangle(cv, ax, ay, bx, by, cx, cy, r, gr, bl, 1.0f);
    else
        canvas_stroke_triangle(cv, ax, ay, bx, by, cx, cy, r, gr, bl, 1.0f);
}

static void draw_edge(Canvas *cv, const Vector *g, Vec3 a, Vec3 b, float r, float gr, float bl)
{
    float ax, ay, az, bx, by, bz;
    float w = (float)canvas_width(cv), h = (float)canvas_height(cv);
    if (!project(g, w, h, a, &ax, &ay, &az) || !project(g, w, h, b, &bx, &by, &bz))
        return;
    canvas_draw_line(cv, ax, ay, bx, by, r, gr, bl, 1.0f);
}

/* Box: 8 corners, 12 edges. y from 0. */
static void draw_box(Canvas *cv, const Vector *g, float x, float z, float yaw, float hx, float hy,
                     float hz, float r, float gr, float bl, int fill)
{
    Vec3 p[8];
    int e[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                    {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    int f[6][3] = {{0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6}, {0, 4, 5}, {0, 5, 1}};
    int i;
    static const float ox[8] = {-1, 1, 1, -1, -1, 1, 1, -1};
    static const float oy[8] = {0, 0, 0, 0, 1, 1, 1, 1};
    static const float oz[8] = {-1, -1, 1, 1, -1, -1, 1, 1};
    for (i = 0; i < 8; i++)
        p[i] = place(v3(ox[i] * hx, oy[i] * hy, oz[i] * hz), x, z, yaw);
    if (fill) {
        for (i = 0; i < 6; i++)
            draw_tri(cv, g, p[f[i][0]], p[f[i][1]], p[f[i][2]], r, gr, bl, 1);
    }
    for (i = 0; i < 12; i++)
        draw_edge(cv, g, p[e[i][0]], p[e[i][1]], r, gr, bl);
}

static void draw_pyramid(Canvas *cv, const Vector *g, float x, float z, float s, float r, float gr,
                         float bl)
{
    Vec3 base[4], apex;
    int i;
    base[0] = v3(x - s, 0, z - s);
    base[1] = v3(x + s, 0, z - s);
    base[2] = v3(x + s, 0, z + s);
    base[3] = v3(x - s, 0, z + s);
    apex = v3(x, s * 1.4f, z);
    for (i = 0; i < 4; i++) {
        draw_tri(cv, g, base[i], base[(i + 1) & 3], apex, r * 0.35f, gr * 0.35f, bl * 0.35f, 1);
        draw_tri(cv, g, base[i], base[(i + 1) & 3], apex, r, gr, bl, 0);
        draw_edge(cv, g, base[i], base[(i + 1) & 3], r, gr, bl);
    }
}

static void draw_tank(Canvas *cv, const Vector *g, const Tank *t, float r, float gr, float bl, int fill)
{
    if (!t->alive)
        return;
    draw_box(cv, g, t->x, t->z, t->yaw, 10.0f, 6.0f, 14.0f, r, gr, bl, fill);
    draw_box(cv, g, t->x, t->z, t->yaw, 6.0f, 10.0f, 6.0f, r, gr, bl, 0);
    draw_edge(cv, g, place(v3(0, 8, 6), t->x, t->z, t->yaw),
              place(v3(0, 8, 22), t->x, t->z, t->yaw), r, gr, bl);
}

static void fire(Vector *g, const Tank *from, int friendly)
{
    int i;
    float c = cosf(from->yaw), s = sinf(from->yaw);
    for (i = 0; i < SHOT_N; i++) {
        if (g->shot[i].life > 0.0f)
            continue;
        g->shot[i].x = from->x + s * 26.0f;
        g->shot[i].y = 8.0f;
        g->shot[i].z = from->z + c * 26.0f;
        g->shot[i].vx = s * 280.0f;
        g->shot[i].vy = 0;
        g->shot[i].vz = c * 280.0f;
        g->shot[i].life = 2.4f;
        g->shot[i].friendly = friendly;
        return;
    }
}

static float dist2(float x0, float z0, float x1, float z1)
{
    float dx = x1 - x0, dz = z1 - z0;
    return dx * dx + dz * dz;
}

/* Closest point on segment a→b to p, then circle test. Avoids tunneling. */
static int seg_hits(float ax, float az, float bx, float bz, float px, float pz, float rad)
{
    float abx = bx - ax, abz = bz - az;
    float apx = px - ax, apz = pz - az;
    float ab2 = abx * abx + abz * abz;
    float t = 0.0f, qx, qz;
    if (ab2 > 1e-6f) {
        t = (apx * abx + apz * abz) / ab2;
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;
    }
    qx = ax + abx * t;
    qz = az + abz * t;
    return dist2(qx, qz, px, pz) < rad * rad;
}

static void reset_world(Vector *g)
{
    int i;
    g->you.x = 0;
    g->you.z = 0;
    g->you.yaw = 0;
    g->you.alive = 1;
    g->you.cool = 0;
    g->you.hit = 0;
    g->score = 0;
    g->lives = 3;
    g->won = 0;
    memset(g->shot, 0, sizeof(g->shot));
    for (i = 0; i < ENEMY_N; i++) {
        float a = (float)i * (2.0f * PI / (float)ENEMY_N) + 0.4f;
        g->foe[i].x = cosf(a) * (420.0f + (float)(i * 50));
        g->foe[i].z = sinf(a) * (420.0f + (float)(i * 50));
        g->foe[i].yaw = a + PI;
        g->foe[i].alive = 1;
        g->foe[i].cool = 5.0f + (float)i * 1.2f;
        g->foe[i].hit = 0;
    }
    for (i = 0; i < MTN_N; i++) {
        float a = (float)i * (2.0f * PI / (float)MTN_N);
        g->mtn_x[i] = cosf(a) * 620.0f;
        g->mtn_z[i] = sinf(a) * 620.0f;
        g->mtn_s[i] = 70.0f + (float)((i * 17) % 50);
    }
}

static void *init(Canvas *c)
{
    Vector *g = canvas_calloc(1, sizeof(*g));
    int i;
    reset_world(g);
    for (i = 0; i < STAR_N; i++) {
        g->star[i][0] = (float)((i * 97) % 1000) / 1000.0f;
        g->star[i][1] = (float)((i * 53) % 420) / 1000.0f;
    }
    g->snd_shot = canvas_sound_tone(c, 880.0f, 70.0f, 0.4f);
    g->snd_hit = canvas_sound_tone(c, 220.0f, 140.0f, 0.45f);
    g->snd_boom = canvas_sound_noise(c, 280.0f, 0.5f);
    canvas_set_title(c, "Vector - tank range  arrows drive/turn  space fire");
    return g;
}

static void update(void *state, Canvas *c, float dt)
{
    Vector *g = state;
    float turn = 0, drive = 0;
    int i, j, left;

    if (canvas_key_pressed(c, KEY_ESCAPE) || canvas_key_pressed(c, KEY_Q))
        canvas_quit(c);
    if ((g->won || g->lives <= 0) && canvas_key_pressed(c, KEY_R))
        reset_world(g);
    if (g->won || g->lives <= 0 || !g->you.alive)
        return;

    if (canvas_key_down(c, KEY_LEFT) || canvas_key_down(c, KEY_A))
        turn -= 1.6f;
    if (canvas_key_down(c, KEY_RIGHT) || canvas_key_down(c, KEY_D))
        turn += 1.6f;
    if (canvas_key_down(c, KEY_UP) || canvas_key_down(c, KEY_W))
        drive += 90.0f;
    if (canvas_key_down(c, KEY_DOWN) || canvas_key_down(c, KEY_S))
        drive -= 55.0f;
    g->you.yaw = wrap_pi(g->you.yaw + turn * dt);
    g->you.x += sinf(g->you.yaw) * drive * dt;
    g->you.z += cosf(g->you.yaw) * drive * dt;
    g->you.x = clampf(g->you.x, -500.0f, 500.0f);
    g->you.z = clampf(g->you.z, -500.0f, 500.0f);
    g->you.cool -= dt;
    g->you.hit -= dt;
    if (g->you.cool < 0.0f && canvas_key_pressed(c, KEY_SPACE)) {
        fire(g, &g->you, 1);
        g->you.cool = 0.28f;
        canvas_sound_play(c, g->snd_shot, 0.7f);
    }

    for (i = 0; i < ENEMY_N; i++) {
        Tank *e = &g->foe[i];
        float want, d2;
        if (!e->alive)
            continue;
        e->cool -= dt;
        e->hit -= dt;
        d2 = dist2(e->x, e->z, g->you.x, g->you.z);
        want = atan2f(g->you.x - e->x, g->you.z - e->z);
        e->yaw = ang_to(e->yaw, want, 1.1f * dt);
        if (d2 > 80.0f * 80.0f) {
            e->x += sinf(e->yaw) * 36.0f * dt;
            e->z += cosf(e->yaw) * 36.0f * dt;
        }
        {
            float aim = wrap_pi(want - e->yaw);
            if (aim < 0.0f)
                aim = -aim;
            if (e->cool < 0.0f && d2 < 260.0f * 260.0f && aim < 0.18f) {
                fire(g, e, 0);
                e->cool = 2.4f + (float)(i % 3) * 0.4f;
            }
        }
    }

    for (i = 0; i < SHOT_N; i++) {
        Shot *s = &g->shot[i];
        float ox, oz;
        if (s->life <= 0.0f)
            continue;
        ox = s->x;
        oz = s->z;
        s->x += s->vx * dt;
        s->y += s->vy * dt;
        s->z += s->vz * dt;
        s->life -= dt;
        if (s->friendly) {
            for (j = 0; j < ENEMY_N; j++) {
                if (!g->foe[j].alive)
                    continue;
                /* Tank body is about 20 x 28; circle r=22 matches the wire box. */
                if (seg_hits(ox, oz, s->x, s->z, g->foe[j].x, g->foe[j].z, 22.0f)) {
                    g->foe[j].alive = 0;
                    g->foe[j].hit = 0.5f;
                    s->life = 0;
                    g->score += 100;
                    canvas_sound_play(c, g->snd_boom, 0.8f);
                }
            }
        } else if (seg_hits(ox, oz, s->x, s->z, g->you.x, g->you.z, 16.0f)) {
            s->life = 0;
            g->you.hit = 0.4f;
            g->lives--;
            canvas_sound_play(c, g->snd_hit, 0.8f);
            if (g->lives <= 0)
                g->you.alive = 0;
        }
    }

    left = 0;
    for (i = 0; i < ENEMY_N; i++)
        if (g->foe[i].alive)
            left++;
    if (left == 0)
        g->won = 1;
}

static void render(void *state, Canvas *c)
{
    Vector *g = state;
    float w = (float)canvas_width(c), h = (float)canvas_height(c);
    char hud[96];
    int i;
    float flash = g->you.hit > 0.0f ? 0.18f : 0.0f;

    canvas_clear(c, 0.02f + flash, 0.03f, 0.05f);
    canvas_begin_hud(c);

    for (i = 0; i < STAR_N; i++)
        canvas_draw_pixel(c, g->star[i][0] * w, g->star[i][1] * h * 0.45f, 0.75f, 0.82f, 0.9f, 1.0f);

    canvas_fill_rect(c, 0, h * 0.48f, w, h * 0.52f, 0.04f, 0.07f, 0.04f, 1.0f);
    canvas_draw_line(c, 0, h * 0.48f, w, h * 0.48f, 0.2f, 0.55f, 0.25f, 1.0f);

    for (i = 0; i < MTN_N; i++)
        draw_pyramid(c, g, g->mtn_x[i], g->mtn_z[i], g->mtn_s[i], 0.25f, 0.85f, 0.35f);

    for (i = 0; i < ENEMY_N; i++)
        draw_tank(c, g, &g->foe[i], 1.0f, 0.35f, 0.2f, g->foe[i].hit > 0.0f);

    for (i = 0; i < SHOT_N; i++) {
        Shot *s = &g->shot[i];
        Vec3 tip, tail;
        float tx, ty, tz, ux, uy, uz, w = (float)canvas_width(c), h = (float)canvas_height(c);
        if (s->life <= 0.0f)
            continue;
        tip = v3(s->x, s->y, s->z);
        tail = v3(s->x - s->vx * 0.14f, s->y, s->z - s->vz * 0.14f);
        if (project(g, w, h, tip, &tx, &ty, &tz)) {
            if (project(g, w, h, tail, &ux, &uy, &uz))
                canvas_draw_line(c, ux, uy, tx, ty, 1.0f, s->friendly ? 0.95f : 0.35f,
                                 s->friendly ? 0.25f : 0.15f, 1.0f);
            canvas_fill_triangle(c, tx, ty - 5, tx - 4, ty + 3, tx + 4, ty + 3, 1.0f,
                                 s->friendly ? 0.9f : 0.3f, 0.15f, 1.0f);
        }
    }

    /* Cockpit reticle and ground chevron. */
    canvas_draw_line(c, w * 0.5f - 14, h * 0.48f, w * 0.5f + 14, h * 0.48f, 0.4f, 1.0f, 0.45f, 0.9f);
    canvas_draw_line(c, w * 0.5f, h * 0.48f - 10, w * 0.5f, h * 0.48f + 10, 0.4f, 1.0f, 0.45f, 0.9f);
    canvas_stroke_triangle(c, w * 0.5f, h - 36, w * 0.5f - 18, h - 18, w * 0.5f + 18, h - 18, 0.3f,
                           0.9f, 0.4f, 0.8f);

    /* 360° radar, top-left. You sit in the middle; up on the disc is your gun. */
    {
        const float rx = 58.0f, ry = 64.0f, rr = 46.0f, scale = (rr - 5.0f) / 640.0f;
        const int segs = 28;
        int k, nearest = -1;
        float near_d2 = 1e12f, near_px = 0.0f;
        for (k = 0; k < segs; k++) {
            float a0 = (float)k * (2.0f * PI / (float)segs);
            float a1 = (float)(k + 1) * (2.0f * PI / (float)segs);
            float x0 = rx + cosf(a0) * rr, y0 = ry + sinf(a0) * rr;
            float x1 = rx + cosf(a1) * rr, y1 = ry + sinf(a1) * rr;
            canvas_fill_triangle(c, rx, ry, x0, y0, x1, y1, 0.02f, 0.09f, 0.04f, 0.34f);
            canvas_draw_line(c, x0, y0, x1, y1, 0.3f, 0.9f, 0.45f, 0.5f);
        }
        /* Forward view wedge (what the reticle sees). */
        canvas_fill_triangle(c, rx, ry, rx - 18, ry - rr + 2, rx + 18, ry - rr + 2, 0.2f, 0.7f, 0.3f,
                             0.28f);
        canvas_draw_line(c, rx, ry - rr + 1, rx, ry + rr - 1, 0.25f, 0.7f, 0.35f, 0.2f);
        canvas_draw_line(c, rx - rr + 1, ry, rx + rr - 1, ry, 0.25f, 0.7f, 0.35f, 0.2f);
        canvas_draw_text(c, rx - 3, ry - rr - 2, "F", 0.55f, 1.0f, 0.55f);
        canvas_draw_text(c, rx - rr - 8, ry + 4, "L", 0.7f, 0.85f, 0.4f);
        canvas_draw_text(c, rx + rr + 2, ry + 4, "R", 0.7f, 0.85f, 0.4f);
        for (i = 0; i < ENEMY_N; i++) {
            float dx = g->foe[i].x - g->you.x;
            float dz = g->foe[i].z - g->you.z;
            float px = (dx * cosf(g->you.yaw) - dz * sinf(g->you.yaw)) * scale;
            float pz = (dx * sinf(g->you.yaw) + dz * cosf(g->you.yaw)) * scale;
            float d = sqrtf(px * px + pz * pz), lim = rr - 6.0f, bx, by, d2;
            if (d > lim && d > 1e-3f) {
                px *= lim / d;
                pz *= lim / d;
            }
            bx = rx + px;
            by = ry - pz;
            if (g->foe[i].alive) {
                canvas_draw_line(c, rx, ry, bx, by, 1.0f, 0.35f, 0.15f, 0.35f);
                canvas_fill_triangle(c, bx, by - 4, bx - 3, by + 3, bx + 3, by + 3, 1.0f, 0.28f, 0.12f,
                                     0.95f);
                d2 = dist2(g->foe[i].x, g->foe[i].z, g->you.x, g->you.z);
                if (d2 < near_d2) {
                    near_d2 = d2;
                    nearest = i;
                    near_px = px;
                }
            } else
                canvas_stroke_triangle(c, bx, by - 3, bx - 2, by + 2, bx + 2, by + 2, 0.5f, 0.5f, 0.45f,
                                       0.45f);
        }
        /* You: always in the middle, nose toward F. */
        canvas_fill_triangle(c, rx, ry - 10, rx - 6, ry + 6, rx + 6, ry + 6, 0.35f, 1.0f, 0.45f, 0.95f);
        canvas_stroke_triangle(c, rx, ry - 10, rx - 6, ry + 6, rx + 6, ry + 6, 0.85f, 1.0f, 0.7f, 0.9f);
        if (nearest >= 0) {
            const char *hint = fabsf(near_px) < 4.0f ? "FIRE" : (near_px < 0.0f ? "TURN L" : "TURN R");
            canvas_draw_text(c, rx - 22, ry + rr + 14, hint, 0.95f, 0.95f, 0.4f);
        }
    }

    if (g->won)
        snprintf(hud, sizeof(hud), "RANGE CLEAR   score %d   press R", g->score);
    else if (g->lives <= 0)
        snprintf(hud, sizeof(hud), "DESTROYED   score %d   press R", g->score);
    else
        snprintf(hud, sizeof(hud), "VECTOR   score %d   lives %d   arrows drive   space fire", g->score,
                 g->lives);
    canvas_draw_text(c, 118, 22, hud, 0.45f, 1.0f, 0.5f);
    if (g->won || g->lives <= 0)
        canvas_draw_text(c, w * 0.5f - 80, h * 0.42f, g->won ? "RANGE CLEAR" : "DESTROYED", 1.0f, 0.95f,
                         0.4f);
    canvas_end_hud(c);
}

static void shutdown(void *state, Canvas *c)
{
    (void)c;
    canvas_free(state);
}

CANVAS_EXPORT const Game canvas_game = {
    .name = "Vector",
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
