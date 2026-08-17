#include "canvas.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLS 50
#define ROWS 15
#define TILE 32.0f
#define MOB_MAX 12
#define POP_MAX 8
#define HUD 28.0f

#define GRAV_UP 1650.0f
#define GRAV_DOWN 2600.0f
#define JUMP_V (-620.0f)
#define JUMP_CUT 0.42f
#define WALK_ACC 1700.0f
#define AIR_ACC 1100.0f
#define FRICTION 2200.0f
#define MAX_WALK 195.0f
#define MAX_RUN 305.0f
#define STOMP_V (-340.0f)

static const char *LEVEL[] = {
    "                                                  ",
    "                                                  ",
    "                                                  ",
    "            ?  ?                                  ",
    "                                                  ",
    "         o  o  o                 ===              ",
    "                                     g            ",
    "                   ===                            ",
    "       ?    B    ?         o o            g       ",
    "                                                  ",
    " P                    ##        g    #       F    ",
    "##############    ##########    ### ##############",
    "##############    ##########    ### ##############",
    "##############    ##########    ### ##############",
    "##############    ##########    ### ##############",
};

typedef struct {
    float x, y, vx, vy;
    int face, grounded, jump_held;
    float coyote, buffer;
} Hero;

typedef struct {
    float x, y, vx, vy, sx, sy;
    int alive, flat;
    float flat_t;
} Mob;

typedef struct {
    float x, y, t;
    int on;
} Pop;

typedef struct {
    char tiles[ROWS][COLS + 1];
    Hero hero;
    Mob mobs[MOB_MAX];
    Pop pops[POP_MAX];
    int mob_n, score, coins, lives, won, dead;
    float flash, spawn_x, spawn_y, cam_look;
    unsigned tex_ground, tex_q, tex_used, tex_brick, tex_coin;
    unsigned tex_hero, tex_mob, tex_pipe, tex_flag;
    unsigned snd_jump, snd_coin, snd_bump, snd_stomp, snd_die, snd_win;
    Canvas *cv;
} Plat;

static int solid_tile(char t)
{
    return t == '#' || t == '?' || t == 'B' || t == 'D' || t == '=' || t == 'T';
}

static char tile_at(const Plat *p, int c, int r)
{
    if (r < 0)
        return ' ';
    if (r >= ROWS)
        return ' ';
    if (c < 0 || c >= COLS)
        return '#';
    return p->tiles[r][c];
}

static void put_px(unsigned char *p, int w, int h, int x, int y, int r, int g, int b, int a)
{
    if (x < 0 || y < 0 || x >= w || y >= h)
        return;
    int i = (y * w + x) * 4;
    p[i] = (unsigned char)r;
    p[i + 1] = (unsigned char)g;
    p[i + 2] = (unsigned char)b;
    p[i + 3] = (unsigned char)a;
}

static unsigned make_noise_tile(Canvas *c, int r0, int g0, int b0, int nmul)
{
    unsigned char px[16 * 16 * 4];
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int n = ((x * 13 + y * 7) & 7) * nmul;
            int edge = (x == 0 || y == 0 || x == 15 || y == 15);
            put_px(px, 16, 16, x, y, r0 + n - (edge ? 25 : 0), g0 + n / 2 - (edge ? 20 : 0),
                   b0 + n / 3 - (edge ? 15 : 0), 255);
        }
    }
    return canvas_texture_rgba(c, 16, 16, px);
}

static unsigned make_q_tile(Canvas *c, int used)
{
    unsigned char px[16 * 16 * 4];
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int edge = (x < 2 || y < 2 || x > 13 || y > 13);
            if (used)
                put_px(px, 16, 16, x, y, edge ? 90 : 140, edge ? 70 : 110, edge ? 40 : 60, 255);
            else {
                int q = (x >= 6 && x <= 9 && y >= 4 && y <= 10 && !(x > 8 && y > 7 && y < 9));
                if (y >= 12 && y <= 13 && x >= 7 && x <= 8)
                    q = 1;
                put_px(px, 16, 16, x, y, q ? 255 : (edge ? 160 : 230), q ? 230 : (edge ? 110 : 180),
                       q ? 40 : (edge ? 20 : 40), 255);
            }
        }
    }
    return canvas_texture_rgba(c, 16, 16, px);
}

