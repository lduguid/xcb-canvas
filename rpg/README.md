# rpg/

Generic ARPG engine. It does **not** include `canvas.h`. A game binds `RpgRules` and supplies names, numbers, art, input, and combat effects. Crypt (`./arpg`) is one such game; another can use the same library with different stats, classes, and maps.

Ids are integers the game defines (`ST_STR`, `IT_HPOT`, `CLASS_MAGE`, …). The engine never stores class names, spell names, or species names.

Call `rpg_bind(&rules)` **every frame**. Do not copy rule function pointers onto the heap — F5 rebuilds the plugin and would leave dangling pointers.

## Files

| File | Role |
|------|------|
| `rpg.h` / `rpg.c` | Stats, items, bags, equipment, class/skill/talent build, 5-slot action bar, gold/XP, shop, combat *pipeline* |
| `loot.h` / `loot.c` | Ground piles |
| `world.h` / `world.c` | Town / overworld / dungeon graph, places, fog, vendor stock, bank stash |
| `dungeon.h` / `dungeon.c` | 72×72 tile maps, generation, walk / slide / LOS |
| `path.h` / `path.c` | Tile A* (`dungeon_astar`) |
| `actor.h` / `actor.c` | Combatants: roles, chase/flee, path follow, melee vs ability *intent* |

Caps that matter to a game: `RPG_STAT_MAX` 32, `RPG_INV_MAX` 80, `RPG_SLOT_MAX` 12, `RPG_BAR_N` 5, `RPG_BOOK_MAX` 32 (skills and talents each), `RPG_ACTOR_MAX` 80, `RPG_LOOT_MAX` 96, `RPG_PLACE_MAX` 12, `RPG_VENDOR_N` 8, `RPG_PATH_MAX` 192. Bag size is `min(inv_w * inv_h, RPG_INV_MAX)`.

## Binding `RpgRules`

Fill:

- `stat_n`, `inv_w`, `inv_h`, `slot_n`, `max_level`, `slot_name[]`
- Indices into `RpgStats.v` the engine uses: `hp`, `hp_max`, `mp`, `mp_max`, `gold`, `level`, `xp`, `xp_next` (`−1` = unused)
- Callbacks: `fill_base`, `reset_derived`, `derive`, `level_up`, `xp_to_next`, `combat` scripts (or fallback `melee`), `item_value`, `item_price`, `use_item`, `describe`
- Optional catalogs: `classes` / `class_n`, `skills` / `skill_n`, `talents` / `talent_n`, `talent_per_level`, `terrain` / `terrain_n`
- Optional `apply_build` — extra live-stat work after `derive`

With no catalog (`class_n` / `skill_n` / `talent_n` = 0), the hero still stores class id, skill ids, talent ids, and unspent points. The engine does not gate or apply catalog mods until you publish tables.

Crypt binds in `games/crypt.c` (`crypt_bind()` at the start of `update`). Balance lives in `games/crypt_tune.c`.

## Stats

`RpgStats` is `int v[RPG_STAT_MAX]`, not named STR/DEX fields. `rpg_get` / `rpg_set` / `rpg_add` no-op unless rules are bound and `id < stat_n`.

A hero has `base` (persistent) and `live` (this frame after gear and build).

**`rpg_hero_refresh` pipeline**

1. `live = base`
2. `reset_derived(live)` — zero computed fields (armor, HP max, …) so they are not stacked twice
3. Add worn-item `mods[]`
4. Add catalog skill/talent `mods[] * rank` (only if that id is in the bound catalog)
5. `derive(live)` — game computes HP max from VIT, etc.
6. Clamp current HP/MP to the new maxima
7. `apply_build(live, hero)` if set — nonlinear / post-derive extras
8. Clamp HP/MP again

`rpg_recalc(base, inv, live)` is gear only (steps 1–3, 5–6). Prefer `rpg_hero_refresh` once the hero has a build.

If `base` HP (or MP) is `−1`, or both current and max are `0`, refresh fills current to max. `rpg_gain_xp` sets HP/MP to `−1` on level so the hero heals to the new cap.

