#ifndef RPG_H
#define RPG_H

/* ARPG engine API. Manages stats, items, bags, equipment, gold, XP,
 * action bar, class/skill/talent builds, combat resolution, and ground
 * loot as abstract systems. A game binds RpgRules with its own stat
 * list, item kinds, class/skill/talent catalogs, combat scripts, slot
 * names, formulas, and generators. */

#include <stddef.h>

enum {
    RPG_STAT_MAX = 32,
    RPG_INV_MAX = 80,
    RPG_SLOT_MAX = 12,
    RPG_BAR_N = 5,
    RPG_BOOK_MAX = 32,
    RPG_CLASS_START = 8,
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

enum {
    RPG_BAR_EMPTY = 0,
    RPG_BAR_ITEM,  /* id = item kind; finds a stack in the bag */
    RPG_BAR_SKILL  /* id = game skill; engine does not cast it */
};

typedef struct {
    int type;
    int id;
} RpgBarSlot;

typedef struct {
    RpgBarSlot slot[RPG_BAR_N];
} RpgBar;

/* Ranked id list (skills or talents). Bar slots bind a skill *id*, not
 * a book index, so the list can compact. The game names and casts. */
typedef struct {
    int id;
    int rank;
} RpgRanked;

typedef struct {
    RpgRanked v[RPG_BOOK_MAX];
    int n;
} RpgBook;

typedef struct {
    int class_id; /* 0 = none; game-defined otherwise */
    int talent_unspent;
    RpgBook skills;
    RpgBook talents;
} RpgBuild;

typedef struct {
    RpgItem grid[RPG_INV_MAX];
    RpgItem wear[RPG_SLOT_MAX];
} RpgInv;

typedef struct {
    RpgStats base, live;
    RpgInv inv;
    RpgBar bar;
    RpgBuild build;
} RpgHero;

enum {
    RPG_SF_ACTIVE = 1 << 0,  /* may sit on the action bar */
    RPG_SF_PASSIVE = 1 << 1  /* catalog mods apply on refresh */
};

/* Optional catalogs on RpgRules. Id 0 is unused. class_id 0 = any class.
 * max_rank / cost 0 means 1. The engine never stores names. */
typedef struct {
    int id;
    int start_pts;
    int start_skill[RPG_CLASS_START];
} RpgClassInfo;

typedef struct {
    int id;
    int class_id;
    int max_rank;
    int grant_level; /* auto-learn at this hero level; 0 = never */
    unsigned flags;
    int mods[RPG_STAT_MAX]; /* added rank times, before derive */
} RpgSkillInfo;

typedef struct {
    int id;
    int class_id;
    int max_rank;
    int req_level;
    int req_id; /* another talent or skill, or 0 */
    int req_rank;
    int cost;
    int mods[RPG_STAT_MAX];
} RpgTalentInfo;

typedef struct {
    int gold;
    RpgItem item;
} RpgDrop;

/* Combat styles are engine-level. Damage type ids are game-defined (0 = none). */
enum { RPG_STYLE_MELEE = 0, RPG_STYLE_SPELL };

enum {
    RPG_HIT_NONE = 0,
    RPG_HIT_MISS,
    RPG_HIT_DODGE,
    RPG_HIT_PARRY,
    RPG_HIT_BLOCK, /* connected; damage already reduced */
    RPG_HIT_HIT
};

enum {
    RPG_AF_CANT_DODGE = 1 << 0,
    RPG_AF_CANT_PARRY = 1 << 1,
    RPG_AF_CANT_BLOCK = 1 << 2,
    RPG_AF_ALWAYS_HIT = 1 << 3
};

typedef struct {
    const RpgStats *atk;
    const RpgStats *def;
    int style;     /* RPG_STYLE_* */
    int dtype;     /* game damage type */
    int skill;     /* 0 = basic swing */
    int power;     /* extra raw the game's roll_damage may add */
    unsigned flags;
} RpgAttack;

typedef struct {
    int outcome;
    int dmg;
    int raw;
    int mitigated;
    int crit;
    int dtype;
} RpgHit;

/* Game combat scripts. NULL skips that phase. Chance hooks return 0–100
 * (or <0 to skip). Damage hooks return the new amount. */
typedef struct {
    int (*dodge)(const RpgAttack *a);
    int (*parry)(const RpgAttack *a);
    int (*to_hit)(const RpgAttack *a);
    int (*block)(const RpgAttack *a);
    int (*roll_damage)(const RpgAttack *a);
    int (*crit_chance)(const RpgAttack *a);
    int (*crit_apply)(const RpgAttack *a, int dmg);
    int (*block_apply)(const RpgAttack *a, int dmg);
    int (*armor)(const RpgAttack *a, int dmg);
    int (*resist)(const RpgAttack *a, int dmg);
    int (*floor_dmg)(const RpgAttack *a, int dmg);
} RpgCombat;

enum {
    RPG_TF_WALK = 1 << 0,
    RPG_TF_BLOCK_LOS = 1 << 1,
    RPG_TF_HAZARD = 1 << 2
};

/* Per tile-kind row. Index = dungeon tile id. cost 0 → 1. speed_pct 0 → 100. */
typedef struct {
    unsigned flags;
    int cost;
    int speed_pct;
    int dtype;
    int power;
} RpgTerrain;

typedef struct RpgRules {
    int stat_n;
    int inv_w, inv_h;
    int slot_n;
    int max_level;
    const char *const *slot_name;

    /* Indices into RpgStats.v the engine uses for built-in helpers. -1 = unused. */
    int hp, hp_max, mp, mp_max;
    int gold, level, xp, xp_next;

    const RpgClassInfo *classes;
    int class_n;
    const RpgSkillInfo *skills;
    int skill_n;
    const RpgTalentInfo *talents;
    int talent_n;
    int talent_per_level; /* added to unspent on each hero level; 0 = none */

    const RpgTerrain *terrain;
    int terrain_n;

    void (*fill_base)(RpgStats *base);
    void (*reset_derived)(RpgStats *live); /* before summing worn mods */
    void (*derive)(RpgStats *live);        /* after summing worn / build mods */
    void (*level_up)(RpgStats *base);
    int (*xp_to_next)(int level);
    RpgCombat combat;
    int (*melee)(const RpgStats *atk, const RpgStats *def, int *crit); /* fallback if combat unset */
    int (*item_value)(const RpgItem *it);
    int (*item_price)(const RpgItem *it);
    int (*use_item)(RpgStats *live, RpgItem *it);
    void (*describe)(const RpgItem *it, char *buf, size_t n);
    void (*apply_build)(RpgStats *live, const RpgHero *h); /* optional, after derive */
} RpgRules;

void rpg_seed(unsigned s);
unsigned rpg_randu(void);
int rpg_rng(int lo, int hi);

void rpg_bind(const RpgRules *rules);
const RpgRules *rpg_rules(void);
const RpgTerrain *rpg_terrain(int kind);

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
int rpg_hero_gain_xp(RpgHero *h, int xp); /* levels, talent pts, granted skills */
int rpg_heal(RpgStats *live, int n);
int rpg_mana(RpgStats *live, int n);
int rpg_resolve(const RpgAttack *a, RpgHit *out);
int rpg_hit_connected(const RpgHit *h);
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
int rpg_inv_find(const RpgInv *inv, int kind);
int rpg_inv_count(const RpgInv *inv, int kind);
int rpg_equip(RpgInv *inv, int grid_i);
int rpg_unequip(RpgInv *inv, int slot);
int rpg_use(RpgHero *h, RpgItem *it);

void rpg_bar_clear(RpgBar *b);
void rpg_bar_bind_item(RpgBar *b, int slot, int kind);
void rpg_bar_bind_skill(RpgBar *b, int slot, int skill);
void rpg_bar_put(RpgBar *b, int slot, int type, int id);
void rpg_hero_bar_put(RpgHero *h, int slot, int type, int id);
void rpg_bar_unbind(RpgBar *b, int slot);
void rpg_bar_swap(RpgBar *bar, int ia, int ib);
/* 1 used an item, 0 empty/missing, -1 skill (game should cast). */
int rpg_bar_activate(RpgHero *h, int slot);

void rpg_book_clear(RpgBook *b);
int rpg_book_n(const RpgBook *b);
int rpg_book_id(const RpgBook *b, int i);
int rpg_book_rank_at(const RpgBook *b, int i);
int rpg_book_has(const RpgBook *b, int id);
int rpg_book_get(const RpgBook *b, int id); /* rank, or 0 */
int rpg_book_set(RpgBook *b, int id, int rank); /* rank 0 forgets; -1 full/bad */
int rpg_book_learn(RpgBook *b, int id);     /* rank 1 if new */
int rpg_book_forget(RpgBook *b, int id);

const RpgClassInfo *rpg_class_info(int id);
const RpgSkillInfo *rpg_skill_info(int id);
const RpgTalentInfo *rpg_talent_info(int id);

int rpg_class_get(const RpgHero *h);
int rpg_class_ok(int class_id);
int rpg_class_set(RpgHero *h, int class_id); /* respecs build; -1 unknown id */
int rpg_talent_unspent(const RpgHero *h);
void rpg_talent_grant(RpgHero *h, int n);
void rpg_build_respec(RpgHero *h);

int rpg_skill_known(const RpgHero *h, int id);
int rpg_skill_rank(const RpgHero *h, int id);
int rpg_skill_ok(const RpgHero *h, int id);   /* this class may use it */
int rpg_skill_learn(RpgHero *h, int id);      /* ensure rank 1 */
int rpg_skill_train(RpgHero *h, int id);      /* +1 rank */
int rpg_skill_forget(RpgHero *h, int id);

int rpg_talent_rank(const RpgHero *h, int id);
int rpg_talent_ok(const RpgHero *h, int id); /* can take next rank now */
int rpg_talent_take(RpgHero *h, int id);     /* spend pts, +1 rank */

int rpg_buy(RpgHero *h, RpgItem *stock, int stock_n, int i);
int rpg_sell(RpgHero *h, int grid_i);

#endif
