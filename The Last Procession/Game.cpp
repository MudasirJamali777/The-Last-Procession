#include "Game.h"
#include "Utils.h"
#include <algorithm>
#include <cmath>

Game::Game() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenW, screenH, "THE LAST PROCESSION");
    SetTargetFPS(60);

    camera.position = { 18.0f, 24.0f, 18.0f };
    camera.target = { 0.0f, 0.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 38.0f;
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
    gold = 85;
    iron = 55;
    ember = 18;
    towers.clear();
    enemies.clear();
    shots.clear();
    fortress = Fortress{};
    state = PlayState::BuildPhase;
    buildChoice = BuildChoice::WatchbowNest;
    announcement = "FORTIFY THE GATE // 1 WATCHBOW  2 CENSER  H REPAIR";
    announcementTimer = 3.6f;
    hoveredValid = false;
    hoveredCell = { -1, -1 };
    BuildMap();
    BuildWave(1);
}

void Game::BuildMap() {
    grid.width = 14;
    grid.height = 12;
    grid.cellSize = 2.0f;
    grid.origin = { -14.0f, 0.0f, -12.0f };
    grid.tiles.assign((size_t)grid.width * (size_t)grid.height, GridTile{});

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            grid.At(x, y).kind = TileKind::Buildable;
            grid.At(x, y).occupied = false;
        }
    }

    lanes.clear();
    lanes.resize(2);

    auto addSegment = [&](std::vector<GridCoord>& lane, int x0, int y0, int x1, int y1) {
        int dx = (x1 > x0) ? 1 : ((x1 < x0) ? -1 : 0);
        int dy = (y1 > y0) ? 1 : ((y1 < y0) ? -1 : 0);
        int x = x0;
        int y = y0;

        if (lane.empty() || lane.back().x != x || lane.back().y != y) {
            lane.push_back({ x, y });
        }

        while (x != x1 || y != y1) {
            x += dx;
            y += dy;
            lane.push_back({ x, y });
        }
        };

    addSegment(lanes[0], 0, 3, 4, 3);
    addSegment(lanes[0], 4, 3, 4, 4);
    addSegment(lanes[0], 4, 4, 6, 4);
    addSegment(lanes[0], 6, 4, 7, 5);

    addSegment(lanes[1], 0, 8, 4, 8);
    addSegment(lanes[1], 4, 8, 4, 7);
    addSegment(lanes[1], 4, 7, 6, 7);
    addSegment(lanes[1], 6, 7, 7, 6);
    addSegment(lanes[1], 7, 6, 7, 5);

    for (int laneIndex = 0; laneIndex < (int)lanes.size(); ++laneIndex) {
        for (int i = 0; i < (int)lanes[laneIndex].size(); ++i) {
            GridCoord c = lanes[laneIndex][i];
            if (!grid.InBounds(c.x, c.y)) continue;
            TileKind kind = (i == 0) ? TileKind::Spawn : TileKind::Road;
            grid.At(c.x, c.y).kind = kind;
        }
    }

    fortress.gateCell = { 7, 5 };
    fortress.coreCell = { 9, 5 };

    const GridCoord fortressCells[] = {
        { 8, 4 }, { 9, 4 }, { 10, 4 },
        { 8, 5 }, { 9, 5 }, { 10, 5 },
        { 8, 6 }, { 9, 6 }, { 10, 6 }
    };
    for (const GridCoord& c : fortressCells) {
        if (grid.InBounds(c.x, c.y)) {
            grid.At(c.x, c.y).kind = TileKind::Fortress;
        }
    }

    cameraFocus = grid.CellCenter(6, 5);
}