static unsigned make_coin(Canvas *c)
{
    unsigned char px[16 * 16 * 4];
    int x, y;
    memset(px, 0, sizeof(px));
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            float dx = (x - 7.5f) / 3.2f, dy = (y - 7.5f) / 5.5f;
            if (dx * dx + dy * dy < 1.0f)
                put_px(px, 16, 16, x, y, 255, 210, 50, 255);
        }
    }
    return canvas_texture_rgba(c, 16, 16, px);
}

static unsigned make_hero(Canvas *c)
{
    const int fw = 16, fh = 24, frames = 4;
    unsigned char *px = calloc((size_t)fw * frames * fh, 4);
    int f, x, y;
    for (f = 0; f < frames; f++) {
        int ox = f * fw;
        int leg = (f == 1) ? -2 : (f == 3) ? 2 : 0;
        for (y = 2; y <= 6; y++)
            for (x = 5; x <= 11; x++)
                put_px(px, fw * frames, fh, ox + x, y, 230, 40, 40, 255);
        for (x = 4; x <= 12; x++)
            put_px(px, fw * frames, fh, ox + x, 2, 230, 40, 40, 255);
        for (y = 7; y <= 11; y++)
            for (x = 5; x <= 10; x++)
                put_px(px, fw * frames, fh, ox + x, y, 240, 200, 160, 255);
        for (y = 12; y <= 18; y++)
            for (x = 4; x <= 11; x++)
                put_px(px, fw * frames, fh, ox + x, y, 30, 70, 200, 255);
        for (y = 19; y <= 23; y++) {
            put_px(px, fw * frames, fh, ox + 6 + leg, y, 90, 50, 20, 255);
            put_px(px, fw * frames, fh, ox + 9 - leg, y, 90, 50, 20, 255);
        }
    }
    {
        unsigned t = canvas_texture_rgba(c, fw * frames, fh, px);
        free(px);
        return t;
    }
}

static unsigned make_mob(Canvas *c)
{
    const int fw = 16, frames = 2;
    unsigned char *px = calloc((size_t)fw * frames * 16, 4);
    int f, x, y;
    for (f = 0; f < frames; f++) {
        int ox = f * fw, foot = f ? 1 : -1;
        for (y = 4; y <= 13; y++) {
            for (x = 2; x <= 13; x++) {
                float dx = x - 7.5f, dy = y - 8.5f;
                if (dx * dx + dy * dy < 36.0f)
                    put_px(px, fw * frames, 16, ox + x, y, 150, 90, 40, 255);
            }
        }
        put_px(px, fw * frames, 16, ox + 5, 8, 20, 20, 20, 255);
        put_px(px, fw * frames, 16, ox + 10, 8, 20, 20, 20, 255);
        for (x = 3; x <= 6; x++)
            put_px(px, fw * frames, 16, ox + x + foot, 14, 40, 30, 20, 255);
        for (x = 9; x <= 12; x++)
            put_px(px, fw * frames, 16, ox + x - foot, 14, 40, 30, 20, 255);
    }
    {
        unsigned t = canvas_texture_rgba(c, fw * frames, 16, px);
        free(px);
        return t;
    }
}

static unsigned make_pipe(Canvas *c)
{
    unsigned char px[16 * 16 * 4];
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int lip = y < 4;
            int r = lip ? 40 : 20, g = lip ? 190 : 150, b = lip ? 50 : 40;
            if (x < 2 || x > 13) {
                r -= 15;
                g -= 30;
            }
            put_px(px, 16, 16, x, y, r, g, b, 255);
        }
    }
    return canvas_texture_rgba(c, 16, 16, px);
}

