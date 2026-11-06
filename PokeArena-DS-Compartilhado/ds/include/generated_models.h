#ifndef GENERATED_MODELS_H
#define GENERATED_MODELS_H
#include <stdint.h>
typedef struct { int16_t x[3],y[3],z[3]; uint16_t color; } NativeTriangle;
typedef struct { int count; const NativeTriangle *triangles; } NativeMesh;
extern const NativeMesh nativeMeshes[4];
#endif
