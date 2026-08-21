#include "canvas.h"
#include "games/crypt.h"
#include "rpg/actor.h"
#include "rpg/dungeon.h"
#include "rpg/loot.h"
#include "rpg/rpg.h"
#include "rpg/world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE DUN_TILE
#define MOB_MAX RPG_ACTOR_MAX
#define FX_MAX 72
#define BLOOD_MAX 140
#define HERO_RAD 10.0f
#define MELEE 34.0f
#define LOOT_TAKE 30.0f
#define WEAR_CELL 44.0f
#define BAG_CELL 36.0f
#define BAR_CELL 42.0f
#define BAR_GAP 5.0f
#define BOOK_W 8
#define BOOK_H 3

enum { UI_NONE = 0, UI_INV, UI_VENDOR, UI_BANK, UI_QUEST };
enum { DRAG_NONE = 0, DRAG_BAG, DRAG_BAR, DRAG_BOOK };

typedef struct {
    int on, dmg, crit;
    float x, y, t, r, g, b;
} Fx;

typedef struct {
    int on;
    float x, y, a;
} Blood;

typedef struct {
    RpgWorld world;
    RpgHero pc;
    RpgGround ground, ow_ground;
    Sprite spr;
    Sheet tiles, walk, icons;
    Sheet ts_haven, ts_wilds, ts_dun;
    unsigned portrait, portal;
    RpgActor mobs[MOB_MAX];
    Fx fx[FX_MAX];
    Blood blood[BLOOD_MAX];
    RpgActor ow_mobs[MOB_MAX];
    int ui, dead, quit_ask, hover_loot, hover_mob, hover_place;
    int target, loot_want, has_dest, mob_n;
    int ow_ready, ow_mob_n, drag_src, drag_i, drag_live;
    float dest_x, dest_y, attack_cd, bob, step_t, dead_t, note_t, walk_gait;
    float drag_px, drag_py;
    int walk_beat;
    char note[72];
    unsigned snd_hit, snd_miss, snd_die, snd_loot, snd_step, snd_drink, snd_level;
} Arpg;

static int LS(const Arpg *g, int id)
{
    return rpg_get(&g->pc.live, id);
}

static int MS(const RpgActor *m, int id)
{
    return rpg_get(&m->st, id);
}

static Dungeon *cmap(Arpg *g)
{
    return &g->world.maps[g->world.zone];
}

static int tile_seen(const Arpg *g, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= DUN_W || ty >= DUN_H)
        return 0;
    return g->world.seen[g->world.zone][ty][tx];
}

