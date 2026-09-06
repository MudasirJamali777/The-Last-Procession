#include "Game.h"
#include "Utils.h"
#include <algorithm>
#include <cmath>

Game::Game() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenW, screenH, "THE LAST PROCESSION");

    int monitor = GetCurrentMonitor();
    int monitorW = GetMonitorWidth(monitor);
    int monitorH = GetMonitorHeight(monitor);
    screenW = (int)(monitorW * 0.82f);
    screenH = (int)(monitorH * 0.82f);
    if (screenW < 1280) screenW = 1280;
    if (screenH < 720) screenH = 720;
    SetWindowSize(screenW, screenH);
    SetWindowPosition((monitorW - screenW) / 2, (monitorH - screenH) / 2);

    SetTargetFPS(60);

    camera.position = { 40.0f, 34.0f, 40.0f };
    camera.target = { 0.0f, 0.45f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 36.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    ResetRun();
}

Game::~Game() {
    if (IsWindowReady()) {
        CloseWindow();
    }
}

void Game::Run() {
    while (!WindowShouldClose()) {
        screenW = GetScreenWidth();
        screenH = GetScreenHeight();

        float dt = GetFrameTime();
        if (dt > 0.033f) dt = 0.033f;

        Update(dt);
        Draw();
    }
}

void Game::ResetRun() {
    gold = 140;
    iron = 95;
    ember = 28;
    fervor = 0;
    hymnTimer = 0.0f;
    worldTime = 0.0f;
    towers.clear();
    enemies.clear();
    shots.clear();
    deathFx.clear();
    fortress = Fortress{};
    state = PlayState::BuildPhase;
    buildChoice = BuildChoice::WatchbowNest;
    announcement = "MEGA PROCESSION MAP // 3 ROADS  HUGE FORTRESS  WIDE SIEGE FIELD";
    announcementTimer = 4.8f;
    hoveredValid = false;
    hoveredCell = { -1, -1 };
    hoveredTowerIndex = -1;
    cameraZoom = 40.0f;
    BuildMap();
    BuildWave(1);
}

void Game::AddProp(PropType type, int x, int y, bool blockCell) {
    if (!grid.InBounds(x, y)) return;

    props.push_back({ type, { x, y } });
    if (blockCell) {
        GridTile& tile = grid.At(x, y);
        if (tile.kind == TileKind::Buildable) tile.kind = TileKind::Blocked;
        tile.occupied = true;
        tile.height = std::max(tile.height, 0.34f);
    }
}

void Game::BuildMap() {
    grid.width = 44;
    grid.height = 34;
    grid.cellSize = 2.4f;
    grid.origin = { -52.8f, 0.0f, -40.8f };
    grid.tiles.assign((size_t)grid.width * (size_t)grid.height, GridTile{});
    props.clear();

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            GridTile& tile = grid.At(x, y);
            tile.kind = TileKind::Buildable;
            tile.occupied = false;
            float ridgeX = std::fabs((float)x - (float)grid.width * 0.5f) * 0.006f;
            float ridgeY = std::fabs((float)y - (float)grid.height * 0.5f) * 0.005f;
            float variation = 0.02f * (float)((x * 3 + y * 5) % 4);
            tile.height = 0.16f + ridgeX + ridgeY + variation;
        }
    }

    lanes.clear();
    lanes.resize(3);

    auto addSegment = [&](std::vector<GridCoord>& lane, int x0, int y0, int x1, int y1) {
        int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
        if (steps <= 0) {
            if (lane.empty() || lane.back().x != x0 || lane.back().y != y0) {
                lane.push_back({ x0, y0 });
            }
            return;
        }

        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / (float)steps;
            int x = (int)std::round((float)x0 + (float)(x1 - x0) * t);
            int y = (int)std::round((float)y0 + (float)(y1 - y0) * t);
            if (lane.empty() || lane.back().x != x || lane.back().y != y) {
                lane.push_back({ x, y });
            }
        }
        };

    addSegment(lanes[0], 0, 5, 10, 5);
    addSegment(lanes[0], 10, 5, 16, 6);
    addSegment(lanes[0], 16, 6, 21, 8);
    addSegment(lanes[0], 21, 8, 26, 11);
    addSegment(lanes[0], 26, 11, 28, 16);

    addSegment(lanes[1], 0, 16, 10, 16);
    addSegment(lanes[1], 10, 16, 16, 16);
    addSegment(lanes[1], 16, 16, 22, 16);
    addSegment(lanes[1], 22, 16, 26, 16);
    addSegment(lanes[1], 26, 16, 28, 16);

    addSegment(lanes[2], 0, 28, 10, 28);
    addSegment(lanes[2], 10, 28, 16, 25);
    addSegment(lanes[2], 16, 25, 21, 22);
    addSegment(lanes[2], 21, 22, 26, 19);
    addSegment(lanes[2], 26, 19, 28, 16);

    for (const std::vector<GridCoord>& lane : lanes) {
        for (int i = 0; i < (int)lane.size(); ++i) {
            const GridCoord& c = lane[i];
            if (!grid.InBounds(c.x, c.y)) continue;
            GridTile& tile = grid.At(c.x, c.y);
            tile.kind = (i == 0) ? TileKind::Spawn : TileKind::Road;
            tile.occupied = false;
            tile.height = (i == 0) ? 0.05f : 0.07f;
        }
    }

    fortress.gateCell = { 28, 16 };
    fortress.coreCell = { 35, 16 };

    for (int y = 11; y <= 21; ++y) {
        for (int x = 31; x <= 40; ++x) {
            GridTile& tile = grid.At(x, y);
            tile.kind = TileKind::Fortress;
            tile.occupied = true;
            tile.height = 0.12f;
        }
    }

    const GridCoord courtRoad[] = {
        { 28, 16 }, { 29, 16 }, { 30, 16 }, { 31, 16 }, { 32, 16 }, { 33, 16 },
        { 31, 15 }, { 31, 17 }, { 32, 15 }, { 32, 17 }, { 33, 15 }, { 33, 17 }
    };
    for (const GridCoord& c : courtRoad) {
        GridTile& tile = grid.At(c.x, c.y);
        tile.kind = TileKind::Road;
        tile.occupied = false;
        tile.height = 0.05f;
    }

    auto blockBuildable = [&](int x, int y) {
        if (!grid.InBounds(x, y)) return;
        GridTile& tile = grid.At(x, y);
        if (tile.kind == TileKind::Buildable) {
            tile.kind = TileKind::Blocked;
            tile.occupied = true;
            tile.height = 0.42f;
        }
        };

    for (int x = 2; x <= 18; x += 4) blockBuildable(x, 2 + (x % 3));
    for (int x = 4; x <= 22; x += 5) blockBuildable(x, 10 + (x % 4));
    for (int x = 6; x <= 24; x += 4) blockBuildable(x, 30 - (x % 5));
    for (int y = 4; y <= 28; y += 4) blockBuildable(38, y);
    for (int y = 6; y <= 26; y += 5) blockBuildable(41, y);
    blockBuildable(27, 10); blockBuildable(27, 22); blockBuildable(30, 9); blockBuildable(30, 23);

    int deadTrees[][2] = { {2,3},{6,2},{10,3},{14,2},{18,3},{8,12},{16,10},{12,29},{20,27},{38,8},{41,11},{38,24} };
    for (auto& p : deadTrees) AddProp(PropType::DeadTree, p[0], p[1], true);
    int graves[][2] = { {5,31},{9,31},{13,31},{38,5},{41,6},{38,28},{41,27},{39,16} };
    for (auto& p : graves) AddProp(PropType::GraveMarker, p[0], p[1], true);
    int rubble[][2] = { {4,11},{9,13},{15,12},{18,28},{27,10},{27,22},{30,9},{30,23} };
    for (auto& p : rubble) AddProp(PropType::RubblePile, p[0], p[1], true);
    int carts[][2] = { {29,11},{29,21},{41,14},{41,18} };
    for (auto& p : carts) AddProp(PropType::CartWreck, p[0], p[1], true);
    int braziers[][2] = { {31,12},{31,20},{34,11},{34,21},{40,12},{40,20},{29,14},{29,18} };
    for (auto& p : braziers) AddProp(PropType::Brazier, p[0], p[1], false);
    int banners[][2] = { {32,11},{37,11},{32,21},{37,21},{40,14},{40,18} };
    for (auto& p : banners) AddProp(PropType::BannerPole, p[0], p[1], false);

    cameraFocus = grid.CellCenter(21, 16);
}

