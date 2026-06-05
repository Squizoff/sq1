#include "pch.h"

int MAP_SIZE = 0;
Cell* MAPDATA = nullptr;

std::vector<v3> bulletTrail;
std::vector<DLight> dynamicLights;

GameState state;