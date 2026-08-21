#include "dungeon.h"
#include "rpg.h"

#include <math.h>
#include <string.h>

static int in_map(int x, int y)
{
    return x >= 0 && y >= 0 && x < DUN_W && y < DUN_H;
}

int dungeon_get(const Dungeon *d, int tx, int ty)
{
    if (!d || !in_map(tx, ty))
        return DUN_WALL;
    return d->tile[ty][tx];
}

int dungeon_walk(const Dungeon *d, int tx, int ty)
{
    int kind;
    const RpgTerrain *t;

    if (!d || !in_map(tx, ty))
        return 0;
    kind = d->tile[ty][tx];
    if (kind == DUN_WALL)
        return 0;
    t = rpg_terrain(kind);
    if (t)
        return (t->flags & RPG_TF_WALK) != 0;
    return kind != DUN_WALL;
}

int dungeon_opaque(const Dungeon *d, int tx, int ty)
{
    int kind;
    const RpgTerrain *t;

    if (!d || !in_map(tx, ty))
        return 1;
    kind = d->tile[ty][tx];
    if (kind == DUN_WALL)
        return 1;
    t = rpg_terrain(kind);
    if (t)
        return (t->flags & RPG_TF_BLOCK_LOS) != 0;
    return 0;
}

int dungeon_step_cost(const Dungeon *d, int tx, int ty)
{
    const RpgTerrain *t = rpg_terrain(dungeon_get(d, tx, ty));
    int c;

    if (!t || t->cost <= 0)
        return 1;
    c = t->cost;
    if (c > 64)
        c = 64;
    return c;
}

float dungeon_speed_at(const Dungeon *d, float x, float y)
{
    int tx, ty;
    const RpgTerrain *t;
    int pct;

    dungeon_pos_tile(x, y, &tx, &ty);
    t = rpg_terrain(dungeon_get(d, tx, ty));
    if (!t)
        return 1.0f;
    pct = t->speed_pct;
    if (pct <= 0)
        pct = 100;
    if (pct < 8)
        pct = 8;
    if (pct > 200)
        pct = 200;
    return (float)pct / 100.0f;
}

void dungeon_fill(Dungeon *d, unsigned char t)
{
    int x, y;

    for (y = 0; y < DUN_H; y++) {
        for (x = 0; x < DUN_W; x++)
            d->tile[y][x] = t;
    }
}

void dungeon_rect(Dungeon *d, int x, int y, int w, int h, unsigned char t)
{
    int i, j;

    for (j = y; j < y + h; j++) {
        for (i = x; i < x + w; i++) {
            if (in_map(i, j))
                d->tile[j][i] = t;
        }
    }
}

void dungeon_set(Dungeon *d, int tx, int ty, unsigned char t)
{
    if (in_map(tx, ty))
        d->tile[ty][tx] = t;
}

static void fill_rect(Dungeon *d, int x, int y, int w, int h, unsigned char t)
{
    dungeon_rect(d, x, y, w, h, t);
}

static int room_ok(const Dungeon *d, DunRoom r)
{
    int i, j, pad = 1;

    if (r.x < 2 || r.y < 2 || r.x + r.w >= DUN_W - 2 || r.y + r.h >= DUN_H - 2)
        return 0;
    for (j = r.y - pad; j < r.y + r.h + pad; j++) {
        for (i = r.x - pad; i < r.x + r.w + pad; i++) {
            if (!in_map(i, j))
                return 0;
            if (d->tile[j][i] == DUN_FLOOR)
                return 0;
        }
    }
    return 1;
}

static void carve_h(Dungeon *d, int x0, int x1, int y)
{
    int a = x0 < x1 ? x0 : x1;
    int b = x0 < x1 ? x1 : x0;
    int x;

    for (x = a; x <= b; x++) {
        if (in_map(x, y))
            d->tile[y][x] = DUN_FLOOR;
        if (in_map(x, y - 1) && rpg_rng(0, 4) == 0)
            d->tile[y - 1][x] = DUN_FLOOR;
    }
}

static void carve_v(Dungeon *d, int y0, int y1, int x)
{
    int a = y0 < y1 ? y0 : y1;
    int b = y0 < y1 ? y1 : y0;
    int y;

    for (y = a; y <= b; y++) {
        if (in_map(x, y))
            d->tile[y][x] = DUN_FLOOR;
        if (in_map(x + 1, y) && rpg_rng(0, 4) == 0)
            d->tile[y][x + 1] = DUN_FLOOR;
    }
}

static void connect_rooms(Dungeon *d, DunRoom a, DunRoom b)
{
    int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int bx = b.x + b.w / 2, by = b.y + b.h / 2;

    if (rpg_rng(0, 1)) {
        carve_h(d, ax, bx, ay);
        carve_v(d, ay, by, bx);
    } else {
        carve_v(d, ay, by, ax);
        carve_h(d, ax, bx, by);
    }
}

