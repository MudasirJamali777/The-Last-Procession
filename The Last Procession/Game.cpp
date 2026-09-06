#include "Game.h"
#include "Utils.h"
#include <algorithm>
#include <cmath>

Game::Game() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenW, screenH, "THE LAST PROCESSION");
    SetTargetFPS(60);

    camera.position = { 20.0f, 20.0f, 20.0f };
    camera.target = { 0.0f, 0.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 40.0f;
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
    gold = 70;
    iron = 40;
    ember = 10;
    towers.clear();
    enemies.clear();
    shots.clear();
    fortress = Fortress{};
    state = PlayState::BuildPhase;
    announcement = "BUILD YOUR FIRST WATCHBOW NEST";
    announcementTimer = 3.0f;
    hoveredValid = false;
    hoveredCell = { -1, -1 };
    BuildMap();
    BuildWave(1);
}

void Game::BuildMap() {
    grid.width = 12;
    grid.height = 10;
    grid.cellSize = 2.0f;
    grid.origin = { -12.0f, 0.0f, -10.0f };
    grid.tiles.assign((size_t)grid.width * (size_t)grid.height, GridTile{});

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            grid.At(x, y).kind = TileKind::Buildable;
            grid.At(x, y).occupied = false;
        }
    }

    path.clear();
    path.push_back({ 0, 5 });
    path.push_back({ 1, 5 });
    path.push_back({ 2, 5 });
    path.push_back({ 3, 5 });
    path.push_back({ 4, 5 });
    path.push_back({ 5, 5 });
    path.push_back({ 6, 5 });
    path.push_back({ 7, 5 });
    path.push_back({ 8, 5 });

    for (size_t i = 0; i < path.size(); ++i) {
        TileKind kind = (i == 0) ? TileKind::Spawn : (i + 1 == path.size() ? TileKind::Fortress : TileKind::Road);
        grid.At(path[i].x, path[i].y).kind = kind;
    }

    const GridCoord fortressCells[] = {
        { 8, 4 }, { 9, 4 }, { 8, 5 }, { 9, 5 }, { 8, 6 }, { 9, 6 }
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

    int count = 5 + (waveNumber - 1) * 2;
    for (int i = 0; i < count; ++i) {
        SpawnEntry entry{};
        entry.spawnTime = 0.85f * i;
        entry.type = EnemyType::AshRaider;
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
    announcement = TextFormat("WAVE %d // HOLD THE PROCESSION", wave.number);
    announcementTimer = 2.5f;
}

void Game::SpawnEnemy(EnemyType type) {
    Enemy enemy{};
    enemy.type = type;
    enemy.pathIndex = 0;
    enemy.hp = 28 + (wave.number - 1) * 7;
    enemy.maxHp = enemy.hp;
    enemy.speed = 1.65f + (wave.number - 1) * 0.14f;
    Vector3 start = grid.CellCenter(path.front().x, path.front().y);
    enemy.pos = { start.x - 1.5f, 0.55f, start.z };
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
    float move = 10.0f * dt;
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
    camera.position = { cameraFocus.x + 20.0f, 20.0f, cameraFocus.z + 20.0f };
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
            SpawnEnemy(wave.spawns[wave.nextSpawnIndex].type);
            wave.nextSpawnIndex++;
        }
    }

    UpdateEnemies(dt);
    UpdateTowers(dt);

    if (wave.nextSpawnIndex >= (int)wave.spawns.size() && enemies.empty()) {
        wave.active = false;
        state = PlayState::BuildPhase;
        gold += 30 + wave.number * 8;
        iron += 10 + wave.number * 3;
        ember += 4 + wave.number;
        wave.number++;
        BuildWave(wave.number);
        announcement = "SIEGE BROKEN // REINFORCE THE FORTRESS";
        announcementTimer = 3.0f;
    }
}

void Game::UpdateEnemies(float dt) {
    for (Enemy& enemy : enemies) {
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);

        if (enemy.pathIndex >= (int)path.size()) {
            continue;
        }

        Vector3 target = grid.CellCenter(path[enemy.pathIndex].x, path[enemy.pathIndex].y);
        target.y = enemy.pos.y;
        Vector3 toTarget = Vec3Sub(target, enemy.pos);
        float dist = LengthXZ(toTarget);

        if (dist < 0.15f) {
            enemy.pathIndex++;
            if (enemy.pathIndex >= (int)path.size()) {
                DamageCore(10);
            }
            continue;
        }

        Vector3 dir = NormalizeXZ(toTarget);
        enemy.pos = Vec3Add(enemy.pos, Vec3Scale(dir, enemy.speed * dt));
    }

    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& enemy) {
        return enemy.hp <= 0 || enemy.pathIndex >= 9999;
        }), enemies.end());
}

