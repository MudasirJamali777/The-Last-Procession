#include "Game.h"
#include "Utils.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace {
    const char* kProfileSavePath = "the_last_procession_profile.txt";
    const char* kSuspendSavePath = "the_last_procession_suspend.txt";
    const char* kSuspendSaveHeader = "TLP_RUN_V2";
}

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

    camera.position = { 52.0f, 42.0f, 52.0f };
    camera.target = { 0.0f, 0.45f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 36.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    LoadLegacyProfile();
    hasSuspendedChronicle = HasSuspendedRun();
    ResetRun(hasSuspendedChronicle);
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

void Game::ResetRun(bool preserveSuspend) {
    gold = 140 + legacy.arsenalRank * 4;
    iron = 95 + legacy.rampartRank * 6;
    ember = 28 + legacy.emberkeepRank * 5;
    fervor = 0;
    waveGateDamageTaken = 0;
    waveCoreDamageTaken = 0;
    hymnTimer = 0.0f;
    worldTime = 0.0f;
    stormFlash = 0.0f;
    sanctumPulseTimer = 6.0f;
    sanctumPulseVisual = 0.0f;
    waveOmen = "THREE ROADS BURN";
    omenLane = -1;
    legacyAshEarnedThisRun = 0;
    towers.clear();
    enemies.clear();
    shots.clear();
    deathFx.clear();
    fortress = Fortress{};
    directive = RoyalDirective{};
    state = PlayState::BuildPhase;
    buildChoice = BuildChoice::WatchbowNest;
    announcement = preserveSuspend
        ? "BATCH 16 // GRAND SHRINE MAP, PRESS L TO RESTORE YOUR CHRONICLE"
        : "BATCH 16 // GRAND SHRINE MAP AND BORDERLANDS";
    announcementTimer = 5.2f;
    hoveredValid = false;
    hoveredCell = { -1, -1 };
    hoveredTowerIndex = -1;
    cameraZoom = 48.0f;
    BuildMap();
    fortress.gateMaxHp += legacy.rampartRank * 22;
    fortress.gateHp = fortress.gateMaxHp;
    BuildWave(1);
    if (!preserveSuspend) {
        legacy.runsStarted++;
    }
    SaveLegacyProfile();
    if (!preserveSuspend) {
        SaveSuspendedRun();
        hasSuspendedChronicle = true;
    }
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

void Game::LoadLegacyProfile() {
    legacy = LegacyProfile{};

    std::ifstream in(kProfileSavePath);
    if (!in) return;

    std::string header;
    in >> header;
    if (header != "TLP_PROFILE_V1") return;

    std::string key;
    while (in >> key) {
        int value = 0;
        in >> value;
        if (key == "ash") legacy.ash = value;
        else if (key == "highestWave") legacy.highestWave = value;
        else if (key == "runsStarted") legacy.runsStarted = value;
        else if (key == "waystones") legacy.totalWaystonesConsecrated = value;
        else if (key == "breakers") legacy.breakersSlain = value;
        else if (key == "directives") legacy.directivesCompleted = value;
        else if (key == "flawless") legacy.flawlessWaves = value;
        else if (key == "rampart") legacy.rampartRank = value;
        else if (key == "arsenal") legacy.arsenalRank = value;
        else if (key == "emberkeep") legacy.emberkeepRank = value;
        else if (key == "hymn") legacy.hymnRank = value;
    }

    legacy.ash = std::max(0, legacy.ash);
    legacy.highestWave = std::max(1, legacy.highestWave);
    legacy.runsStarted = std::max(0, legacy.runsStarted);
    legacy.totalWaystonesConsecrated = std::max(0, legacy.totalWaystonesConsecrated);
    legacy.breakersSlain = std::max(0, legacy.breakersSlain);
    legacy.directivesCompleted = std::max(0, legacy.directivesCompleted);
    legacy.flawlessWaves = std::max(0, legacy.flawlessWaves);
    legacy.rampartRank = std::max(0, std::min(legacy.rampartRank, GetLegacyUpgradeMaxRank(0)));
    legacy.arsenalRank = std::max(0, std::min(legacy.arsenalRank, GetLegacyUpgradeMaxRank(1)));
    legacy.emberkeepRank = std::max(0, std::min(legacy.emberkeepRank, GetLegacyUpgradeMaxRank(2)));
    legacy.hymnRank = std::max(0, std::min(legacy.hymnRank, GetLegacyUpgradeMaxRank(3)));
}

void Game::SaveLegacyProfile() const {
    std::ofstream out(kProfileSavePath, std::ios::trunc);
    if (!out) return;

    out << "TLP_PROFILE_V1\n";
    out << "ash " << legacy.ash << "\n";
    out << "highestWave " << legacy.highestWave << "\n";
    out << "runsStarted " << legacy.runsStarted << "\n";
    out << "waystones " << legacy.totalWaystonesConsecrated << "\n";
    out << "breakers " << legacy.breakersSlain << "\n";
    out << "directives " << legacy.directivesCompleted << "\n";
    out << "flawless " << legacy.flawlessWaves << "\n";
    out << "rampart " << legacy.rampartRank << "\n";
    out << "arsenal " << legacy.arsenalRank << "\n";
    out << "emberkeep " << legacy.emberkeepRank << "\n";
    out << "hymn " << legacy.hymnRank << "\n";
}

bool Game::HasSuspendedRun() const {
    std::ifstream in(kSuspendSavePath);
    if (!in) return false;

    std::string header;
    in >> header;
    return header == kSuspendSaveHeader;
}

void Game::SaveSuspendedRun() const {
    std::ofstream out(kSuspendSavePath, std::ios::trunc);
    if (!out) return;

    out << kSuspendSaveHeader << "\n";
    out << "wave " << wave.number << "\n";
    out << "gold " << gold << "\n";
    out << "iron " << iron << "\n";
    out << "ember " << ember << "\n";
    out << "fervor " << fervor << "\n";
    out << "buildChoice " << (int)buildChoice << "\n";
    out << "gateHp " << fortress.gateHp << "\n";
    out << "gateMax " << fortress.gateMaxHp << "\n";
    out << "coreHp " << fortress.coreHp << "\n";
    out << "coreMax " << fortress.coreMaxHp << "\n";
    out << "cameraZoom " << cameraZoom << "\n";
    out << "runAsh " << legacyAshEarnedThisRun << "\n";
    out << "waystoneCount " << waystones.size() << "\n";
    for (const WaystoneSite& stone : waystones) {
        out << stone.cell.x << ' ' << stone.cell.y << ' ' << (stone.consecrated ? 1 : 0) << "\n";
    }
    out << "bastionCount " << bastions.size() << "\n";
    for (const BastionSite& bastion : bastions) {
        out << bastion.cell.x << ' ' << bastion.cell.y << ' ' << bastion.laneIndex << ' ' << bastion.level << ' ' << bastion.cooldown << "\n";
    }
    out << "towerCount " << towers.size() << "\n";
    for (const Tower& tower : towers) {
        out << (int)tower.type << ' ' << tower.gridX << ' ' << tower.gridY << ' ' << tower.level << ' ' << tower.cooldown << "\n";
    }
}

bool Game::LoadSuspendedRun() {
    std::ifstream in(kSuspendSavePath);
    if (!in) return false;

    std::string header;
    in >> header;
    if (header != kSuspendSaveHeader) return false;

    struct SavedStone { int x = 0; int y = 0; int consecrated = 0; };
    struct SavedBastion { int x = 0; int y = 0; int laneIndex = 0; int level = 0; float cooldown = 0.0f; };
    struct SavedTower { int type = 0; int x = 0; int y = 0; int level = 1; float cooldown = 0.0f; };

    int savedWave = 1;
    int savedGold = 140;
    int savedIron = 95;
    int savedEmber = 28;
    int savedFervor = 0;
    int savedBuildChoice = 0;
    int savedGateHp = 95;
    int savedGateMax = 95;
    int savedCoreHp = 120;
    int savedCoreMax = 120;
    float savedZoom = 40.0f;
    int savedRunAsh = 0;
    std::vector<SavedStone> stoneData;
    std::vector<SavedBastion> bastionData;
    std::vector<SavedTower> towerData;

    std::string key;
    while (in >> key) {
        if (key == "wave") in >> savedWave;
        else if (key == "gold") in >> savedGold;
        else if (key == "iron") in >> savedIron;
        else if (key == "ember") in >> savedEmber;
        else if (key == "fervor") in >> savedFervor;
        else if (key == "buildChoice") in >> savedBuildChoice;
        else if (key == "gateHp") in >> savedGateHp;
        else if (key == "gateMax") in >> savedGateMax;
        else if (key == "coreHp") in >> savedCoreHp;
        else if (key == "coreMax") in >> savedCoreMax;
        else if (key == "cameraZoom") in >> savedZoom;
        else if (key == "runAsh") in >> savedRunAsh;
        else if (key == "waystoneCount") {
            int count = 0;
            in >> count;
            stoneData.clear();
            for (int i = 0; i < count; ++i) {
                SavedStone stone{};
                in >> stone.x >> stone.y >> stone.consecrated;
                stoneData.push_back(stone);
            }
        }
        else if (key == "bastionCount") {
            int count = 0;
            in >> count;
            bastionData.clear();
            for (int i = 0; i < count; ++i) {
                SavedBastion bastion{};
                in >> bastion.x >> bastion.y >> bastion.laneIndex >> bastion.level >> bastion.cooldown;
                bastionData.push_back(bastion);
            }
        }
        else if (key == "towerCount") {
            int count = 0;
            in >> count;
            towerData.clear();
            for (int i = 0; i < count; ++i) {
                SavedTower tower{};
                in >> tower.type >> tower.x >> tower.y >> tower.level >> tower.cooldown;
                towerData.push_back(tower);
            }
        }
    }

    gold = std::max(0, savedGold);
    iron = std::max(0, savedIron);
    ember = std::max(0, savedEmber);
    fervor = std::max(0, std::min(savedFervor, fervorMax));
    hymnTimer = 0.0f;
    worldTime = 0.0f;
    stormFlash = 0.0f;
    sanctumPulseTimer = 6.0f;
    sanctumPulseVisual = 0.0f;
    legacyAshEarnedThisRun = std::max(0, savedRunAsh);
    towers.clear();
    enemies.clear();
    shots.clear();
    deathFx.clear();
    fortress = Fortress{};
    state = PlayState::BuildPhase;
    buildChoice = (BuildChoice)std::max(0, std::min(savedBuildChoice, 3));
    hoveredValid = false;
    hoveredCell = { -1, -1 };
    hoveredTowerIndex = -1;
    cameraZoom = ClampFloat(savedZoom, cameraMinZoom, cameraMaxZoom);
    BuildMap();

    fortress.gateMaxHp = std::max(1, savedGateMax);
    fortress.gateHp = std::max(0, std::min(savedGateHp, fortress.gateMaxHp));
    fortress.coreMaxHp = std::max(1, savedCoreMax);
    fortress.coreHp = std::max(0, std::min(savedCoreHp, fortress.coreMaxHp));

    for (const SavedStone& stone : stoneData) {
        int index = FindWaystoneIndexAtCell(stone.x, stone.y);
        if (index >= 0) {
            waystones[index].consecrated = (stone.consecrated != 0);
        }
    }

    for (const SavedBastion& saved : bastionData) {
        int index = FindBastionIndexAtCell(saved.x, saved.y);
        if (index >= 0) {
            bastions[index].level = std::max(0, std::min(saved.level, 3));
            bastions[index].cooldown = ClampFloat(saved.cooldown, 0.0f, 8.0f);
        }
    }

    for (const SavedTower& saved : towerData) {
        if (!grid.InBounds(saved.x, saved.y)) continue;
        if (FindWaystoneIndexAtCell(saved.x, saved.y) >= 0) continue;
        if (FindBastionIndexAtCell(saved.x, saved.y) >= 0) continue;
        GridTile& tile = grid.At(saved.x, saved.y);
        if (tile.kind != TileKind::Buildable || tile.occupied) continue;

        Tower tower{};
        tower.type = (TowerType)std::max(0, std::min(saved.type, 3));
        tower.gridX = saved.x;
        tower.gridY = saved.y;
        tower.level = std::max(1, std::min(saved.level, 3));
        tower.pos = grid.CellCenter(saved.x, saved.y);
        tower.pos.y = 1.0f;
        ApplyTowerStats(tower);
        tower.cooldown = ClampFloat(saved.cooldown, 0.0f, tower.maxCooldown);
        towers.push_back(tower);
        tile.occupied = true;
    }

    BuildWave(std::max(1, savedWave));
    announcement = "CHRONICLE RESTORED // THE STORM IS HELD AT THE GATE";
    announcementTimer = 3.4f;
    hasSuspendedChronicle = true;
    return true;
}

void Game::AwardLegacyAsh(int amount) {
    if (amount <= 0) return;
    legacy.ash += amount;
    legacyAshEarnedThisRun += amount;
    SaveLegacyProfile();
}

void Game::TryBuyLegacyUpgrade(int slot) {
    if (state != PlayState::BuildPhase) return;

    int rank = GetLegacyUpgradeRank(slot);
    int maxRank = GetLegacyUpgradeMaxRank(slot);
    if (rank >= maxRank) {
        announcement = "LEGACY DOCTRINE AT MAX RANK";
        announcementTimer = 1.0f;
        return;
    }

    int cost = GetLegacyUpgradeCost(slot);
    if (legacy.ash < cost) {
        announcement = "NOT ENOUGH LEGACY ASH";
        announcementTimer = 1.0f;
        return;
    }

    legacy.ash -= cost;
    if (slot == 0) legacy.rampartRank++;
    else if (slot == 1) legacy.arsenalRank++;
    else if (slot == 2) legacy.emberkeepRank++;
    else legacy.hymnRank++;

    if (slot == 0) {
        fortress.gateMaxHp += 22;
        fortress.gateHp += 22;
        if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
        iron += 6;
    }
    else if (slot == 1) {
        for (Tower& tower : towers) {
            float cooldownRatio = (tower.maxCooldown > 0.0f) ? (tower.cooldown / tower.maxCooldown) : 0.0f;
            ApplyTowerStats(tower);
            tower.cooldown = tower.maxCooldown * cooldownRatio;
        }
    }
    else if (slot == 2) {
        ember += 6;
    }
    else {
        GainFervor(12);
    }

    SaveLegacyProfile();
    SaveSuspendedRun();
    hasSuspendedChronicle = true;
    announcement = TextFormat("%s AWAKENED // RANK %d", GetLegacyUpgradeLabel(slot), GetLegacyUpgradeRank(slot));
    announcementTimer = 1.6f;
}

void Game::TriggerCrownfireDecree() {
    if (state != PlayState::BattlePhase) return;

    int raisedBastions = 0;
    for (const BastionSite& bastion : bastions) {
        if (bastion.level > 0) raisedBastions++;
    }

    int consecrated = GetConsecratedWaystoneCount();
    int cost = 48 - legacy.hymnRank * 4 - consecrated * 2;
    if (cost < 28) cost = 28;

    if (raisedBastions <= 0 && consecrated <= 0) {
        announcement = "THE FORTRESS HAS NO CROWNFIRE TO COMMAND";
        announcementTimer = 1.0f;
        return;
    }
    if (fervor < cost) {
        announcement = TextFormat("NEED %d FERVOR FOR CROWNFIRE", cost);
        announcementTimer = 1.0f;
        return;
    }

    fervor -= cost;
    stormFlash = std::max(stormFlash, 0.28f);
    sanctumPulseVisual = std::max(sanctumPulseVisual, 0.95f);

    Vector3 core = grid.CellCenter(fortress.coreCell.x, fortress.coreCell.y);
    std::vector<Vector3> origins;
    origins.push_back({ core.x + 2.0f, 6.8f, core.z });
    for (const BastionSite& bastion : bastions) {
        if (bastion.level <= 0) continue;
        Vector3 p = grid.CellCenter(bastion.cell.x, bastion.cell.y);
        p.y = 4.2f + 0.35f * (float)bastion.level;
        origins.push_back(p);
    }

    int volleys = 3 + raisedBastions + (consecrated >= 2 ? 1 : 0);
    int hits = 0;
    int slain = 0;
    std::vector<int> used;

    for (int shotIndex = 0; shotIndex < volleys; ++shotIndex) {
        int bestIndex = -1;
        float bestScore = -100000.0f;
        for (int i = 0; i < (int)enemies.size(); ++i) {
            if (enemies[i].hp <= 0) continue;
            bool alreadyUsed = false;
            for (int usedIndex : used) {
                if (usedIndex == i) { alreadyUsed = true; break; }
            }
            if (alreadyUsed) continue;

            float score = 0.0f;
            score += (enemies[i].laneIndex == omenLane) ? 180.0f : 35.0f;
            score += enemies[i].elite ? 70.0f : 0.0f;
            score += (enemies[i].type == EnemyType::ProcessionBreaker) ? 110.0f : 0.0f;
            score += (enemies[i].type == EnemyType::DirgeHerald) ? 84.0f : 0.0f;
            score += (enemies[i].type == EnemyType::BannerKnight) ? 22.0f : 0.0f;
            score += (enemies[i].type == EnemyType::AshHound) ? 16.0f : 0.0f;
            score += (float)enemies[i].pathIndex * 18.0f;
            score -= DistanceXZ(core, enemies[i].pos) * 0.30f;
            if (score > bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }

        if (bestIndex < 0) break;
        used.push_back(bestIndex);

        Enemy& target = enemies[bestIndex];
        int damage = 18 + raisedBastions * 5 + consecrated * 4 + legacy.arsenalRank * 2;
        if (target.laneIndex == omenLane) damage += 8;
        if (target.type == EnemyType::ProcessionBreaker) damage += 10;
        if (target.elite) damage += 4;

        int before = target.hp;
        target.hp -= damage;
        target.hitFlash = 0.18f;
        target.slowTimer = std::max(target.slowTimer, 1.10f + 0.20f * (float)raisedBastions);
        hits++;

        ShotFx fx{};
        fx.start = origins[shotIndex % (int)origins.size()];
        fx.end = { target.pos.x, target.pos.y + 0.58f, target.pos.z };
        fx.life = 0.20f;
        fx.color = (target.laneIndex == omenLane) ? Color{ 255, 214, 146, 255 } : Color{ 214, 196, 156, 255 };
        shots.push_back(fx);

        if (before > 0 && target.hp <= 0) {
            RegisterEnemyKill(target);
            slain++;
        }
    }

    if (hits <= 0) {
        announcement = "CROWNFIRE SCOURS AN EMPTY HORIZON";
        announcementTimer = 0.9f;
    }
    else {
        announcement = TextFormat("CROWNFIRE DECREE // %d STRUCK, %d SLAIN", hits, slain);
        announcementTimer = 1.4f;
    }
}

int Game::GetLegacyUpgradeCost(int slot) const {
    int rank = GetLegacyUpgradeRank(slot);
    if (slot == 0) return 18 + rank * 16 + rank * rank * 4;
    if (slot == 1) return 22 + rank * 18 + rank * rank * 5;
    if (slot == 2) return 16 + rank * 14 + rank * rank * 4;
    return 20 + rank * 17 + rank * rank * 5;
}

int Game::GetLegacyUpgradeRank(int slot) const {
    if (slot == 0) return legacy.rampartRank;
    if (slot == 1) return legacy.arsenalRank;
    if (slot == 2) return legacy.emberkeepRank;
    return legacy.hymnRank;
}

int Game::GetLegacyUpgradeMaxRank(int) const {
    return 4;
}

const char* Game::GetLegacyUpgradeLabel(int slot) const {
    if (slot == 0) return "RAMPART DOCTRINE";
    if (slot == 1) return "ARSENAL DOCTRINE";
    if (slot == 2) return "EMBER RELIQUARY";
    return "HYMNAL CODEX";
}

void Game::BuildMap() {
    grid.width = 60;
    grid.height = 46;
    grid.cellSize = 2.4f;
    grid.origin = { -72.0f, 0.0f, -55.2f };
    grid.tiles.assign((size_t)grid.width * (size_t)grid.height, GridTile{});
    props.clear();
    waystones.clear();
    bastions.clear();

    const int shrineCx = 30;
    const int shrineCy = 23;

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            GridTile& tile = grid.At(x, y);
            tile.kind = TileKind::Buildable;
            tile.occupied = false;

            float dx = std::fabs((float)x - (float)shrineCx);
            float dy = std::fabs((float)y - (float)shrineCy);
            float basin = 0.14f + dx * 0.0024f + dy * 0.0030f;
            float terrace = (((x / 5) + (y / 4)) % 2 == 0) ? 0.00f : 0.02f;
            float northRise = (y < 10) ? (float)(10 - y) * 0.018f : 0.0f;
            float southRise = (y > grid.height - 11) ? (float)(y - (grid.height - 11)) * 0.018f : 0.0f;
            tile.height = basin + terrace + northRise + southRise;
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

    addSegment(lanes[0], 3, 8, 10, 8);
    addSegment(lanes[0], 10, 8, 16, 10);
    addSegment(lanes[0], 16, 10, 22, 13);
    addSegment(lanes[0], 22, 13, 26, 16);
    addSegment(lanes[0], 26, 16, 28, 18);
    addSegment(lanes[0], 28, 18, 34, 18);
    addSegment(lanes[0], 34, 18, 40, 19);
    addSegment(lanes[0], 40, 19, 46, 23);

    addSegment(lanes[1], 3, 23, 11, 23);
    addSegment(lanes[1], 11, 23, 18, 23);
    addSegment(lanes[1], 18, 23, 23, 24);
    addSegment(lanes[1], 23, 24, 26, 26);
    addSegment(lanes[1], 26, 26, 30, 27);
    addSegment(lanes[1], 30, 27, 36, 27);
    addSegment(lanes[1], 36, 27, 42, 25);
    addSegment(lanes[1], 42, 25, 46, 23);

    addSegment(lanes[2], 3, 37, 10, 37);
    addSegment(lanes[2], 10, 37, 17, 35);
    addSegment(lanes[2], 17, 35, 23, 32);
    addSegment(lanes[2], 23, 32, 27, 29);
    addSegment(lanes[2], 27, 29, 32, 28);
    addSegment(lanes[2], 32, 28, 37, 27);
    addSegment(lanes[2], 37, 27, 42, 26);
    addSegment(lanes[2], 42, 26, 46, 23);

    for (const std::vector<GridCoord>& lane : lanes) {
        for (int i = 0; i < (int)lane.size(); ++i) {
            const GridCoord& c = lane[i];
            if (!grid.InBounds(c.x, c.y)) continue;
            GridTile& tile = grid.At(c.x, c.y);
            tile.kind = (i == 0) ? TileKind::Spawn : TileKind::Road;
            tile.occupied = false;
            tile.height = (i == 0) ? 0.05f : 0.06f;
        }
    }

    fortress.gateCell = { 46, 23 };
    fortress.coreCell = { 54, 23 };

    for (int y = 15; y <= 31; ++y) {
        for (int x = 49; x <= 58; ++x) {
            GridTile& tile = grid.At(x, y);
            tile.kind = TileKind::Fortress;
            tile.occupied = true;
            tile.height = 0.12f;
        }
    }

    const GridCoord courtRoad[] = {
        { 46, 23 }, { 47, 23 }, { 48, 23 }, { 49, 23 }, { 50, 23 }, { 51, 23 }, { 52, 23 }, { 53, 23 },
        { 49, 22 }, { 50, 22 }, { 51, 22 }, { 52, 22 },
        { 49, 24 }, { 50, 24 }, { 51, 24 }, { 52, 24 },
        { 50, 21 }, { 50, 25 }
    };
    for (const GridCoord& c : courtRoad) {
        GridTile& tile = grid.At(c.x, c.y);
        tile.kind = TileKind::Road;
        tile.occupied = false;
        tile.height = 0.05f;
    }

    auto setTerrainBlocked = [&](int x, int y, float height) {
        if (!grid.InBounds(x, y)) return;
        GridTile& tile = grid.At(x, y);
        if (tile.kind == TileKind::Road || tile.kind == TileKind::Spawn || tile.kind == TileKind::Fortress) return;
        tile.kind = TileKind::Blocked;
        tile.occupied = true;
        tile.height = std::max(tile.height, height);
        };

    auto liftTerrainEllipse = [&](int cx, int cy, float rx, float ry, float peak, float blockThreshold) {
        for (int y = 0; y < grid.height; ++y) {
            for (int x = 0; x < grid.width; ++x) {
                float nx = ((float)x - (float)cx) / rx;
                float ny = ((float)y - (float)cy) / ry;
                float d = nx * nx + ny * ny;
                if (d > 1.0f) continue;

                GridTile& tile = grid.At(x, y);
                if (tile.kind == TileKind::Road || tile.kind == TileKind::Spawn || tile.kind == TileKind::Fortress) continue;

                float rise = (1.0f - d) * peak;
                tile.height = std::max(tile.height, 0.22f + rise);
                if (d <= blockThreshold && tile.kind == TileKind::Buildable) {
                    tile.kind = TileKind::Blocked;
                    tile.occupied = true;
                }
            }
        }
        };

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            int distLeft = x;
            int distRight = (grid.width - 1) - x;
            int distTop = y;
            int distBottom = (grid.height - 1) - y;
            int edgeDist = std::min(std::min(distLeft, distRight), std::min(distTop, distBottom));
            if (edgeDist < 3) {
                float edgeHeight = 0.78f + (float)(2 - edgeDist) * 0.18f + 0.03f * (float)((x + y) % 2);
                setTerrainBlocked(x, y, edgeHeight);
            }
        }
    }

    liftTerrainEllipse(15, 9, 6.8f, 4.2f, 0.84f, 0.42f);
    liftTerrainEllipse(24, 14, 5.4f, 3.4f, 0.54f, 0.26f);
    liftTerrainEllipse(41, 10, 5.8f, 3.6f, 0.72f, 0.36f);
    liftTerrainEllipse(18, 37, 7.2f, 4.2f, 0.80f, 0.42f);
    liftTerrainEllipse(35, 35, 6.4f, 4.0f, 0.66f, 0.30f);
    liftTerrainEllipse(30, 23, 3.4f, 2.6f, 0.52f, 0.68f);

    for (int y = 18; y <= 28; ++y) {
        for (int x = 24; x <= 36; ++x) {
            if (!grid.InBounds(x, y)) continue;
            GridTile& tile = grid.At(x, y);
            if (tile.kind == TileKind::Road || tile.kind == TileKind::Spawn || tile.kind == TileKind::Fortress) continue;

            float dx = std::fabs((float)x - (float)shrineCx);
            float dy = std::fabs((float)y - (float)shrineCy);
            if (dx <= 6.0f && dy <= 5.0f) {
                tile.height = std::max(tile.height, 0.24f + 0.02f * (6.0f - dx) + 0.015f * (5.0f - dy));
            }
        }
    }

    auto addWaystone = [&](int x, int y) {
        if (!grid.InBounds(x, y)) return;
        GridTile& tile = grid.At(x, y);
        if (tile.kind == TileKind::Road || tile.kind == TileKind::Spawn || tile.kind == TileKind::Fortress) return;
        tile.kind = TileKind::Blocked;
        tile.occupied = false;
        tile.height = std::max(tile.height, 0.28f);
        waystones.push_back({ { x, y }, false });
        };

    auto addBastion = [&](int x, int y, int laneIndex) {
        if (!grid.InBounds(x, y)) return;
        GridTile& tile = grid.At(x, y);
        tile.kind = TileKind::Fortress;
        tile.occupied = true;
        tile.height = std::max(tile.height, 0.14f);
        bastions.push_back({ { x, y }, laneIndex, 0, 0.0f });
        };

    const int pineTrees[][2] = {
        { 11, 6 }, { 14, 7 }, { 17, 6 }, { 20, 8 }, { 15, 11 }, { 22, 10 },
        { 12, 39 }, { 16, 38 }, { 19, 40 }, { 22, 37 }, { 25, 35 }, { 29, 35 },
        { 38, 8 }, { 43, 8 }, { 46, 10 }, { 34, 36 }, { 39, 34 }, { 43, 35 }
    };
    for (const auto& p : pineTrees) AddProp(PropType::PineTree, p[0], p[1], true);

    const int deadTrees[][2] = {
        { 5, 5 }, { 6, 12 }, { 5, 23 }, { 6, 34 }, { 8, 41 },
        { 46, 5 }, { 53, 8 }, { 56, 12 }, { 54, 35 }, { 49, 40 }
    };
    for (const auto& p : deadTrees) AddProp(PropType::DeadTree, p[0], p[1], true);

    const int graves[][2] = {
        { 9, 18 }, { 10, 28 }, { 24, 17 }, { 25, 30 }, { 44, 18 }, { 44, 28 }, { 52, 23 }
    };
    for (const auto& p : graves) AddProp(PropType::GraveMarker, p[0], p[1], true);

    const int rubble[][2] = {
        { 12, 15 }, { 20, 13 }, { 27, 17 }, { 30, 29 }, { 37, 18 }, { 39, 27 }, { 46, 19 }, { 46, 27 }
    };
    for (const auto& p : rubble) AddProp(PropType::RubblePile, p[0], p[1], false);

    const int carts[][2] = { { 49, 18 }, { 49, 28 }, { 56, 20 }, { 56, 26 } };
    for (const auto& p : carts) AddProp(PropType::CartWreck, p[0], p[1], true);

    const int braziers[][2] = {
        { 27, 20 }, { 33, 20 }, { 27, 26 }, { 33, 26 },
        { 47, 20 }, { 47, 26 }, { 51, 18 }, { 51, 28 }, { 54, 17 }, { 54, 29 }
    };
    for (const auto& p : braziers) AddProp(PropType::Brazier, p[0], p[1], false);

    const int banners[][2] = {
        { 11, 8 }, { 11, 23 }, { 11, 37 }, { 29, 18 }, { 31, 27 }, { 45, 23 }, { 53, 15 }, { 53, 31 }
    };
    for (const auto& p : banners) AddProp(PropType::BannerPole, p[0], p[1], false);

    addWaystone(25, 20);
    addWaystone(26, 31);
    addWaystone(41, 23);

    addBastion(51, 17, 0);
    addBastion(56, 23, 1);
    addBastion(51, 29, 2);

    cameraFocus = grid.CellCenter(shrineCx + 1, shrineCy);
}


void Game::BuildWave(int waveNumber) {
    wave = WaveState{};
    wave.number = waveNumber;
    wave.active = false;

    int laneCount = (int)lanes.size();
    if (laneCount <= 0) laneCount = 1;

    auto countTowerType = [&](TowerType type) {
        int count = 0;
        for (const Tower& tower : towers) {
            if (tower.type == type) count++;
        }
        return count;
        };

    auto laneDefenseScore = [&](int laneIndex) {
        if (laneIndex < 0 || laneIndex >= (int)lanes.size()) return 0.0f;
        float score = 0.0f;
        for (const Tower& tower : towers) {
            float bestDist = 99999.0f;
            for (const GridCoord& step : lanes[laneIndex]) {
                Vector3 p = grid.CellCenter(step.x, step.y);
                float dist = DistanceXZ(p, tower.pos);
                if (dist < bestDist) bestDist = dist;
            }

            float laneWeight = 0.0f;
            if (bestDist <= tower.range + 1.0f) {
                laneWeight = 12.0f + (float)tower.level * 6.0f;
                if (tower.type == TowerType::WatchbowNest) laneWeight += 6.0f;
                else if (tower.type == TowerType::CenserShrine) laneWeight += 4.0f;
                else if (tower.type == TowerType::ReliquarySpire) laneWeight += 7.0f;
                else laneWeight += 5.0f;
                laneWeight += tower.range * 1.6f;
            }
            else if (bestDist <= tower.range + grid.cellSize * 1.8f) {
                laneWeight = 4.0f + (float)tower.level * 2.5f;
            }
            score += laneWeight;
        }

        for (const BastionSite& bastion : bastions) {
            if (bastion.level <= 0) continue;
            if (bastion.laneIndex == laneIndex) score += 18.0f + bastion.level * 8.0f;
        }

        for (const WaystoneSite& stone : waystones) {
            if (!stone.consecrated) continue;
            for (const GridCoord& step : lanes[laneIndex]) {
                if (std::abs(step.x - stone.cell.x) + std::abs(step.y - stone.cell.y) <= 4) {
                    score += 10.0f;
                    break;
                }
            }
        }

        return score;
        };

    int weakestLane = 0;
    float weakestScore = 100000.0f;
    for (int laneIndex = 0; laneIndex < laneCount; ++laneIndex) {
        float score = laneDefenseScore(laneIndex);
        if (score < weakestScore) {
            weakestScore = score;
            weakestLane = laneIndex;
        }
    }

    int watchbows = countTowerType(TowerType::WatchbowNest);
    int censers = countTowerType(TowerType::CenserShrine);
    int spires = countTowerType(TowerType::ReliquarySpire);
    int barricades = countTowerType(TowerType::PilgrimBarricade);

    int dominantStyle = 0;
    int dominantCount = watchbows;
    if (censers > dominantCount) { dominantCount = censers; dominantStyle = 1; }
    if (spires > dominantCount) { dominantCount = spires; dominantStyle = 2; }
    if (barricades > dominantCount) { dominantCount = barricades; dominantStyle = 3; }

    int count = 16 + (waveNumber - 1) * 4;
    bool bossWave = (waveNumber % 5 == 0);
    omenLane = (waveNumber <= 1) ? -1 : weakestLane;
    waveOmen = "THREE ROADS BURN";

    bool houndPressure = false;
    bool heraldPressure = false;
    bool brutePressure = false;
    bool knightPressure = false;

    if (bossWave) {
        omenLane = weakestLane;
        waveOmen = (waveNumber >= 10) ? "ADAPTIVE DOUBLE BREAKER MARCH" : "ADAPTIVE BREAKER MARCH";
        brutePressure = true;
        heraldPressure = true;
    }
    else if (dominantCount <= 0) {
        int pattern = waveNumber % 4;
        if (pattern == 1) waveOmen = "BROKEN COLUMN ADVANCE";
        else if (pattern == 2) { omenLane = 0; waveOmen = "NORTH ROAD HOUND HUNT"; houndPressure = true; }
        else if (pattern == 3) { omenLane = laneCount > 1 ? 1 : 0; waveOmen = "MIDDLE ROAD KNIGHT LANCE"; knightPressure = true; }
        else { omenLane = laneCount > 2 ? 2 : laneCount - 1; waveOmen = "SOUTH ROAD FUNERAL CHANT"; heraldPressure = true; }
    }
    else if (dominantStyle == 0) {
        omenLane = weakestLane;
        waveOmen = "ADAPTIVE HOUND HUNT";
        houndPressure = true;
    }
    else if (dominantStyle == 1) {
        omenLane = weakestLane;
        waveOmen = "ADAPTIVE BRUTE POUND";
        brutePressure = true;
    }
    else if (dominantStyle == 2) {
        omenLane = weakestLane;
        waveOmen = "ADAPTIVE SWARM LUNGE";
        houndPressure = true;
        knightPressure = true;
    }
    else {
        omenLane = weakestLane;
        waveOmen = "ADAPTIVE HERALD SCREEN";
        heraldPressure = true;
        knightPressure = true;
    }

    for (int i = 0; i < count; ++i) {
        SpawnEntry entry{};
        entry.spawnTime = 0.52f * i;
        bool focusLane = (omenLane >= 0) && ((i % 3) != 2 || bossWave);
        entry.laneIndex = focusLane ? omenLane : ((i + waveNumber + (houndPressure ? 1 : 0)) % laneCount);
        entry.type = EnemyType::AshRaider;

        if (focusLane) {
            entry.spawnTime -= 0.07f * (float)(1 + (i % 2));
            if (entry.spawnTime < 0.0f) entry.spawnTime = 0.0f;
        }

        int roll = (i * 3 + waveNumber + entry.laneIndex * 5) % 12;
        if (waveNumber >= 2 && (roll == 3 || roll == 8 || (brutePressure && roll == 1))) {
            entry.type = EnemyType::GraveBrute;
            entry.spawnTime += 0.08f;
        }
        if (waveNumber >= 3 && (roll == 5 || (knightPressure && (roll == 0 || roll == 10)))) {
            entry.type = EnemyType::BannerKnight;
        }
        if (waveNumber >= 4 && (houndPressure && (roll == 2 || roll == 7 || (focusLane && (i % 4) == 1)))) {
            entry.type = EnemyType::AshHound;
            entry.spawnTime -= 0.10f;
            if (entry.spawnTime < 0.0f) entry.spawnTime = 0.0f;
        }
        if (waveNumber >= 5 && (heraldPressure && (roll == 4 || (focusLane && (i % 5) == 0)))) {
            entry.type = EnemyType::DirgeHerald;
            entry.spawnTime += 0.14f;
        }
        if (waveNumber >= 7 && !houndPressure && entry.type == EnemyType::AshRaider && (i % 7) == 0) {
            entry.type = EnemyType::AshHound;
        }
        if (waveNumber >= 8 && !heraldPressure && entry.type == EnemyType::AshRaider && (i % 6) == 1) {
            entry.type = EnemyType::DirgeHerald;
            entry.spawnTime += 0.08f;
        }
        if (waveNumber >= 9 && brutePressure && entry.type == EnemyType::BannerKnight && (i % 4) == 0) {
            entry.type = EnemyType::GraveBrute;
            entry.spawnTime += 0.10f;
        }

        entry.elite = waveNumber >= 4 && ((i + waveNumber + entry.laneIndex) % 7 == 0);
        if (focusLane && waveNumber >= 6 && (i % 5) == 0) entry.elite = true;
        if (entry.type == EnemyType::DirgeHerald && waveNumber >= 8) entry.elite = true;
        if (entry.type == EnemyType::AshHound && waveNumber >= 6 && (i % 4) == 0) entry.elite = true;
        if (bossWave && i >= count - 5) entry.elite = true;

        wave.spawns.push_back(entry);
    }

    if (bossWave) {
        SpawnEntry escortA{};
        escortA.spawnTime = 0.52f * count + 0.20f;
        escortA.laneIndex = weakestLane;
        escortA.type = EnemyType::AshHound;
        escortA.elite = true;
        wave.spawns.push_back(escortA);

        SpawnEntry escortB{};
        escortB.spawnTime = 0.52f * count + 0.55f;
        escortB.laneIndex = laneCount > 1 ? ((weakestLane + 1) % laneCount) : 0;
        escortB.type = EnemyType::BannerKnight;
        escortB.elite = true;
        wave.spawns.push_back(escortB);

        SpawnEntry escortC{};
        escortC.spawnTime = 0.52f * count + 0.90f;
        escortC.laneIndex = weakestLane;
        escortC.type = EnemyType::DirgeHerald;
        escortC.elite = true;
        wave.spawns.push_back(escortC);

        if (laneCount > 2) {
            SpawnEntry escortD{};
            escortD.spawnTime = 0.52f * count + 1.15f;
            escortD.laneIndex = (weakestLane + 2) % laneCount;
            escortD.type = EnemyType::GraveBrute;
            escortD.elite = true;
            wave.spawns.push_back(escortD);
        }

        SpawnEntry bossA{};
        bossA.spawnTime = 0.52f * count + 1.55f;
        bossA.laneIndex = weakestLane;
        bossA.type = EnemyType::ProcessionBreaker;
        bossA.elite = (waveNumber >= 15);
        wave.spawns.push_back(bossA);

        if (waveNumber >= 10 && laneCount >= 3) {
            SpawnEntry bossB{};
            bossB.spawnTime = bossA.spawnTime + 2.2f;
            bossB.laneIndex = (weakestLane + 1) % laneCount;
            bossB.type = EnemyType::ProcessionBreaker;
            bossB.elite = (waveNumber >= 15);
            wave.spawns.push_back(bossB);
        }
    }

    ConfigureRoyalDirective();
}

void Game::ConfigureRoyalDirective() {
    directive = RoyalDirective{};

    int hounds = 0;
    int heralds = 0;
    int breakers = 0;
    int elites = 0;
    for (const SpawnEntry& entry : wave.spawns) {
        if (entry.type == EnemyType::AshHound) hounds++;
        if (entry.type == EnemyType::DirgeHerald) heralds++;
        if (entry.type == EnemyType::ProcessionBreaker) breakers++;
        if (entry.elite) elites++;
    }

    directive.rewardGold = 14 + wave.number * 3;
    directive.rewardIron = 8 + wave.number * 2;
    directive.rewardEmber = 2 + wave.number / 2;
    directive.rewardAsh = 4 + wave.number / 2;

    if (breakers > 0) {
        directive.type = RoyalDirectiveType::BreakBreakers;
        directive.target = breakers;
        directive.title = "BREAKER DECREE";
        directive.detail = "Slay every Procession Breaker before the sanctum shatters.";
        directive.rewardGold += 18;
        directive.rewardIron += 6;
        directive.rewardEmber += 4;
        directive.rewardAsh += 4;
    }
    else if (heralds > 0) {
        directive.type = RoyalDirectiveType::SilenceHeralds;
        directive.target = heralds;
        directive.title = "SILENCE THE CHANT";
        directive.detail = "Kill each Dirge Herald in this wave.";
        directive.rewardGold += 10;
        directive.rewardEmber += 3;
        directive.rewardAsh += 2;
    }
    else if (omenLane == 0 && hounds >= 4) {
        directive.type = RoyalDirectiveType::HuntHounds;
        directive.target = std::min(hounds, 6 + wave.number / 5);
        directive.title = "ASH HUNT";
        directive.detail = "Cull the omen hounds before they flood the walls.";
        directive.rewardGold += 12;
        directive.rewardIron += 4;
        directive.rewardAsh += 1;
    }
    else if (elites >= 2) {
        directive.type = RoyalDirectiveType::SlayElites;
        directive.target = std::min(elites, 3 + wave.number / 7);
        directive.title = "CULL THE ANOINTED";
        directive.detail = "Destroy the elite attackers marked by the omen.";
        directive.rewardGold += 12;
        directive.rewardEmber += 2;
        directive.rewardAsh += 3;
    }
    else if ((wave.number % 2) == 1) {
        directive.type = RoyalDirectiveType::HoldGate;
        directive.target = 1;
        directive.title = "HOLD THE GATE";
        directive.detail = "Let no enemy damage the front gate this wave.";
        directive.rewardGold += 4;
        directive.rewardIron += 6;
    }
    else {
        directive.type = RoyalDirectiveType::HoldCore;
        directive.target = 1;
        directive.title = "KEEP THE SANCTUM";
        directive.detail = "Let no enemy strike the Holy Core this wave.";
        directive.rewardEmber += 3;
        directive.rewardAsh += 1;
    }

    waveGateDamageTaken = 0;
    waveCoreDamageTaken = 0;
}

void Game::StartWave() {
    if (state != PlayState::BuildPhase) return;

    SaveSuspendedRun();
    hasSuspendedChronicle = true;
    if (wave.number > legacy.highestWave) {
        legacy.highestWave = wave.number;
        SaveLegacyProfile();
    }

    waveGateDamageTaken = 0;
    waveCoreDamageTaken = 0;
    directive.progress = 0;
    directive.failed = false;
    directive.completed = false;
    wave.active = true;
    wave.timer = 0.0f;
    wave.nextSpawnIndex = 0;
    state = PlayState::BattlePhase;
    sanctumPulseTimer = std::max(2.6f, 6.0f - 0.4f * (float)GetConsecratedWaystoneCount());
    if (stormFlash < 0.12f) stormFlash = 0.12f;
    announcement = TextFormat("WAVE %d // %s // %s", wave.number, waveOmen.c_str(), directive.title.c_str());
    announcementTimer = 3.0f;
}

void Game::SpawnEnemy(EnemyType type, int laneIndex, bool elite) {
    if (laneIndex < 0 || laneIndex >= (int)lanes.size() || lanes[laneIndex].empty()) laneIndex = 0;

    Enemy enemy{};
    enemy.type = type;
    enemy.laneIndex = laneIndex;
    enemy.pathIndex = 0;
    enemy.elite = elite;

    if (type == EnemyType::ProcessionBreaker) {
        enemy.hp = 340 + (wave.number - 1) * 55;
        enemy.maxHp = enemy.hp;
        enemy.speed = 0.92f + (wave.number - 1) * 0.05f;
        enemy.attackCooldown = 1.18f;
        enemy.gateDamage = 24;
        enemy.coreDamage = 20;
        enemy.pos.y = 1.10f;
    }
    else if (type == EnemyType::DirgeHerald) {
        enemy.hp = 54 + (wave.number - 1) * 12;
        enemy.maxHp = enemy.hp;
        enemy.speed = 1.20f + (wave.number - 1) * 0.08f;
        enemy.attackCooldown = 0.94f;
        enemy.gateDamage = 8;
        enemy.coreDamage = 10;
        enemy.pos.y = 0.86f;
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
    else if (type == EnemyType::AshHound) {
        enemy.hp = 18 + (wave.number - 1) * 5;
        enemy.maxHp = enemy.hp;
        enemy.speed = 2.90f + (wave.number - 1) * 0.18f;
        enemy.attackCooldown = 0.56f;
        enemy.gateDamage = 4;
        enemy.coreDamage = 5;
        enemy.pos.y = 0.48f;
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

    if (elite) {
        float hpMul = 1.48f;
        if (type == EnemyType::ProcessionBreaker) hpMul = 1.22f;
        else if (type == EnemyType::AshHound) hpMul = 1.35f;
        else if (type == EnemyType::DirgeHerald) hpMul = 1.40f;
        enemy.hp = (int)std::round((float)enemy.hp * hpMul);
        enemy.maxHp = enemy.hp;
        enemy.speed *= (type == EnemyType::ProcessionBreaker) ? 1.05f : 1.10f;
        enemy.attackCooldown *= 0.92f;
        enemy.gateDamage += 3 + wave.number / 6;
        enemy.coreDamage += 3 + wave.number / 5;
        enemy.pos.y += 0.10f;
    }

    if (omenLane >= 0 && laneIndex == omenLane) {
        enemy.speed *= 1.05f;
    }

    Vector3 start = grid.CellCenter(lanes[laneIndex][0].x, lanes[laneIndex][0].y);
    Vector3 next = start;
    if (lanes[laneIndex].size() > 1) {
        next = grid.CellCenter(lanes[laneIndex][1].x, lanes[laneIndex][1].y);
    }
    Vector3 forward = NormalizeXZ(Vec3Sub(next, start));
    Vector3 side = { -forward.z, 0.0f, forward.x };
    int formation = (wave.nextSpawnIndex % 5) - 2;
    float lateral = 0.18f * (float)formation;
    if (type == EnemyType::AshHound) lateral *= 1.5f;
    if (type == EnemyType::ProcessionBreaker) lateral *= 0.6f;

    enemy.pos.x = start.x - 1.2f + side.x * lateral;
    enemy.pos.z = start.z + side.z * lateral;
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
    UpdateAtmosphere(dt);

    if (state == PlayState::GameOver) {
        UpdateDeathFx(dt);
        if (IsKeyPressed(KEY_L)) {
            if (LoadSuspendedRun()) return;
            announcement = "NO CHRONICLE TO RESTORE";
            announcementTimer = 1.2f;
        }
        if (IsKeyPressed(KEY_ENTER)) ResetRun(false);
        return;
    }

    UpdateCamera(dt);
    UpdateHoverCell();
    UpdateShots(dt);
    UpdateDeathFx(dt);

    if (state == PlayState::BuildPhase) UpdateBuildPhase();
    else if (state == PlayState::BattlePhase) UpdateBattlePhase(dt);
}

void Game::UpdateAtmosphere(float dt) {
    if (stormFlash > 0.0f) {
        stormFlash -= dt * 0.80f;
        if (stormFlash < 0.0f) stormFlash = 0.0f;
    }
    if (sanctumPulseVisual > 0.0f) {
        sanctumPulseVisual -= dt * 0.72f;
        if (sanctumPulseVisual < 0.0f) sanctumPulseVisual = 0.0f;
    }

    if (state == PlayState::BattlePhase) {
        float interval = std::max(4.4f, 6.8f - 0.10f * (float)wave.number);
        int prevStep = (int)((worldTime - dt) / interval);
        int currStep = (int)(worldTime / interval);
        if (currStep != prevStep) {
            float flash = (wave.number % 5 == 0) ? 0.42f : (omenLane >= 0 ? 0.26f : 0.18f);
            if (flash > stormFlash) stormFlash = flash;
        }

        int consecrated = GetConsecratedWaystoneCount();
        if (consecrated >= 2 && fortress.coreHp > 0) {
            sanctumPulseTimer -= dt;
            float recharge = std::max(4.2f, 10.5f - consecrated * 1.2f - legacy.hymnRank * 0.6f);
            if (sanctumPulseTimer <= 0.0f) {
                TriggerSanctumPulse();
                sanctumPulseTimer = recharge;
            }
        }
        else {
            sanctumPulseTimer = 5.0f;
        }
    }
    else {
        sanctumPulseTimer = 5.0f;
    }
}

void Game::UpdateCamera(float dt) {
    float move = 26.0f * dt;
    if (IsKeyDown(KEY_A)) cameraFocus.x -= move;
    if (IsKeyDown(KEY_D)) cameraFocus.x += move;
    if (IsKeyDown(KEY_W)) cameraFocus.z -= move;
    if (IsKeyDown(KEY_S)) cameraFocus.z += move;

    cameraZoom -= GetMouseWheelMove() * 2.4f;
    cameraZoom = ClampFloat(cameraZoom, cameraMinZoom, cameraMaxZoom);

    float xMargin = 14.0f + cameraZoom * 0.42f;
    float zMargin = 12.0f + cameraZoom * 0.38f;
    float maxXMargin = grid.width * grid.cellSize * 0.38f;
    float maxZMargin = grid.height * grid.cellSize * 0.38f;
    if (xMargin > maxXMargin) xMargin = maxXMargin;
    if (zMargin > maxZMargin) zMargin = maxZMargin;

    float minX = grid.origin.x + xMargin;
    float maxX = grid.origin.x + grid.width * grid.cellSize - xMargin;
    float minZ = grid.origin.z + zMargin;
    float maxZ = grid.origin.z + grid.height * grid.cellSize - zMargin;
    cameraFocus.x = ClampFloat(cameraFocus.x, minX, maxX);
    cameraFocus.z = ClampFloat(cameraFocus.z, minZ, maxZ);

    camera.target = { cameraFocus.x, 0.45f, cameraFocus.z };
    camera.position = { cameraFocus.x + cameraZoom, cameraZoom * 0.84f, cameraFocus.z + cameraZoom * 0.96f };
    camera.fovy = 35.0f;
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
    if (IsKeyPressed(KEY_L)) {
        if (LoadSuspendedRun()) return;
        announcement = "NO CHRONICLE TO RESTORE";
        announcementTimer = 1.1f;
    }
    if (IsKeyPressed(KEY_R)) TryFortifyBastion();
    if (IsKeyPressed(KEY_FIVE)) TryBuyLegacyUpgrade(0);
    if (IsKeyPressed(KEY_SIX)) TryBuyLegacyUpgrade(1);
    if (IsKeyPressed(KEY_SEVEN)) TryBuyLegacyUpgrade(2);
    if (IsKeyPressed(KEY_EIGHT)) TryBuyLegacyUpgrade(3);
    if (IsKeyPressed(KEY_U)) TryUpgradeTower();
    if (IsKeyPressed(KEY_X)) TrySellTower();
    if (IsKeyPressed(KEY_C)) TryConsecrateWaystone();

    if (IsKeyPressed(KEY_H)) {
        if (fortress.gateHp >= fortress.gateMaxHp) {
            announcement = "GATE ALREADY WHOLE";
            announcementTimer = 1.0f;
        }
        else if (iron >= 15) {
            iron -= 15;
            fortress.gateHp += 22 + legacy.rampartRank * 4;
            if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
            SaveSuspendedRun();
            hasSuspendedChronicle = true;
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
            fortress.coreHp += 18 + legacy.hymnRank * 2;
            if (fortress.coreHp > fortress.coreMaxHp) fortress.coreHp = fortress.coreMaxHp;
            SaveSuspendedRun();
            hasSuspendedChronicle = true;
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
    if (IsKeyPressed(KEY_Q)) TriggerCrownfireDecree();

    if (wave.active) {
        wave.timer += dt;
        while (wave.nextSpawnIndex < (int)wave.spawns.size() && wave.timer >= wave.spawns[wave.nextSpawnIndex].spawnTime) {
            const SpawnEntry& entry = wave.spawns[wave.nextSpawnIndex];
            SpawnEnemy(entry.type, entry.laneIndex, entry.elite);
            if (entry.elite && stormFlash < 0.14f) stormFlash = 0.14f;
            wave.nextSpawnIndex++;
        }
    }

    UpdateEnemies(dt);
    UpdateTowers(dt);
    UpdateBastions(dt);

    if (wave.nextSpawnIndex >= (int)wave.spawns.size() && enemies.empty()) {
        wave.active = false;
        state = PlayState::BuildPhase;
        sanctumPulseVisual = 0.0f;

        if (!directive.failed && !directive.completed) {
            if (directive.type == RoyalDirectiveType::HoldGate) {
                directive.completed = (waveGateDamageTaken <= 0);
            }
            else if (directive.type == RoyalDirectiveType::HoldCore) {
                directive.completed = (waveCoreDamageTaken <= 0);
            }
        }

        if (!directive.completed && directive.type != RoyalDirectiveType::HoldGate && directive.type != RoyalDirectiveType::HoldCore) {
            directive.failed = true;
        }

        int clearedWave = wave.number;
        int blessedSites = GetConsecratedWaystoneCount();
        int raisedBastions = 0;
        for (const BastionSite& bastion : bastions) if (bastion.level > 0) raisedBastions++;

        bool directiveCompleted = directive.completed;
        int directiveRewardGold = directive.rewardGold;
        int directiveRewardIron = directive.rewardIron;
        int directiveRewardEmber = directive.rewardEmber;
        int directiveRewardAsh = directive.rewardAsh;
        bool flawlessWave = (waveGateDamageTaken == 0 && waveCoreDamageTaken == 0);

        gold += 32 + clearedWave * 9 + blessedSites * 6 + raisedBastions * 4;
        iron += 14 + clearedWave * 4 + (fortress.gateHp > 0 ? 6 : 0) + blessedSites * 3 + raisedBastions * 2;
        ember += 4 + clearedWave + blessedSites * 2 + legacy.emberkeepRank;
        if (fortress.gateHp > 0) {
            fortress.gateHp += 6 + blessedSites * 2 + legacy.rampartRank * 2 + raisedBastions * 3;
            if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
        }
        fervor += 18 + blessedSites * 4 + legacy.hymnRank * 2 + raisedBastions * 3;
        if (fervor > fervorMax) fervor = fervorMax;
        AwardLegacyAsh(8 + clearedWave * 2 + blessedSites * 2 + raisedBastions * 2 + ((clearedWave % 5 == 0) ? 6 : 0));

        if (directiveCompleted) {
            gold += directiveRewardGold;
            iron += directiveRewardIron;
            ember += directiveRewardEmber;
            AwardLegacyAsh(directiveRewardAsh);
            legacy.directivesCompleted++;
            SaveLegacyProfile();
        }

        if (flawlessWave) {
            legacy.flawlessWaves++;
            AwardLegacyAsh(1 + clearedWave / 6);
            SaveLegacyProfile();
        }

        wave.number++;
        BuildWave(wave.number);
        SaveSuspendedRun();
        hasSuspendedChronicle = true;
        if (stormFlash < 0.20f) stormFlash = 0.20f;

        if (directiveCompleted && flawlessWave) {
            announcement = TextFormat("ROYAL DIRECTIVE FULFILLED // FLAWLESS HOLD // +G%d I%d E%d ASH %d", directiveRewardGold, directiveRewardIron, directiveRewardEmber, directiveRewardAsh);
        }
        else if (directiveCompleted) {
            announcement = TextFormat("ROYAL DIRECTIVE FULFILLED // +G%d I%d E%d ASH %d", directiveRewardGold, directiveRewardIron, directiveRewardEmber, directiveRewardAsh);
        }
        else if (flawlessWave) {
            announcement = "FLAWLESS HOLD // REBUILD, UPGRADE, FORTIFY";
        }
        else if (blessedSites > 0 && raisedBastions > 0) {
            announcement = TextFormat("SIEGE BROKEN // %d WAYSTONES AND %d BASTIONS HOLD THE MARCH", blessedSites, raisedBastions);
        }
        else if (blessedSites > 0) {
            announcement = TextFormat("SIEGE BROKEN // %d WAYSTONES EMPOWER THE MARCH", blessedSites);
        }
        else {
            announcement = "SIEGE BROKEN // REBUILD, UPGRADE, FORTIFY";
        }
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
        if (hymnTimer > 0.0f) speedMul *= enemy.elite ? 0.88f : 0.84f;
        if (enemy.elite) speedMul *= 1.08f;
        if (enemy.type == EnemyType::AshHound) speedMul *= 1.15f;
        if (omenLane >= 0 && enemy.laneIndex == omenLane && !enemy.pastGate) speedMul *= 1.06f;

        bool buffedByHerald = false;
        for (const Enemy& other : enemies) {
            if (&other == &enemy || other.hp <= 0) continue;
            if (other.type == EnemyType::DirgeHerald && DistanceXZ(other.pos, enemy.pos) <= 4.2f) {
                buffedByHerald = true;
                break;
            }
        }
        if (buffedByHerald) {
            speedMul *= 1.16f;
            if (enemy.slowTimer > 0.0f) speedMul *= 1.12f;
        }

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
                if (enemy.type == EnemyType::DirgeHerald && DistanceXZ(enemy.pos, grid.CellCenter(fortress.gateCell.x, fortress.gateCell.y)) < 5.2f) {
                    if (enemy.attackTimer <= 0.0f) {
                        DamageGate(enemy.gateDamage + 2);
                        enemy.attackTimer = enemy.attackCooldown + 0.15f;
                    }
                    continue;
                }

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
        if (coreDist < ((enemy.type == EnemyType::DirgeHerald) ? 1.6f : 1.0f)) {
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
        bool blessed = IsTowerBlessed(tower);
        int wardLevel = GetTowerBastionWardLevel(tower);
        float effectiveRange = tower.range + (blessed ? 0.90f : 0.0f) + 0.34f * (float)wardLevel;
        int effectiveDamage = tower.damage + (blessed ? 3 : 0) + wardLevel * 2;
        float fireMul = towerSpeedMul * (blessed ? 1.18f : 1.0f) * (1.0f + 0.08f * (float)wardLevel);
        if (blessed && wardLevel > 0) {
            effectiveDamage += 2;
            fireMul *= 1.05f;
        }

        tower.cooldown -= dt * fireMul;
        if (tower.cooldown > 0.0f) continue;

        if (tower.type == TowerType::CenserShrine) {
            bool hitAny = false;
            int hitCount = 0;
            for (Enemy& enemy : enemies) {
                if (enemy.hp <= 0) continue;
                if (DistanceXZ(tower.pos, enemy.pos) <= effectiveRange) {
                    int before = enemy.hp;
                    enemy.hp -= effectiveDamage;
                    enemy.hitFlash = 0.08f;
                    if (tower.level >= 2) {
                        enemy.slowTimer = std::max(enemy.slowTimer, 0.35f + 0.25f * (float)tower.level + 0.10f * (float)wardLevel);
                    }
                    if (before > 0 && enemy.hp <= 0) RegisterEnemyKill(enemy);
                    hitAny = true;
                    hitCount++;
                }
            }
            if (hitAny) {
                tower.cooldown = tower.maxCooldown;
                if (blessed && hitCount >= 2) GainFervor(hitCount);
                if (wardLevel >= 2) GainFervor(1 + hitCount / 2);
                ShotFx fx{};
                fx.start = { tower.pos.x, tower.pos.y + 0.5f, tower.pos.z };
                fx.end = { tower.pos.x, tower.pos.y + 2.6f, tower.pos.z };
                fx.life = 0.16f;
                fx.color = wardLevel > 0 ? Color{ 255, 226, 168, 255 } : (blessed ? Color{ 255, 214, 142, 255 } : Color{ 244, 166, 84, 255 });
                shots.push_back(fx);
            }
            continue;
        }

        if (tower.type == TowerType::PilgrimBarricade) {
            int bestIndex = -1;
            float bestDist = effectiveRange;
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
                target.hp -= effectiveDamage;
                target.hitFlash = 0.12f;
                target.slowTimer = std::max(target.slowTimer, 0.8f + 0.25f * (float)tower.level + (blessed ? 0.35f : 0.0f) + 0.08f * (float)wardLevel);
                tower.cooldown = tower.maxCooldown;

                ShotFx fx{};
                fx.start = { tower.pos.x, 0.25f, tower.pos.z };
                fx.end = { target.pos.x, target.pos.y * 0.55f, target.pos.z };
                fx.life = 0.12f;
                fx.color = wardLevel > 0 ? Color{ 228, 208, 150, 255 } : (blessed ? Color{ 228, 190, 132, 255 } : Color{ 162, 102, 76, 255 });
                shots.push_back(fx);

                if (before > 0 && target.hp <= 0) RegisterEnemyKill(target);

                if (tower.level >= 2 || blessed || wardLevel >= 2) {
                    int splashDamage = std::max(1, effectiveDamage / 2 + wardLevel);
                    for (int i = 0; i < (int)enemies.size(); ++i) {
                        if (i == bestIndex || enemies[i].hp <= 0) continue;
                        if (DistanceXZ(target.pos, enemies[i].pos) <= 2.4f + 0.20f * (float)wardLevel) {
                            int splashBefore = enemies[i].hp;
                            enemies[i].hp -= splashDamage;
                            enemies[i].hitFlash = 0.08f;
                            if (splashBefore > 0 && enemies[i].hp <= 0) RegisterEnemyKill(enemies[i]);
                        }
                    }
                }
            }
            continue;
        }

        int bestIndex = -1;
        float bestScore = -10000.0f;
        for (int i = 0; i < (int)enemies.size(); ++i) {
            if (enemies[i].hp <= 0) continue;
            float dist = DistanceXZ(tower.pos, enemies[i].pos);
            if (dist > effectiveRange) continue;

            float score = 0.0f;
            score += enemies[i].pastGate ? 1200.0f : 0.0f;
            score += (float)enemies[i].pathIndex * 16.0f;
            score += (enemies[i].type == EnemyType::ProcessionBreaker) ? 40.0f : 0.0f;
            score += (enemies[i].type == EnemyType::DirgeHerald) ? 30.0f : 0.0f;
            score += (enemies[i].type == EnemyType::BannerKnight) ? 16.0f : 0.0f;
            score += (enemies[i].type == EnemyType::GraveBrute) ? 10.0f : 0.0f;
            score += (enemies[i].type == EnemyType::AshHound) ? 8.0f : 0.0f;
            score += enemies[i].elite ? 28.0f : 0.0f;
            score -= dist;
            if (score > bestScore) { bestScore = score; bestIndex = i; }
        }

        if (bestIndex >= 0) {
            Enemy& target = enemies[bestIndex];
            int before = target.hp;
            target.hp -= effectiveDamage;
            target.hitFlash = 0.12f;
            if (tower.type == TowerType::ReliquarySpire) {
                target.slowTimer = std::max(target.slowTimer, 1.5f + 0.25f * (float)tower.level + (blessed ? 0.35f : 0.0f) + 0.12f * (float)wardLevel);
            }
            tower.cooldown = tower.maxCooldown;

            ShotFx fx{};
            fx.start = { tower.pos.x, tower.pos.y + 1.8f, tower.pos.z };
            fx.end = { target.pos.x, target.pos.y + 0.4f, target.pos.z };
            fx.life = (tower.type == TowerType::ReliquarySpire) ? 0.17f : 0.10f;
            fx.color = wardLevel > 0 ? Color{ 250, 228, 172, 255 } : (blessed ? Color{ 248, 222, 156, 255 } : tower.color);
            shots.push_back(fx);

            if (before > 0 && target.hp <= 0) RegisterEnemyKill(target);

            if (tower.type == TowerType::WatchbowNest && tower.level >= 3) {
                int secondIndex = -1;
                float secondScore = -10000.0f;
                for (int i = 0; i < (int)enemies.size(); ++i) {
                    if (i == bestIndex || enemies[i].hp <= 0) continue;
                    float dist = DistanceXZ(tower.pos, enemies[i].pos);
                    if (dist > effectiveRange) continue;
                    float score = (float)enemies[i].pathIndex * 12.0f - dist + (enemies[i].elite ? 22.0f : 0.0f);
                    if (score > secondScore) {
                        secondScore = score;
                        secondIndex = i;
                    }
                }
                if (secondIndex >= 0) {
                    int chainDamage = std::max(1, (effectiveDamage * 3) / 5 + wardLevel);
                    Enemy& extra = enemies[secondIndex];
                    int extraBefore = extra.hp;
                    extra.hp -= chainDamage;
                    extra.hitFlash = 0.10f;
                    ShotFx extraFx{};
                    extraFx.start = { tower.pos.x, tower.pos.y + 1.8f, tower.pos.z };
                    extraFx.end = { extra.pos.x, extra.pos.y + 0.4f, extra.pos.z };
                    extraFx.life = 0.08f;
                    extraFx.color = wardLevel > 0 ? Color{ 255, 236, 194, 255 } : (blessed ? Color{ 255, 236, 188, 255 } : Color{ 218, 198, 132, 255 });
                    shots.push_back(extraFx);
                    if (extraBefore > 0 && extra.hp <= 0) RegisterEnemyKill(extra);
                }
            }
            else if (tower.type == TowerType::ReliquarySpire && tower.level >= 2) {
                int secondIndex = -1;
                float secondDist = 99999.0f;
                for (int i = 0; i < (int)enemies.size(); ++i) {
                    if (i == bestIndex || enemies[i].hp <= 0) continue;
                    float dist = DistanceXZ(target.pos, enemies[i].pos);
                    if (dist <= 4.2f + 0.30f * (float)wardLevel && dist < secondDist) {
                        secondDist = dist;
                        secondIndex = i;
                    }
                }
                if (secondIndex >= 0) {
                    int chainDamage = std::max(1, effectiveDamage / 2 + tower.level + wardLevel);
                    Enemy& extra = enemies[secondIndex];
                    int extraBefore = extra.hp;
                    extra.hp -= chainDamage;
                    extra.hitFlash = 0.10f;
                    extra.slowTimer = std::max(extra.slowTimer, 0.9f + 0.2f * (float)tower.level + 0.10f * (float)wardLevel);
                    ShotFx extraFx{};
                    extraFx.start = { target.pos.x, target.pos.y + 0.5f, target.pos.z };
                    extraFx.end = { extra.pos.x, extra.pos.y + 0.4f, extra.pos.z };
                    extraFx.life = 0.12f;
                    extraFx.color = wardLevel > 0 ? Color{ 214, 234, 255, 255 } : (blessed ? Color{ 224, 244, 255, 255 } : Color{ 122, 170, 236, 255 });
                    shots.push_back(extraFx);
                    if (extraBefore > 0 && extra.hp <= 0) RegisterEnemyKill(extra);
                }
            }
        }
    }
}

void Game::UpdateBastions(float dt) {
    int consecrated = GetConsecratedWaystoneCount();

    for (BastionSite& bastion : bastions) {
        if (bastion.level <= 0) continue;

        float fireSpeed = 1.0f + 0.08f * (float)legacy.rampartRank + (bastion.laneIndex == omenLane ? 0.18f : 0.0f) + (consecrated >= 3 ? 0.08f : 0.0f);
        bastion.cooldown -= dt * fireSpeed;
        if (bastion.cooldown > 0.0f) continue;

        Vector3 origin = grid.CellCenter(bastion.cell.x, bastion.cell.y);
        origin.y = 4.0f + 0.38f * (float)bastion.level;
        float range = 20.0f + 2.8f * (float)bastion.level + (bastion.laneIndex == 1 ? 1.5f : 0.0f);

        int bestIndex = -1;
        float bestScore = -10000.0f;
        for (int i = 0; i < (int)enemies.size(); ++i) {
            if (enemies[i].hp <= 0) continue;
            float dist = DistanceXZ(origin, enemies[i].pos);
            if (dist > range) continue;

            float score = 0.0f;
            score += (enemies[i].laneIndex == bastion.laneIndex) ? 90.0f : 0.0f;
            score += enemies[i].elite ? 36.0f : 0.0f;
            score += (enemies[i].type == EnemyType::ProcessionBreaker) ? 64.0f : 0.0f;
            score += (enemies[i].type == EnemyType::DirgeHerald) ? 46.0f : 0.0f;
            score += (enemies[i].type == EnemyType::AshHound) ? 10.0f : 0.0f;
            score += (float)enemies[i].pathIndex * 14.0f;
            score -= dist;
            if (score > bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }

        if (bestIndex < 0) {
            bastion.cooldown = 0.16f;
            continue;
        }

        Enemy& target = enemies[bestIndex];
        int damage = 9 + bastion.level * 6 + legacy.rampartRank * 2 + consecrated;
        if (bastion.laneIndex == omenLane) damage += 4;
        if (target.elite) damage += 2;

        int before = target.hp;
        target.hp -= damage;
        target.hitFlash = 0.14f;
        if (bastion.level >= 2) {
            target.slowTimer = std::max(target.slowTimer, 0.65f + 0.18f * (float)bastion.level);
        }

        ShotFx fx{};
        fx.start = origin;
        fx.end = { target.pos.x, target.pos.y + 0.6f, target.pos.z };
        fx.life = 0.14f;
        fx.color = (bastion.level >= 3) ? Color{ 255, 226, 166, 255 } : Color{ 214, 188, 138, 255 };
        shots.push_back(fx);

        if (before > 0 && target.hp <= 0) RegisterEnemyKill(target);

        if (bastion.level >= 3) {
            int splashDamage = std::max(2, damage / 2);
            for (int i = 0; i < (int)enemies.size(); ++i) {
                if (i == bestIndex || enemies[i].hp <= 0) continue;
                if (DistanceXZ(target.pos, enemies[i].pos) <= 2.8f) {
                    int splashBefore = enemies[i].hp;
                    enemies[i].hp -= splashDamage;
                    enemies[i].hitFlash = 0.08f;
                    if (splashBefore > 0 && enemies[i].hp <= 0) RegisterEnemyKill(enemies[i]);
                }
            }
        }

        bastion.cooldown = 1.85f - 0.24f * (float)bastion.level;
        if (bastion.cooldown < 0.76f) bastion.cooldown = 0.76f;
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

void Game::TriggerSanctumPulse() {
    if (state != PlayState::BattlePhase) return;

    int consecrated = GetConsecratedWaystoneCount();
    if (consecrated < 2 || fortress.coreHp <= 0) return;

    Vector3 corePos = grid.CellCenter(fortress.coreCell.x, fortress.coreCell.y);
    corePos.y = 0.8f;

    float radius = 18.0f + consecrated * 3.4f;
    int damage = 2 + consecrated + legacy.hymnRank;
    int harmed = 0;
    int slain = 0;

    sanctumPulseVisual = 1.30f;
    if (stormFlash < 0.16f) stormFlash = 0.16f;

    for (Enemy& enemy : enemies) {
        if (enemy.hp <= 0) continue;
        if (DistanceXZ(corePos, enemy.pos) <= radius) {
            int before = enemy.hp;
            enemy.hp -= damage;
            enemy.hitFlash = 0.06f;
            enemy.slowTimer = std::max(enemy.slowTimer, 1.05f + 0.15f * (float)consecrated);
            harmed++;
            if (before > 0 && enemy.hp <= 0) {
                RegisterEnemyKill(enemy);
                slain++;
            }
        }
    }

    GainFervor(3 + consecrated + legacy.hymnRank);
    if (slain > 0) announcement = TextFormat("SANCTUM BELL RESOUNDS // %d HERETICS BROKEN", slain);
    else if (harmed > 0) announcement = "SANCTUM BELL RESOUNDS";
    else announcement = "THE SANCTUM GATHERS LIGHT";
    announcementTimer = harmed > 0 ? 0.9f : 0.7f;
}

void Game::TryPlaceTower() {
    if (!hoveredValid || !grid.InBounds(hoveredCell.x, hoveredCell.y)) return;

    GridTile& tile = grid.At(hoveredCell.x, hoveredCell.y);
    if (FindWaystoneIndexAtCell(hoveredCell.x, hoveredCell.y) >= 0) {
        announcement = "WAYSTONE CANNOT BE BUILT OVER";
        announcementTimer = 1.1f;
        return;
    }
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
    SaveSuspendedRun();
    hasSuspendedChronicle = true;
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
    SaveSuspendedRun();
    hasSuspendedChronicle = true;
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
    SaveSuspendedRun();
    hasSuspendedChronicle = true;
    announcement = "DEFENSE DISMANTLED";
    announcementTimer = 1.2f;
}

void Game::TryConsecrateWaystone() {
    if (!hoveredValid) {
        announcement = "HOVER A WAYSTONE";
        announcementTimer = 1.0f;
        return;
    }

    int index = FindWaystoneIndexAtCell(hoveredCell.x, hoveredCell.y);
    if (index < 0) {
        announcement = "HOVER A WAYSTONE";
        announcementTimer = 1.0f;
        return;
    }

    WaystoneSite& stone = waystones[index];
    if (stone.consecrated) {
        announcement = "WAYSTONE ALREADY CONSECRATED";
        announcementTimer = 1.0f;
        return;
    }

    const int ironCost = 8;
    const int emberCost = 14;
    if (iron < ironCost || ember < emberCost) {
        announcement = "NEED 8 IRON AND 14 EMBER";
        announcementTimer = 1.0f;
        return;
    }

    iron -= ironCost;
    ember -= emberCost;
    stone.consecrated = true;
    legacy.totalWaystonesConsecrated++;
    SaveLegacyProfile();
    fervor += 20 + legacy.hymnRank * 2;
    if (fervor > fervorMax) fervor = fervorMax;
    SaveSuspendedRun();
    hasSuspendedChronicle = true;
    announcement = "WAYSTONE CONSECRATED";
    announcementTimer = 1.4f;
}

void Game::TryFortifyBastion() {
    if (!hoveredValid) {
        announcement = "HOVER A BASTION SOCKET";
        announcementTimer = 1.0f;
        return;
    }

    int index = FindBastionIndexAtCell(hoveredCell.x, hoveredCell.y);
    if (index < 0) {
        announcement = "HOVER A BASTION SOCKET";
        announcementTimer = 1.0f;
        return;
    }

    BastionSite& bastion = bastions[index];
    if (bastion.level >= 3) {
        announcement = "BASTION AT MAX RANK";
        announcementTimer = 1.0f;
        return;
    }

    int goldCost = GetBastionUpgradeGoldCost(bastion);
    int ironCost = GetBastionUpgradeIronCost(bastion);
    int emberCost = GetBastionUpgradeEmberCost(bastion);
    if (gold < goldCost || iron < ironCost || ember < emberCost) {
        announcement = TextFormat("NEED G%d I%d E%d", goldCost, ironCost, emberCost);
        announcementTimer = 1.0f;
        return;
    }

    gold -= goldCost;
    iron -= ironCost;
    ember -= emberCost;
    bastion.level++;
    bastion.cooldown = 0.0f;
    fortress.gateHp += 8 + bastion.level * 3;
    if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
    GainFervor(5 + bastion.level * 2 + legacy.hymnRank);
    SaveSuspendedRun();
    hasSuspendedChronicle = true;
    announcement = TextFormat("%s FORTIFIED TO LVL %d", GetBastionLabel(bastion.laneIndex), bastion.level);
    announcementTimer = 1.5f;
}

void Game::DamageGate(int amount) {
    bool directiveBroken = false;
    if (state == PlayState::BattlePhase) {
        waveGateDamageTaken += amount;
        if (directive.type == RoyalDirectiveType::HoldGate && !directive.failed && !directive.completed) {
            directive.failed = true;
            directiveBroken = true;
        }
    }

    fortress.gateHp -= amount;
    if (fortress.gateHp < 0) fortress.gateHp = 0;
    if (stormFlash < (amount >= 20 ? 0.28f : 0.10f)) stormFlash = (amount >= 20 ? 0.28f : 0.10f);
    announcement = directiveBroken ? "DIRECTIVE FAILED // THE GATE WAS STRUCK" : TextFormat("FRONT GATE STRUCK // %d HP", fortress.gateHp);
    announcementTimer = directiveBroken ? 1.0f : 0.75f;
}

void Game::DamageCore(int amount) {
    bool directiveBroken = false;
    if (state == PlayState::BattlePhase) {
        waveCoreDamageTaken += amount;
        if (directive.type == RoyalDirectiveType::HoldCore && !directive.failed && !directive.completed) {
            directive.failed = true;
            directiveBroken = true;
        }
    }

    fortress.coreHp -= amount;
    if (fortress.coreHp < 0) fortress.coreHp = 0;
    if (stormFlash < 0.34f) stormFlash = 0.34f;
    announcement = directiveBroken ? "DIRECTIVE FAILED // THE SANCTUM WAS BREACHED" : TextFormat("HOLY CORE STRUCK // %d HP", fortress.coreHp);
    announcementTimer = directiveBroken ? 1.1f : 1.0f;
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
    hymnTimer = GetWarHymnDuration();
    sanctumPulseVisual = std::max(sanctumPulseVisual, 0.34f);
    if (stormFlash < 0.18f) stormFlash = 0.18f;
    announcement = "WAR HYMN AWAKENED";
    announcementTimer = 1.6f;
}

void Game::RegisterEnemyKill(const Enemy& enemy) {
    if (state == PlayState::BattlePhase && !directive.failed && !directive.completed) {
        bool trackedKill = false;
        if (directive.type == RoyalDirectiveType::SlayElites && enemy.elite) trackedKill = true;
        else if (directive.type == RoyalDirectiveType::SilenceHeralds && enemy.type == EnemyType::DirgeHerald) trackedKill = true;
        else if (directive.type == RoyalDirectiveType::BreakBreakers && enemy.type == EnemyType::ProcessionBreaker) trackedKill = true;
        else if (directive.type == RoyalDirectiveType::HuntHounds && enemy.type == EnemyType::AshHound) trackedKill = true;

        if (trackedKill) {
            directive.progress++;
            if (directive.progress >= directive.target) {
                directive.progress = directive.target;
                directive.completed = true;
                announcement = TextFormat("ROYAL DIRECTIVE FULFILLED // %s", directive.title.c_str());
                announcementTimer = 1.5f;
            }
        }
    }

    int goldReward = 4;
    int emberReward = 0;
    int fervorReward = 8 + legacy.hymnRank;

    if (enemy.type == EnemyType::ProcessionBreaker) {
        goldReward = 36;
        emberReward = 6 + legacy.emberkeepRank;
        fervorReward = 36 + legacy.hymnRank * 2;
        legacy.breakersSlain++;
        AwardLegacyAsh(6 + wave.number / 2);
        if (fortress.gateHp > 0) {
            fortress.gateHp += 10 + legacy.rampartRank * 2;
            if (fortress.gateHp > fortress.gateMaxHp) fortress.gateHp = fortress.gateMaxHp;
        }
    }
    else if (enemy.type == EnemyType::DirgeHerald) {
        goldReward = 14;
        emberReward = 3 + legacy.emberkeepRank / 2;
        fervorReward = 20 + legacy.hymnRank;
    }
    else if (enemy.type == EnemyType::BannerKnight) {
        goldReward = 12;
        emberReward = 2 + legacy.emberkeepRank / 2;
        fervorReward = 18 + legacy.hymnRank;
    }
    else if (enemy.type == EnemyType::GraveBrute) {
        goldReward = 8;
        emberReward = 1 + legacy.emberkeepRank / 2;
        fervorReward = 14 + legacy.hymnRank;
    }
    else if (enemy.type == EnemyType::AshHound) {
        goldReward = 3;
        emberReward = 0;
        fervorReward = 6 + legacy.hymnRank;
    }

    if (enemy.elite) {
        goldReward += 8 + wave.number / 3;
        emberReward += 1 + legacy.emberkeepRank / 2;
        fervorReward += 10 + legacy.hymnRank;
        AwardLegacyAsh(3 + wave.number / 4);
    }

    gold += goldReward;
    ember += emberReward;
    GainFervor(fervorReward);

    Color burstColor = { 186, 96, 84, 255 };
    if (enemy.type == EnemyType::AshHound) burstColor = { 144, 126, 96, 255 };
    if (enemy.type == EnemyType::GraveBrute) burstColor = { 120, 92, 150, 255 };
    if (enemy.type == EnemyType::BannerKnight) burstColor = { 188, 154, 92, 255 };
    if (enemy.type == EnemyType::DirgeHerald) burstColor = { 92, 146, 172, 255 };
    if (enemy.type == EnemyType::ProcessionBreaker) burstColor = { 222, 94, 94, 255 };
    if (enemy.elite) burstColor = Tint(burstColor, 1.28f);

    int pieces = 5;
    if (enemy.type == EnemyType::AshHound) pieces = 4;
    if (enemy.type == EnemyType::GraveBrute) pieces = 7;
    if (enemy.type == EnemyType::DirgeHerald) pieces = 8;
    if (enemy.type == EnemyType::ProcessionBreaker) pieces = 12;
    if (enemy.elite) pieces += 4;

    for (int i = 0; i < pieces; ++i) {
        float angle = ((float)i / (float)pieces) * 6.2831853f;
        float speed = 1.2f + 0.22f * (float)i + (enemy.elite ? 0.25f : 0.0f);
        DeathFx fx{};
        fx.pos = { enemy.pos.x, enemy.pos.y + 0.25f, enemy.pos.z };
        fx.vel = { std::cos(angle) * speed, 1.2f + 0.08f * (float)i, std::sin(angle) * speed };
        fx.life = 0.55f + 0.04f * (float)i;
        fx.maxLife = fx.life;
        fx.size = (enemy.type == EnemyType::ProcessionBreaker) ? 0.22f : 0.14f;
        if (enemy.type == EnemyType::DirgeHerald) fx.size = 0.16f;
        if (enemy.elite) fx.size += 0.05f;
        fx.color = burstColor;
        deathFx.push_back(fx);
    }

    if (enemy.type == EnemyType::ProcessionBreaker) {
        announcement = enemy.elite ? "THE ELITE BREAKER HAS FALLEN" : "THE BREAKER HAS FALLEN";
        announcementTimer = 1.8f;
    }
    else if (enemy.type == EnemyType::DirgeHerald) {
        announcement = enemy.elite ? "THE ELITE HERALD HAS BEEN SILENCED" : "THE DIRGE HERALD IS SILENT";
        announcementTimer = 1.0f;
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

int Game::FindWaystoneIndexAtCell(int x, int y) const {
    for (int i = 0; i < (int)waystones.size(); ++i) {
        if (waystones[i].cell.x == x && waystones[i].cell.y == y) return i;
    }
    return -1;
}

int Game::FindBastionIndexAtCell(int x, int y) const {
    for (int i = 0; i < (int)bastions.size(); ++i) {
        if (bastions[i].cell.x == x && bastions[i].cell.y == y) return i;
    }
    return -1;
}

int Game::GetTowerBastionWardLevel(const Tower& tower) const {
    int best = 0;
    for (const BastionSite& bastion : bastions) {
        if (bastion.level <= 0) continue;
        Vector3 center = grid.CellCenter(bastion.cell.x, bastion.cell.y);
        float radius = grid.cellSize * (2.6f + 0.38f * (float)bastion.level);
        if (DistanceXZ(center, tower.pos) <= radius && bastion.level > best) {
            best = bastion.level;
        }
    }
    return best;
}

bool Game::IsTowerBlessed(const Tower& tower) const {
    for (const WaystoneSite& stone : waystones) {
        if (!stone.consecrated) continue;
        Vector3 center = grid.CellCenter(stone.cell.x, stone.cell.y);
        if (DistanceXZ(center, tower.pos) <= grid.cellSize * 3.25f) {
            return true;
        }
    }
    return false;
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

    tower.damage += legacy.arsenalRank;
    tower.range += 0.18f * (float)legacy.arsenalRank;
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
    case EnemyType::AshHound: return "ASH HOUND";
    case EnemyType::GraveBrute: return "GRAVE BRUTE";
    case EnemyType::BannerKnight: return "BANNER KNIGHT";
    case EnemyType::DirgeHerald: return "DIRGE HERALD";
    case EnemyType::ProcessionBreaker: return "PROCESSION BREAKER";
    }
    return "ASH RAIDER";
}

const char* Game::GetBastionLabel(int laneIndex) const {
    if (laneIndex == 0) return "NORTH RAMPART";
    if (laneIndex == 1) return "GATEHOUSE GREATBOW";
    return "SOUTH RAMPART";
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

int Game::GetConsecratedWaystoneCount() const {
    int count = 0;
    for (const WaystoneSite& stone : waystones) {
        if (stone.consecrated) count++;
    }
    return count;
}

float Game::GetWarHymnDuration() const {
    return 6.0f + 0.9f * (float)legacy.hymnRank;
}

int Game::GetBastionUpgradeGoldCost(const BastionSite& bastion) const {
    return 18 + bastion.level * 18 + bastion.laneIndex * 3;
}

int Game::GetBastionUpgradeIronCost(const BastionSite& bastion) const {
    return 10 + bastion.level * 8;
}

int Game::GetBastionUpgradeEmberCost(const BastionSite& bastion) const {
    return bastion.level >= 2 ? 5 + bastion.laneIndex * 2 : 0;
}

std::string Game::GetDirectiveProgressText() const {
    if (directive.type == RoyalDirectiveType::HoldGate) {
        if (directive.failed) return std::string("FAILED // gate took ") + std::to_string(waveGateDamageTaken) + " damage";
        return (waveGateDamageTaken <= 0) ? "Gate remains unbroken." : "Gate must take no more damage.";
    }
    if (directive.type == RoyalDirectiveType::HoldCore) {
        if (directive.failed) return std::string("FAILED // sanctum took ") + std::to_string(waveCoreDamageTaken) + " damage";
        return (waveCoreDamageTaken <= 0) ? "Holy Core remains untouched." : "Holy Core must take no more damage.";
    }

    std::string progress = std::to_string(directive.progress) + "/" + std::to_string(directive.target);
    if (directive.completed) progress += " // FULFILLED";
    else if (directive.failed) progress += " // FAILED";

    if (directive.type == RoyalDirectiveType::SlayElites) return std::string("Elite kills ") + progress;
    if (directive.type == RoyalDirectiveType::SilenceHeralds) return std::string("Heralds silenced ") + progress;
    if (directive.type == RoyalDirectiveType::BreakBreakers) return std::string("Breakers slain ") + progress;
    return std::string("Hounds culled ") + progress;
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

    Vector3 mapCenter = {
        grid.origin.x + grid.width * grid.cellSize * 0.5f,
        0.0f,
        grid.origin.z + grid.height * grid.cellSize * 0.5f
    };
    float outerW = grid.width * grid.cellSize + 120.0f;
    float outerH = grid.height * grid.cellSize + 120.0f;
    float midW = grid.width * grid.cellSize + 58.0f;
    float midH = grid.height * grid.cellSize + 58.0f;
    float nearW = grid.width * grid.cellSize + 18.0f;
    float nearH = grid.height * grid.cellSize + 18.0f;

    DrawPlane({ mapCenter.x, -0.28f, mapCenter.z }, { outerW, outerH }, { 18, 22, 28, 255 });
    DrawPlane({ mapCenter.x, -0.12f, mapCenter.z }, { midW, midH }, { 28, 32, 36, 255 });
    DrawPlane({ mapCenter.x, -0.04f, mapCenter.z }, { nearW, nearH }, { 38, 44, 40, 255 });

    DrawTiles();
    DrawEnvironment();
    DrawFortress();
    DrawTowers();
    DrawEnemies();
    DrawEffects();

    EndMode3D();
}


void Game::DrawTiles() const {
    auto isRoadAdjacent = [&](int x, int y) {
        for (int oy = -1; oy <= 1; ++oy) {
            for (int ox = -1; ox <= 1; ++ox) {
                if (ox == 0 && oy == 0) continue;
                int nx = x + ox;
                int ny = y + oy;
                if (!grid.InBounds(nx, ny)) continue;
                TileKind kind = grid.At(nx, ny).kind;
                if (kind == TileKind::Road || kind == TileKind::Spawn) return true;
            }
        }
        return false;
        };

    const int shrineCx = 30;
    const int shrineCy = 23;

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            const GridTile& tile = grid.At(x, y);
            Vector3 center = grid.CellCenter(x, y);
            center.y = tile.height * 0.5f - 0.05f;

            bool roadAdjacent = isRoadAdjacent(x, y);
            bool borderBand = (x < 4 || y < 4 || x >= grid.width - 4 || y >= grid.height - 4);
            bool shrineCourt = (std::abs(x - shrineCx) <= 6 && std::abs(y - shrineCy) <= 5);
            bool highland = tile.height >= 0.78f;
            bool midland = tile.height >= 0.46f;
            int hash = (x * 67 + y * 97 + x * y * 11) & 255;

            Color tileColor = { 76, 86, 74, 255 };
            Color topColor = { 98, 112, 96, 255 };

            if (y <= 14) {
                tileColor = { 74, 86, 76, 255 };
                topColor = { 94, 108, 98, 255 };
            }
            else if (y >= 31) {
                tileColor = { 80, 84, 72, 255 };
                topColor = { 104, 110, 90, 255 };
            }
            else {
                tileColor = { 82, 90, 76, 255 };
                topColor = { 104, 116, 92, 255 };
            }

            if (shrineCourt && tile.kind == TileKind::Buildable) {
                tileColor = { 78, 84, 82, 255 };
                topColor = { 110, 114, 110, 255 };
            }
            if (roadAdjacent && tile.kind == TileKind::Buildable) {
                topColor = Tint(topColor, 0.95f);
            }
            if (midland && tile.kind == TileKind::Buildable) {
                topColor = Tint(topColor, 1.03f);
            }
            if (borderBand && tile.kind == TileKind::Buildable) {
                tileColor = { 66, 72, 70, 255 };
                topColor = { 90, 96, 94, 255 };
            }

            if (tile.kind == TileKind::Road) {
                tileColor = { 82, 70, 52, 255 };
                topColor = { 142, 124, 90, 255 };
                if (state == PlayState::BattlePhase && omenLane >= 0) {
                    for (const GridCoord& step : lanes[omenLane]) {
                        if (step.x == x && step.y == y) {
                            tileColor = { 92, 62, 54, 255 };
                            topColor = { 176, 118, 96, 255 };
                            break;
                        }
                    }
                }
            }
            else if (tile.kind == TileKind::Spawn) {
                tileColor = { 76, 42, 42, 255 };
                topColor = { 150, 78, 64, 255 };
            }
            else if (tile.kind == TileKind::Fortress) {
                tileColor = { 90, 96, 108, 255 };
                topColor = { 128, 134, 146, 255 };
            }
            else if (tile.kind == TileKind::Blocked) {
                if (highland || borderBand) {
                    tileColor = { 56, 60, 64, 255 };
                    topColor = { 92, 96, 102, 255 };
                }
                else if (shrineCourt) {
                    tileColor = { 74, 78, 84, 255 };
                    topColor = { 112, 116, 122, 255 };
                }
                else {
                    tileColor = { 62, 66, 68, 255 };
                    topColor = { 94, 98, 100, 255 };
                }
            }

            if (hoveredValid && hoveredCell.x == x && hoveredCell.y == y) {
                int hoveredWaystone = FindWaystoneIndexAtCell(x, y);
                int hoveredBastion = FindBastionIndexAtCell(x, y);
                if (hoveredBastion >= 0) {
                    topColor = { 208, 176, 116, 255 };
                }
                else if (hoveredWaystone >= 0) {
                    topColor = { 168, 188, 220, 255 };
                }
                else if (hoveredTowerIndex >= 0) {
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
            DrawCube({ center.x, center.y + tile.height * 0.5f - 0.02f, center.z }, grid.cellSize * 0.90f, 0.06f, grid.cellSize * 0.90f, topColor);
            DrawCubeWires(center, grid.cellSize * 0.98f, tile.height, grid.cellSize * 0.98f, { 28, 32, 30, 255 });

            if (tile.kind == TileKind::Road || tile.kind == TileKind::Spawn) {
                DrawCube({ center.x, center.y + tile.height * 0.5f + 0.02f, center.z }, grid.cellSize * 0.48f, 0.04f, grid.cellSize * 0.82f, { 164, 146, 110, 255 });
                DrawCube({ center.x - grid.cellSize * 0.34f, center.y + tile.height * 0.5f, center.z }, 0.10f, 0.10f, grid.cellSize * 0.72f, { 70, 60, 46, 255 });
                DrawCube({ center.x + grid.cellSize * 0.34f, center.y + tile.height * 0.5f, center.z }, 0.10f, 0.10f, grid.cellSize * 0.72f, { 70, 60, 46, 255 });
                if (((x + y) % 6) == 0) {
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.05f, center.z }, grid.cellSize * 0.16f, 0.05f, grid.cellSize * 0.16f, { 194, 180, 134, 255 });
                }
            }
            else if (tile.kind == TileKind::Fortress) {
                if ((x + y) % 2 == 0) {
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.03f, center.z }, grid.cellSize * 0.36f, 0.04f, grid.cellSize * 0.36f, { 158, 164, 176, 255 });
                }
                else {
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.03f, center.z }, grid.cellSize * 0.72f, 0.03f, grid.cellSize * 0.08f, { 148, 154, 166, 255 });
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.03f, center.z }, grid.cellSize * 0.08f, 0.03f, grid.cellSize * 0.72f, { 148, 154, 166, 255 });
                }
            }
            else if (tile.kind == TileKind::Blocked) {
                if (highland || borderBand) {
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.12f, center.z }, grid.cellSize * 0.62f, 0.22f, grid.cellSize * 0.62f, { 118, 120, 126, 255 });
                    DrawCube({ center.x + 0.30f, center.y + tile.height * 0.5f + 0.22f, center.z - 0.18f }, 0.38f, 0.16f, 0.34f, { 82, 84, 88, 255 });
                }
                else if (shrineCourt) {
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.02f, center.z }, grid.cellSize * 0.72f, 0.05f, grid.cellSize * 0.72f, { 136, 140, 146, 255 });
                }
            }
            else if (tile.kind == TileKind::Buildable && !tile.occupied) {
                if (shrineCourt && (hash % 5 == 0)) {
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.02f, center.z }, 0.84f, 0.03f, 0.28f, { 118, 122, 120, 255 });
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.02f, center.z }, 0.28f, 0.03f, 0.84f, { 118, 122, 120, 255 });
                }
                else if (!roadAdjacent && hash % 31 == 0) {
                    DrawCube({ center.x - 0.20f, center.y + tile.height * 0.5f + 0.04f, center.z - 0.10f }, 0.26f, 0.08f, 0.18f, { 102, 96, 88, 255 });
                    DrawCube({ center.x + 0.16f, center.y + tile.height * 0.5f + 0.03f, center.z + 0.12f }, 0.18f, 0.06f, 0.16f, { 86, 82, 78, 255 });
                }
                else if (hash % 47 == 7) {
                    DrawCube({ center.x, center.y + tile.height * 0.5f + 0.02f, center.z }, 0.62f, 0.02f, 0.18f, { 62, 66, 60, 255 });
                }
            }
        }
    }
}


