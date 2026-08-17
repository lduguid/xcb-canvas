#include "canvas.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLS 28
#define ROWS 31
#define TILE 20.0f
#define GHOST_N 4
#define HUD 40.0f

static const char *LEVEL[] = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "     #.##### ## #####.#     ",
    "     #.##          ##.#     ",
    "     #.## ###--### ##.#     ",
    "######.## #      # ##.######",
    "      .   #      #   .      ",
    "######.## #      # ##.######",
    "     #.## ######## ##.#     ",
    "     #.##          ##.#     ",
    "     #.## ######## ##.#     ",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#o..##................##..o#",
    "###.##.##.########.##.##.###",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#.##########.##.##########.#",
    "#..........................#",
    "############################",
};

static const int DX[4] = {1, 0, -1, 0};
static const int DY[4] = {0, 1, 0, -1};

typedef struct {
    float x, y;
    int dir, next;
    int alive;
} Actor;

typedef struct {
    Actor a;
    int id;
    int eaten;
    float wait;
} Ghost;

typedef struct {
    char tiles[ROWS][COLS + 1];
    Actor pac;
    Ghost ghosts[GHOST_N];
    int score, lives, pellets;
    int won, dead, scatter, mode_i;
    float fright, mouth, flash, mode_t;
    Sheet pac_sheet, ghost_sheet;
    unsigned snd_waka[2], snd_power, snd_eat, snd_die, snd_win;
    int waka;
} Pacman;

/* Arcade direction tie-break: up, left, down, right. */
static const int DIR_PRI[4] = {3, 2, 1, 0};

/* Scatter corners: Blinky NE, Pinky NW, Inky SE, Clyde SW. */
static const int SCATTER[GHOST_N][2] = {
    {25, 0},
    {2, 0},
    {27, 30},
    {0, 30},
};

/* First-level scatter/chase wave lengths (seconds). */
static const float MODE_SECS[] = {7, 20, 7, 20, 5, 20, 5, 1e9f};
static const int MODE_SCATTER[] = {1, 0, 1, 0, 1, 0, 1, 0};

static int wrap_c(int c)
{
    if (c < 0)
        return COLS - 1;
    if (c >= COLS)
        return 0;
    return c;
}

static char tile_at(const Pacman *p, int c, int r)
{
    if (r < 0 || r >= ROWS)
        return '#';
    return p->tiles[r][wrap_c(c)];
}

static int wall(const Pacman *p, int c, int r)
{
    char t = tile_at(p, c, r);
    return t == '#' || t == '-';
}

static void cell_of(float x, float y, int *c, int *r)
{
    *c = (int)(x / TILE);
    *r = (int)(y / TILE);
}

static int in_house(int c, int r)
{
    return r >= 12 && r <= 15 && c >= 11 && c <= 16;
}

static int ghost_door_ok(const Ghost *g)
{
    int c, r;
    if (g->eaten)
        return 1;
    cell_of(g->a.x, g->a.y, &c, &r);
    return in_house(c, r);
}

static int ghost_blocked(const Pacman *p, const Ghost *g, int c, int r)
{
    char t = tile_at(p, c, r);
    if (t == '#')
        return 1;
    if (t == '-')
        return !ghost_door_ok(g);
    return 0;
}

static int near_center(float x, float y)
{
    float fx = fmodf(x, TILE);
    float fy = fmodf(y, TILE);
    if (fx < 0)
        fx += TILE;
    if (fy < 0)
        fy += TILE;
    return fabsf(fx - TILE * 0.5f) < 2.2f && fabsf(fy - TILE * 0.5f) < 2.2f;
}

static void snap_center(Actor *a)
{
    int c, r;
    cell_of(a->x, a->y, &c, &r);
    a->x = (c + 0.5f) * TILE;
    a->y = (r + 0.5f) * TILE;
}

static void load_or_solid(Canvas *c, Sheet *s, const char *path, int cw, int ch)
{
    if (!canvas_sheet_load(c, s, path, cw, ch)) {
        unsigned t = canvas_texture_solid(c, 1.0f, 0.85f, 0.0f);
        canvas_sheet_init(s, t, cw, ch, cw, ch);
    }
}

