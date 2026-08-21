#include "games/crypt_tune.h"

/*
 * Crypt balance knobs — this is the file to edit while playing.
 *
 *   F5  live formulas (HP, melee, walk, drop rates, spawn counts)
 *   F6  new hero (start stats, kit, gold) and a fresh world
 *
 * Keep related numbers together. One value per line.
 */

const CryptTune Crypt = {
    /* starting character (F6) */
    .start = {
        .str = 20,
        .dex = 15,
        .mag = 10,
        .vit = 25,
        .xp_next = 80,
        .gold = 40,
    },
    .kit = {
        .club_min = 3,
        .club_max = 7,
        .hp_pots = 4,
        .mp_pots = 2,
    },
    .bag = {
        .w = 10,
        .h = 4,
        .max_level = 50,
    },

    /* live stats from attributes + gear */
    .derive = {
        .hp_base = 40,
        .hp_per_vit = 3,
        .hp_per_level = 8,
        .mp_base = 12,
        .mp_per_mag = 2,
        .mp_per_level = 4,
        .punch_min = 2, /* unarmed, before STR */
        .punch_max = 4,
        .dmg_str_div_min = 4, /* +STR/n min dmg */
        .dmg_str_div_max = 3,
        .ac_dex_div = 8, /* +DEX/n armor */
    },
    .on_level = {
        .str = 2,
        .dex = 1,
        .vit = 2,
        .mag_even = 1, /* MAG on even levels */
    },
    .xp = {
        .base = 60,
        .per_sq = 18, /* 60 + level^2 * 18 */
    },

    /* to-hit and crit */
    .melee = {
        .hit_base = 70,
        .hit_dex_div = 2, /* + (atkDEX - defDEX) / n */
        .hit_min = 30,
        .hit_max = 94,
        .armor_div = 5, /* damage minus AC/n */
        .crit_base = 5,
        .crit_dex_div = 20,
        .crit_bonus_pct = 50, /* +50% dmg */
    },

    .potion = {
        .hp_base = 40,
        .hp_per_level = 4,
        .mp_base = 30,
        .mp_per_mag = 2,
    },
    .price = {
        .pot_hp_each = 8,
        .pot_mp_each = 10,
        .pot_buy = 25,
        .gear_base = 12,
        .dmg = 2,
        .armor = 4,
        .affix = 6,
        .magic_mul = 2,
        .rare_mul = 4,
        .floor = 5,
        .markup_num = 3, /* vendor buy = value * 3/2 + 8 */
        .markup_den = 2,
        .markup_add = 8,
    },

    /* corpse drops, 0–99 roll (champ subtracts luck, then compares) */
    .drop = {
        .gold_min = 4,
        .gold_max = 12,
        .gold_per_depth = 6,
        .champ_gold_mul = 3,
        .potion_pct = 18,
        .gear_pct = 48,
        .champ_gear_pct = 20,
        .champ_luck = 25,
    },
    .gear = {
        .weapon_pct = 38,
        .armor_pct = 58,
        .helm_pct = 72,
        .shield_pct = 86, /* else ring */
        .magic_over = 55,
        .rare_over = 88,
        .per_depth = 3, /* bump = depth / n */
        .wpn_min = 3,
        .wpn_max = 7,
        .arm = 4,
        .helm = 2,
        .shld = 3,
    },
    .affix = {
        .str_lo = 2, .str_hi = 6,
        .dex_lo = 2, .dex_hi = 6,
        .vit_lo = 2, .vit_hi = 5,
        .ac_lo = 2, .ac_hi = 8,
        .dmin_lo = 1, .dmin_hi = 4,
        .dmax_lo = 2, .dmax_hi = 6,
        .life_lo = 6, .life_hi = 18,
        .mana_lo = 4, .mana_hi = 14,
        .mag_lo = 1, .mag_hi = 4,
    },

    /* how depth / champion scale a species row */
    .scale = {
        .champ_hp = 1.8f,
        .champ_xp = 2.5f,
        .champ_ac = 1.3f,
        .champ_dex = 6,
        .champ_gold_mul = 3,
        .dex_base = 8,
        .str_base = 10,
        .xp_per_depth = 8,
    },

    /* The Wilds */
    .wilds = {
        .count = 5,
        .extra = 3, /* rng 0..extra added */
        .gate_tiles = 14, /* keep imps off the Haven gate */
        .imp_dmin = 1,
        .imp_dmax = 3,
    },
    /* generated floors */
    .dungeon = {
        .per_room = 2,
        .per_depth_div = 3,
        .extra = 2,
        .cap = 7,
        .depth1 = 1,
        .depth1_extra = 1,
        .champ_pct = 8,
        .champ_per_depth = 1,
        .reveal = 8,
        .reveal_overworld = 11,
    },

    /* movement / swing cadence / death tax */
    .feel = {
        .walk = 175.0f,
        .walk_dex = 1.2f,
        .swing = 0.48f,
        .swing_dex = 0.004f,
        .swing_min = 0.28f,
        .swing_max = 0.55f,
        .mob_swing = 0.70f,
        .death_gold_div = 10, /* lose gold/n */
    },
};

/* hp  dmin dmax ac  xp  gold     speed aggro range rad   color
 * tweak a row, F6 into a floor that spawns that species */
const CryptSpecies crypt_species[] = {
    { "Imp",      18, 2,  5,  2, 12, 1,  6,  95.0f, 160.0f, 28.0f, 10.0f, 0.72f, 0.22f, 0.18f },
    { "Skeleton", 32, 4,  9,  6, 22, 2, 10,  80.0f, 190.0f, 32.0f, 11.0f, 0.78f, 0.76f, 0.68f },
    { "Brute",    55, 6, 13, 10, 36, 4, 16,  70.0f, 170.0f, 34.0f, 14.0f, 0.42f, 0.55f, 0.28f },
    { "Shade",    28, 8, 16,  3, 40, 6, 20, 110.0f, 220.0f, 36.0f, 10.0f, 0.42f, 0.28f, 0.62f },
};
const int crypt_species_n = 4;