void Game::DrawEnvironment() const {
    float cs = grid.cellSize;
    Vector3 mapCenter = {
        grid.origin.x + grid.width * cs * 0.5f,
        0.0f,
        grid.origin.z + grid.height * cs * 0.5f
    };
    Color outerRock = stormFlash > 0.08f ? Color{ 56, 64, 80, 255 } : Color{ 36, 40, 46, 255 };

    auto drawEscarpment = [&](float x, float y, float z, float sx, float sy, float sz, Color color) {
        DrawCube({ x, y, z }, sx, sy, sz, color);
        DrawCube({ x, y + sy * 0.42f, z }, sx * 0.74f, sy * 0.24f, sz * 0.74f, Tint(color, 1.08f));
        DrawCubeWires({ x, y, z }, sx, sy, sz, Tint(color, 0.74f));
        };

    auto drawMountain = [&](float x, float z, float scale, Color rock) {
        DrawCylinder({ x, 2.2f * scale, z }, 6.2f * scale, 2.2f * scale, 4.4f * scale, 10, rock);
        DrawCylinder({ x, 4.8f * scale, z }, 3.0f * scale, 0.8f * scale, 3.2f * scale, 10, Tint(rock, 1.10f));
        DrawCylinder({ x, 6.7f * scale, z }, 1.5f * scale, 0.2f * scale, 1.8f * scale, 10, Tint(rock, 1.22f));
        };

    auto drawRoadArch = [&](int laneIndex, int cellX, int cellY, Color cloth) {
        if (!grid.InBounds(cellX, cellY)) return;
        bool omen = (laneIndex == omenLane);
        Vector3 c = grid.CellCenter(cellX, cellY);
        float h = grid.At(cellX, cellY).height;
        Color archStone = omen ? Color{ 126, 112, 116, 255 } : Color{ 96, 100, 110, 255 };
        Color banner = omen ? Color{ 176, 70, 66, 255 } : cloth;
        DrawCube({ c.x - 1.10f, h + 1.60f, c.z }, 0.34f, 3.0f, 0.34f, archStone);
        DrawCube({ c.x + 1.10f, h + 1.60f, c.z }, 0.34f, 3.0f, 0.34f, archStone);
        DrawCube({ c.x, h + 3.02f, c.z }, 2.55f, 0.28f, 0.44f, Tint(archStone, 1.14f));
        DrawCube({ c.x + 0.10f, h + 2.30f, c.z }, 0.12f, 1.00f, 1.74f, banner);
        if (omen) {
            float beacon = 0.20f + 0.08f * std::sin(worldTime * 5.2f + (float)laneIndex);
            DrawSphere({ c.x, h + 3.52f + beacon, c.z }, 0.22f + beacon * 0.55f, { 255, 164, 132, 255 });
        }
        };

    auto drawMinorShrine = [&](int cellX, int cellY, bool lit) {
        if (!grid.InBounds(cellX, cellY)) return;
        Vector3 c = grid.CellCenter(cellX, cellY);
        float h = grid.At(cellX, cellY).height;
        DrawCube({ c.x, h + 0.28f, c.z }, 1.24f, 0.46f, 1.24f, { 74, 78, 86, 255 });
        DrawCube({ c.x, h + 1.08f, c.z }, 0.44f, 1.18f, 0.44f, { 126, 130, 140, 255 });
        DrawCube({ c.x, h + 1.80f, c.z }, 0.78f, 0.22f, 0.78f, { 152, 156, 166, 255 });
        if (lit) {
            float pulse = 0.10f + 0.05f * std::sin(worldTime * 3.0f + (float)(cellX + cellY));
            DrawSphere({ c.x, h + 2.16f + pulse, c.z }, 0.18f + pulse * 0.25f, { 250, 206, 132, 255 });
        }
        };

    auto drawLowWall = [&](int cellX, int cellY, float sizeX, float sizeZ, Color color) {
        if (!grid.InBounds(cellX, cellY)) return;
        Vector3 c = grid.CellCenter(cellX, cellY);
        float h = grid.At(cellX, cellY).height;
        DrawCube({ c.x, h + 0.38f, c.z }, sizeX, 0.76f, sizeZ, color);
        DrawCubeWires({ c.x, h + 0.38f, c.z }, sizeX, 0.76f, sizeZ, Tint(color, 0.75f));
        };

    float west = grid.origin.x - cs * 1.7f;
    float east = grid.origin.x + grid.width * cs + cs * 1.7f;
    float north = grid.origin.z - cs * 1.7f;
    float south = grid.origin.z + grid.height * cs + cs * 1.7f;

    for (int i = 0; i < 8; ++i) {
        float tx = grid.origin.x + 6.0f + (float)i * (grid.width * cs - 12.0f) / 7.0f;
        float hA = 5.8f + 0.5f * (float)(i % 2);
        float hB = 5.4f + 0.5f * (float)((i + 1) % 2);
        drawEscarpment(tx, hA * 0.5f - 0.10f, north, 12.0f, hA, 6.0f, outerRock);
        drawEscarpment(tx, hB * 0.5f - 0.10f, south, 12.0f, hB, 6.4f, outerRock);
    }
    for (int i = 0; i < 6; ++i) {
        float tz = grid.origin.z + 8.0f + (float)i * (grid.height * cs - 16.0f) / 5.0f;
        float hA = 5.0f + 0.6f * (float)(i % 2);
        float hB = 5.6f + 0.4f * (float)((i + 1) % 2);
        drawEscarpment(west, hA * 0.5f - 0.10f, tz, 6.6f, hA, 12.0f, outerRock);
        drawEscarpment(east, hB * 0.5f - 0.10f, tz, 6.8f, hB, 12.0f, outerRock);
    }

    drawMountain(grid.origin.x - 10.0f, grid.origin.z - 12.0f, 1.05f, { 52, 56, 62, 255 });
    drawMountain(grid.origin.x - 12.0f, grid.origin.z + grid.height * cs + 12.0f, 1.00f, { 50, 54, 60, 255 });
    drawMountain(grid.origin.x + grid.width * cs + 12.0f, grid.origin.z - 10.0f, 1.04f, { 54, 58, 64, 255 });
    drawMountain(grid.origin.x + grid.width * cs + 14.0f, grid.origin.z + grid.height * cs + 10.0f, 1.08f, { 50, 54, 60, 255 });
    drawMountain(grid.origin.x + 18.0f, grid.origin.z - 18.0f, 0.92f, { 58, 60, 66, 255 });
    drawMountain(grid.origin.x + 30.0f, grid.origin.z + grid.height * cs + 18.0f, 0.90f, { 58, 60, 66, 255 });

    drawRoadArch(0, 11, 8, { 124, 42, 42, 255 });
    drawRoadArch(1, 11, 23, { 124, 42, 42, 255 });
    drawRoadArch(2, 11, 37, { 124, 42, 42, 255 });
    drawRoadArch(1, 46, 23, { 176, 134, 72, 255 });

    drawMinorShrine(20, 18, true);
    drawMinorShrine(20, 29, false);
    drawMinorShrine(41, 14, true);
    drawMinorShrine(39, 33, false);

    {
        Vector3 c = grid.CellCenter(30, 23);
        float h = grid.At(30, 23).height;
        float pulse = 0.10f * std::sin(worldTime * 2.6f) + (hymnTimer > 0.0f ? 0.10f : 0.0f) + stormFlash * 0.08f;
        Color stoneA = { 82, 88, 96, 255 };
        Color stoneB = { 118, 124, 134, 255 };

        DrawCylinder({ c.x, h + 0.16f, c.z }, 4.8f, 5.2f, 0.32f, 20, stoneA);
        DrawCylinder({ c.x, h + 0.54f, c.z }, 3.8f, 4.1f, 0.36f, 20, Tint(stoneA, 1.08f));
        DrawCylinder({ c.x, h + 1.08f, c.z }, 2.3f, 2.7f, 0.64f, 20, stoneB);
        DrawCube({ c.x - 3.2f, h + 0.28f, c.z }, 1.8f, 0.20f, 2.2f, { 122, 126, 132, 255 });
        DrawCube({ c.x + 3.2f, h + 0.28f, c.z }, 1.8f, 0.20f, 2.2f, { 122, 126, 132, 255 });
        DrawCube({ c.x, h + 0.28f, c.z - 3.2f }, 2.2f, 0.20f, 1.8f, { 122, 126, 132, 255 });
        DrawCube({ c.x, h + 0.28f, c.z + 3.2f }, 2.2f, 0.20f, 1.8f, { 122, 126, 132, 255 });

        float colX[4] = { -1.7f, 1.7f, -1.7f, 1.7f };
        float colZ[4] = { -1.7f, -1.7f, 1.7f, 1.7f };
        for (int i = 0; i < 4; ++i) {
            DrawCube({ c.x + colX[i], h + 2.28f, c.z + colZ[i] }, 0.52f, 2.56f, 0.52f, stoneB);
            DrawCube({ c.x + colX[i], h + 3.62f, c.z + colZ[i] }, 0.82f, 0.20f, 0.82f, Tint(stoneB, 1.10f));
            float flame = 0.10f + 0.05f * std::sin(worldTime * 4.0f + (float)i * 1.3f);
            DrawSphere({ c.x + colX[i], h + 4.02f + flame, c.z + colZ[i] }, 0.16f + flame * 0.35f, { 255, 204, 126, 255 });
        }

        DrawCube({ c.x, h + 2.10f, c.z }, 1.86f, 0.24f, 1.86f, { 168, 170, 176, 255 });
        DrawCube({ c.x, h + 2.78f, c.z }, 0.58f, 1.02f, 0.58f, { 196, 198, 206, 255 });
        DrawCube({ c.x, h + 3.40f, c.z }, 1.10f, 0.20f, 1.10f, { 214, 206, 186, 255 });
        DrawSphere({ c.x, h + 4.10f + pulse, c.z }, 0.34f + pulse * 0.70f, { 244, 222, 156, 255 });
        DrawSphere({ c.x, h + 4.36f + pulse * 1.4f, c.z }, 0.16f + pulse * 0.35f, { 255, 244, 204, 255 });
        DrawCircle3D({ c.x, h + 0.10f, c.z }, 5.2f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade({ 220, 206, 162, 255 }, 0.10f));
        if (sanctumPulseVisual > 0.0f) {
            DrawCircle3D({ c.x, h + 0.12f, c.z }, 5.6f + (1.30f - sanctumPulseVisual) * 4.2f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade({ 244, 224, 168, 255 }, 0.16f * sanctumPulseVisual));
        }
    }

    drawLowWall(15, 15, 2.8f, 0.34f, { 94, 96, 102, 255 });
    drawLowWall(18, 31, 2.6f, 0.34f, { 94, 96, 102, 255 });
    drawLowWall(36, 15, 0.34f, 2.4f, { 98, 100, 106, 255 });
    drawLowWall(37, 31, 0.34f, 2.4f, { 98, 100, 106, 255 });
    drawLowWall(43, 20, 2.0f, 0.34f, { 106, 98, 92, 255 });
    drawLowWall(43, 27, 2.0f, 0.34f, { 106, 98, 92, 255 });

    for (int i = 0; i < 26; ++i) {
        float orbit = worldTime * (0.20f + 0.015f * (float)i) + (float)i;
        Vector3 mote = {
            grid.origin.x + 6.0f + std::fmod(orbit * 6.4f + (float)(i * 9), grid.width * cs - 12.0f),
            1.5f + 0.7f * std::sin(orbit * 1.7f),
            grid.origin.z + 5.0f + std::fmod(orbit * 4.3f + (float)(i * 7), grid.height * cs - 10.0f)
        };
        Color moteColor = (stormFlash > 0.10f) ? Color{ 150, 166, 198, 170 } : Color{ 74, 82, 94, 150 };
        DrawSphere(mote, 0.08f + 0.02f * std::sin(orbit * 2.0f), moteColor);
    }

    Vector3 crowA = { mapCenter.x - 34.0f + 8.0f * std::sin(worldTime * 0.8f), 7.6f + 0.6f * std::sin(worldTime * 1.8f), mapCenter.z - 20.0f + 5.0f * std::cos(worldTime * 0.8f) };
    Vector3 crowB = { mapCenter.x + 16.0f + 7.0f * std::cos(worldTime * 0.7f), 6.8f + 0.5f * std::sin(worldTime * 1.6f + 1.5f), mapCenter.z + 18.0f + 4.0f * std::sin(worldTime * 0.7f) };
    Vector3 crowC = { mapCenter.x + 34.0f + 6.0f * std::sin(worldTime * 0.9f + 2.0f), 8.2f + 0.5f * std::cos(worldTime * 1.5f), mapCenter.z - 4.0f + 4.5f * std::cos(worldTime * 0.9f + 2.0f) };
    DrawSphere(crowA, 0.12f, { 18, 20, 24, 255 });
    DrawSphere(crowB, 0.12f, { 18, 20, 24, 255 });
    DrawSphere(crowC, 0.12f, { 18, 20, 24, 255 });

    if (stormFlash > 0.04f) {
        float lx[] = { mapCenter.x - 46.0f, mapCenter.x - 10.0f, mapCenter.x + 22.0f, mapCenter.x + 52.0f };
        float lz[] = { mapCenter.z - 34.0f, mapCenter.z + 26.0f, mapCenter.z - 10.0f, mapCenter.z + 34.0f };
        for (int i = 0; i < 4; ++i) {
            DrawCube({ lx[i], 9.4f, lz[i] }, 0.18f, 18.5f, 0.18f, Fade({ 214, 228, 255, 255 }, stormFlash * 0.62f));
            DrawSphere({ lx[i], 18.4f, lz[i] }, 0.36f, Fade({ 236, 242, 255, 255 }, stormFlash * 0.54f));
        }
    }

    for (const WaystoneSite& stone : waystones) {
        Vector3 center = grid.CellCenter(stone.cell.x, stone.cell.y);
        float ground = grid.At(stone.cell.x, stone.cell.y).height;
        Color base = stone.consecrated ? Color{ 166, 156, 126, 255 } : Color{ 92, 96, 102, 255 };
        Color glow = stone.consecrated ? Color{ 248, 214, 138, 255 } : Color{ 118, 132, 150, 255 };
        float pulse = stone.consecrated ? (0.12f * std::sin(worldTime * 3.0f + (float)stone.cell.x) + 0.12f) : 0.0f;

        DrawCube({ center.x, ground + 0.30f, center.z }, 1.10f, 0.50f, 1.10f, { 72, 76, 84, 255 });
        DrawCube({ center.x, ground + 1.18f, center.z }, 0.46f, 1.40f, 0.46f, base);
        DrawCube({ center.x, ground + 2.06f, center.z }, 0.74f, 0.34f, 0.74f, base);
        DrawSphere({ center.x, ground + 2.56f + pulse, center.z }, 0.22f + pulse * 0.28f, glow);
        if (stone.consecrated) {
            DrawCircle3D({ center.x, ground + 0.06f, center.z }, 4.8f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(glow, 0.12f));
            if (sanctumPulseVisual > 0.0f) {
                DrawCircle3D({ center.x, ground + 0.08f, center.z }, 5.0f + (1.3f - sanctumPulseVisual) * 3.0f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(glow, 0.14f * sanctumPulseVisual));
            }
        }
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
        else if (prop.type == PropType::PineTree) {
            DrawCylinder({ center.x, center.y + 0.74f, center.z }, 0.18f, 0.24f, 1.48f, 8, { 90, 68, 50, 255 });
            DrawCylinder({ center.x, center.y + 1.28f, center.z }, 0.88f, 0.18f, 1.24f, 8, { 52, 78, 54, 255 });
            DrawCylinder({ center.x, center.y + 1.92f, center.z }, 0.68f, 0.10f, 1.06f, 8, { 62, 90, 62, 255 });
            DrawCylinder({ center.x, center.y + 2.44f, center.z }, 0.46f, 0.04f, 0.82f, 8, { 74, 102, 72, 255 });
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
            DrawCylinder({ center.x, center.y + 1.85f, center.z }, 0.10f, 0.10f, 3.4f, 6, { 126, 126, 138, 255 });
            float sway = 0.18f * std::sin(worldTime * 2.4f + (float)prop.cell.x);
            DrawCube({ center.x + 0.56f, center.y + 2.70f, center.z + sway }, 0.96f, 1.18f, 0.10f, { 124, 42, 42, 255 });
        }
    }

    const int emberPools[][2] = { { 16, 12 }, { 23, 34 }, { 37, 13 }, { 44, 31 } };
    for (const auto& p : emberPools) {
        if (!grid.InBounds(p[0], p[1])) continue;
        Vector3 c = grid.CellCenter(p[0], p[1]);
        float h = grid.At(p[0], p[1]).height;
        DrawCylinder({ c.x, h + 0.02f, c.z }, 0.72f, 0.72f, 0.04f, 10, { 52, 44, 36, 255 });
        DrawSphere({ c.x, h + 0.08f, c.z }, 0.16f + 0.04f * std::sin(worldTime * 2.8f + (float)p[0]), { 168, 94, 70, 255 });
    }
}


