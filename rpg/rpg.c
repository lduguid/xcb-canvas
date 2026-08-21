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

const RpgTerrain *rpg_terrain(int kind)
{
    if (!R || !R->terrain || kind < 0 || kind >= R->terrain_n)
        return NULL;
    return &R->terrain[kind];
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

static void apply_mods_rank(RpgStats *s, const int *mods, int rank)
{
    int i, n;

    if (!s || !mods || rank <= 0 || !R)
        return;
    n = R->stat_n;
    if (n > RPG_STAT_MAX)
        n = RPG_STAT_MAX;
    for (i = 0; i < n; i++)
        s->v[i] += mods[i] * rank;
}

static void live_from_base(RpgStats *live, const RpgStats *base)
{
    *live = *base;
    if (R && R->reset_derived)
        R->reset_derived(live);
}

static void apply_gear(RpgStats *live, const RpgInv *inv)
{
    int i;

    if (!inv)
        return;
    for (i = 0; i < rpg_slot_n(); i++)
        apply_item(live, &inv->wear[i]);
}

static void clamp_vitals(RpgStats *live, const RpgStats *base)
{
    int hp, mp, hpmax, mpmax;

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

static void finish_live(RpgStats *live, const RpgStats *base)
{
    if (R && R->derive)
        R->derive(live);
    clamp_vitals(live, base);
}

static const RpgSkillInfo *find_skill(int id);
static const RpgTalentInfo *find_talent(int id);

static void apply_build_mods(RpgStats *live, const RpgHero *h)
{
    int i;
    const RpgSkillInfo *s;
    const RpgTalentInfo *t;

    if (!h)
        return;
    for (i = 0; i < h->build.skills.n; i++) {
        s = find_skill(h->build.skills.v[i].id);
        if (s)
            apply_mods_rank(live, s->mods, h->build.skills.v[i].rank);
    }
    for (i = 0; i < h->build.talents.n; i++) {
        t = find_talent(h->build.talents.v[i].id);
        if (t)
            apply_mods_rank(live, t->mods, h->build.talents.v[i].rank);
    }
}

void rpg_recalc(const RpgStats *base, const RpgInv *inv, RpgStats *live)
{
    if (!live || !base)
        return;
    live_from_base(live, base);
    apply_gear(live, inv);
    finish_live(live, base);
}

void rpg_hero_refresh(RpgHero *h)
{
    if (!h)
        return;
    live_from_base(&h->live, &h->base);
    apply_gear(&h->live, &h->inv);
    apply_build_mods(&h->live, h);
    finish_live(&h->live, &h->base);
    if (R && R->apply_build)
        R->apply_build(&h->live, h);
    clamp_vitals(&h->live, &h->base);
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

static int combat_ready(void)
{
    const RpgCombat *c;

    if (!R)
        return 0;
    c = &R->combat;
    return c->dodge || c->parry || c->to_hit || c->block || c->roll_damage || c->armor || c->resist;
}

static int roll_pct(int chance)
{
    if (chance <= 0)
        return 0;
    return rpg_rng(1, 100) <= chance;
}

int rpg_hit_connected(const RpgHit *h)
{
    return h && (h->outcome == RPG_HIT_HIT || h->outcome == RPG_HIT_BLOCK);
}

int rpg_resolve(const RpgAttack *a, RpgHit *out)
{
    const RpgCombat *c;
    int chance, blocked = 0, dmg = 0, raw = 0;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!a || !a->atk || !a->def || !out)
        return 0;
    out->dtype = a->dtype;
    if (!combat_ready()) {
        if (R && R->melee) {
            out->dmg = R->melee(a->atk, a->def, &out->crit);
            out->raw = out->dmg;
            out->outcome = out->dmg > 0 ? RPG_HIT_HIT : RPG_HIT_MISS;
            return 1;
        }
        return 0;
    }
    c = &R->combat;
    if (!(a->flags & RPG_AF_CANT_DODGE) && c->dodge) {
        chance = c->dodge(a);
        if (chance >= 0 && roll_pct(chance)) {
            out->outcome = RPG_HIT_DODGE;
            return 1;
        }
    }
    if (!(a->flags & RPG_AF_CANT_PARRY) && c->parry) {
        chance = c->parry(a);
        if (chance >= 0 && roll_pct(chance)) {
            out->outcome = RPG_HIT_PARRY;
            return 1;
        }
    }
    if (!(a->flags & RPG_AF_ALWAYS_HIT) && c->to_hit) {
        chance = c->to_hit(a);
        if (chance >= 0 && !roll_pct(chance)) {
            out->outcome = RPG_HIT_MISS;
            return 1;
        }
    }
    if (!(a->flags & RPG_AF_CANT_BLOCK) && c->block) {
        chance = c->block(a);
        if (chance >= 0 && roll_pct(chance))
            blocked = 1;
    }
    if (c->roll_damage)
        dmg = c->roll_damage(a);
    raw = dmg;
    if (c->crit_chance && roll_pct(c->crit_chance(a))) {
        out->crit = 1;
        if (c->crit_apply)
            dmg = c->crit_apply(a, dmg);
    }
    if (blocked && c->block_apply)
        dmg = c->block_apply(a, dmg);
    if (c->armor)
        dmg = c->armor(a, dmg);
    if (c->resist)
        dmg = c->resist(a, dmg);
    if (c->floor_dmg)
        dmg = c->floor_dmg(a, dmg);
    else if (dmg < 0)
        dmg = 0;
    out->raw = raw;
    out->dmg = dmg;
    out->mitigated = raw - dmg;
    if (out->mitigated < 0)
        out->mitigated = 0;
    out->outcome = blocked ? RPG_HIT_BLOCK : RPG_HIT_HIT;
    return 1;
}

int rpg_melee(const RpgStats *atk, const RpgStats *def, int *crit)
{
    RpgAttack a;
    RpgHit h;

    memset(&a, 0, sizeof(a));
    a.atk = atk;
    a.def = def;
    a.style = RPG_STYLE_MELEE;
    if (!rpg_resolve(&a, &h)) {
        if (crit)
            *crit = 0;
        return 0;
    }
    if (crit)
        *crit = h.crit;
    return rpg_hit_connected(&h) ? h.dmg : 0;
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

int rpg_inv_find(const RpgInv *inv, int kind)
{
    int i, n = rpg_inv_n();

    if (!inv || kind == RPG_NONE)
        return -1;
    for (i = 0; i < n; i++) {
        if (inv->grid[i].kind == kind)
            return i;
    }
    return -1;
}

int rpg_inv_count(const RpgInv *inv, int kind)
{
    int i, n = rpg_inv_n(), t = 0;

    if (!inv || kind == RPG_NONE)
        return 0;
    for (i = 0; i < n; i++) {
        if (inv->grid[i].kind != kind)
            continue;
        t += inv->grid[i].stack > 0 ? inv->grid[i].stack : 1;
    }
    return t;
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

void rpg_bar_clear(RpgBar *b)
{
    if (b)
        memset(b, 0, sizeof(*b));
}

static int bar_ok(int slot)
{
    return slot >= 0 && slot < RPG_BAR_N;
}

void rpg_bar_bind_item(RpgBar *b, int slot, int kind)
{
    if (!b || !bar_ok(slot) || kind == RPG_NONE)
        return;
    b->slot[slot].type = RPG_BAR_ITEM;
    b->slot[slot].id = kind;
}

void rpg_bar_bind_skill(RpgBar *b, int slot, int skill)
{
    if (!b || !bar_ok(slot) || skill == 0)
        return;
    b->slot[slot].type = RPG_BAR_SKILL;
    b->slot[slot].id = skill;
}

void rpg_bar_put(RpgBar *b, int slot, int type, int id)
{
    if (!b || !bar_ok(slot))
        return;
    if (type == RPG_BAR_ITEM)
        rpg_bar_bind_item(b, slot, id);
    else if (type == RPG_BAR_SKILL)
        rpg_bar_bind_skill(b, slot, id);
    else
        rpg_bar_unbind(b, slot);
}

void rpg_book_clear(RpgBook *b)
{
    if (b)
        memset(b, 0, sizeof(*b));
}

int rpg_book_n(const RpgBook *b)
{
    return b ? b->n : 0;
}

int rpg_book_id(const RpgBook *b, int i)
{
    if (!b || i < 0 || i >= b->n)
        return 0;
    return b->v[i].id;
}

int rpg_book_rank_at(const RpgBook *b, int i)
{
    if (!b || i < 0 || i >= b->n)
        return 0;
    return b->v[i].rank;
}

int rpg_book_get(const RpgBook *b, int id)
{
    int i;

    if (!b || id == 0)
        return 0;
    for (i = 0; i < b->n; i++) {
        if (b->v[i].id == id)
            return b->v[i].rank;
    }
    return 0;
}

int rpg_book_has(const RpgBook *b, int id)
{
    return rpg_book_get(b, id) > 0;
}

int rpg_book_forget(RpgBook *b, int id)
{
    int i, j;

    if (!b || id == 0)
        return -1;
    for (i = 0; i < b->n; i++) {
        if (b->v[i].id != id)
            continue;
        for (j = i; j < b->n - 1; j++)
            b->v[j] = b->v[j + 1];
        memset(&b->v[--b->n], 0, sizeof(b->v[0]));
        return 0;
    }
    return -1;
}

int rpg_book_set(RpgBook *b, int id, int rank)
{
    int i;

    if (!b || id == 0)
        return -1;
    if (rank <= 0)
        return rpg_book_forget(b, id);
    for (i = 0; i < b->n; i++) {
        if (b->v[i].id == id) {
            b->v[i].rank = rank;
            return 0;
        }
    }
    if (b->n >= RPG_BOOK_MAX)
        return -1;
    b->v[b->n].id = id;
    b->v[b->n].rank = rank;
    b->n++;
    return 0;
}

int rpg_book_learn(RpgBook *b, int id)
{
    if (rpg_book_has(b, id))
        return 0;
    return rpg_book_set(b, id, 1);
}

static int class_allows(int have, int owner)
{
    return owner == 0 || owner == have;
}

static int rank_cap(int max_rank)
{
    return max_rank > 0 ? max_rank : 1;
}

static int talent_cost(const RpgTalentInfo *t)
{
    if (!t || t->cost <= 0)
        return 1;
    return t->cost;
}

static const RpgClassInfo *find_class(int id)
{
    int i;

    if (!R || !R->classes || id == 0)
        return NULL;
    for (i = 0; i < R->class_n; i++) {
        if (R->classes[i].id == id)
            return &R->classes[i];
    }
    return NULL;
}

static const RpgSkillInfo *find_skill(int id)
{
    int i;

    if (!R || !R->skills || id == 0)
        return NULL;
    for (i = 0; i < R->skill_n; i++) {
        if (R->skills[i].id == id)
            return &R->skills[i];
    }
    return NULL;
}

static const RpgTalentInfo *find_talent(int id)
{
    int i;

    if (!R || !R->talents || id == 0)
        return NULL;
    for (i = 0; i < R->talent_n; i++) {
        if (R->talents[i].id == id)
            return &R->talents[i];
    }
    return NULL;
}

const RpgClassInfo *rpg_class_info(int id)
{
    return find_class(id);
}

const RpgSkillInfo *rpg_skill_info(int id)
{
    return find_skill(id);
}

const RpgTalentInfo *rpg_talent_info(int id)
{
    return find_talent(id);
}

static int hero_level(const RpgHero *h)
{
    if (!h || !R || R->level < 0)
        return 0;
    return rpg_get(&h->base, R->level);
}

static int prereq_rank(const RpgHero *h, int id)
{
    int r;

    if (!h || id == 0)
        return 0;
    r = rpg_book_get(&h->build.talents, id);
    if (r > 0)
        return r;
    return rpg_book_get(&h->build.skills, id);
}

static void bar_unbind_skill(RpgHero *h, int skill)
{
    int i;

    if (!h)
        return;
    for (i = 0; i < RPG_BAR_N; i++) {
        if (h->bar.slot[i].type == RPG_BAR_SKILL && (skill == 0 || h->bar.slot[i].id == skill))
            rpg_bar_unbind(&h->bar, i);
    }
}

static int skill_bindable(int id)
{
    const RpgSkillInfo *s = find_skill(id);

    if (!s)
        return 1;
    if (s->flags & RPG_SF_ACTIVE)
        return 1;
    if (s->flags & RPG_SF_PASSIVE)
        return 0;
    return 1;
}

void rpg_hero_bar_put(RpgHero *h, int slot, int type, int id)
{
    if (!h)
        return;
    if (type == RPG_BAR_SKILL && (!rpg_book_has(&h->build.skills, id) || !skill_bindable(id)))
        return;
    rpg_bar_put(&h->bar, slot, type, id);
}

int rpg_class_get(const RpgHero *h)
{
    return h ? h->build.class_id : 0;
}

int rpg_talent_unspent(const RpgHero *h)
{
    return h ? h->build.talent_unspent : 0;
}

void rpg_talent_grant(RpgHero *h, int n)
{
    if (h && n > 0)
        h->build.talent_unspent += n;
}

int rpg_class_ok(int class_id)
{
    if (class_id == 0)
        return 1;
    if (!R || R->class_n <= 0)
        return 1;
    return find_class(class_id) != NULL;
}

static void grant_level_skills(RpgHero *h)
{
    int i, lv;
    const RpgSkillInfo *s;

    if (!h || !R || !R->skills)
        return;
    lv = hero_level(h);
    for (i = 0; i < R->skill_n; i++) {
        s = &R->skills[i];
        if (s->id == 0 || s->grant_level <= 0 || lv < s->grant_level)
            continue;
        if (!class_allows(h->build.class_id, s->class_id))
            continue;
        if (!rpg_book_has(&h->build.skills, s->id))
            rpg_book_set(&h->build.skills, s->id, 1);
    }
}

void rpg_build_respec(RpgHero *h)
{
    const RpgClassInfo *c;
    int i;

    if (!h)
        return;
    rpg_book_clear(&h->build.skills);
    rpg_book_clear(&h->build.talents);
    bar_unbind_skill(h, 0);
    c = find_class(h->build.class_id);
    h->build.talent_unspent = c ? c->start_pts : 0;
    if (c) {
        for (i = 0; i < RPG_CLASS_START; i++) {
            if (c->start_skill[i])
                rpg_book_set(&h->build.skills, c->start_skill[i], 1);
        }
    }
    grant_level_skills(h);
    rpg_hero_refresh(h);
}

int rpg_class_set(RpgHero *h, int class_id)
{
    if (!h || !rpg_class_ok(class_id))
        return -1;
    h->build.class_id = class_id;
    rpg_build_respec(h);
    return 0;
}

int rpg_skill_known(const RpgHero *h, int id)
{
    return h && rpg_book_has(&h->build.skills, id);
}

int rpg_skill_rank(const RpgHero *h, int id)
{
    return h ? rpg_book_get(&h->build.skills, id) : 0;
}

int rpg_skill_ok(const RpgHero *h, int id)
{
    const RpgSkillInfo *s;

    if (!h || id == 0)
        return 0;
    if (!R || R->skill_n <= 0)
        return 1;
    s = find_skill(id);
    return s && class_allows(h->build.class_id, s->class_id);
}

int rpg_skill_learn(RpgHero *h, int id)
{
    if (!h || !rpg_skill_ok(h, id))
        return -1;
    if (rpg_book_learn(&h->build.skills, id) != 0)
        return -1;
    rpg_hero_refresh(h);
    return 0;
}

int rpg_skill_train(RpgHero *h, int id)
{
    const RpgSkillInfo *s;
    int cur, cap;

    if (!h || !rpg_skill_ok(h, id))
        return -1;
    s = find_skill(id);
    cap = s ? rank_cap(s->max_rank) : 99;
    cur = rpg_book_get(&h->build.skills, id);
    if (cur >= cap)
        return -1;
    if (rpg_book_set(&h->build.skills, id, cur + 1) != 0)
        return -1;
    rpg_hero_refresh(h);
    return 0;
}

int rpg_skill_forget(RpgHero *h, int id)
{
    if (!h || rpg_book_forget(&h->build.skills, id) != 0)
        return -1;
    bar_unbind_skill(h, id);
    rpg_hero_refresh(h);
    return 0;
}

int rpg_talent_rank(const RpgHero *h, int id)
{
    return h ? rpg_book_get(&h->build.talents, id) : 0;
}

int rpg_talent_ok(const RpgHero *h, int id)
{
    const RpgTalentInfo *t;
    int cur, cap, need, req;

    if (!h || id == 0)
        return 0;
    t = find_talent(id);
    if (R && R->talent_n > 0) {
        if (!t || !class_allows(h->build.class_id, t->class_id))
            return 0;
        if (t->req_level > 0 && hero_level(h) < t->req_level)
            return 0;
        req = t->req_rank > 0 ? t->req_rank : 1;
        if (t->req_id && prereq_rank(h, t->req_id) < req)
            return 0;
        cap = rank_cap(t->max_rank);
        need = talent_cost(t);
    } else {
        cap = 99;
        need = 1;
    }
    cur = rpg_book_get(&h->build.talents, id);
    if (cur >= cap)
        return 0;
    return h->build.talent_unspent >= need;
}

int rpg_talent_take(RpgHero *h, int id)
{
    const RpgTalentInfo *t;
    int cur, need;

    if (!rpg_talent_ok(h, id))
        return -1;
    t = find_talent(id);
    need = talent_cost(t);
    cur = rpg_book_get(&h->build.talents, id);
    if (rpg_book_set(&h->build.talents, id, cur + 1) != 0)
        return -1;
    h->build.talent_unspent -= need;
    rpg_hero_refresh(h);
    return 0;
}

int rpg_hero_gain_xp(RpgHero *h, int xp)
{
    int n;

    if (!h)
        return 0;
    n = rpg_gain_xp(&h->base, xp);
    if (n && R && R->talent_per_level > 0)
        h->build.talent_unspent += n * R->talent_per_level;
    if (n)
        grant_level_skills(h);
    rpg_hero_refresh(h);
    return n;
}

void rpg_bar_unbind(RpgBar *b, int slot)
{
    if (!b || !bar_ok(slot))
        return;
    memset(&b->slot[slot], 0, sizeof(b->slot[slot]));
}

void rpg_bar_swap(RpgBar *bar, int ia, int ib)
{
    RpgBarSlot t;

    if (!bar || !bar_ok(ia) || !bar_ok(ib) || ia == ib)
        return;
    t = bar->slot[ia];
    bar->slot[ia] = bar->slot[ib];
    bar->slot[ib] = t;
}

int rpg_bar_activate(RpgHero *h, int slot)
{
    RpgBarSlot *s;
    int gi;

    if (!h || !bar_ok(slot))
        return 0;
    s = &h->bar.slot[slot];
    if (s->type == RPG_BAR_SKILL)
        return rpg_skill_known(h, s->id) && skill_bindable(s->id) ? -1 : 0;
    if (s->type != RPG_BAR_ITEM || s->id == RPG_NONE)
        return 0;
    gi = rpg_inv_find(&h->inv, s->id);
    if (gi < 0)
        return 0;
    if (h->inv.grid[gi].flags & RPG_IF_USE)
        return rpg_use(h, &h->inv.grid[gi]) ? 1 : 0;
    if (rpg_equip(&h->inv, gi) == 0)
        return 1;
    return 0;
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