static void count_pellets(Pacman *p)
{
    int r, c;
    p->pellets = 0;
    for (r = 0; r < ROWS; r++)
        for (c = 0; c < COLS; c++)
            if (p->tiles[r][c] == '.' || p->tiles[r][c] == 'o')
                p->pellets++;
}

static void reset_actors(Pacman *p)
{
    int i;
    p->pac.x = 14.0f * TILE;
    p->pac.y = 23.5f * TILE;
    p->pac.dir = 2;
    p->pac.next = 2;
    p->pac.alive = 1;
    p->dead = 0;
    p->fright = 0;
    p->mode_t = 0;
    p->mode_i = 0;
    p->scatter = 1;
    for (i = 0; i < GHOST_N; i++) {
        p->ghosts[i].id = i;
        p->ghosts[i].a.dir = 3;
        p->ghosts[i].a.next = 3;
        p->ghosts[i].eaten = 0;
        p->ghosts[i].a.y = 14.5f * TILE;
    }
    /* Blinky starts outside. Others leave together; pathing keeps them apart. */
    p->ghosts[0].a.x = 14.5f * TILE;
    p->ghosts[0].a.y = 11.5f * TILE;
    p->ghosts[0].a.dir = 2;
    p->ghosts[0].a.next = 2;
    p->ghosts[0].wait = 0;
    p->ghosts[1].a.x = 14.5f * TILE;
    p->ghosts[1].wait = 0.4f;
    p->ghosts[2].a.x = 12.5f * TILE;
    p->ghosts[2].wait = 1.2f;
    p->ghosts[3].a.x = 15.5f * TILE;
    p->ghosts[3].wait = 2.2f;
}

static void *init(Canvas *c)
{
    Pacman *p = calloc(1, sizeof(*p));
    int i;
    for (i = 0; i < ROWS; i++)
        memcpy(p->tiles[i], LEVEL[i], COLS);
    load_or_solid(c, &p->pac_sheet, "assets/pacman/pac.png", 16, 16);
    load_or_solid(c, &p->ghost_sheet, "assets/pacman/ghosts.png", 16, 16);
    p->lives = 3;
    count_pellets(p);
    reset_actors(p);
    p->snd_waka[0] = canvas_sound_tone(c, 620.0f, 70.0f, 0.35f);
    p->snd_waka[1] = canvas_sound_tone(c, 480.0f, 70.0f, 0.35f);
    p->snd_power = canvas_sound_tone(c, 180.0f, 280.0f, 0.45f);
    p->snd_eat = canvas_sound_tone(c, 900.0f, 160.0f, 0.45f);
    p->snd_die = canvas_sound_noise(c, 420.0f, 0.5f);
    p->snd_win = canvas_sound_tone(c, 740.0f, 400.0f, 0.4f);
    canvas_set_title(c, "Pac-Man - level 1  arrows move");
    return p;
}

static int ghost_pick(Pacman *p, Ghost *g, int fright);

static int can_go(const Pacman *p, float x, float y, int dir, const Ghost *g)
{
    int c, r;
    cell_of(x, y, &c, &r);
    c = wrap_c(c + DX[dir]);
    r += DY[dir];
    return g ? !ghost_blocked(p, g, c, r) : !wall(p, c, r);
}

static float mid_dist(const Actor *a)
{
    float fx = fmodf(a->x, TILE);
    float fy = fmodf(a->y, TILE);
    if (fx < 0)
        fx += TILE;
    if (fy < 0)
        fy += TILE;
    if (a->dir == 0)
        return TILE * 0.5f - fx;
    if (a->dir == 2)
        return fx - TILE * 0.5f;
    if (a->dir == 1)
        return TILE * 0.5f - fy;
    return fy - TILE * 0.5f;
}

static void wrap_actor(Actor *a)
{
    int c, r;
    cell_of(a->x, a->y, &c, &r);
    if (c < 0)
        a->x += COLS * TILE;
    if (c >= COLS)
        a->x -= COLS * TILE;
}

static void apply_turn(Pacman *p, Actor *a, const Ghost *g)
{
    if (a->next != a->dir && can_go(p, a->x, a->y, a->next, g))
        a->dir = a->next;
}

