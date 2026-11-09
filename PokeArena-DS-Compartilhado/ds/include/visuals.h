#ifndef ARENA_VISUALS_H
#define ARENA_VISUALS_H
#include <stdint.h>
#include <stdbool.h>
enum { FX_FIRE_BOLT, FX_FIRE_BURST, FX_FIRE_DASH, FX_FIRE_BEAM, FX_FIRE_ULT,
       FX_WATER_BOLT, FX_WATER_BURST, FX_WATER_DASH, FX_WATER_BEAM, FX_WATER_ULT,
       FX_VINE, FX_LEAF, FX_DUST, FX_PETALS, FX_SOLAR,
       FX_SPARK, FX_DISCHARGE, FX_QUICK, FX_THUNDER, FX_STORM, FX_IMPACT, FX_EVOLVE,
       FX_COUNT, FX_FRAMES=6, VISUAL_DIRECTIONS=8 };
typedef struct {
    int species, anim, facing, elapsed, rateRemainder;
    int idleTime, walkTime, walkGrace, idleBlend;
    uint32_t serial;
    bool once, hurtRequested;
} VisualPose;
typedef struct { int x,y,zoom; } VisualCamera; /* Q8 screen coordinates and zoom. */
int visualGroundY(int y);
int visualRandomPeriod(uint32_t *state);
void visualCameraStep(VisualCamera *camera,int playerX,int playerY,int enemyX,int enemyY,
                      bool fight,bool frozen,int ultimateTicks);
int visualLerp(int oldValue,int newValue,int half);
int visualDirection(int x,int y);
int visualTargetFacing(int current,int dx,int dy);
int visualStyle(int species,int slot);
int visualFrame(const uint8_t *durations,int count,int elapsed,bool loop);
void visualPoseStep(VisualPose *pose,int species,int requested,int facing,uint32_t serial,int delta,int duration);
#endif