static unsigned make_flag(Canvas *c)
{
    unsigned char px[16 * 16 * 4];
    int x, y;
    memset(px, 0, sizeof(px));
    for (y = 0; y < 16; y++)
        put_px(px, 16, 16, 3, y, 40, 180, 70, 255);
    for (y = 1; y <= 7; y++)
        for (x = 4; x <= 4 + (7 - y); x++)
            put_px(px, 16, 16, x, y, 230, 40, 50, 255);
    return canvas_texture_rgba(c, 16, 16, px);
}

static void add_pop(Plat *p, float x, float y)
{
    int i;
    for (i = 0; i < POP_MAX; i++) {
        if (!p->pops[i].on) {
            p->pops[i].x = x;
            p->pops[i].y = y;
            p->pops[i].t = 0.45f;
            p->pops[i].on = 1;
            return;
        }
    }
}

static void spawn_mobs(Plat *p)
{
    int r, col;
    p->mob_n = 0;
    for (r = 0; r < ROWS && p->mob_n < MOB_MAX; r++) {
        for (col = 0; col < COLS && p->mob_n < MOB_MAX; col++) {
            if (p->tiles[r][col] == 'g') {
                Mob *m = &p->mobs[p->mob_n++];
                m->sx = m->x = (col + 0.5f) * TILE;
                m->sy = m->y = (r + 1) * TILE;
                m->vx = -55.0f;
                m->vy = 0;
                m->alive = 1;
                m->flat = 0;
                m->flat_t = 0;
                p->tiles[r][col] = ' ';
            }
        }
    }
}

static void find_spawn(Plat *p)
{
    int r, col;
    p->spawn_x = 2.5f * TILE;
    p->spawn_y = 11.0f * TILE;
    for (r = 0; r < ROWS; r++) {
        for (col = 0; col < COLS; col++) {
            if (p->tiles[r][col] == 'P') {
                p->spawn_x = (col + 0.5f) * TILE;
                p->spawn_y = (r + 1) * TILE;
                p->tiles[r][col] = ' ';
                return;
            }
        }
    }
}

static void reset_mobs(Plat *p)
{
    int i;
    for (i = 0; i < p->mob_n; i++) {
        Mob *m = &p->mobs[i];
        m->x = m->sx;
        m->y = m->sy;
        m->vx = -55.0f;
        m->vy = 0;
        m->alive = 1;
        m->flat = 0;
        m->flat_t = 0;
    }
}

static void reset_hero(Plat *p)
{
    p->hero.x = p->spawn_x;
    p->hero.y = p->spawn_y;
    p->hero.vx = p->hero.vy = 0;
    p->hero.face = 1;
    p->hero.grounded = 0;
    p->hero.jump_held = 0;
    p->hero.coyote = 0;
    p->hero.buffer = 0;
    p->cam_look = 64.0f;
    p->dead = 0;
    p->flash = 0;
}

static void load_level(Plat *p)
{
    int i;
    for (i = 0; i < ROWS; i++)
        memcpy(p->tiles[i], LEVEL[i], COLS);
    find_spawn(p);
    spawn_mobs(p);
}

static void *init(Canvas *c)
{
    Plat *p = calloc(1, sizeof(*p));
    p->tex_ground = make_noise_tile(c, 180, 90, 40, 3);
    p->tex_brick = make_noise_tile(c, 190, 80, 50, 2);
    p->tex_q = make_q_tile(c, 0);
    p->tex_used = make_q_tile(c, 1);
    p->tex_coin = make_coin(c);
    p->tex_hero = make_hero(c);
    p->tex_mob = make_mob(c);
    p->tex_pipe = make_pipe(c);
    p->tex_flag = make_flag(c);
    p->cv = c;
    p->lives = 3;
    p->snd_jump = canvas_sound_tone(c, 420.0f, 90.0f, 0.4f);
    p->snd_coin = canvas_sound_tone(c, 980.0f, 110.0f, 0.4f);
    p->snd_bump = canvas_sound_noise(c, 50.0f, 0.35f);
    p->snd_stomp = canvas_sound_tone(c, 220.0f, 80.0f, 0.45f);
    p->snd_die = canvas_sound_noise(c, 380.0f, 0.5f);
    p->snd_win = canvas_sound_tone(c, 660.0f, 350.0f, 0.45f);
    load_level(p);
    reset_hero(p);
    canvas_cam_bounds(c, 0, 0, COLS * TILE, ROWS * TILE);
    canvas_cam_set(c, 0, 0);
    canvas_set_title(c, "Plat - arrows/WASD run, space/up jump, shift run");
    return p;
}

