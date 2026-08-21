#include "actor.h"

#include <math.h>
#include <string.h>

void rpg_actor_clear(RpgActor *a)
{
    if (!a)
        return;
    memset(a, 0, sizeof(*a));
    a->face = 1;
}

void rpg_actor_feel(RpgActor *a, const RpgActorFeel *f)
{
    if (!a || !f)
        return;
    a->speed = f->speed < 1.0f ? 1.0f : f->speed;
    a->sight = f->sight > 1.0f ? f->sight : 96.0f;
    a->leash = f->leash > 1.0f ? f->leash : a->sight * 1.65f;
    a->range = f->range;
    a->radius = f->radius;
    a->see_walls = f->see_walls ? 1 : 0;
    a->attack_reload = f->attack_reload > 0.0f ? f->attack_reload : 0.7f;
    a->ability_reload = f->ability_reload > 0.0f ? f->ability_reload : 4.0f;
    a->role = f->role;
    a->ability = f->ability;
}

void rpg_actor_note_move(RpgActor *a, float ox, float oy, float dt)
{
    float dist, inv;

    if (!a)
        return;
    inv = dt > 1e-6f ? 1.0f / dt : 0.0f;
    a->vx = (a->x - ox) * inv;
    a->vy = (a->y - oy) * inv;
    dist = sqrtf((a->x - ox) * (a->x - ox) + (a->y - oy) * (a->y - oy));
    if (dist > 0.35f) {
        a->gait += dist / (DUN_TILE * 1.5f);
        while (a->gait >= 1.0f)
            a->gait -= 1.0f;
        if (a->vx < -6.0f)
            a->face = -1;
        if (a->vx > 6.0f)
            a->face = 1;
    } else {
        a->vx = a->vy = 0.0f;
        a->gait = 0.0f;
    }
    if (!a->face)
        a->face = 1;
}

const char *rpg_role_name(int role)
{
    switch (role) {
    case RPG_ROLE_CHAMPION:
        return "Champion";
    case RPG_ROLE_BOSS:
        return "Boss";
    default:
        return "";
    }
}

static void reset_path(RpgActor *a)
{
    a->path_len = 0;
    a->path_i = 0;
    a->goal_tx = a->goal_ty = -1;
}

static void chase_move(RpgActor *a, const Dungeon *d, float hx, float hy, float dt, int see)
{
    float dx, dy, len, wx, wy, vx, vy, rad, budget, step;
    int sx, sy, gx, gy, rebuild;

    rad = a->radius > 1.0f ? a->radius * 0.6f : 6.0f;
    budget = a->speed * dt;
    dx = hx - a->x;
    dy = hy - a->y;
    len = sqrtf(dx * dx + dy * dy);

    if (see && len > 1.0f) {
        reset_path(a);
        step = budget < len ? budget : len;
        dungeon_slide(d, &a->x, &a->y, dx / len * step, dy / len * step, rad);
        return;
    }

    dungeon_pos_tile(a->x, a->y, &sx, &sy);
    dungeon_pos_tile(hx, hy, &gx, &gy);
    rebuild = a->path_len <= 0 || a->path_i >= a->path_len || a->goal_tx != gx || a->goal_ty != gy;
    if (rebuild) {
        a->path_len = dungeon_astar(d, sx, sy, gx, gy, a->path_x, a->path_y, RPG_PATH_MAX);
        a->path_i = 0;
        a->goal_tx = gx;
        a->goal_ty = gy;
    }

    while (budget > 0.4f) {
        if (a->path_i >= a->path_len) {
            if (len > 1.0f) {
                step = budget < len ? budget : len;
                dungeon_slide(d, &a->x, &a->y, dx / len * step, dy / len * step, rad);
            }
            break;
        }
        dungeon_tile_pos(a->path_x[a->path_i], a->path_y[a->path_i], &wx, &wy);
        vx = wx - a->x;
        vy = wy - a->y;
        len = sqrtf(vx * vx + vy * vy);
        if (len < 6.0f) {
            a->path_i++;
            continue;
        }
        step = budget < len ? budget : len;
        dungeon_slide(d, &a->x, &a->y, vx / len * step, vy / len * step, rad);
        break;
    }
}

RpgAiResult rpg_ai_step(RpgActor *a, const Dungeon *d, float hx, float hy, float dt, int target_ok)
{
    RpgAiResult r = { RPG_ACT_NONE, 0 };
    float ox, oy, dx, dy, len, reach;
    int see;

    if (!a || !a->alive || !d)
        return r;
    ox = a->x;
    oy = a->y;
    a->attack_cd -= dt;
    a->ability_cd -= dt;
    a->hurt -= dt;
    if (!target_ok) {
        a->ai = RPG_AI_IDLE;
        reset_path(a);
        rpg_actor_note_move(a, ox, oy, dt);
        return r;
    }

    dx = hx - a->x;
    dy = hy - a->y;
    len = sqrtf(dx * dx + dy * dy);
    see = dungeon_line_clear(d, a->x, a->y, hx, hy);
    reach = a->range > 1.0f ? a->range : 28.0f;

    if (a->ai == RPG_AI_IDLE) {
        if (len <= a->sight && (see || a->see_walls))
            a->ai = RPG_AI_CHASE;
        else {
            rpg_actor_note_move(a, ox, oy, dt);
            return r;
        }
    }

    if (a->ai == RPG_AI_FLEE) {
        if (len > a->leash || len < 1.0f) {
            a->ai = RPG_AI_IDLE;
            reset_path(a);
        } else if (len > 1.0f) {
            dungeon_slide(d, &a->x, &a->y, -dx / len * a->speed * dt, -dy / len * a->speed * dt,
                          a->radius > 1.0f ? a->radius * 0.6f : 6.0f);
        }
        rpg_actor_note_move(a, ox, oy, dt);
        return r;
    }

    if (len > a->leash) {
        a->ai = RPG_AI_IDLE;
        reset_path(a);
        rpg_actor_note_move(a, ox, oy, dt);
        return r;
    }

    if (len <= reach) {
        reset_path(a);
        if (a->ability && a->ability_cd <= 0.0f) {
            a->ability_cd = a->ability_reload;
            r.act = RPG_ACT_ABILITY;
            r.ability = a->ability;
        } else if (a->attack_cd <= 0.0f) {
            a->attack_cd = a->attack_reload;
            r.act = RPG_ACT_MELEE;
        }
        rpg_actor_note_move(a, ox, oy, dt);
        return r;
    }

    chase_move(a, d, hx, hy, dt, see);
    rpg_actor_note_move(a, ox, oy, dt);
    return r;
}
