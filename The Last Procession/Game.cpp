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
    gold = 110;
    iron = 65;
    ember = 22;
    fervor = 0;
    hymnTimer = 0.0f;
    worldTime = 0.0f;
    towers.clear();
    enemies.clear();
    shots.clear();
    fortress = Fortress{};
    state = PlayState::BuildPhase;
    buildChoice = BuildChoice::WatchbowNest;
    announcement = "FORTIFY THE ROAD // 1 WATCHBOW  2 CENSER  3 SPIRE  U UPGRADE  H REPAIR";
    announcementTimer = 4.4f;
    hoveredValid = false;
    hoveredCell = { -1, -1 };
    hoveredTowerIndex = -1;
    cameraZoom = 22.0f;
    BuildMap();
    BuildWave(1);
}

void Game::AddProp(PropType type, int x, int y, bool blockCell) {
    if (!grid.InBounds(x, y)) {
        return;
    }

    props.push_back({ type, { x, y } });
    if (blockCell) {
        GridTile& tile = grid.At(x, y);
        if (tile.kind == TileKind::Buildable) {
            tile.kind = TileKind::Blocked;
        }
        tile.occupied = true;
        tile.height = std::max(tile.height, 0.28f);
    }
}

void Game::BuildMap() {
    grid.width = 16;
    grid.height = 14;
    grid.cellSize = 2.0f;
    grid.origin = { -16.0f, 0.0f, -14.0f };
    grid.tiles.assign((size_t)grid.width * (size_t)grid.height, GridTile{});
    props.clear();

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            GridTile& tile = grid.At(x, y);
            tile.kind = TileKind::Buildable;
            tile.occupied = false;
            float base = 0.18f;
            base += 0.03f * (float)((x + y) % 3);
            base += 0.02f * (float)((x * 7 + y * 11) % 2);
            tile.height = base;
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
    addSegment(lanes[0], 4, 3, 5, 4);
    addSegment(lanes[0], 5, 4, 6, 4);
    addSegment(lanes[0], 6, 4, 7, 5);
    addSegment(lanes[0], 7, 5, 8, 5);

    addSegment(lanes[1], 0, 10, 4, 10);
    addSegment(lanes[1], 4, 10, 5, 9);
    addSegment(lanes[1], 5, 9, 6, 8);
    addSegment(lanes[1], 6, 8, 7, 7);
    addSegment(lanes[1], 7, 7, 8, 6);
    addSegment(lanes[1], 8, 6, 8, 5);

    for (int laneIndex = 0; laneIndex < (int)lanes.size(); ++laneIndex) {
        for (int i = 0; i < (int)lanes[laneIndex].size(); ++i) {
            GridCoord c = lanes[laneIndex][i];
            if (!grid.InBounds(c.x, c.y)) continue;
            GridTile& tile = grid.At(c.x, c.y);
            tile.kind = (i == 0) ? TileKind::Spawn : TileKind::Road;
            tile.occupied = false;
            tile.height = (i == 0) ? 0.06f : 0.08f;
        }
    }

    fortress.gateCell = { 8, 5 };
    fortress.coreCell = { 10, 5 };

    const GridCoord fortressCells[] = {
        { 9, 4 }, { 10, 4 }, { 11, 4 },
        { 9, 5 }, { 10, 5 }, { 11, 5 },
        { 9, 6 }, { 10, 6 }, { 11, 6 }
    };
    for (const GridCoord& c : fortressCells) {
        if (grid.InBounds(c.x, c.y)) {
            GridTile& tile = grid.At(c.x, c.y);
            tile.kind = TileKind::Fortress;
            tile.occupied = true;
            tile.height = 0.12f;
        }
    }

    GridTile& gateTile = grid.At(fortress.gateCell.x, fortress.gateCell.y);
    gateTile.kind = TileKind::Road;
    gateTile.occupied = false;
    gateTile.height = 0.08f;

    const GridCoord blockedCells[] = {
        { 2, 1 }, { 3, 1 }, { 5, 1 }, { 6, 2 }, { 12, 2 },
        { 1, 5 }, { 3, 6 }, { 13, 3 }, { 14, 4 }, { 12, 8 },
        { 2, 12 }, { 4, 12 }, { 6, 11 }, { 13, 10 }, { 14, 11 },
        { 5, 6 }, { 5, 7 }, { 12, 5 }, { 12, 6 }
    };
    for (const GridCoord& c : blockedCells) {
        if (!grid.InBounds(c.x, c.y)) continue;
        GridTile& tile = grid.At(c.x, c.y);
        if (tile.kind == TileKind::Buildable) {
            tile.kind = TileKind::Blocked;
            tile.occupied = true;
            tile.height = 0.30f;
        }
    }

    AddProp(PropType::DeadTree, 2, 1, true);
    AddProp(PropType::DeadTree, 5, 1, true);
    AddProp(PropType::DeadTree, 13, 10, true);
    AddProp(PropType::DeadTree, 14, 11, true);
    AddProp(PropType::DeadTree, 3, 6, true);

    AddProp(PropType::GraveMarker, 12, 2, true);
    AddProp(PropType::GraveMarker, 13, 3, true);
    AddProp(PropType::GraveMarker, 2, 12, true);
    AddProp(PropType::GraveMarker, 4, 12, true);
    AddProp(PropType::GraveMarker, 12, 8, true);

    AddProp(PropType::RubblePile, 1, 5, true);
    AddProp(PropType::RubblePile, 6, 2, true);
    AddProp(PropType::RubblePile, 14, 4, true);
    AddProp(PropType::RubblePile, 6, 11, true);

    AddProp(PropType::CartWreck, 5, 6, true);
    AddProp(PropType::CartWreck, 12, 5, true);

    AddProp(PropType::Brazier, 8, 4, false);
    AddProp(PropType::Brazier, 8, 6, false);
    AddProp(PropType::Brazier, 11, 4, false);
    AddProp(PropType::Brazier, 11, 6, false);

    AddProp(PropType::BannerPole, 9, 3, false);
    AddProp(PropType::BannerPole, 11, 3, false);
    AddProp(PropType::BannerPole, 9, 7, false);
    AddProp(PropType::BannerPole, 11, 7, false);

    cameraFocus = grid.CellCenter(7, 6);
}

