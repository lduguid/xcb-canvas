#ifndef CRYPT_H
#define CRYPT_H

/* Crypt: this game's stats, items, loot tables, maps, monsters.
 * Tweak numbers in games/crypt_tune.c (F5 / F6). */

#include "games/crypt_tune.h"
#include "rpg/actor.h"
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
    ST_HIT,
    ST_DODGE,
    ST_PARRY,
    ST_BLOCK,
    ST_BLKAMT,
    ST_CRIT,
    ST_CRITDMG,
    ST_ASPD,
    ST_PEN,
    ST_RPHYS,
    ST_RFIRE,
    ST_RCOLD,
    ST_RLIT,
    ST_RPOIS,
    ST_N
};

enum { DT_PHYS = 1, DT_FIRE, DT_COLD, DT_LIT, DT_POIS };

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

enum { CRYPT_AB_NONE = 0, CRYPT_AB_SLAM };

enum { CRYPT_LAVA = 2, CRYPT_MUD = 3, CRYPT_ICE = 4 };

extern const RpgRules crypt_rules;

void crypt_bind(void);
void crypt_world_init(RpgWorld *w, unsigned seed);
void crypt_paint_terrain(Dungeon *d, int zone, int depth);
void crypt_kit(RpgHero *pc);
RpgItem crypt_potion(int life);
RpgItem crypt_roll_gear(int depth);
RpgDrop crypt_roll_drop(int depth, int champion);
void crypt_mob_stats(RpgStats *out, const CryptSpecies *sp, int depth, int champion);
void crypt_actor_setup(RpgActor *a, int species, int depth, int champion);
void crypt_actor_refresh(RpgActor *a);
int crypt_roll_species(int depth);
int crypt_boss_species(void);
const char *crypt_zone_title(const RpgWorld *w);
float crypt_swing_period(const RpgStats *s);

#endif