static void move_actor(Pacman *p, Actor *a, float speed, float dt, const Ghost *g)
{
    float step = speed * dt;
    float md;

    if (step > TILE * 0.4f)
        step = TILE * 0.4f;
    md = mid_dist(a);
    if (md > 0.05f && step >= md) {
        snap_center(a);
        step -= md;
        apply_turn(p, a, g);
    } else if (near_center(a->x, a->y)) {
        apply_turn(p, a, g);
    }
    if (!can_go(p, a->x, a->y, a->dir, g)) {
        snap_center(a);
        apply_turn(p, a, g);
        if (!can_go(p, a->x, a->y, a->dir, g))
            return;
    }
    a->x += DX[a->dir] * step;
    a->y += DY[a->dir] * step;
    wrap_actor(a);
}

static void move_ghost(Pacman *p, Ghost *g, float dt)
{
    float speed, step, md;
    int fright = p->fright > 0 && !g->eaten;

    if (g->eaten)
        speed = 140.0f;
    else if (p->fright > 0)
        speed = 52.0f;
    else
        speed = 76.0f;
    step = speed * dt;
    if (step > TILE * 0.4f)
        step = TILE * 0.4f;

    if (g->eaten) {
        g->a.next = ghost_pick(p, g, 0);
        if (can_go(p, g->a.x, g->a.y, g->a.next, g))
            g->a.dir = g->a.next;
    }

    md = mid_dist(&g->a);
    if (md > 0.05f && step >= md) {
        snap_center(&g->a);
        step -= md;
        g->a.next = ghost_pick(p, g, fright);
        apply_turn(p, &g->a, g);
    }
    if (!can_go(p, g->a.x, g->a.y, g->a.dir, g)) {
        snap_center(&g->a);
        g->a.next = ghost_pick(p, g, fright);
        apply_turn(p, &g->a, g);
        if (!can_go(p, g->a.x, g->a.y, g->a.dir, g))
            return;
    }
    g->a.x += DX[g->a.dir] * step;
    g->a.y += DY[g->a.dir] * step;
    wrap_actor(&g->a);
}

static void ghost_target(Pacman *p, Ghost *g, int *tc, int *tr)
{
    int pc, pr, pdir = p->pac.dir;
    int ahead_c, ahead_r;

    cell_of(p->pac.x, p->pac.y, &pc, &pr);
    ahead_c = pc + DX[pdir] * 4;
    ahead_r = pr + DY[pdir] * 4;
    if (pdir == 3)
        ahead_c -= 4;

    if (g->eaten) {
        *tc = 14;
        *tr = 14;
        return;
    }
    if (in_house((int)(g->a.x / TILE), (int)(g->a.y / TILE))) {
        *tc = 14;
        *tr = 11;
        return;
    }
    if (p->scatter) {
        *tc = SCATTER[g->id][0];
        *tr = SCATTER[g->id][1];
        return;
    }
    switch (g->id) {
    case 0: /* Blinky: chase Pac-Man */
        *tc = pc;
        *tr = pr;
        break;
    case 1: /* Pinky: 4 tiles ahead (up also shifts left, arcade overflow) */
        *tc = ahead_c;
        *tr = ahead_r;
        break;
    case 2: { /* Inky: double the vector from Blinky through 2 tiles ahead */
        int ac = pc + DX[pdir] * 2;
        int ar = pr + DY[pdir] * 2;
        int bx, by;
        if (pdir == 3)
            ac -= 2;
        cell_of(p->ghosts[0].a.x, p->ghosts[0].a.y, &bx, &by);
        *tc = ac + (ac - bx);
        *tr = ar + (ar - by);
        break;
    }
    default: { /* Clyde: chase if far, else his scatter corner */
        int gc, gr;
        cell_of(g->a.x, g->a.y, &gc, &gr);
        if (abs(gc - pc) + abs(gr - pr) > 8) {
            *tc = pc;
            *tr = pr;
        } else {
            *tc = SCATTER[3][0];
            *tr = SCATTER[3][1];
        }
        break;
    }
    }
}