void Game::BuildWave(int waveNumber) {
    wave = WaveState{};
    wave.number = waveNumber;
    wave.active = false;

    int laneCount = (int)lanes.size();
    if (laneCount <= 0) laneCount = 1;

    int count = 15 + (waveNumber - 1) * 3;
    bool bossWave = (waveNumber % 5 == 0);

    for (int i = 0; i < count; ++i) {
        SpawnEntry entry{};
        entry.spawnTime = 0.56f * i;
        entry.laneIndex = i % laneCount;
        entry.type = EnemyType::AshRaider;

        if (waveNumber >= 2 && (i % 4 == 3)) {
            entry.type = EnemyType::GraveBrute;
            entry.spawnTime += 0.10f;
        }
        if (waveNumber >= 3 && (i % 5 == 2)) {
            entry.type = EnemyType::BannerKnight;
        }
        if (waveNumber >= 6 && (i % 3 == 0)) {
            entry.spawnTime -= 0.05f;
        }
        wave.spawns.push_back(entry);
    }

    if (bossWave) {
        SpawnEntry bossA{};
        bossA.spawnTime = 0.56f * count + 1.2f;
        bossA.laneIndex = laneCount > 1 ? 1 : 0;
        bossA.type = EnemyType::ProcessionBreaker;
        wave.spawns.push_back(bossA);

        if (waveNumber >= 10 && laneCount >= 3) {
            SpawnEntry bossB{};
            bossB.spawnTime = bossA.spawnTime + 2.0f;
            bossB.laneIndex = 2;
            bossB.type = EnemyType::ProcessionBreaker;
            wave.spawns.push_back(bossB);
        }
    }
}

void Game::StartWave() {
    if (state != PlayState::BuildPhase) return;

    wave.active = true;
    wave.timer = 0.0f;
    wave.nextSpawnIndex = 0;
    state = PlayState::BattlePhase;
    announcement = (wave.number % 5 == 0)
        ? TextFormat("WAVE %d // BREAKER MARCH", wave.number)
        : TextFormat("WAVE %d // THREE ROADS BURN", wave.number);
    announcementTimer = 2.6f;
}

void Game::SpawnEnemy(EnemyType type, int laneIndex) {
    if (laneIndex < 0 || laneIndex >= (int)lanes.size() || lanes[laneIndex].empty()) laneIndex = 0;

    Enemy enemy{};
    enemy.type = type;
    enemy.laneIndex = laneIndex;
    enemy.pathIndex = 0;

    if (type == EnemyType::ProcessionBreaker) {
        enemy.hp = 340 + (wave.number - 1) * 55;
        enemy.maxHp = enemy.hp;
        enemy.speed = 0.92f + (wave.number - 1) * 0.05f;
        enemy.attackCooldown = 1.18f;
        enemy.gateDamage = 24;
        enemy.coreDamage = 20;
        enemy.pos.y = 1.10f;
    }
    else if (type == EnemyType::BannerKnight) {
        enemy.hp = 58 + (wave.number - 1) * 12;
        enemy.maxHp = enemy.hp;
        enemy.speed = 1.55f + (wave.number - 1) * 0.10f;
        enemy.attackCooldown = 0.86f;
        enemy.gateDamage = 9;
        enemy.coreDamage = 11;
        enemy.pos.y = 0.76f;
    }
    else if (type == EnemyType::GraveBrute) {
        enemy.hp = 78 + (wave.number - 1) * 16;
        enemy.maxHp = enemy.hp;
        enemy.speed = 1.12f + (wave.number - 1) * 0.08f;
        enemy.attackCooldown = 1.05f;
        enemy.gateDamage = 13;
        enemy.coreDamage = 14;
        enemy.pos.y = 0.88f;
    }
    else {
        enemy.hp = 30 + (wave.number - 1) * 8;
        enemy.maxHp = enemy.hp;
        enemy.speed = 1.82f + (wave.number - 1) * 0.14f;
        enemy.attackCooldown = 0.80f;
        enemy.gateDamage = 6;
        enemy.coreDamage = 8;
        enemy.pos.y = 0.64f;
    }

    Vector3 start = grid.CellCenter(lanes[laneIndex][0].x, lanes[laneIndex][0].y);
    enemy.pos.x = start.x - 1.2f;
    enemy.pos.z = start.z;
    enemies.push_back(enemy);
}

void Game::Update(float dt) {
    worldTime += dt;
    if (announcementTimer > 0.0f) {
        announcementTimer -= dt;
        if (announcementTimer < 0.0f) announcementTimer = 0.0f;
    }
    if (hymnTimer > 0.0f) {
        hymnTimer -= dt;
        if (hymnTimer < 0.0f) hymnTimer = 0.0f;
    }

    if (state == PlayState::GameOver) {
        UpdateDeathFx(dt);
        if (IsKeyPressed(KEY_ENTER)) ResetRun();
        return;
    }

    UpdateCamera(dt);
    UpdateHoverCell();
    UpdateShots(dt);
    UpdateDeathFx(dt);

    if (state == PlayState::BuildPhase) UpdateBuildPhase();
    else if (state == PlayState::BattlePhase) UpdateBattlePhase(dt);
}

void Game::UpdateCamera(float dt) {
    float move = 26.0f * dt;
    if (IsKeyDown(KEY_A)) cameraFocus.x -= move;
    if (IsKeyDown(KEY_D)) cameraFocus.x += move;
    if (IsKeyDown(KEY_W)) cameraFocus.z -= move;
    if (IsKeyDown(KEY_S)) cameraFocus.z += move;

    cameraZoom -= GetMouseWheelMove() * 2.2f;
    cameraZoom = ClampFloat(cameraZoom, cameraMinZoom, cameraMaxZoom);

    float minX = grid.origin.x + 22.0f;
    float maxX = grid.origin.x + grid.width * grid.cellSize - 22.0f;
    float minZ = grid.origin.z + 20.0f;
    float maxZ = grid.origin.z + grid.height * grid.cellSize - 20.0f;
    cameraFocus.x = ClampFloat(cameraFocus.x, minX, maxX);
    cameraFocus.z = ClampFloat(cameraFocus.z, minZ, maxZ);

    camera.target = { cameraFocus.x, 0.45f, cameraFocus.z };
    camera.position = { cameraFocus.x + cameraZoom, cameraZoom * 0.86f, cameraFocus.z + cameraZoom };
    camera.fovy = 36.0f;
}

void Game::UpdateHoverCell() {
    hoveredValid = false;
    hoveredCell = { -1, -1 };
    hoveredTowerIndex = -1;

    Vector3 worldPoint{};
    if (!RayToGround(&worldPoint)) return;

    GridCoord cell = grid.WorldToCell(worldPoint);
    if (!grid.InBounds(cell.x, cell.y)) return;

    hoveredValid = true;
    hoveredCell = cell;
    hoveredTowerIndex = FindTowerIndexAtCell(cell.x, cell.y);
}

