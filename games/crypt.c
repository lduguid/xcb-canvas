#include "games/crypt.h"

#include "rpg/dungeon.h"

#include <stdio.h>
#include <string.h>

static const char *const slot_names[] = { "Weapon", "Shield", "Helm", "Armor", "Ring" };

static void fill_base(RpgStats *s)
{
    memset(s, 0, sizeof(*s));
    s->v[ST_STR] = Crypt.start.str;
    s->v[ST_DEX] = Crypt.start.dex;
    s->v[ST_MAG] = Crypt.start.mag;
    s->v[ST_VIT] = Crypt.start.vit;
    s->v[ST_LV] = 1;
    s->v[ST_NEXT] = Crypt.start.xp_next;
    s->v[ST_HP] = -1;
    s->v[ST_MP] = -1;
}

static void reset_derived(RpgStats *live)
{
    live->v[ST_ARMOR] = 0;
    live->v[ST_DMIN] = Crypt.derive.punch_min;
    live->v[ST_DMAX] = Crypt.derive.punch_max;
    live->v[ST_LIFE] = 0;
    live->v[ST_MANA] = 0;
}

static void derive(RpgStats *live)
{
    int lv = live->v[ST_LV];
    const CryptTune *T = &Crypt;

    if (lv < 1)
        lv = 1;
    live->v[ST_HPMAX] = T->derive.hp_base + live->v[ST_VIT] * T->derive.hp_per_vit + live->v[ST_LIFE] +
                        (lv - 1) * T->derive.hp_per_level;
    live->v[ST_MPMAX] = T->derive.mp_base + live->v[ST_MAG] * T->derive.mp_per_mag + live->v[ST_MANA] +
                        (lv - 1) * T->derive.mp_per_level;
    live->v[ST_DMIN] += live->v[ST_STR] / T->derive.dmg_str_div_min;
    live->v[ST_DMAX] += live->v[ST_STR] / T->derive.dmg_str_div_max;
    if (live->v[ST_DMAX] < live->v[ST_DMIN])
        live->v[ST_DMAX] = live->v[ST_DMIN];
    live->v[ST_ARMOR] += live->v[ST_DEX] / T->derive.ac_dex_div;
}

static void level_up(RpgStats *base)
{
    base->v[ST_STR] += Crypt.on_level.str;
    base->v[ST_DEX] += Crypt.on_level.dex;
    base->v[ST_VIT] += Crypt.on_level.vit;
    if ((base->v[ST_LV] & 1) == 0)
        base->v[ST_MAG] += Crypt.on_level.mag_even;
}

static int xp_to_next(int level)
{
    return Crypt.xp.base + level * level * Crypt.xp.per_sq;
}

static int melee(const RpgStats *atk, const RpgStats *def, int *crit)
{
    int chance, dmg, span;
    const CryptTune *T = &Crypt;

    if (crit)
        *crit = 0;
    chance = T->melee.hit_base + (atk->v[ST_DEX] - def->v[ST_DEX]) / T->melee.hit_dex_div;
    if (chance < T->melee.hit_min)
        chance = T->melee.hit_min;
    if (chance > T->melee.hit_max)
        chance = T->melee.hit_max;
    if (rpg_rng(1, 100) > chance)
        return 0;
    span = atk->v[ST_DMAX] - atk->v[ST_DMIN];
    dmg = atk->v[ST_DMIN] + (span > 0 ? rpg_rng(0, span) : 0);
    dmg -= def->v[ST_ARMOR] / T->melee.armor_div;
    if (rpg_rng(1, 100) <= T->melee.crit_base + atk->v[ST_DEX] / T->melee.crit_dex_div) {
        dmg += dmg * T->melee.crit_bonus_pct / 100;
        if (crit)
            *crit = 1;
    }
    if (dmg < 1)
        dmg = 1;
    return dmg;
}

