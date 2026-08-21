#ifndef RPG_LOOT_H
#define RPG_LOOT_H

#include "rpg.h"

/* Ground loot field. The game draws piles; the engine owns take/add. */

enum { RPG_LOOT_MAX = 96 };

typedef struct {
    int on, gold;
    RpgItem item;
    float x, y;
} RpgLoot;

typedef struct {
    RpgLoot pile[RPG_LOOT_MAX];
    int used;
} RpgGround;

void rpg_ground_clear(RpgGround *g);
int rpg_ground_add(RpgGround *g, float x, float y, RpgDrop drop);
int rpg_ground_at(const RpgGround *g, float x, float y, float rad);
/* 1 = pile gone, 2 = gold taken item left (pack full), 0 = nothing */
int rpg_ground_take(RpgGround *g, int i, RpgHero *h);

#endif