static int overlap_solid(const Plat *p, float x, float y, float hw, float hh, int *hc, int *hr)
{
    int c0 = (int)((x - hw) / TILE), c1 = (int)((x + hw - 0.01f) / TILE);
    int r0 = (int)((y - hh) / TILE), r1 = (int)((y - 0.01f) / TILE);
    int c, r;
    if (hc)
        *hc = -1;
    if (hr)
        *hr = -1;
    for (r = r0; r <= r1; r++) {
        for (c = c0; c <= c1; c++) {
            if (solid_tile(tile_at(p, c, r))) {
                if (hc)
                    *hc = c;
                if (hr)
                    *hr = r;
                return 1;
            }
        }
    }
    return 0;
}

static void bump_block(Plat *p, int c, int r)
{
    char t;
    if (c < 0 || r < 0 || c >= COLS || r >= ROWS)
        return;
    t = p->tiles[r][c];
    if (t == '?') {
        p->tiles[r][c] = 'D';
        p->coins++;
        p->score += 200;
        add_pop(p, (c + 0.5f) * TILE, r * TILE);
        if (p->cv)
            canvas_sound_play(p->cv, p->snd_coin, 0.7f);
    } else if (t == 'B') {
        p->tiles[r][c] = ' ';
        p->score += 50;
        if (p->cv)
            canvas_sound_play(p->cv, p->snd_bump, 0.55f);
    }
}

static void move_actor(Plat *p, float *x, float *y, float *vx, float *vy, float hw, float hh,
                      float dt, int bump)
{
    int c, r;
    float dx = *vx * dt;
    float dy = *vy * dt;

    *x += dx;
    /* Inset the feet so the floor is not treated as a side wall. */
    if (overlap_solid(p, *x, *y - 2.0f, hw, hh - 2.0f, &c, &r)) {
        if (dx > 0.0f)
            *x = c * TILE - hw;
        else if (dx < 0.0f)
            *x = (c + 1) * TILE + hw;
        *vx = 0;
    }
    *y += dy;
    if (overlap_solid(p, *x, *y, hw, hh, &c, &r)) {
        if (dy > 0.0f) {
            *y = r * TILE;
            *vy = 0;
        } else if (dy < 0.0f) {
            *y = (r + 1) * TILE + hh;
            if (bump)
                bump_block(p, c, r);
            *vy = 40.0f;
        }
    }
}

static int on_ground(const Plat *p, float x, float y, float hw)
{
    return overlap_solid(p, x, y + 2.0f, hw, 4.0f, NULL, NULL);
}

static void try_jump(Plat *p)
{
    Hero *h = &p->hero;
    if ((h->grounded || h->coyote > 0) && h->buffer > 0) {
        h->vy = JUMP_V;
        h->grounded = 0;
        h->coyote = 0;
        h->buffer = 0;
        h->jump_held = 1;
        if (p->cv)
            canvas_sound_play(p->cv, p->snd_jump, 0.55f);
    }
}

