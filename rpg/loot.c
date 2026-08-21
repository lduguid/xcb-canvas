#include "loot.h"

#include <string.h>

void rpg_ground_clear(RpgGround *g)
{
    memset(g, 0, sizeof(*g));
}

int rpg_ground_add(RpgGround *g, float x, float y, RpgDrop drop)
{
    int i;

    if (drop.gold <= 0 && !rpg_item_ok(&drop.item))
        return -1;
    for (i = 0; i < RPG_LOOT_MAX; i++) {
        if (g->pile[i].on)
            continue;
        g->pile[i].on = 1;
        g->pile[i].x = x;
        g->pile[i].y = y;
        g->pile[i].gold = drop.gold;
        g->pile[i].item = drop.item;
        if (i >= g->used)
            g->used = i + 1;
        return i;
    }
    return -1;
}

int rpg_ground_at(const RpgGround *g, float x, float y, float rad)
{
    int i, best = -1;
    float bd = rad * rad, d, dx, dy;

    for (i = 0; i < RPG_LOOT_MAX; i++) {
        if (!g->pile[i].on)
            continue;
        dx = x - g->pile[i].x;
        dy = y - g->pile[i].y;
        d = dx * dx + dy * dy;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

int rpg_ground_take(RpgGround *g, int i, RpgHero *h)
{
    RpgLoot *L;
    int got = 0, item_left = 0;

    if (!g || !h || i < 0 || i >= RPG_LOOT_MAX || !g->pile[i].on)
        return 0;
    L = &g->pile[i];
    if (L->gold > 0) {
        const RpgRules *r = rpg_rules();
        if (r && r->gold >= 0) {
            rpg_add(&h->live, r->gold, L->gold);
            rpg_set(&h->base, r->gold, rpg_get(&h->live, r->gold));
        }
        L->gold = 0;
        got = 1;
    }
    if (rpg_item_ok(&L->item)) {
        if (rpg_inv_add(&h->inv, L->item) != 0)
            item_left = 1;
        else {
            memset(&L->item, 0, sizeof(L->item));
            got = 1;
        }
    }
    if (L->gold <= 0 && !rpg_item_ok(&L->item))
        L->on = 0;
    if (got)
        rpg_hero_refresh(h);
    if (got && item_left)
        return 2;
    return got ? 1 : 0;
}