void Game::BuildWave(int waveNumber) {
    wave = WaveState{};
    wave.number = waveNumber;
    wave.active = false;

    int count = 7 + (waveNumber - 1) * 2;
    for (int i = 0; i < count; ++i) {
        SpawnEntry entry{};
        entry.spawnTime = 0.70f * i;
        entry.laneIndex = i % 2;
        entry.type = EnemyType::AshRaider;

        if (waveNumber >= 2 && (i % 4 == 3)) {
            entry.type = EnemyType::GraveBrute;
            entry.spawnTime += 0.2f;
        }
        if (waveNumber >= 4 && (i % 5 == 1)) {
            entry.type = EnemyType::GraveBrute;
        }
        if (waveNumber >= 6 && (i % 3 == 0)) {
            entry.spawnTime -= 0.08f;
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
    announcement = TextFormat("WAVE %d // THE ROAD DARKENS", wave.number);
    announcementTimer = 2.4f;
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
        enemy.hp = 76 + (wave.number - 1) * 16;
        enemy.maxHp = enemy.hp;
        enemy.speed = 1.12f + (wave.number - 1) * 0.08f;
        enemy.attackCooldown = 1.05f;
        enemy.gateDamage = 13;
        enemy.coreDamage = 14;
        enemy.pos.y = 0.78f;
    }
    else {
        enemy.hp = 30 + (wave.number - 1) * 8;
        enemy.maxHp = enemy.hp;
        enemy.speed = 1.82f + (wave.number - 1) * 0.14f;
        enemy.attackCooldown = 0.80f;
        enemy.gateDamage = 6;
        enemy.coreDamage = 8;
        enemy.pos.y = 0.56f;
    }

    Vector3 start = grid.CellCenter(lanes[laneIndex][0].x, lanes[laneIndex][0].y);
    enemy.pos.x = start.x - 1.0f;
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
    float move = 12.0f * dt;
    if (IsKeyDown(KEY_A)) cameraFocus.x -= move;
    if (IsKeyDown(KEY_D)) cameraFocus.x += move;
    if (IsKeyDown(KEY_W)) cameraFocus.z -= move;
    if (IsKeyDown(KEY_S)) cameraFocus.z += move;

    cameraZoom -= GetMouseWheelMove() * 1.6f;
    cameraZoom = ClampFloat(cameraZoom, cameraMinZoom, cameraMaxZoom);

    float minX = grid.origin.x + 5.0f;
    float maxX = grid.origin.x + grid.width * grid.cellSize - 5.0f;
    float minZ = grid.origin.z + 5.0f;
    float maxZ = grid.origin.z + grid.height * grid.cellSize - 5.0f;

    cameraFocus.x = ClampFloat(cameraFocus.x, minX, maxX);
    cameraFocus.z = ClampFloat(cameraFocus.z, minZ, maxZ);

    camera.target = { cameraFocus.x, 0.5f, cameraFocus.z };
    camera.position = { cameraFocus.x + cameraZoom, cameraZoom * 1.08f, cameraFocus.z + cameraZoom };
}

void Game::UpdateHoverCell() {
    hoveredValid = false;
    hoveredCell = { -1, -1 };
    hoveredTowerIndex = -1;

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
    hoveredTowerIndex = FindTowerIndexAtCell(cell.x, cell.y);
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
    if (IsKeyPressed(KEY_THREE)) {
        buildChoice = BuildChoice::ReliquarySpire;
        announcement = "RELIQUARY SPIRE SELECTED";
        announcementTimer = 1.0f;
    }
    if (IsKeyPressed(KEY_U)) {
        TryUpgradeTower();
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
    if (IsKeyPressed(KEY_F)) {
        TriggerWarHymn();
    }

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
        gold += 30 + wave.number * 9;
        iron += 13 + wave.number * 4 + (fortress.gateHp > 0 ? 6 : 0);
        ember += 4 + wave.number;
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
    corePos.y = 0.55f;

    for (Enemy& enemy : enemies) {
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        enemy.slowTimer = std::max(0.0f, enemy.slowTimer - dt);
        enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);

        float speedMul = 1.0f;
        if (enemy.slowTimer > 0.0f) speedMul *= 0.58f;
        if (hymnTimer > 0.0f) speedMul *= 0.84f;

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
                    enemy.pos = Vec3Add(enemy.pos, Vec3Scale(dir, enemy.speed * speedMul * dt));
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
        if (coreDist < 1.0f) {
            if (enemy.attackTimer <= 0.0f) {
                DamageCore(enemy.coreDamage);
                enemy.attackTimer = enemy.attackCooldown;
            }
        }
        else {
            Vector3 dir = NormalizeXZ(toCore);
            enemy.pos = Vec3Add(enemy.pos, Vec3Scale(dir, enemy.speed * speedMul * dt));
        }
    }

    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& enemy) {
        return enemy.hp <= 0;
        }), enemies.end());
}

