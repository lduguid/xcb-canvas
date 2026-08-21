#ifndef CRYPT_H
#define CRYPT_H

/* Crypt: this game's stats, items, loot tables, maps, monsters.
 * Tweak numbers in games/crypt_tune.c (F5 / F6). */

#include "games/crypt_tune.h"
#include "rpg/rpg.h"
#include "rpg/world.h"

enum {
    ST_STR = 0,
    ST_DEX,
    ST_MAG,
    ST_VIT,
    ST_HP,
    ST_HPMAX,
    ST_MP,
    ST_MPMAX,
    ST_ARMOR,
    ST_DMIN,
    ST_DMAX,
    ST_LV,
    ST_XP,
    ST_NEXT,
    ST_GOLD,
    ST_LIFE,
    ST_MANA,
    ST_N
};

enum { SL_WEP = 0, SL_OFF, SL_HELM, SL_ARM, SL_RING, SL_N };

enum {
    IT_WEP = 1,
    IT_OFF,
    IT_HELM,
    IT_ARM,
    IT_RING,
    IT_HPOT,
    IT_MPOT
};

extern const RpgRules crypt_rules;

void crypt_bind(void);
void crypt_world_init(RpgWorld *w, unsigned seed);
void crypt_kit(RpgHero *pc);
RpgItem crypt_potion(int life);
RpgItem crypt_roll_gear(int depth);
RpgDrop crypt_roll_drop(int depth, int champion);
void crypt_mob_stats(RpgStats *out, const CryptSpecies *sp, int depth, int champion);
const char *crypt_zone_title(const RpgWorld *w);

#endif