void Game::DrawFortress() const {
    Vector3 gate = grid.CellCenter(fortress.gateCell.x, fortress.gateCell.y);
    Vector3 core = grid.CellCenter(fortress.coreCell.x, fortress.coreCell.y);
    Vector3 mid = { (gate.x + core.x) * 0.5f + 1.6f, 0.0f, core.z };

    DrawCube({ mid.x, 0.75f, mid.z }, 24.0f, 1.5f, 16.0f, { 70, 76, 84, 255 });
    DrawCube({ mid.x + 1.8f, 2.9f, mid.z }, 16.0f, 5.8f, 12.6f, stormFlash > 0.06f ? Color{ 154, 160, 176, 255 } : Color{ 144, 148, 156, 255 });
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

    for (const BastionSite& bastion : bastions) {
        Vector3 p = grid.CellCenter(bastion.cell.x, bastion.cell.y);
        float bodyY = 3.2f + 0.18f * (float)bastion.level;
        bool omen = (bastion.laneIndex == omenLane && state == PlayState::BattlePhase);
        Color stone = bastion.level > 0 ? Color{ 154, 146, 126, 255 } : Color{ 92, 96, 104, 255 };
        if (omen) stone = Tint(stone, 1.18f);
        DrawCube({ p.x, bodyY, p.z }, 1.9f, 2.6f + 0.26f * (float)bastion.level, 1.9f, stone);
        DrawCube({ p.x, bodyY + 1.5f + 0.12f * (float)bastion.level, p.z }, 2.5f, 0.26f, 2.5f, Tint(stone, 1.12f));
        DrawCube({ p.x + 0.42f, bodyY + 1.86f + 0.18f * (float)bastion.level, p.z }, 1.55f, 0.18f, 0.28f, bastion.level > 0 ? Color{ 174, 146, 102, 255 } : Color{ 112, 110, 112, 255 });
        DrawCube({ p.x - 0.18f, bodyY + 1.86f + 0.18f * (float)bastion.level, p.z }, 0.24f, 0.18f, 1.12f, bastion.level > 0 ? Color{ 174, 146, 102, 255 } : Color{ 112, 110, 112, 255 });
        DrawCube({ p.x - 0.66f, bodyY + 1.72f + 0.18f * (float)bastion.level, p.z }, 0.58f, 0.12f, 0.22f, bastion.level >= 2 ? Color{ 202, 178, 124, 255 } : Color{ 124, 120, 116, 255 });
        if (bastion.level > 0) {
            float flame = 0.12f + 0.05f * std::sin(worldTime * 4.4f + (float)bastion.laneIndex * 1.6f);
            DrawSphere({ p.x, bodyY + 2.20f + flame, p.z }, 0.16f + flame * 0.4f, omen ? Color{ 255, 176, 132, 255 } : Color{ 255, 214, 142, 255 });
            DrawCircle3D({ p.x, 0.10f, p.z }, 1.10f + 0.18f * (float)bastion.level, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(omen ? Color{ 255, 172, 126, 255 } : Color{ 244, 214, 144, 255 }, 0.10f));
        }
    }

    Vector3 lanterns[] = {
        { gate.x + 2.8f, 4.2f, gate.z - 3.2f }, { gate.x + 2.8f, 4.2f, gate.z + 3.2f },
        { core.x + 2.0f, 6.8f, core.z - 2.6f }, { core.x + 2.0f, 6.8f, core.z + 2.6f }
    };
    for (const Vector3& p : lanterns) {
        float flicker = 0.10f + 0.05f * std::sin(worldTime * 5.0f + p.x);
        DrawSphere({ p.x, p.y + flicker, p.z }, 0.20f + flicker * 0.45f, { 255, 194, 110, 255 });
    }

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
        DrawCylinder({ wagonA.x - 1.2f, 0.38f, wagonA.z + oz }, 0.42f, 0.42f, 0.10f, 10, { 78, 60, 44, 255 });
        DrawCylinder({ wagonA.x + 1.2f, 0.38f, wagonA.z + oz }, 0.42f, 0.42f, 0.10f, 10, { 78, 60, 44, 255 });
        DrawCylinder({ wagonB.x - 1.2f, 0.38f, wagonB.z + oz }, 0.42f, 0.42f, 0.10f, 10, { 78, 60, 44, 255 });
        DrawCylinder({ wagonB.x + 1.2f, 0.38f, wagonB.z + oz }, 0.42f, 0.42f, 0.10f, 10, { 78, 60, 44, 255 });
    }
    float wheelOffsetsC[2] = { -1.05f, 1.05f };
    for (float ox : wheelOffsetsC) {
        DrawCylinder({ wagonC.x + ox, 0.38f, wagonC.z - 1.12f }, 0.40f, 0.40f, 0.10f, 10, { 78, 60, 44, 255 });
        DrawCylinder({ wagonC.x + ox, 0.38f, wagonC.z + 1.12f }, 0.40f, 0.40f, 0.10f, 10, { 78, 60, 44, 255 });
    }

    float glowPulse = 0.08f * std::sin(worldTime * 3.0f) + (hymnTimer > 0.0f ? 0.14f : 0.0f) + stormFlash * 0.12f;
    DrawSphere({ core.x + 2.0f, 7.1f + glowPulse, core.z }, 0.92f + glowPulse, { 236, 202, 116, 255 });
    DrawSphere({ core.x + 2.0f, 7.1f + glowPulse * 1.4f, core.z }, 0.46f + glowPulse * 0.7f, { 255, 240, 178, 255 });

    if (sanctumPulseVisual > 0.0f) {
        float radiusA = 4.0f + (1.30f - sanctumPulseVisual) * 12.0f;
        float radiusB = 2.6f + (1.30f - sanctumPulseVisual) * 8.5f;
        DrawCircle3D({ core.x + 2.0f, 0.18f, core.z }, radiusA, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade({ 242, 220, 156, 255 }, 0.22f * sanctumPulseVisual));
        DrawCircle3D({ gate.x + 1.8f, 0.12f, gate.z }, radiusB, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade({ 212, 188, 132, 255 }, 0.18f * sanctumPulseVisual));
    }
}