void dungeon_gen(Dungeon *d, unsigned seed)
{
    int tries, i;
    DunRoom r;

    rpg_seed(seed);
    memset(d, 0, sizeof(*d));
    d->room_n = 0;

    for (tries = 0; tries < 80 && d->room_n < DUN_ROOMS; tries++) {
        r.w = rpg_rng(5, 11);
        r.h = rpg_rng(5, 10);
        r.x = rpg_rng(2, DUN_W - r.w - 3);
        r.y = rpg_rng(2, DUN_H - r.h - 3);
        if (!room_ok(d, r))
            continue;
        fill_rect(d, r.x, r.y, r.w, r.h, DUN_FLOOR);
        d->rooms[d->room_n++] = r;
    }
    if (d->room_n < 2) {
        r.x = 8;
        r.y = 8;
        r.w = 10;
        r.h = 8;
        fill_rect(d, r.x, r.y, r.w, r.h, DUN_FLOOR);
        d->rooms[d->room_n++] = r;
        r.x = 28;
        r.y = 20;
        fill_rect(d, r.x, r.y, r.w, r.h, DUN_FLOOR);
        d->rooms[d->room_n++] = r;
    }
    for (i = 1; i < d->room_n; i++)
        connect_rooms(d, d->rooms[i - 1], d->rooms[i]);
    if (d->room_n > 3)
        connect_rooms(d, d->rooms[0], d->rooms[d->room_n - 1]);

    d->start_tx = d->rooms[0].x + d->rooms[0].w / 2;
    d->start_ty = d->rooms[0].y + d->rooms[0].h / 2;
    d->stair_tx = d->rooms[d->room_n - 1].x + d->rooms[d->room_n - 1].w / 2;
    d->stair_ty = d->rooms[d->room_n - 1].y + d->rooms[d->room_n - 1].h / 2;
}

void dungeon_pos_tile(float x, float y, int *tx, int *ty)
{
    if (tx)
        *tx = (int)floorf(x / DUN_TILE);
    if (ty)
        *ty = (int)floorf(y / DUN_TILE);
}

void dungeon_tile_pos(int tx, int ty, float *x, float *y)
{
    if (x)
        *x = (float)tx * DUN_TILE + DUN_TILE * 0.5f;
    if (y)
        *y = (float)ty * DUN_TILE + DUN_TILE * 0.5f;
}

static int blocked_xy(const Dungeon *d, float x, float y)
{
    int tx, ty;

    dungeon_pos_tile(x, y, &tx, &ty);
    return !dungeon_walk(d, tx, ty);
}

int dungeon_blocked(const Dungeon *d, float x, float y, float rad)
{
    return blocked_xy(d, x - rad, y - rad) || blocked_xy(d, x + rad, y - rad) ||
           blocked_xy(d, x - rad, y + rad) || blocked_xy(d, x + rad, y + rad);
}

void dungeon_slide(const Dungeon *d, float *x, float *y, float dx, float dy, float rad)
{
    float nx = *x + dx, ny = *y + dy;

    if (!dungeon_blocked(d, nx, ny, rad)) {
        *x = nx;
        *y = ny;
        return;
    }
    if (!dungeon_blocked(d, nx, *y, rad)) {
        *x = nx;
        return;
    }
    if (!dungeon_blocked(d, *x, ny, rad))
        *y = ny;
}

int dungeon_line_clear(const Dungeon *d, float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    float dist = sqrtf(dx * dx + dy * dy);
    int steps, i, tx, ty;

    if (dist < 1.0f)
        return 1;
    steps = (int)(dist / 8.0f) + 1;
    for (i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        dungeon_pos_tile(x0 + dx * t, y0 + dy * t, &tx, &ty);
        if (dungeon_opaque(d, tx, ty))
            return 0;
    }
    return 1;
}

int dungeon_random_floor(const Dungeon *d, int *tx, int *ty)
{
    int i;

    for (i = 0; i < 40; i++) {
        *tx = rpg_rng(1, DUN_W - 2);
        *ty = rpg_rng(1, DUN_H - 2);
        if (dungeon_walk(d, *tx, *ty))
            return 1;
    }
    return dungeon_room_floor(d, rpg_rng(0, d->room_n - 1), tx, ty);
}

int dungeon_room_floor(const Dungeon *d, int room, int *tx, int *ty)
{
    DunRoom r;

    if (d->room_n <= 0)
        return 0;
    if (room < 0)
        room = 0;
    if (room >= d->room_n)
        room = d->room_n - 1;
    r = d->rooms[room];
    *tx = r.x + rpg_rng(1, r.w - 2);
    *ty = r.y + rpg_rng(1, r.h - 2);
    if (!dungeon_walk(d, *tx, *ty)) {
        *tx = r.x + r.w / 2;
        *ty = r.y + r.h / 2;
    }
    return 1;
}