static int item_value(const RpgItem *it)
{
    int v, i;
    const CryptTune *T = &Crypt;

    if (!rpg_item_ok(it))
        return 0;
    if (it->kind == IT_HPOT)
        return T->price.pot_hp_each * (it->stack > 0 ? it->stack : 1);
    if (it->kind == IT_MPOT)
        return T->price.pot_mp_each * (it->stack > 0 ? it->stack : 1);
    v = T->price.gear_base + it->mods[ST_DMIN] * T->price.dmg + it->mods[ST_DMAX] * T->price.dmg +
        it->mods[ST_ARMOR] * T->price.armor;
    for (i = ST_STR; i <= ST_VIT; i++)
        v += it->mods[i] * T->price.affix;
    v += it->mods[ST_LIFE] + it->mods[ST_MANA];
    if (it->rarity == RPG_MAGIC)
        v *= T->price.magic_mul;
    if (it->rarity == RPG_RARE)
        v *= T->price.rare_mul;
    if (v < T->price.floor)
        v = T->price.floor;
    return v;
}

static int item_price(const RpgItem *it)
{
    int v = item_value(it);

    if (it && (it->kind == IT_HPOT || it->kind == IT_MPOT))
        return Crypt.price.pot_buy;
    return v * Crypt.price.markup_num / Crypt.price.markup_den + Crypt.price.markup_add;
}

static int use_item(RpgStats *live, RpgItem *it)
{
    int n = 0;

    if (it->kind == IT_HPOT)
        n = rpg_heal(live, Crypt.potion.hp_base + live->v[ST_LV] * Crypt.potion.hp_per_level);
    else if (it->kind == IT_MPOT)
        n = rpg_mana(live, Crypt.potion.mp_base + live->v[ST_MAG] * Crypt.potion.mp_per_mag);
    else
        return 0;
    it->stack--;
    if (it->stack <= 0)
        memset(it, 0, sizeof(*it));
    return n;
}

static void describe(const RpgItem *it, char *buf, size_t n)
{
    snprintf(buf, n, "%s   %+d-%d dmg  %+d ac  %+d str %+d dex %+d vit  %+d life", it->name, it->mods[ST_DMIN],
             it->mods[ST_DMAX], it->mods[ST_ARMOR], it->mods[ST_STR], it->mods[ST_DEX], it->mods[ST_VIT],
             it->mods[ST_LIFE]);
}

static int kind_slot(int kind)
{
    switch (kind) {
    case IT_WEP:
        return SL_WEP;
    case IT_OFF:
        return SL_OFF;
    case IT_HELM:
        return SL_HELM;
    case IT_ARM:
        return SL_ARM;
    case IT_RING:
        return SL_RING;
    default:
        return -1;
    }
}

RpgItem crypt_potion(int life)
{
    RpgItem it;

    memset(&it, 0, sizeof(it));
    it.kind = life ? IT_HPOT : IT_MPOT;
    it.rarity = RPG_WHITE;
    it.stack = 1;
    it.slot = -1;
    it.flags = RPG_IF_STACK | RPG_IF_USE | RPG_IF_STOCK;
    snprintf(it.name, sizeof(it.name), "%s", life ? "Potion of Life" : "Potion of Mana");
    return it;
}

static void copy_name(char *dst, size_t n, const char *s)
{
    size_t i;

    for (i = 0; i + 1 < n && s[i]; i++)
        dst[i] = s[i];
    dst[i] = 0;
}

static const char *weapon_base[] = { "Club", "Short Sword", "Axe", "Mace", "Blade" };
static const char *armor_base[] = { "Rags", "Leather", "Chain", "Scale" };
static const char *helm_base[] = { "Cap", "Helm", "Mask" };
static const char *shield_base[] = { "Buckler", "Kite Shield" };
static const char *ring_base[] = { "Ring", "Band" };
static const char *pre_magic[] = { "Sharp", "Heavy", "Fine", "Jagged", "Sturdy", "Quick" };
static const char *suf_magic[] = { "of Worth", "of the Fox", "of Life", "of the Bear", "of Light" };
static const char *pre_rare[] = { "Cruel", "Holy", "Grim", "Ancient", "Blood" };

