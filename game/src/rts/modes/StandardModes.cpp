#include "StandardModes.hpp"
#include <algorithm>

namespace Vehement {

// ============================================================================
// MeleeMode
// ============================================================================

MeleeMode::MeleeMode() {
    SetupMeleeVictoryConditions();
}

void MeleeMode::Initialize(GameState& state) {
    GameMode::Initialize(state);

    // Setup default teams (2 teams for basic melee)
    TeamConfig team1;
    team1.teamId = 1;
    team1.name = "Team 1";
    team1.color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
    team1.sharedVision = true;

    TeamConfig team2;
    team2.teamId = 2;
    team2.name = "Team 2";
    team2.color = glm::vec4(0.2f, 0.4f, 1.0f, 1.0f);
    team2.sharedVision = true;

    SetTeams({team1, team2});
}

void MeleeMode::OnGameStart(GameState& state) {
    GameMode::OnGameStart(state);
    // Grant starting resources based on rules
}

void MeleeMode::OnUpdate(GameState& state, float deltaTime) {
    GameMode::OnUpdate(state, deltaTime);

    // Check time limit
    int timeLimit = GetRuleInt("time_limit");
    if (timeLimit > 0 && m_gameTime >= timeLimit * 60.0f) {
        // Time's up - determine winner by score or resources
    }
}

void MeleeMode::SetupMeleeVictoryConditions() {
    // Destroy all buildings
    VictoryCondition destroyAll;
    destroyAll.id = "destroy_all";
    destroyAll.name = "Destruction";
    destroyAll.description = "Destroy all enemy buildings";
    destroyAll.enabled = true;
    destroyAll.checkFunction = [](GameState& state, int playerId) -> bool {
        // Check if all enemies have no buildings
        return false;  // Implement based on GameState
    };
    AddVictoryCondition(destroyAll);
}

// ============================================================================
// FreeForAllMode
// ============================================================================

FreeForAllMode::FreeForAllMode() {
    SetupFFAPlayers();
}

void FreeForAllMode::Initialize(GameState& state) {
    GameMode::Initialize(state);

    // Clear teams - everyone is on their own
    SetTeams({});

    // Set up player slots without team assignments
    SetupFFAPlayers();
}

void FreeForAllMode::SetupFFAPlayers() {
    m_playerSlots.clear();

    glm::vec4 colors[] = {
        glm::vec4(1.0f, 0.2f, 0.2f, 1.0f),  // Red
        glm::vec4(0.2f, 0.4f, 1.0f, 1.0f),  // Blue
        glm::vec4(0.2f, 0.8f, 0.2f, 1.0f),  // Green
        glm::vec4(1.0f, 1.0f, 0.2f, 1.0f),  // Yellow
        glm::vec4(1.0f, 0.5f, 0.0f, 1.0f),  // Orange
        glm::vec4(0.6f, 0.2f, 1.0f, 1.0f),  // Purple
        glm::vec4(0.2f, 1.0f, 1.0f, 1.0f),  // Cyan
        glm::vec4(1.0f, 0.4f, 0.7f, 1.0f)   // Pink
    };

    for (int i = 0; i < GetMaxPlayers(); i++) {
        PlayerSlot slot;
        slot.slotId = i;
        slot.name = "Player " + std::to_string(i + 1);
        slot.teamId = -1;  // No team
        slot.color = colors[i % 8];
        slot.race = "random";
        slot.startLocation = -1;
        m_playerSlots.push_back(slot);
    }
}

// ============================================================================
// CaptureTheFlagMode
// ============================================================================

CaptureTheFlagMode::CaptureTheFlagMode() {
    SetupCTFRules();
    SetupCTFVictory();
}

void CaptureTheFlagMode::Initialize(GameState& state) {
    GameMode::Initialize(state);

    m_teamScores.clear();
    m_flags.clear();

    // Initialize team scores
    for (const auto& team : m_teams) {
        m_teamScores[team.teamId] = 0;
    }
}

void CaptureTheFlagMode::OnUpdate(GameState& state, float deltaTime) {
    GameMode::OnUpdate(state, deltaTime);

    // Update flag return timers
    for (auto& flag : m_flags) {
        if (!flag.isAtBase && flag.carrierUnitId == -1) {
            flag.returnTimer -= deltaTime;
            if (flag.returnTimer <= 0.0f) {
                OnFlagReturned(flag.ownerTeam);
            }
        }
    }
}

void CaptureTheFlagMode::OnUnitDestroyed(GameState& state, Unit& unit) {
    GameMode::OnUnitDestroyed(state, unit);

    // Check if unit was carrying a flag
    for (auto& flag : m_flags) {
        if (flag.carrierUnitId == static_cast<int>(0) /* unit.GetId() */) {
            OnFlagDropped(flag.ownerTeam, glm::vec3(0.0f) /* unit.GetPosition() */);
        }
    }
}

void CaptureTheFlagMode::OnFlagCaptured(int capturingTeam, int flagTeam) {
    m_teamScores[capturingTeam]++;

    // Return flag to base
    for (auto& flag : m_flags) {
        if (flag.ownerTeam == flagTeam) {
            flag.isAtBase = true;
            flag.carrierUnitId = -1;
            break;
        }
    }

    // Check victory
    if (m_teamScores[capturingTeam] >= m_targetScore) {
        // capturingTeam wins
    }
}

void CaptureTheFlagMode::OnFlagReturned(int teamId) {
    for (auto& flag : m_flags) {
        if (flag.ownerTeam == teamId) {
            flag.isAtBase = true;
            flag.carrierUnitId = -1;
            break;
        }
    }
}

void CaptureTheFlagMode::OnFlagDropped(int teamId, const glm::vec3& position) {
    for (auto& flag : m_flags) {
        if (flag.ownerTeam == teamId) {
            flag.carrierUnitId = -1;
            flag.position = position;
            flag.returnTimer = m_flagReturnTime;
            break;
        }
    }
}

void CaptureTheFlagMode::SetupCTFRules() {
    AddRule({
        "target_score", "Captures to Win", "Number of flag captures to win",
        "Victory", RuleType::Integer, 3, 3, {}, 1, 10, false
    });

    AddRule({
        "flag_return_time", "Flag Return Time", "Seconds before dropped flag returns",
        "Flags", RuleType::Float, 30.0f, 30.0f, {}, 10.0f, 120.0f, false
    });

    AddRule({
        "flag_carrier_speed", "Carrier Speed %", "Movement speed while carrying flag",
        "Flags", RuleType::Integer, 80, 80, {}, 50, 100, false
    });
}

void CaptureTheFlagMode::SetupCTFVictory() {
    VictoryCondition ctfWin;
    ctfWin.id = "ctf_score";
    ctfWin.name = "Capture Score";
    ctfWin.description = "Reach target flag capture score";
    ctfWin.enabled = true;
    ctfWin.checkFunction = [this](GameState& state, int playerId) -> bool {
        // Find player's team and check score
        for (const auto& slot : m_playerSlots) {
            if (slot.slotId == playerId && slot.teamId >= 0) {
                auto it = m_teamScores.find(slot.teamId);
                if (it != m_teamScores.end()) {
                    return it->second >= m_targetScore;
                }
            }
        }
        return false;
    };
    AddVictoryCondition(ctfWin);
}

void CaptureTheFlagMode::SpawnFlags(GameState& state) {
    // Spawn a flag for each team at their base
}

// ============================================================================
// KingOfTheHillMode
// ============================================================================

KingOfTheHillMode::KingOfTheHillMode() {
    SetupKOTHRules();
    SetupKOTHVictory();
}

void KingOfTheHillMode::Initialize(GameState& state) {
    GameMode::Initialize(state);

    m_teamPoints.clear();

    // Initialize team points
    for (const auto& team : m_teams) {
        m_teamPoints[team.teamId] = 0.0f;
    }

    // Setup default hill in center
    HillZone centralHill;
    centralHill.position = glm::vec3(0.0f, 0.0f, 0.0f);  // Map center
    centralHill.radius = 15.0f;
    centralHill.controllingTeam = -1;
    centralHill.captureProgress = 0.0f;
    centralHill.isContested = false;
    m_hills.push_back(centralHill);
}

void KingOfTheHillMode::OnUpdate(GameState& state, float deltaTime) {
    GameMode::OnUpdate(state, deltaTime);

    UpdateHillControl(state);

    // Award points to controlling team
    for (const auto& hill : m_hills) {
        if (hill.controllingTeam >= 0 && !hill.isContested) {
            m_teamPoints[hill.controllingTeam] += m_pointsPerSecond * deltaTime;
        }
    }

    // Check victory
    for (const auto& [teamId, points] : m_teamPoints) {
        if (points >= m_targetPoints) {
            // Team wins
            break;
        }
    }
}

void KingOfTheHillMode::UpdateHillControl(GameState& state) {
    // Check units in each hill zone and update control
    for (auto& hill : m_hills) {
        std::unordered_map<int, int> teamUnitsInZone;

        // Count units from each team in the zone
        // Implementation depends on GameState

        // Determine control state
        int dominantTeam = -1;
        int maxUnits = 0;
        bool contested = false;

        for (const auto& [teamId, count] : teamUnitsInZone) {
            if (count > maxUnits) {
                dominantTeam = teamId;
                maxUnits = count;
                contested = false;
            } else if (count == maxUnits && count > 0) {
                contested = true;
            }
        }

        hill.isContested = contested;

        if (!contested && dominantTeam >= 0) {
            if (hill.controllingTeam != dominantTeam) {
                // Capturing
                hill.captureProgress += (1.0f / m_captureTime);
                if (hill.captureProgress >= 1.0f) {
                    hill.controllingTeam = dominantTeam;
                    hill.captureProgress = 0.0f;
                }
            }
        }
    }
}

void KingOfTheHillMode::SetupKOTHRules() {
    AddRule({
        "target_points", "Points to Win", "Victory point target",
        "Victory", RuleType::Float, 1000.0f, 1000.0f, {}, 100.0f, 5000.0f, false
    });

    AddRule({
        "points_per_second", "Points Per Second", "Points gained per second of control",
        "Scoring", RuleType::Float, 1.0f, 1.0f, {}, 0.5f, 5.0f, false
    });

    AddRule({
        "capture_time", "Capture Time", "Seconds to capture an uncontested hill",
        "Control", RuleType::Float, 10.0f, 10.0f, {}, 5.0f, 30.0f, false
    });
}

void KingOfTheHillMode::SetupKOTHVictory() {
    VictoryCondition kothWin;
    kothWin.id = "koth_score";
    kothWin.name = "Hill Control";
    kothWin.description = "Reach target victory points";
    kothWin.enabled = true;
    kothWin.checkFunction = [this](GameState& state, int playerId) -> bool {
        for (const auto& slot : m_playerSlots) {
            if (slot.slotId == playerId && slot.teamId >= 0) {
                auto it = m_teamPoints.find(slot.teamId);
                if (it != m_teamPoints.end()) {
                    return it->second >= m_targetPoints;
                }
            }
        }
        return false;
    };
    AddVictoryCondition(kothWin);
}

// ============================================================================
// SurvivalMode
// ============================================================================

SurvivalMode::SurvivalMode() {
    SetupSurvivalRules();
}

void SurvivalMode::Initialize(GameState& state) {
    GameMode::Initialize(state);

    m_currentWave = 0;
    m_enemiesRemaining = 0;
    m_totalEnemiesKilled = 0;
    m_waveTimer = 0.0f;
    m_waveInProgress = false;
}

void SurvivalMode::OnGameStart(GameState& state) {
    GameMode::OnGameStart(state);

    // Start first wave after delay
    m_waveTimer = m_timeBetweenWaves;
}

void SurvivalMode::OnUpdate(GameState& state, float deltaTime) {
    GameMode::OnUpdate(state, deltaTime);

    if (!m_waveInProgress) {
        m_waveTimer -= deltaTime;
        if (m_waveTimer <= 0.0f) {
            StartNextWave(state);
        }
    } else {
        CheckWaveComplete(state);
    }
}

void SurvivalMode::OnUnitDestroyed(GameState& state, Unit& unit) {
    GameMode::OnUnitDestroyed(state, unit);

    // Check if destroyed unit was an enemy
    // if (unit.GetPlayerId() == ENEMY_PLAYER_ID) {
    //     m_enemiesRemaining--;
    //     m_totalEnemiesKilled++;
    // }
}

void SurvivalMode::SetupSurvivalRules() {
    AddRule({
        "starting_wave", "Starting Wave", "Wave number to start at",
        "Waves", RuleType::Integer, 1, 1, {}, 1, 20, false
    });

    AddRule({
        "time_between_waves", "Wave Interval", "Seconds between waves",
        "Waves", RuleType::Float, 30.0f, 30.0f, {}, 10.0f, 120.0f, false
    });

    AddRule({
        "enemy_scaling", "Enemy Scaling %", "How much stronger enemies get per wave",
        "Difficulty", RuleType::Integer, 10, 10, {}, 0, 50, false
    });
}

void SurvivalMode::StartNextWave(GameState& state) {
    m_currentWave++;
    m_waveInProgress = true;

    // Configure wave
    m_currentWaveConfig.waveNumber = m_currentWave;
    m_currentWaveConfig.baseEnemyCount = 5 + m_currentWave * 2;
    m_currentWaveConfig.enemyHealthMultiplier = 1.0f + (m_currentWave - 1) * 0.1f;
    m_currentWaveConfig.enemyDamageMultiplier = 1.0f + (m_currentWave - 1) * 0.05f;
    m_currentWaveConfig.timeBetweenSpawns = 2.0f;

    // Determine enemy types based on wave
    m_currentWaveConfig.enemyTypes.clear();
    m_currentWaveConfig.enemyTypes.push_back("enemy_basic");
    if (m_currentWave >= 3) {
        m_currentWaveConfig.enemyTypes.push_back("enemy_ranged");
    }
    if (m_currentWave >= 5) {
        m_currentWaveConfig.enemyTypes.push_back("enemy_tank");
    }
    if (m_currentWave >= 10) {
        m_currentWaveConfig.enemyTypes.push_back("enemy_boss");
    }

    SpawnWaveEnemies(state);
}

void SurvivalMode::SpawnWaveEnemies(GameState& state) {
    m_enemiesRemaining = m_currentWaveConfig.baseEnemyCount;
    // Spawn enemies with configured stats
}

void SurvivalMode::CheckWaveComplete(GameState& state) {
    if (m_enemiesRemaining <= 0) {
        m_waveInProgress = false;
        m_waveTimer = m_timeBetweenWaves;
    }
}

// ============================================================================
// TowerDefenseMode
// ============================================================================

TowerDefenseMode::TowerDefenseMode() {
    SetupTDRules();
}

void TowerDefenseMode::Initialize(GameState& state) {
    GameMode::Initialize(state);

    m_currentWave = 0;
    m_lives = GetRuleInt("starting_lives");
    m_gold = GetRuleInt("starting_gold");
    m_score = 0;
}

void TowerDefenseMode::OnGameStart(GameState& state) {
    GameMode::OnGameStart(state);

    LoadCreepPath(state);
    StartWave(state, 1);
}

void TowerDefenseMode::OnUpdate(GameState& state, float deltaTime) {
    GameMode::OnUpdate(state, deltaTime);

    // Spawn creeps on timer
    if (m_creepsToSpawn > 0) {
        m_spawnTimer -= deltaTime;
        if (m_spawnTimer <= 0.0f && !m_waves.empty()) {
            auto& wave = m_waves[m_currentWave - 1];
            if (m_currentCreepIndex < wave.creepCounts.size()) {
                auto& [creepType, count] = wave.creepCounts[m_currentCreepIndex];
                if (count > 0) {
                    SpawnCreep(state, creepType);
                    wave.creepCounts[m_currentCreepIndex].second--;
                    m_creepsToSpawn--;
                } else {
                    m_currentCreepIndex++;
                }
            }
            m_spawnTimer = wave.spawnInterval;
        }
    }

    // Check defeat
    if (m_lives <= 0) {
        // Game over
    }
}

void TowerDefenseMode::OnBuildingCreated(GameState& state, Unit& building) {
    GameMode::OnBuildingCreated(state, building);
    // Track towers for scoring
}

void TowerDefenseMode::OnCreepReachedGoal(int creepId) {
    m_lives--;
}

void TowerDefenseMode::OnCreepKilled(int creepId, int killerTowerId) {
    m_score += 10;
    m_gold += 5;
}

void TowerDefenseMode::SetupTDRules() {
    AddRule({
        "starting_lives", "Starting Lives", "Lives before game over",
        "Core", RuleType::Integer, 20, 20, {}, 1, 100, false
    });

    AddRule({
        "starting_gold", "Starting Gold", "Gold to build initial towers",
        "Economy", RuleType::Integer, 100, 100, {}, 50, 500, false
    });

    AddRule({
        "creep_speed", "Creep Speed %", "How fast creeps move",
        "Difficulty", RuleType::Integer, 100, 100, {}, 50, 200, false
    });

    AddRule({
        "auto_start", "Auto Start Waves", "Automatically start next wave",
        "Waves", RuleType::Boolean, true, true, {}, 0, 0, false
    });
}

void TowerDefenseMode::LoadCreepPath(GameState& state) {
    // Load path from map data
    m_creepPath.clear();

    // Default straight path for testing
    for (int i = 0; i < 10; i++) {
        PathNode node;
        node.position = glm::vec3(-50.0f + i * 10.0f, 0.0f, 0.0f);
        node.nextNodeIndex = i + 1;
        m_creepPath.push_back(node);
    }
    if (!m_creepPath.empty()) {
        m_creepPath.back().nextNodeIndex = -1;  // End of path
    }
}

void TowerDefenseMode::StartWave(GameState& state, int waveNumber) {
    m_currentWave = waveNumber;
    m_currentCreepIndex = 0;

    // Configure wave
    CreepWave wave;
    wave.waveNumber = waveNumber;
    wave.spawnInterval = 1.5f;
    wave.healthMultiplier = 1.0f + (waveNumber - 1) * 0.15f;
    wave.goldReward = 5 + waveNumber;

    // Creep composition
    int baseCount = 5 + waveNumber * 2;
    wave.creepCounts.push_back({"creep_basic", baseCount});
    if (waveNumber >= 3) {
        wave.creepCounts.push_back({"creep_fast", baseCount / 2});
    }
    if (waveNumber >= 5) {
        wave.creepCounts.push_back({"creep_armored", baseCount / 3});
    }
    if (waveNumber % 5 == 0) {
        wave.creepCounts.push_back({"creep_boss", 1});
    }

    // Calculate total creeps
    m_creepsToSpawn = 0;
    for (const auto& [type, count] : wave.creepCounts) {
        m_creepsToSpawn += count;
    }

    m_waves.push_back(wave);
    m_spawnTimer = 0.0f;
}

void TowerDefenseMode::SpawnCreep(GameState& state, const std::string& creepType) {
    // Spawn creep at path start
}

// ============================================================================
// RegicideMode
// ============================================================================

RegicideMode::RegicideMode() {
    SetupRegicideRules();
}

void RegicideMode::Initialize(GameState& state) {
    GameMode::Initialize(state);
    m_playerKings.clear();
}

void RegicideMode::OnGameStart(GameState& state) {
    GameMode::OnGameStart(state);
    SpawnKings(state);
}

void RegicideMode::OnUnitDestroyed(GameState& state, Unit& unit) {
    GameMode::OnUnitDestroyed(state, unit);

    // Check if destroyed unit was a king
    for (auto& [playerId, kingId] : m_playerKings) {
        if (kingId == static_cast<int>(0) /* unit.GetId() */) {
            OnPlayerDefeat(state, playerId);
            break;
        }
    }
}

void RegicideMode::SetupRegicideRules() {
    AddRule({
        "king_invulnerable_start", "King Invulnerable Start", "Seconds king is invulnerable at start",
        "King", RuleType::Float, 60.0f, 60.0f, {}, 0.0f, 300.0f, false
    });

    AddRule({
        "king_health_bonus", "King Health %", "King bonus health percentage",
        "King", RuleType::Integer, 200, 200, {}, 100, 500, false
    });
}

void RegicideMode::SpawnKings(GameState& state) {
    // Spawn a king for each player at their start location
}

// ============================================================================
// DeathmatchMode
// ============================================================================

DeathmatchMode::DeathmatchMode() {
    SetupDeathmatchRules();
}

void DeathmatchMode::Initialize(GameState& state) {
    GameMode::Initialize(state);
}

void DeathmatchMode::OnGameStart(GameState& state) {
    GameMode::OnGameStart(state);
    UnlockAllTech(state);
}

void DeathmatchMode::SetupDeathmatchRules() {
    // Override default starting resources
    SetRule("starting_gold", 50000);
    SetRule("starting_wood", 50000);

    AddRule({
        "instant_build", "Instant Build", "Buildings complete instantly",
        "Speed", RuleType::Boolean, true, true, {}, 0, 0, false
    });

    AddRule({
        "instant_train", "Instant Train", "Units train instantly",
        "Speed", RuleType::Boolean, true, true, {}, 0, 0, false
    });

    AddRule({
        "instant_research", "Instant Research", "Research completes instantly",
        "Speed", RuleType::Boolean, true, true, {}, 0, 0, false
    });
}

void DeathmatchMode::UnlockAllTech(GameState& state) {
    // Unlock all research and upgrades for all players
}

} // namespace Vehement
