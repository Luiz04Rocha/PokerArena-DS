#include "arena.h"
#include "game.h"
#include <stdlib.h>

const ArenaProp arenaProps[] = {
    {30, 75, 28, 28, 9, 8, PROP_ROCK},
    {265, 150, 32, 32, 10, 9, PROP_ROCK_MOSS},
    {246, 44, 28, 28, 8, 7, PROP_ROCK},
    {-7, 169, 26, 26, 7, 7, PROP_ROCK_MOSS},
    {268, 216, 32, 32, 11, 10, PROP_ROCK},
    {38, 177, 38, 19, 16, 6, PROP_LOG},
    {223, 211, 38, 19, 15, 6, PROP_LOG},
    {-5, 25, 56, 62, 5, 6, PROP_TREE},
    {33, 20, 56, 62, 5, 6, PROP_TREE},
    {71, 17, 56, 62, 5, 6, PROP_TREE},
    {109, 15, 56, 62, 5, 6, PROP_TREE},
    {147, 15, 56, 62, 5, 6, PROP_TREE},
    {185, 17, 56, 62, 5, 6, PROP_TREE},
    {223, 20, 56, 62, 5, 6, PROP_TREE},
    {261, 25, 56, 62, 5, 6, PROP_TREE},
    {-12, 86, 56, 62, 5, 6, PROP_TREE},
    {265, 86, 48, 53, 6, 7, PROP_TREE},
    {-14, 145, 56, 62, 5, 6, PROP_TREE},
    {270, 145, 56, 62, 5, 6, PROP_TREE},
    {-14, 205, 56, 62, 5, 6, PROP_TREE}
};
const int arenaPropCount = sizeof(arenaProps) / sizeof(arenaProps[0]);

static bool clipAxis(int start, int end, int low, int high, int *enter, int *leave) {
    if ((start <= low && end <= low) || (start >= high && end >= high)) return false;
    if (start == end) return true;
    int delta = end - start;
    int first = ((delta > 0 ? low : high) - start) * ARENA_PATH_FULL / delta;
    int last = ((delta > 0 ? high : low) - start) * ARENA_PATH_FULL / delta;
    if (first > *enter) *enter = first;
    if (last < *leave) *leave = last;
    return *enter <= *leave;
}

int arenaSweep(int x, int y, int tx, int ty, int margin, const ArenaProp **hit) {
    int fraction = ARENA_PATH_FULL;
    if (hit) *hit = NULL;
    for (int i = 0; i < arenaPropCount; ++i) {
        const ArenaProp *prop = &arenaProps[i];
        int enter = 0, leave = fraction;
        if (!clipAxis(x, tx, (prop->x - prop->halfWidth - margin) * POS_SCALE,
                      (prop->x + prop->halfWidth + margin) * POS_SCALE, &enter, &leave)) continue;
        if (!clipAxis(y, ty, (prop->y - prop->depth - margin) * POS_SCALE,
                      (prop->y + margin) * POS_SCALE, &enter, &leave)) continue;
        if (enter < fraction) {
            fraction = enter;
            if (hit) *hit = prop;
        }
    }
    return fraction;
}

int arenaAt(int start, int end, int fraction) {
    return start + (end - start) * fraction / ARENA_PATH_FULL;
}

static int clampPosition(int value, int low, int high) {
    if (value < low * POS_SCALE) return low * POS_SCALE;
    if (value > high * POS_SCALE) return high * POS_SCALE;
    return value;
}

void arenaMove(int *x, int *y, int dx, int dy) {
    if (!dx && !dy) return;
    int tx = clampPosition(*x + dx, ARENA_LEFT, ARENA_RIGHT);
    int ty = clampPosition(*y + dy, ARENA_TOP, ARENA_BOTTOM);
    int fraction = arenaSweep(*x, *y, tx, ty, ARENA_ACTOR_MARGIN, NULL);
    if (fraction == ARENA_PATH_FULL) {
        *x = tx;
        *y = ty;
        return;
    }
    *x = arenaAt(*x, tx, arenaSweep(*x, *y, tx, *y, ARENA_ACTOR_MARGIN, NULL));
    *y = arenaAt(*y, ty, arenaSweep(*x, *y, *x, ty, ARENA_ACTOR_MARGIN, NULL));
}

static int distance(int dx, int dy) {
    int a = abs(dx), b = abs(dy);
    return a > b ? a + b * 106 / 256 : b + a * 106 / 256;
}

void arenaPursue(int *x, int *y, int targetX, int targetY, int speed) {
    const ArenaProp *prop;
    int tx = targetX, ty = targetY;
    arenaSweep(*x, *y, tx, ty, ARENA_ACTOR_MARGIN, &prop);
    if (prop) {
        /* A clear corner lets followers route around a face instead of
         * alternating between the blocked direction and one sidestep. */
        int margin = ARENA_ACTOR_MARGIN + 1;
        int xs[] = {(prop->x - prop->halfWidth - margin) * POS_SCALE,
                    (prop->x + prop->halfWidth + margin) * POS_SCALE};
        int ys[] = {(prop->y - prop->depth - margin) * POS_SCALE,
                    (prop->y + margin) * POS_SCALE};
        bool targetInside = targetX > xs[0] && targetX < xs[1] &&
                            targetY > ys[0] && targetY < ys[1];
        int best = 0x7fffffff;
        for (int i = 0; i < 4; ++i) {
            int cx = xs[i & 1], cy = ys[i >> 1];
            if (clampPosition(cx, ARENA_LEFT, ARENA_RIGHT) != cx ||
                clampPosition(cy, ARENA_TOP, ARENA_BOTTOM) != cy) continue;
            int leg = distance(cx - *x, cy - *y);
            if ((!targetInside && leg == 0) ||
                arenaSweep(*x, *y, cx, cy, ARENA_ACTOR_MARGIN, NULL) < ARENA_PATH_FULL) continue;
            int cost = leg + distance(targetX - cx, targetY - cy);
            if (!targetInside && arenaSweep(cx, cy, targetX, targetY, ARENA_ACTOR_MARGIN, NULL) < ARENA_PATH_FULL)
                cost += 24 * POS_SCALE;
            if (cost < best) {
                best = cost;
                tx = cx;
                ty = cy;
            }
        }
    }
    int dx = tx - *x, dy = ty - *y;
    int remaining = distance(dx, dy);
    if (remaining > speed) {
        dx = dx * speed / remaining;
        dy = dy * speed / remaining;
    }
    arenaMove(x, y, dx, dy);
}
