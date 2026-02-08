#ifndef UTILS_H
#define UTILS_H

#include <cstdint>

#ifndef M_PI_4
#define M_PI_4 0.7853981633974483
#endif

#define DEG2RAD(angle) ((angle) * 0.017453292519943295)

struct RGBA {
    uint8_t r, g, b, a = 255;
};

enum BlinkPattern {
    CONSTANT,
    PULSE,
    FLICKER
};

struct DLight {
    float x, y;
    float radius;
    RGBA color;
    float intensity;
    BlinkPattern pattern;
    float time;
};

typedef struct { float x, y, z; } v3;

float clamp(float value, float min_val, float max_val);

void add_dynamic_light(float x, float y, float radius, RGBA color, float intensity, BlinkPattern pattern);

#endif