#pragma once

#include "GameMode.hpp"

namespace Vehement {

/**
 * @brief Melee Mode - Classic RTS battle
 *
 * Rules:
 * - Destroy all enemy buildings and units to win
 * - Standard resource gathering and base building
 * - Optional teams
 */
class MeleeMode : public GameMode {
public:
    MeleeMode();

    std::string GetId() const override { return "melee"; }
    std::string GetName() const override { return "Melee"; }
    std::string GetDescription() const override {
        return "Classic RTS battle - destroy all enemy buildings and units to win";
    }
    std::string GetCategory() const override { return "Standard"; }

    int GetMinPlayers() const override { return 2; }
    int GetMaxPlayers() const override { return 12; }

    void Initialize(GameState& state) override;
    void OnGameStart(GameState& state) override;
    void OnUpdate(GameState& state, float deltaTime) override;

private:
    void SetupMeleeVictoryConditions();
};

/**
 * @brief Free For All Mode - Every player for themselves
 *
 * Rules:
 * - No teams allowed
 * - Last player standing wins
 * - Diplomacy disabled
 */
class FreeForAllMode : public GameMode {
public:
    FreeForAllMode();

    std::string GetId() const override { return "ffa"; }
    std::string GetName() const override { return "Free For All"; }
    std::string GetDescription() const override {
        return "Every player for themselves - last player standing wins";
    }
    std::string GetCategory() const override { return "Standard"; }

    int GetMinPlayers() const override { return 3; }
    int GetMaxPlayers() const override { return 8; }
    bool AllowsTeams() const override { return false; }

    void Initialize(GameState& state) override;

private:
    void SetupFFAPlayers();
};

/**
 * @brief Capture The Flag Mode
 *
 * Rules:
 * - Each team has a flag at their base
 * - Capture enemy flag and return to your base to score
 * - First to target score wins
 */
class CaptureTheFlagMode : public GameMode {
public:
    CaptureTheFlagMode();

    std::string GetId() const override { return "ctf"; }
    std::string GetName() const override { return "Capture The Flag"; }
    std::string GetDescription() const override {
        return "Capture enemy flags and return them to your base to score";
    }
    std::string GetCategory() const override { return "Objective"; }

    int GetMinPlayers() const override { return 4; }
    int GetMaxPlayers() const override { return 12; }

    void Initialize(GameState& state) override;
    void OnUpdate(GameState& state, float deltaTime) override;
    void OnUnitDestroyed(GameState& state, Unit& unit) override;

    // CTF-specific
    void OnFlagCaptured(int capturingTeam, int flagTeam);
    void OnFlagReturned(int teamId);
    void OnFlagDropped(int teamId, const glm::vec3& position);

private:
    void SetupCTFRules();
    void SetupCTFVictory();
    void SpawnFlags(GameState& state);

    struct FlagState {
        int ownerTeam;
        bool isAtBase;
        int carrierUnitId;
        glm::vec3 position;
        float returnTimer;
    };

    std::vector<FlagState> m_flags;
    std::unordered_map<int, int> m_teamScores;
    int m_targetScore = 3;
    float m_flagReturnTime = 30.0f;
};

/**
 * @brief King of the Hill Mode
 *
 * Rules:
 * - Control the central hill to accumulate points
 * - Contested hill doesn't give points
 * - First to target score wins
 */
class KingOfTheHillMode : public GameMode {
public:
    KingOfTheHillMode();

    std::string GetId() const override { return "koth"; }
    std::string GetName() const override { return "King of the Hill"; }
    std::string GetDescription() const override {
        return "Control the central point to accumulate victory points";
    }
    std::string GetCategory() const override { return "Objective"; }

    int GetMinPlayers() const override { return 2; }
    int GetMaxPlayers() const override { return 8; }

    void Initialize(GameState& state) override;
    void OnUpdate(GameState& state, float deltaTime) override;

private:
    void SetupKOTHRules();
    void SetupKOTHVictory();
    void UpdateHillControl(GameState& state);

    struct HillZone {
        glm::vec3 position;
        float radius;
        int controllingTeam;
        float captureProgress;
        bool isContested;
    };

    std::vector<HillZone> m_hills;
    std::unordered_map<int, float> m_teamPoints;
    float m_targetPoints = 1000.0f;
    float m_pointsPerSecond = 1.0f;
    float m_captureTime = 10.0f;
};

/**
 * @brief Survival Mode - Cooperative PvE
 *
 * Rules:
 * - All players on same team
 * - Endless waves of AI enemies
 * - Survive as long as possible
 * - Score based on waves completed
 */
class SurvivalMode : public GameMode {
public:
    SurvivalMode();

    std::string GetId() const override { return "survival"; }
    std::string GetName() const override { return "Survival"; }
    std::string GetDescription() const override {
        return "Work together to survive endless waves of enemies";
    }
    std::string GetCategory() const override { return "Cooperative"; }