void Game::UpdateTowers(float dt) {
    for (Tower& tower : towers) {
        tower.cooldown -= dt;
        if (tower.cooldown > 0.0f) {
            continue;
        }

        int bestIndex = -1;
        float bestDist = tower.range;
        for (int i = 0; i < (int)enemies.size(); ++i) {
            float dist = DistanceXZ(tower.pos, enemies[i].pos);
            if (dist <= bestDist) {
                bestDist = dist;
                bestIndex = i;
            }
        }

        if (bestIndex >= 0) {
            enemies[bestIndex].hp -= tower.damage;
            enemies[bestIndex].hitFlash = 0.12f;
            tower.cooldown = tower.maxCooldown;

            ShotFx fx{};
            fx.start = { tower.pos.x, tower.pos.y + 1.2f, tower.pos.z };
            fx.end = { enemies[bestIndex].pos.x, enemies[bestIndex].pos.y + 0.2f, enemies[bestIndex].pos.z };
            fx.life = 0.08f;
            shots.push_back(fx);

            if (enemies[bestIndex].hp <= 0) {
                gold += 4;
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
    if (!hoveredValid) {
        return;
    }

    if (!grid.InBounds(hoveredCell.x, hoveredCell.y)) {
        return;
    }

    GridTile& tile = grid.At(hoveredCell.x, hoveredCell.y);
    if (tile.kind != TileKind::Buildable || tile.occupied || CellHasTower(hoveredCell.x, hoveredCell.y)) {
        announcement = "CANNOT BUILD THERE";
        announcementTimer = 1.2f;
        return;
    }

    const int towerCost = 25;
    if (gold < towerCost) {
        announcement = "NOT ENOUGH GOLD";
        announcementTimer = 1.2f;
        return;
    }

    Tower tower{};
    tower.gridX = hoveredCell.x;
    tower.gridY = hoveredCell.y;
    tower.pos = grid.CellCenter(hoveredCell.x, hoveredCell.y);
    tower.pos.y = 0.8f;
    towers.push_back(tower);

    tile.occupied = true;
    gold -= towerCost;
    announcement = "WATCHBOW NEST RAISED";
    announcementTimer = 1.3f;
}

void Game::DamageCore(int amount) {
    fortress.coreHp -= amount;
    if (fortress.coreHp < 0) {
        fortress.coreHp = 0;
    }

    for (Enemy& enemy : enemies) {
        if (enemy.pathIndex >= (int)path.size()) {
            enemy.pathIndex = 9999;
        }
    }

    announcement = TextFormat("HOLY CORE STRUCK // %d HP", fortress.coreHp);
    announcementTimer = 1.5f;

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

    DrawPlane({ 0.0f, -0.01f, 0.0f }, { 60.0f, 60.0f }, { 48, 58, 46, 255 });
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

            Color tileColor = { 92, 106, 88, 255 };
            if (tile.kind == TileKind::Road) tileColor = { 130, 116, 84, 255 };
            else if (tile.kind == TileKind::Spawn) tileColor = { 138, 88, 72, 255 };
            else if (tile.kind == TileKind::Fortress) tileColor = { 116, 126, 138, 255 };

            if (hoveredValid && state == PlayState::BuildPhase && hoveredCell.x == x && hoveredCell.y == y) {
                if (tile.kind == TileKind::Buildable && !tile.occupied) tileColor = { 116, 156, 116, 255 };
                else tileColor = { 166, 86, 86, 255 };
            }

            DrawCube(center, grid.cellSize * 0.95f, 0.10f, grid.cellSize * 0.95f, tileColor);
            DrawCubeWires(center, grid.cellSize * 0.95f, 0.10f, grid.cellSize * 0.95f, { 28, 34, 26, 255 });
        }
    }
}

void Game::DrawFortress() const {
    Vector3 a = grid.CellCenter(8, 5);
    Vector3 b = grid.CellCenter(9, 5);
    Vector3 core = { (a.x + b.x) * 0.5f, 1.1f, (a.z + b.z) * 0.5f };

    DrawCube(core, 3.4f, 2.2f, 3.8f, { 138, 144, 156, 255 });
    DrawCubeWires(core, 3.4f, 2.2f, 3.8f, { 44, 48, 58, 255 });

    DrawCube({ core.x - 1.4f, 1.8f, core.z - 1.2f }, 0.9f, 3.6f, 0.9f, { 156, 160, 170, 255 });
    DrawCube({ core.x + 1.4f, 1.8f, core.z - 1.2f }, 0.9f, 3.6f, 0.9f, { 156, 160, 170, 255 });
    DrawCube({ core.x, 1.5f, core.z + 1.3f }, 1.2f, 2.8f, 0.9f, { 118, 94, 72, 255 });
    DrawSphere({ core.x, 2.5f, core.z }, 0.42f, { 236, 202, 116, 255 });
}

void Game::DrawTowers() const {
    for (const Tower& tower : towers) {
        DrawCube({ tower.pos.x, 0.55f, tower.pos.z }, 1.0f, 1.1f, 1.0f, { 110, 92, 76, 255 });
        DrawCube({ tower.pos.x, 1.35f, tower.pos.z }, 0.65f, 0.65f, 0.65f, { 166, 146, 112, 255 });
        DrawCylinder({ tower.pos.x, 1.9f, tower.pos.z }, 0.18f, 0.18f, 0.9f, 8, { 194, 172, 118, 255 });

        if (state == PlayState::BuildPhase) {
            DrawCircle3D({ tower.pos.x, 0.03f, tower.pos.z }, tower.range, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade({ 188, 156, 96, 255 }, 0.12f));
        }
    }
}

void Game::DrawEnemies() const {
    for (const Enemy& enemy : enemies) {
        Color color = enemy.hitFlash > 0.0f ? WHITE : Color{ 170, 82, 76, 255 };
        DrawCube(enemy.pos, 0.8f, 1.1f, 0.8f, color);
        DrawCubeWires(enemy.pos, 0.8f, 1.1f, 0.8f, { 48, 20, 20, 255 });

        float hpRatio = (float)enemy.hp / (float)enemy.maxHp;
        if (hpRatio < 0.0f) hpRatio = 0.0f;
        DrawCube({ enemy.pos.x, 1.35f, enemy.pos.z }, 0.9f, 0.08f, 0.14f, { 40, 10, 10, 255 });
        DrawCube({ enemy.pos.x - (0.9f * (1.0f - hpRatio)) * 0.5f, 1.36f, enemy.pos.z }, 0.9f * hpRatio, 0.05f, 0.10f, { 96, 220, 96, 255 });
    }
}

void Game::DrawEffects() const {
    for (const ShotFx& shot : shots) {
        DrawLine3D(shot.start, shot.end, { 252, 228, 160, 255 });
    }
}

void Game::DrawUi() const {
    DrawRectangle(16, 16, 360, 126, Fade(BLACK, 0.55f));
    DrawRectangleLines(16, 16, 360, 126, { 188, 156, 96, 255 });
    DrawText("THE LAST PROCESSION // BATCH 1", 28, 28, 24, { 232, 220, 198, 255 });
    DrawText(TextFormat("WAVE %d", wave.number), 28, 60, 20, { 188, 156, 96, 255 });
    DrawText(TextFormat("GOLD %d   IRON %d   EMBER %d", gold, iron, ember), 28, 86, 20, { 200, 210, 188, 255 });
    DrawText(TextFormat("HOLY CORE %d / %d", fortress.coreHp, fortress.coreMaxHp), 28, 112, 20, fortress.coreHp > 30 ? Color{ 118, 184, 126, 255 } : Color{ 188, 76, 76, 255 });

    DrawRectangle(16, screenH - 84, screenW - 32, 68, Fade(BLACK, 0.60f));
    DrawRectangleLines(16, screenH - 84, screenW - 32, 68, { 110, 126, 172, 255 });

    if (state == PlayState::BuildPhase) {
        DrawText("BUILD PHASE // Left Click place Watchbow Nest (25 Gold) // SPACE start siege // WASD pan camera", 28, screenH - 68, 20, { 226, 218, 206, 255 });
    }
    else if (state == PlayState::BattlePhase) {
        DrawText(TextFormat("BATTLE PHASE // Enemies %d // Towers %d // Hold the line", (int)enemies.size(), (int)towers.size()), 28, screenH - 68, 20, { 226, 218, 206, 255 });
    }
    else {
        DrawText("GAME OVER // Press ENTER to restart the procession", 28, screenH - 68, 20, { 226, 218, 206, 255 });
    }

    if (announcementTimer > 0.0f) {
        int width = MeasureText(announcement.c_str(), 28);
        DrawRectangle(screenW / 2 - width / 2 - 18, 18, width + 36, 42, Fade(BLACK, 0.68f));
        DrawRectangleLines(screenW / 2 - width / 2 - 18, 18, width + 36, 42, { 188, 156, 96, 255 });
        DrawText(announcement.c_str(), screenW / 2 - width / 2, 27, 28, { 188, 156, 96, 255 });
    }

    if (hoveredValid) {
        DrawText(TextFormat("CELL %d, %d", hoveredCell.x, hoveredCell.y), screenW - 170, 20, 20, { 200, 210, 188, 255 });
    }

    if (state == PlayState::GameOver) {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.45f));
        const char* title = "THE PROCESSION HAS FALLEN";
        int w = MeasureText(title, 42);
        DrawText(title, screenW / 2 - w / 2, screenH / 2 - 26, 42, { 196, 82, 82, 255 });
    }
}