void Game::DrawTowers() const {
    auto drawTowerModel = [&](const Tower& tower, float alpha, bool blessed, int wardLevel) {
        auto FC = [&](Color c) { return Fade(c, alpha); };
        float levelLift = 0.14f * (float)(tower.level - 1);

        DrawCylinder({ tower.pos.x, 0.10f, tower.pos.z }, 0.90f, 0.90f, 0.12f, 14, FC({ 44, 48, 54, 140 }));

        if (wardLevel > 0) {
            DrawCircle3D({ tower.pos.x, 0.07f, tower.pos.z }, 1.05f + 0.12f * (float)wardLevel, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade({ 222, 194, 132, 255 }, (0.08f + 0.02f * (float)wardLevel) * alpha));
            DrawSphere({ tower.pos.x, 0.22f, tower.pos.z }, 0.10f + 0.02f * (float)wardLevel, FC({ 230, 202, 146, 255 }));
        }
        if (blessed) {
            DrawCircle3D({ tower.pos.x, 0.06f, tower.pos.z }, tower.range + 0.90f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade({ 248, 214, 138, 255 }, 0.06f * alpha));
            DrawSphere({ tower.pos.x, 0.18f, tower.pos.z }, 0.10f, FC({ 248, 214, 138, 255 }));
        }

        if (tower.type == TowerType::WatchbowNest) {
            DrawCube({ tower.pos.x, 0.72f, tower.pos.z }, 1.20f, 1.25f, 1.20f, FC({ 108, 86, 66, 255 }));
            DrawCube({ tower.pos.x, 1.70f + levelLift, tower.pos.z }, 0.82f, 0.74f + levelLift, 0.82f, FC(wardLevel > 0 ? Color{ 184, 162, 120, 255 } : Color{ 166, 146, 112, 255 }));
            DrawCube({ tower.pos.x + 0.30f, 2.25f + levelLift, tower.pos.z }, 0.80f, 0.14f, 0.24f, FC({ 184, 166, 120, 255 }));
            DrawCube({ tower.pos.x - 0.18f, 2.25f + levelLift, tower.pos.z }, 0.26f, 0.14f, 0.82f, FC({ 184, 166, 120, 255 }));
        }
        else if (tower.type == TowerType::CenserShrine) {
            DrawCube({ tower.pos.x, 0.62f, tower.pos.z }, 1.20f, 1.02f, 1.20f, FC({ 108, 92, 80, 255 }));
            DrawCylinder({ tower.pos.x, 1.58f, tower.pos.z }, 0.34f, 0.42f, 1.52f + levelLift, 10, FC({ 166, 132, 88, 255 }));
            DrawSphere({ tower.pos.x, 2.58f + levelLift, tower.pos.z }, 0.34f + 0.06f * (float)(tower.level - 1), FC(wardLevel > 0 ? Color{ 255, 230, 170, 255 } : (blessed ? Color{ 255, 218, 150, 255 } : Color{ 244, 166, 84, 255 })));
            if (hymnTimer > 0.0f || blessed || wardLevel > 0) {
                DrawSphere({ tower.pos.x, 2.88f + levelLift, tower.pos.z }, 0.16f, FC({ 255, 228, 168, 220 }));
            }
        }
        else if (tower.type == TowerType::ReliquarySpire) {
            DrawCube({ tower.pos.x, 0.62f, tower.pos.z }, 1.08f, 1.02f, 1.08f, FC({ 86, 98, 122, 255 }));
            DrawCube({ tower.pos.x, 1.88f + levelLift, tower.pos.z }, 0.52f, 2.28f + levelLift, 0.52f, FC({ 108, 126, 160, 255 }));
            DrawSphere({ tower.pos.x, 3.12f + levelLift, tower.pos.z }, 0.28f + 0.05f * (float)(tower.level - 1), FC(wardLevel > 0 ? Color{ 220, 236, 255, 255 } : (blessed ? Color{ 194, 232, 255, 255 } : Color{ 122, 170, 236, 255 })));
            float orbit = worldTime * 2.4f;
            DrawSphere({ tower.pos.x + 0.28f * std::sin(orbit), 2.76f + levelLift, tower.pos.z + 0.28f * std::cos(orbit) }, 0.10f, FC({ 180, 214, 255, 255 }));
        }
        else {
            DrawCube({ tower.pos.x, 0.44f, tower.pos.z }, 1.34f, 0.74f, 0.58f, FC({ 118, 88, 64, 255 }));
            DrawCube({ tower.pos.x, 0.64f, tower.pos.z - 0.28f }, 1.24f, 1.06f + levelLift, 0.14f, FC({ 146, 104, 72, 255 }));
            DrawCube({ tower.pos.x, 0.64f, tower.pos.z + 0.28f }, 1.24f, 1.06f + levelLift, 0.14f, FC({ 146, 104, 72, 255 }));
            DrawCube({ tower.pos.x - 0.42f, 0.54f, tower.pos.z }, 0.16f, 0.86f + levelLift, 0.48f, FC({ 88, 64, 44, 255 }));
            DrawCube({ tower.pos.x + 0.42f, 0.54f, tower.pos.z }, 0.16f, 0.86f + levelLift, 0.48f, FC({ 88, 64, 44, 255 }));
            if (wardLevel > 0) {
                DrawSphere({ tower.pos.x, 1.18f + levelLift, tower.pos.z }, 0.12f, FC({ 226, 198, 132, 255 }));
            }
        }
        };

    for (int i = 0; i < (int)towers.size(); ++i) {
        const Tower& tower = towers[i];
        bool blessed = IsTowerBlessed(tower);
        int wardLevel = GetTowerBastionWardLevel(tower);
        drawTowerModel(tower, 1.0f, blessed, wardLevel);

        if (state == PlayState::BuildPhase && hoveredTowerIndex == i) {
            float previewRange = tower.range + (blessed ? 0.90f : 0.0f) + 0.34f * (float)wardLevel;
            for (int ring = 0; ring < tower.level; ++ring) {
                DrawCircle3D({ tower.pos.x, 0.04f + 0.01f * (float)ring, tower.pos.z }, previewRange - 0.05f * (float)ring, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(wardLevel > 0 ? Color{ 230, 208, 154, 255 } : (blessed ? Color{ 248, 214, 138, 255 } : tower.color), blessed ? 0.22f : 0.18f));
            }
        }
    }

    if (state == PlayState::BuildPhase && hoveredValid && hoveredTowerIndex < 0 && FindWaystoneIndexAtCell(hoveredCell.x, hoveredCell.y) < 0 && FindBastionIndexAtCell(hoveredCell.x, hoveredCell.y) < 0) {
        const GridTile& tile = grid.At(hoveredCell.x, hoveredCell.y);
        if (tile.kind == TileKind::Buildable && !tile.occupied) {
            Tower preview = MakeTower(buildChoice, hoveredCell.x, hoveredCell.y);
            bool blessed = IsTowerBlessed(preview);
            int wardLevel = GetTowerBastionWardLevel(preview);
            drawTowerModel(preview, 0.46f, blessed, wardLevel);
            float previewRange = preview.range + (blessed ? 0.90f : 0.0f) + 0.34f * (float)wardLevel;
            DrawCircle3D({ preview.pos.x, 0.04f, preview.pos.z }, previewRange, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(wardLevel > 0 ? Color{ 230, 208, 154, 255 } : (blessed ? Color{ 248, 214, 138, 255 } : preview.color), 0.16f));
        }
    }
}

