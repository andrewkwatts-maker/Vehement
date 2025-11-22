#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Vehement {

class InGameEditor;
class MapFile;

/**
 * @brief Player slot configuration
 */
struct PlayerSlot {
    int playerId = 0;
    std::string name;
    std::string faction;
    int teamId = 0;
    bool isHuman = false;
    bool isActive = true;
    int startingGold = 500;
    int startingWood = 200;
    int startingFood = 0;
    std::string aiDifficulty = "normal";
    std::string color = "#FF0000";
    std::string startLocation;
};

/**
 * @brief Team configuration
 */
struct Team {
    int teamId = 0;
    std::string name;
    std::vector<int> playerIds;
    bool sharedVision = true;
    bool sharedControl = false;
    bool sharedResources = false;
};

/**
 * @brief Alliance setting between players
 */
struct AllianceSetting {
    int player1 = 0;
    int player2 = 0;
    bool allied = false;
    bool sharedVision = false;
    bool sharedControl = false;
    bool canAttack = true;
};

/**
 * @brief Victory condition type
 */
enum class VictoryType : uint8_t {
    LastManStanding,    // Eliminate all enemies
    KillLeader,         // Kill enemy hero/leader
    ControlPoints,      // Hold control points
    ResourceGoal,       // Gather X resources
    BuildWonder,        // Complete special building
    TimeLimit,          // Survive for duration
    Score,              // Reach score threshold
    Custom              // Script-defined
};

/**
 * @brief Victory condition
 */
struct VictoryCondition {
    VictoryType type = VictoryType::LastManStanding;
    int targetAmount = 0;
    float timeLimit = 0.0f;
    std::string targetId;
    std::string description;
    bool enabled = true;
};

/**
 * @brief Tech level restriction
 */
enum class TechLevel : uint8_t {
    None,       // No restrictions
    Age1,       // First age only
    Age2,       // Up to second age
    Age3,       // Up to third age
    Age4,       // All ages
    Custom      // Custom restrictions
};

/**
 * @brief Special rule
 */
struct SpecialRule {
    std::string id;
    std::string name;
    std::string description;
    bool enabled = false;
    std::unordered_map<std::string, std::string> parameters;
};

/**
 * @brief Complete scenario settings
 */
struct ScenarioConfig {
    // General
    std::string gameName;
    std::string gameDescription;
    int maxPlayers = 8;
    bool allowObservers = true;

    // Player slots
    std::vector<PlayerSlot> playerSlots;
    std::vector<Team> teams;
    std::vector<AllianceSetting> alliances;

    // Starting conditions
    int startingGold = 500;
    int startingWood = 200;
    int startingFood = 0;
    int startingPopCap = 50;

    // Tech restrictions
    TechLevel maxTechLevel = TechLevel::None;
    std::vector<std::string> disabledUnits;
    std::vector<std::string> disabledBuildings;
    std::vector<std::string> disabledTechs;

    // Victory conditions
    std::vector<VictoryCondition> victoryConditions;

    // Time settings
    bool hasTimeLimit = false;
    float timeLimitMinutes = 30.0f;
    float dayNightCycleSpeed = 1.0f;
    float startTimeOfDay = 0.5f;

    // Special rules
    std::vector<SpecialRule> specialRules;

    // Fog of war
    bool fogOfWarEnabled = true;
    bool exploredMapStart = false;
    bool revealedMapStart = false;

    // Game speed
    float gameSpeed = 1.0f;
    bool allowPause = true;
    bool allowSpeedChange = true;
};

/**
 * @brief Scenario Settings Editor
 *
 * Configures game rules including:
 * - Starting resources
 * - Tech level restrictions
 * - Alliance settings
 * - Victory conditions
 * - Time limits
 * - Special rules
 */
class ScenarioSettings {
public:
    ScenarioSettings();
    ~ScenarioSettings();

    ScenarioSettings(const ScenarioSettings&) = delete;
    ScenarioSettings& operator=(const ScenarioSettings&) = delete;

    bool Initialize(InGameEditor& parent);
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // File operations
    void LoadFromFile(const MapFile& file);
    void SaveToFile(MapFile& file) const;

    void LoadDefaults();
    void ApplyPreset(const std::string& presetName);

    // Update/Render
    void Update(float deltaTime);
    void Render();
    void ProcessInput();

    // Config access
    [[nodiscard]] ScenarioConfig& GetConfig() { return m_config; }
    [[nodiscard]] const ScenarioConfig& GetConfig() const { return m_config; }
    void SetConfig(const ScenarioConfig& config) { m_config = config; }

    // Player management
    void AddPlayerSlot();
    void RemovePlayerSlot(int index);
    void SetPlayerSlot(int index, const PlayerSlot& slot);
    [[nodiscard]] const std::vector<PlayerSlot>& GetPlayerSlots() const { return m_config.playerSlots; }

    // Team management
    void AddTeam(const std::string& name);
    void RemoveTeam(int teamId);
    void AssignPlayerToTeam(int playerId, int teamId);
    [[nodiscard]] const std::vector<Team>& GetTeams() const { return m_config.teams; }

    // Alliance management
    void SetAlliance(int player1, int player2, bool allied, bool sharedVision = false);
    [[nodiscard]] bool AreAllied(int player1, int player2) const;

    // Victory conditions
    void AddVictoryCondition(const VictoryCondition& condition);
    void RemoveVictoryCondition(int index);
    void UpdateVictoryCondition(int index, const VictoryCondition& condition);

    // Tech restrictions
    void SetTechLevel(TechLevel level);
    void DisableUnit(const std::string& unitId);
    void EnableUnit(const std::string& unitId);
    void DisableBuilding(const std::string& buildingId);
    void EnableBuilding(const std::string& buildingId);
    void DisableTech(const std::string& techId);
    void EnableTech(const std::string& techId);

    // Special rules
    void EnableSpecialRule(const std::string& ruleId);
    void DisableSpecialRule(const std::string& ruleId);
    void SetRuleParameter(const std::string& ruleId, const std::string& param, const std::string& value);

    // Presets
    [[nodiscard]] std::vector<std::string> GetPresetNames() const;

private:
    void RenderGeneralSettings();
    void RenderPlayerSettings();
    void RenderTeamSettings();
    void RenderAllianceMatrix();
    void RenderResourceSettings();
    void RenderTechSettings();
    void RenderVictorySettings();
    void RenderTimeSettings();
    void RenderSpecialRules();

    void InitializeSpecialRules();

    bool m_initialized = false;
    InGameEditor* m_parent = nullptr;
    ScenarioConfig m_config;

    // Available special rules
    std::vector<SpecialRule> m_availableRules;

    // Editor state
    int m_selectedTab = 0;
    int m_selectedPlayer = 0;
};

[[nodiscard]] const char* GetVictoryTypeName(VictoryType type);
[[nodiscard]] const char* GetTechLevelName(TechLevel level);

} // namespace Vehement
