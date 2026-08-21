#ifndef CRYPT_TUNE_H
#define CRYPT_TUNE_H

/* Crypt balance. Numbers live in games/crypt_tune.c — edit there, F5.
 * Formulas (HP/VIT, hit chance, walk speed) apply on the next swing/refresh.
 * Starting kit, gold, bag size, and monster tables need F6 (new run). */

typedef struct {
    const char *name;
    int hp, dmg_min, dmg_max, armor, xp, gold_min, gold_max;
    float speed, aggro, range, radius;
    float r, g, b;
} CryptSpecies;

typedef struct CryptTune {
    struct { int str, dex, mag, vit, xp_next, gold; } start;
    struct { int club_min, club_max, hp_pots, mp_pots; } kit;
    struct { int w, h, max_level; } bag;

    struct {
        int hp_base, hp_per_vit, hp_per_level;
        int mp_base, mp_per_mag, mp_per_level;
        int punch_min, punch_max;
        int dmg_str_div_min, dmg_str_div_max, ac_dex_div;
    } derive;

    struct { int str, dex, vit, mag_even; } on_level;
    struct { int base, per_sq; } xp;

    struct {
        int hit_base, hit_dex_div, hit_min, hit_max;
        int armor_div, crit_base, crit_dex_div, crit_bonus_pct;
    } melee;

    struct { int hp_base, hp_per_level, mp_base, mp_per_mag; } potion;
    struct {
        int pot_hp_each, pot_mp_each, pot_buy;
        int gear_base, dmg, armor, affix, magic_mul, rare_mul, floor;
        int markup_num, markup_den, markup_add; /* sell * num/den + add */
    } price;

    struct {
        int gold_min, gold_max, gold_per_depth, champ_gold_mul;
        int potion_pct, gear_pct, champ_gear_pct, champ_luck;
    } drop;

    struct {
        int weapon_pct, armor_pct, helm_pct, shield_pct;
        int magic_over, rare_over, per_depth;
        int wpn_min, wpn_max, arm, helm, shld;
    } gear;

    struct {
        int str_lo, str_hi, dex_lo, dex_hi, vit_lo, vit_hi;
        int ac_lo, ac_hi, dmin_lo, dmin_hi, dmax_lo, dmax_hi;
        int life_lo, life_hi, mana_lo, mana_hi, mag_lo, mag_hi;
    } affix;

    struct {
        float champ_hp, champ_xp, champ_ac;
        int champ_dex, champ_gold_mul, dex_base, str_base, xp_per_depth;
    } scale;

    struct {
        int count, extra, gate_tiles;
        int imp_dmin, imp_dmax;
    } wilds;

    struct {
        int per_room, per_depth_div, extra, cap;
        int depth1, depth1_extra, champ_pct, champ_per_depth;
        int reveal, reveal_overworld;
    } dungeon;

    struct {
        float walk, walk_dex;
        float swing, swing_dex, swing_min, swing_max, mob_swing;
        int death_gold_div;
    } feel;
} CryptTune;

extern const CryptTune Crypt;
extern const CryptSpecies crypt_species[];
extern const int crypt_species_n;

#endif
