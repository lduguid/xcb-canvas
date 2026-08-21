#include "rpg.h"

#include <stdio.h>
#include <string.h>

static unsigned rng_s = 1u;
static const RpgRules *R;

void rpg_seed(unsigned s)
{
    rng_s = s ? s : 1u;
}

unsigned rpg_randu(void)
{
    rng_s = rng_s * 1664525u + 1013904223u;
    return rng_s;
}

int rpg_rng(int lo, int hi)
{
    unsigned span;

    if (hi <= lo)
        return lo;
    span = (unsigned)(hi - lo + 1);
    return lo + (int)(rpg_randu() % span);
}

void rpg_bind(const RpgRules *rules)
{
    R = rules;
}

const RpgRules *rpg_rules(void)
{
    return R;
}

static int stat_ok(int id)
{
    return id >= 0 && id < RPG_STAT_MAX && R && id < R->stat_n;
}

int rpg_get(const RpgStats *s, int id)
{
    if (!s || !stat_ok(id))
        return 0;
    return s->v[id];
}

void rpg_set(RpgStats *s, int id, int v)
{
    if (!s || !stat_ok(id))
        return;
    s->v[id] = v;
}

void rpg_add(RpgStats *s, int id, int d)
{
    if (!s || !stat_ok(id))
        return;
    s->v[id] += d;
}

int rpg_inv_w(void)
{
    return R && R->inv_w > 0 ? R->inv_w : 0;
}

int rpg_inv_h(void)
{
    return R && R->inv_h > 0 ? R->inv_h : 0;
}

int rpg_inv_n(void)
{
    int n = rpg_inv_w() * rpg_inv_h();

    if (n > RPG_INV_MAX)
        n = RPG_INV_MAX;
    if (n < 0)
        n = 0;
    return n;
}

int rpg_slot_n(void)
{
    int n = R ? R->slot_n : 0;

    if (n > RPG_SLOT_MAX)
        n = RPG_SLOT_MAX;
    if (n < 0)
        n = 0;
    return n;
}

const char *rpg_slot_name(int slot)
{
    if (!R || slot < 0 || slot >= rpg_slot_n())
        return "";
    if (!R->slot_name || !R->slot_name[slot])
        return "";
    return R->slot_name[slot];
}

int rpg_item_ok(const RpgItem *it)
{
    return it && it->kind != RPG_NONE;
}

int rpg_item_mod(const RpgItem *it, int stat)
{
    if (!rpg_item_ok(it) || stat < 0 || stat >= RPG_STAT_MAX)
        return 0;
    return it->mods[stat];
}

static void apply_item(RpgStats *s, const RpgItem *it)
{
    int i, n;

    if (!rpg_item_ok(it) || !R)
        return;
    n = R->stat_n;
    if (n > RPG_STAT_MAX)
        n = RPG_STAT_MAX;
    for (i = 0; i < n; i++)
        s->v[i] += it->mods[i];
}

void rpg_recalc(const RpgStats *base, const RpgInv *inv, RpgStats *live)
{
    int i, hp, mp, hpmax, mpmax;

    if (!live || !base)
        return;
    *live = *base;
    if (R && R->reset_derived)
        R->reset_derived(live);
    if (inv) {
        for (i = 0; i < rpg_slot_n(); i++)
            apply_item(live, &inv->wear[i]);
    }
    if (R && R->derive)
        R->derive(live);
    if (!R)
        return;
    if (R->hp >= 0 && R->hp_max >= 0) {
        hp = rpg_get(base, R->hp);
        hpmax = rpg_get(live, R->hp_max);
        if (hp < 0 || (hp == 0 && rpg_get(base, R->hp_max) == 0))
            hp = hpmax;
        if (hp > hpmax)
            hp = hpmax;
        rpg_set(live, R->hp, hp);
    }
    if (R->mp >= 0 && R->mp_max >= 0) {
        mp = rpg_get(base, R->mp);
        mpmax = rpg_get(live, R->mp_max);
        if (mp < 0 || (mp == 0 && rpg_get(base, R->mp_max) == 0))
            mp = mpmax;
        if (mp > mpmax)
            mp = mpmax;
        rpg_set(live, R->mp, mp);
    }
}