static int roll_kind(int depth, RpgItem *it)
{
    int roll, bump = depth / Crypt.gear.per_depth;
    const CryptTune *T = &Crypt;

    roll = rpg_rng(0, 99);
    memset(it, 0, sizeof(*it));
    it->stack = 1;
    it->flags = 0;
    if (roll < T->gear.weapon_pct) {
        it->kind = IT_WEP;
        it->mods[ST_DMIN] = T->gear.wpn_min + bump;
        it->mods[ST_DMAX] = T->gear.wpn_max + bump * 2 + rpg_rng(0, 2);
    } else if (roll < T->gear.armor_pct) {
        it->kind = IT_ARM;
        it->mods[ST_ARMOR] = T->gear.arm + bump * 2 + rpg_rng(0, 3);
    } else if (roll < T->gear.helm_pct) {
        it->kind = IT_HELM;
        it->mods[ST_ARMOR] = T->gear.helm + bump + rpg_rng(0, 2);
    } else if (roll < T->gear.shield_pct) {
        it->kind = IT_OFF;
        it->mods[ST_ARMOR] = T->gear.shld + bump + rpg_rng(0, 2);
    } else {
        it->kind = IT_RING;
        it->mods[ST_STR] = rpg_rng(0, 1 + bump / 2);
        it->mods[ST_DEX] = rpg_rng(0, 1 + bump / 2);
        it->mods[ST_VIT] = rpg_rng(0, 1);
        it->mods[ST_LIFE] = rpg_rng(0, 4 + bump);
        it->mods[ST_MANA] = rpg_rng(0, 3 + bump);
    }
    it->slot = kind_slot(it->kind);
    return it->kind;
}

static const char *base_name(int kind)
{
    switch (kind) {
    case IT_WEP:
        return weapon_base[rpg_rng(0, 4)];
    case IT_ARM:
        return armor_base[rpg_rng(0, 3)];
    case IT_HELM:
        return helm_base[rpg_rng(0, 2)];
    case IT_OFF:
        return shield_base[rpg_rng(0, 1)];
    default:
        return ring_base[rpg_rng(0, 1)];
    }
}

static void affix(RpgItem *it)
{
    const CryptTune *A = &Crypt;

    switch (rpg_rng(0, 6)) {
    case 0:
        it->mods[ST_STR] += rpg_rng(A->affix.str_lo, A->affix.str_hi);
        break;
    case 1:
        it->mods[ST_DEX] += rpg_rng(A->affix.dex_lo, A->affix.dex_hi);
        break;
    case 2:
        it->mods[ST_VIT] += rpg_rng(A->affix.vit_lo, A->affix.vit_hi);
        break;
    case 3:
        it->mods[ST_ARMOR] += rpg_rng(A->affix.ac_lo, A->affix.ac_hi);
        break;
    case 4:
        it->mods[ST_DMIN] += rpg_rng(A->affix.dmin_lo, A->affix.dmin_hi);
        it->mods[ST_DMAX] += rpg_rng(A->affix.dmax_lo, A->affix.dmax_hi);
        break;
    case 5:
        it->mods[ST_LIFE] += rpg_rng(A->affix.life_lo, A->affix.life_hi);
        break;
    default:
        it->mods[ST_MANA] += rpg_rng(A->affix.mana_lo, A->affix.mana_hi);
        it->mods[ST_MAG] += rpg_rng(A->affix.mag_lo, A->affix.mag_hi);
        break;
    }
}