void Game::BuildWave(int waveNumber) {
    wave = WaveState{};
    wave.number = waveNumber;
    wave.active = false;

    int count = 6 + (waveNumber - 1) * 2;
    for (int i = 0; i < count; ++i) {
        SpawnEntry entry{};
        entry.spawnTime = 0.75f * i;
        entry.laneIndex = i % 2;
        entry.type = EnemyType::AshRaider;

        if (waveNumber >= 2 && (i % 4 == 3)) {
            entry.type = EnemyType::GraveBrute;
            entry.spawnTime += 0.25f;
        }
        if (waveNumber >= 4 && (i % 5 == 2)) {
            entry.type = EnemyType::GraveBrute;
        }

        wave.spawns.push_back(entry);
    }
}

void Game::StartWave() {
    if (state != PlayState::BuildPhase) {
        return;
    }

    wave.active = true;
    wave.timer = 0.0f;
    wave.nextSpawnIndex = 0;
    state = PlayState::BattlePhase;
    announcement = TextFormat("WAVE %d // DEFEND THE GATE", wave.number);
    announcementTimer = 2.5f;
}

void Game::SpawnEnemy(EnemyType type, int laneIndex) {
    if (laneIndex < 0 || laneIndex >= (int)lanes.size() || lanes[laneIndex].empty()) {
        laneIndex = 0;
    }

    Enemy enemy{};
    enemy.type = type;
    enemy.laneIndex = laneIndex;
    enemy.pathIndex = 0;

    if (type == EnemyType::GraveBrute) {
        enemy.hp = 70 + (wave.number - 1) * 14;
        enemy.maxHp = enemy.hp;
        enemy.speed = 1.10f + (wave.number - 1) * 0.08f;
        enemy.attackCooldown = 1.10f;
        enemy.gateDamage = 12;
        enemy.coreDamage = 14;
        enemy.pos.y = 0.75f;
    }
    else {
        enemy.hp = 28 + (wave.number - 1) * 7;
        enemy.maxHp = enemy.hp;
        enemy.speed = 1.70f + (wave.number - 1) * 0.14f;
        enemy.attackCooldown = 0.78f;
        enemy.gateDamage = 6;
        enemy.coreDamage = 8;
        enemy.pos.y = 0.55f;
    }

    Vector3 start = grid.CellCenter(lanes[laneIndex][0].x, lanes[laneIndex][0].y);
    enemy.pos.x = start.x - 1.2f;
    enemy.pos.z = start.z;
    enemies.push_back(enemy);
}

void Game::Update(float dt) {
    if (announcementTimer > 0.0f) {
        announcementTimer -= dt;
        if (announcementTimer < 0.0f) announcementTimer = 0.0f;
    }

    if (state == PlayState::GameOver) {
        if (IsKeyPressed(KEY_ENTER)) {
            ResetRun();
        }
        return;
    }

    UpdateCamera(dt);
    UpdateHoverCell();
    UpdateShots(dt);

    if (state == PlayState::BuildPhase) {
        UpdateBuildPhase();
    }
    else if (state == PlayState::BattlePhase) {
        UpdateBattlePhase(dt);
    }
}

void Game::UpdateCamera(float dt) {
    float move = 11.0f * dt;
    if (IsKeyDown(KEY_A)) {
        cameraFocus.x -= move;
        cameraFocus.z += move;
    }
    if (IsKeyDown(KEY_D)) {
        cameraFocus.x += move;
        cameraFocus.z -= move;
    }
    if (IsKeyDown(KEY_W)) {
        cameraFocus.x -= move;
        cameraFocus.z -= move;
    }
    if (IsKeyDown(KEY_S)) {
        cameraFocus.x += move;
        cameraFocus.z += move;
    }

    camera.target = cameraFocus;
    camera.position = { cameraFocus.x + 19.0f, 24.0f, cameraFocus.z + 19.0f };
}

void Game::UpdateHoverCell() {
    hoveredValid = false;
    hoveredCell = { -1, -1 };

    Vector3 worldPoint{};
    if (!RayToGround(&worldPoint)) {
        return;
    }

    GridCoord cell = grid.WorldToCell(worldPoint);
    if (!grid.InBounds(cell.x, cell.y)) {
        return;
    }

    hoveredValid = true;
    hoveredCell = cell;
}

