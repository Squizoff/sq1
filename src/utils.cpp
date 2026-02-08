#include "pch.h"

float clamp(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

void add_dynamic_light(float x, float y, float radius, RGBA color, float intensity, BlinkPattern pattern) {
    DLight newLight = { x, y, radius, color, intensity, pattern, 0.0f };
    dynamicLights.push_back(newLight);
}
