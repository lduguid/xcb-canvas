#include "path.h"

#include <string.h>

#define INF 0x3fffffff

typedef struct {
    unsigned short x, y;
    int f;
} Node;

static int heur(int x, int y, int gx, int gy)
{
    int dx = x - gx, dy = y - gy;
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    return dx + dy;
}

int dungeon_astar(const Dungeon *d, int sx, int sy, int gx, int gy, unsigned char *ox, unsigned char *oy, int maxn)
{
    static int gscore[DUN_H][DUN_W];
    static unsigned char closed[DUN_H][DUN_W];
    static short came_x[DUN_H][DUN_W], came_y[DUN_H][DUN_W];
    static Node heap[DUN_W * DUN_H];
    static const int dirx[4] = { 1, -1, 0, 0 };
    static const int diry[4] = { 0, 0, 1, -1 };
    int heap_n = 0, x, y, i, n, cx, cy;

    if (!d || !ox || !oy || maxn <= 0)
        return 0;
    if (!dungeon_walk(d, gx, gy))
        return 0;
    if (!dungeon_walk(d, sx, sy)) {
        for (i = 0; i < 4; i++) {
            if (dungeon_walk(d, sx + dirx[i], sy + diry[i])) {
                sx += dirx[i];
                sy += diry[i];
                break;
            }
        }
        if (!dungeon_walk(d, sx, sy))
            return 0;
    }
    if (sx == gx && sy == gy)
        return 0;

    for (y = 0; y < DUN_H; y++) {
        for (x = 0; x < DUN_W; x++) {
            gscore[y][x] = INF;
            closed[y][x] = 0;
            came_x[y][x] = -1;
            came_y[y][x] = -1;
        }
    }

    gscore[sy][sx] = 0;
    heap[0].x = (unsigned short)sx;
    heap[0].y = (unsigned short)sy;
    heap[0].f = heur(sx, sy, gx, gy);
    heap_n = 1;

    while (heap_n > 0) {
        int best = 0, nx, ny, ng, f;
        Node cur;

        for (i = 1; i < heap_n; i++) {
            if (heap[i].f < heap[best].f)
                best = i;
        }
        cur = heap[best];
        heap[best] = heap[--heap_n];
        cx = cur.x;
        cy = cur.y;
        if (closed[cy][cx])
            continue;
        closed[cy][cx] = 1;
        if (cx == gx && cy == gy)
            break;
        for (i = 0; i < 4; i++) {
            nx = cx + dirx[i];
            ny = cy + diry[i];
            if (!dungeon_walk(d, nx, ny) || closed[ny][nx])
                continue;
            ng = gscore[cy][cx] + 1;
            if (ng >= gscore[ny][nx])
                continue;
            gscore[ny][nx] = ng;
            came_x[ny][nx] = (short)cx;
            came_y[ny][nx] = (short)cy;
            f = ng + heur(nx, ny, gx, gy);
            if (heap_n < DUN_W * DUN_H) {
                heap[heap_n].x = (unsigned short)nx;
                heap[heap_n].y = (unsigned short)ny;
                heap[heap_n].f = f;
                heap_n++;
            }
        }
    }

    if (gscore[gy][gx] >= INF)
        return 0;

    {
        int rx[RPG_PATH_MAX], ry[RPG_PATH_MAX], rn = 0, px, py;

        x = gx;
        y = gy;
        while (!(x == sx && y == sy) && rn < RPG_PATH_MAX) {
            rx[rn] = x;
            ry[rn] = y;
            rn++;
            px = came_x[y][x];
            py = came_y[y][x];
            if (px < 0 || py < 0)
                break;
            x = px;
            y = py;
        }
        n = rn < maxn ? rn : maxn;
        for (i = 0; i < n; i++) {
            ox[i] = (unsigned char)rx[rn - 1 - i];
            oy[i] = (unsigned char)ry[rn - 1 - i];
        }
        return n;
    }
}
