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
    live->v[ST_HIT] = 0;
    live->v[ST_DODGE] = 0;
    live->v[ST_PARRY] = 0;
    live->v[ST_BLOCK] = 0;
    live->v[ST_BLKAMT] = 0;
    live->v[ST_CRIT] = 0;
    live->v[ST_CRITDMG] = 0;
    live->v[ST_ASPD] = 0;
    live->v[ST_PEN] = 0;
    live->v[ST_RPHYS] = 0;
    live->v[ST_RFIRE] = 0;
    live->v[ST_RCOLD] = 0;
    live->v[ST_RLIT] = 0;
    live->v[ST_RPOIS] = 0;
}

static void fill_combat_attrs(RpgStats *s)
{
    const CryptTune *T = &Crypt;
    int dex = s->v[ST_DEX];

    s->v[ST_HIT] += T->melee.hit_base + (T->derive.hit_dex_div > 0 ? dex / T->derive.hit_dex_div : 0);
    s->v[ST_DODGE] += T->derive.dodge_div > 0 ? dex / T->derive.dodge_div : 0;
    s->v[ST_PARRY] += T->derive.parry_div > 0 ? dex / T->derive.parry_div : 0;
    s->v[ST_CRIT] += T->melee.crit_base + (T->melee.crit_dex_div > 0 ? dex / T->melee.crit_dex_div : 0);
    s->v[ST_CRITDMG] += T->melee.crit_bonus_pct;
    s->v[ST_ASPD] += T->derive.aspd_div > 0 ? dex / T->derive.aspd_div : 0;
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
    fill_combat_attrs(live);
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

static int clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static int cb_chance(int v, int hi)
{
    return clampi(v, 0, hi);
}

static int cb_dodge(const RpgAttack *a)
{
    return cb_chance(a->def->v[ST_DODGE], Crypt.melee.avoid_max);
}

static int cb_parry(const RpgAttack *a)
{
    if (a->style != RPG_STYLE_MELEE)
        return 0;
    return cb_chance(a->def->v[ST_PARRY], Crypt.melee.avoid_max);
}

static int cb_to_hit(const RpgAttack *a)
{
    return clampi(a->atk->v[ST_HIT], Crypt.melee.hit_min, Crypt.melee.hit_max);
}

static int cb_block(const RpgAttack *a)
{
    if (a->style != RPG_STYLE_MELEE)
        return 0;
    return cb_chance(a->def->v[ST_BLOCK], Crypt.melee.avoid_max);
}

static int cb_roll_damage(const RpgAttack *a)
{
    int lo, hi, span, dmg;

    lo = a->atk->v[ST_DMIN];
    hi = a->atk->v[ST_DMAX];
    if (hi < lo)
        hi = lo;
    span = hi - lo;
    dmg = lo + (span > 0 ? rpg_rng(0, span) : 0);
    if (a->power > 0)
        dmg += a->power;
    return dmg;
}

static int cb_crit_chance(const RpgAttack *a)
{
    return cb_chance(a->atk->v[ST_CRIT], Crypt.melee.crit_max);
}

static int cb_crit_apply(const RpgAttack *a, int dmg)
{
    int pct = a->atk->v[ST_CRITDMG];

    if (pct < 0)
        pct = 0;
    return dmg + dmg * pct / 100;
}

static int cb_block_apply(const RpgAttack *a, int dmg)
{
    dmg -= a->def->v[ST_BLKAMT];
    if (dmg < Crypt.melee.block_min)
        dmg = Crypt.melee.block_min;
    return dmg;
}

static int cb_armor(const RpgAttack *a, int dmg)
{
    int ac;

    if (a->dtype != 0 && a->dtype != DT_PHYS)
        return dmg;
    ac = a->def->v[ST_ARMOR] - a->atk->v[ST_PEN];
    if (ac < 0)
        ac = 0;
    if (Crypt.melee.armor_div > 0)
        dmg -= ac / Crypt.melee.armor_div;
    return dmg;
}

static int resist_stat(int dtype)
{
    switch (dtype) {
    case DT_FIRE:
        return ST_RFIRE;
    case DT_COLD:
        return ST_RCOLD;
    case DT_LIT:
        return ST_RLIT;
    case DT_POIS:
        return ST_RPOIS;
    default:
        return ST_RPHYS;
    }
}

static int cb_resist(const RpgAttack *a, int dmg)
{
    int r = a->def->v[resist_stat(a->dtype ? a->dtype : DT_PHYS)];

    r = clampi(r, 0, Crypt.melee.resist_cap);
    return dmg * (100 - r) / 100;
}

static int cb_floor_dmg(const RpgAttack *a, int dmg)
{
    (void)a;
    return dmg < 1 ? 1 : dmg;
}

float crypt_swing_period(const RpgStats *s)
{
    int haste;
    float p;

    haste = s ? s->v[ST_ASPD] : 0;
    if (haste < 0)
        haste = 0;
    p = Crypt.feel.swing * 100.0f / (100.0f + (float)haste);
    if (p < Crypt.feel.swing_min)
        p = Crypt.feel.swing_min;
    if (p > Crypt.feel.swing_max)
        p = Crypt.feel.swing_max;
    return p;
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
    v += (it->mods[ST_HIT] + it->mods[ST_DODGE] + it->mods[ST_PARRY] + it->mods[ST_BLOCK] + it->mods[ST_CRIT] +
          it->mods[ST_PEN] + it->mods[ST_RPHYS] + it->mods[ST_RFIRE] + it->mods[ST_RCOLD] + it->mods[ST_RLIT] +
          it->mods[ST_RPOIS]) *
         T->price.affix / 2;
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
    snprintf(buf, n,
             "%s   %+d-%d dmg  %+d ac  %+d hit %+d dodge %+d parry %+d blk  %+d str %+d dex %+d vit  %+d life",
             it->name, it->mods[ST_DMIN], it->mods[ST_DMAX], it->mods[ST_ARMOR], it->mods[ST_HIT], it->mods[ST_DODGE],
             it->mods[ST_PARRY], it->mods[ST_BLOCK], it->mods[ST_STR], it->mods[ST_DEX], it->mods[ST_VIT],
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
        it->mods[ST_PARRY] = T->gear.wpn_parry;
    } else if (roll < T->gear.armor_pct) {
        it->kind = IT_ARM;
        it->mods[ST_ARMOR] = T->gear.arm + bump * 2 + rpg_rng(0, 3);
        it->mods[ST_RPHYS] = bump;
    } else if (roll < T->gear.helm_pct) {
        it->kind = IT_HELM;
        it->mods[ST_ARMOR] = T->gear.helm + bump + rpg_rng(0, 2);
    } else if (roll < T->gear.shield_pct) {
        it->kind = IT_OFF;
        it->mods[ST_ARMOR] = T->gear.shld + bump + rpg_rng(0, 2);
        it->mods[ST_BLOCK] = T->gear.shld_block + bump;
        it->mods[ST_BLKAMT] = T->gear.shld_blkamt + bump / 2;
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

    switch (rpg_rng(0, 9)) {
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
    case 6:
        it->mods[ST_HIT] += rpg_rng(A->affix.hit_lo, A->affix.hit_hi);
        break;
    case 7:
        it->mods[ST_DODGE] += rpg_rng(A->affix.dodge_lo, A->affix.dodge_hi);
        break;
    case 8:
        it->mods[ST_CRIT] += rpg_rng(A->affix.crit_lo, A->affix.crit_hi);
        break;
    default:
        it->mods[ST_MANA] += rpg_rng(A->affix.mana_lo, A->affix.mana_hi);
        it->mods[ST_MAG] += rpg_rng(A->affix.mag_lo, A->affix.mag_hi);
        it->mods[ST_RFIRE + rpg_rng(0, 3)] += rpg_rng(A->affix.resist_lo, A->affix.resist_hi);
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
    fill_combat_attrs(out);
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
    f.attack_reload = Crypt.feel.mob_swing * 100.0f / (100.0f + (float)a->st.v[ST_ASPD]);
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
    f.attack_reload = Crypt.feel.mob_swing * 100.0f / (100.0f + (float)a->st.v[ST_ASPD]);
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
    .combat = {
        .dodge = cb_dodge,
        .parry = cb_parry,
        .to_hit = cb_to_hit,
        .block = cb_block,
        .roll_damage = cb_roll_damage,
        .crit_chance = cb_crit_chance,
        .crit_apply = cb_crit_apply,
        .block_apply = cb_block_apply,
        .armor = cb_armor,
        .resist = cb_resist,
        .floor_dmg = cb_floor_dmg,
    },
    .item_value = item_value,
    .item_price = item_price,
    .use_item = use_item,
    .describe = describe,
};

void crypt_bind(void)
{
    static RpgRules live;
    static RpgTerrain terr[5];

    memset(terr, 0, sizeof(terr));
    terr[DUN_WALL] = (RpgTerrain){ RPG_TF_BLOCK_LOS, 0, 100, 0, 0 };
    terr[DUN_FLOOR] = (RpgTerrain){ RPG_TF_WALK, 1, 100, 0, 0 };
    terr[CRYPT_LAVA] = (RpgTerrain){ RPG_TF_WALK | RPG_TF_HAZARD, Crypt.terrain.lava_cost, Crypt.terrain.lava_speed,
                                    DT_FIRE, Crypt.terrain.lava_power };
    terr[CRYPT_MUD] = (RpgTerrain){ RPG_TF_WALK, Crypt.terrain.mud_cost, Crypt.terrain.mud_speed, 0, 0 };
    terr[CRYPT_ICE] = (RpgTerrain){ RPG_TF_WALK, Crypt.terrain.ice_cost, Crypt.terrain.ice_speed, 0, 0 };

    live = crypt_rules;
    live.inv_w = Crypt.bag.w;
    live.inv_h = Crypt.bag.h;
    live.max_level = Crypt.bag.max_level;
    live.terrain = terr;
    live.terrain_n = 5;
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
    club.mods[ST_PARRY] = Crypt.gear.wpn_parry;
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
    crypt_paint_terrain(d, RPG_ZONE_TOWN, 0);
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
    crypt_paint_terrain(d, RPG_ZONE_OVERWORLD, 0);
}

static int terrain_reserved(const Dungeon *d, int tx, int ty)
{
    int dx, dy;

    dx = tx - d->start_tx;
    dy = ty - d->start_ty;
    if (dx * dx + dy * dy < 25)
        return 1;
    dx = tx - d->stair_tx;
    dy = ty - d->stair_ty;
    if (dx * dx + dy * dy < 16)
        return 1;
    return 0;
}

static void stamp_blob(Dungeon *d, int cx, int cy, int r, unsigned char t)
{
    int x, y;

    for (y = cy - r; y <= cy + r; y++) {
        for (x = cx - r; x <= cx + r; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy > r * r)
                continue;
            if (terrain_reserved(d, x, y))
                continue;
            if (dungeon_get(d, x, y) != DUN_FLOOR)
                continue;
            dungeon_set(d, x, y, t);
        }
    }
}

static int random_plain(const Dungeon *d, int *tx, int *ty)
{
    int i;

    for (i = 0; i < 48; i++) {
        *tx = rpg_rng(1, DUN_W - 2);
        *ty = rpg_rng(1, DUN_H - 2);
        if (dungeon_get(d, *tx, *ty) == DUN_FLOOR && !terrain_reserved(d, *tx, *ty))
            return 1;
    }
    return 0;
}

static void stamp_spots(Dungeon *d, int n, unsigned char t)
{
    int i, tx, ty, r, cap;

    cap = Crypt.terrain.blob;
    if (cap < 1)
        cap = 1;
    if (cap > 4)
        cap = 4;
    for (i = 0; i < n; i++) {
        if (!random_plain(d, &tx, &ty))
            continue;
        r = rpg_rng(1, cap);
        stamp_blob(d, tx, ty, r, t);
    }
}

void crypt_paint_terrain(Dungeon *d, int zone, int depth)
{
    int lava = 0, mud = 0, ice = 0;

    if (!d)
        return;
    if (zone == RPG_ZONE_TOWN)
        mud = Crypt.terrain.town_mud;
    else if (zone == RPG_ZONE_OVERWORLD) {
        mud = Crypt.terrain.wilds_mud;
        ice = Crypt.terrain.wilds_ice;
    } else {
        if (depth < 1)
            depth = 1;
        lava = Crypt.terrain.lava_spots + depth / 2;
        mud = Crypt.terrain.mud_spots;
        ice = Crypt.terrain.ice_spots;
    }
    stamp_spots(d, lava, CRYPT_LAVA);
    stamp_spots(d, mud, CRYPT_MUD);
    stamp_spots(d, ice, CRYPT_ICE);
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