`rpg_hero_sync` copies live HP, MP, gold, XP, next, and level back onto `base`. Use it after potions or other live-only changes you want to keep.

`rpg_heal` / `rpg_mana` bump live current, capped at max.

## Combat

The engine owns the **pipeline**. The game owns the **numbers and scripts**. Stats used in a swing (hit, dodge, armor, resist, …) are ordinary `RpgStats` slots the game defines. `rpg_resolve` never names DEX or fire.

Fill `RpgRules.combat` with C callbacks (bound every frame, same as `derive`). NULL skips that phase.

**`rpg_resolve(attack, &hit)` order**

1. Dodge chance (skipped if `RPG_AF_CANT_DODGE`)
2. Parry chance (melee typically; skipped if `RPG_AF_CANT_PARRY`)
3. To-hit chance (fail → miss; skipped if `RPG_AF_ALWAYS_HIT`)
4. Block chance — still a connect; `outcome` becomes `RPG_HIT_BLOCK`
5. `roll_damage` — raw roll; `attack.power` is a hint the script may add
6. Crit chance, then `crit_apply`
7. `block_apply` if blocked
8. `armor` (physical mitigation; a script can no-op on elemental `dtype`)
9. `resist` (typed reduction)
10. `floor_dmg` (or clamp at 0)

Chance scripts return 0–100 (`< 0` skips). The engine rolls `1..100`. Damage scripts return the new amount.

`RpgAttack`: attacker/defender stat bags, `style` (`RPG_STYLE_MELEE` / `SPELL`), game `dtype`, optional `skill` / `power` / `flags`.

`RpgHit`: `outcome` (`MISS` / `DODGE` / `PARRY` / `BLOCK` / `HIT`), `dmg`, `raw`, `mitigated`, `crit`, `dtype`. `rpg_hit_connected` is true for HIT and BLOCK.

`rpg_melee(atk, def, &crit)` is a thin melee resolve for callers that only want a damage int (`0` = avoided). Prefer `rpg_resolve` when the HUD should say Dodge vs Miss.

If every combat hook is NULL, resolve falls back to the old single `melee` callback.

Attack **period** is not inside `rpg_resolve`. Actors use `attack_reload`; the hero swing timer is the game’s (Crypt: `crypt_swing_period` from `ST_ASPD`).

## Hero

```
RpgHero { base, live, inv, bar, build }
RpgBuild { class_id, talent_unspent, skills, talents }
```

`rpg_hero_init` zeros the hero, calls `fill_base`, clears the bag, refreshes. Class is `0` (none). The game then gives kit, gold, and optionally `rpg_class_set`.

## Items, bag, equipment

`RpgItem`: `kind` (0 = empty), `rarity`, `stack`, `slot` (`−1` if not wearable), `flags`, `mods[RPG_STAT_MAX]`, `name`.

Flags: `RPG_IF_STACK`, `RPG_IF_USE`, `RPG_IF_STOCK` (vendor keeps a copy after a buy).

Suggested rarities `RPG_WHITE` / `RPG_MAGIC` / `RPG_RARE` are only numbers; the game may use others.

- `rpg_inv_add` — stack onto the first matching `kind`+`name` if `RPG_IF_STACK`, else first empty cell. `0` ok, `−1` full. Stack `≤ 0` becomes 1.
- `rpg_inv_remove` / `rpg_inv_move` — `0` ok, `−1` fail (move puts the item back if the destination is full)
- `rpg_inv_find(kind)` — first bag index, or `−1`
- `rpg_inv_count(kind)` — sum of stacks
- `rpg_equip(grid_i)` — swap bag cell with `wear[item.slot]`. Fails if `slot` is `−1` or out of range
- `rpg_unequip(slot)` — worn item back into the bag; `−1` if bag full
- `rpg_use` — requires `RPG_IF_USE` and `use_item`; on success syncs live → base. The callback decides stack decrement and the effect

`item_value` is sell gold; `item_price` is buy gold. `describe` fills tooltip text.

## Gold and shop

Gold is whatever stat index `rules.gold` points at (`−1` disables shop).