RpgItem crypt_roll_gear(int depth)
{
    RpgItem it;
    int rarity_roll;
    char built[40];

    roll_kind(depth, &it);
    rarity_roll = rpg_rng(0, 99) + depth;
    if (rarity_roll > Crypt.gear.rare_over) {
        it.rarity = RPG_RARE;
        affix(&it);
        affix(&it);
        snprintf(built, sizeof(built), "%s %s %s", pre_rare[rpg_rng(0, 4)], base_name(it.kind),
                 suf_magic[rpg_rng(0, 4)]);
    } else if (rarity_roll > Crypt.gear.magic_over) {
        it.rarity = RPG_MAGIC;
        affix(&it);
        if (rpg_rng(0, 1))
            snprintf(built, sizeof(built), "%s %s", pre_magic[rpg_rng(0, 5)], base_name(it.kind));
        else
            snprintf(built, sizeof(built), "%s %s", base_name(it.kind), suf_magic[rpg_rng(0, 4)]);
    } else {
        it.rarity = RPG_WHITE;
        copy_name(built, sizeof(built), base_name(it.kind));
    }
    copy_name(it.name, sizeof(it.name), built);
    return it;
}

RpgDrop crypt_roll_drop(int depth, int champion)
{
    RpgDrop d;
    int roll;
    const CryptTune *T = &Crypt;

    memset(&d, 0, sizeof(d));
    d.gold = rpg_rng(T->drop.gold_min, T->drop.gold_max + depth * T->drop.gold_per_depth);
    if (champion)
        d.gold *= T->drop.champ_gold_mul;
    roll = rpg_rng(0, 99);
    if (champion)
        roll -= T->drop.champ_luck;
    if (roll < T->drop.potion_pct)
        d.item = crypt_potion(rpg_rng(0, 2) != 0);
    else if (roll < T->drop.gear_pct + (champion ? T->drop.champ_gear_pct : 0))
        d.item = crypt_roll_gear(depth);
    return d;
}

void crypt_mob_stats(RpgStats *out, const CryptSpecies *sp, int depth, int champion)
{
    int scale = depth;
    const CryptTune *T = &Crypt;
    float mul = champion ? T->scale.champ_hp : 1.0f;

    memset(out, 0, sizeof(*out));
    if (!sp)
        return;
    if (scale < 1)
        scale = 1;
    out->v[ST_HPMAX] = (int)((sp->hp + (scale - 1) * (sp->hp / 3 + 4)) * mul);
    out->v[ST_HP] = out->v[ST_HPMAX];
    out->v[ST_DMIN] = (int)((sp->dmg_min + scale / 2) * mul);
    out->v[ST_DMAX] = (int)((sp->dmg_max + scale) * mul);
    out->v[ST_ARMOR] = (int)((sp->armor + scale / 2) * (champion ? T->scale.champ_ac : 1.0f));
    out->v[ST_DEX] = T->scale.dex_base + scale + (champion ? T->scale.champ_dex : 0);
    out->v[ST_STR] = T->scale.str_base + scale * 2;
    out->v[ST_LV] = scale;
    out->v[ST_XP] = (int)((sp->xp + scale * T->scale.xp_per_depth) * (champion ? T->scale.champ_xp : 1.0f));
    out->v[ST_GOLD] = rpg_rng(sp->gold_min, sp->gold_max) * scale;
    if (champion)
        out->v[ST_GOLD] *= T->scale.champ_gold_mul;
}

static float crypt_move_speed(const CryptSpecies *sp, int role)
{
    float s, mul;

    s = sp && sp->speed > 1.0f ? sp->speed : 1.0f;
    if (role == RPG_ROLE_CHAMPION)
        mul = Crypt.scale.champ_speed;
    else if (role == RPG_ROLE_BOSS)
        mul = Crypt.scale.boss_speed;
    else
        mul = 1.0f;
    if (mul < 0.05f)
        mul = 1.0f;
    return s * mul;
}