static int home_step(Pacman *p, Ghost *g)
{
    int gc, gr, goal_c, goal_r, i;
    int q[ROWS * COLS];
    int first[ROWS * COLS];
    unsigned char seen[ROWS][COLS];
    int head = 0, tail = 0;

    cell_of(g->a.x, g->a.y, &gc, &gr);
    gc = wrap_c(gc);
    if (gr < 0 || gr >= ROWS)
        return g->a.dir;

    if (gc == 14 && gr == 11)
        return 1;
    if (in_house(gc, gr)) {
        goal_c = 14;
        goal_r = 14;
    } else {
        goal_c = 14;
        goal_r = 11;
    }
    if (gc == goal_c && gr == goal_r)
        return g->a.dir;

    memset(seen, 0, sizeof(seen));
    q[tail] = gr * COLS + gc;
    first[tail] = -1;
    tail++;
    seen[gr][gc] = 1;

    while (head < tail) {
        int idx = q[head];
        int fd = first[head];
        int c = idx % COLS, r = idx / COLS;
        head++;
        for (i = 0; i < 4; i++) {
            int nc = wrap_c(c + DX[i]);
            int nr = r + DY[i];
            int nd;
            if (nr < 0 || nr >= ROWS || seen[nr][nc])
                continue;
            if (ghost_blocked(p, g, nc, nr))
                continue;
            seen[nr][nc] = 1;
            nd = (fd < 0) ? i : fd;
            if (nc == goal_c && nr == goal_r)
                return nd;
            q[tail] = nr * COLS + nc;
            first[tail] = nd;
            tail++;
        }
    }
    return -1;
}

static int ghost_pick(Pacman *p, Ghost *g, int fright)
{
    int i, best = g->a.dir, best_d = 10000, n = 0;
    int gc, gr, tc, tr, rev = (g->a.dir + 2) & 3;
    int house, opts[4];

    cell_of(g->a.x, g->a.y, &gc, &gr);
    house = in_house(gc, gr);
    if (g->eaten) {
        int step = home_step(p, g);
        if (step >= 0)
            return step;
    }
    ghost_target(p, g, &tc, &tr);
    for (i = 0; i < 4; i++) {
        int d = DIR_PRI[i];
        if (!house && d == rev)
            continue;
        if (can_go(p, g->a.x, g->a.y, d, g))
            opts[n++] = d;
    }
    if (n == 0)
        return rev;
    if (fright && !house && !g->eaten)
        return opts[rand() % n];
    for (i = 0; i < n; i++) {
        int nc = wrap_c(gc + DX[opts[i]]);
        int nr = gr + DY[opts[i]];
        int d = abs(nc - tc) + abs(nr - tr);
        if (d < best_d) {
            best_d = d;
            best = opts[i];
        }
    }
    return best;
}

static void update_modes(Pacman *p, float dt)
{
    int next;
    if (p->fright > 0)
        return;
    p->mode_t += dt;
    if (p->mode_t < MODE_SECS[p->mode_i])
        return;
    p->mode_t = 0;
    if (p->mode_i < (int)(sizeof(MODE_SECS) / sizeof(MODE_SECS[0])) - 1)
        p->mode_i++;
    next = MODE_SCATTER[p->mode_i];
    if (next != p->scatter) {
        int i;
        p->scatter = next;
        for (i = 0; i < GHOST_N; i++) {
            Ghost *g = &p->ghosts[i];
            int c, r;
            cell_of(g->a.x, g->a.y, &c, &r);
            if (!g->eaten && !in_house(c, r)) {
                g->a.next = (g->a.dir + 2) & 3;
                g->a.dir = g->a.next;
            }
        }
    }
}

static void eat_at_pac(Pacman *p, Canvas *cv)
{
    int c, r;
    cell_of(p->pac.x, p->pac.y, &c, &r);
    c = wrap_c(c);
    if (r < 0 || r >= ROWS)
        return;
    if (p->tiles[r][c] == '.') {
        p->tiles[r][c] = ' ';
        p->score += 10;
        p->pellets--;
        canvas_sound_play(cv, p->snd_waka[p->waka & 1], 0.55f);
        p->waka++;
    } else if (p->tiles[r][c] == 'o') {
        int i;
        p->tiles[r][c] = ' ';
        p->score += 50;
        p->pellets--;
        p->fright = 6.0f;
        canvas_sound_play(cv, p->snd_power, 0.7f);
        for (i = 0; i < GHOST_N; i++) {
            Ghost *g = &p->ghosts[i];
            int gc, gr;
            if (g->eaten)
                continue;
            cell_of(g->a.x, g->a.y, &gc, &gr);
            if (in_house(gc, gr))
                continue;
            g->a.dir = (g->a.dir + 2) & 3;
            g->a.next = g->a.dir;
        }
    }
    if (p->pellets <= 0) {
        p->won = 1;
        canvas_sound_play(cv, p->snd_win, 0.8f);
    }
}

