#ifndef RPG_PATH_H
#define RPG_PATH_H

#include "dungeon.h"

/* Tile A* on a Dungeon. Game AI and click-to-move can share this.
 * Returns number of waypoints written (not including the start tile). 0 if none. */

enum { RPG_PATH_MAX = 192 };

int dungeon_astar(const Dungeon *d, int sx, int sy, int gx, int gy, unsigned char *ox, unsigned char *oy, int maxn);

#endif
