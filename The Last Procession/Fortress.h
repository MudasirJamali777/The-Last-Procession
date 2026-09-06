#pragma once
#include "Grid.h"

struct Fortress {
    GridCoord gateCell = { 7, 5 };
    GridCoord coreCell = { 9, 5 };
    int gateHp = 80;
    int gateMaxHp = 80;
    int coreHp = 100;
    int coreMaxHp = 100;
};