void Game::UpdateBuildPhase() {
    if (IsKeyPressed(KEY_ONE)) {
        buildChoice = BuildChoice::WatchbowNest;
        announcement = "WATCHBOW NEST SELECTED";
        announcementTimer = 1.0f;
    }
    if (IsKeyPressed(KEY_TWO)) {
        buildChoice = BuildChoice::CenserShrine;
        announcement = "CENSER SHRINE SELECTED";
        announcementTimer = 1.0f;
    }
    if (IsKeyPressed(KEY_H)) {
        if (fortress.gateHp >= fortress.gateMaxHp) {
            announcement = "GATE ALREADY WHOLE";
            announcementTimer = 1.0f;
        }
        else if (iron >= 15) {
            iron -= 15;
            fortress.gateHp += 20;
            if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
            announcement = "GATE REPAIRED";
            announcementTimer = 1.2f;
        }
        else {
            announcement = "NOT ENOUGH IRON";
            announcementTimer = 1.0f;
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        TryPlaceTower();
    }

    if (IsKeyPressed(KEY_SPACE)) {
        StartWave();
    }
}

void Game::UpdateBattlePhase(float dt) {
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
        gold += 28 + wave.number * 8;
        iron += 12 + wave.number * 4 + (fortress.gateHp > 0 ? 6 : 0);
        ember += 3 + wave.number;
        wave.number++;
        BuildWave(wave.number);
        announcement = "SIEGE BROKEN // REBUILD AND REARM";
        announcementTimer = 3.0f;
    }
}

void Game::UpdateEnemies(float dt) {
    Vector3 gatePos = grid.CellCenter(fortress.gateCell.x, fortress.gateCell.y);
    gatePos.y = 0.55f;
    Vector3 corePos = grid.CellCenter(fortress.coreCell.x, fortress.coreCell.y);
    corePos.y = 0.55f;

    for (Enemy& enemy : enemies) {
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);

        if (!enemy.pastGate) {
            const std::vector<GridCoord>& lane = lanes[enemy.laneIndex];
            if (enemy.pathIndex < (int)lane.size()) {
                Vector3 target = grid.CellCenter(lane[enemy.pathIndex].x, lane[enemy.pathIndex].y);
                target.y = enemy.pos.y;
                Vector3 toTarget = Vec3Sub(target, enemy.pos);
                float dist = LengthXZ(toTarget);

                if (dist < 0.12f) {
                    enemy.pathIndex++;
                }
                else {
                    Vector3 dir = NormalizeXZ(toTarget);
                    enemy.pos = Vec3Add(enemy.pos, Vec3Scale(dir, enemy.speed * dt));
                }
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
        if (coreDist < 0.9f) {
            if (enemy.attackTimer <= 0.0f) {
                DamageCore(enemy.coreDamage);
                enemy.attackTimer = enemy.attackCooldown;
            }
        }
        else {
            Vector3 dir = NormalizeXZ(toCore);
            enemy.pos = Vec3Add(enemy.pos, Vec3Scale(dir, enemy.speed * dt));
        }
    }

    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& enemy) {
        return enemy.hp <= 0;
        }), enemies.end());
}

