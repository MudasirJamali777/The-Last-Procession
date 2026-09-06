#pragma once
#include <raylib.h>

enum class EnemyType {
    AshRaider
};

struct Enemy {
    EnemyType type = EnemyType::AshRaider;
    Vector3 pos = { 0.0f, 0.55f, 0.0f };
    int hp = 28;
    int maxHp = 28;
    float speed = 2.4f;
    float hitFlash = 0.0f;
    int pathIndex = 0;
};
