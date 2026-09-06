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

enum class BuildChoice {
    WatchbowNest,
    CenserShrine,
    ReliquarySpire
};

enum class PropType {
    DeadTree,
    GraveMarker,
    RubblePile,
    Brazier,
    BannerPole,
    CartWreck
};

struct Prop {
    PropType type = PropType::DeadTree;
    GridCoord cell{};
};

struct ShotFx {
    Vector3 start = { 0.0f, 0.0f, 0.0f };
    Vector3 end = { 0.0f, 0.0f, 0.0f };
    float life = 0.0f;
    Color color = WHITE;
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
    float cameraZoom = 22.0f;
    float cameraMinZoom = 14.0f;
    float cameraMaxZoom = 30.0f;

    GridMap grid;
    Fortress fortress;
    std::vector<std::vector<GridCoord>> lanes;
    std::vector<Prop> props;
    std::vector<Tower> towers;
    std::vector<Enemy> enemies;
    std::vector<ShotFx> shots;
    WaveState wave;

    PlayState state = PlayState::BuildPhase;
    BuildChoice buildChoice = BuildChoice::WatchbowNest;
    int gold = 70;
    int iron = 40;
    int ember = 10;
    int fervor = 0;
    int fervorMax = 100;
    float hymnTimer = 0.0f;
    float worldTime = 0.0f;
    std::string announcement = "BUILD YOUR FIRST WATCHBOW NEST";
    float announcementTimer = 0.0f;

    GridCoord hoveredCell = { -1, -1 };
    bool hoveredValid = false;
    int hoveredTowerIndex = -1;

    void ResetRun();
    void BuildMap();
    void AddProp(PropType type, int x, int y, bool blockCell);
    void BuildWave(int waveNumber);
    void StartWave();
    void SpawnEnemy(EnemyType type, int laneIndex);

    void Update(float dt);
    void UpdateCamera(float dt);
    void UpdateHoverCell();
    void UpdateBuildPhase();
    void UpdateBattlePhase(float dt);
    void UpdateEnemies(float dt);
    void UpdateTowers(float dt);
    void UpdateShots(float dt);

    void TryPlaceTower();
    void TryUpgradeTower();
    void DamageGate(int amount);
    void DamageCore(int amount);
    void GainFervor(int amount);
    void TriggerWarHymn();
    bool RayToGround(Vector3* outPoint) const;
    int FindTowerIndexAtCell(int x, int y) const;
    Tower MakeTower(BuildChoice choice, int cellX, int cellY) const;
    void ApplyTowerStats(Tower& tower) const;
    const char* BuildChoiceLabel(BuildChoice choice) const;
    const char* TowerLabel(TowerType type) const;
    int GetTowerUpgradeGoldCost(const Tower& tower) const;
    int GetTowerUpgradeEmberCost(const Tower& tower) const;

    void Draw() const;
    void DrawWorld() const;
    void DrawTiles() const;
    void DrawEnvironment() const;
    void DrawFortress() const;
    void DrawTowers() const;
    void DrawEnemies() const;
    void DrawEffects() const;
    void DrawUi() const;
};
