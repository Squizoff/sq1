#include "pch.h"

int leftMouseButtonPressed = 0;

void rotate(float rotX, float rotY) {
    float cosR = cos(rotX), sinR = sin(rotX);
    v3 d = state.dir, p = state.plane;
    state.dir = { d.x * cosR - d.y * sinR, d.x * sinR + d.y * cosR, 0 };
    state.plane = { p.x * cosR - p.y * sinR, p.x * sinR + p.y * cosR, 0 };
    state.pitch += rotY;
    state.pitch = std::max((float)-MAX_PITCH, std::min((float)MAX_PITCH, state.pitch));
}

int check_collision(float x, float y) {
    int mapX = (int)x;
    int mapY = (int)y;
    if (mapX < 0 || mapX >= MAP_SIZE || mapY < 0 || mapY >= MAP_SIZE) return 1;

    int tile = MAPDATA[mapY * MAP_SIZE + mapX];
    return tile != 0 && tile != 2 && tile != 3;
}

void cast_ray() {
    bulletTrail.clear();
    v3 rayDir = state.dir;
    v3 mapPos = { static_cast<float>((int)state.pos.x), static_cast<float>((int)state.pos.y), state.pos.z };

    v3 deltaDist = { fabsf(1.0f / rayDir.x), fabsf(1.0f / rayDir.y), 0.0f };
    v3 sideDist;
    v3 step = {
        static_cast<float>((rayDir.x < 0) ? -1 : 1),
        static_cast<float>((rayDir.y < 0) ? -1 : 1),
        0.0f
    };

    sideDist.x = (rayDir.x < 0) ? (state.pos.x - mapPos.x) * deltaDist.x : (mapPos.x + 1 - state.pos.x) * deltaDist.x;
    sideDist.y = (rayDir.y < 0) ? (state.pos.y - mapPos.y) * deltaDist.y : (mapPos.y + 1 - state.pos.y) * deltaDist.y;

    while (1) {
        if (sideDist.x < sideDist.y) {
            sideDist.x += deltaDist.x;
            mapPos.x += step.x;
        }
        else {
            sideDist.y += deltaDist.y;
            mapPos.y += step.y;
        }

        if (mapPos.x < 0 || mapPos.x >= MAP_SIZE || mapPos.y < 0 || mapPos.y >= MAP_SIZE) break;

        bulletTrail.push_back({ mapPos.x + 0.5f, mapPos.y + 0.5f, mapPos.z + 0.5f });

        int index = (int)mapPos.y * MAP_SIZE + (int)mapPos.x;

        if (MAPDATA[index] != 0) {
            if (MAPDATA[index] == 1) {
                printf("Hit a wall at (%d, %d, %d)\n", (int)mapPos.x, (int)mapPos.y, (int)mapPos.z);
            }
            else if (MAPDATA[index] == 2) {
                MAPDATA[index] = 0;
                printf("Object destroyed at (%d, %d, %d)\n", (int)mapPos.x, (int)mapPos.y, (int)mapPos.z);
            }
            break;
        }
    }
}

void update_player(const uint8_t* keystate) {
    state.velocity.x = MOVE_SPEED * state.deltaTime;
    state.velocity.y = MOVE_SPEED * state.deltaTime;
    if (keystate[SDL_SCANCODE_LSHIFT]) {
        state.velocity.x *= 1.5f;
        state.velocity.y *= 1.5f;
    }

    float newX = state.pos.x;
    float newY = state.pos.y;

    if (keystate[SDL_SCANCODE_W]) {
        newX += state.dir.x * state.velocity.x;
        newY += state.dir.y * state.velocity.y;
    }
    if (keystate[SDL_SCANCODE_S]) {
        newX -= state.dir.x * state.velocity.x;
        newY -= state.dir.y * state.velocity.y;
    }

    if (keystate[SDL_SCANCODE_A]) {
        float sideDirX = -state.dir.y;
        float sideDirY = state.dir.x;
        newX += sideDirX * state.velocity.x;
        newY += sideDirY * state.velocity.y;
    }
    if (keystate[SDL_SCANCODE_D]) {
        float sideDirX = state.dir.y;
        float sideDirY = -state.dir.x;
        newX += sideDirX * state.velocity.x;
        newY += sideDirY * state.velocity.y;
    }

    if (!check_collision(newX, state.pos.y)) {
        state.pos.x = newX;
    }
    if (!check_collision(state.pos.x, newY)) {
        state.pos.y = newY;
    }

    int mouseX, mouseY;
    Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
    SDL_GetRelativeMouseState(&mouseX, &mouseY);

    float rotX = ROT_SPEED * 0.01f * -mouseX;
    float rotY = PITCH_SPEED * -mouseY;

    rotate(rotX, rotY);

    if ((mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) && !leftMouseButtonPressed) {
        leftMouseButtonPressed = 1;
        cast_ray();
    }

    if (!(mouseState & SDL_BUTTON(SDL_BUTTON_LEFT))) {
        leftMouseButtonPressed = 0;
    }
}