static void update(void *state, Canvas *c, float dt)
{
    Pacman *p = state;
    int i, keydir = -1;
    float z, maze_w = COLS * TILE, maze_h = ROWS * TILE;

    if (canvas_key_pressed(c, KEY_ESCAPE) || canvas_key_pressed(c, KEY_Q))
        canvas_quit(c);
    if (canvas_key_down(c, KEY_LEFT) || canvas_key_down(c, KEY_A))
        keydir = 2;
    if (canvas_key_down(c, KEY_RIGHT) || canvas_key_down(c, KEY_D))
        keydir = 0;
    if (canvas_key_down(c, KEY_UP) || canvas_key_down(c, KEY_W))
        keydir = 3;
    if (canvas_key_down(c, KEY_DOWN) || canvas_key_down(c, KEY_S))
        keydir = 1;

    z = (float)canvas_width(c) / maze_w;
    if ((float)(canvas_height(c) - (int)HUD) / maze_h < z)
        z = (float)(canvas_height(c) - (int)HUD) / maze_h;
    if (z < 0.2f)
        z = 0.2f;
    canvas_cam_zoom(c, z);
    canvas_cam_set(c, -(canvas_width(c) / z - maze_w) * 0.5f, -HUD / z);

    if (p->won || p->lives <= 0)
        return;
    if (p->dead) {
        p->flash += dt;
        if (p->flash > 1.2f) {
            p->lives--;
            p->dead = 0;
            p->flash = 0;
            if (p->lives > 0)
                reset_actors(p);
        }
        return;
    }

    if (keydir >= 0)
        p->pac.next = keydir;

    p->mouth += dt * 10.0f;
    p->fright -= dt;
    if (p->fright < 0)
        p->fright = 0;

    update_modes(p, dt);
    move_actor(p, &p->pac, 88.0f, dt, NULL);
    eat_at_pac(p, c);

    for (i = 0; i < GHOST_N; i++) {
        Ghost *g = &p->ghosts[i];
        int gc, gr, pc, pr;
        if (g->wait > 0) {
            g->wait -= dt;
            continue;
        }
        move_ghost(p, g, dt);
        cell_of(g->a.x, g->a.y, &gc, &gr);
        cell_of(p->pac.x, p->pac.y, &pc, &pr);
        if (g->eaten) {
            if (in_house(gc, gr) && gr >= 13) {
                static const float hx[GHOST_N] = {14.5f, 14.5f, 12.5f, 15.5f};
                g->eaten = 0;
                g->a.x = hx[g->id] * TILE;
                g->a.y = 14.5f * TILE;
                g->a.dir = 3;
                g->a.next = 3;
                g->wait = 3.0f;
            }
            continue;
        }
        if (abs(gc - pc) + abs(gr - pr) == 0 ||
            (fabsf(g->a.x - p->pac.x) < TILE * 0.8f && fabsf(g->a.y - p->pac.y) < TILE * 0.8f)) {
            if (p->fright > 0) {
                g->eaten = 1;
                p->score += 200;
                g->a.next = ghost_pick(p, g, 0);
                g->a.dir = g->a.next;
                canvas_sound_play(c, p->snd_eat, 0.75f);
            } else {
                p->dead = 1;
                p->flash = 0;
                canvas_sound_play(c, p->snd_die, 0.8f);
            }
        }
    }
}