static float crypt_sight(const CryptSpecies *sp, int role)
{
    float s, mul;

    s = sp && sp->sight > 1.0f ? sp->sight : 96.0f;
    mul = (role == RPG_ROLE_CHAMPION || role == RPG_ROLE_BOSS) ? Crypt.scale.champ_sight : 1.0f;
    if (mul < 0.05f)
        mul = 1.0f;
    return s * mul;
}

void crypt_actor_setup(RpgActor *a, int species, int depth, int champion)
{
    const CryptSpecies *sp;
    RpgActorFeel f;

    if (!a)
        return;
    rpg_actor_clear(a);
    if (species < 0 || species >= crypt_species_n)
        species = 0;
    sp = &crypt_species[species];
    a->alive = 1;
    a->kind = species;
    if (sp->role == RPG_ROLE_BOSS)
        champion = 1;
    crypt_mob_stats(&a->st, sp, depth, champion);
    memset(&f, 0, sizeof(f));
    if (sp->role == RPG_ROLE_BOSS)
        f.role = RPG_ROLE_BOSS;
    else if (champion)
        f.role = RPG_ROLE_CHAMPION;
    else
        f.role = RPG_ROLE_MINION;
    f.speed = crypt_move_speed(sp, f.role);
    f.sight = crypt_sight(sp, f.role);
    f.leash = sp->leash;
    f.range = sp->range;
    f.radius = sp->radius;
    f.see_walls = sp->see_walls;
    f.attack_reload = Crypt.feel.mob_swing;
    f.ability_reload = sp->ability_cd;
    f.ability = sp->ability;
    rpg_actor_feel(a, &f);
}

void crypt_actor_refresh(RpgActor *a)
{
    const CryptSpecies *sp;
    RpgActorFeel f;

    if (!a || !a->alive || a->kind < 0 || a->kind >= crypt_species_n)
        return;
    sp = &crypt_species[a->kind];
    memset(&f, 0, sizeof(f));
    f.role = a->role;
    f.speed = crypt_move_speed(sp, a->role);
    f.sight = crypt_sight(sp, a->role);
    f.leash = sp->leash;
    f.range = sp->range;
    f.radius = sp->radius;
    f.see_walls = sp->see_walls;
    f.attack_reload = Crypt.feel.mob_swing;
    f.ability_reload = sp->ability_cd;
    f.ability = sp->ability;
    rpg_actor_feel(a, &f);
}

int crypt_roll_species(int depth)
{
    int ids[16], n = 0, i, cap;

    if (depth <= 1)
        return 0;
    cap = depth > 6 ? crypt_species_n - 1 : (depth < 3 ? 1 : 2);
    for (i = 0; i <= cap && i < crypt_species_n; i++) {
        if (crypt_species[i].role == RPG_ROLE_BOSS)
            continue;
        if (n < 16)
            ids[n++] = i;
    }
    if (!n)
        return 0;
    return ids[rpg_rng(0, n - 1)];
}

int crypt_boss_species(void)
{
    int i;

    for (i = 0; i < crypt_species_n; i++) {
        if (crypt_species[i].role == RPG_ROLE_BOSS)
            return i;
    }
    return -1;
}

const RpgRules crypt_rules = {
    .stat_n = ST_N,
    .inv_w = 10,
    .inv_h = 4,
    .slot_n = SL_N,
    .max_level = 50,
    .slot_name = slot_names,
    .hp = ST_HP,
    .hp_max = ST_HPMAX,
    .mp = ST_MP,
    .mp_max = ST_MPMAX,
    .gold = ST_GOLD,
    .level = ST_LV,
    .xp = ST_XP,
    .xp_next = ST_NEXT,
    .fill_base = fill_base,
    .reset_derived = reset_derived,
    .derive = derive,
    .level_up = level_up,
    .xp_to_next = xp_to_next,
    .melee = melee,
    .item_value = item_value,
    .item_price = item_price,
    .use_item = use_item,
    .describe = describe,
};

void crypt_bind(void)
{
    static RpgRules live;

    live = crypt_rules;
    live.inv_w = Crypt.bag.w;
    live.inv_h = Crypt.bag.h;
    live.max_level = Crypt.bag.max_level;
    rpg_bind(&live);
}