void Game::UpdateBuildPhase() {
    if (IsKeyPressed(KEY_ONE)) { buildChoice = BuildChoice::WatchbowNest; announcement = "WATCHBOW NEST SELECTED"; announcementTimer = 1.0f; }
    if (IsKeyPressed(KEY_TWO)) { buildChoice = BuildChoice::CenserShrine; announcement = "CENSER SHRINE SELECTED"; announcementTimer = 1.0f; }
    if (IsKeyPressed(KEY_THREE)) { buildChoice = BuildChoice::ReliquarySpire; announcement = "RELIQUARY SPIRE SELECTED"; announcementTimer = 1.0f; }
    if (IsKeyPressed(KEY_FOUR)) { buildChoice = BuildChoice::PilgrimBarricade; announcement = "PILGRIM BARRICADE SELECTED"; announcementTimer = 1.0f; }
    if (IsKeyPressed(KEY_U)) TryUpgradeTower();
    if (IsKeyPressed(KEY_X)) TrySellTower();

    if (IsKeyPressed(KEY_H)) {
        if (fortress.gateHp >= fortress.gateMaxHp) {
            announcement = "GATE ALREADY WHOLE";
            announcementTimer = 1.0f;
        }
        else if (iron >= 15) {
            iron -= 15;
            fortress.gateHp += 22;
            if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
            announcement = "GATE REPAIRED";
            announcementTimer = 1.2f;
        }
        else {
            announcement = "NOT ENOUGH IRON";
            announcementTimer = 1.0f;
        }
    }

    if (IsKeyPressed(KEY_J)) {
        if (fortress.coreHp >= fortress.coreMaxHp) {
            announcement = "CORE ALREADY CONSECRATED";
            announcementTimer = 1.0f;
        }
        else if (gold >= 30 && ember >= 10) {
            gold -= 30;
            ember -= 10;
            fortress.coreHp += 18;
            if (fortress.coreHp > fortress.coreMaxHp) fortress.coreHp = fortress.coreMaxHp;
            announcement = "HOLY CORE CONSECRATED";
            announcementTimer = 1.2f;
        }
        else {
            announcement = "NEED 30 GOLD AND 10 EMBER";
            announcementTimer = 1.0f;
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) TryPlaceTower();
    if (IsKeyPressed(KEY_SPACE)) StartWave();
}

void Game::UpdateBattlePhase(float dt) {
    if (IsKeyPressed(KEY_F)) TriggerWarHymn();

    if (wave.active) {
        wave.timer += dt;
        while (wave.nextSpawnIndex < (int)wave.spawns.size() && wave.timer >= wave.spawns[wave.nextSpawnIndex].spawnTime) {
            const SpawnEntry& entry = wave.spawns[wave.nextSpawnIndex];
            SpawnEnemy(entry.type, entry.laneIndex);
            wave.nextSpawnIndex++;
        }
    }

    UpdateEnemies(dt);
    UpdateTowers(dt);

    if (wave.nextSpawnIndex >= (int)wave.spawns.size() && enemies.empty()) {
        wave.active = false;
        state = PlayState::BuildPhase;
        gold += 32 + wave.number * 9;
        iron += 14 + wave.number * 4 + (fortress.gateHp > 0 ? 6 : 0);
        ember += 4 + wave.number;
        if (fortress.gateHp > 0) {
            fortress.gateHp += 6;
            if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
        }
        fervor += 18;
        if (fervor > fervorMax) fervor = fervorMax;
        wave.number++;
        BuildWave(wave.number);
        announcement = "SIEGE BROKEN // REBUILD, UPGRADE, CONSECRATE";
        announcementTimer = 3.2f;
    }
}

void Game::UpdateEnemies(float dt) {
    Vector3 corePos = grid.CellCenter(fortress.coreCell.x, fortress.coreCell.y);
    corePos.y = 0.8f;

    for (Enemy& enemy : enemies) {
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        enemy.slowTimer = std::max(0.0f, enemy.slowTimer - dt);
        enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);

        float speedMul = 1.0f;
        if (enemy.slowTimer > 0.0f) speedMul *= 0.58f;
        if (hymnTimer > 0.0f) speedMul *= 0.84f;

        if (enemy.type != EnemyType::BannerKnight) {
            for (const Enemy& other : enemies) {
                if (&other == &enemy || other.hp <= 0 || other.type != EnemyType::BannerKnight) continue;
                if (DistanceXZ(other.pos, enemy.pos) <= 3.0f) {
                    speedMul *= 1.12f;
                    break;
                }
            }
        }

        if (!enemy.pastGate) {
            const std::vector<GridCoord>& lane = lanes[enemy.laneIndex];
            if (enemy.pathIndex < (int)lane.size()) {
                Vector3 target = grid.CellCenter(lane[enemy.pathIndex].x, lane[enemy.pathIndex].y);
                target.y = enemy.pos.y;
                Vector3 toTarget = Vec3Sub(target, enemy.pos);
                float dist = LengthXZ(toTarget);
                if (dist < 0.12f) enemy.pathIndex++;
                else enemy.pos = Vec3Add(enemy.pos, Vec3Scale(NormalizeXZ(toTarget), enemy.speed * speedMul * dt));
                continue;
            }

            if (fortress.gateHp > 0) {
                if (enemy.attackTimer <= 0.0f) {
                    DamageGate(enemy.gateDamage);
                    enemy.attackTimer = enemy.attackCooldown;
                }
                continue;
            }

            enemy.pastGate = true;
        }

        Vector3 toCore = Vec3Sub(corePos, enemy.pos);
        float coreDist = LengthXZ(toCore);
        if (coreDist < 1.0f) {
            if (enemy.attackTimer <= 0.0f) {
                DamageCore(enemy.coreDamage);
                enemy.attackTimer = enemy.attackCooldown;
            }
        }
        else {
            enemy.pos = Vec3Add(enemy.pos, Vec3Scale(NormalizeXZ(toCore), enemy.speed * speedMul * dt));
        }
    }

    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& enemy) { return enemy.hp <= 0; }), enemies.end());
}

void Game::UpdateTowers(float dt) {
    float towerSpeedMul = hymnTimer > 0.0f ? 1.75f : 1.0f;

    for (Tower& tower : towers) {
        tower.cooldown -= dt * towerSpeedMul;
        if (tower.cooldown > 0.0f) continue;

        if (tower.type == TowerType::CenserShrine) {
            bool hitAny = false;
            for (Enemy& enemy : enemies) {
                if (enemy.hp <= 0) continue;
                if (DistanceXZ(tower.pos, enemy.pos) <= tower.range) {
                    int before = enemy.hp;
                    enemy.hp -= tower.damage;
                    enemy.hitFlash = 0.08f;
                    if (before > 0 && enemy.hp <= 0) RegisterEnemyKill(enemy);
                    hitAny = true;
                }
            }
            if (hitAny) {
                tower.cooldown = tower.maxCooldown;
                ShotFx fx{};
                fx.start = { tower.pos.x, tower.pos.y + 0.5f, tower.pos.z };
                fx.end = { tower.pos.x, tower.pos.y + 2.6f, tower.pos.z };
                fx.life = 0.14f;
                fx.color = { 244, 166, 84, 255 };
                shots.push_back(fx);
            }
            continue;
        }

        if (tower.type == TowerType::PilgrimBarricade) {
            int bestIndex = -1;
            float bestDist = tower.range;
            for (int i = 0; i < (int)enemies.size(); ++i) {
                if (enemies[i].hp <= 0) continue;
                float dist = DistanceXZ(tower.pos, enemies[i].pos);
                if (dist <= bestDist) {
                    bestDist = dist;
                    bestIndex = i;
                }
            }

            if (bestIndex >= 0) {
                Enemy& target = enemies[bestIndex];
                int before = target.hp;
                target.hp -= tower.damage;
                target.hitFlash = 0.12f;
                target.slowTimer = std::max(target.slowTimer, 0.7f + 0.25f * (float)tower.level);
                tower.cooldown = tower.maxCooldown;

                ShotFx fx{};
                fx.start = { tower.pos.x, 0.25f, tower.pos.z };
                fx.end = { target.pos.x, target.pos.y * 0.55f, target.pos.z };
                fx.life = 0.12f;
                fx.color = { 162, 102, 76, 255 };
                shots.push_back(fx);

                if (before > 0 && target.hp <= 0) RegisterEnemyKill(target);
            }
            continue;
        }

        int bestIndex = -1;
        float bestScore = -10000.0f;
        for (int i = 0; i < (int)enemies.size(); ++i) {
            if (enemies[i].hp <= 0) continue;
            float dist = DistanceXZ(tower.pos, enemies[i].pos);
            if (dist > tower.range) continue;

            float score = 0.0f;
            score += enemies[i].pastGate ? 1200.0f : 0.0f;
            score += (float)enemies[i].pathIndex * 16.0f;
            score += (enemies[i].type == EnemyType::ProcessionBreaker) ? 40.0f : 0.0f;
            score += (enemies[i].type == EnemyType::BannerKnight) ? 16.0f : 0.0f;
            score += (enemies[i].type == EnemyType::GraveBrute) ? 10.0f : 0.0f;
            score -= dist;
            if (score > bestScore) { bestScore = score; bestIndex = i; }
        }

        if (bestIndex >= 0) {
            Enemy& target = enemies[bestIndex];
            int before = target.hp;
            target.hp -= tower.damage;
            target.hitFlash = 0.12f;
            if (tower.type == TowerType::ReliquarySpire) target.slowTimer = std::max(target.slowTimer, 1.5f + 0.25f * (float)tower.level);
            tower.cooldown = tower.maxCooldown;

            ShotFx fx{};
            fx.start = { tower.pos.x, tower.pos.y + 1.8f, tower.pos.z };
            fx.end = { target.pos.x, target.pos.y + 0.4f, target.pos.z };
            fx.life = (tower.type == TowerType::ReliquarySpire) ? 0.15f : 0.10f;
            fx.color = tower.color;
            shots.push_back(fx);

            if (before > 0 && target.hp <= 0) RegisterEnemyKill(target);
        }
    }
}

void Game::UpdateShots(float dt) {
    for (ShotFx& shot : shots) shot.life -= dt;
    shots.erase(std::remove_if(shots.begin(), shots.end(), [](const ShotFx& shot) { return shot.life <= 0.0f; }), shots.end());
}

void Game::UpdateDeathFx(float dt) {
    for (DeathFx& fx : deathFx) {
        fx.life -= dt;
        fx.pos = Vec3Add(fx.pos, Vec3Scale(fx.vel, dt));
        fx.vel.y -= 1.8f * dt;
    }
    deathFx.erase(std::remove_if(deathFx.begin(), deathFx.end(), [](const DeathFx& fx) { return fx.life <= 0.0f; }), deathFx.end());
}