void Game::UpdateTowers(float dt) {
    for (Tower& tower : towers) {
        tower.cooldown -= dt;
        if (tower.cooldown > 0.0f) {
            continue;
        }

        if (tower.type == TowerType::CenserShrine) {
            bool hitAny = false;
            for (Enemy& enemy : enemies) {
                if (enemy.hp <= 0) continue;
                float dist = DistanceXZ(tower.pos, enemy.pos);
                if (dist <= tower.range) {
                    int before = enemy.hp;
                    enemy.hp -= tower.damage;
                    enemy.hitFlash = 0.08f;
                    if (before > 0 && enemy.hp <= 0) {
                        gold += (enemy.type == EnemyType::GraveBrute) ? 8 : 4;
                    }
                    hitAny = true;
                }
            }

            if (hitAny) {
                tower.cooldown = tower.maxCooldown;
                ShotFx fx{};
                fx.start = { tower.pos.x, tower.pos.y + 0.3f, tower.pos.z };
                fx.end = { tower.pos.x, tower.pos.y + 1.8f, tower.pos.z };
                fx.life = 0.12f;
                fx.color = { 244, 166, 84, 255 };
                shots.push_back(fx);
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
            score += enemies[i].pastGate ? 1000.0f : 0.0f;
            score += (float)enemies[i].pathIndex * 20.0f;
            score += (enemies[i].type == EnemyType::GraveBrute) ? 6.0f : 0.0f;
            score -= dist;
            if (score > bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }

        if (bestIndex >= 0) {
            int before = enemies[bestIndex].hp;
            enemies[bestIndex].hp -= tower.damage;
            enemies[bestIndex].hitFlash = 0.12f;
            tower.cooldown = tower.maxCooldown;

            ShotFx fx{};
            fx.start = { tower.pos.x, tower.pos.y + 1.2f, tower.pos.z };
            fx.end = { enemies[bestIndex].pos.x, enemies[bestIndex].pos.y + 0.2f, enemies[bestIndex].pos.z };
            fx.life = 0.08f;
            fx.color = tower.color;
            shots.push_back(fx);

            if (before > 0 && enemies[bestIndex].hp <= 0) {
                gold += (enemies[bestIndex].type == EnemyType::GraveBrute) ? 8 : 4;
            }
        }
    }
}

void Game::UpdateShots(float dt) {
    for (ShotFx& shot : shots) {
        shot.life -= dt;
    }
    shots.erase(std::remove_if(shots.begin(), shots.end(), [](const ShotFx& shot) {
        return shot.life <= 0.0f;
        }), shots.end());
}

Tower Game::MakeTower(BuildChoice choice, int cellX, int cellY) const {
    Tower tower{};
    tower.gridX = cellX;
    tower.gridY = cellY;
    tower.pos = grid.CellCenter(cellX, cellY);
    tower.pos.y = 0.8f;

    if (choice == BuildChoice::CenserShrine) {
        tower.type = TowerType::CenserShrine;
        tower.range = 3.35f;
        tower.maxCooldown = 0.55f;
        tower.damage = 4;
        tower.goldCost = 20;
        tower.emberCost = 8;
        tower.color = { 244, 166, 84, 255 };
    }
    else {
        tower.type = TowerType::WatchbowNest;
        tower.range = 5.6f;
        tower.maxCooldown = 0.72f;
        tower.damage = 10;
        tower.goldCost = 25;
        tower.emberCost = 0;
        tower.color = { 194, 172, 118, 255 };
    }

    return tower;
}

const char* Game::BuildChoiceLabel(BuildChoice choice) const {
    switch (choice) {
    case BuildChoice::WatchbowNest: return "WATCHBOW NEST";
    case BuildChoice::CenserShrine: return "CENSER SHRINE";
    }
    return "WATCHBOW NEST";
}

void Game::TryPlaceTower() {
    if (!hoveredValid || !grid.InBounds(hoveredCell.x, hoveredCell.y)) {
        return;
    }

    GridTile& tile = grid.At(hoveredCell.x, hoveredCell.y);
    if (tile.kind != TileKind::Buildable || tile.occupied || CellHasTower(hoveredCell.x, hoveredCell.y)) {
        announcement = "CANNOT BUILD THERE";
        announcementTimer = 1.2f;
        return;
    }

    Tower tower = MakeTower(buildChoice, hoveredCell.x, hoveredCell.y);
    if (gold < tower.goldCost) {
        announcement = "NOT ENOUGH GOLD";
        announcementTimer = 1.2f;
        return;
    }
    if (ember < tower.emberCost) {
        announcement = "NOT ENOUGH EMBER";
        announcementTimer = 1.2f;
        return;
    }

    towers.push_back(tower);
    tile.occupied = true;
    gold -= tower.goldCost;
    ember -= tower.emberCost;
    announcement = std::string(BuildChoiceLabel(buildChoice)) + " RAISED";
    announcementTimer = 1.3f;
}

void Game::DamageGate(int amount) {
    fortress.gateHp -= amount;
    if (fortress.gateHp < 0) fortress.gateHp = 0;
    announcement = TextFormat("FRONT GATE STRUCK // %d HP", fortress.gateHp);
    announcementTimer = 0.75f;
}

void Game::DamageCore(int amount) {
    fortress.coreHp -= amount;
    if (fortress.coreHp < 0) {
        fortress.coreHp = 0;
    }

    announcement = TextFormat("HOLY CORE STRUCK // %d HP", fortress.coreHp);
    announcementTimer = 1.2f;

    if (fortress.coreHp <= 0) {
        state = PlayState::GameOver;
        announcement = "THE PROCESSION HAS FALLEN";
        announcementTimer = 4.0f;
    }
}

bool Game::RayToGround(Vector3* outPoint) const {
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    if (std::fabs(ray.direction.y) < 0.0001f) {
        return false;
    }

    float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f) {
        return false;
    }

    outPoint->x = ray.position.x + ray.direction.x * t;
    outPoint->y = 0.0f;
    outPoint->z = ray.position.z + ray.direction.z * t;
    return true;
}

bool Game::CellHasTower(int x, int y) const {
    for (const Tower& tower : towers) {
        if (tower.gridX == x && tower.gridY == y) {
            return true;
        }
    }
    return false;
}

void Game::Draw() const {
    BeginDrawing();
    ClearBackground({ 22, 24, 31, 255 });

    DrawWorld();
    DrawUi();

    EndDrawing();
}

void Game::DrawWorld() const {
    BeginMode3D(camera);

    DrawPlane({ 0.0f, -0.01f, 0.0f }, { 68.0f, 68.0f }, { 46, 58, 44, 255 });
    DrawTiles();
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
            center.y = -0.05f;

            Color tileColor = { 90, 106, 84, 255 };
            if (tile.kind == TileKind::Road) tileColor = { 128, 114, 80, 255 };
            else if (tile.kind == TileKind::Spawn) tileColor = { 144, 84, 70, 255 };
            else if (tile.kind == TileKind::Fortress) tileColor = { 112, 122, 136, 255 };

            if (hoveredValid && state == PlayState::BuildPhase && hoveredCell.x == x && hoveredCell.y == y) {
                if (tile.kind == TileKind::Buildable && !tile.occupied) {
                    tileColor = (buildChoice == BuildChoice::WatchbowNest) ? Color{ 116, 156, 116, 255 } : Color{ 178, 128, 76, 255 };
                }
                else {
                    tileColor = { 166, 86, 86, 255 };
                }
            }

            DrawCube(center, grid.cellSize * 0.95f, 0.10f, grid.cellSize * 0.95f, tileColor);
            DrawCubeWires(center, grid.cellSize * 0.95f, 0.10f, grid.cellSize * 0.95f, { 28, 34, 26, 255 });
        }
    }
}

