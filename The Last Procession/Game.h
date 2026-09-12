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
    ReliquarySpire,
    PilgrimBarricade
};

enum class PropType {
    DeadTree,
    GraveMarker,
    RubblePile,
    Brazier,
    BannerPole,
    CartWreck
};

enum class RoyalDirectiveType {
    HoldGate,
    HoldCore,
    SlayElites,
    SilenceHeralds,
    BreakBreakers,
    HuntHounds
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

struct DeathFx {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float life = 0.0f;
    float maxLife = 0.0f;
    float size = 0.18f;
    Color color = WHITE;
};

struct WaystoneSite {
    GridCoord cell{};
    bool consecrated = false;
};

struct BastionSite {
    GridCoord cell{};
    int laneIndex = 0;
    int level = 0;
    float cooldown = 0.0f;
};

struct RoyalDirective {
    RoyalDirectiveType type = RoyalDirectiveType::HoldGate;
    int target = 1;
    int progress = 0;
    int rewardGold = 0;
    int rewardIron = 0;
    int rewardEmber = 0;
    int rewardAsh = 0;
    bool failed = false;
    bool completed = false;
    std::string title = "HOLD THE GATE";
    std::string detail = "Let no enemy damage the front gate this wave.";
};

struct LegacyProfile {
    int ash = 0;
    int highestWave = 1;
    int runsStarted = 0;
    int totalWaystonesConsecrated = 0;
    int breakersSlain = 0;
    int directivesCompleted = 0;
    int flawlessWaves = 0;
    int rampartRank = 0;
    int arsenalRank = 0;
    int emberkeepRank = 0;
    int hymnRank = 0;
};

class Game {
public:
    Game();
    ~Game();
    void Run();

private:
    int screenW = 1600;
    int screenH = 900;

    Camera3D camera{};
    Vector3 cameraFocus = { 0.0f, 0.0f, 0.0f };
    float cameraZoom = 40.0f;
    float cameraMinZoom = 28.0f;
    float cameraMaxZoom = 60.0f;

    GridMap grid;
    Fortress fortress;
    std::vector<std::vector<GridCoord>> lanes;
    std::vector<Prop> props;
    std::vector<WaystoneSite> waystones;
    std::vector<BastionSite> bastions;
    std::vector<Tower> towers;
    std::vector<Enemy> enemies;
    std::vector<ShotFx> shots;
    std::vector<DeathFx> deathFx;
    WaveState wave;
    RoyalDirective directive;

    PlayState state = PlayState::BuildPhase;
    BuildChoice buildChoice = BuildChoice::WatchbowNest;
    int gold = 70;
    int iron = 40;
    int ember = 10;
    int fervor = 0;
    int fervorMax = 100;
    int waveGateDamageTaken = 0;
    int waveCoreDamageTaken = 0;
    float hymnTimer = 0.0f;
    float worldTime = 0.0f;
    float stormFlash = 0.0f;
    float sanctumPulseTimer = 6.0f;
    float sanctumPulseVisual = 0.0f;
    std::string waveOmen = "THREE ROADS BURN";
    int omenLane = -1;
    LegacyProfile legacy{};
    int legacyAshEarnedThisRun = 0;
    bool hasSuspendedChronicle = false;
    std::string announcement = "BUILD YOUR FIRST WATCHBOW NEST";
    float announcementTimer = 0.0f;

    GridCoord hoveredCell = { -1, -1 };
    bool hoveredValid = false;
    int hoveredTowerIndex = -1;

    void ResetRun(bool preserveSuspend = false);
    void BuildMap();
    void AddProp(PropType type, int x, int y, bool blockCell);
    void BuildWave(int waveNumber);
    void ConfigureRoyalDirective();
    void StartWave();
    void SpawnEnemy(EnemyType type, int laneIndex, bool elite);

    void Update(float dt);
    void UpdateAtmosphere(float dt);
    void UpdateCamera(float dt);
    void UpdateHoverCell();
    void UpdateBuildPhase();
    void UpdateBattlePhase(float dt);
    void UpdateEnemies(float dt);
    void UpdateTowers(float dt);
    void UpdateBastions(float dt);
    void UpdateShots(float dt);
    void UpdateDeathFx(float dt);

    void LoadLegacyProfile();
    void SaveLegacyProfile() const;
    bool HasSuspendedRun() const;
    void SaveSuspendedRun() const;
    bool LoadSuspendedRun();
    void AwardLegacyAsh(int amount);
    void TryBuyLegacyUpgrade(int slot);
    void TriggerCrownfireDecree();
    int GetLegacyUpgradeCost(int slot) const;
    int GetLegacyUpgradeRank(int slot) const;
    int GetLegacyUpgradeMaxRank(int slot) const;
    const char* GetLegacyUpgradeLabel(int slot) const;
    void TriggerSanctumPulse();

    void TryPlaceTower();
    void TryUpgradeTower();
    void TrySellTower();
    void TryConsecrateWaystone();
    void TryFortifyBastion();
    void DamageGate(int amount);
    void DamageCore(int amount);
    void GainFervor(int amount);
    void TriggerWarHymn();
    void RegisterEnemyKill(const Enemy& enemy);
    bool RayToGround(Vector3* outPoint) const;
    int FindTowerIndexAtCell(int x, int y) const;
    int FindWaystoneIndexAtCell(int x, int y) const;
    int FindBastionIndexAtCell(int x, int y) const;
    int GetTowerBastionWardLevel(const Tower& tower) const;
    bool IsTowerBlessed(const Tower& tower) const;
    Tower MakeTower(BuildChoice choice, int cellX, int cellY) const;
    void ApplyTowerStats(Tower& tower) const;
    const char* BuildChoiceLabel(BuildChoice choice) const;
    const char* TowerLabel(TowerType type) const;
    const char* EnemyLabel(EnemyType type) const;
    const char* GetBastionLabel(int laneIndex) const;
    int GetTowerUpgradeGoldCost(const Tower& tower) const;
    int GetTowerUpgradeIronCost(const Tower& tower) const;
    int GetTowerUpgradeEmberCost(const Tower& tower) const;
    int GetTowerSellGoldRefund(const Tower& tower) const;
    int GetTowerSellIronRefund(const Tower& tower) const;
    int GetTowerSellEmberRefund(const Tower& tower) const;
    int GetConsecratedWaystoneCount() const;
    float GetWarHymnDuration() const;
    int GetBastionUpgradeGoldCost(const BastionSite& bastion) const;
    int GetBastionUpgradeIronCost(const BastionSite& bastion) const;
    int GetBastionUpgradeEmberCost(const BastionSite& bastion) const;
    std::string GetDirectiveProgressText() const;

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