static void draw_maze(Pacman *p, Canvas *c)
{
    int r, col;
    for (r = 0; r < ROWS; r++) {
        for (col = 0; col < COLS; col++) {
            float x = col * TILE, y = r * TILE;
            char t = p->tiles[r][col];
            if (t == '#')
                canvas_fill_rect(c, x + 1, y + 1, TILE - 2, TILE - 2, 0.15f, 0.25f, 0.85f, 1);
            else if (t == '-')
                canvas_fill_rect(c, x + 2, y + TILE * 0.4f, TILE - 4, 3, 1, 0.72f, 0.85f, 1);
            else if (t == '.')
                canvas_fill_rect(c, x + TILE * 0.5f - 2, y + TILE * 0.5f - 2, 4, 4, 1, 0.86f, 0.55f, 1);
            else if (t == 'o') {
                float s = 5.0f + sinf(canvas_time(c) * 8.0f) * 1.5f;
                canvas_fill_rect(c, x + TILE * 0.5f - s, y + TILE * 0.5f - s, s * 2, s * 2, 1, 0.86f, 0.55f, 1);
            }
        }
    }
}

static void render(void *state, Canvas *c)
{
    Pacman *p = state;
    Sprite spr;
    char hud[96];
    int i, frame;

    canvas_clear(c, 0.0f, 0.0f, 0.05f);
    draw_maze(p, c);

    frame = ((int)p->mouth) & 3;
    sprite_from_sheet(&spr, &p->pac_sheet, 0, p->pac.x, p->pac.y);
    sprite_anim(&spr, &p->pac_sheet, 0, 4, 0.0f);
    spr.w = spr.h = 18.0f;
    spr.origin_x = spr.origin_y = 0.5f;
    spr.frame = p->dead ? 0 : frame;
    spr.angle = (p->pac.dir == 0) ? 0 : (p->pac.dir == 1) ? 90 : (p->pac.dir == 2) ? 180 : -90;
    spr.visible = !p->dead || ((int)(p->flash * 8) & 1);
    canvas_draw_sprite(c, &spr);

    for (i = 0; i < GHOST_N; i++) {
        Ghost *g = &p->ghosts[i];
        sprite_from_sheet(&spr, &p->ghost_sheet, g->id, g->a.x, g->a.y);
        spr.w = spr.h = 18.0f;
        spr.origin_x = spr.origin_y = 0.5f;
        if (g->eaten) {
            spr.w = spr.h = 10;
            spr.r = spr.g = spr.b = 1;
            spr.a = 1;
        } else if (p->fright > 0) {
            int blink = p->fright < 2.0f && ((int)(p->fright * 6) & 1);
            spr.r = blink ? 1 : 0.15f;
            spr.g = blink ? 1 : 0.25f;
            spr.b = blink ? 1 : 0.95f;
        }
        canvas_draw_sprite(c, &spr);
    }

    canvas_begin_hud(c);
    canvas_fill_rect(c, 0, 0, (float)canvas_width(c), HUD, 0, 0, 0, 0.75f);
    if (p->won)
        snprintf(hud, sizeof(hud), "LEVEL CLEAR   score %d   R to restart", p->score);
    else if (p->lives <= 0)
        snprintf(hud, sizeof(hud), "GAME OVER   score %d   R to restart", p->score);
    else
        snprintf(hud, sizeof(hud), "PAC-MAN   score %d   lives %d   pellets %d", p->score, p->lives,
                 p->pellets);
    canvas_draw_text(c, 10, 26, hud, 1, 0.92f, 0.35f);
    canvas_end_hud(c);
}

static void restart(Pacman *p)
{
    int i;
    for (i = 0; i < ROWS; i++)
        memcpy(p->tiles[i], LEVEL[i], COLS);
    p->score = 0;
    p->lives = 3;
    p->won = 0;
    count_pellets(p);
    reset_actors(p);
}

static void update_wrap(void *state, Canvas *c, float dt)
{
    Pacman *p = state;
    update(state, c, dt);
    if ((p->won || p->lives <= 0) && canvas_key_pressed(c, KEY_R))
        restart(p);
}

static void shutdown(void *state, Canvas *c)
{
    (void)c;
    free(state);
}

CANVAS_EXPORT const Game canvas_game = {
    .name = "Pac-Man",
    .width = 580,
    .height = 700,
    .init = init,
    .update = update_wrap,
    .render = render,
    .shutdown = shutdown,
};

#ifndef CANVAS_PLUGIN
int main(void)
{
    return canvas_run(&canvas_game);
}
#endif