static void update_hero(Plat *p, Canvas *c, float dt)
{
    Hero *h = &p->hero;
    float acc, maxv, ax = 0;
    int run = canvas_key_down(c, KEY_SHIFT);
    int left = canvas_key_down(c, KEY_LEFT) || canvas_key_down(c, KEY_A);
    int right = canvas_key_down(c, KEY_RIGHT) || canvas_key_down(c, KEY_D);
    int jump = canvas_key_down(c, KEY_SPACE) || canvas_key_down(c, KEY_UP) || canvas_key_down(c, KEY_W);

    if (canvas_key_pressed(c, KEY_SPACE) || canvas_key_pressed(c, KEY_UP) || canvas_key_pressed(c, KEY_W))
        h->buffer = 0.12f;

    if (left)
        ax -= 1;
    if (right)
        ax += 1;
    if (ax > 0)
        h->face = 1;
    if (ax < 0)
        h->face = -1;

    acc = h->grounded ? WALK_ACC : AIR_ACC;
    maxv = run ? MAX_RUN : MAX_WALK;
    if (ax != 0) {
        h->vx += ax * acc * dt;
        if (h->vx > maxv)
            h->vx = maxv;
        if (h->vx < -maxv)
            h->vx = -maxv;
    } else if (h->grounded) {
        if (h->vx > 0) {
            h->vx -= FRICTION * dt;
            if (h->vx < 0)
                h->vx = 0;
        } else if (h->vx < 0) {
            h->vx += FRICTION * dt;
            if (h->vx > 0)
                h->vx = 0;
        }
    }

    if (!jump && h->jump_held && h->vy < 0)
        h->vy *= JUMP_CUT;
    if (!jump)
        h->jump_held = 0;

    h->vy += ((h->vy < 0 && h->jump_held) ? GRAV_UP : GRAV_DOWN) * dt;
    if (h->vy > 900.0f)
        h->vy = 900.0f;

    move_actor(p, &h->x, &h->y, &h->vx, &h->vy, 10.0f, 28.0f, dt, 1);
    h->grounded = on_ground(p, h->x, h->y, 10.0f);
    h->coyote = h->grounded ? 0.09f : h->coyote - dt;
    h->buffer -= dt;
    if (h->buffer < 0)
        h->buffer = 0;
    try_jump(p);

    if (h->x < 12.0f)
        h->x = 12.0f;
    if (h->x > COLS * TILE - 12.0f)
        h->x = COLS * TILE - 12.0f;
}

static void update_mobs(Plat *p, float dt)
{
    int i;
    for (i = 0; i < p->mob_n; i++) {
        Mob *m = &p->mobs[i];
        if (!m->alive)
            continue;
        if (m->flat) {
            m->flat_t -= dt;
            if (m->flat_t <= 0)
                m->alive = 0;
            continue;
        }
        {
            float old_x = m->x;
            m->vy += GRAV_DOWN * dt;
            if (m->vy > 700.0f)
                m->vy = 700.0f;
            move_actor(p, &m->x, &m->y, &m->vx, &m->vy, 10.0f, 16.0f, dt, 0);
            if (fabsf(m->x - old_x) < 0.01f)
                m->vx = -m->vx;
        }
        if (m->y > ROWS * TILE + 40.0f)
            m->alive = 0;
    }
}

static void collide_hero_mobs(Plat *p)
{
    Hero *h = &p->hero;
    int i;
    for (i = 0; i < p->mob_n; i++) {
        Mob *m = &p->mobs[i];
        float dx, dy;
        if (!m->alive || m->flat)
            continue;
        dx = h->x - m->x;
        dy = h->y - m->y;
        if (fabsf(dx) > 16.0f || dy < -28.0f || dy > 8.0f)
            continue;
        if (h->vy > 40.0f && dy < 2.0f) {
            m->flat = 1;
            m->flat_t = 0.35f;
            h->vy = STOMP_V;
            h->jump_held = 1;
            p->score += 100;
            if (p->cv)
                canvas_sound_play(p->cv, p->snd_stomp, 0.7f);
        } else {
            p->dead = 1;
            p->flash = 0;
            if (p->cv)
                canvas_sound_play(p->cv, p->snd_die, 0.8f);
        }
    }
}