void Game::DrawFortress() const {
    Vector3 core = grid.CellCenter(fortress.coreCell.x, fortress.coreCell.y);
    core.y = 1.3f;

    DrawCube(core, 5.6f, 2.6f, 5.8f, { 138, 144, 156, 255 });
    DrawCubeWires(core, 5.6f, 2.6f, 5.8f, { 44, 48, 58, 255 });

    DrawCube({ core.x - 2.0f, 2.3f, core.z - 2.0f }, 1.0f, 4.4f, 1.0f, { 156, 160, 170, 255 });
    DrawCube({ core.x + 2.0f, 2.3f, core.z - 2.0f }, 1.0f, 4.4f, 1.0f, { 156, 160, 170, 255 });
    DrawCube({ core.x - 2.0f, 2.3f, core.z + 2.0f }, 1.0f, 4.4f, 1.0f, { 156, 160, 170, 255 });
    DrawCube({ core.x + 2.0f, 2.3f, core.z + 2.0f }, 1.0f, 4.4f, 1.0f, { 156, 160, 170, 255 });

    Vector3 gate = grid.CellCenter(fortress.gateCell.x, fortress.gateCell.y);
    gate.y = 0.85f;
    float gateRatio = (float)fortress.gateHp / (float)fortress.gateMaxHp;
    if (gateRatio < 0.0f) gateRatio = 0.0f;
    Color gateColor = (fortress.gateHp > 0) ? Color{ (unsigned char)(118 + 60 * gateRatio), (unsigned char)(94 + 40 * gateRatio), 72, 255 } : Color{ 74, 54, 46, 255 };
    DrawCube(gate, 1.3f, 1.7f, 3.7f, gateColor);
    DrawCubeWires(gate, 1.3f, 1.7f, 3.7f, { 42, 30, 22, 255 });

    DrawSphere({ core.x, 2.9f, core.z }, 0.46f, { 236, 202, 116, 255 });
    DrawSphere({ core.x, 2.9f, core.z }, 0.25f, { 255, 238, 178, 255 });
}

