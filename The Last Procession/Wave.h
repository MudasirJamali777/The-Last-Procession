#pragma once
#include <vector>
#include "Enemy.h"

struct SpawnEntry {
    float spawnTime = 0.0f;
    EnemyType type = EnemyType::AshRaider;
    int laneIndex = 0;
};

struct WaveState {
    int number = 1;
    float timer = 0.0f;
    int nextSpawnIndex = 0;
    bool active = false;
    std::vector<SpawnEntry> spawns;
};
