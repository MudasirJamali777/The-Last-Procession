#pragma once
#include "Grid.h"

struct Fortress {
    GridCoord coreCell = { 8, 5 };
    int coreHp = 100;
    int coreMaxHp = 100;
    int gateHp = 60;
    int gateMaxHp = 60;
};