    int GetMinPlayers() const override { return 1; }
    int GetMaxPlayers() const override { return 4; }
    bool AllowsTeams() const override { return false; }

    void Initialize(GameState& state) override;
    void OnGameStart(GameState& state) override;
    void OnUpdate(GameState& state, float deltaTime) override;
    void OnUnitDestroyed(GameState& state, Unit& unit) override;

private:
    void SetupSurvivalRules();
    void StartNextWave(GameState& state);
    void SpawnWaveEnemies(GameState& state);
    void CheckWaveComplete(GameState& state);

    struct WaveConfig {
        int waveNumber;
        int baseEnemyCount;
        float enemyHealthMultiplier;
        float enemyDamageMultiplier;
        std::vector<std::string> enemyTypes;
        float timeBetweenSpawns;
    };

    int m_currentWave = 0;
    int m_enemiesRemaining = 0;
    int m_totalEnemiesKilled = 0;
    float m_waveTimer = 0.0f;
    float m_timeBetweenWaves = 30.0f;
    bool m_waveInProgress = false;
    WaveConfig m_currentWaveConfig;
};

/**
 * @brief Tower Defense Mode
 *
 * Rules:
 * - Build towers along paths
 * - Enemies spawn and follow predetermined paths
 * - Prevent enemies from reaching the goal
 * - Lives system
 */
class TowerDefenseMode : public GameMode {
public:
    TowerDefenseMode();

    std::string GetId() const override { return "tower_defense"; }
    std::string GetName() const override { return "Tower Defense"; }
    std::string GetDescription() const override {
        return "Build towers to defend against waves of creeps";
    }
    std::string GetCategory() const override { return "Cooperative"; }

    int GetMinPlayers() const override { return 1; }
    int GetMaxPlayers() const override { return 4; }
    bool AllowsTeams() const override { return false; }

    void Initialize(GameState& state) override;
    void OnGameStart(GameState& state) override;
    void OnUpdate(GameState& state, float deltaTime) override;
    void OnBuildingCreated(GameState& state, Unit& building) override;

    // TD-specific
    void OnCreepReachedGoal(int creepId);
    void OnCreepKilled(int creepId, int killerTowerId);

private:
    void SetupTDRules();
    void LoadCreepPath(GameState& state);
    void StartWave(GameState& state, int waveNumber);
    void SpawnCreep(GameState& state, const std::string& creepType);

    struct PathNode {
        glm::vec3 position;
        int nextNodeIndex;
    };

    struct CreepWave {
        int waveNumber;
        std::vector<std::pair<std::string, int>> creepCounts;  // type, count
        float spawnInterval;
        float healthMultiplier;
        int goldReward;
    };

    std::vector<PathNode> m_creepPath;
    std::vector<CreepWave> m_waves;
    int m_currentWave = 0;
    int m_lives = 20;
    int m_gold = 100;
    int m_score = 0;
    float m_spawnTimer = 0.0f;
    int m_creepsToSpawn = 0;
    int m_currentCreepIndex = 0;
};

/**
 * @brief Regicide Mode - Kill the enemy king
 *
 * Rules:
 * - Each player has a special King unit
 * - Killing the enemy king wins the game
 * - King cannot be rebuilt
 */
class RegicideMode : public GameMode {
public:
    RegicideMode();

    std::string GetId() const override { return "regicide"; }
    std::string GetName() const override { return "Regicide"; }
    std::string GetDescription() const override {
        return "Kill the enemy King to win - protect yours at all costs";
    }
    std::string GetCategory() const override { return "Standard"; }

    int GetMinPlayers() const override { return 2; }
    int GetMaxPlayers() const override { return 8; }

    void Initialize(GameState& state) override;
    void OnGameStart(GameState& state) override;
    void OnUnitDestroyed(GameState& state, Unit& unit) override;

private:
    void SetupRegicideRules();
    void SpawnKings(GameState& state);

    std::unordered_map<int, int> m_playerKings;  // playerId -> kingUnitId
};

/**
 * @brief Deathmatch Mode - Start with full tech and resources
 *
 * Rules:
 * - All players start with massive resources
 * - All tech instantly available
 * - Fast-paced combat focus
 */
class DeathmatchMode : public GameMode {
public:
    DeathmatchMode();

    std::string GetId() const override { return "deathmatch"; }
    std::string GetName() const override { return "Deathmatch"; }
    std::string GetDescription() const override {
        return "Start with full tech and resources - instant action";
    }
    std::string GetCategory() const override { return "Standard"; }

    void Initialize(GameState& state) override;
    void OnGameStart(GameState& state) override;

private:
    void SetupDeathmatchRules();
    void UnlockAllTech(GameState& state);
};

} // namespace Vehement
