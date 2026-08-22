#ifndef RPG_WORLD_H
#define RPG_WORLD_H

#include "dungeon.h"
#include "rpg.h"

/* Generic town / overworld / dungeon graph. Place names and layout are
 * game content; this is just the travel model. */

enum {
    RPG_ZONE_TOWN = 0,
    RPG_ZONE_OVERWORLD,
    RPG_ZONE_DUNGEON,
    RPG_ZONE_N
};

enum {
    RPG_PLACE_NONE = 0,
    RPG_PLACE_VENDOR,
    RPG_PLACE_BANK,
    RPG_PLACE_QUEST,
    RPG_PLACE_CAMP,
    RPG_PLACE_GATE,
    RPG_PLACE_PORTAL,
    RPG_PLACE_EXIT,
    RPG_PLACE_STAIRS
};

enum { RPG_PLACE_MAX = 12, RPG_VENDOR_N = 8 };

typedef struct {
    int kind;
    int tx, ty;
    int dest_zone;
    int dest_id;
    int dest_depth;
    char name[32];
    char prompt[48];
} RpgPlace;

typedef struct {
    int zone;
    int dungeon_id;
    int depth;
    Dungeon maps[RPG_ZONE_N];
    unsigned char seen[RPG_ZONE_N][DUN_H][DUN_W];
    RpgPlace places[RPG_ZONE_N][RPG_PLACE_MAX];
    int place_n[RPG_ZONE_N];
    RpgInv bank;
    RpgItem vendor[RPG_VENDOR_N];
} RpgWorld;

int rpg_zone_safe(int zone);
const char *rpg_zone_name(int zone);

void rpg_world_clear(RpgWorld *w);
RpgPlace *rpg_world_add_place(RpgWorld *w, int zone, int kind, int tx, int ty, const char *name, const char *prompt);
void rpg_world_bind_dungeon(RpgWorld *w, int dungeon_id, int depth);
void rpg_world_reveal_all(RpgWorld *w, int zone);
void rpg_world_reveal_around(RpgWorld *w, int zone, int cx, int cy, int rad);

int rpg_place_near(const RpgWorld *w, float x, float y, float rad);
RpgPlace *rpg_place_kind(RpgWorld *w, int zone, int kind);

#endif