void Game::TryPlaceTower() {
    if (!hoveredValid || !grid.InBounds(hoveredCell.x, hoveredCell.y)) return;

    GridTile& tile = grid.At(hoveredCell.x, hoveredCell.y);
    if (tile.kind != TileKind::Buildable || tile.occupied || hoveredTowerIndex >= 0) {
        announcement = "CANNOT BUILD THERE";
        announcementTimer = 1.1f;
        return;
    }

    Tower tower = MakeTower(buildChoice, hoveredCell.x, hoveredCell.y);
    if (gold < tower.goldCost) { announcement = "NOT ENOUGH GOLD"; announcementTimer = 1.1f; return; }
    if (iron < tower.ironCost) { announcement = "NOT ENOUGH IRON"; announcementTimer = 1.1f; return; }
    if (ember < tower.emberCost) { announcement = "NOT ENOUGH EMBER"; announcementTimer = 1.1f; return; }

    towers.push_back(tower);
    tile.occupied = true;
    gold -= tower.goldCost;
    iron -= tower.ironCost;
    ember -= tower.emberCost;
    announcement = std::string(BuildChoiceLabel(buildChoice)) + " RAISED";
    announcementTimer = 1.3f;
}

void Game::TryUpgradeTower() {
    if (hoveredTowerIndex < 0 || hoveredTowerIndex >= (int)towers.size()) {
        announcement = "HOVER A TOWER TO UPGRADE";
        announcementTimer = 1.0f;
        return;
    }

    Tower& tower = towers[hoveredTowerIndex];
    if (tower.level >= 3) {
        announcement = "TOWER AT MAX CONSECRATION";
        announcementTimer = 1.0f;
        return;
    }

    int goldCost = GetTowerUpgradeGoldCost(tower);
    int ironCost = GetTowerUpgradeIronCost(tower);
    int emberCost = GetTowerUpgradeEmberCost(tower);
    if (gold < goldCost) { announcement = "NOT ENOUGH GOLD"; announcementTimer = 1.0f; return; }
    if (iron < ironCost) { announcement = "NOT ENOUGH IRON"; announcementTimer = 1.0f; return; }
    if (ember < emberCost) { announcement = "NOT ENOUGH EMBER"; announcementTimer = 1.0f; return; }

    gold -= goldCost;
    iron -= ironCost;
    ember -= emberCost;
    tower.level++;
    ApplyTowerStats(tower);
    announcement = TextFormat("%s UPGRADED TO LVL %d", TowerLabel(tower.type), tower.level);
    announcementTimer = 1.4f;
}

void Game::TrySellTower() {
    if (hoveredTowerIndex < 0 || hoveredTowerIndex >= (int)towers.size()) {
        announcement = "HOVER A TOWER TO SELL";
        announcementTimer = 1.0f;
        return;
    }

    Tower tower = towers[hoveredTowerIndex];
    gold += GetTowerSellGoldRefund(tower);
    iron += GetTowerSellIronRefund(tower);
    ember += GetTowerSellEmberRefund(tower);
    if (grid.InBounds(tower.gridX, tower.gridY)) grid.At(tower.gridX, tower.gridY).occupied = false;
    towers.erase(towers.begin() + hoveredTowerIndex);
    hoveredTowerIndex = -1;
    announcement = "DEFENSE DISMANTLED";
    announcementTimer = 1.2f;
}

void Game::DamageGate(int amount) {
    fortress.gateHp -= amount;
    if (fortress.gateHp < 0) fortress.gateHp = 0;
    announcement = TextFormat("FRONT GATE STRUCK // %d HP", fortress.gateHp);
    announcementTimer = 0.75f;
}

void Game::DamageCore(int amount) {
    fortress.coreHp -= amount;
    if (fortress.coreHp < 0) fortress.coreHp = 0;
    announcement = TextFormat("HOLY CORE STRUCK // %d HP", fortress.coreHp);
    announcementTimer = 1.0f;
    if (fortress.coreHp <= 0) {
        state = PlayState::GameOver;
        announcement = "THE PROCESSION HAS FALLEN";
        announcementTimer = 4.0f;
    }
}

void Game::GainFervor(int amount) {
    fervor += amount;
    if (fervor > fervorMax) fervor = fervorMax;
}

void Game::TriggerWarHymn() {
    if (fervor < fervorMax) {
        announcement = "FERVOR NOT FULL";
        announcementTimer = 0.8f;
        return;
    }

    fervor = 0;
    hymnTimer = 6.0f;
    announcement = "WAR HYMN AWAKENED";
    announcementTimer = 1.6f;
}

void Game::RegisterEnemyKill(const Enemy& enemy) {
    int goldReward = 4;
    int emberReward = 0;
    int fervorReward = 8;

    if (enemy.type == EnemyType::ProcessionBreaker) {
        goldReward = 36;
        emberReward = 6;
        fervorReward = 36;
        if (fortress.gateHp > 0) {
            fortress.gateHp += 10;
            if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
        }
    }
    else if (enemy.type == EnemyType::BannerKnight) {
        goldReward = 12;
        emberReward = 2;
        fervorReward = 18;
    }
    else if (enemy.type == EnemyType::GraveBrute) {
        goldReward = 8;
        emberReward = 1;
        fervorReward = 14;
    }

    gold += goldReward;
    ember += emberReward;
    GainFervor(fervorReward);

    Color burstColor = { 186, 96, 84, 255 };
    if (enemy.type == EnemyType::GraveBrute) burstColor = { 120, 92, 150, 255 };
    if (enemy.type == EnemyType::BannerKnight) burstColor = { 188, 154, 92, 255 };
    if (enemy.type == EnemyType::ProcessionBreaker) burstColor = { 222, 94, 94, 255 };

    int pieces = 5;
    if (enemy.type == EnemyType::GraveBrute) pieces = 7;
    if (enemy.type == EnemyType::ProcessionBreaker) pieces = 12;

    for (int i = 0; i < pieces; ++i) {
        float angle = ((float)i / (float)pieces) * 6.2831853f;
        float speed = 1.2f + 0.22f * (float)i;
        DeathFx fx{};
        fx.pos = { enemy.pos.x, enemy.pos.y + 0.25f, enemy.pos.z };
        fx.vel = { std::cos(angle) * speed, 1.2f + 0.08f * (float)i, std::sin(angle) * speed };
        fx.life = 0.55f + 0.04f * (float)i;
        fx.maxLife = fx.life;
        fx.size = (enemy.type == EnemyType::ProcessionBreaker) ? 0.22f : 0.14f;
        fx.color = burstColor;
        deathFx.push_back(fx);
    }

    if (enemy.type == EnemyType::ProcessionBreaker) {
        announcement = "THE BREAKER HAS FALLEN";
        announcementTimer = 1.8f;
    }
}

bool Game::RayToGround(Vector3* outPoint) const {
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    if (std::fabs(ray.direction.y) < 0.0001f) return false;

    float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f) return false;

    outPoint->x = ray.position.x + ray.direction.x * t;
    outPoint->y = 0.0f;
    outPoint->z = ray.position.z + ray.direction.z * t;
    return true;
}

int Game::FindTowerIndexAtCell(int x, int y) const {
    for (int i = 0; i < (int)towers.size(); ++i) {
        if (towers[i].gridX == x && towers[i].gridY == y) return i;
    }
    return -1;
}

Tower Game::MakeTower(BuildChoice choice, int cellX, int cellY) const {
    Tower tower{};
    tower.gridX = cellX;
    tower.gridY = cellY;
    tower.level = 1;
    tower.pos = grid.CellCenter(cellX, cellY);
    tower.pos.y = 1.0f;

    if (choice == BuildChoice::CenserShrine) tower.type = TowerType::CenserShrine;
    else if (choice == BuildChoice::ReliquarySpire) tower.type = TowerType::ReliquarySpire;
    else if (choice == BuildChoice::PilgrimBarricade) tower.type = TowerType::PilgrimBarricade;
    else tower.type = TowerType::WatchbowNest;

    ApplyTowerStats(tower);
    return tower;
}

