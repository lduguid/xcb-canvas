#ifndef RPG_ACTOR_H
#define RPG_ACTOR_H

#include "dungeon.h"
#include "path.h"
#include "rpg.h"

/* Combatant the engine can walk, detect, and path. Species names, stat
 * formulas, and what an ability *does* belong to the game. `sight` is how
 * far they notice the hero (world units). */

enum { RPG_ACTOR_MAX = 80 };

enum {
    RPG_ROLE_MINION = 0,
    RPG_ROLE_CHAMPION,
    RPG_ROLE_BOSS
};

enum {
    RPG_AI_IDLE = 0,
    RPG_AI_CHASE,
    RPG_AI_FLEE
};

enum {
    RPG_ACT_NONE = 0,
    RPG_ACT_MELEE,
    RPG_ACT_ABILITY
};

typedef struct {
    int alive;
    int kind; /* game species / archetype id */
    int role;
    int ai;
    int ability; /* 0 = none; otherwise game-defined */
    RpgStats st;
    float x, y;
    float vx, vy;
    float gait; /* 0–1 walk cycle, advanced by distance moved */
    int face;   /* -1 left, +1 right */
    float speed, sight, leash, range, radius;
    int see_walls;
    float attack_cd, attack_reload;
    float ability_cd, ability_reload;
    float hurt;
    int path_len, path_i, goal_tx, goal_ty;
    unsigned char path_x[RPG_PATH_MAX], path_y[RPG_PATH_MAX];
} RpgActor;

typedef struct {
    float speed, sight, leash, range, radius;
    float attack_reload, ability_reload;
    int role, ability, see_walls;
} RpgActorFeel;

typedef struct {
    int act;
    int ability;
} RpgAiResult;

void rpg_actor_clear(RpgActor *a);
void rpg_actor_feel(RpgActor *a, const RpgActorFeel *f);
void rpg_actor_note_move(RpgActor *a, float ox, float oy, float dt);
const char *rpg_role_name(int role);

/* Step chase / flee / idle. Moves with dungeon_slide + A* when LOS is blocked.
 * Returns an intent (melee or ability); the game applies damage and VFX. */
RpgAiResult rpg_ai_step(RpgActor *a, const Dungeon *d, float hx, float hy, float dt, int target_ok);

#endif