- `rpg_buy(hero, stock, n, i)` — `0` ok, `−1` if poor, full, or bad index. `RPG_IF_STOCK` buys one and leaves the stall item; otherwise the stall slot is cleared
- `rpg_sell(hero, grid_i)` — removes that bag cell, adds `item_value`

Vendor rows live on `RpgWorld.vendor[RPG_VENDOR_N]`. The bank is `RpgWorld.bank` (an `RpgInv`). The game moves items with `rpg_inv_move`.

## XP and levels

`rpg_gain_xp(base, xp)` adds XP and levels while `xp >= xp_next` and `level < max_level` (default cap 50). Each level: subtract `xp_next`, `+1` level, `level_up(base)`, set next from `xp_to_next`, set HP/MP to `−1`. Returns how many levels were gained. Does **not** refresh live stats or award talent points.

`rpg_hero_gain_xp(hero, xp)` does that, then:

- `talent_unspent += levels * talent_per_level` (if `talent_per_level > 0`)
- auto-learns catalog skills whose `grant_level` is `> 0`, `≤` the new level, and allowed for this class
- `rpg_hero_refresh`

Use the hero form in play. Use the stats form only if you are leveling a bag of numbers with no build.

## Class, skills, talents

`RpgBook` is a packed list of `{id, rank}` (`n` used). The action bar stores a **skill id**, not a book index, so the list can compact.

Low-level list ops (`rpg_book_*`) do not consult catalogs. Hero-level ops do.

### Class

`class_id` `0` is none. Any other id is legal if there is no class catalog; with a catalog it must appear in `RpgClassInfo`.

`rpg_class_set(hero, id)` sets the id and **respecs**. `−1` if the id is unknown.

`rpg_build_respec`:

- clears skills and talents
- unbinds every action-bar **skill** (item binds stay)
- `talent_unspent =` that class’s `start_pts` (or `0` if no class row)
- learns `start_skill[0..7]` at rank 1
- grants `grant_level` skills for the current level
- refreshes

`RpgClassInfo`: `id`, `start_pts`, `start_skill[RPG_CLASS_START]`. No name field.

### Skills

`RpgSkillInfo`: `id`, `class_id` (`0` = any class), `max_rank` (`0` means 1), `grant_level` (`0` = never auto), `flags`, `mods[]`.

Flags: `RPG_SF_ACTIVE` may sit on the bar; `RPG_SF_PASSIVE` only may not. If flags are `0`, the skill is bindable. Catalog `mods[]` still apply on refresh for any known catalogued skill (rank times), whether or not `PASSIVE` is set.

| Call | Effect |
|------|--------|
| `rpg_skill_ok` | this class may use this id (no catalog ⇒ yes) |
| `rpg_skill_learn` | ensure rank 1 |
| `rpg_skill_train` | `+1` rank, capped (`99` if no catalog row) |
| `rpg_skill_forget` | drop it and unbind matching bar slots |
| `rpg_skill_known` / `rpg_skill_rank` | query |

Hero-level learn/train/forget refresh stats. `0` ok, `−1` fail.

The engine does **not** spend mana or play VFX. That is the game, when the bar returns “cast this”.

### Talents

`RpgTalentInfo`: `id`, `class_id`, `max_rank`, `req_level`, `req_id` (another talent or skill, or `0`), `req_rank`, `cost` (`0` means 1), `mods[]`.

`rpg_talent_ok` is true when the next rank is allowed **and** `talent_unspent >= cost`. With a catalog it also checks class, level, and prereq rank (`req_rank` `0` means 1). With no catalog: cap 99, cost 1, no other gates.

`rpg_talent_take` spends the cost, `+1` rank, refreshes. `rpg_talent_grant(hero, n)` adds unspent points (quests / debug). `rpg_talent_unspent` / `rpg_talent_rank` query.

Put nonlinear talent effects in `apply_build`, not in `mods[]`.

## Action bar

Five slots. Each is `{type, id}`:

- `RPG_BAR_EMPTY`
- `RPG_BAR_ITEM` — `id` is an **item kind**. Activate finds the first bag stack of that kind
- `RPG_BAR_SKILL` — `id` is a **skill id**. The engine does not cast