void Game::ApplyTowerStats(Tower& tower) const {
    if (tower.type == TowerType::CenserShrine) {
        tower.range = 3.4f + 0.45f * (float)(tower.level - 1);
        tower.maxCooldown = 0.60f - 0.05f * (float)(tower.level - 1);
        if (tower.maxCooldown < 0.42f) tower.maxCooldown = 0.42f;
        tower.damage = 4 + 2 * (tower.level - 1);
        tower.goldCost = 20;
        tower.ironCost = 0;
        tower.emberCost = 8;
        tower.color = { 244, 166, 84, 255 };
    }
    else if (tower.type == TowerType::ReliquarySpire) {
        tower.range = 5.8f + 0.55f * (float)(tower.level - 1);
        tower.maxCooldown = 1.08f - 0.10f * (float)(tower.level - 1);
        if (tower.maxCooldown < 0.82f) tower.maxCooldown = 0.82f;
        tower.damage = 7 + 3 * (tower.level - 1);
        tower.goldCost = 32;
        tower.ironCost = 0;
        tower.emberCost = 10;
        tower.color = { 122, 170, 236, 255 };
    }
    else if (tower.type == TowerType::PilgrimBarricade) {
        tower.range = 1.75f + 0.25f * (float)(tower.level - 1);
        tower.maxCooldown = 0.60f - 0.06f * (float)(tower.level - 1);
        if (tower.maxCooldown < 0.42f) tower.maxCooldown = 0.42f;
        tower.damage = 15 + 7 * (tower.level - 1);
        tower.goldCost = 18;
        tower.ironCost = 6;
        tower.emberCost = 0;
        tower.color = { 162, 102, 76, 255 };
    }
    else {
        tower.range = 6.0f + 0.60f * (float)(tower.level - 1);
        tower.maxCooldown = 0.72f - 0.08f * (float)(tower.level - 1);
        if (tower.maxCooldown < 0.50f) tower.maxCooldown = 0.50f;
        tower.damage = 10 + 5 * (tower.level - 1);
        tower.goldCost = 25;
        tower.ironCost = 0;
        tower.emberCost = 0;
        tower.color = { 194, 172, 118, 255 };
    }

    if (tower.cooldown > tower.maxCooldown) tower.cooldown = tower.maxCooldown;
}

const char* Game::BuildChoiceLabel(BuildChoice choice) const {
    switch (choice) {
    case BuildChoice::WatchbowNest: return "WATCHBOW NEST";
    case BuildChoice::CenserShrine: return "CENSER SHRINE";
    case BuildChoice::ReliquarySpire: return "RELIQUARY SPIRE";
    case BuildChoice::PilgrimBarricade: return "PILGRIM BARRICADE";
    }
    return "WATCHBOW NEST";
}

const char* Game::TowerLabel(TowerType type) const {
    switch (type) {
    case TowerType::WatchbowNest: return "WATCHBOW NEST";
    case TowerType::CenserShrine: return "CENSER SHRINE";
    case TowerType::ReliquarySpire: return "RELIQUARY SPIRE";
    case TowerType::PilgrimBarricade: return "PILGRIM BARRICADE";
    }
    return "WATCHBOW NEST";
}

const char* Game::EnemyLabel(EnemyType type) const {
    switch (type) {
    case EnemyType::AshRaider: return "ASH RAIDER";
    case EnemyType::GraveBrute: return "GRAVE BRUTE";
    case EnemyType::BannerKnight: return "BANNER KNIGHT";
    case EnemyType::ProcessionBreaker: return "PROCESSION BREAKER";
    }
    return "ASH RAIDER";
}

int Game::GetTowerUpgradeGoldCost(const Tower& tower) const {
    int base = 18 + tower.level * 12;
    if (tower.type == TowerType::CenserShrine) base += 4;
    if (tower.type == TowerType::ReliquarySpire) base += 10;
    if (tower.type == TowerType::PilgrimBarricade) base -= 4;
    return base;
}

int Game::GetTowerUpgradeIronCost(const Tower& tower) const {
    if (tower.type == TowerType::PilgrimBarricade) return 4 + tower.level * 3;
    return 0;
}

int Game::GetTowerUpgradeEmberCost(const Tower& tower) const {
    if (tower.type == TowerType::WatchbowNest) return 0;
    if (tower.type == TowerType::PilgrimBarricade) return 0;
    if (tower.type == TowerType::CenserShrine) return 4 + tower.level * 2;
    return 6 + tower.level * 2;
}

int Game::GetTowerSellGoldRefund(const Tower& tower) const {
    return tower.goldCost / 2 + tower.level * 7;
}

int Game::GetTowerSellIronRefund(const Tower& tower) const {
    return tower.ironCost / 2 + ((tower.type == TowerType::PilgrimBarricade) ? (tower.level - 1) * 2 : 0);
}

int Game::GetTowerSellEmberRefund(const Tower& tower) const {
    return tower.emberCost / 2 + ((tower.emberCost > 0) ? (tower.level - 1) * 2 : 0);
}

void Game::Draw() const {
    BeginDrawing();
    ClearBackground({ 11, 14, 22, 255 });
    DrawWorld();
    DrawUi();
    EndDrawing();
}

void Game::DrawWorld() const {
    BeginMode3D(camera);

    DrawPlane({ 0.0f, -0.08f, 0.0f }, { 240.0f, 240.0f }, { 24, 28, 34, 255 });
    DrawPlane({ 0.0f, -0.04f, 0.0f }, { 190.0f, 190.0f }, { 34, 40, 32, 255 });

    DrawTiles();
    DrawEnvironment();
    DrawFortress();
    DrawTowers();
    DrawEnemies();
    DrawEffects();

    EndMode3D();
}

void Game::DrawTiles() const {
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            const GridTile& tile = grid.At(x, y);
            Vector3 center = grid.CellCenter(x, y);
            center.y = tile.height * 0.5f - 0.05f;

            Color tileColor = { 84, 96, 78, 255 };
            Color topColor = { 94, 110, 90, 255 };
            if (tile.kind == TileKind::Road) {
                tileColor = { 86, 72, 54, 255 };
                topColor = { 132, 116, 86, 255 };
            }
            else if (tile.kind == TileKind::Spawn) {
                tileColor = { 82, 44, 44, 255 };
                topColor = { 150, 82, 68, 255 };
            }
            else if (tile.kind == TileKind::Fortress) {
                tileColor = { 88, 94, 108, 255 };
                topColor = { 126, 132, 144, 255 };
            }
            else if (tile.kind == TileKind::Blocked) {
                tileColor = { 54, 58, 60, 255 };
                topColor = { 86, 90, 94, 255 };
            }

            if (hoveredValid && hoveredCell.x == x && hoveredCell.y == y) {
                if (hoveredTowerIndex >= 0) {
                    topColor = { 122, 156, 220, 255 };
                }
                else if (state == PlayState::BuildPhase && tile.kind == TileKind::Buildable && !tile.occupied) {
                    if (buildChoice == BuildChoice::WatchbowNest) topColor = { 126, 174, 120, 255 };
                    else if (buildChoice == BuildChoice::CenserShrine) topColor = { 196, 142, 82, 255 };
                    else if (buildChoice == BuildChoice::ReliquarySpire) topColor = { 104, 144, 210, 255 };
                    else topColor = { 176, 116, 84, 255 };
                }
                else {
                    topColor = { 174, 84, 84, 255 };
                }
            }

            DrawCube(center, grid.cellSize * 0.98f, tile.height, grid.cellSize * 0.98f, tileColor);
            DrawCube({ center.x, center.y + tile.height * 0.5f - 0.02f, center.z }, grid.cellSize * 0.88f, 0.06f, grid.cellSize * 0.88f, topColor);
            DrawCubeWires(center, grid.cellSize * 0.98f, tile.height, grid.cellSize * 0.98f, { 28, 32, 30, 255 });

            if (tile.kind == TileKind::Road || tile.kind == TileKind::Spawn) {
                DrawCube({ center.x, center.y + tile.height * 0.5f + 0.02f, center.z }, grid.cellSize * 0.46f, 0.04f, grid.cellSize * 0.78f, { 154, 138, 104, 255 });
                DrawCube({ center.x - grid.cellSize * 0.34f, center.y + tile.height * 0.5f, center.z }, 0.10f, 0.10f, grid.cellSize * 0.72f, { 70, 60, 46, 255 });
                DrawCube({ center.x + grid.cellSize * 0.34f, center.y + tile.height * 0.5f, center.z }, 0.10f, 0.10f, grid.cellSize * 0.72f, { 70, 60, 46, 255 });
            }
        }
    }
}

