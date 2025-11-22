#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Vehement {

class InGameEditor;

/**
 * @brief Build order entry
 */
struct BuildOrderEntry {
    std::string buildingId;
    int priority = 0;
    int targetCount = 1;
    std::string condition;
};

/**
 * @brief Unit training order
 */
struct TrainOrderEntry {
    std::string unitId;
    int priority = 0;
    int targetCount = 0;  // 0 = unlimited
    float ratio = 0.0f;   // ratio of army composition
    std::string condition;
};

/**
 * @brief Attack wave configuration
 */
struct AttackWave {
    std::string id;
    std::string name;
    float minArmySize = 10.0f;
    float minTimer = 300.0f;  // seconds since game start
    float repeatInterval = 0.0f;  // 0 = one time
    std::string targetPriority;  // "base", "army", "workers", "nearest"
    bool waitForAll = true;  // wait for all units before attacking
    std::vector<std::string> requiredUnits;
};

/**
 * @brief AI strategy pattern
 */
struct AIStrategy {
    std::string id;
    std::string name;
    std::string description;

    // Economy
    int targetWorkers = 20;
    float workerRatio = 0.3f;
    bool expandAggressively = false;

    // Military
    float aggressiveness = 0.5f;  // 0 = defensive, 1 = aggressive
    float armyRatio = 0.5f;
    std::vector<TrainOrderEntry> trainOrder;
    std::vector<AttackWave> attackWaves;

    // Build order
    std::vector<BuildOrderEntry> buildOrder;

    // Tech
    std::vector<std::string> researchPriority;
};

/**
 * @brief AI difficulty preset
 */
struct AIDifficulty {
    std::string id;
    std::string name;

    // Resource bonuses
    float gatherRateBonus = 1.0f;
    float buildSpeedBonus = 1.0f;
    float damageBonus = 1.0f;
    float healthBonus = 1.0f;

    // Behavior
    float reactionTime = 1.0f;  // seconds delay
    float decisionAccuracy = 1.0f;  // 0-1
    bool cheats = false;  // can see through fog
    float microLevel = 0.5f;  // unit micro management skill

    // Economy
    int maxWorkers = 30;
    bool expandsNaturally = true;
};

/**
 * @brief Complete AI configuration for a player
 */
struct AIConfig {
    std::string playerId;
    std::string difficultyId = "normal";
    std::string strategyId = "balanced";

    // Overrides
    std::unordered_map<std::string, float> statOverrides;
    std::vector<BuildOrderEntry> customBuildOrder;
    std::vector<TrainOrderEntry> customTrainOrder;
};

/**
 * @brief AI Editor - Configure AI behavior
 *
 * Features:
 * - AI difficulty settings
 * - Build orders
 * - Attack timing
 * - Unit preferences
 * - Strategy patterns
 */
class AIEditor {
public:
    AIEditor();
    ~AIEditor();

    AIEditor(const AIEditor&) = delete;
    AIEditor& operator=(const AIEditor&) = delete;

    bool Initialize(InGameEditor& parent);
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);
    void Render();
    void ProcessInput();

    // Difficulty management
    [[nodiscard]] const std::vector<AIDifficulty>& GetDifficulties() const { return m_difficulties; }
    [[nodiscard]] AIDifficulty* GetDifficulty(const std::string& id);
    void CreateDifficulty(const AIDifficulty& difficulty);
    void UpdateDifficulty(const std::string& id, const AIDifficulty& difficulty);
    void DeleteDifficulty(const std::string& id);

    // Strategy management
    [[nodiscard]] const std::vector<AIStrategy>& GetStrategies() const { return m_strategies; }
    [[nodiscard]] AIStrategy* GetStrategy(const std::string& id);
    void CreateStrategy(const AIStrategy& strategy);
    void UpdateStrategy(const std::string& id, const AIStrategy& strategy);
    void DeleteStrategy(const std::string& id);

    // AI config for players
    void SetAIConfig(const std::string& playerId, const AIConfig& config);
    [[nodiscard]] AIConfig* GetAIConfig(const std::string& playerId);

    // Selection
    void SelectDifficulty(const std::string& id);
    void SelectStrategy(const std::string& id);
    [[nodiscard]] const std::string& GetSelectedDifficultyId() const { return m_selectedDifficultyId; }
    [[nodiscard]] const std::string& GetSelectedStrategyId() const { return m_selectedStrategyId; }

    // Testing
    void TestAI(const std::string& strategyId);
    void SimulateDecision(const std::string& scenario);

    // Callbacks
    std::function<void()> OnAIConfigChanged;

private:
    void RenderDifficultyEditor();
    void RenderStrategyEditor();
    void RenderBuildOrderEditor();
    void RenderTrainOrderEditor();
    void RenderAttackWaveEditor();
    void RenderAIAssignment();
    void RenderTestPanel();

    void InitializeDefaults();
    std::string GenerateStrategyId();

    bool m_initialized = false;
    InGameEditor* m_parent = nullptr;

    // Data
    std::vector<AIDifficulty> m_difficulties;
    std::vector<AIStrategy> m_strategies;
    std::vector<AIConfig> m_playerConfigs;

    // Selection
    std::string m_selectedDifficultyId;
    std::string m_selectedStrategyId;

    // Editor state
    int m_selectedTab = 0;
    int m_nextStrategyId = 1;

    // Test state
    bool m_showTestPanel = false;
    std::vector<std::string> m_testLog;
};

} // namespace Vehement
