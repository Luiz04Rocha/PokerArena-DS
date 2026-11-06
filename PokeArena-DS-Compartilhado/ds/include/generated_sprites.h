#ifndef GENERATED_SPRITES_H
#define GENERATED_SPRITES_H
#include <stdint.h>
enum { NATIVE_IDLE, NATIVE_WALK, NATIVE_ATTACK, NATIVE_HURT, NATIVE_CELEBRATE, NATIVE_DEFEATED, NATIVE_ANIM_COUNT };
typedef struct {
    const uint16_t *pixels, *frameOffsets;
    uint8_t frames, durations[16], sourceSize, foot[8];
    uint8_t rushTime, hitTime, returnTime;
    const uint8_t *flame;
    const int8_t *anchors;
} NativeAnim;
typedef struct { uint16_t palette[16]; NativeAnim anim[NATIVE_ANIM_COUNT]; const uint16_t *portrait; } NativeSpecies;
extern const NativeSpecies nativeSpecies[11];
#endif