void Game::DrawEnvironment() const {
    float cs = grid.cellSize;

    for (int x = -1; x <= grid.width; ++x) {
        float wx = grid.origin.x + x * cs + cs * 0.5f;
        DrawCube({ wx, 2.5f, grid.origin.z - cs * 0.55f }, cs * 1.05f, 5.0f, cs * 1.2f, { 30, 34, 40, 255 });
        DrawCube({ wx, 2.5f, grid.origin.z + grid.height * cs + cs * 0.55f }, cs * 1.05f, 5.0f, cs * 1.2f, { 30, 34, 40, 255 });
    }
    for (int y = 0; y < grid.height; ++y) {
        float wz = grid.origin.z + y * cs + cs * 0.5f;
        DrawCube({ grid.origin.x - cs * 0.55f, 2.5f, wz }, cs * 1.2f, 5.0f, cs * 1.05f, { 30, 34, 40, 255 });
        DrawCube({ grid.origin.x + grid.width * cs + cs * 0.55f, 2.5f, wz }, cs * 1.2f, 5.0f, cs * 1.05f, { 30, 34, 40, 255 });
    }

    for (int i = 0; i < 22; ++i) {
        float orbit = worldTime * (0.26f + 0.02f * (float)i) + (float)i;
        Vector3 mote = {
            grid.origin.x + 4.0f + std::fmod(orbit * 5.6f + (float)(i * 7), grid.width * cs - 8.0f),
            1.6f + 0.6f * std::sin(orbit * 1.8f),
            grid.origin.z + 3.0f + std::fmod(orbit * 3.8f + (float)(i * 5), grid.height * cs - 6.0f)
        };
        DrawSphere(mote, 0.08f, { 74, 82, 94, 160 });
    }

    for (const Prop& prop : props) {
        Vector3 center = grid.CellCenter(prop.cell.x, prop.cell.y);
        float ground = grid.At(prop.cell.x, prop.cell.y).height;
        center.y = ground;

        if (prop.type == PropType::DeadTree) {
            DrawCube({ center.x, center.y + 1.10f, center.z }, 0.42f, 2.0f, 0.42f, { 82, 62, 50, 255 });
            DrawCube({ center.x + 0.36f, center.y + 1.70f, center.z + 0.10f }, 0.92f, 0.16f, 0.16f, { 92, 70, 54, 255 });
            DrawCube({ center.x - 0.30f, center.y + 1.30f, center.z - 0.24f }, 0.76f, 0.16f, 0.16f, { 92, 70, 54, 255 });
        }
        else if (prop.type == PropType::GraveMarker) {
            DrawCube({ center.x, center.y + 0.36f, center.z }, 0.88f, 0.72f, 0.24f, { 136, 142, 150, 255 });
            DrawCube({ center.x, center.y + 0.76f, center.z }, 0.52f, 0.18f, 0.24f, { 156, 160, 168, 255 });
        }
        else if (prop.type == PropType::RubblePile) {
            DrawCube({ center.x - 0.22f, center.y + 0.16f, center.z - 0.14f }, 0.70f, 0.28f, 0.60f, { 104, 96, 88, 255 });
            DrawCube({ center.x + 0.20f, center.y + 0.24f, center.z + 0.18f }, 0.60f, 0.38f, 0.54f, { 126, 118, 106, 255 });
            DrawCube({ center.x + 0.06f, center.y + 0.14f, center.z + 0.34f }, 0.50f, 0.22f, 0.38f, { 88, 82, 76, 255 });
        }
        else if (prop.type == PropType::CartWreck) {
            DrawCube({ center.x, center.y + 0.26f, center.z }, 1.28f, 0.32f, 0.86f, { 110, 84, 58, 255 });
            DrawCube({ center.x - 0.38f, center.y + 0.62f, center.z }, 0.14f, 0.56f, 0.14f, { 94, 72, 50, 255 });
            DrawCube({ center.x + 0.38f, center.y + 0.62f, center.z }, 0.14f, 0.56f, 0.14f, { 94, 72, 50, 255 });
            DrawCylinder({ center.x - 0.56f, center.y + 0.16f, center.z + 0.30f }, 0.22f, 0.22f, 0.10f, 8, { 86, 66, 50, 255 });
            DrawCylinder({ center.x + 0.56f, center.y + 0.16f, center.z - 0.30f }, 0.22f, 0.22f, 0.10f, 8, { 86, 66, 50, 255 });
        }
        else if (prop.type == PropType::Brazier) {
            float flicker = 0.14f + 0.06f * std::sin(worldTime * 6.0f + (float)(prop.cell.x + prop.cell.y));
            DrawCylinder({ center.x, center.y + 0.32f, center.z }, 0.26f, 0.34f, 0.46f, 8, { 104, 92, 76, 255 });
            DrawSphere({ center.x, center.y + 0.84f + flicker, center.z }, 0.22f + flicker * 0.36f, { 248, 170, 92, 255 });
            DrawSphere({ center.x, center.y + 1.02f + flicker, center.z }, 0.12f + flicker * 0.26f, { 255, 226, 164, 255 });
        }
        else if (prop.type == PropType::BannerPole) {
            DrawCylinder({ center.x, center.y + 1.75f, center.z }, 0.10f, 0.10f, 3.2f, 6, { 126, 126, 138, 255 });
            float sway = 0.18f * std::sin(worldTime * 2.5f + (float)prop.cell.x);
            DrawCube({ center.x + 0.52f, center.y + 2.58f, center.z + sway }, 0.92f, 1.10f, 0.10f, { 124, 42, 42, 255 });
        }
    }
}

void Game::DrawFortress() const {
    Vector3 gate = grid.CellCenter(fortress.gateCell.x, fortress.gateCell.y);
    Vector3 core = grid.CellCenter(fortress.coreCell.x, fortress.coreCell.y);
    Vector3 mid = { (gate.x + core.x) * 0.5f + 1.6f, 0.0f, core.z };

    DrawCube({ mid.x, 0.75f, mid.z }, 24.0f, 1.5f, 16.0f, { 70, 76, 84, 255 });
    DrawCube({ mid.x + 1.8f, 2.9f, mid.z }, 16.0f, 5.8f, 12.6f, { 144, 148, 156, 255 });
    DrawCubeWires({ mid.x + 1.8f, 2.9f, mid.z }, 16.0f, 5.8f, 12.6f, { 60, 64, 72, 255 });

    DrawCube({ mid.x - 6.2f, 3.0f, mid.z }, 0.9f, 2.4f, 11.0f, { 118, 124, 134, 255 });
    DrawCube({ mid.x + 5.8f, 3.0f, mid.z }, 0.9f, 2.4f, 11.0f, { 118, 124, 134, 255 });
    DrawCube({ mid.x, 3.0f, mid.z - 5.8f }, 15.2f, 2.4f, 0.9f, { 118, 124, 134, 255 });
    DrawCube({ mid.x, 3.0f, mid.z + 5.8f }, 15.2f, 2.4f, 0.9f, { 118, 124, 134, 255 });

    DrawCube({ core.x - 0.8f, 4.6f, core.z - 4.5f }, 1.7f, 8.6f, 1.7f, { 176, 178, 184, 255 });
    DrawCube({ core.x + 4.8f, 4.6f, core.z - 4.5f }, 1.7f, 8.6f, 1.7f, { 176, 178, 184, 255 });
    DrawCube({ core.x - 0.8f, 4.6f, core.z + 4.5f }, 1.7f, 8.6f, 1.7f, { 176, 178, 184, 255 });
    DrawCube({ core.x + 4.8f, 4.6f, core.z + 4.5f }, 1.7f, 8.6f, 1.7f, { 176, 178, 184, 255 });
    DrawCube({ core.x + 2.0f, 6.0f, core.z }, 5.4f, 2.2f, 5.2f, { 162, 166, 174, 255 });

    float gateRatio = (float)fortress.gateHp / (float)fortress.gateMaxHp;
    if (gateRatio < 0.0f) gateRatio = 0.0f;
    Color gateColor = fortress.gateHp > 0 ? Color{ (unsigned char)(124 + 56 * gateRatio), (unsigned char)(92 + 40 * gateRatio), 70, 255 } : Color{ 72, 54, 44, 255 };
    DrawCube({ gate.x + 1.8f, 1.6f, gate.z }, 2.2f, 3.2f, 5.4f, gateColor);
    DrawCubeWires({ gate.x + 1.8f, 1.6f, gate.z }, 2.2f, 3.2f, 5.4f, { 42, 30, 22, 255 });
    DrawCube({ gate.x + 3.2f, 2.8f, gate.z - 2.6f }, 0.9f, 5.8f, 0.9f, { 138, 144, 152, 255 });
    DrawCube({ gate.x + 3.2f, 2.8f, gate.z + 2.6f }, 0.9f, 5.8f, 0.9f, { 138, 144, 152, 255 });
    DrawCube({ gate.x + 4.0f, 4.2f, gate.z }, 2.8f, 2.4f, 6.8f, { 132, 138, 148, 255 });

    Vector3 wagonA = { core.x + 5.8f, 1.1f, core.z - 8.0f };
    Vector3 wagonB = { core.x + 5.8f, 1.1f, core.z + 8.0f };
    Vector3 wagonC = { core.x - 5.6f, 1.1f, core.z };
    DrawCube(wagonA, 3.6f, 1.6f, 2.4f, { 110, 84, 58, 255 });
    DrawCube(wagonB, 3.6f, 1.6f, 2.4f, { 110, 84, 58, 255 });
    DrawCube(wagonC, 3.2f, 1.6f, 3.4f, { 110, 84, 58, 255 });
    DrawCube({ wagonA.x, 2.5f, wagonA.z }, 2.2f, 1.2f, 1.6f, { 144, 124, 94, 255 });
    DrawCube({ wagonB.x, 2.5f, wagonB.z }, 2.2f, 1.2f, 1.6f, { 144, 124, 94, 255 });
    DrawCube({ wagonC.x, 2.5f, wagonC.z }, 1.8f, 1.2f, 2.2f, { 144, 124, 94, 255 });

    float wheelOffsetsA[2] = { -1.20f, 1.20f };
    for (float oz : wheelOffsetsA) {
        DrawCylinder({ wagonA.x - 1.2f, 0.38f, wagonA.z + oz }, 0.42f, 0.42f, 0.12f, 10, { 78, 60, 44, 255 });
        DrawCylinder({ wagonA.x + 1.2f, 0.38f, wagonA.z + oz }, 0.42f, 0.42f, 0.12f, 10, { 78, 60, 44, 255 });
        DrawCylinder({ wagonB.x - 1.2f, 0.38f, wagonB.z + oz }, 0.42f, 0.42f, 0.12f, 10, { 78, 60, 44, 255 });
        DrawCylinder({ wagonB.x + 1.2f, 0.38f, wagonB.z + oz }, 0.42f, 0.42f, 0.12f, 10, { 78, 60, 44, 255 });
    }
    float wheelOffsetsC[2] = { -1.05f, 1.05f };
    for (float ox : wheelOffsetsC) {
        DrawCylinder({ wagonC.x + ox, 0.38f, wagonC.z - 1.12f }, 0.40f, 0.40f, 0.12f, 10, { 78, 60, 44, 255 });
        DrawCylinder({ wagonC.x + ox, 0.38f, wagonC.z + 1.12f }, 0.40f, 0.40f, 0.12f, 10, { 78, 60, 44, 255 });
    }

    float glowPulse = 0.08f * std::sin(worldTime * 3.0f) + (hymnTimer > 0.0f ? 0.14f : 0.0f);
    DrawSphere({ core.x + 2.0f, 7.1f + glowPulse, core.z }, 0.92f + glowPulse, { 236, 202, 116, 255 });
    DrawSphere({ core.x + 2.0f, 7.1f + glowPulse * 1.4f, core.z }, 0.46f + glowPulse * 0.7f, { 255, 240, 178, 255 });
}

