#include "world.h"

#include <stdio.h>
#include <string.h>

int rpg_zone_safe(int zone)
{
    return zone == RPG_ZONE_TOWN;
}

const char *rpg_zone_name(int zone)
{
    switch (zone) {
    case RPG_ZONE_TOWN:
        return "Town";
    case RPG_ZONE_OVERWORLD:
        return "Overworld";
    case RPG_ZONE_DUNGEON:
        return "Dungeon";
    default:
        return "Unknown";
    }
}

void rpg_world_clear(RpgWorld *w)
{
    memset(w, 0, sizeof(*w));
    w->zone = RPG_ZONE_TOWN;
    w->depth = 1;
    rpg_inv_clear(&w->bank);
}

RpgPlace *rpg_world_add_place(RpgWorld *w, int zone, int kind, int tx, int ty, const char *name, const char *prompt)
{
    RpgPlace *p;

    if (w->place_n[zone] >= RPG_PLACE_MAX)
        return NULL;
    p = &w->places[zone][w->place_n[zone]++];
    memset(p, 0, sizeof(*p));
    p->kind = kind;
    p->tx = tx;
    p->ty = ty;
    snprintf(p->name, sizeof(p->name), "%s", name ? name : "");
    snprintf(p->prompt, sizeof(p->prompt), "%s", prompt ? prompt : "");
    return p;
}

void rpg_world_reveal_all(RpgWorld *w, int zone)
{
    memset(w->seen[zone], 1, sizeof(w->seen[zone]));
}

void rpg_world_reveal_around(RpgWorld *w, int zone, int cx, int cy, int rad)
{
    int dx, dy;

    for (dy = -rad; dy <= rad; dy++) {
        for (dx = -rad; dx <= rad; dx++) {
            int tx = cx + dx, ty = cy + dy;
            if (tx < 0 || ty < 0 || tx >= DUN_W || ty >= DUN_H)
                continue;
            if (dx * dx + dy * dy <= rad * rad)
                w->seen[zone][ty][tx] = 1;
        }
    }
}

void rpg_world_bind_dungeon(RpgWorld *w, int dungeon_id, int depth)
{
    Dungeon *d = &w->maps[RPG_ZONE_DUNGEON];
    const char *name = "Dungeon";
    int i;
    char prompt[48];
    RpgPlace *p;

    w->dungeon_id = dungeon_id;
    w->depth = depth < 1 ? 1 : depth;
    for (i = 0; i < w->place_n[RPG_ZONE_OVERWORLD]; i++) {
        p = &w->places[RPG_ZONE_OVERWORLD][i];
        if (p->kind == RPG_PLACE_PORTAL && p->dest_id == dungeon_id) {
            name = p->name;
            break;
        }
    }
    w->place_n[RPG_ZONE_DUNGEON] = 0;
    p = rpg_world_add_place(w, RPG_ZONE_DUNGEON, RPG_PLACE_EXIT, d->start_tx, d->start_ty, "Exit",
                            "Space — return to the wilds");
    if (p) {
        p->dest_zone = RPG_ZONE_OVERWORLD;
        p->dest_id = dungeon_id;
    }
    snprintf(prompt, sizeof(prompt), "Space — descend %s", name);
    rpg_world_add_place(w, RPG_ZONE_DUNGEON, RPG_PLACE_STAIRS, d->stair_tx, d->stair_ty, "Stairs", prompt);
}

int rpg_place_near(const RpgWorld *w, float x, float y, float rad)
{
    int i, best = -1;
    float bd = rad * rad, d, px, py;
    const RpgPlace *p;

    for (i = 0; i < w->place_n[w->zone]; i++) {
        p = &w->places[w->zone][i];
        dungeon_tile_pos(p->tx, p->ty, &px, &py);
        d = (x - px) * (x - px) + (y - py) * (y - py);
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

RpgPlace *rpg_place_kind(RpgWorld *w, int zone, int kind)
{
    int i;

    for (i = 0; i < w->place_n[zone]; i++) {
        if (w->places[zone][i].kind == kind)
            return &w->places[zone][i];
    }
    return NULL;
}
