#ifndef RPG_DUNGEON_H
#define RPG_DUNGEON_H

enum { DUN_W = 72, DUN_H = 72, DUN_ROOMS = 14 };

enum { DUN_WALL = 0, DUN_FLOOR = 1 }; /* 2+ are game tile kinds via RpgTerrain */

#define DUN_TILE 32.0f

void dungeon_pos_tile(float x, float y, int *tx, int *ty);
void dungeon_tile_pos(int tx, int ty, float *x, float *y);

typedef struct {
    int x, y, w, h;
} DunRoom;

typedef struct {
    unsigned char tile[DUN_H][DUN_W];
    DunRoom rooms[DUN_ROOMS];
    int room_n;
    int start_tx, start_ty;
    int stair_tx, stair_ty;
} Dungeon;

void dungeon_fill(Dungeon *d, unsigned char t);
void dungeon_rect(Dungeon *d, int x, int y, int w, int h, unsigned char t);
void dungeon_set(Dungeon *d, int tx, int ty, unsigned char t);
int dungeon_get(const Dungeon *d, int tx, int ty);

void dungeon_gen(Dungeon *d, unsigned seed);
int dungeon_walk(const Dungeon *d, int tx, int ty);
int dungeon_opaque(const Dungeon *d, int tx, int ty);
int dungeon_step_cost(const Dungeon *d, int tx, int ty);
float dungeon_speed_at(const Dungeon *d, float x, float y);
int dungeon_blocked(const Dungeon *d, float x, float y, float rad);
void dungeon_slide(const Dungeon *d, float *x, float *y, float dx, float dy, float rad);
int dungeon_line_clear(const Dungeon *d, float x0, float y0, float x1, float y1);
int dungeon_random_floor(const Dungeon *d, int *tx, int *ty);
int dungeon_room_floor(const Dungeon *d, int room, int *tx, int *ty);

#endif
