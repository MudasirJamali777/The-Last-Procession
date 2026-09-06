#pragma once
#include <raylib.h>

enum class TowerType {
    WatchbowNest
};

struct Tower {
    TowerType type = TowerType::WatchbowNest;
    int gridX = 0;
    int gridY = 0;
    Vector3 pos = { 0.0f, 0.8f, 0.0f };
    float range = 5.4f;
    float cooldown = 0.0f;
    float maxCooldown = 0.72f;
    int damage = 10;
    int cost = 25;
};