static void collect_coins(Plat *p)
{
    int c = (int)(p->hero.x / TILE);
    int r = (int)((p->hero.y - 14.0f) / TILE);
    int dc, dr;
    for (dr = -1; dr <= 0; dr++) {
        for (dc = -1; dc <= 1; dc++) {
            int cc = c + dc, rr = r + dr;
            if (cc < 0 || rr < 0 || cc >= COLS || rr >= ROWS)
                continue;
            if (p->tiles[rr][cc] == 'o') {
                p->tiles[rr][cc] = ' ';
                p->coins++;
                p->score += 100;
                add_pop(p, (cc + 0.5f) * TILE, rr * TILE);
                if (p->cv)
                    canvas_sound_play(p->cv, p->snd_coin, 0.65f);
            }
            if (p->tiles[rr][cc] == 'F' && !p->won) {
                p->won = 1;
                if (p->cv)
                    canvas_sound_play(p->cv, p->snd_win, 0.8f);
            }
        }
    }
}

static void restart(Plat *p)
{
    p->score = 0;
    p->coins = 0;
    p->lives = 3;
    p->won = 0;
    load_level(p);
    reset_hero(p);
}

static void update(void *state, Canvas *c, float dt)
{
    Plat *p = state;
    float vw, want, camx, camy, vh;
    int i;

    if (dt > 0.05f)
        dt = 0.05f;
    if (canvas_key_pressed(c, KEY_ESCAPE) || canvas_key_pressed(c, KEY_Q))
        canvas_quit(c);
    if ((p->won || p->lives <= 0) && canvas_key_pressed(c, KEY_R))
        restart(p);

    vw = (float)canvas_width(c);
    vh = (float)canvas_height(c) - HUD;
    canvas_cam_zoom(c, 1.0f);
    canvas_cam_bounds(c, 0, 0, COLS * TILE, ROWS * TILE);

    if (p->won || p->lives <= 0)
        goto cam;
    if (p->dead) {
        p->flash += dt;
        if (p->flash > 1.0f) {
            p->lives--;
            if (p->lives > 0) {
                reset_mobs(p);
                reset_hero(p);
            }
        }
        goto cam;
    }

    update_hero(p, c, dt);
    update_mobs(p, dt);
    collide_hero_mobs(p);
    collect_coins(p);
    if (p->hero.y > ROWS * TILE + 20.0f) {
        p->dead = 1;
        p->flash = 0;
        canvas_sound_play(c, p->snd_die, 0.8f);
    }

    for (i = 0; i < POP_MAX; i++) {
        if (!p->pops[i].on)
            continue;
        p->pops[i].t -= dt;
        p->pops[i].y -= 70.0f * dt;
        if (p->pops[i].t <= 0)
            p->pops[i].on = 0;
    }

cam:
    want = p->hero.face > 0 ? 56.0f : -28.0f;
    p->cam_look += (want - p->cam_look) * (1.0f - expf(-8.0f * dt));
    camx = p->hero.x + p->cam_look - vw * 0.42f;
    camy = ROWS * TILE - vh;
    if (camy < 0)
        camy = 0;
    canvas_cam_set(c, camx, camy);
}

static unsigned tile_tex(const Plat *p, char t)
{
    if (t == '#')
        return p->tex_ground;
    if (t == '?')
        return p->tex_q;
    if (t == 'D')
        return p->tex_used;
    if (t == 'B' || t == '=')
        return p->tex_brick;
    if (t == 'T')
        return p->tex_pipe;
    return 0;
}