`rpg_bar_bind_item` / `rpg_bar_bind_skill` / `rpg_bar_put` write a slot with no extra checks. `rpg_hero_bar_put` is what the HUD should use: a skill bind is ignored unless the hero knows it and it is bindable (not passive-only).

`rpg_bar_activate(hero, slot)`:

- `1` — used a `RPG_IF_USE` item, or equipped a wearable
- `0` — empty, missing stack, unknown/unusable skill
- `−1` — known bindable skill; **the game should cast**

Keys, drag, and drawing stay in the game. Drop both bag items and spellbook cells through `rpg_hero_bar_put`. Drag a bar slot onto another slot with `rpg_bar_swap`; drag off or right-click with `rpg_bar_unbind`.

## Ground loot

`RpgDrop { gold, item }`. `rpg_ground_add` returns the pile index, or `−1` if the field is full or the drop is empty.

`rpg_ground_at(x, y, rad)` — nearest pile index, or `−1`.

`rpg_ground_take(ground, i, hero)`:

- `1` — pile gone (gold and/or item taken)
- `2` — gold taken, item still on the ground (pack full)
- `0` — nothing happened

Gold is added to live and base. A successful take refreshes the hero. The game draws piles.

## World

Three zones: `RPG_ZONE_TOWN`, `RPG_ZONE_OVERWORLD`, `RPG_ZONE_DUNGEON`. Only town is `rpg_zone_safe`. Each zone has its own `Dungeon`, fog (`seen[zone][ty][tx]`), and place list.

Places (`RpgPlace`): `kind`, tile `tx,ty`, optional `dest_zone` / `dest_id` / `dest_depth`, `name`, `prompt`. Kinds are `VENDOR`, `BANK`, `QUEST`, `CAMP`, `GATE`, `PORTAL`, `EXIT`, `STAIRS`. The engine stores them; the game decides what Space does (buy, rest, enter, …).

`rpg_world_add_place` returns `NULL` if that zone already has `RPG_PLACE_MAX` places. `rpg_place_near(world, x, y, rad)` uses tile centers (`dungeon_tile_pos`). `rpg_place_kind` finds the first of that kind in a zone.

`rpg_world_reveal_all` / `rpg_world_reveal_around` stamp fog. Drawing unseen tiles is the game.

`rpg_world_bind_dungeon(id, depth)` sets current dungeon id/depth, clears dungeon places, and adds an `EXIT` on `start_tx,ty` plus `STAIRS` on `stair_tx,ty`. Call it after you generate or copy that map. Depth is at least 1.

`rpg_zone_name` returns `"Town"` / `"Overworld"` / `"Dungeon"` — generic labels, not Crypt names.

## Dungeon and movement

Maps are `DUN_W`×`DUN_H` (72×72). `DUN_WALL` `0`, `DUN_FLOOR` `1`. Tile ids `2+` are game-defined. Bind a `RpgTerrain` row per id on `RpgRules.terrain` (`terrain_n` is the table length). World units: `DUN_TILE` is **32**, regardless of PNG cell size. `dungeon_pos_tile` / `dungeon_tile_pos` convert (tile center = `(tx+0.5, ty+0.5) * 32`).

`RpgTerrain`: `flags` (`RPG_TF_WALK`, `RPG_TF_BLOCK_LOS`, `RPG_TF_HAZARD`), A* `cost` (`0` → `1`), `speed_pct` (`0` → `100`, clamp 8–200), and hazard `dtype` / `power` for the game to feed `rpg_resolve`. Names, tints, and which tiles get stamped stay in the game. `rpg_terrain(kind)` looks up the bound row.

`dungeon_walk` — in bounds and walkable (`DUN_FLOOR`, or `RPG_TF_WALK`). `dungeon_opaque` — blocks LOS (`DUN_WALL` or `RPG_TF_BLOCK_LOS`). Walkable hazards do not block sight. `dungeon_blocked(x,y,rad)` — any of the four corners of the radius sits on a non-walkable tile. `dungeon_speed_at` / `dungeon_step_cost` read the tile under a point.