void Game::DrawTowers() const {
    for (const Tower& tower : towers) {
        if (tower.type == TowerType::CenserShrine) {
            DrawCube({ tower.pos.x, 0.42f, tower.pos.z }, 1.0f, 0.85f, 1.0f, { 108, 92, 80, 255 });
            DrawCylinder({ tower.pos.x, 1.15f, tower.pos.z }, 0.28f, 0.34f, 1.2f, 8, { 166, 132, 88, 255 });
            DrawSphere({ tower.pos.x, 1.95f, tower.pos.z }, 0.24f, { 244, 166, 84, 255 });
        }
        else {
            DrawCube({ tower.pos.x, 0.55f, tower.pos.z }, 1.0f, 1.1f, 1.0f, { 110, 92, 76, 255 });
            DrawCube({ tower.pos.x, 1.35f, tower.pos.z }, 0.65f, 0.65f, 0.65f, { 166, 146, 112, 255 });
            DrawCylinder({ tower.pos.x, 1.9f, tower.pos.z }, 0.18f, 0.18f, 0.9f, 8, { 194, 172, 118, 255 });
        }

        if (state == PlayState::BuildPhase) {
            DrawCircle3D({ tower.pos.x, 0.03f, tower.pos.z }, tower.range, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(tower.color, 0.12f));
        }
    }
}

void Game::DrawEnemies() const {
    for (const Enemy& enemy : enemies) {
        Color baseColor = (enemy.type == EnemyType::GraveBrute) ? Color{ 110, 88, 138, 255 } : Color{ 170, 82, 76, 255 };
        Color color = enemy.hitFlash > 0.0f ? WHITE : baseColor;
        float size = (enemy.type == EnemyType::GraveBrute) ? 1.1f : 0.8f;
        float height = (enemy.type == EnemyType::GraveBrute) ? 1.5f : 1.1f;
        DrawCube(enemy.pos, size, height, size, color);
        DrawCubeWires(enemy.pos, size, height, size, { 48, 20, 20, 255 });

        float hpRatio = (float)enemy.hp / (float)enemy.maxHp;
        if (hpRatio < 0.0f) hpRatio = 0.0f;
        DrawCube({ enemy.pos.x, enemy.pos.y + height * 0.72f, enemy.pos.z }, 0.9f, 0.08f, 0.14f, { 40, 10, 10, 255 });
        DrawCube({ enemy.pos.x - (0.9f * (1.0f - hpRatio)) * 0.5f, enemy.pos.y + height * 0.73f, enemy.pos.z }, 0.9f * hpRatio, 0.05f, 0.10f, { 96, 220, 96, 255 });
    }
}

