#ifndef GENERATED_VISUALS_H
#define GENERATED_VISUALS_H
#include <stdint.h>
#include "visuals.h"
typedef struct { uint16_t palette[16]; const uint16_t *frames[8][6]; } NativeEffect;
extern const NativeEffect nativeEffects[FX_COUNT];
extern const uint16_t native_grass[4096],native_dirt[4096];
extern const uint8_t hudFont[96][8];
#endif
