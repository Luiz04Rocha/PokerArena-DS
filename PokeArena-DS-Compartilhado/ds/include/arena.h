#ifndef ARENA_LAYOUT_H
#define ARENA_LAYOUT_H

enum {
    ARENA_LEFT = 10, ARENA_RIGHT = 246, ARENA_TOP = 49, ARENA_BOTTOM = 190,
    ARENA_ACTOR_MARGIN = 4, ARENA_PATH_FULL = 1024
};

typedef enum { PROP_ROCK, PROP_ROCK_MOSS, PROP_LOG, PROP_TREE } ArenaPropKind;
typedef struct {
    int x, y, width, height;
    int halfWidth, depth;  /* Footprint extends back from the sprite's baseline. */
    ArenaPropKind kind;
} ArenaProp;

extern const ArenaProp arenaProps[];
extern const int arenaPropCount;

/* Sweeps use game coordinates (POS_SCALE units per pixel). A blocked path
 * returns a fraction of ARENA_PATH_FULL and, optionally, the prop it hit. */
int arenaSweep(int x, int y, int tx, int ty, int margin, const ArenaProp **hit);
int arenaAt(int start, int end, int fraction);
void arenaMove(int *x, int *y, int dx, int dy);
void arenaPursue(int *x, int *y, int targetX, int targetY, int speed);

#endif