void Game::DrawEffects() const {
    for (const ShotFx& shot : shots) {
        DrawLine3D(shot.start, shot.end, shot.color);
    }
}

void Game::DrawUi() const {
    DrawRectangle(16, 16, 430, 150, Fade(BLACK, 0.58f));
    DrawRectangleLines(16, 16, 430, 150, { 188, 156, 96, 255 });
    DrawText("THE LAST PROCESSION // BATCH 2", 28, 28, 24, { 232, 220, 198, 255 });
    DrawText(TextFormat("WAVE %d", wave.number), 28, 60, 20, { 188, 156, 96, 255 });
    DrawText(TextFormat("GOLD %d   IRON %d   EMBER %d", gold, iron, ember), 28, 86, 20, { 200, 210, 188, 255 });
    DrawText(TextFormat("GATE %d / %d", fortress.gateHp, fortress.gateMaxHp), 28, 112, 20, fortress.gateHp > 0 ? Color{ 188, 156, 96, 255 } : Color{ 188, 76, 76, 255 });
    DrawText(TextFormat("HOLY CORE %d / %d", fortress.coreHp, fortress.coreMaxHp), 220, 112, 20, fortress.coreHp > 30 ? Color{ 118, 184, 126, 255 } : Color{ 188, 76, 76, 255 });
    DrawText(TextFormat("BUILD: %s", BuildChoiceLabel(buildChoice)), 220, 60, 18, buildChoice == BuildChoice::WatchbowNest ? Color{ 194, 172, 118, 255 } : Color{ 244, 166, 84, 255 });

    DrawRectangle(16, screenH - 104, screenW - 32, 88, Fade(BLACK, 0.62f));
    DrawRectangleLines(16, screenH - 104, screenW - 32, 88, { 110, 126, 172, 255 });

    if (state == PlayState::BuildPhase) {
        DrawText("BUILD PHASE // 1 Watchbow (25G)   2 Censer Shrine (20G + 8E)   H Repair Gate (15 Iron)", 28, screenH - 88, 20, { 226, 218, 206, 255 });
        DrawText("Left Click build on green tiles // SPACE start siege // WASD pan camera", 28, screenH - 58, 20, { 190, 198, 188, 255 });
    }
    else if (state == PlayState::BattlePhase) {
        int raiders = 0;
        int brutes = 0;
        for (const Enemy& enemy : enemies) {
            if (enemy.type == EnemyType::GraveBrute) brutes++;
            else raiders++;
        }
        DrawText(TextFormat("BATTLE PHASE // Raiders %d   Brutes %d   Towers %d", raiders, brutes, (int)towers.size()), 28, screenH - 88, 20, { 226, 218, 206, 255 });
        DrawText("Protect the gate first // Brutes hit harder and crack the procession open", 28, screenH - 58, 20, { 190, 198, 188, 255 });
    }
    else {
        DrawText("GAME OVER // Press ENTER to restart the procession", 28, screenH - 74, 20, { 226, 218, 206, 255 });
    }

    if (announcementTimer > 0.0f) {
        int width = MeasureText(announcement.c_str(), 26);
        DrawRectangle(screenW / 2 - width / 2 - 18, 18, width + 36, 40, Fade(BLACK, 0.70f));
        DrawRectangleLines(screenW / 2 - width / 2 - 18, 18, width + 36, 40, { 188, 156, 96, 255 });
        DrawText(announcement.c_str(), screenW / 2 - width / 2, 26, 26, { 188, 156, 96, 255 });
    }

    if (hoveredValid) {
        DrawText(TextFormat("CELL %d, %d", hoveredCell.x, hoveredCell.y), screenW - 180, 22, 20, { 200, 210, 188, 255 });
    }

    if (state == PlayState::GameOver) {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.48f));
        const char* title = "THE PROCESSION HAS FALLEN";
        int w = MeasureText(title, 42);
        DrawText(title, screenW / 2 - w / 2, screenH / 2 - 30, 42, { 196, 82, 82, 255 });
    }
}
