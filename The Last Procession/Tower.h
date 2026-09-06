#pragma once
#include <raylib.h>

enum class TowerType {
    WatchbowNest,
    CenserShrine,
    ReliquarySpire,
    PilgrimBarricade
};

struct Tower {
    TowerType type = TowerType::WatchbowNest;
    int gridX = 0;
    int gridY = 0;
    int level = 1;
    Vector3 pos = { 0.0f, 0.8f, 0.0f };
    float range = 5.4f;
    float cooldown = 0.0f;
    float maxCooldown = 0.72f;
    int damage = 10;
    int goldCost = 25;
    int ironCost = 0;
    int emberCost = 0;
    Color color = { 194, 172, 118, 255 };
};
