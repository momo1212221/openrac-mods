#pragma once
#include <stdint.h>
// Minimal ABI mirror for rendering unit tests; production uses the real SDK.
typedef struct openrac_overlay {
    uint32_t size;
    void* context;
    int (*project)(void*, const float[3], float*, float*);
    void (*rect)(void*, float, float, float, float, uint32_t);
    void (*text)(void*, float, float, float, uint32_t, const char*);
    float (*fade)(void*, float, float, float);
    float (*text_width)(void*, float, const char*);
} openrac_overlay;
