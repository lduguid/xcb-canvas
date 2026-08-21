#ifndef RPG_H
#define RPG_H

/* ARPG engine API. Manages stats, items, bags, equipment, gold, XP, and
 * ground loot as abstract systems. A game binds RpgRules with its own
 * stat list, item kinds, slot names, formulas, and generators. */

#include <stddef.h>

enum {
    RPG_STAT_MAX = 32,
    RPG_INV_MAX = 80,
    RPG_SLOT_MAX = 12,
    RPG_NAME = 40
};

enum { RPG_NONE = 0 }; /* item kind 0 = empty */

enum { RPG_WHITE = 0, RPG_MAGIC, RPG_RARE }; /* suggested rarities; games may use others */

enum {
    RPG_IF_STACK = 1 << 0,
    RPG_IF_USE = 1 << 1,
    RPG_IF_STOCK = 1 << 2 /* vendor keeps a copy after a buy */
};

typedef struct {
    int v[RPG_STAT_MAX];
} RpgStats;

typedef struct {
    int kind; /* game-defined, 0 empty */
    int rarity;
    int stack;
    int slot; /* wear slot, or -1 */
    unsigned flags;
    int mods[RPG_STAT_MAX];
    char name[RPG_NAME];
} RpgItem;

typedef struct {
    RpgItem grid[RPG_INV_MAX];
    RpgItem wear[RPG_SLOT_MAX];
} RpgInv;

typedef struct {
    RpgStats base, live;
    RpgInv inv;
} RpgHero;

typedef struct {
    int gold;
    RpgItem item;
} RpgDrop;

typedef struct RpgRules {
    int stat_n;
    int inv_w, inv_h;
    int slot_n;
    int max_level;
    const char *const *slot_name;

    /* Indices into RpgStats.v the engine uses for built-in helpers. -1 = unused. */
    int hp, hp_max, mp, mp_max;
    int gold, level, xp, xp_next;

    void (*fill_base)(RpgStats *base);
    void (*reset_derived)(RpgStats *live); /* before summing worn mods */
    void (*derive)(RpgStats *live);        /* after summing worn mods */
    void (*level_up)(RpgStats *base);
    int (*xp_to_next)(int level);
    int (*melee)(const RpgStats *atk, const RpgStats *def, int *crit);
    int (*item_value)(const RpgItem *it);
    int (*item_price)(const RpgItem *it);
    int (*use_item)(RpgStats *live, RpgItem *it);
    void (*describe)(const RpgItem *it, char *buf, size_t n);
} RpgRules;

void rpg_seed(unsigned s);
unsigned rpg_randu(void);
int rpg_rng(int lo, int hi);

void rpg_bind(const RpgRules *rules);
const RpgRules *rpg_rules(void);

int rpg_get(const RpgStats *s, int id);
void rpg_set(RpgStats *s, int id, int v);
void rpg_add(RpgStats *s, int id, int d);

int rpg_inv_w(void);
int rpg_inv_h(void);
int rpg_inv_n(void);
int rpg_slot_n(void);
const char *rpg_slot_name(int slot);

void rpg_hero_init(RpgHero *h);
void rpg_hero_refresh(RpgHero *h);
void rpg_hero_sync(RpgHero *h);
void rpg_recalc(const RpgStats *base, const RpgInv *inv, RpgStats *live);

int rpg_gain_xp(RpgStats *base, int xp);
int rpg_heal(RpgStats *live, int n);
int rpg_mana(RpgStats *live, int n);
int rpg_melee(const RpgStats *atk, const RpgStats *def, int *crit);

int rpg_item_ok(const RpgItem *it);
int rpg_item_mod(const RpgItem *it, int stat);
void rpg_item_desc(const RpgItem *it, char *buf, size_t n);
int rpg_item_value(const RpgItem *it);
int rpg_item_price(const RpgItem *it);

void rpg_inv_clear(RpgInv *inv);
int rpg_inv_add(RpgInv *inv, RpgItem it);
int rpg_inv_remove(RpgInv *inv, int grid_i, RpgItem *out);
int rpg_inv_move(RpgInv *from, int grid_i, RpgInv *to);
int rpg_equip(RpgInv *inv, int grid_i);
int rpg_unequip(RpgInv *inv, int slot);
int rpg_use(RpgHero *h, RpgItem *it);

int rpg_buy(RpgHero *h, RpgItem *stock, int stock_n, int i);
int rpg_sell(RpgHero *h, int grid_i);

#endif