void Game::DrawTowers() const {
    for (int i = 0; i < (int)towers.size(); ++i) {
        const Tower& tower = towers[i];
        float levelLift = 0.14f * (float)(tower.level - 1);

        DrawCylinder({ tower.pos.x, 0.10f, tower.pos.z }, 0.90f, 0.90f, 0.12f, 14, { 44, 48, 54, 140 });

        if (tower.type == TowerType::WatchbowNest) {
            DrawCube({ tower.pos.x, 0.72f, tower.pos.z }, 1.20f, 1.25f, 1.20f, { 108, 86, 66, 255 });
            DrawCube({ tower.pos.x, 1.70f + levelLift, tower.pos.z }, 0.82f, 0.74f + levelLift, 0.82f, { 166, 146, 112, 255 });
            DrawCube({ tower.pos.x + 0.30f, 2.25f + levelLift, tower.pos.z }, 0.80f, 0.14f, 0.24f, { 184, 166, 120, 255 });
            DrawCube({ tower.pos.x - 0.18f, 2.25f + levelLift, tower.pos.z }, 0.26f, 0.14f, 0.82f, { 184, 166, 120, 255 });
        }
        else if (tower.type == TowerType::CenserShrine) {
            DrawCube({ tower.pos.x, 0.62f, tower.pos.z }, 1.20f, 1.02f, 1.20f, { 108, 92, 80, 255 });
            DrawCylinder({ tower.pos.x, 1.58f, tower.pos.z }, 0.34f, 0.42f, 1.52f + levelLift, 10, { 166, 132, 88, 255 });
            DrawSphere({ tower.pos.x, 2.58f + levelLift, tower.pos.z }, 0.34f + 0.06f * (float)(tower.level - 1), { 244, 166, 84, 255 });
            if (hymnTimer > 0.0f) {
                DrawSphere({ tower.pos.x, 2.88f + levelLift, tower.pos.z }, 0.16f, { 255, 228, 168, 220 });
            }
        }
        else if (tower.type == TowerType::ReliquarySpire) {
            DrawCube({ tower.pos.x, 0.62f, tower.pos.z }, 1.08f, 1.02f, 1.08f, { 86, 98, 122, 255 });
            DrawCube({ tower.pos.x, 1.88f + levelLift, tower.pos.z }, 0.52f, 2.28f + levelLift, 0.52f, { 108, 126, 160, 255 });
            DrawSphere({ tower.pos.x, 3.12f + levelLift, tower.pos.z }, 0.28f + 0.05f * (float)(tower.level - 1), { 122, 170, 236, 255 });
            float orbit = worldTime * 2.4f;
            DrawSphere({ tower.pos.x + 0.28f * std::sin(orbit), 2.76f + levelLift, tower.pos.z + 0.28f * std::cos(orbit) }, 0.10f, { 180, 214, 255, 255 });
        }
        else {
            DrawCube({ tower.pos.x, 0.44f, tower.pos.z }, 1.34f, 0.74f, 0.58f, { 118, 88, 64, 255 });
            DrawCube({ tower.pos.x, 0.64f, tower.pos.z - 0.28f }, 1.24f, 1.06f + levelLift, 0.14f, { 146, 104, 72, 255 });
            DrawCube({ tower.pos.x, 0.64f, tower.pos.z + 0.28f }, 1.24f, 1.06f + levelLift, 0.14f, { 146, 104, 72, 255 });
            DrawCube({ tower.pos.x - 0.42f, 0.54f, tower.pos.z }, 0.16f, 0.86f + levelLift, 0.48f, { 88, 64, 44, 255 });
            DrawCube({ tower.pos.x + 0.42f, 0.54f, tower.pos.z }, 0.16f, 0.86f + levelLift, 0.48f, { 88, 64, 44, 255 });
        }

        if (state == PlayState::BuildPhase && hoveredTowerIndex == i) {
            for (int ring = 0; ring < tower.level; ++ring) {
                DrawCircle3D({ tower.pos.x, 0.04f + 0.01f * (float)ring, tower.pos.z }, tower.range - 0.05f * (float)ring, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(tower.color, 0.18f));
            }
        }
    }
}

void Game::DrawEnemies() const {
    for (const Enemy& enemy : enemies) {
        Color baseColor = { 170, 82, 76, 255 };
        if (enemy.type == EnemyType::GraveBrute) baseColor = { 110, 88, 138, 255 };
        else if (enemy.type == EnemyType::BannerKnight) baseColor = { 170, 146, 88, 255 };
        else if (enemy.type == EnemyType::ProcessionBreaker) baseColor = { 178, 72, 72, 255 };
        if (enemy.slowTimer > 0.0f) baseColor = Tint(baseColor, 1.10f);
        Color color = enemy.hitFlash > 0.0f ? WHITE : baseColor;

        DrawCylinder({ enemy.pos.x, 0.08f, enemy.pos.z }, 0.60f, 0.60f, 0.08f, 12, { 40, 42, 48, 140 });

        if (enemy.type == EnemyType::AshRaider) {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 0.86f, 1.26f, 0.78f, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 0.84f, enemy.pos.z }, 0.24f, { 216, 188, 154, 255 });
            DrawCube({ enemy.pos.x + 0.26f, enemy.pos.y + 0.30f, enemy.pos.z }, 0.18f, 0.80f, 0.18f, { 94, 72, 58, 255 });
        }
        else if (enemy.type == EnemyType::GraveBrute) {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 1.18f, 1.72f, 1.02f, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 1.02f, enemy.pos.z }, 0.28f, { 204, 188, 194, 255 });
            DrawCube({ enemy.pos.x + 0.44f, enemy.pos.y + 0.34f, enemy.pos.z }, 0.22f, 1.12f, 0.22f, { 70, 64, 78, 255 });
            DrawCube({ enemy.pos.x + 0.44f, enemy.pos.y + 0.82f, enemy.pos.z }, 0.52f, 0.24f, 0.24f, { 116, 110, 130, 255 });
        }
        else if (enemy.type == EnemyType::BannerKnight) {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 0.96f, 1.50f, 0.86f, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 0.98f, enemy.pos.z }, 0.25f, { 224, 206, 170, 255 });
            DrawCube({ enemy.pos.x + 0.34f, enemy.pos.y + 0.64f, enemy.pos.z }, 0.12f, 1.30f, 0.12f, { 122, 122, 130, 255 });
            DrawCube({ enemy.pos.x + 0.70f, enemy.pos.y + 1.10f, enemy.pos.z }, 0.58f, 0.54f, 0.10f, { 164, 122, 64, 255 });
        }
        else {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 1.60f, 2.26f, 1.34f, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 1.26f, enemy.pos.z }, 0.34f, { 196, 172, 172, 255 });
            DrawCube({ enemy.pos.x, enemy.pos.y + 1.42f, enemy.pos.z }, 1.22f, 0.36f, 1.22f, { 104, 46, 46, 255 });
            DrawCube({ enemy.pos.x + 0.62f, enemy.pos.y + 0.42f, enemy.pos.z }, 0.24f, 1.30f, 0.24f, { 84, 64, 64, 255 });
            DrawCube({ enemy.pos.x + 0.62f, enemy.pos.y + 1.00f, enemy.pos.z }, 0.74f, 0.30f, 0.30f, { 122, 84, 84, 255 });
        }

        float hpRatio = (float)enemy.hp / (float)enemy.maxHp;
        if (hpRatio < 0.0f) hpRatio = 0.0f;
        float hpY = enemy.pos.y + ((enemy.type == EnemyType::ProcessionBreaker) ? 1.80f : (enemy.type == EnemyType::GraveBrute ? 1.30f : 1.12f));
        DrawCube({ enemy.pos.x, hpY, enemy.pos.z }, 1.18f, 0.10f, 0.16f, { 40, 10, 10, 255 });
        DrawCube({ enemy.pos.x - (1.18f * (1.0f - hpRatio)) * 0.5f, hpY + 0.01f, enemy.pos.z }, 1.18f * hpRatio, 0.06f, 0.12f, { 96, 220, 96, 255 });
    }
}