**`dungeon_slide` is the only way positions should change.** It tries full `(dx,dy)`, then X only, then Y only. Heroes, clicks, and AI should all go through it so collision stays consistent. Scale the step by `dungeon_speed_at` so mud/ice apply; the actor AI already does this.

`dungeon_line_clear` steps every 8 units and rejects `dungeon_opaque` tiles. `dungeon_gen(seed)` carves up to `DUN_ROOMS` (14) rooms of `DUN_FLOOR`, sets `start_*` to room 0 center and `stair_*` to the last room. It calls `rpg_seed(seed)` and therefore resets the global RNG. Stamp extra tile kinds after gen.

`dungeon_random_floor` / `dungeon_room_floor` pick spawn tiles (any walkable kind).

## Path

`dungeon_astar(d, sx,sy, gx,gy, ox,oy, maxn)` writes waypoints **after** the start tile. Return value is the count (`0` if none). 4-connected, walkable tiles only; each step adds `dungeon_step_cost` so AI prefers around hazards. Hero click-to-move and mob chase can share it. Actors store their own path (`path_x/y`, `path_len`, `path_i`).

## Actors

`RpgActor` is a combatant the engine can walk and detect. `kind` is a game species id. `ability` is a game ability id (`0` = none). `role` is `RPG_ROLE_MINION` / `CHAMPION` / `BOSS`. `rpg_role_name` is `""` / `"Champion"` / `"Boss"`.

`rpg_actor_feel` copies a feel row onto the actor. Defaults if a field is unset: `speed` at least 1, `sight` 96 if `≤ 1`, `leash` = `sight * 1.65` if `≤ 1`, `attack_reload` 0.7, `ability_reload` 4.

Units: `speed` and `sight` / `leash` / `range` are **world units** (tile = 32). `range ≤ 1` is treated as 28.

`rpg_ai_step(actor, map, hx, hy, dt, target_ok)` moves the actor and returns an **intent**:

| `act` | Meaning |
|-------|---------|
| `RPG_ACT_NONE` | idle / moving / no target |
| `RPG_ACT_MELEE` | in range, attack cooldown ready — game applies `rpg_melee` and VFX |
| `RPG_ACT_ABILITY` | in range, ability set and its cooldown ready — game applies the effect (`result.ability`) |

AI, when `target_ok`:

- Idle → chase if `dist ≤ sight` and (LOS or `see_walls`)
- Chase → idle if `dist > leash`
- In `range`: fire ability if ready, else melee if ready; does not step closer
- Else: slide toward the hero on LOS; A* around walls if not
- Flee (`RPG_AI_FLEE`): slide away until beyond leash

`see_walls` only affects **detection**. They still path around walls.

Every step calls `rpg_actor_note_move`, which sets `vx,vy`, `face` (`±1`), and `gait` `0–1` from **distance moved** (`DUN_TILE * 1.5` per cycle). Faster actors cycle faster. The game draws bob / facing from that. Do not set `x,y` yourself if you also call `rpg_ai_step` — it already slides.

`target_ok` 0 forces idle (safe zones, dead hero, …).

## RNG

`rpg_seed` / `rpg_randu` / `rpg_rng(lo, hi)` inclusive. Shared by dungeon gen, loot rolls you route through it, and anything else in the plugin. `dungeon_gen` reseeds.

## What the game still does

- Bind rules every frame; keep catalogs as **static or stack** data, not heap copies of function pointers
- Name stats, items, classes, skills, talents, species
- Draw, input, audio, HUD (bag, spellbook, hotbar, talent screen)
- Cast when `rpg_bar_activate` returns `−1`
- Combat scripts and which stats they read; apply HP and VFX from `RpgHit` after `rpg_ai_step`
- Interpret places (open vendor, rest, enter portal, …)
- Snapshot dungeon floors if leave/return should keep mobs, loot, and fog
- Choose tilesets; world size is always 32-unit tiles

## License

Code here is part of xcb-canvas. Art used by Crypt is separate — see `assets/crypt/CREDITS` and the Crypt section in the root `README.md`.
