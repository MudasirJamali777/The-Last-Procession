#pragma once
#include <raylib.h>

enum class EnemyType {
    AshRaider,
    GraveBrute
};

struct Enemy {
    EnemyType type = EnemyType::AshRaider;
    Vector3 pos = { 0.0f, 0.55f, 0.0f };
    int hp = 28;
    int maxHp = 28;
    float speed = 2.0f;
    float hitFlash = 0.0f;
    float slowTimer = 0.0f;
    int laneIndex = 0;
    int pathIndex = 0;
    float attackTimer = 0.0f;
    float attackCooldown = 0.8f;
    int gateDamage = 6;
    int coreDamage = 8;
    bool pastGate = false;
};