void Game::UpdateTowers(float dt) {
    float towerSpeedMul = hymnTimer > 0.0f ? 1.75f : 1.0f;

    for (Tower& tower : towers) {
        tower.cooldown -= dt * towerSpeedMul;
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
                        GainFervor((enemy.type == EnemyType::GraveBrute) ? 14 : 8);
                    }
                    hitAny = true;
                }
            }

            if (hitAny) {
                tower.cooldown = tower.maxCooldown;
                ShotFx fx{};
                fx.start = { tower.pos.x, tower.pos.y + 0.35f, tower.pos.z };
                fx.end = { tower.pos.x, tower.pos.y + 1.9f, tower.pos.z };
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
            score += (float)enemies[i].pathIndex * 16.0f;
            score += (enemies[i].type == EnemyType::GraveBrute) ? 10.0f : 0.0f;
            score -= dist;
            if (score > bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }

        if (bestIndex >= 0) {
            Enemy& target = enemies[bestIndex];
            int before = target.hp;
            target.hp -= tower.damage;
            target.hitFlash = 0.12f;
            if (tower.type == TowerType::ReliquarySpire) {
                target.slowTimer = 1.5f + 0.25f * (float)tower.level;
            }
            tower.cooldown = tower.maxCooldown;

            ShotFx fx{};
            fx.start = { tower.pos.x, tower.pos.y + 1.2f, tower.pos.z };
            fx.end = { target.pos.x, target.pos.y + 0.2f, target.pos.z };
            fx.life = 0.10f;
            fx.color = tower.color;
            shots.push_back(fx);

            if (before > 0 && target.hp <= 0) {
                gold += (target.type == EnemyType::GraveBrute) ? 8 : 4;
                GainFervor((target.type == EnemyType::GraveBrute) ? 14 : 8);
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

void Game::TryPlaceTower() {
    if (!hoveredValid || !grid.InBounds(hoveredCell.x, hoveredCell.y)) {
        return;
    }

    GridTile& tile = grid.At(hoveredCell.x, hoveredCell.y);
    if (tile.kind != TileKind::Buildable || tile.occupied || hoveredTowerIndex >= 0) {
        announcement = "CANNOT BUILD THERE";
        announcementTimer = 1.1f;
        return;
    }

    Tower tower = MakeTower(buildChoice, hoveredCell.x, hoveredCell.y);
    if (gold < tower.goldCost) {
        announcement = "NOT ENOUGH GOLD";
        announcementTimer = 1.1f;
        return;
    }
    if (ember < tower.emberCost) {
        announcement = "NOT ENOUGH EMBER";
        announcementTimer = 1.1f;
        return;
    }

    towers.push_back(tower);
    tile.occupied = true;
    gold -= tower.goldCost;
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
    int emberCost = GetTowerUpgradeEmberCost(tower);
    if (gold < goldCost) {
        announcement = "NOT ENOUGH GOLD";
        announcementTimer = 1.0f;
        return;
    }
    if (ember < emberCost) {
        announcement = "NOT ENOUGH EMBER";
        announcementTimer = 1.0f;
        return;
    }

    gold -= goldCost;
    ember -= emberCost;
    tower.level++;
    ApplyTowerStats(tower);
    announcement = TextFormat("%s UPGRADED TO LVL %d", TowerLabel(tower.type), tower.level);
    announcementTimer = 1.4f;
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

int Game::FindTowerIndexAtCell(int x, int y) const {
    for (int i = 0; i < (int)towers.size(); ++i) {
        if (towers[i].gridX == x && towers[i].gridY == y) {
            return i;
        }
    }
    return -1;
}

Tower Game::MakeTower(BuildChoice choice, int cellX, int cellY) const {
    Tower tower{};
    tower.gridX = cellX;
    tower.gridY = cellY;
    tower.level = 1;
    tower.pos = grid.CellCenter(cellX, cellY);
    tower.pos.y = 0.8f;

    if (choice == BuildChoice::CenserShrine) {
        tower.type = TowerType::CenserShrine;
    }
    else if (choice == BuildChoice::ReliquarySpire) {
        tower.type = TowerType::ReliquarySpire;
    }
    else {
        tower.type = TowerType::WatchbowNest;
    }

    ApplyTowerStats(tower);
    return tower;
}

void Game::ApplyTowerStats(Tower& tower) const {
    if (tower.type == TowerType::CenserShrine) {
        tower.range = 3.2f + 0.35f * (float)(tower.level - 1);
        tower.maxCooldown = 0.58f - 0.05f * (float)(tower.level - 1);
        if (tower.maxCooldown < 0.42f) tower.maxCooldown = 0.42f;
        tower.damage = 4 + 2 * (tower.level - 1);
        tower.goldCost = 20;
        tower.emberCost = 8;
        tower.color = { 244, 166, 84, 255 };
    }
    else if (tower.type == TowerType::ReliquarySpire) {
        tower.range = 5.3f + 0.45f * (float)(tower.level - 1);
        tower.maxCooldown = 1.08f - 0.10f * (float)(tower.level - 1);
        if (tower.maxCooldown < 0.82f) tower.maxCooldown = 0.82f;
        tower.damage = 7 + 3 * (tower.level - 1);
        tower.goldCost = 32;
        tower.emberCost = 10;
        tower.color = { 122, 170, 236, 255 };
    }
    else {
        tower.range = 5.6f + 0.55f * (float)(tower.level - 1);
        tower.maxCooldown = 0.72f - 0.08f * (float)(tower.level - 1);
        if (tower.maxCooldown < 0.50f) tower.maxCooldown = 0.50f;
        tower.damage = 10 + 5 * (tower.level - 1);
        tower.goldCost = 25;
        tower.emberCost = 0;
        tower.color = { 194, 172, 118, 255 };
    }

    if (tower.cooldown > tower.maxCooldown) {
        tower.cooldown = tower.maxCooldown;
    }
}

const char* Game::BuildChoiceLabel(BuildChoice choice) const {
    switch (choice) {
    case BuildChoice::WatchbowNest: return "WATCHBOW NEST";
    case BuildChoice::CenserShrine: return "CENSER SHRINE";
    case BuildChoice::ReliquarySpire: return "RELIQUARY SPIRE";
    }
    return "WATCHBOW NEST";
}

const char* Game::TowerLabel(TowerType type) const {
    switch (type) {
    case TowerType::WatchbowNest: return "WATCHBOW NEST";
    case TowerType::CenserShrine: return "CENSER SHRINE";
    case TowerType::ReliquarySpire: return "RELIQUARY SPIRE";
    }
    return "WATCHBOW NEST";
}

int Game::GetTowerUpgradeGoldCost(const Tower& tower) const {
    int base = 18 + tower.level * 12;
    if (tower.type == TowerType::CenserShrine) base += 4;
    if (tower.type == TowerType::ReliquarySpire) base += 10;
    return base;
}

int Game::GetTowerUpgradeEmberCost(const Tower& tower) const {
    if (tower.type == TowerType::WatchbowNest) return 0;
    if (tower.type == TowerType::CenserShrine) return 4 + tower.level * 2;
    return 6 + tower.level * 2;
}

void Game::Draw() const {
    BeginDrawing();
    ClearBackground({ 19, 22, 30, 255 });

    DrawWorld();
    DrawUi();

    EndDrawing();
}

void Game::DrawWorld() const {
    BeginMode3D(camera);

    DrawPlane({ 0.0f, -0.05f, 0.0f }, { 84.0f, 84.0f }, { 38, 48, 38, 255 });
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

            Color tileColor = { 88, 104, 82, 255 };
            if (tile.kind == TileKind::Road) tileColor = { 128, 114, 82, 255 };
            else if (tile.kind == TileKind::Spawn) tileColor = { 142, 84, 68, 255 };
            else if (tile.kind == TileKind::Fortress) tileColor = { 114, 122, 136, 255 };
            else if (tile.kind == TileKind::Blocked) tileColor = { 74, 82, 70, 255 };

            if (hoveredValid && state == PlayState::BuildPhase && hoveredCell.x == x && hoveredCell.y == y) {
                if (hoveredTowerIndex >= 0) {
                    tileColor = { 110, 140, 190, 255 };
                }
                else if (tile.kind == TileKind::Buildable && !tile.occupied) {
                    if (buildChoice == BuildChoice::WatchbowNest) tileColor = { 116, 156, 116, 255 };
                    else if (buildChoice == BuildChoice::CenserShrine) tileColor = { 178, 128, 76, 255 };
                    else tileColor = { 92, 132, 188, 255 };
                }
                else {
                    tileColor = { 166, 86, 86, 255 };
                }
            }

            DrawCube(center, grid.cellSize * 0.95f, tile.height, grid.cellSize * 0.95f, tileColor);
            DrawCubeWires(center, grid.cellSize * 0.95f, tile.height, grid.cellSize * 0.95f, { 28, 34, 26, 255 });
        }
    }
}

void Game::DrawEnvironment() const {
    for (const Prop& prop : props) {
        Vector3 center = grid.CellCenter(prop.cell.x, prop.cell.y);
        float ground = grid.At(prop.cell.x, prop.cell.y).height;
        center.y = ground;

        if (prop.type == PropType::DeadTree) {
            DrawCube({ center.x, center.y + 0.85f, center.z }, 0.32f, 1.6f, 0.32f, { 82, 62, 50, 255 });
            DrawCube({ center.x + 0.28f, center.y + 1.35f, center.z + 0.10f }, 0.70f, 0.14f, 0.14f, { 92, 70, 54, 255 });
            DrawCube({ center.x - 0.22f, center.y + 1.05f, center.z - 0.18f }, 0.56f, 0.14f, 0.14f, { 92, 70, 54, 255 });
        }
        else if (prop.type == PropType::GraveMarker) {
            DrawCube({ center.x, center.y + 0.30f, center.z }, 0.72f, 0.60f, 0.18f, { 124, 128, 134, 255 });
            DrawCube({ center.x, center.y + 0.62f, center.z }, 0.46f, 0.14f, 0.20f, { 146, 150, 158, 255 });
        }
        else if (prop.type == PropType::RubblePile) {
            DrawCube({ center.x - 0.18f, center.y + 0.12f, center.z - 0.12f }, 0.52f, 0.22f, 0.44f, { 104, 96, 88, 255 });
            DrawCube({ center.x + 0.14f, center.y + 0.18f, center.z + 0.12f }, 0.44f, 0.32f, 0.38f, { 126, 118, 106, 255 });
            DrawCube({ center.x + 0.02f, center.y + 0.10f, center.z + 0.28f }, 0.36f, 0.18f, 0.26f, { 88, 82, 76, 255 });
        }
        else if (prop.type == PropType::CartWreck) {
            DrawCube({ center.x, center.y + 0.22f, center.z }, 0.95f, 0.25f, 0.65f, { 110, 84, 58, 255 });
            DrawCube({ center.x - 0.28f, center.y + 0.52f, center.z }, 0.12f, 0.44f, 0.12f, { 94, 72, 50, 255 });
            DrawCube({ center.x + 0.28f, center.y + 0.52f, center.z }, 0.12f, 0.44f, 0.12f, { 94, 72, 50, 255 });
            DrawCylinder({ center.x - 0.42f, center.y + 0.14f, center.z + 0.24f }, 0.18f, 0.18f, 0.08f, 8, { 86, 66, 50, 255 });
            DrawCylinder({ center.x + 0.42f, center.y + 0.14f, center.z - 0.24f }, 0.18f, 0.18f, 0.08f, 8, { 86, 66, 50, 255 });
        }
        else if (prop.type == PropType::Brazier) {
            float flicker = 0.12f + 0.05f * std::sin(worldTime * 6.0f + (float)(prop.cell.x + prop.cell.y));
            DrawCylinder({ center.x, center.y + 0.28f, center.z }, 0.22f, 0.28f, 0.40f, 8, { 100, 88, 74, 255 });
            DrawSphere({ center.x, center.y + 0.68f + flicker, center.z }, 0.18f + flicker * 0.30f, { 248, 170, 92, 255 });
            DrawSphere({ center.x, center.y + 0.80f + flicker, center.z }, 0.10f + flicker * 0.22f, { 255, 226, 164, 255 });
        }
        else if (prop.type == PropType::BannerPole) {
            DrawCylinder({ center.x, center.y + 1.30f, center.z }, 0.08f, 0.08f, 2.50f, 6, { 126, 126, 138, 255 });
            float sway = 0.16f * std::sin(worldTime * 2.5f + (float)prop.cell.x);
            DrawCube({ center.x + 0.38f, center.y + 2.00f, center.z + sway }, 0.64f, 0.78f, 0.08f, { 124, 42, 42, 255 });
        }
    }
}

void Game::DrawFortress() const {
    Vector3 core = grid.CellCenter(fortress.coreCell.x, fortress.coreCell.y);
    core.y = 1.35f;

    DrawCube(core, 6.0f, 2.7f, 5.8f, { 138, 144, 156, 255 });
    DrawCubeWires(core, 6.0f, 2.7f, 5.8f, { 44, 48, 58, 255 });

    DrawCube({ core.x - 2.1f, 2.45f, core.z - 2.0f }, 1.0f, 4.6f, 1.0f, { 156, 160, 170, 255 });
    DrawCube({ core.x + 2.1f, 2.45f, core.z - 2.0f }, 1.0f, 4.6f, 1.0f, { 156, 160, 170, 255 });
    DrawCube({ core.x - 2.1f, 2.45f, core.z + 2.0f }, 1.0f, 4.6f, 1.0f, { 156, 160, 170, 255 });
    DrawCube({ core.x + 2.1f, 2.45f, core.z + 2.0f }, 1.0f, 4.6f, 1.0f, { 156, 160, 170, 255 });

    Vector3 gate = grid.CellCenter(fortress.gateCell.x, fortress.gateCell.y);
    gate.y = 0.90f;
    float gateRatio = (float)fortress.gateHp / (float)fortress.gateMaxHp;
    if (gateRatio < 0.0f) gateRatio = 0.0f;
    Color gateColor = (fortress.gateHp > 0) ? Color{ (unsigned char)(118 + 56 * gateRatio), (unsigned char)(94 + 40 * gateRatio), 72, 255 } : Color{ 72, 54, 44, 255 };
    DrawCube(gate, 1.35f, 1.8f, 3.8f, gateColor);
    DrawCubeWires(gate, 1.35f, 1.8f, 3.8f, { 42, 30, 22, 255 });

    float glowPulse = 0.06f * std::sin(worldTime * 3.0f);
    if (hymnTimer > 0.0f) glowPulse += 0.10f;
    DrawSphere({ core.x, 3.00f + glowPulse, core.z }, 0.46f + glowPulse, { 236, 202, 116, 255 });
    DrawSphere({ core.x, 3.00f + glowPulse * 1.5f, core.z }, 0.25f + glowPulse * 0.6f, { 255, 240, 178, 255 });
}

void Game::DrawTowers() const {
    for (const Tower& tower : towers) {
        float levelLift = 0.08f * (float)(tower.level - 1);

        if (tower.type == TowerType::CenserShrine) {
            DrawCube({ tower.pos.x, 0.42f, tower.pos.z }, 1.0f, 0.85f, 1.0f, { 108, 92, 80, 255 });
            DrawCylinder({ tower.pos.x, 1.15f, tower.pos.z }, 0.28f, 0.34f, 1.2f + levelLift, 8, { 166, 132, 88, 255 });
            DrawSphere({ tower.pos.x, 1.95f + levelLift, tower.pos.z }, 0.24f + 0.04f * (float)(tower.level - 1), { 244, 166, 84, 255 });
        }
        else if (tower.type == TowerType::ReliquarySpire) {
            DrawCube({ tower.pos.x, 0.45f, tower.pos.z }, 0.95f, 0.9f, 0.95f, { 86, 98, 122, 255 });
            DrawCube({ tower.pos.x, 1.35f + levelLift * 0.5f, tower.pos.z }, 0.45f, 1.5f + levelLift, 0.45f, { 108, 126, 160, 255 });
            DrawSphere({ tower.pos.x, 2.20f + levelLift, tower.pos.z }, 0.22f + 0.04f * (float)(tower.level - 1), { 122, 170, 236, 255 });
        }
        else {
            DrawCube({ tower.pos.x, 0.55f, tower.pos.z }, 1.0f, 1.1f, 1.0f, { 110, 92, 76, 255 });
            DrawCube({ tower.pos.x, 1.35f + levelLift * 0.3f, tower.pos.z }, 0.65f, 0.65f + levelLift, 0.65f, { 166, 146, 112, 255 });
            DrawCylinder({ tower.pos.x, 1.9f + levelLift, tower.pos.z }, 0.18f, 0.18f, 0.9f + levelLift, 8, { 194, 172, 118, 255 });
        }

        for (int i = 0; i < tower.level; ++i) {
            DrawCylinder({ tower.pos.x, 0.06f, tower.pos.z }, tower.range - 0.12f * (float)i, tower.range - 0.12f * (float)i, 0.02f, 18, Fade(tower.color, 0.05f));
        }
    }
}

void Game::DrawEnemies() const {
    for (const Enemy& enemy : enemies) {
        Color baseColor = (enemy.type == EnemyType::GraveBrute) ? Color{ 110, 88, 138, 255 } : Color{ 170, 82, 76, 255 };
        if (enemy.slowTimer > 0.0f) {
            baseColor = Tint(baseColor, 1.10f);
        }
        Color color = enemy.hitFlash > 0.0f ? WHITE : baseColor;
        float size = (enemy.type == EnemyType::GraveBrute) ? 1.10f : 0.82f;
        float height = (enemy.type == EnemyType::GraveBrute) ? 1.50f : 1.12f;
        DrawCube(enemy.pos, size, height, size, color);
        DrawCubeWires(enemy.pos, size, height, size, { 48, 20, 20, 255 });

        float hpRatio = (float)enemy.hp / (float)enemy.maxHp;
        if (hpRatio < 0.0f) hpRatio = 0.0f;
        DrawCube({ enemy.pos.x, enemy.pos.y + height * 0.72f, enemy.pos.z }, 0.96f, 0.08f, 0.14f, { 40, 10, 10, 255 });
        DrawCube({ enemy.pos.x - (0.96f * (1.0f - hpRatio)) * 0.5f, enemy.pos.y + height * 0.73f, enemy.pos.z }, 0.96f * hpRatio, 0.05f, 0.10f, { 96, 220, 96, 255 });
    }
}

void Game::DrawEffects() const {
    for (const ShotFx& shot : shots) {
        DrawLine3D(shot.start, shot.end, shot.color);
    }
}

void Game::DrawUi() const {
    DrawRectangle(16, 16, 520, 174, Fade(BLACK, 0.60f));
    DrawRectangleLines(16, 16, 520, 174, { 188, 156, 96, 255 });
    DrawText("THE LAST PROCESSION // BATCH 3", 28, 28, 24, { 232, 220, 198, 255 });
    DrawText(TextFormat("WAVE %d", wave.number), 28, 60, 20, { 188, 156, 96, 255 });
    DrawText(TextFormat("GOLD %d   IRON %d   EMBER %d", gold, iron, ember), 28, 86, 20, { 200, 210, 188, 255 });
    DrawText(TextFormat("GATE %d / %d", fortress.gateHp, fortress.gateMaxHp), 28, 112, 20, fortress.gateHp > 0 ? Color{ 188, 156, 96, 255 } : Color{ 188, 76, 76, 255 });
    DrawText(TextFormat("HOLY CORE %d / %d", fortress.coreHp, fortress.coreMaxHp), 220, 112, 20, fortress.coreHp > 30 ? Color{ 118, 184, 126, 255 } : Color{ 188, 76, 76, 255 });
    DrawText(TextFormat("BUILD: %s", BuildChoiceLabel(buildChoice)), 220, 60, 18, buildChoice == BuildChoice::WatchbowNest ? Color{ 194, 172, 118, 255 } : (buildChoice == BuildChoice::CenserShrine ? Color{ 244, 166, 84, 255 } : Color{ 122, 170, 236, 255 }));
    DrawText(TextFormat("FERVOR %d / %d", fervor, fervorMax), 220, 86, 18, hymnTimer > 0.0f ? Color{ 250, 228, 164, 255 } : Color{ 168, 190, 216, 255 });
    DrawText(TextFormat("ZOOM %.1f", cameraZoom), 400, 86, 18, { 196, 204, 210, 255 });

    DrawRectangle(16, screenH - 118, screenW - 32, 102, Fade(BLACK, 0.64f));
    DrawRectangleLines(16, screenH - 118, screenW - 32, 102, { 110, 126, 172, 255 });

    if (state == PlayState::BuildPhase) {
        DrawText("BUILD PHASE // 1 Watchbow  2 Censer  3 Reliquary Spire  U Upgrade Hovered Tower  H Repair Gate", 28, screenH - 102, 20, { 226, 218, 206, 255 });
        DrawText("Left Click build // Mouse Wheel zoom // WASD axis pan // SPACE begin siege", 28, screenH - 72, 20, { 190, 198, 188, 255 });
    }
    else if (state == PlayState::BattlePhase) {
        int raiders = 0;
        int brutes = 0;
        for (const Enemy& enemy : enemies) {
            if (enemy.type == EnemyType::GraveBrute) brutes++;
            else raiders++;
        }
        DrawText(TextFormat("BATTLE PHASE // Raiders %d   Brutes %d   Towers %d", raiders, brutes, (int)towers.size()), 28, screenH - 102, 20, { 226, 218, 206, 255 });
        DrawText("Fill Fervor by killing enemies // Press F at full Fervor to trigger the War Hymn", 28, screenH - 72, 20, hymnTimer > 0.0f ? Color{ 248, 224, 160, 255 } : Color{ 190, 198, 188, 255 });
    }
    else {
        DrawText("GAME OVER // Press ENTER to restart the procession", 28, screenH - 86, 20, { 226, 218, 206, 255 });
    }

    if (announcementTimer > 0.0f) {
        int width = MeasureText(announcement.c_str(), 26);
        DrawRectangle(screenW / 2 - width / 2 - 18, 18, width + 36, 40, Fade(BLACK, 0.72f));
        DrawRectangleLines(screenW / 2 - width / 2 - 18, 18, width + 36, 40, { 188, 156, 96, 255 });
        DrawText(announcement.c_str(), screenW / 2 - width / 2, 26, 26, { 188, 156, 96, 255 });
    }

    if (hoveredValid) {
        DrawText(TextFormat("CELL %d, %d", hoveredCell.x, hoveredCell.y), screenW - 188, 20, 20, { 200, 210, 188, 255 });
    }

    if (hoveredTowerIndex >= 0 && hoveredTowerIndex < (int)towers.size()) {
        const Tower& tower = towers[hoveredTowerIndex];
        int costGold = GetTowerUpgradeGoldCost(tower);
        int costEmber = GetTowerUpgradeEmberCost(tower);
        DrawRectangle(screenW - 292, 52, 270, 122, Fade(BLACK, 0.60f));
        DrawRectangleLines(screenW - 292, 52, 270, 122, tower.color);
        DrawText(TowerLabel(tower.type), screenW - 278, 64, 20, { 232, 220, 198, 255 });
        DrawText(TextFormat("LEVEL %d  DMG %d  RNG %.1f", tower.level, tower.damage, tower.range), screenW - 278, 92, 18, { 198, 208, 214, 255 });
        DrawText(TextFormat("RATE %.2f sec", tower.maxCooldown), screenW - 278, 116, 18, { 198, 208, 214, 255 });
        if (tower.level < 3) {
            DrawText(TextFormat("U UPGRADE // %d GOLD  %d EMBER", costGold, costEmber), screenW - 278, 140, 18, { 216, 194, 134, 255 });
        }
        else {
            DrawText("MAX CONSECRATION REACHED", screenW - 278, 140, 18, { 216, 194, 134, 255 });
        }
    }

    if (state == PlayState::GameOver) {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.50f));
        const char* title = "THE PROCESSION HAS FALLEN";
        int w = MeasureText(title, 42);
        DrawText(title, screenW / 2 - w / 2, screenH / 2 - 30, 42, { 196, 82, 82, 255 });
    }
}
