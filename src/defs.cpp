#include "pch.h"

int MAP_SIZE;
uint8_t* MAPDATA = new uint8_t[MAP_SIZE * MAP_SIZE];

std::vector<v3> bulletTrail;
std::vector<DLight> dynamicLights;

GameState state;