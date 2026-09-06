#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include "Grid.h"
#include "Enemy.h"
#include "Fortress.h"
#include "Tower.h"
#include "Wave.h"

enum class PlayState {
    BuildPhase,
    BattlePhase,
    GameOver
};

struct ShotFx {
    Vector3 start = { 0.0f, 0.0f, 0.0f };
    Vector3 end = { 0.0f, 0.0f, 0.0f };
    float life = 0.0f;
};

class Game {
public:
    Game();
    ~Game();
    void Run();

private:
    int screenW = 1280;
    int screenH = 720;

    Camera3D camera{};
    Vector3 cameraFocus = { 0.0f, 0.0f, 0.0f };

    GridMap grid;
    Fortress fortress;
    std::vector<GridCoord> path;
    std::vector<Tower> towers;
    std::vector<Enemy> enemies;
    std::vector<ShotFx> shots;
    WaveState wave;

    PlayState state = PlayState::BuildPhase;
    int gold = 70;
    int iron = 40;
    int ember = 10;
    std::string announcement = "BUILD YOUR FIRST WATCHBOW NEST";
    float announcementTimer = 0.0f;

    GridCoord hoveredCell = { -1, -1 };
    bool hoveredValid = false;

    void ResetRun();
    void BuildMap();
    void BuildWave(int waveNumber);
    void StartWave();
    void SpawnEnemy(EnemyType type);

    void Update(float dt);
    void UpdateCamera(float dt);
    void UpdateHoverCell();
    void UpdateBuildPhase();
    void UpdateBattlePhase(float dt);
    void UpdateEnemies(float dt);
    void UpdateTowers(float dt);
    void UpdateShots(float dt);

    void TryPlaceTower();
    void DamageCore(int amount);
    bool RayToGround(Vector3* outPoint) const;
    bool CellHasTower(int x, int y) const;

    void Draw() const;
    void DrawWorld() const;
    void DrawTiles() const;
    void DrawFortress() const;
    void DrawTowers() const;
    void DrawEnemies() const;
    void DrawEffects() const;
    void DrawUi() const;
};