void rpg_hero_refresh(RpgHero *h)
{
    if (!h)
        return;
    rpg_recalc(&h->base, &h->inv, &h->live);
}

void rpg_hero_init(RpgHero *h)
{
    if (!h)
        return;
    memset(h, 0, sizeof(*h));
    if (R && R->fill_base)
        R->fill_base(&h->base);
    rpg_inv_clear(&h->inv);
    rpg_hero_refresh(h);
}

void rpg_hero_sync(RpgHero *h)
{
    if (!h || !R)
        return;
    if (R->hp >= 0)
        rpg_set(&h->base, R->hp, rpg_get(&h->live, R->hp));
    if (R->mp >= 0)
        rpg_set(&h->base, R->mp, rpg_get(&h->live, R->mp));
    if (R->gold >= 0)
        rpg_set(&h->base, R->gold, rpg_get(&h->live, R->gold));
    if (R->xp >= 0)
        rpg_set(&h->base, R->xp, rpg_get(&h->live, R->xp));
    if (R->xp_next >= 0)
        rpg_set(&h->base, R->xp_next, rpg_get(&h->live, R->xp_next));
    if (R->level >= 0)
        rpg_set(&h->base, R->level, rpg_get(&h->live, R->level));
}

int rpg_gain_xp(RpgStats *base, int xp)
{
    int gained = 0, cap;

    if (!base || !R || R->xp < 0 || R->xp_next < 0 || R->level < 0)
        return 0;
    if (xp < 0)
        xp = 0;
    rpg_add(base, R->xp, xp);
    cap = R->max_level > 0 ? R->max_level : 50;
    while (rpg_get(base, R->xp) >= rpg_get(base, R->xp_next) && rpg_get(base, R->level) < cap) {
        rpg_add(base, R->xp, -rpg_get(base, R->xp_next));
        rpg_add(base, R->level, 1);
        gained++;
        if (R->level_up)
            R->level_up(base);
        if (R->xp_to_next)
            rpg_set(base, R->xp_next, R->xp_to_next(rpg_get(base, R->level)));
        if (R->hp >= 0)
            rpg_set(base, R->hp, -1);
        if (R->mp >= 0)
            rpg_set(base, R->mp, -1);
    }
    return gained;
}

static int bump_stat(RpgStats *live, int cur, int max, int n)
{
    int before, v, cap;

    if (!live || cur < 0 || max < 0)
        return 0;
    before = rpg_get(live, cur);
    v = before + n;
    cap = rpg_get(live, max);
    if (v > cap)
        v = cap;
    rpg_set(live, cur, v);
    return v - before;
}

int rpg_heal(RpgStats *live, int n)
{
    return R ? bump_stat(live, R->hp, R->hp_max, n) : 0;
}

int rpg_mana(RpgStats *live, int n)
{
    return R ? bump_stat(live, R->mp, R->mp_max, n) : 0;
}

int rpg_melee(const RpgStats *atk, const RpgStats *def, int *crit)
{
    if (crit)
        *crit = 0;
    if (!R || !R->melee)
        return 0;
    return R->melee(atk, def, crit);
}

void rpg_item_desc(const RpgItem *it, char *buf, size_t n)
{
    if (!buf || n == 0)
        return;
    buf[0] = 0;
    if (!rpg_item_ok(it))
        return;
    if (R && R->describe)
        R->describe(it, buf, n);
    else
        snprintf(buf, n, "%s", it->name);
}

int rpg_item_value(const RpgItem *it)
{
    if (!rpg_item_ok(it) || !R || !R->item_value)
        return 0;
    return R->item_value(it);
}

int rpg_item_price(const RpgItem *it)
{
    if (!rpg_item_ok(it) || !R || !R->item_price)
        return 0;
    return R->item_price(it);
}

void rpg_inv_clear(RpgInv *inv)
{
    if (inv)
        memset(inv, 0, sizeof(*inv));
}

static int stack_onto(RpgItem *dst, RpgItem src)
{
    if (!rpg_item_ok(dst) || dst->kind != src.kind)
        return 0;
    if ((src.flags & RPG_IF_STACK) == 0)
        return 0;
    if (strcmp(dst->name, src.name) != 0)
        return 0;
    dst->stack += src.stack > 0 ? src.stack : 1;
    return 1;
}

