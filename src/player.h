#ifndef PLAYER_H
#define PLAYER_H

#include <cstdint>

int check_collision(float x, float y);
void update_player(const uint8_t* keystate);

#endif