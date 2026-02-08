#ifndef DEFS_H
#define DEFS_H

#include <SDL2/SDL.h>
#include <vector>

#include "player.h"
#include "renderer.h"
#include "textures.h"
#include "utils.h"

#undef min
#undef max

constexpr int USE_GPU = 1;
#if 0
constexpr float DESCALER = 0.38f;
constexpr int SCREEN_WIDTH = static_cast<int>(1280 * DESCALER);
constexpr int SCREEN_HEIGHT = static_cast<int>(720 * DESCALER);
#else
constexpr int SCREEN_WIDTH = 320;
constexpr int SCREEN_HEIGHT = 200;
#endif
constexpr float MOVE_SPEED = 5.0f;
constexpr float ROT_SPEED = 0.1f;
constexpr float PITCH_SPEED = 0.3f;
constexpr int MAX_PITCH = 90;
constexpr int count_t = 6;

extern int MAP_SIZE;
extern uint8_t* MAPDATA;

extern std::vector<v3> bulletTrail;

extern std::vector<DLight> dynamicLights;

struct GameState {
    SDL_Window* window = NULL;
    SDL_Texture* texture = NULL;
    SDL_Renderer* renderer = NULL;
    uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT] = { 0 };
    v3 pos = { 0.0f, 0.0f, 0.0f };
    v3 dir = { 0.0f, 0.0f, 0.0f };
    v3 plane = { 0.0f, 0.0f, 0.0f };
    SDL_Surface* textures[count_t] = { NULL };
    int tex_width[count_t];
    int tex_height[count_t];
    float deltaTime = 0.0f;
    float pitch = 0.0f;
    v3 velocity = { 0.0f, 0.0f, 0.0f };
};

extern GameState state;

#endif