void Game::DrawEnemies() const {
    for (const Enemy& enemy : enemies) {
        float scale = enemy.elite ? 1.16f : 1.0f;
        Color baseColor = { 170, 82, 76, 255 };
        if (enemy.type == EnemyType::AshHound) baseColor = { 132, 114, 86, 255 };
        else if (enemy.type == EnemyType::GraveBrute) baseColor = { 110, 88, 138, 255 };
        else if (enemy.type == EnemyType::BannerKnight) baseColor = { 170, 146, 88, 255 };
        else if (enemy.type == EnemyType::DirgeHerald) baseColor = { 96, 146, 176, 255 };
        else if (enemy.type == EnemyType::ProcessionBreaker) baseColor = { 178, 72, 72, 255 };
        if (enemy.laneIndex == omenLane && state == PlayState::BattlePhase) baseColor = Tint(baseColor, 1.12f);
        if (enemy.slowTimer > 0.0f) baseColor = Tint(baseColor, 1.10f);
        if (enemy.elite) baseColor = Tint(baseColor, 1.22f);
        Color color = enemy.hitFlash > 0.0f ? WHITE : baseColor;

        DrawCylinder({ enemy.pos.x, 0.08f, enemy.pos.z }, 0.60f * scale, 0.60f * scale, 0.08f, 12, enemy.elite ? Color{ 72, 56, 36, 180 } : Color{ 40, 42, 48, 140 });

        if (enemy.type == EnemyType::AshRaider) {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 0.86f * scale, 1.26f * scale, 0.78f * scale, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 0.84f * scale, enemy.pos.z }, 0.24f * scale, { 216, 188, 154, 255 });
            DrawCube({ enemy.pos.x + 0.26f * scale, enemy.pos.y + 0.30f * scale, enemy.pos.z }, 0.18f * scale, 0.80f * scale, 0.18f * scale, { 94, 72, 58, 255 });
        }
        else if (enemy.type == EnemyType::AshHound) {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 1.02f * scale, 0.56f * scale, 0.46f * scale, color);
            DrawCube({ enemy.pos.x - 0.36f * scale, enemy.pos.y + 0.18f * scale, enemy.pos.z }, 0.44f * scale, 0.34f * scale, 0.34f * scale, color);
            DrawCube({ enemy.pos.x + 0.28f * scale, enemy.pos.y + 0.18f * scale, enemy.pos.z }, 0.26f * scale, 0.20f * scale, 0.20f * scale, { 196, 176, 146, 255 });
            DrawCube({ enemy.pos.x + 0.18f * scale, enemy.pos.y - 0.10f * scale, enemy.pos.z - 0.16f * scale }, 0.10f * scale, 0.30f * scale, 0.10f * scale, { 82, 68, 54, 255 });
            DrawCube({ enemy.pos.x - 0.12f * scale, enemy.pos.y - 0.10f * scale, enemy.pos.z + 0.16f * scale }, 0.10f * scale, 0.30f * scale, 0.10f * scale, { 82, 68, 54, 255 });
        }
        else if (enemy.type == EnemyType::GraveBrute) {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 1.18f * scale, 1.72f * scale, 1.02f * scale, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 1.02f * scale, enemy.pos.z }, 0.28f * scale, { 204, 188, 194, 255 });
            DrawCube({ enemy.pos.x + 0.44f * scale, enemy.pos.y + 0.34f * scale, enemy.pos.z }, 0.22f * scale, 1.12f * scale, 0.22f * scale, { 70, 64, 78, 255 });
            DrawCube({ enemy.pos.x + 0.44f * scale, enemy.pos.y + 0.82f * scale, enemy.pos.z }, 0.52f * scale, 0.24f * scale, 0.24f * scale, { 116, 110, 130, 255 });
        }
        else if (enemy.type == EnemyType::BannerKnight) {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 0.96f * scale, 1.50f * scale, 0.86f * scale, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 0.98f * scale, enemy.pos.z }, 0.25f * scale, { 224, 206, 170, 255 });
            DrawCube({ enemy.pos.x + 0.34f * scale, enemy.pos.y + 0.64f * scale, enemy.pos.z }, 0.12f * scale, 1.30f * scale, 0.12f * scale, { 122, 122, 130, 255 });
            DrawCube({ enemy.pos.x + 0.70f * scale, enemy.pos.y + 1.10f * scale, enemy.pos.z }, 0.58f * scale, 0.54f * scale, 0.10f * scale, { 164, 122, 64, 255 });
        }
        else if (enemy.type == EnemyType::DirgeHerald) {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 0.78f * scale, 1.64f * scale, 0.78f * scale, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 1.08f * scale, enemy.pos.z }, 0.24f * scale, { 214, 214, 224, 255 });
            DrawCylinder({ enemy.pos.x + 0.40f * scale, enemy.pos.y + 0.62f * scale, enemy.pos.z }, 0.06f * scale, 0.06f * scale, 1.42f * scale, 6, { 126, 126, 138, 255 });
            DrawCube({ enemy.pos.x + 0.78f * scale, enemy.pos.y + 1.02f * scale, enemy.pos.z }, 0.60f * scale, 0.74f * scale, 0.08f * scale, { 128, 96, 144, 255 });
            float hymn = 0.18f * std::sin(worldTime * 4.0f + enemy.pos.x);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 1.54f * scale + hymn, enemy.pos.z }, 0.12f * scale, { 180, 222, 255, 255 });
        }
        else {
            DrawCube({ enemy.pos.x, enemy.pos.y, enemy.pos.z }, 1.60f * scale, 2.26f * scale, 1.34f * scale, color);
            DrawSphere({ enemy.pos.x, enemy.pos.y + 1.26f * scale, enemy.pos.z }, 0.34f * scale, { 196, 172, 172, 255 });
            DrawCube({ enemy.pos.x, enemy.pos.y + 1.42f * scale, enemy.pos.z }, 1.22f * scale, 0.36f * scale, 1.22f * scale, { 104, 46, 46, 255 });
            DrawCube({ enemy.pos.x + 0.62f * scale, enemy.pos.y + 0.42f * scale, enemy.pos.z }, 0.24f * scale, 1.30f * scale, 0.24f * scale, { 84, 64, 64, 255 });
            DrawCube({ enemy.pos.x + 0.62f * scale, enemy.pos.y + 1.00f * scale, enemy.pos.z }, 0.74f * scale, 0.30f * scale, 0.30f * scale, { 122, 84, 84, 255 });
        }

        if (enemy.elite) {
            float orbit = 0.18f * std::sin(worldTime * 4.0f + enemy.pos.x);
            DrawCircle3D({ enemy.pos.x, 0.10f, enemy.pos.z }, 0.82f * scale, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade({ 244, 214, 144, 255 }, 0.12f));
            DrawSphere({ enemy.pos.x, enemy.pos.y + 1.46f * scale + orbit, enemy.pos.z }, 0.14f * scale, { 244, 214, 144, 255 });
        }

        float hpRatio = (float)enemy.hp / (float)enemy.maxHp;
        if (hpRatio < 0.0f) hpRatio = 0.0f;
        float hpWidth = enemy.elite ? 1.42f : 1.18f;
        float hpY = enemy.pos.y + ((enemy.type == EnemyType::ProcessionBreaker) ? 1.80f * scale : (enemy.type == EnemyType::DirgeHerald ? 1.52f * scale : (enemy.type == EnemyType::GraveBrute ? 1.30f * scale : 1.12f * scale)));
        DrawCube({ enemy.pos.x, hpY, enemy.pos.z }, hpWidth, 0.10f, 0.16f, { 40, 10, 10, 255 });
        DrawCube({ enemy.pos.x - (hpWidth * (1.0f - hpRatio)) * 0.5f, hpY + 0.01f, enemy.pos.z }, hpWidth * hpRatio, 0.06f, 0.12f, enemy.elite ? Color{ 240, 206, 116, 255 } : Color{ 96, 220, 96, 255 });
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
    int consecratedWaystones = GetConsecratedWaystoneCount();
    int hoveredWaystone = hoveredValid ? FindWaystoneIndexAtCell(hoveredCell.x, hoveredCell.y) : -1;
    int hoveredBastion = hoveredValid ? FindBastionIndexAtCell(hoveredCell.x, hoveredCell.y) : -1;
    int raisedBastions = 0;
    for (const BastionSite& bastion : bastions) if (bastion.level > 0) raisedBastions++;
    int crownfireCost = 48 - legacy.hymnRank * 4 - consecratedWaystones * 2;
    if (crownfireCost < 28) crownfireCost = 28;

    int nextRaiders = 0, nextHounds = 0, nextBrutes = 0, nextKnights = 0, nextHeralds = 0, nextBreakers = 0, nextElites = 0;
    for (const SpawnEntry& entry : wave.spawns) {
        if (entry.type == EnemyType::AshRaider) nextRaiders++;
        else if (entry.type == EnemyType::AshHound) nextHounds++;
        else if (entry.type == EnemyType::GraveBrute) nextBrutes++;
        else if (entry.type == EnemyType::BannerKnight) nextKnights++;
        else if (entry.type == EnemyType::DirgeHerald) nextHeralds++;
        else if (entry.type == EnemyType::ProcessionBreaker) nextBreakers++;
        if (entry.elite) nextElites++;
    }

    std::string directiveProgress = GetDirectiveProgressText();

    DrawRectangle(22, 18, 1120, 214, Fade(BLACK, 0.68f));
    DrawRectangleLines(22, 18, 1120, 214, { 188, 156, 96, 255 });
    DrawText("THE LAST PROCESSION", 40, 30, 34, { 236, 228, 210, 255 });
    DrawText("BATCH 16 // GRAND SHRINE MAP AND BORDERLANDS", 40, 68, 20, { 196, 172, 118, 255 });
    DrawText(TextFormat("WAVE %d", wave.number), 40, 100, 24, { 188, 156, 96, 255 });
    DrawText(TextFormat("OMEN // %s", waveOmen.c_str()), 160, 100, 24, omenLane >= 0 ? Color{ 226, 136, 116, 255 } : Color{ 198, 208, 214, 255 });
    DrawText(TextFormat("GOLD %d   IRON %d   EMBER %d", gold, iron, ember), 40, 132, 22, { 210, 214, 204, 255 });
    DrawText(TextFormat("GATE %d / %d", fortress.gateHp, fortress.gateMaxHp), 330, 132, 22, fortress.gateHp > 0 ? Color{ 210, 176, 112, 255 } : Color{ 198, 76, 76, 255 });
    DrawText(TextFormat("HOLY CORE %d / %d", fortress.coreHp, fortress.coreMaxHp), 520, 132, 22, fortress.coreHp > 30 ? Color{ 128, 196, 136, 255 } : Color{ 198, 76, 76, 255 });
    DrawText(TextFormat("FERVOR %d / %d", fervor, fervorMax), 780, 132, 22, hymnTimer > 0.0f ? Color{ 250, 228, 164, 255 } : Color{ 168, 190, 216, 255 });
    DrawText(TextFormat("LEGACY ASH %d   RUN ASH +%d", legacy.ash, legacyAshEarnedThisRun), 40, 164, 22, { 214, 196, 142, 255 });
    DrawText(TextFormat("WAYSTONES %d / %d   BASTIONS %d / %d", consecratedWaystones, (int)waystones.size(), raisedBastions, (int)bastions.size()), 390, 164, 22, consecratedWaystones > 0 || raisedBastions > 0 ? Color{ 228, 208, 150, 255 } : Color{ 160, 170, 180, 255 });
    DrawText(TextFormat("DIRECTIVES %d   FLAWLESS %d", legacy.directivesCompleted, legacy.flawlessWaves), 780, 164, 22, { 196, 208, 214, 255 });
    DrawText(hasSuspendedChronicle ? "CHRONICLE READY // PRESS L TO RESTORE" : "NO CHRONICLE SAVED YET", 40, 190, 18, hasSuspendedChronicle ? Color{ 170, 198, 220, 255 } : Color{ 124, 132, 140, 255 });
    DrawText(TextFormat("CROWNFIRE Q // %d FERVOR", crownfireCost), 420, 190, 18, fervor >= crownfireCost ? Color{ 255, 220, 154, 255 } : Color{ 150, 154, 162, 255 });
    DrawText(TextFormat("DIRECTIVE // %s", directive.title.c_str()), 700, 190, 18, directive.failed ? Color{ 214, 104, 104, 255 } : (directive.completed ? Color{ 164, 220, 146, 255 } : Color{ 228, 208, 150, 255 }));

    DrawRectangle(screenW - 430, 18, 408, 278, Fade(BLACK, 0.72f));
    DrawRectangleLines(screenW - 430, 18, 408, 278, { 110, 126, 172, 255 });
    DrawText("FORTRESS ALTAR", screenW - 410, 34, 30, { 236, 228, 210, 255 });
    DrawText(TextFormat("BEST WAVE %d   BREAKERS %d", legacy.highestWave, legacy.breakersSlain), screenW - 410, 72, 18, { 198, 208, 214, 255 });
    DrawText(TextFormat("TOTAL WAYSTONES %d   RUNS %d", legacy.totalWaystonesConsecrated, legacy.runsStarted), screenW - 410, 94, 18, { 198, 208, 214, 255 });
    DrawText(TextFormat("DIRECTIVES %d   FLAWLESS %d", legacy.directivesCompleted, legacy.flawlessWaves), screenW - 410, 116, 18, { 214, 196, 142, 255 });
    DrawText(TextFormat("5 %s  R%d/%d  COST %d", GetLegacyUpgradeLabel(0), legacy.rampartRank, GetLegacyUpgradeMaxRank(0), GetLegacyUpgradeCost(0)), screenW - 410, 144, 18, { 210, 176, 112, 255 });
    DrawText(TextFormat("6 %s  R%d/%d  COST %d", GetLegacyUpgradeLabel(1), legacy.arsenalRank, GetLegacyUpgradeMaxRank(1), GetLegacyUpgradeCost(1)), screenW - 410, 166, 18, { 194, 172, 118, 255 });
    DrawText(TextFormat("7 %s  R%d/%d  COST %d", GetLegacyUpgradeLabel(2), legacy.emberkeepRank, GetLegacyUpgradeMaxRank(2), GetLegacyUpgradeCost(2)), screenW - 410, 188, 18, { 244, 166, 84, 255 });
    DrawText(TextFormat("8 %s  R%d/%d  COST %d", GetLegacyUpgradeLabel(3), legacy.hymnRank, GetLegacyUpgradeMaxRank(3), GetLegacyUpgradeCost(3)), screenW - 410, 210, 18, { 168, 190, 216, 255 });
    DrawText(TextFormat("NEXT // R%d H%d B%d K%d D%d X%d E%d", nextRaiders, nextHounds, nextBrutes, nextKnights, nextHeralds, nextBreakers, nextElites), screenW - 410, 238, 18, { 220, 208, 170, 255 });

    for (int i = 0; i < (int)bastions.size(); ++i) {
        const BastionSite& bastion = bastions[i];
        int y = 260 + i * 20;
        Color lineColor = (bastion.laneIndex == omenLane) ? Color{ 234, 150, 124, 255 } : (bastion.level > 0 ? Color{ 214, 196, 142, 255 } : Color{ 154, 164, 174, 255 });
        DrawText(TextFormat("R %s  LVL %d  COST G%d I%d E%d", GetBastionLabel(bastion.laneIndex), bastion.level, GetBastionUpgradeGoldCost(bastion), GetBastionUpgradeIronCost(bastion), GetBastionUpgradeEmberCost(bastion)), screenW - 410, y, 18, lineColor);
    }

    DrawRectangle(22, screenH - 176, screenW - 44, 154, Fade(BLACK, 0.72f));
    DrawRectangleLines(22, screenH - 176, screenW - 44, 154, { 110, 126, 172, 255 });

    if (state == PlayState::BuildPhase) {
        DrawText("BUILD // 1 Bow  2 Censer  3 Spire  4 Barricade  U Upgrade  X Sell  C Consecrate  R Fortify  H Gate  J Core  L Restore  SPACE Begin", 40, screenH - 156, 22, { 228, 220, 208, 255 });
        DrawText(TextFormat("FORECAST // Raiders %d  Hounds %d  Brutes %d  Knights %d  Heralds %d  Breakers %d  Elites %d", nextRaiders, nextHounds, nextBrutes, nextKnights, nextHeralds, nextBreakers, nextElites), 40, screenH - 122, 20, { 198, 208, 214, 255 });
    }
    else if (state == PlayState::BattlePhase) {
        int raiders = 0, hounds = 0, brutes = 0, knights = 0, heralds = 0, breakers = 0, elites = 0;
        for (const Enemy& enemy : enemies) {
            if (enemy.type == EnemyType::AshRaider) raiders++;
            else if (enemy.type == EnemyType::AshHound) hounds++;
            else if (enemy.type == EnemyType::GraveBrute) brutes++;
            else if (enemy.type == EnemyType::BannerKnight) knights++;
            else if (enemy.type == EnemyType::DirgeHerald) heralds++;
            else if (enemy.type == EnemyType::ProcessionBreaker) breakers++;
            if (enemy.elite) elites++;
        }
        DrawText(TextFormat("BATTLE // Raiders %d  Hounds %d  Brutes %d  Knights %d  Heralds %d  Breakers %d  Elites %d", raiders, hounds, brutes, knights, heralds, breakers, elites), 40, screenH - 156, 20, { 228, 220, 208, 255 });
        DrawText(TextFormat("Heralds buff nearby heretics // Q Crownfire // F War Hymn %.1fs // Fortified bastions ward nearby towers", GetWarHymnDuration()), 40, screenH - 122, 20, fervor >= crownfireCost ? Color{ 248, 224, 160, 255 } : Color{ 190, 198, 188, 255 });
    }
    else {
        DrawText("GAME OVER // ENTER starts a new procession // L restores the last chronicle", 40, screenH - 156, 26, { 228, 220, 208, 255 });
    }

    DrawText(TextFormat("CROWN DIRECTIVE // %s", directive.title.c_str()), 40, screenH - 88, 22, directive.failed ? Color{ 214, 104, 104, 255 } : (directive.completed ? Color{ 164, 220, 146, 255 } : Color{ 228, 208, 150, 255 }));
    DrawText(directive.detail.c_str(), 40, screenH - 58, 18, { 206, 212, 220, 255 });
    DrawText(TextFormat("%s // REWARD G%d I%d E%d ASH %d", directiveProgress.c_str(), directive.rewardGold, directive.rewardIron, directive.rewardEmber, directive.rewardAsh), 40, screenH - 32, 18, directive.failed ? Color{ 214, 132, 132, 255 } : (directive.completed ? Color{ 176, 226, 158, 255 } : Color{ 220, 198, 136, 255 }));

    if (announcementTimer > 0.0f) {
        int width = MeasureText(announcement.c_str(), 32);
        DrawRectangle(screenW / 2 - width / 2 - 24, 24, width + 48, 50, Fade(BLACK, 0.78f));
        DrawRectangleLines(screenW / 2 - width / 2 - 24, 24, width + 48, 50, { 188, 156, 96, 255 });
        DrawText(announcement.c_str(), screenW / 2 - width / 2, 34, 32, { 214, 186, 126, 255 });
    }

    if (hoveredValid) {
        DrawText(TextFormat("CELL %d, %d", hoveredCell.x, hoveredCell.y), screenW - 210, 304, 24, { 210, 218, 226, 255 });
    }

    if (hoveredBastion >= 0) {
        const BastionSite& bastion = bastions[hoveredBastion];
        DrawRectangle(screenW - 430, 336, 390, 190, Fade(BLACK, 0.72f));
        DrawRectangleLines(screenW - 430, 336, 390, 190, bastion.level > 0 ? Color{ 220, 188, 122, 255 } : Color{ 120, 132, 150, 255 });
        DrawText(GetBastionLabel(bastion.laneIndex), screenW - 410, 352, 28, { 236, 228, 210, 255 });
        DrawText((bastion.laneIndex == omenLane) ? "OMEN FACING // PRIMARY THREAT LANE" : "WALL ROLE // SUPPORT FIRE LANE", screenW - 410, 386, 20, (bastion.laneIndex == omenLane) ? Color{ 230, 144, 116, 255 } : Color{ 198, 208, 214, 255 });
        DrawText(TextFormat("LEVEL %d", bastion.level), screenW - 410, 412, 22, { 206, 212, 220, 255 });
        DrawText(TextFormat("Ward aura level %d to nearby towers", bastion.level), screenW - 410, 438, 20, bastion.level > 0 ? Color{ 232, 208, 150, 255 } : Color{ 146, 154, 162, 255 });
        if (bastion.level < 3) {
            DrawText(TextFormat("R FORTIFY // G%d I%d E%d", GetBastionUpgradeGoldCost(bastion), GetBastionUpgradeIronCost(bastion), GetBastionUpgradeEmberCost(bastion)), screenW - 410, 464, 20, { 220, 198, 136, 255 });
        }
        else {
            DrawText("MAX FORTIFICATION REACHED", screenW - 410, 464, 20, { 220, 198, 136, 255 });
        }
        DrawText("Fortified bastions auto-fire and feed Crownfire", screenW - 410, 492, 20, { 196, 204, 212, 255 });
    }
    else if (hoveredWaystone >= 0) {
        const WaystoneSite& stone = waystones[hoveredWaystone];
        DrawRectangle(screenW - 430, 336, 390, 160, Fade(BLACK, 0.72f));
        DrawRectangleLines(screenW - 430, 336, 390, 160, stone.consecrated ? Color{ 244, 214, 144, 255 } : Color{ 118, 132, 150, 255 });
        DrawText("ROAD WAYSTONE", screenW - 410, 352, 28, { 236, 228, 210, 255 });
        DrawText(stone.consecrated ? "STATUS // CONSECRATED" : "STATUS // DORMANT", screenW - 410, 386, 22, stone.consecrated ? Color{ 244, 214, 144, 255 } : Color{ 170, 180, 194, 255 });
        DrawText(consecratedWaystones >= 2 ? "Sanctum bell active // Crownfire cost reduced" : "Consecrate more stones to awaken the sanctum bell", screenW - 410, 416, 20, { 206, 212, 220, 255 });
        DrawText(stone.consecrated ? "Already part of the holy march" : "Press C // cost 8 Iron and 14 Ember", screenW - 410, 444, 20, { 220, 198, 136, 255 });
    }
    else if (hoveredTowerIndex >= 0 && hoveredTowerIndex < (int)towers.size()) {
        const Tower& tower = towers[hoveredTowerIndex];
        int costGold = GetTowerUpgradeGoldCost(tower);
        int costIron = GetTowerUpgradeIronCost(tower);
        int costEmber = GetTowerUpgradeEmberCost(tower);
        int sellGold = GetTowerSellGoldRefund(tower);
        int sellIron = GetTowerSellIronRefund(tower);
        int sellEmber = GetTowerSellEmberRefund(tower);
        bool blessed = IsTowerBlessed(tower);
        int wardLevel = GetTowerBastionWardLevel(tower);

        DrawRectangle(screenW - 430, 336, 390, 234, Fade(BLACK, 0.72f));
        DrawRectangleLines(screenW - 430, 336, 390, 234, wardLevel > 0 ? Color{ 232, 208, 150, 255 } : (blessed ? Color{ 244, 214, 144, 255 } : tower.color));
        DrawText(TowerLabel(tower.type), screenW - 410, 352, 28, { 236, 228, 210, 255 });
        DrawText(TextFormat("LEVEL %d", tower.level), screenW - 410, 386, 22, { 206, 212, 220, 255 });
        DrawText(TextFormat("DAMAGE %d%s", tower.damage, blessed ? " + BLESSING" : ""), screenW - 410, 414, 22, blessed ? Color{ 244, 214, 144, 255 } : Color{ 206, 212, 220, 255 });
        DrawText(TextFormat("RANGE %.1f   RATE %.2f", tower.range, tower.maxCooldown), screenW - 410, 442, 22, { 206, 212, 220, 255 });
        DrawText(TextFormat("BASTION WARD %d", wardLevel), screenW - 410, 470, 20, wardLevel > 0 ? Color{ 232, 208, 150, 255 } : Color{ 150, 156, 164, 255 });
        DrawText(TextFormat("COUNTER // %s", (waveOmen.find("HOUND") != std::string::npos) ? "WATCHBOW + BARRICADE" : (waveOmen.find("CHANT") != std::string::npos ? "CENSER + SPIRE" : "BALANCED FIRE")), screenW - 410, 494, 20, { 198, 208, 214, 255 });
        if (tower.level < 3) {
            DrawText(TextFormat("U UPGRADE // G%d I%d E%d", costGold, costIron, costEmber), screenW - 410, 520, 20, { 220, 198, 136, 255 });
        }
        else {
            DrawText("MAX CONSECRATION REACHED", screenW - 410, 520, 20, { 220, 198, 136, 255 });
        }
        DrawText(TextFormat("X SELL // G%d I%d E%d", sellGold, sellIron, sellEmber), screenW - 410, 544, 20, { 196, 180, 156, 255 });
    }

    if (stormFlash > 0.0f) {
        DrawRectangle(0, 0, screenW, screenH, Fade({ 216, 224, 255, 255 }, stormFlash * 0.18f));
    }

    if (state == PlayState::GameOver) {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.54f));
        const char* title = "THE PROCESSION HAS FALLEN";
        int w = MeasureText(title, 54);
        DrawText(title, screenW / 2 - w / 2, screenH / 2 - 80, 54, { 210, 84, 84, 255 });
        DrawText(TextFormat("LAST RUN ASH +%d   BEST WAVE %d   DIRECTIVES %d", legacyAshEarnedThisRun, legacy.highestWave, legacy.directivesCompleted), screenW / 2 - 240, screenH / 2 - 14, 24, { 214, 196, 142, 255 });
        DrawText(hasSuspendedChronicle ? "PRESS L TO RESTORE THE LAST BUILD-PHASE CHRONICLE" : "NO CHRONICLE AVAILABLE", screenW / 2 - 250, screenH / 2 + 18, 22, hasSuspendedChronicle ? Color{ 170, 198, 220, 255 } : Color{ 138, 142, 146, 255 });
        DrawText("PRESS ENTER TO BEGIN A NEW PROCESSION", screenW / 2 - 210, screenH / 2 + 48, 22, { 228, 220, 208, 255 });
    }
}