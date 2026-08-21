# rpg/

ARPG rules engine used by Crypt (`./arpg`). It does **not** include `canvas.h`. Presentation, input, and drawing stay in the game (`games/arpg.c`). A different game can bind the same library with its own stats, slots, and formulas.

## What it owns

| File | Role |
|------|------|
| `rpg.h` / `rpg.c` | Stat bags, items, inventory, equipment, gold/XP, buy/sell/use, melee *dispatch* |
| `loot.h` / `loot.c` | Ground piles (`RpgGround`) |
| `world.h` / `world.c` | Town / overworld / dungeon graph and named places |
| `dungeon.h` / `dungeon.c` | 72×72 tile maps, generation, walk/collision |

Stats are `RpgStats { int v[RPG_STAT_MAX]; }`, not hardcoded STR/DEX fields. Items have `kind`, `rarity`, `slot` (−1 if not wearable), `flags`, `mods[]`, and `name`. Inventory size and slot count come from the bound `RpgRules`.

Call `rpg_bind(&rules)` every frame from the game. Do not store function pointers from the plugin on the heap — F5 would leave dangling pointers.

## Binding a game

Fill `RpgRules` with:

- How many stats, bag width/height, wear slots, max level
- Indices for HP, gold, XP, and so on (`−1` if unused)
- Callbacks: `fill_base`, `derive`, `melee`, prices, potions, item text

Crypt does this in `games/crypt.c` (`crypt_bind()` at the start of `update`). Balance numbers live in `games/crypt_tune.c`.

## Collision

`dungeon_walk` is floor vs wall. `dungeon_slide` moves a point with a radius and stops on walls. The game must apply that as the **only** position change (do not also add sprite velocity, or you will walk through tiles).

## License

Code here is part of xcb-canvas. Art used by Crypt is separate — see `assets/crypt/CREDITS` and the Crypt section in the root `README.md`.