static void render(void *state, Canvas *c)
{
    Plat *p = state;
    Sprite spr;
    float vx, vy, vw, vh;
    int x0, y0, x1, y1, tx, ty, i, frame;
    char hud[96];

    canvas_clear(c, 0.36f, 0.58f, 0.95f);
    canvas_view(c, &vx, &vy, &vw, &vh);
    x0 = (int)(vx / TILE) - 1;
    y0 = (int)(vy / TILE) - 1;
    x1 = (int)((vx + vw) / TILE) + 1;
    y1 = (int)((vy + vh) / TILE) + 1;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= COLS)
        x1 = COLS - 1;
    if (y1 >= ROWS)
        y1 = ROWS - 1;

    for (i = 0; i < 6; i++) {
        float cx = 80.0f + (float)i * 280.0f;
        canvas_fill_rect(c, cx, 36.0f + (float)(i % 3) * 10.0f, 70, 18, 1, 1, 1, 0.85f);
        canvas_fill_rect(c, cx + 24, 24.0f + (float)(i % 3) * 10.0f, 50, 18, 1, 1, 1, 0.85f);
    }

    for (ty = y0; ty <= y1; ty++) {
        for (tx = x0; tx <= x1; tx++) {
            char t = p->tiles[ty][tx];
            unsigned tex = tile_tex(p, t);
            float x = tx * TILE, y = ty * TILE;
            if (tex)
                canvas_blit(c, tex, x, y, TILE, TILE, 0, 0, 1, 1, 1, 1, 1, 1);
            else if (t == 'o')
                canvas_blit(c, p->tex_coin, x + 6, y + 6, 20, 20, 0, 0, 1, 1, 1, 1, 1, 1);
            else if (t == 'F') {
                canvas_fill_rect(c, x + 12, y - TILE * 3, 4, TILE * 4, 0.2f, 0.75f, 0.3f, 1);
                canvas_blit(c, p->tex_flag, x + 8, y - TILE * 3, 28, 28, 0, 0, 1, 1, 1, 1, 1, 1);
            }
        }
    }

    for (i = 0; i < p->mob_n; i++) {
        Mob *m = &p->mobs[i];
        if (!m->alive)
            continue;
        sprite_init(&spr, m->x, m->y, m->flat ? 28.0f : 24.0f, m->flat ? 10.0f : 24.0f, p->tex_mob);
        spr.origin_x = 0.5f;
        spr.origin_y = 1.0f;
        spr.frames = 2;
        spr.frame_w = 16;
        spr.frame = m->flat ? 0 : ((int)(canvas_time(c) * 8) & 1);
        canvas_draw_sprite(c, &spr);
    }

    for (i = 0; i < POP_MAX; i++) {
        if (!p->pops[i].on)
            continue;
        canvas_blit(c, p->tex_coin, p->pops[i].x - 8, p->pops[i].y - 8, 16, 16, 0, 0, 1, 1, 1, 1, 1, 1);
    }

    frame = p->hero.grounded ? ((int)(fabsf(p->hero.vx) * 0.08f + canvas_time(c) * 10) & 3) : 1;
    if (p->hero.grounded && fabsf(p->hero.vx) < 20.0f)
        frame = 0;
    sprite_init(&spr, p->hero.x, p->hero.y, 26, 38, p->tex_hero);
    spr.origin_x = 0.5f;
    spr.origin_y = 1.0f;
    spr.frames = 4;
    spr.frame_w = 16;
    spr.frame_h = 24;
    spr.frame = frame;
    spr.flip_x = p->hero.face < 0;
    spr.visible = !p->dead || ((int)(p->flash * 8) & 1);
    canvas_draw_sprite(c, &spr);

    canvas_begin_hud(c);
    canvas_fill_rect(c, 0, 0, (float)canvas_width(c), HUD, 0, 0, 0, 0.55f);
    if (p->won)
        snprintf(hud, sizeof(hud), "FLAG!  score %d  coins %d   R to restart", p->score, p->coins);
    else if (p->lives <= 0)
        snprintf(hud, sizeof(hud), "GAME OVER  score %d   R to restart", p->score);
    else
        snprintf(hud, sizeof(hud), "PLAT   score %d   coins %d   lives %d", p->score, p->coins, p->lives);
    canvas_draw_text(c, 10, 20, hud, 1, 0.92f, 0.35f);
    canvas_end_hud(c);
}

static void shutdown(void *state, Canvas *c)
{
    (void)c;
    free(state);
}

static const Game game = {
    .name = "Plat",
    .width = 800,
    .height = 508,
    .init = init,
    .update = update,
    .render = render,
    .shutdown = shutdown,
};

int main(void)
{
    return canvas_run(&game);
}
