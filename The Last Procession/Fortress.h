#pragma once
#include "Grid.h"

struct Fortress {
    GridCoord gateCell = { 8, 5 };
    GridCoord coreCell = { 10, 5 };
    int gateHp = 95;
    int gateMaxHp = 95;
    int coreHp = 120;
    int coreMaxHp = 120;
};