int rpg_inv_add(RpgInv *inv, RpgItem it)
{
    int i, n = rpg_inv_n();

    if (!inv || !rpg_item_ok(&it))
        return -1;
    if (it.stack <= 0)
        it.stack = 1;
    if (it.flags & RPG_IF_STACK) {
        for (i = 0; i < n; i++) {
            if (stack_onto(&inv->grid[i], it))
                return 0;
        }
    }
    for (i = 0; i < n; i++) {
        if (!rpg_item_ok(&inv->grid[i])) {
            inv->grid[i] = it;
            return 0;
        }
    }
    return -1;
}

int rpg_inv_remove(RpgInv *inv, int grid_i, RpgItem *out)
{
    if (!inv || grid_i < 0 || grid_i >= rpg_inv_n() || !rpg_item_ok(&inv->grid[grid_i]))
        return -1;
    if (out)
        *out = inv->grid[grid_i];
    memset(&inv->grid[grid_i], 0, sizeof(inv->grid[grid_i]));
    return 0;
}

int rpg_inv_move(RpgInv *from, int grid_i, RpgInv *to)
{
    RpgItem it;

    if (rpg_inv_remove(from, grid_i, &it) != 0)
        return -1;
    if (rpg_inv_add(to, it) != 0) {
        rpg_inv_add(from, it);
        return -1;
    }
    return 0;
}

int rpg_equip(RpgInv *inv, int grid_i)
{
    RpgItem hold, worn;
    int slot;

    if (!inv || grid_i < 0 || grid_i >= rpg_inv_n())
        return -1;
    hold = inv->grid[grid_i];
    slot = hold.slot;
    if (!rpg_item_ok(&hold) || slot < 0 || slot >= rpg_slot_n())
        return -1;
    worn = inv->wear[slot];
    inv->wear[slot] = hold;
    inv->grid[grid_i] = worn;
    return 0;
}

int rpg_unequip(RpgInv *inv, int slot)
{
    RpgItem worn;

    if (!inv || slot < 0 || slot >= rpg_slot_n())
        return -1;
    worn = inv->wear[slot];
    if (!rpg_item_ok(&worn))
        return -1;
    if (rpg_inv_add(inv, worn) != 0)
        return -1;
    memset(&inv->wear[slot], 0, sizeof(worn));
    return 0;
}

int rpg_use(RpgHero *h, RpgItem *it)
{
    int n;

    if (!h || !it || !R || !R->use_item || (it->flags & RPG_IF_USE) == 0)
        return 0;
    n = R->use_item(&h->live, it);
    if (n)
        rpg_hero_sync(h);
    return n;
}

int rpg_buy(RpgHero *h, RpgItem *stock, int stock_n, int i)
{
    RpgItem one;
    int price, g;

    if (!h || !stock || !R || R->gold < 0 || i < 0 || i >= stock_n || !rpg_item_ok(&stock[i]))
        return -1;
    one = stock[i];
    if (one.flags & RPG_IF_STOCK)
        one.stack = 1;
    price = rpg_item_price(&one);
    g = rpg_get(&h->live, R->gold);
    if (g < price)
        return -1;
    if (rpg_inv_add(&h->inv, one) != 0)
        return -1;
    rpg_set(&h->live, R->gold, g - price);
    rpg_set(&h->base, R->gold, g - price);
    if ((stock[i].flags & RPG_IF_STOCK) == 0)
        memset(&stock[i], 0, sizeof(stock[i]));
    rpg_hero_refresh(h);
    return 0;
}

int rpg_sell(RpgHero *h, int grid_i)
{
    RpgItem it;
    int g;

    if (!h || !R || R->gold < 0 || rpg_inv_remove(&h->inv, grid_i, &it) != 0)
        return -1;
    g = rpg_get(&h->live, R->gold) + rpg_item_value(&it);
    rpg_set(&h->live, R->gold, g);
    rpg_set(&h->base, R->gold, g);
    rpg_hero_refresh(h);
    return 0;
}