void crypt_kit(RpgHero *pc)
{
    RpgItem club, pot;

    memset(&club, 0, sizeof(club));
    club.kind = IT_WEP;
    club.slot = SL_WEP;
    club.rarity = RPG_WHITE;
    club.stack = 1;
    club.mods[ST_DMIN] = Crypt.kit.club_min;
    club.mods[ST_DMAX] = Crypt.kit.club_max;
    snprintf(club.name, sizeof(club.name), "Club");
    rpg_inv_add(&pc->inv, club);
    rpg_equip(&pc->inv, 0);
    pot = crypt_potion(1);
    pot.stack = Crypt.kit.hp_pots;
    rpg_inv_add(&pc->inv, pot);
    pot = crypt_potion(0);
    pot.stack = Crypt.kit.mp_pots;
    rpg_inv_add(&pc->inv, pot);
    rpg_bar_bind_item(&pc->bar, 0, IT_HPOT);
    rpg_bar_bind_item(&pc->bar, 1, IT_MPOT);
    rpg_set(&pc->base, ST_GOLD, Crypt.start.gold);
    rpg_hero_refresh(pc);
}

static void carve_road(Dungeon *d, int x0, int y0, int x1, int y1, int rad)
{
    int x = x0, y = y0, i, j;

    while (x != x1) {
        for (j = -rad; j <= rad; j++)
            for (i = -rad; i <= rad; i++)
                dungeon_set(d, x + i, y + j, DUN_FLOOR);
        x += (x1 > x) ? 1 : -1;
    }
    while (y != y1) {
        for (j = -rad; j <= rad; j++)
            for (i = -rad; i <= rad; i++)
                dungeon_set(d, x + i, y + j, DUN_FLOOR);
        y += (y1 > y) ? 1 : -1;
    }
}

static void tree_clump(Dungeon *d, int cx, int cy, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        int tx = cx + rpg_rng(-3, 3);
        int ty = cy + rpg_rng(-3, 3);
        dungeon_set(d, tx, ty, DUN_WALL);
        if (rpg_rng(0, 2) == 0)
            dungeon_set(d, tx + 1, ty, DUN_WALL);
    }
}

static void vendor_stock(RpgWorld *w)
{
    RpgItem pot;
    int i;

    memset(w->vendor, 0, sizeof(w->vendor));
    pot = crypt_potion(1);
    pot.stack = 20;
    w->vendor[0] = pot;
    pot = crypt_potion(0);
    pot.stack = 20;
    w->vendor[1] = pot;
    for (i = 2; i < RPG_VENDOR_N; i++)
        w->vendor[i] = crypt_roll_gear(1 + i / 3);
}

static void build_town(RpgWorld *w)
{
    Dungeon *d = &w->maps[RPG_ZONE_TOWN];
    RpgPlace *p;

    memset(d, 0, sizeof(*d));
    dungeon_fill(d, DUN_WALL);
    dungeon_rect(d, 16, 14, 40, 42, DUN_FLOOR);
    dungeon_rect(d, 18, 16, 8, 8, DUN_WALL);
    dungeon_rect(d, 46, 16, 8, 8, DUN_WALL);
    dungeon_rect(d, 18, 40, 8, 8, DUN_WALL);
    dungeon_rect(d, 46, 40, 8, 8, DUN_WALL);
    dungeon_rect(d, 34, 52, 5, 12, DUN_FLOOR);
    d->start_tx = 36;
    d->start_ty = 32;
    d->stair_tx = 36;
    d->stair_ty = 58;
    w->place_n[RPG_ZONE_TOWN] = 0;
    rpg_world_add_place(w, RPG_ZONE_TOWN, RPG_PLACE_VENDOR, 30, 28, "Trader", "Space — buy / sell");
    rpg_world_add_place(w, RPG_ZONE_TOWN, RPG_PLACE_BANK, 42, 28, "Stash", "Space — bank");
    rpg_world_add_place(w, RPG_ZONE_TOWN, RPG_PLACE_QUEST, 36, 22, "Notice board", "Space — contracts");
    p = rpg_world_add_place(w, RPG_ZONE_TOWN, RPG_PLACE_GATE, 36, 58, "Town gate", "Space — leave town");
    if (p)
        p->dest_zone = RPG_ZONE_OVERWORLD;
}