static void place_xy(const RpgPlace *p, float *x, float *y)
{
    *x = p->tx * TILE + TILE * 0.5f;
    *y = p->ty * TILE + TILE * 0.5f;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static float dist2(float ax, float ay, float bx, float by)
{
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

static float gait_bob(float gait)
{
    return sinf(gait * 6.2831853f);
}

#define QUIT_BW 108.0f
#define QUIT_BH 28.0f

static void quit_panel(int W, int H, float *x, float *y, float *w, float *h)
{
    *w = 340.0f;
    *h = 128.0f;
    *x = (float)W * 0.5f - *w * 0.5f;
    *y = (float)H * 0.5f - *h * 0.5f;
}

static void quit_btns(int W, int H, float *stay_x, float *quit_x, float *by)
{
    float x, y, w, h;

    quit_panel(W, H, &x, &y, &w, &h);
    *by = y + 72.0f;
    *stay_x = x + 28.0f;
    *quit_x = x + w - 28.0f - QUIT_BW;
}

static int hit_rect(float mx, float my, float x, float y, float w, float h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static const Sheet *zone_tiles(const Arpg *g)
{
    if (g->world.zone == RPG_ZONE_TOWN && g->ts_haven.texture)
        return &g->ts_haven;
    if (g->world.zone == RPG_ZONE_OVERWORLD && g->ts_wilds.texture)
        return &g->ts_wilds;
    if (g->world.zone == RPG_ZONE_DUNGEON && g->ts_dun.texture)
        return &g->ts_dun;
    return &g->tiles;
}

static void load_or_solid(Canvas *c, Sheet *s, const char *path, int cw, int ch)
{
    if (!canvas_sheet_load(c, s, path, cw, ch)) {
        unsigned t = canvas_texture_solid(c, 0.45f, 0.12f, 0.12f);
        canvas_sheet_init(s, t, cw, ch, cw, ch);
    }
}

static void rarity_rgb(int rarity, float *r, float *g, float *b)
{
    if (rarity == RPG_RARE) {
        *r = 1.0f;
        *g = 0.82f;
        *b = 0.22f;
    } else if (rarity == RPG_MAGIC) {
        *r = 0.40f;
        *g = 0.58f;
        *b = 1.0f;
    } else {
        *r = 0.86f;
        *g = 0.86f;
        *b = 0.82f;
    }
}

static void sync_hp(Arpg *g)
{
    rpg_hero_sync(&g->pc);
}

static void refresh(Arpg *g)
{
    rpg_hero_refresh(&g->pc);
}

static void say(Arpg *g, Canvas *c, const char *s)
{
    snprintf(g->note, sizeof(g->note), "%s", s);
    g->note_t = 2.2f;
    canvas_trace(c, "note", "%s", s);
}

static void add_fx(Arpg *g, float x, float y, int dmg, int crit, float r, float gg, float b)
{
    int i;

    for (i = 0; i < FX_MAX; i++) {
        if (g->fx[i].on)
            continue;
        g->fx[i].on = 1;
        g->fx[i].x = x;
        g->fx[i].y = y;
        g->fx[i].t = 0.7f;
        g->fx[i].dmg = dmg;
        g->fx[i].crit = crit;
        g->fx[i].r = r;
        g->fx[i].g = gg;
        g->fx[i].b = b;
        return;
    }
}

static void add_blood(Arpg *g, float x, float y)
{
    int i;

    for (i = 0; i < BLOOD_MAX; i++) {
        if (g->blood[i].on)
            continue;
        g->blood[i].on = 1;
        g->blood[i].x = x + (float)rpg_rng(-6, 6);
        g->blood[i].y = y + (float)rpg_rng(-4, 4);
        g->blood[i].a = 0.55f;
        return;
    }
}

static void spawn_mobs(Arpg *g)
{
    int r, n, k, tx, ty, sp, champ, i, boss;
    Dungeon *d = cmap(g);
    int depth = g->world.depth;

    memset(g->mobs, 0, sizeof(g->mobs));
    g->mob_n = 0;
    if (rpg_zone_safe(g->world.zone))
        return;
    if (g->world.zone == RPG_ZONE_OVERWORLD) {
        n = Crypt.wilds.count + rpg_rng(0, Crypt.wilds.extra);
        for (k = 0; k < n && g->mob_n < MOB_MAX; k++) {
            if (!dungeon_random_floor(d, &tx, &ty))
                break;
            if ((tx - 36) * (tx - 36) + (ty - 10) * (ty - 10) < Crypt.wilds.gate_tiles * Crypt.wilds.gate_tiles)
                continue;
            i = g->mob_n++;
            crypt_actor_setup(&g->mobs[i], 0, 1, 0);
            dungeon_tile_pos(tx, ty, &g->mobs[i].x, &g->mobs[i].y);
            rpg_set(&g->mobs[i].st, ST_DMIN, Crypt.wilds.imp_dmin);
            rpg_set(&g->mobs[i].st, ST_DMAX, Crypt.wilds.imp_dmax);
        }
        return;
    }
    for (r = 1; r < d->room_n && g->mob_n < MOB_MAX; r++) {
        n = Crypt.dungeon.per_room + depth / Crypt.dungeon.per_depth_div + rpg_rng(0, Crypt.dungeon.extra);
        if (depth <= 1)
            n = Crypt.dungeon.depth1 + rpg_rng(0, Crypt.dungeon.depth1_extra);
        if (n > Crypt.dungeon.cap)
            n = Crypt.dungeon.cap;
        for (k = 0; k < n && g->mob_n < MOB_MAX; k++) {
            dungeon_room_floor(d, r, &tx, &ty);
            sp = crypt_roll_species(depth);
            champ = (depth > 1 && rpg_rng(0, 99) < Crypt.dungeon.champ_pct + depth * Crypt.dungeon.champ_per_depth) ? 1
                                                                                                                    : 0;
            i = g->mob_n++;
            crypt_actor_setup(&g->mobs[i], sp, depth, champ);
            dungeon_tile_pos(tx, ty, &g->mobs[i].x, &g->mobs[i].y);
        }
    }
    boss = crypt_boss_species();
    if (boss >= 0 && depth >= Crypt.dungeon.boss_min_depth && d->room_n > 1 && g->mob_n < MOB_MAX) {
        dungeon_room_floor(d, d->room_n - 1, &tx, &ty);
        i = g->mob_n++;
        crypt_actor_setup(&g->mobs[i], boss, depth, 1);
        dungeon_tile_pos(tx, ty, &g->mobs[i].x, &g->mobs[i].y);
    }
}

static void reveal(Arpg *g)
{
    int rad = g->world.zone == RPG_ZONE_DUNGEON ? Crypt.dungeon.reveal : Crypt.dungeon.reveal_overworld;
    rpg_world_reveal_around(&g->world, g->world.zone, (int)(g->spr.x / TILE), (int)(g->spr.y / TILE), rad);
}

static int lit_tile(const Arpg *g, int tx, int ty)
{
    int cx, cy, dx, dy, rad;

    if (g->world.zone == RPG_ZONE_TOWN)
        return 1;
    rad = g->world.zone == RPG_ZONE_DUNGEON ? Crypt.dungeon.reveal : Crypt.dungeon.reveal_overworld;
    cx = (int)(g->spr.x / TILE);
    cy = (int)(g->spr.y / TILE);
    dx = tx - cx;
    dy = ty - cy;
    return dx * dx + dy * dy <= rad * rad;
}

static void clear_ephemeral(Arpg *g)
{
    memset(g->fx, 0, sizeof(g->fx));
    memset(g->blood, 0, sizeof(g->blood));
    g->target = -1;
    g->has_dest = 0;
    g->ui = UI_NONE;
    g->dead = 0;
    g->loot_want = -1;
}

static void snapshot_overworld(Arpg *g)
{
    if (g->world.zone != RPG_ZONE_OVERWORLD)
        return;
    memcpy(g->ow_mobs, g->mobs, sizeof(g->mobs));
    g->ow_ground = g->ground;
    g->ow_mob_n = g->mob_n;
    g->ow_ready = 1;
}

static void cam_on_hero(Arpg *g, Canvas *c)
{
    canvas_cam_bounds(c, 0, 0, DUN_W * TILE, DUN_H * TILE);
    canvas_cam_set(c, g->spr.x - canvas_width(c) * 0.5f, g->spr.y - canvas_height(c) * 0.5f);
}

static void set_zone_title(Arpg *g, Canvas *c)
{
    char title[96];
    const char *dname = crypt_zone_title(&g->world);
    int i;

    if (g->world.zone == RPG_ZONE_DUNGEON) {
        for (i = 0; i < g->world.place_n[RPG_ZONE_OVERWORLD]; i++) {
            RpgPlace *p = &g->world.places[RPG_ZONE_OVERWORLD][i];
            if (p->kind == RPG_PLACE_PORTAL && p->dest_id == g->world.dungeon_id)
                dname = p->name;
        }
        snprintf(title, sizeof(title), "%s — depth %d  (safe town: Haven)", dname, g->world.depth);
    } else {
        snprintf(title, sizeof(title), "%s — click loot to take, I character", dname);
    }
    canvas_set_title(c, title);
    canvas_trace(c, "zone", "%s depth %d", dname, g->world.zone == RPG_ZONE_DUNGEON ? g->world.depth : 0);
}

static void enter_town(Arpg *g, Canvas *c, int at_gate)
{
    RpgPlace *gate;
    snapshot_overworld(g);
    g->world.zone = RPG_ZONE_TOWN;
    memset(g->mobs, 0, sizeof(g->mobs));
    rpg_ground_clear(&g->ground);
    g->mob_n = 0;
    clear_ephemeral(g);
    gate = rpg_place_kind(&g->world, RPG_ZONE_TOWN, RPG_PLACE_GATE);
    if (at_gate && gate)
        place_xy(gate, &g->spr.x, &g->spr.y);
    else {
        g->spr.x = cmap(g)->start_tx * TILE + TILE * 0.5f;
        g->spr.y = cmap(g)->start_ty * TILE + TILE * 0.5f;
    }
    refresh(g);
    cam_on_hero(g, c);
    set_zone_title(g, c);
}

static void enter_overworld(Arpg *g, Canvas *c, float x, float y)
{
    snapshot_overworld(g);
    g->world.zone = RPG_ZONE_OVERWORLD;
    clear_ephemeral(g);
    if (g->ow_ready) {
        memcpy(g->mobs, g->ow_mobs, sizeof(g->mobs));
        g->ground = g->ow_ground;
        g->mob_n = g->ow_mob_n;
    } else {
        rpg_ground_clear(&g->ground);
        spawn_mobs(g);
        snapshot_overworld(g);
    }
    g->spr.x = x;
    g->spr.y = y;
    cam_on_hero(g, c);
    set_zone_title(g, c);
}

static void enter_dungeon(Arpg *g, Canvas *c, int id, int depth)
{
    snapshot_overworld(g);
    dungeon_gen(&g->world.maps[RPG_ZONE_DUNGEON], rpg_randu() ? rpg_randu() : 1u);
    memset(g->world.seen[RPG_ZONE_DUNGEON], 0, sizeof(g->world.seen[RPG_ZONE_DUNGEON]));
    rpg_world_bind_dungeon(&g->world, id, depth);
    g->world.zone = RPG_ZONE_DUNGEON;
    rpg_ground_clear(&g->ground);
    clear_ephemeral(g);
    g->spr.x = cmap(g)->start_tx * TILE + TILE * 0.5f;
    g->spr.y = cmap(g)->start_ty * TILE + TILE * 0.5f;
    spawn_mobs(g);
    refresh(g);
    cam_on_hero(g, c);
    set_zone_title(g, c);
}

static void interact_place(Arpg *g, Canvas *c, int pi)
{
    RpgPlace *p, *dest;
    float x, y;

    if (pi < 0 || pi >= g->world.place_n[g->world.zone])
        return;
    p = &g->world.places[g->world.zone][pi];
    switch (p->kind) {
    case RPG_PLACE_VENDOR:
        g->ui = (g->ui == UI_VENDOR) ? UI_NONE : UI_VENDOR;
        break;
    case RPG_PLACE_BANK:
        g->ui = (g->ui == UI_BANK) ? UI_NONE : UI_BANK;
        break;
    case RPG_PLACE_QUEST:
        g->ui = (g->ui == UI_QUEST) ? UI_NONE : UI_QUEST;
        break;
    case RPG_PLACE_GATE:
        if (p->dest_zone == RPG_ZONE_OVERWORLD) {
            dest = rpg_place_kind(&g->world, RPG_ZONE_OVERWORLD, RPG_PLACE_GATE);
            place_xy(dest ? dest : p, &x, &y);
            enter_overworld(g, c, x, y);
        } else {
            enter_town(g, c, 1);
        }
        break;
    case RPG_PLACE_PORTAL:
        enter_dungeon(g, c, p->dest_id, p->dest_depth);
        break;
    case RPG_PLACE_EXIT:
        dest = NULL;
        {
            int i;
            for (i = 0; i < g->world.place_n[RPG_ZONE_OVERWORLD]; i++) {
                if (g->world.places[RPG_ZONE_OVERWORLD][i].kind == RPG_PLACE_PORTAL &&
                    g->world.places[RPG_ZONE_OVERWORLD][i].dest_id == g->world.dungeon_id) {
                    dest = &g->world.places[RPG_ZONE_OVERWORLD][i];
                    break;
                }
            }
        }
        if (!dest)
            dest = rpg_place_kind(&g->world, RPG_ZONE_OVERWORLD, RPG_PLACE_GATE);
        place_xy(dest, &x, &y);
        enter_overworld(g, c, x, y);
        break;
    case RPG_PLACE_STAIRS:
        enter_dungeon(g, c, g->world.dungeon_id, g->world.depth + 1);
        break;
    default:
        break;
    }
}

static int fire_bar(Arpg *g, Canvas *c, int slot)
{
    RpgBarSlot *s;
    int use = 0, gi, rc;

    if (slot < 0 || slot >= RPG_BAR_N)
        return 0;
    s = &g->pc.bar.slot[slot];
    if (s->type == RPG_BAR_ITEM) {
        gi = rpg_inv_find(&g->pc.inv, s->id);
        if (gi >= 0 && (g->pc.inv.grid[gi].flags & RPG_IF_USE))
            use = 1;
    }
    rc = rpg_bar_activate(&g->pc, slot);
    if (rc > 0) {
        if (use) {
            canvas_sound_play(c, g->snd_drink, 0.6f);
            say(g, c, "Used");
            sync_hp(g);
        } else {
            say(g, c, "Equipped");
            refresh(g);
        }
        return 1;
    }
    if (rc < 0)
        say(g, c, "Can't cast that yet");
    else if (s->type == RPG_BAR_ITEM)
        say(g, c, "None left");
    return 0;
}

static void bar_origin(int W, int H, float *x, float *y)
{
    (void)W;
    *x = 268.0f;
    *y = (float)H - 56.0f;
}

static int bar_hit(int W, int H, float mx, float my, int *slot)
{
    float x, y, sx;
    int i;

    bar_origin(W, H, &x, &y);
    if (my < y || my >= y + BAR_CELL)
        return 0;
    for (i = 0; i < RPG_BAR_N; i++) {
        sx = x + (float)i * (BAR_CELL + BAR_GAP);
        if (mx >= sx && mx < sx + BAR_CELL) {
            *slot = i;
            return 1;
        }
    }
    return 0;
}

static void book_origin(float px, float py, float *x, float *y)
{
    *x = px + 250.0f;
    *y = py + 208.0f;
}

static int book_hit(float mx, float my, float ox, float oy, int *out)
{
    int col, row;
    float x, y;

    x = mx - ox;
    y = my - oy;
    if (x < 0 || y < 0)
        return 0;
    col = (int)(x / BAG_CELL);
    row = (int)(y / BAG_CELL);
    if (col < 0 || row < 0 || col >= BOOK_W || row >= BOOK_H)
        return 0;
    *out = row * BOOK_W + col;
    return 1;
}

static void draw_skill_mark(Canvas *c, float x, float y, float s)
{
    canvas_fill_rect(c, x + s * 0.18f, y + s * 0.18f, s * 0.64f, s * 0.64f, 0.38f, 0.3f, 0.58f, 0.95f);
}

static int drag_payload(const Arpg *g, RpgBarSlot *out)
{
    int id;

    memset(out, 0, sizeof(*out));
    if (g->drag_src == DRAG_BAG && g->drag_i >= 0 && g->drag_i < rpg_inv_n() &&
        rpg_item_ok(&g->pc.inv.grid[g->drag_i])) {
        out->type = RPG_BAR_ITEM;
        out->id = g->pc.inv.grid[g->drag_i].kind;
        return 1;
    }
    if (g->drag_src == DRAG_BOOK) {
        id = rpg_book_id(&g->pc.build.skills, g->drag_i);
        if (id) {
            out->type = RPG_BAR_SKILL;
            out->id = id;
            return 1;
        }
    }
    if (g->drag_src == DRAG_BAR && g->drag_i >= 0 && g->drag_i < RPG_BAR_N &&
        g->pc.bar.slot[g->drag_i].type != RPG_BAR_EMPTY) {
        *out = g->pc.bar.slot[g->drag_i];
        return 1;
    }
    return 0;
}

static void drag_begin(Arpg *g, int src, int i, float mx, float my)
{
    g->drag_src = src;
    g->drag_i = i;
    g->drag_live = 0;
    g->drag_px = mx;
    g->drag_py = my;
}

static int take_loot(Arpg *g, Canvas *c, int i)
{
    char msg[72];
    int had_item, had_gold, ok;
    RpgLoot *L;

    if (i < 0 || i >= RPG_LOOT_MAX || !g->ground.pile[i].on)
        return 0;
    L = &g->ground.pile[i];
    had_item = rpg_item_ok(&L->item);
    had_gold = L->gold > 0;
    if (had_item)
        snprintf(msg, sizeof(msg), "Took %s", L->item.name);
    else if (had_gold)
        snprintf(msg, sizeof(msg), "Took %d gold", L->gold);
    else
        msg[0] = 0;
    ok = rpg_ground_take(&g->ground, i, &g->pc);
    if (!ok) {
        if (g->note_t <= 0.0f)
            say(g, c, "Pack is full");
        g->loot_want = -1;
        return 0;
    }
    if (had_item && L->on && rpg_item_ok(&L->item)) {
        say(g, c, "Pack is full");
        if (had_gold)
            canvas_sound_play(c, g->snd_loot, 0.35f);
        return 1;
    }
    if (msg[0])
        say(g, c, msg);
    canvas_sound_play(c, g->snd_loot, 0.55f);
    if (g->loot_want == i)
        g->loot_want = -1;
    return 1;
}

static int loot_at(Arpg *g, float x, float y)
{
    return rpg_ground_at(&g->ground, x, y, 32.0f);
}

static int mob_at(Arpg *g, float x, float y)
{
    int i, best = -1;
    float bd = 28.0f * 28.0f, d, rad;

    for (i = 0; i < g->mob_n; i++) {
        if (!g->mobs[i].alive)
            continue;
        rad = crypt_species[g->mobs[i].kind].radius + 8.0f;
        d = dist2(x, y, g->mobs[i].x, g->mobs[i].y);
        if (d < rad * rad && d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

static void kill_mob(Arpg *g, Canvas *c, int i)
{
    RpgActor *m = &g->mobs[i];
    RpgDrop drop;
    int lv;

    m->alive = 0;
    add_blood(g, m->x, m->y);
    add_blood(g, m->x, m->y);
    drop = crypt_roll_drop(g->world.depth, m->role != RPG_ROLE_MINION);
    drop.gold += MS(m, ST_GOLD);
    if (drop.gold > 0) {
        RpgDrop gold = { 0 };
        gold.gold = drop.gold;
        rpg_ground_add(&g->ground, m->x, m->y, gold);
    }
    if (rpg_item_ok(&drop.item)) {
        RpgDrop it = { 0 };
        it.item = drop.item;
        rpg_ground_add(&g->ground, m->x + 10.0f, m->y + 6.0f, it);
    }
    lv = rpg_hero_gain_xp(&g->pc, MS(m, ST_XP));
    refresh(g);
    if (lv) {
        canvas_sound_play(c, g->snd_level, 0.8f);
        canvas_trace(c, "level", "%d", LS(g, ST_LV));
    }
    canvas_sound_play(c, g->snd_die, 0.55f);
    if (g->target == i)
        g->target = -1;
}

static void hero_swing(Arpg *g, Canvas *c)
{
    RpgActor *m;
    int dmg, crit;
    const CryptSpecies *sp;

    if (g->target < 0 || g->attack_cd > 0.0f || g->dead)
        return;
    m = &g->mobs[g->target];
    if (!m->alive) {
        g->target = -1;
        return;
    }
    sp = &crypt_species[m->kind];
    if (dist2(g->spr.x, g->spr.y, m->x, m->y) > (MELEE + sp->radius) * (MELEE + sp->radius))
        return;
    dmg = rpg_melee(&g->pc.live, &m->st, &crit);
    g->attack_cd = clampf(Crypt.feel.swing - LS(g, ST_DEX) * Crypt.feel.swing_dex, Crypt.feel.swing_min,
                          Crypt.feel.swing_max);
    if (dmg <= 0) {
        canvas_sound_play(c, g->snd_miss, 0.4f);
        add_fx(g, m->x, m->y - 18.0f, 0, 0, 0.7f, 0.7f, 0.7f);
        canvas_trace(c, "miss", "%s", crypt_species[m->kind].name);
        return;
    }
    rpg_add(&m->st, ST_HP, -dmg);
    m->hurt = 0.12f;
    add_fx(g, m->x, m->y - 20.0f, dmg, crit, 1.0f, crit ? 0.9f : 0.35f, 0.2f);
    canvas_sound_play(c, g->snd_hit, crit ? 0.9f : 0.65f);
    canvas_trace(c, "hit", "%s %d%s hp %d", crypt_species[m->kind].name, dmg, crit ? " crit" : "",
                 rpg_get(&m->st, ST_HP));
    if (rpg_get(&m->st, ST_HP) <= 0)
        kill_mob(g, c, g->target);
}

static void hurt_hero(Arpg *g, Canvas *c, int dmg, int crit)
{
    if (dmg <= 0)
        return;
    rpg_add(&g->pc.live, ST_HP, -dmg);
    sync_hp(g);
    add_fx(g, g->spr.x, g->spr.y - 28.0f, dmg, crit, 0.95f, 0.2f, 0.2f);
    canvas_sound_play(c, g->snd_hit, crit ? 0.7f : 0.45f);
    canvas_trace(c, "hurt", "%d hp %d", dmg, LS(g, ST_HP));
    if (LS(g, ST_HP) <= 0) {
        rpg_set(&g->pc.live, ST_HP, 0);
        sync_hp(g);
        g->dead = 1;
        g->dead_t = 0.0f;
        g->has_dest = 0;
        g->target = -1;
        canvas_sound_play(c, g->snd_die, 0.9f);
        canvas_trace(c, "death", "hp 0 gold %d zone %s", LS(g, ST_GOLD), crypt_zone_title(&g->world));
    }
}

static void mob_ability(Arpg *g, Canvas *c, RpgActor *m, int ab)
{
    int dmg, crit;
    float reach, d;

    if (ab == CRYPT_AB_SLAM) {
        reach = m->range * 1.85f;
        d = dist2(g->spr.x, g->spr.y, m->x, m->y);
        add_fx(g, m->x, m->y - 10.0f, 0, 1, 0.95f, 0.45f, 0.15f);
        canvas_trace(c, "ability", "%s slam", crypt_species[m->kind].name);
        if (d <= reach * reach) {
            dmg = rpg_melee(&m->st, &g->pc.live, &crit);
            if (dmg > 0)
                dmg = dmg + dmg / 2;
            hurt_hero(g, c, dmg, 1);
        }
        return;
    }
}

static void mob_ai(Arpg *g, Canvas *c, float dt)
{
    int i, dmg, crit;
    RpgActor *m;
    RpgAiResult act;

    for (i = 0; i < g->mob_n; i++) {
        m = &g->mobs[i];
        if (!m->alive)
            continue;
        crypt_actor_refresh(m);
        act = rpg_ai_step(m, cmap(g), g->spr.x, g->spr.y, dt, !g->dead);
        if (g->dead || act.act == RPG_ACT_NONE)
            continue;
        if (act.act == RPG_ACT_ABILITY) {
            mob_ability(g, c, m, act.ability);
            continue;
        }
        dmg = rpg_melee(&m->st, &g->pc.live, &crit);
        hurt_hero(g, c, dmg, crit);
    }
}

static void move_hero(Arpg *g, float dt)
{
    float dx, dy, len, sp, step;

    if (g->dead || !g->has_dest)
        return;
    dx = g->dest_x - g->spr.x;
    dy = g->dest_y - g->spr.y;
    len = sqrtf(dx * dx + dy * dy);
    if (g->target >= 0 && g->mobs[g->target].alive) {
        const CryptSpecies *spcs = &crypt_species[g->mobs[g->target].kind];
        if (dist2(g->spr.x, g->spr.y, g->mobs[g->target].x, g->mobs[g->target].y) <
            (MELEE + spcs->radius - 4.0f) * (MELEE + spcs->radius - 4.0f)) {
            g->has_dest = 0;
            g->spr.vx = g->spr.vy = 0;
            return;
        }
        g->dest_x = g->mobs[g->target].x;
        g->dest_y = g->mobs[g->target].y;
        dx = g->dest_x - g->spr.x;
        dy = g->dest_y - g->spr.y;
        len = sqrtf(dx * dx + dy * dy);
    }
    if (g->loot_want >= 0 && g->ground.pile[g->loot_want].on) {
        g->dest_x = g->ground.pile[g->loot_want].x;
        g->dest_y = g->ground.pile[g->loot_want].y;
        dx = g->dest_x - g->spr.x;
        dy = g->dest_y - g->spr.y;
        len = sqrtf(dx * dx + dy * dy);
        if (len < LOOT_TAKE) {
            g->has_dest = 0;
            g->spr.vx = g->spr.vy = 0;
            return;
        }
    }
    if (len < 4.0f) {
        g->has_dest = 0;
        g->spr.vx = g->spr.vy = 0;
        return;
    }
    sp = Crypt.feel.walk + LS(g, ST_DEX) * Crypt.feel.walk_dex;
    step = sp * dt;
    if (step > len)
        step = len;
    dx = dx / len * step;
    dy = dy / len * step;
    {
        float ox = g->spr.x, oy = g->spr.y, dist;
        dungeon_slide(cmap(g), &g->spr.x, &g->spr.y, dx, dy, HERO_RAD);
        g->spr.vx = (g->spr.x - ox) / (dt > 1e-6f ? dt : 1e-6f);
        g->spr.vy = (g->spr.y - oy) / (dt > 1e-6f ? dt : 1e-6f);
        dist = sqrtf((g->spr.x - ox) * (g->spr.x - ox) + (g->spr.y - oy) * (g->spr.y - oy));
        if (dist > 0.35f) {
            g->walk_gait += dist / (TILE * 1.5f);
            while (g->walk_gait >= 1.0f)
                g->walk_gait -= 1.0f;
        } else {
            g->walk_gait = 0.0f;
        }
        if (len >= 4.0f && fabsf(g->spr.x - ox) < 0.02f && fabsf(g->spr.y - oy) < 0.02f) {
            g->has_dest = 0;
            g->spr.vx = g->spr.vy = 0;
            g->walk_gait = 0.0f;
        }
    }
    if (dx < -0.4f)
        g->spr.flip_x = 1;
    if (dx > 0.4f)
        g->spr.flip_x = 0;
}

static int inv_hit_grid(float mx, float my, float ox, float oy, int *out)
{
    int col, row;
    float x, y;

    x = mx - ox;
    y = my - oy;
    if (x < 0 || y < 0)
        return 0;
    col = (int)(x / 36.0f);
    row = (int)(y / 36.0f);
    if (col < 0 || row < 0 || col >= rpg_inv_w() || row >= rpg_inv_h())
        return 0;
    *out = row * rpg_inv_w() + col;
    return 1;
}

static void sheet_origin(int W, int H, float *px, float *py)
{
    *px = (float)W * 0.5f - 330.0f;
    *py = (float)H * 0.5f - 200.0f;
}

static void wear_xy(float px, float py, int slot, float *x, float *y)
{
    float cx = px + 100.0f, cy = py + 100.0f;

    switch (slot) {
    case SL_HELM:
        *x = cx;
        *y = cy - 56.0f;
        break;
    case SL_WEP:
        *x = cx - 60.0f;
        *y = cy;
        break;
    case SL_ARM:
        *x = cx;
        *y = cy;
        break;
    case SL_OFF:
        *x = cx + 60.0f;
        *y = cy;
        break;
    default:
        *x = cx;
        *y = cy + 56.0f;
        break;
    }
}

static int hit_wear(float mx, float my, float px, float py, int *slot)
{
    int i;
    float x, y;

    for (i = 0; i < rpg_slot_n(); i++) {
        wear_xy(px, py, i, &x, &y);
        if (mx >= x && mx < x + WEAR_CELL && my >= y && my < y + WEAR_CELL) {
            *slot = i;
            return 1;
        }
    }
    return 0;
}

static void *init(Canvas *c)
{
    Arpg *g = canvas_calloc(1, sizeof(*g));

    load_or_solid(c, &g->tiles, "assets/wander/tiles.png", 16, 16);
    load_or_solid(c, &g->walk, "assets/wander/hero.png", 16, 20);
    canvas_sheet_load(c, &g->icons, "assets/crypt/icons.png", 64, 64);
    canvas_sheet_load(c, &g->ts_haven, "assets/crypt/tiles/haven.png", 64, 64);
    canvas_sheet_load(c, &g->ts_wilds, "assets/crypt/tiles/wilds.png", 64, 64);
    canvas_sheet_load(c, &g->ts_dun, "assets/crypt/tiles/dungeon.png", 64, 64);
    g->portrait = canvas_texture_file(c, "assets/crypt/portraits/male01.png");
    g->portal = canvas_texture_file(c, "assets/crypt/tiles/portal.png");
    sprite_from_sheet(&g->spr, &g->walk, 0, 0, 0);
    sprite_anim(&g->spr, &g->walk, 0, 4, 8.0f);
    g->spr.w = 32.0f;
    g->spr.h = 40.0f;
    g->spr.origin_x = 0.5f;
    g->spr.origin_y = 1.0f;
    g->snd_hit = canvas_sound_noise(c, 55.0f, 0.5f);
    g->snd_miss = canvas_sound_tone(c, 220.0f, 60.0f, 0.25f);
    g->snd_die = canvas_sound_noise(c, 180.0f, 0.55f);
    g->snd_loot = canvas_sound_tone(c, 660.0f, 90.0f, 0.4f);
    g->snd_step = canvas_sound_noise(c, 50.0f, 0.28f);
    g->snd_drink = canvas_sound_tone(c, 480.0f, 80.0f, 0.35f);
    g->snd_level = canvas_sound_tone(c, 880.0f, 160.0f, 0.45f);
    g->hover_loot = g->hover_mob = g->hover_place = g->target = g->loot_want = -1;
    g->walk_beat = -1;
    {
        unsigned seed = canvas_seed(c);
        if (!seed)
            seed = (unsigned)(canvas_time(c) * 997.0f) ^ 0xC0B17u;
        if (!seed)
            seed = 1u;
        rpg_seed(seed);
        canvas_trace(c, "seed", "%u", seed);
        crypt_world_init(&g->world, seed);
    }
    rpg_hero_init(&g->pc);
    crypt_kit(&g->pc);
    g->spr.x = cmap(g)->start_tx * TILE + TILE * 0.5f;
    g->spr.y = cmap(g)->start_ty * TILE + TILE * 0.5f;
    refresh(g);
    cam_on_hero(g, c);
    set_zone_title(g, c);
    return g;
}

static void update(void *state, Canvas *c, float dt)
{
    Arpg *g = state;
    float wx, wy, vx, vy;
    int i, moving, li;
    int pi;
    if (g->quit_ask) {
        float mx = (float)canvas_mouse_x(c), my = (float)canvas_mouse_y(c);
        float stay_x, quit_x, by;
        int click = canvas_mouse_pressed(c, 1);

        quit_btns(canvas_width(c), canvas_height(c), &stay_x, &quit_x, &by);
        if (canvas_key_pressed(c, KEY_ESCAPE) || canvas_key_pressed(c, KEY_Q) ||
            canvas_key_pressed(c, KEY_Y) || canvas_key_pressed(c, KEY_ENTER) ||
            (click && hit_rect(mx, my, quit_x, by, QUIT_BW, QUIT_BH)))
            canvas_quit(c);
        else if (canvas_key_pressed(c, KEY_N) ||
                 (click && hit_rect(mx, my, stay_x, by, QUIT_BW, QUIT_BH)))
            g->quit_ask = 0;
        canvas_cam_follow(c, g->spr.x - canvas_width(c) / (2.0f * canvas_cam_zoom_get(c)),
                          g->spr.y - canvas_height(c) / (2.0f * canvas_cam_zoom_get(c)), 4.8f);
        return;
    }
    if (canvas_key_pressed(c, KEY_ESCAPE) || canvas_key_pressed(c, KEY_Q)) {
        g->quit_ask = 1;
        return;
    }
    crypt_bind();
    if (canvas_key_pressed(c, KEY_I) || canvas_key_pressed(c, KEY_TAB) || canvas_key_pressed(c, KEY_C))
        g->ui = (g->ui == UI_INV) ? UI_NONE : UI_INV;
    if (!g->dead) {
        static const CanvasKey kbar[RPG_BAR_N] = { KEY_1, KEY_2, KEY_3, KEY_4, KEY_5 };
        for (i = 0; i < RPG_BAR_N; i++) {
            if (canvas_key_pressed(c, kbar[i]))
                fire_bar(g, c, i);
        }
    }

    canvas_screen_to_world(c, (float)canvas_mouse_x(c), (float)canvas_mouse_y(c), &wx, &wy);
    g->hover_loot = loot_at(g, wx, wy);
    g->hover_mob = rpg_zone_safe(g->world.zone) ? -1 : mob_at(g, wx, wy);
    g->hover_place = rpg_place_near(&g->world, wx, wy, 22.0f);

    if (g->dead) {
        g->dead_t += dt;
        if (canvas_key_pressed(c, KEY_R) || canvas_mouse_pressed(c, 1)) {
            int lost, gold;
            gold = LS(g, ST_GOLD);
            lost = Crypt.feel.death_gold_div > 0 ? gold / Crypt.feel.death_gold_div : 0;
            gold -= lost;
            if (gold < 0)
                gold = 0;
            rpg_set(&g->pc.base, ST_GOLD, gold);
            rpg_set(&g->pc.live, ST_GOLD, gold);
            rpg_set(&g->pc.base, ST_HP, -1);
            rpg_set(&g->pc.base, ST_MP, -1);
            enter_town(g, c, 0);
            refresh(g);
        }
        canvas_cam_follow(c, g->spr.x - canvas_width(c) / (2.0f * canvas_cam_zoom_get(c)),
                          g->spr.y - canvas_height(c) / (2.0f * canvas_cam_zoom_get(c)), 4.8f);
        return;
    }

    {
        float mx = (float)canvas_mouse_x(c), my = (float)canvas_mouse_y(c);
        int W = canvas_width(c), H = canvas_height(c), bslot = -1, gi, si;
        float px, py, bagx, bagy, bookx, booky;
        RpgBarSlot pay;

        sheet_origin(W, H, &px, &py);
        bagx = px + 250.0f;
        bagy = py + 48.0f;
        book_origin(px, py, &bookx, &booky);
        if (canvas_mouse_pressed(c, 2) && bar_hit(W, H, mx, my, &bslot))
            rpg_bar_unbind(&g->pc.bar, bslot);
        if (canvas_mouse_pressed(c, 1) && !g->drag_src) {
            if (bar_hit(W, H, mx, my, &bslot) && g->pc.bar.slot[bslot].type != RPG_BAR_EMPTY)
                drag_begin(g, DRAG_BAR, bslot, mx, my);
            else if (g->ui == UI_INV && inv_hit_grid(mx, my, bagx, bagy, &gi) &&
                     rpg_item_ok(&g->pc.inv.grid[gi]))
                drag_begin(g, DRAG_BAG, gi, mx, my);
            else if (g->ui == UI_INV && book_hit(mx, my, bookx, booky, &si) && rpg_book_id(&g->pc.build.skills, si))
                drag_begin(g, DRAG_BOOK, si, mx, my);
        }
        if (g->drag_src && canvas_mouse_down(c, 1) &&
            dist2(mx, my, g->drag_px, g->drag_py) > 64.0f)
            g->drag_live = 1;
        if (g->drag_src && !canvas_mouse_down(c, 1)) {
            int over = bar_hit(W, H, mx, my, &bslot);
            int got = drag_payload(g, &pay);

            if (over && g->drag_src == DRAG_BAR) {
                if (g->drag_live)
                    rpg_bar_swap(&g->pc.bar, g->drag_i, bslot);
                else if (bslot == g->drag_i)
                    fire_bar(g, c, bslot);
            } else if (over && got)
                rpg_hero_bar_put(&g->pc, bslot, pay.type, pay.id);
            else if (!over && g->drag_src == DRAG_BAR && g->drag_live)
                rpg_bar_unbind(&g->pc.bar, g->drag_i);
            else if (!over && g->drag_src == DRAG_BAG && !g->drag_live) {
                RpgItem *it = &g->pc.inv.grid[g->drag_i];
                if (it->flags & RPG_IF_USE) {
                    if (rpg_use(&g->pc, it)) {
                        canvas_sound_play(c, g->snd_drink, 0.6f);
                        say(g, c, "Drank a potion");
                    }
                    sync_hp(g);
                } else if (rpg_item_ok(it)) {
                    if (rpg_equip(&g->pc.inv, g->drag_i) == 0) {
                        say(g, c, "Equipped");
                        refresh(g);
                    }
                }
            }
            g->drag_src = DRAG_NONE;
            g->drag_live = 0;
        }
    }

    vx = vy = 0;
    if (g->ui == UI_NONE) {
        if (canvas_key_down(c, KEY_LEFT) || canvas_key_down(c, KEY_A))
            vx -= 1;
        if (canvas_key_down(c, KEY_RIGHT) || canvas_key_down(c, KEY_D))
            vx += 1;
        if (canvas_key_down(c, KEY_UP) || canvas_key_down(c, KEY_W))
            vy -= 1;
        if (canvas_key_down(c, KEY_DOWN) || canvas_key_down(c, KEY_S))
            vy += 1;
        if (vx != 0 || vy != 0) {
            if (vx != 0 && vy != 0) {
                vx *= 0.7071f;
                vy *= 0.7071f;
            }
            g->has_dest = 1;
            g->dest_x = g->spr.x + vx * 72.0f;
            g->dest_y = g->spr.y + vy * 72.0f;
            g->target = -1;
        }
        if (canvas_mouse_pressed(c, 1) && !g->drag_src) {
            int bslot;
            if (bar_hit(canvas_width(c), canvas_height(c), (float)canvas_mouse_x(c), (float)canvas_mouse_y(c),
                        &bslot)) {
                /* click is for the hotbar, not the world */
            } else if (g->hover_mob >= 0) {
                g->loot_want = -1;
                g->target = g->hover_mob;
                g->has_dest = 1;
                g->dest_x = g->mobs[g->target].x;
                g->dest_y = g->mobs[g->target].y;
            } else if (g->hover_loot >= 0) {
                g->target = -1;
                g->loot_want = g->hover_loot;
                if (dist2(g->spr.x, g->spr.y, g->ground.pile[g->hover_loot].x, g->ground.pile[g->hover_loot].y) <
                    LOOT_TAKE * LOOT_TAKE) {
                    take_loot(g, c, g->hover_loot);
                    g->has_dest = 0;
                } else {
                    g->has_dest = 1;
                    g->dest_x = g->ground.pile[g->hover_loot].x;
                    g->dest_y = g->ground.pile[g->hover_loot].y;
                }
            } else {
                g->target = -1;
                g->loot_want = -1;
                g->has_dest = 1;
                g->dest_x = wx;
                g->dest_y = wy;
            }
        }
        if (canvas_mouse_pressed(c, 2) && g->hover_loot >= 0 && !g->drag_src) {
            int bslot;
            if (!bar_hit(canvas_width(c), canvas_height(c), (float)canvas_mouse_x(c), (float)canvas_mouse_y(c),
                         &bslot))
                take_loot(g, c, g->hover_loot);
        }
        if (canvas_key_pressed(c, KEY_SPACE)) {
            li = loot_at(g, g->spr.x, g->spr.y);
            if (li >= 0 && dist2(g->spr.x, g->spr.y, g->ground.pile[li].x, g->ground.pile[li].y) < LOOT_TAKE * LOOT_TAKE)
                take_loot(g, c, li);
            else {
                pi = rpg_place_near(&g->world, g->spr.x, g->spr.y, 28.0f);
                if (pi >= 0)
                    interact_place(g, c, pi);
            }
        }
    } else {
        float ox = (float)canvas_width(c) * 0.5f - 210.0f;
        float oy = (float)canvas_height(c) * 0.5f - 120.0f;
        int slot, gi, vi;
        float mx = (float)canvas_mouse_x(c), my = (float)canvas_mouse_y(c);

        if (canvas_mouse_pressed(c, 1) && g->ui == UI_INV) {
            float px, py;
            sheet_origin(canvas_width(c), canvas_height(c), &px, &py);
            if (hit_wear(mx, my, px, py, &slot)) {
                rpg_unequip(&g->pc.inv, slot);
                refresh(g);
            }
        } else if (canvas_mouse_pressed(c, 1) && g->ui == UI_VENDOR) {
            ox = (float)canvas_width(c) * 0.5f - 180.0f;
            oy = (float)canvas_height(c) * 0.5f - 140.0f;
            vi = (int)((mx - ox) / 36.0f);
            if (my >= oy && my < oy + 34.0f && vi >= 0 && vi < RPG_VENDOR_N)
                rpg_buy(&g->pc, g->world.vendor, RPG_VENDOR_N, vi);
            else if (inv_hit_grid(mx, my, ox, oy + 56.0f, &gi))
                rpg_sell(&g->pc, gi);
        } else if (canvas_mouse_pressed(c, 1) && g->ui == UI_BANK) {
            ox = (float)canvas_width(c) * 0.5f - 200.0f;
            oy = (float)canvas_height(c) * 0.5f - 150.0f;
            if (inv_hit_grid(mx, my, ox, oy, &gi))
                rpg_inv_move(&g->world.bank, gi, &g->pc.inv);
            else if (inv_hit_grid(mx, my, ox, oy + 160.0f, &gi))
                rpg_inv_move(&g->pc.inv, gi, &g->world.bank);
        }
        if (canvas_key_pressed(c, KEY_SPACE))
            g->ui = UI_NONE;
    }

    g->attack_cd -= dt;
    move_hero(g, dt);
    if (!rpg_zone_safe(g->world.zone)) {
        hero_swing(g, c);
        mob_ai(g, c, dt);
    }

    li = (g->loot_want >= 0 && g->ground.pile[g->loot_want].on) ? g->loot_want : loot_at(g, g->spr.x, g->spr.y);
    if (li >= 0 && dist2(g->spr.x, g->spr.y, g->ground.pile[li].x, g->ground.pile[li].y) < LOOT_TAKE * LOOT_TAKE)
        take_loot(g, c, li);

    if (g->note_t > 0.0f)
        g->note_t -= dt;

    moving = g->has_dest && (g->spr.vx * g->spr.vx + g->spr.vy * g->spr.vy) > 64.0f;
    g->spr.fps = 0.0f;
    if (moving && g->spr.frames > 1)
        g->spr.frame = (int)(g->walk_gait * (float)g->spr.frames) % g->spr.frames;
    else {
        g->spr.frame = 0;
        if (!moving)
            g->walk_gait = 0.0f;
    }
    if (moving) {
        int beat = (int)(g->walk_gait * 2.0f);
        if (beat != g->walk_beat) {
            g->walk_beat = beat;
            canvas_sound_play(c, g->snd_step, 0.24f);
        }
    } else {
        g->walk_beat = -1;
        g->step_t = 0.0f;
    }
    /* Position already moved in dungeon_slide; sprite_update would apply vx again. */
    {
        float svx = g->spr.vx, svy = g->spr.vy;
        g->spr.vx = g->spr.vy = 0;
        sprite_update(&g->spr, dt);
        g->spr.vx = svx;
        g->spr.vy = svy;
    }
    reveal(g);
    g->bob += dt;

    for (i = 0; i < FX_MAX; i++) {
        if (!g->fx[i].on)
            continue;
        g->fx[i].t -= dt;
        g->fx[i].y -= 28.0f * dt;
        if (g->fx[i].t <= 0.0f)
            g->fx[i].on = 0;
    }

    if (canvas_wheel(c) != 0.0f) {
        float z = canvas_cam_zoom_get(c) + canvas_wheel(c) * 0.1f;
        canvas_cam_zoom(c, clampf(z, 0.7f, 2.2f));
    }
    canvas_cam_follow(c, g->spr.x - canvas_width(c) / (2.0f * canvas_cam_zoom_get(c)),
                      g->spr.y - canvas_height(c) / (2.0f * canvas_cam_zoom_get(c)), 4.8f);
}

static int item_icon(const RpgItem *it)
{
    /* Flare fantasycore icons.png, 64px, icon_set starts at 0. */
    switch (it->kind) {
    case IT_WEP:
        return 97; /* shortsword */
    case IT_OFF:
        return 120; /* wood shield */
    case IT_HELM:
        return 144; /* leather hood */
    case IT_ARM:
        return 145; /* leather chest */
    case IT_RING:
        return 199;
    case IT_HPOT:
        return 64;
    case IT_MPOT:
        return 65;
    default:
        return -1;
    }
}

static void draw_item_swatch(Canvas *c, const Arpg *g, const RpgItem *it, float x, float y, float s)
{
    float r, gv, b;
    int ic;

    if (!rpg_item_ok(it)) {
        canvas_stroke_rect(c, x, y, s, s, 0.25f, 0.22f, 0.2f, 0.8f);
        return;
    }
    rarity_rgb(it->rarity, &r, &gv, &b);
    canvas_stroke_rect(c, x, y, s, s, r, gv, b, 0.9f);
    ic = item_icon(it);
    if (g && g->icons.texture && ic >= 0) {
        canvas_draw_sheet(c, &g->icons, ic, x + 2.0f, y + 2.0f, s - 4.0f, s - 4.0f);
        return;
    }
    if (it->kind == IT_HPOT) {
        r = 0.85f;
        gv = 0.18f;
        b = 0.16f;
    }
    if (it->kind == IT_MPOT) {
        r = 0.25f;
        gv = 0.40f;
        b = 0.95f;
    }
    canvas_fill_rect(c, x + 3, y + 3, s - 6, s - 6, r, gv, b, 1.0f);
}

static void render_world(Arpg *g, Canvas *c)
{
    float vx, vy, vw, vh;
    int x0, y0, x1, y1, tx, ty, i;
    const CryptSpecies *sp;

    if (g->world.zone == RPG_ZONE_TOWN)
        canvas_clear(c, 0.11f, 0.13f, 0.09f);
    else if (g->world.zone == RPG_ZONE_OVERWORLD)
        canvas_clear(c, 0.07f, 0.12f, 0.06f);
    else
        canvas_clear(c, 0.02f, 0.02f, 0.03f);
    canvas_view(c, &vx, &vy, &vw, &vh);
    x0 = (int)(vx / TILE) - 1;
    y0 = (int)(vy / TILE) - 1;
    x1 = (int)((vx + vw) / TILE) + 1;
    y1 = (int)((vy + vh) / TILE) + 1;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= DUN_W)
        x1 = DUN_W - 1;
    if (y1 >= DUN_H)
        y1 = DUN_H - 1;

    for (ty = y0; ty <= y1; ty++) {
        for (tx = x0; tx <= x1; tx++) {
            int floor, wall_edge, n, cell;
            const Sheet *ts;
            if (!tile_seen(g, tx, ty))
                continue;
            floor = dungeon_walk(cmap(g), tx, ty);
            wall_edge = 0;
            if (!floor) {
                for (n = 0; n < 4; n++) {
                    static const int ox[4] = { 1, -1, 0, 0 };
                    static const int oy[4] = { 0, 0, 1, -1 };
                    if (dungeon_walk(cmap(g), tx + ox[n], ty + oy[n]))
                        wall_edge = 1;
                }
            }
            ts = zone_tiles(g);
            n = canvas_sheet_count(ts);
            if (floor)
                cell = (n > 2) ? ((tx * 3 + ty * 7) & 1) : 0;
            else if (wall_edge)
                cell = (n > 2) ? 2 : 1;
            else
                cell = -1;
            if (cell >= 0)
                canvas_draw_sheet(c, ts, cell, tx * TILE, ty * TILE, TILE + 1.0f, TILE + 1.0f);
            if (!lit_tile(g, tx, ty))
                canvas_fill_rect(c, tx * TILE, ty * TILE, TILE, TILE, 0, 0, 0, 0.55f);
        }
    }

    for (i = 0; i < g->world.place_n[g->world.zone]; i++) {
        RpgPlace *p = &g->world.places[g->world.zone][i];
        float px, py, r = 0.55f, gg = 0.45f, b = 0.2f;
        if (!tile_seen(g, p->tx, p->ty))
            continue;
        place_xy(p, &px, &py);
        if (p->kind == RPG_PLACE_VENDOR) {
            r = 0.25f;
            gg = 0.7f;
            b = 0.28f;
        } else if (p->kind == RPG_PLACE_BANK) {
            r = 0.9f;
            gg = 0.75f;
            b = 0.2f;
        } else if (p->kind == RPG_PLACE_QUEST) {
            r = 0.4f;
            gg = 0.55f;
            b = 0.95f;
        } else if (p->kind == RPG_PLACE_GATE) {
            r = 0.55f;
            gg = 0.4f;
            b = 0.22f;
        } else if (p->kind == RPG_PLACE_PORTAL) {
            r = 0.62f;
            gg = 0.28f;
            b = 0.78f;
        } else if (p->kind == RPG_PLACE_EXIT) {
            r = 0.35f;
            gg = 0.7f;
            b = 0.85f;
        }
        canvas_fill_rect(c, px - 10, py - 10, 20, 20, r, gg, b, 1.0f);
        canvas_stroke_rect(c, px - 12, py - 12, 24, 24, r, gg, b, 1.0f);
        if (g->portal && (p->kind == RPG_PLACE_PORTAL || p->kind == RPG_PLACE_GATE || p->kind == RPG_PLACE_EXIT ||
                          p->kind == RPG_PLACE_STAIRS))
            canvas_blit(c, g->portal, px - 16, py - 16, 32, 32, 0, 0, 1, 1, r, gg, b, 1.0f);
    }

    for (i = 0; i < BLOOD_MAX; i++) {
        if (!g->blood[i].on)
            continue;
        canvas_fill_rect(c, g->blood[i].x, g->blood[i].y, 7, 5, 0.35f, 0.05f, 0.05f, g->blood[i].a);
    }
    for (i = 0; i < RPG_LOOT_MAX; i++) {
        float r, gg, b, bob;
        if (!g->ground.pile[i].on)
            continue;
        bob = sinf(g->bob * 3.0f + (float)i) * 2.0f;
        if (rpg_item_ok(&g->ground.pile[i].item))
            rarity_rgb(g->ground.pile[i].item.rarity, &r, &gg, &b);
        else {
            r = 1.0f;
            gg = 0.84f;
            b = 0.2f;
        }
        if (rpg_item_ok(&g->ground.pile[i].item))
            draw_item_swatch(c, g, &g->ground.pile[i].item, g->ground.pile[i].x - 12,
                             g->ground.pile[i].y - 12 + bob, 24.0f);
        else {
            canvas_fill_rect(c, g->ground.pile[i].x - 8, g->ground.pile[i].y - 8 + bob, 16, 16, r, gg, b, 1.0f);
            canvas_stroke_rect(c, g->ground.pile[i].x - 9, g->ground.pile[i].y - 9 + bob, 18, 18, r, gg, b, 0.95f);
        }
        if (dist2(g->spr.x, g->spr.y, g->ground.pile[i].x, g->ground.pile[i].y) < 110.0f * 110.0f) {
            const char *nm = rpg_item_ok(&g->ground.pile[i].item) ? g->ground.pile[i].item.name : "Gold";
            canvas_draw_text(c, g->ground.pile[i].x - 24, g->ground.pile[i].y - 14 + bob, nm, r, gg, b);
        }
    }
    for (i = 0; i < g->mob_n; i++) {
        float w, h, r, gg, b, bob, lean, lx, ly, step;
        RpgActor *m = &g->mobs[i];
        if (!m->alive)
            continue;
        sp = &crypt_species[m->kind];
        w = sp->radius * 1.6f;
        h = sp->radius * 2.1f;
        r = sp->r;
        gg = sp->g;
        b = sp->b;
        if (m->hurt > 0.0f) {
            r = 1.0f;
            gg = 1.0f;
            b = 1.0f;
        }
        bob = m->ai == RPG_AI_CHASE || m->ai == RPG_AI_FLEE ? gait_bob(m->gait) * 2.2f : 0.0f;
        lean = (float)(m->face ? m->face : 1) * (m->vx != 0.0f || m->vy != 0.0f ? 2.4f : 0.0f);
        lx = m->x - w * 0.5f + lean;
        ly = m->y - h - bob;
        canvas_fill_rect(c, m->x - w * 0.32f, m->y - 3.0f, w * 0.64f, 4.0f, 0, 0, 0, 0.32f);
        canvas_fill_rect(c, lx, ly, w, h, r, gg, b, 1.0f);
        if (m->vx != 0.0f || m->vy != 0.0f) {
            step = m->gait < 0.5f ? -1.0f : 1.0f;
            canvas_fill_rect(c, m->x + step * (w * 0.22f) - 2.0f, m->y - 7.0f + bob * 0.25f, 5.0f, 8.0f,
                             r * 0.55f, gg * 0.55f, b * 0.55f, 1.0f);
        }
        if (m->role == RPG_ROLE_BOSS)
            canvas_stroke_rect(c, lx - 3, ly - 3, w + 6, h + 6, 0.95f, 0.45f, 0.12f, 1.0f);
        else if (m->role == RPG_ROLE_CHAMPION)
            canvas_stroke_rect(c, lx - 2, ly - 2, w + 4, h + 4, 1.0f, 0.82f, 0.2f, 1.0f);
        if (g->target == i)
            canvas_stroke_rect(c, lx - 1, ly - 1, w + 2, h + 2, 0.9f, 0.2f, 0.15f, 1.0f);
        {
            float hp = rpg_get(&m->st, ST_HPMAX)
                           ? (float)rpg_get(&m->st, ST_HP) / (float)rpg_get(&m->st, ST_HPMAX)
                           : 0;
            canvas_fill_rect(c, m->x - 12, ly - 6, 24, 3, 0.15f, 0.05f, 0.05f, 0.9f);
            canvas_fill_rect(c, m->x - 12, ly - 6, 24.0f * clampf(hp, 0, 1), 3, 0.75f, 0.12f, 0.1f, 1.0f);
        }
    }
    {
        Sprite vis = g->spr;
        float bob = (g->spr.vx != 0.0f || g->spr.vy != 0.0f) ? gait_bob(g->walk_gait) * 1.7f : 0.0f;
        vis.y -= bob;
        vis.scale_y = (g->spr.vx != 0.0f || g->spr.vy != 0.0f) ? 1.0f + gait_bob(g->walk_gait) * 0.045f : 1.0f;
        canvas_draw_sprite(c, &vis);
    }
    if (g->has_dest)
        canvas_stroke_rect(c, g->dest_x - 4, g->dest_y - 4, 8, 8, 0.9f, 0.85f, 0.4f, 0.7f);
    for (i = 0; i < FX_MAX; i++) {
        char n[16];
        if (!g->fx[i].on)
            continue;
        if (g->fx[i].dmg <= 0)
            snprintf(n, sizeof(n), "miss");
        else
            snprintf(n, sizeof(n), "%s%d", g->fx[i].crit ? "*" : "", g->fx[i].dmg);
        canvas_draw_text(c, g->fx[i].x - 8, g->fx[i].y, n, g->fx[i].r, g->fx[i].g, g->fx[i].b);
    }
}

static void bar(Canvas *c, float x, float y, float w, float h, float t, float r, float g, float b, const char *label)
{
    canvas_fill_rect(c, x, y, w, h, 0.10f, 0.10f, 0.10f, 0.92f);
    if (t > 0.002f)
        canvas_fill_rect(c, x + 1, y + 1, (w - 2) * clampf(t, 0, 1), h - 2, r, g, b, 1.0f);
    canvas_stroke_rect(c, x, y, w, h, 0.40f, 0.40f, 0.38f, 1.0f);
    if (label)
        canvas_draw_text(c, x + 6, y + h - 4, label, 0.97f, 0.98f, 0.95f);
}

static void render_hud(Arpg *g, Canvas *c)
{
    int W = canvas_width(c), H = canvas_height(c), tx, ty;
    char line[128];
    float hp = LS(g, ST_HPMAX) ? (float)LS(g, ST_HP) / (float)LS(g, ST_HPMAX) : 0;
    float mp = LS(g, ST_MPMAX) ? (float)LS(g, ST_MP) / (float)LS(g, ST_MPMAX) : 0;
    float xp = LS(g, ST_NEXT) ? (float)LS(g, ST_XP) / (float)LS(g, ST_NEXT) : 0;
    int mx = canvas_mouse_x(c), my = canvas_mouse_y(c);

    canvas_begin_hud(c);
    canvas_fill_rect(c, 0, 0, (float)W, 26, 0, 0, 0, 0.5f);
    snprintf(line, sizeof(line), "%s   Lv %d   %d gold   %d foes", crypt_zone_title(&g->world), LS(g, ST_LV),
             LS(g, ST_GOLD), g->mob_n);
    if (g->world.zone == RPG_ZONE_DUNGEON) {
        snprintf(line, sizeof(line), "%s  depth %d   Lv %d   %d gold", crypt_zone_title(&g->world), g->world.depth,
                 LS(g, ST_LV), LS(g, ST_GOLD));
        for (tx = 0; tx < g->world.place_n[RPG_ZONE_OVERWORLD]; tx++) {
            RpgPlace *p = &g->world.places[RPG_ZONE_OVERWORLD][tx];
            if (p->kind == RPG_PLACE_PORTAL && p->dest_id == g->world.dungeon_id)
                snprintf(line, sizeof(line), "%s  depth %d   Lv %d   %d gold", p->name, g->world.depth, LS(g, ST_LV),
                         LS(g, ST_GOLD));
        }
    }
    canvas_draw_text(c, 10, 18, line, 0.9f, 0.88f, 0.8f);

    {
        char hp_s[32], mp_s[32], xp_s[32];
        snprintf(hp_s, sizeof(hp_s), "HP  %d / %d", LS(g, ST_HP), LS(g, ST_HPMAX));
        snprintf(mp_s, sizeof(mp_s), "MP  %d / %d", LS(g, ST_MP), LS(g, ST_MPMAX));
        snprintf(xp_s, sizeof(xp_s), "XP  %d / %d", LS(g, ST_XP), LS(g, ST_NEXT));
        bar(c, 12, (float)H - 58, 240, 16, hp, 0.22f, 0.78f, 0.28f, hp_s);
        bar(c, 12, (float)H - 38, 240, 14, mp, 0.25f, 0.45f, 0.95f, mp_s);
        bar(c, 12, (float)H - 20, 240, 10, xp, 0.82f, 0.70f, 0.18f, xp_s);
    }
    {
        float bx, by, sx;
        int i, gi, nstk, hi;
        RpgItem it;
        RpgBarSlot *s;
        char n[12];

        bar_origin(W, H, &bx, &by);
        canvas_fill_rect(c, bx - 8, by - 10, RPG_BAR_N * (BAR_CELL + BAR_GAP) - BAR_GAP + 16, BAR_CELL + 24, 0.04f,
                         0.03f, 0.03f, 0.78f);
        for (i = 0; i < RPG_BAR_N; i++) {
            sx = bx + (float)i * (BAR_CELL + BAR_GAP);
            s = &g->pc.bar.slot[i];
            canvas_fill_rect(c, sx, by, BAR_CELL, BAR_CELL, 0.13f, 0.12f, 0.11f, 0.94f);
            hi = hit_rect((float)mx, (float)my, sx, by, BAR_CELL, BAR_CELL);
            canvas_stroke_rect(c, sx, by, BAR_CELL, BAR_CELL, hi ? 0.95f : 0.48f, hi ? 0.82f : 0.4f, hi ? 0.4f : 0.28f,
                               1.0f);
            memset(&it, 0, sizeof(it));
            if (s->type == RPG_BAR_ITEM) {
                gi = rpg_inv_find(&g->pc.inv, s->id);
                nstk = rpg_inv_count(&g->pc.inv, s->id);
                if (gi >= 0)
                    it = g->pc.inv.grid[gi];
                else
                    it.kind = s->id;
                draw_item_swatch(c, g, &it, sx + 3, by + 3, BAR_CELL - 6);
                if (nstk > 1) {
                    snprintf(n, sizeof(n), "%d", nstk);
                    canvas_draw_text(c, sx + 22, by + BAR_CELL - 3, n, 1, 1, 1);
                } else if (nstk == 0)
                    canvas_fill_rect(c, sx, by, BAR_CELL, BAR_CELL, 0, 0, 0, 0.35f);
            } else if (s->type == RPG_BAR_SKILL) {
                draw_skill_mark(c, sx + 3, by + 3, BAR_CELL - 6);
            }
            snprintf(n, sizeof(n), "%d", i + 1);
            canvas_draw_text(c, sx + 3, by + 11, n, 0.9f, 0.85f, 0.7f);
        }
    }
    snprintf(line, sizeof(line), "%d-%d dmg   armor %d   STR %d  DEX %d  MAG %d  VIT %d", LS(g, ST_DMIN),
             LS(g, ST_DMAX), LS(g, ST_ARMOR), LS(g, ST_STR), LS(g, ST_DEX), LS(g, ST_MAG), LS(g, ST_VIT));
    canvas_draw_text(c, 520, (float)H - 28, line, 0.85f, 0.85f, 0.8f);
    canvas_draw_text(c, 520, (float)H - 12, "1-5 use   I bag   drag onto 1-5", 0.55f, 0.55f, 0.5f);
    if (g->note_t > 0.0f)
        canvas_draw_text(c, (float)W * 0.5f - 80, (float)H - 72, g->note, 0.95f, 0.9f, 0.55f);
    {
        int near = rpg_place_near(&g->world, g->spr.x, g->spr.y, 28.0f);
        if (near >= 0 && g->ui == UI_NONE)
            canvas_draw_text(c, (float)W * 0.5f - 110, 48, g->world.places[g->world.zone][near].prompt, 0.95f, 0.82f,
                             0.35f);
    }

    /* minimap */
    {
        float ox = (float)W - 132, oy = 32, s = 1.7f;
        canvas_fill_rect(c, ox - 4, oy - 4, DUN_W * s + 8, DUN_H * s + 8, 0, 0, 0, 0.65f);
        for (ty = 0; ty < DUN_H; ty++) {
            for (tx = 0; tx < DUN_W; tx++) {
                if (!tile_seen(g, tx, ty) || !dungeon_walk(cmap(g), tx, ty))
                    continue;
                if (g->world.zone == RPG_ZONE_TOWN)
                    canvas_fill_rect(c, ox + tx * s, oy + ty * s, s, s, 0.35f, 0.32f, 0.22f, 0.9f);
                else if (g->world.zone == RPG_ZONE_OVERWORLD)
                    canvas_fill_rect(c, ox + tx * s, oy + ty * s, s, s, 0.22f, 0.38f, 0.18f, 0.9f);
                else
                    canvas_fill_rect(c, ox + tx * s, oy + ty * s, s, s, 0.28f, 0.22f, 0.18f, 0.9f);
            }
        }
        canvas_fill_rect(c, ox + g->spr.x / TILE * s - 1, oy + g->spr.y / TILE * s - 1, 3, 3, 0.95f, 0.9f, 0.4f, 1);
        for (tx = 0; tx < g->world.place_n[g->world.zone]; tx++) {
            RpgPlace *p = &g->world.places[g->world.zone][tx];
            float r = 0.7f, gg = 0.5f, b = 0.9f;
            if (p->kind == RPG_PLACE_GATE) {
                r = 0.9f;
                gg = 0.7f;
                b = 0.3f;
            }
            if (p->kind == RPG_PLACE_PORTAL) {
                r = 0.7f;
                gg = 0.3f;
                b = 0.85f;
            }
            canvas_fill_rect(c, ox + p->tx * s, oy + p->ty * s, 2, 2, r, gg, b, 1);
        }
    }

    if (g->hover_loot >= 0 && g->ground.pile[g->hover_loot].on) {
        RpgLoot *L = &g->ground.pile[g->hover_loot];
        float r, gg, b;
        if (rpg_item_ok(&L->item)) {
            rarity_rgb(L->item.rarity, &r, &gg, &b);
            snprintf(line, sizeof(line), "%s   click to take", L->item.name);
        } else {
            r = gg = 0.9f;
            b = 0.3f;
            snprintf(line, sizeof(line), "%d gold   click to take", L->gold);
        }
        canvas_draw_text(c, (float)mx + 14, (float)my - 8, line, r, gg, b);
    } else if (g->hover_mob >= 0 && g->mobs[g->hover_mob].alive) {
        RpgActor *m = &g->mobs[g->hover_mob];
        const char *role = rpg_role_name(m->role);
        snprintf(line, sizeof(line), "%s%s%s  %d/%d", role[0] ? role : "", role[0] ? " " : "",
                 crypt_species[m->kind].name, MS(m, ST_HP), MS(m, ST_HPMAX));
        canvas_draw_text(c, (float)mx + 14, (float)my - 8, line, 0.95f, 0.55f, 0.45f);
    } else if (g->hover_place >= 0) {
        RpgPlace *p = &g->world.places[g->world.zone][g->hover_place];
        canvas_draw_text(c, (float)mx + 14, (float)my - 8, p->name, 0.9f, 0.82f, 0.6f);
    }

    if (g->ui == UI_INV) {
        float px, py, bagx, bagy, bookx, booky, sx, sy;
        int i, slot, gi, si;

        sheet_origin(W, H, &px, &py);
        bagx = px + 250.0f;
        bagy = py + 48.0f;
        book_origin(px, py, &bookx, &booky);
        canvas_fill_rect(c, px - 16, py - 16, 660, 400, 0.06f, 0.05f, 0.04f, 0.94f);
        canvas_stroke_rect(c, px - 16, py - 16, 660, 400, 0.55f, 0.42f, 0.22f, 1.0f);
        canvas_draw_text(c, px, py - 2, "Character", 0.92f, 0.85f, 0.55f);
        canvas_draw_text(c, bagx, py - 2, "Bag    click to use    drag to 1-5", 0.92f, 0.85f, 0.55f);
        snprintf(line, sizeof(line), "Lv %d   STR %d  DEX %d  MAG %d  VIT %d", LS(g, ST_LV), LS(g, ST_STR), LS(g, ST_DEX),
                 LS(g, ST_MAG), LS(g, ST_VIT));
        canvas_draw_text(c, px, py + 20, line, 0.82f, 0.8f, 0.72f);
        snprintf(line, sizeof(line), "HP %d/%d   MP %d/%d   %d-%d dmg   armor %d", LS(g, ST_HP), LS(g, ST_HPMAX),
                 LS(g, ST_MP), LS(g, ST_MPMAX), LS(g, ST_DMIN), LS(g, ST_DMAX), LS(g, ST_ARMOR));
        canvas_draw_text(c, px, py + 38, line, 0.7f, 0.78f, 0.62f);
        if (g->portrait)
            canvas_blit(c, g->portrait, px, py + 56, 72, 72, 0, 0, 1, 1, 1, 1, 1, 1);
        for (slot = 0; slot < rpg_slot_n(); slot++) {
            wear_xy(px, py, slot, &sx, &sy);
            draw_item_swatch(c, g, &g->pc.inv.wear[slot], sx, sy, WEAR_CELL);
            canvas_draw_text(c, sx, sy + WEAR_CELL + 12, rpg_slot_name(slot), 0.55f, 0.5f, 0.45f);
        }
        canvas_draw_text(c, px, py + 230, "click a worn slot to remove it", 0.5f, 0.48f, 0.42f);
        for (i = 0; i < rpg_inv_n(); i++) {
            int col = i % rpg_inv_w(), row = i / rpg_inv_w();
            sx = bagx + col * BAG_CELL;
            sy = bagy + row * BAG_CELL;
            draw_item_swatch(c, g, &g->pc.inv.grid[i], sx, sy, 34);
            if (g->pc.inv.grid[i].stack > 1) {
                snprintf(line, sizeof(line), "%d", g->pc.inv.grid[i].stack);
                canvas_draw_text(c, sx + 18, sy + 30, line, 1, 1, 1);
            }
        }
        canvas_draw_text(c, bookx, booky - 16,
                         rpg_book_n(&g->pc.build.skills) ? "Spells    drag to 1-5" : "Spells    none yet    drag to 1-5 later",
                         0.92f, 0.85f, 0.55f);
        for (i = 0; i < BOOK_W * BOOK_H; i++) {
            int col = i % BOOK_W, row = i / BOOK_W;
            sx = bookx + col * BAG_CELL;
            sy = booky + row * BAG_CELL;
            canvas_fill_rect(c, sx, sy, 34, 34, 0.12f, 0.11f, 0.14f, 0.9f);
            canvas_stroke_rect(c, sx, sy, 34, 34, 0.32f, 0.28f, 0.4f, 0.8f);
            if (rpg_book_id(&g->pc.build.skills, i))
                draw_skill_mark(c, sx, sy, 34);
        }
        {
            float r, gg, b;
            if (hit_wear((float)mx, (float)my, px, py, &slot) && rpg_item_ok(&g->pc.inv.wear[slot])) {
                rarity_rgb(g->pc.inv.wear[slot].rarity, &r, &gg, &b);
                rpg_item_desc(&g->pc.inv.wear[slot], line, sizeof(line));
                canvas_draw_text(c, px, py + 360, line, r, gg, b);
            } else if (inv_hit_grid((float)mx, (float)my, bagx, bagy, &gi) && rpg_item_ok(&g->pc.inv.grid[gi])) {
                rarity_rgb(g->pc.inv.grid[gi].rarity, &r, &gg, &b);
                rpg_item_desc(&g->pc.inv.grid[gi], line, sizeof(line));
                canvas_draw_text(c, px, py + 360, line, r, gg, b);
            } else if (book_hit((float)mx, (float)my, bookx, booky, &si) && rpg_book_id(&g->pc.build.skills, si)) {
                canvas_draw_text(c, px, py + 360, "Spell    drag onto 1-5", 0.72f, 0.62f, 0.88f);
            }
        }
    }

    if (g->ui == UI_VENDOR) {
        float ox = (float)W * 0.5f - 180.0f, oy = (float)H * 0.5f - 140.0f;
        int i, gi;
        canvas_fill_rect(c, ox - 24, oy - 36, 420, 280, 0.06f, 0.05f, 0.04f, 0.93f);
        canvas_stroke_rect(c, ox - 24, oy - 36, 420, 280, 0.35f, 0.55f, 0.3f, 1.0f);
        snprintf(line, sizeof(line), "Trader    %d gold    click stock to buy, pack to sell", LS(g, ST_GOLD));
        canvas_draw_text(c, ox - 8, oy - 16, line, 0.85f, 0.9f, 0.7f);
        for (i = 0; i < RPG_VENDOR_N; i++)
            draw_item_swatch(c, g, &g->world.vendor[i], ox + i * 36.0f, oy, 34);
        for (i = 0; i < rpg_inv_n(); i++) {
            int col = i % rpg_inv_w(), row = i / rpg_inv_w();
            draw_item_swatch(c, g, &g->pc.inv.grid[i], ox + col * 36.0f, oy + 56.0f + row * 36.0f, 34);
        }
        canvas_draw_text(c, ox, oy + 220, "Space closes", 0.5f, 0.48f, 0.42f);
        gi = (int)(((float)mx - ox) / 36.0f);
        if ((float)my >= oy && (float)my < oy + 34.0f && gi >= 0 && gi < RPG_VENDOR_N &&
            rpg_item_ok(&g->world.vendor[gi])) {
            RpgItem *it = &g->world.vendor[gi];
            snprintf(line, sizeof(line), "%s   buy %d gold", it->name, rpg_item_price(it));
            canvas_draw_text(c, ox, oy + 238, line, 0.9f, 0.85f, 0.5f);
        } else if (inv_hit_grid((float)mx, (float)my, ox, oy + 56.0f, &gi) && rpg_item_ok(&g->pc.inv.grid[gi])) {
            snprintf(line, sizeof(line), "%s   sell %d gold", g->pc.inv.grid[gi].name, rpg_item_value(&g->pc.inv.grid[gi]));
            canvas_draw_text(c, ox, oy + 238, line, 0.85f, 0.8f, 0.55f);
        }
    }

    if (g->ui == UI_BANK) {
        float ox = (float)W * 0.5f - 200.0f, oy = (float)H * 0.5f - 150.0f;
        int i;
        canvas_fill_rect(c, ox - 20, oy - 32, 420, 360, 0.06f, 0.05f, 0.04f, 0.93f);
        canvas_stroke_rect(c, ox - 20, oy - 32, 420, 360, 0.75f, 0.62f, 0.25f, 1.0f);
        canvas_draw_text(c, ox, oy - 14, "Stash    click to move between stash and pack", 0.9f, 0.82f, 0.5f);
        for (i = 0; i < rpg_inv_n(); i++) {
            int col = i % rpg_inv_w(), row = i / rpg_inv_w();
            draw_item_swatch(c, g, &g->world.bank.grid[i], ox + col * 36.0f, oy + row * 36.0f, 34);
            draw_item_swatch(c, g, &g->pc.inv.grid[i], ox + col * 36.0f, oy + 160.0f + row * 36.0f, 34);
        }
        canvas_draw_text(c, ox, oy + 148, "pack", 0.55f, 0.5f, 0.45f);
        canvas_draw_text(c, ox, oy + 310, "Space closes", 0.5f, 0.48f, 0.42f);
    }

    if (g->ui == UI_QUEST) {
        float ox = (float)W * 0.5f - 200.0f, oy = (float)H * 0.5f - 80.0f;
        canvas_fill_rect(c, ox, oy, 400, 140, 0.06f, 0.05f, 0.08f, 0.93f);
        canvas_stroke_rect(c, ox, oy, 400, 140, 0.4f, 0.5f, 0.85f, 1.0f);
        canvas_draw_text(c, ox + 16, oy + 28, "Notice board", 0.75f, 0.8f, 1.0f);
        canvas_draw_text(c, ox + 16, oy + 56, "No contracts posted. Hunters will pin work here later.", 0.8f, 0.78f,
                         0.7f);
        canvas_draw_text(c, ox + 16, oy + 110, "Space closes", 0.5f, 0.48f, 0.42f);
    }

    if (g->dead) {
        canvas_fill_rect(c, 0, 0, (float)W, (float)H, 0.15f, 0, 0, 0.45f);
        canvas_draw_text(c, (float)W * 0.5f - 90, (float)H * 0.5f, "You have died", 0.95f, 0.75f, 0.7f);
        canvas_draw_text(c, (float)W * 0.5f - 140, (float)H * 0.5f + 22, "click or R — revive in Haven (-10% gold)",
                         0.8f, 0.7f, 0.65f);
    }
    if (g->quit_ask) {
        float x, y, w, h, stay_x, quit_x, by;

        quit_panel(W, H, &x, &y, &w, &h);
        quit_btns(W, H, &stay_x, &quit_x, &by);
        canvas_fill_rect(c, 0, 0, (float)W, (float)H, 0, 0, 0, 0.45f);
        canvas_fill_rect(c, x, y, w, h, 0.08f, 0.07f, 0.06f, 0.96f);
        canvas_stroke_rect(c, x, y, w, h, 0.55f, 0.42f, 0.22f, 1.0f);
        canvas_draw_text(c, x + 28, y + 28, "Quit Crypt?", 0.95f, 0.9f, 0.7f);
        canvas_draw_text(c, x + 28, y + 48, "Esc again or Y quits.  N stays.", 0.65f, 0.62f, 0.55f);
        canvas_fill_rect(c, stay_x, by, QUIT_BW, QUIT_BH, 0.18f, 0.18f, 0.16f, 1.0f);
        canvas_fill_rect(c, quit_x, by, QUIT_BW, QUIT_BH, 0.42f, 0.16f, 0.12f, 1.0f);
        if (hit_rect((float)mx, (float)my, stay_x, by, QUIT_BW, QUIT_BH))
            canvas_stroke_rect(c, stay_x, by, QUIT_BW, QUIT_BH, 0.9f, 0.85f, 0.55f, 1.0f);
        else
            canvas_stroke_rect(c, stay_x, by, QUIT_BW, QUIT_BH, 0.4f, 0.38f, 0.32f, 1.0f);
        if (hit_rect((float)mx, (float)my, quit_x, by, QUIT_BW, QUIT_BH))
            canvas_stroke_rect(c, quit_x, by, QUIT_BW, QUIT_BH, 0.95f, 0.55f, 0.4f, 1.0f);
        else
            canvas_stroke_rect(c, quit_x, by, QUIT_BW, QUIT_BH, 0.7f, 0.28f, 0.2f, 1.0f);
        canvas_draw_text(c, stay_x + 32, by + 20, "Stay", 0.9f, 0.9f, 0.88f);
        canvas_draw_text(c, quit_x + 32, by + 20, "Quit", 0.95f, 0.85f, 0.8f);
    }
    if (g->drag_live) {
        RpgItem ghost;
        RpgBarSlot pay;

        memset(&ghost, 0, sizeof(ghost));
        if (drag_payload(g, &pay) && pay.type == RPG_BAR_SKILL)
            draw_skill_mark(c, (float)mx - 18.0f, (float)my - 18.0f, 36.0f);
        else {
            if (g->drag_src == DRAG_BAG && g->drag_i >= 0 && g->drag_i < rpg_inv_n())
                ghost = g->pc.inv.grid[g->drag_i];
            else if (g->drag_src == DRAG_BAR && g->drag_i >= 0 && g->drag_i < RPG_BAR_N) {
                RpgBarSlot *s = &g->pc.bar.slot[g->drag_i];
                int gi;
                if (s->type == RPG_BAR_ITEM) {
                    gi = rpg_inv_find(&g->pc.inv, s->id);
                    if (gi >= 0)
                        ghost = g->pc.inv.grid[gi];
                    else
                        ghost.kind = s->id;
                }
            }
            if (rpg_item_ok(&ghost) || ghost.kind)
                draw_item_swatch(c, g, &ghost, (float)mx - 18.0f, (float)my - 18.0f, 36.0f);
        }
    }
    canvas_end_hud(c);
}

static void render(void *state, Canvas *c)
{
    Arpg *g = state;
    render_world(g, c);
    render_hud(g, c);
}

static void observe(void *state, Canvas *c, char *buf, size_t n)
{
    Arpg *g = state;
    int i, loot_n = 0, pi;
    const char *place = "";
    char note[80], zone[48], pl[40];

    (void)c;
    if (!buf || n == 0)
        return;
    for (i = 0; i < RPG_LOOT_MAX; i++) {
        if (g->ground.pile[i].on)
            loot_n++;
    }
    pi = rpg_place_near(&g->world, g->spr.x, g->spr.y, 28.0f);
    if (pi >= 0)
        place = g->world.places[g->world.zone][pi].name;
    snprintf(note, sizeof(note), "%s", g->note_t > 0.0f ? g->note : "");
    snprintf(zone, sizeof(zone), "%s", crypt_zone_title(&g->world));
    snprintf(pl, sizeof(pl), "%s", place);
    snprintf(buf, n,
             "{\"hp\":%d,\"hp_max\":%d,\"mp\":%d,\"mp_max\":%d,\"gold\":%d,\"lv\":%d,\"xp\":%d,"
             "\"x\":%.1f,\"y\":%.1f,\"zone\":\"%s\",\"depth\":%d,\"dead\":%d,\"mobs\":%d,\"loot\":%d,"
             "\"ui\":%d,\"target\":%d,\"place\":\"%s\",\"note\":\"%s\"}",
             LS(g, ST_HP), LS(g, ST_HPMAX), LS(g, ST_MP), LS(g, ST_MPMAX), LS(g, ST_GOLD), LS(g, ST_LV),
             LS(g, ST_XP), g->spr.x, g->spr.y, zone, g->world.depth, g->dead, g->mob_n, loot_n, g->ui, g->target,
             pl, note);
}

static void shutdown(void *state, Canvas *c)
{
    (void)c;
    canvas_free(state);
}

CANVAS_EXPORT const Game canvas_game = {
    .name = "Crypt",
    .width = 960,
    .height = 640,
    .init = init,
    .update = update,
    .render = render,
    .shutdown = shutdown,
    .observe = observe,
};

#ifndef CANVAS_PLUGIN
int main(void)
{
    return canvas_run(&canvas_game);
}
#endif