void Game::DrawEffects() const {
    for (const ShotFx& shot : shots) DrawLine3D(shot.start, shot.end, shot.color);
    for (const DeathFx& fx : deathFx) {
        float alpha = (fx.maxLife > 0.0f) ? (fx.life / fx.maxLife) : 0.0f;
        if (alpha < 0.0f) alpha = 0.0f;
        DrawCube(fx.pos, fx.size, fx.size, fx.size, Fade(fx.color, alpha));
    }
}

void Game::DrawUi() const {
    DrawRectangle(22, 18, 760, 170, Fade(BLACK, 0.68f));
    DrawRectangleLines(22, 18, 760, 170, { 188, 156, 96, 255 });
    DrawText("THE LAST PROCESSION", 40, 30, 34, { 236, 228, 210, 255 });
    DrawText("MEGA PROCESSION MAP", 40, 68, 20, { 196, 172, 118, 255 });
    DrawText(TextFormat("WAVE %d", wave.number), 40, 96, 24, { 188, 156, 96, 255 });
    DrawText(TextFormat("GOLD %d   IRON %d   EMBER %d", gold, iron, ember), 150, 96, 22, { 210, 214, 204, 255 });
    DrawText(TextFormat("GATE %d / %d", fortress.gateHp, fortress.gateMaxHp), 40, 126, 22, fortress.gateHp > 0 ? Color{ 210, 176, 112, 255 } : Color{ 198, 76, 76, 255 });
    DrawText(TextFormat("HOLY CORE %d / %d", fortress.coreHp, fortress.coreMaxHp), 240, 126, 22, fortress.coreHp > 30 ? Color{ 128, 196, 136, 255 } : Color{ 198, 76, 76, 255 });
    DrawText(TextFormat("ZOOM %.1f", cameraZoom), 500, 96, 22, { 198, 208, 214, 255 });
    DrawText(TextFormat("FERVOR %d / %d", fervor, fervorMax), 500, 126, 22, hymnTimer > 0.0f ? Color{ 250, 228, 164, 255 } : Color{ 168, 190, 216, 255 });
    DrawText(TextFormat("BUILD %s", BuildChoiceLabel(buildChoice)), 40, 154, 18, buildChoice == BuildChoice::WatchbowNest ? Color{ 194, 172, 118, 255 } : (buildChoice == BuildChoice::CenserShrine ? Color{ 244, 166, 84, 255 } : (buildChoice == BuildChoice::ReliquarySpire ? Color{ 122, 170, 236, 255 } : Color{ 162, 102, 76, 255 })));
    DrawText((wave.number % 5 == 0) ? "OMEN // BREAKER WAVE" : "OMEN // THREE-LANE ASSAULT", 500, 154, 18, (wave.number % 5 == 0) ? Color{ 206, 96, 96, 255 } : Color{ 168, 178, 188, 255 });

    DrawRectangle(22, screenH - 146, screenW - 44, 124, Fade(BLACK, 0.72f));
    DrawRectangleLines(22, screenH - 146, screenW - 44, 124, { 110, 126, 172, 255 });

    if (state == PlayState::BuildPhase) {
        DrawText("BUILD // 1 Bow  2 Censer  3 Spire  4 Barricade  U Upgrade  X Sell  H Repair Gate  J Consecrate Core", 40, screenH - 126, 24, { 228, 220, 208, 255 });
        DrawText("Huge battlefield   3 long roads   Mouse Wheel zoom   WASD axis pan   SPACE begin siege", 40, screenH - 92, 22, { 190, 198, 188, 255 });
    }
    else if (state == PlayState::BattlePhase) {
        int raiders = 0, brutes = 0, knights = 0, breakers = 0;
        for (const Enemy& enemy : enemies) {
            if (enemy.type == EnemyType::AshRaider) raiders++;
            else if (enemy.type == EnemyType::GraveBrute) brutes++;
            else if (enemy.type == EnemyType::BannerKnight) knights++;
            else if (enemy.type == EnemyType::ProcessionBreaker) breakers++;
        }
        DrawText(TextFormat("BATTLE // Raiders %d  Brutes %d  Knights %d  Breakers %d", raiders, brutes, knights, breakers), 40, screenH - 126, 24, { 228, 220, 208, 255 });
        DrawText("Kill to fill Fervor   Press F at full Fervor to awaken the War Hymn", 40, screenH - 92, 22, hymnTimer > 0.0f ? Color{ 248, 224, 160, 255 } : Color{ 190, 198, 188, 255 });
    }
    else {
        DrawText("GAME OVER // Press ENTER to restart the procession", 40, screenH - 110, 26, { 228, 220, 208, 255 });
    }

    if (announcementTimer > 0.0f) {
        int width = MeasureText(announcement.c_str(), 32);
        DrawRectangle(screenW / 2 - width / 2 - 24, 24, width + 48, 50, Fade(BLACK, 0.78f));
        DrawRectangleLines(screenW / 2 - width / 2 - 24, 24, width + 48, 50, { 188, 156, 96, 255 });
        DrawText(announcement.c_str(), screenW / 2 - width / 2, 34, 32, { 214, 186, 126, 255 });
    }

    if (hoveredValid) {
        DrawText(TextFormat("CELL %d, %d", hoveredCell.x, hoveredCell.y), screenW - 210, 24, 24, { 210, 218, 226, 255 });
    }

    if (hoveredTowerIndex >= 0 && hoveredTowerIndex < (int)towers.size()) {
        const Tower& tower = towers[hoveredTowerIndex];
        int costGold = GetTowerUpgradeGoldCost(tower);
        int costIron = GetTowerUpgradeIronCost(tower);
        int costEmber = GetTowerUpgradeEmberCost(tower);
        int sellGold = GetTowerSellGoldRefund(tower);
        int sellIron = GetTowerSellIronRefund(tower);
        int sellEmber = GetTowerSellEmberRefund(tower);

        DrawRectangle(screenW - 430, 60, 390, 184, Fade(BLACK, 0.72f));
        DrawRectangleLines(screenW - 430, 60, 390, 184, tower.color);
        DrawText(TowerLabel(tower.type), screenW - 410, 76, 28, { 236, 228, 210, 255 });
        DrawText(TextFormat("LEVEL %d", tower.level), screenW - 410, 110, 22, { 206, 212, 220, 255 });
        DrawText(TextFormat("DAMAGE %d", tower.damage), screenW - 410, 138, 22, { 206, 212, 220, 255 });
        DrawText(TextFormat("RANGE %.1f   RATE %.2f", tower.range, tower.maxCooldown), screenW - 410, 166, 22, { 206, 212, 220, 255 });
        if (tower.level < 3) {
            DrawText(TextFormat("U UPGRADE // G%d I%d E%d", costGold, costIron, costEmber), screenW - 410, 196, 20, { 220, 198, 136, 255 });
        }
        else {
            DrawText("MAX CONSECRATION REACHED", screenW - 410, 196, 20, { 220, 198, 136, 255 });
        }
        DrawText(TextFormat("X SELL // G%d I%d E%d", sellGold, sellIron, sellEmber), screenW - 410, 220, 20, { 196, 180, 156, 255 });
    }

    if (state == PlayState::GameOver) {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.54f));
        const char* title = "THE PROCESSION HAS FALLEN";
        int w = MeasureText(title, 54);
        DrawText(title, screenW / 2 - w / 2, screenH / 2 - 40, 54, { 210, 84, 84, 255 });
    }
}