static void build_overworld(RpgWorld *w)
{
    Dungeon *d = &w->maps[RPG_ZONE_OVERWORLD];
    int i, x, y;
    RpgPlace *p;
    static const struct {
        const char *name;
        int depth;
        int tx, ty;
    } sites[] = {
        { "The Crypt", 1, 16, 20 },
        { "Bone Hollow", 3, 56, 18 },
        { "The Pit", 5, 18, 54 },
        { "Black Abyss", 8, 58, 56 },
    };

    memset(d, 0, sizeof(*d));
    dungeon_fill(d, DUN_FLOOR);
    for (y = 0; y < DUN_H; y++) {
        dungeon_set(d, 0, y, DUN_WALL);
        dungeon_set(d, DUN_W - 1, y, DUN_WALL);
    }
    for (x = 0; x < DUN_W; x++) {
        dungeon_set(d, x, 0, DUN_WALL);
        dungeon_set(d, x, DUN_H - 1, DUN_WALL);
    }
    for (i = 0; i < 28; i++)
        tree_clump(d, rpg_rng(4, DUN_W - 5), rpg_rng(4, DUN_H - 5), rpg_rng(4, 10));

    d->start_tx = 36;
    d->start_ty = 10;
    w->place_n[RPG_ZONE_OVERWORLD] = 0;
    p = rpg_world_add_place(w, RPG_ZONE_OVERWORLD, RPG_PLACE_GATE, 36, 10, "Haven", "Space — enter town");
    if (p)
        p->dest_zone = RPG_ZONE_TOWN;
    carve_road(d, 36, 10, 36, 36, 1);

    for (i = 0; i < 4; i++) {
        char prompt[48];
        carve_road(d, 36, 36, sites[i].tx, sites[i].ty, 1);
        dungeon_rect(d, sites[i].tx - 2, sites[i].ty - 2, 5, 5, DUN_FLOOR);
        snprintf(prompt, sizeof(prompt), "Space — enter %s", sites[i].name);
        p = rpg_world_add_place(w, RPG_ZONE_OVERWORLD, RPG_PLACE_PORTAL, sites[i].tx, sites[i].ty, sites[i].name,
                                prompt);
        if (p) {
            p->dest_zone = RPG_ZONE_DUNGEON;
            p->dest_id = i;
            p->dest_depth = sites[i].depth;
        }
    }
}

void crypt_world_init(RpgWorld *w, unsigned seed)
{
    crypt_bind();
    rpg_seed(seed ? seed : 1u);
    rpg_world_clear(w);
    build_town(w);
    build_overworld(w);
    vendor_stock(w);
    rpg_world_reveal_all(w, RPG_ZONE_TOWN);
    rpg_world_reveal_around(w, RPG_ZONE_OVERWORLD, 36, 10, 10);
}

const char *crypt_zone_title(const RpgWorld *w)
{
    int i;

    if (w->zone == RPG_ZONE_TOWN)
        return "Haven";
    if (w->zone == RPG_ZONE_OVERWORLD)
        return "The Wilds";
    for (i = 0; i < w->place_n[RPG_ZONE_OVERWORLD]; i++) {
        const RpgPlace *p = &w->places[RPG_ZONE_OVERWORLD][i];
        if (p->kind == RPG_PLACE_PORTAL && p->dest_id == w->dungeon_id)
            return p->name;
    }
    return "Dungeon";
}
