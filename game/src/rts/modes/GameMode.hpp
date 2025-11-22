#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <variant>
#include <glm/glm.hpp>

namespace Vehement {

class GameState;
class Player;
class Unit;

/**
 * @brief Game mode rule types
 */
enum class RuleType : uint8_t {
    Boolean,
    Integer,
    Float,
    String,
    Enum
};

/**
 * @brief Game mode rule definition
 */
struct ModeRule {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
    RuleType type;
    std::variant<bool, int, float, std::string> defaultValue;
    std::variant<bool, int, float, std::string> currentValue;
    std::vector<std::string> enumOptions;  // For enum type
    float minValue = 0.0f;                  // For numeric types
    float maxValue = 100.0f;                // For numeric types
    bool allowInGame = false;               // Can be changed during game
};

/**
 * @brief Victory condition definition
 */
struct VictoryCondition {
    std::string id;
    std::string name;
    std::string description;
    bool enabled = true;
    std::unordered_map<std::string, std::variant<bool, int, float, std::string>> parameters;
    std::function<bool(GameState&, int playerId)> checkFunction;
};

/**
 * @brief Defeat condition definition
 */
struct DefeatCondition {
    std::string id;
    std::string name;
    std::string description;
    bool enabled = true;
    std::unordered_map<std::string, std::variant<bool, int, float, std::string>> parameters;
    std::function<bool(GameState&, int playerId)> checkFunction;
};

/**
 * @brief Team configuration
 */
struct TeamConfig {
    int teamId;
    std::string name;
    glm::vec4 color;
    std::vector<int> playerSlots;
    bool sharedVision = true;
    bool sharedControl = false;
    bool sharedResources = false;
};

/**
 * @brief Player slot configuration
 */
struct PlayerSlot {
    int slotId;
    std::string name;
    int teamId = -1;
    glm::vec4 color;
    std::string race;
    int startLocation = -1;  // -1 = random
    bool isComputer = false;
    std::string aiProfile;
    int handicap = 100;  // Percentage
};

/**
 * @brief Game Mode - Base class for all game modes
 *
 * Features:
 * - Customizable rules
 * - Victory/defeat conditions
 * - Team configuration
 * - Player setup
 * - Game flow hooks
 */
class GameMode {
public:
    GameMode();
    virtual ~GameMode() = default;

    // Identity
    virtual std::string GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual std::string GetDescription() const = 0;
    virtual std::string GetCategory() const { return "Custom"; }
    virtual std::string GetIconPath() const { return ""; }

    // Player requirements
    virtual int GetMinPlayers() const { return 2; }
    virtual int GetMaxPlayers() const { return 8; }
    virtual int GetRecommendedPlayers() const { return GetMaxPlayers(); }
    virtual bool AllowsTeams() const { return true; }
    virtual bool AllowsSpectators() const { return true; }

    // Initialization
    virtual void Initialize(GameState& state);
    virtual void SetupPlayers(GameState& state);
    virtual void SetupTeams(GameState& state);
    virtual void OnGameStart(GameState& state);

    // Game loop hooks
    virtual void OnUpdate(GameState& state, float deltaTime);
    virtual void OnPlayerJoin(GameState& state, int playerId);
    virtual void OnPlayerLeave(GameState& state, int playerId);
    virtual void OnUnitCreated(GameState& state, Unit& unit);
    virtual void OnUnitDestroyed(GameState& state, Unit& unit);
    virtual void OnBuildingCreated(GameState& state, Unit& building);
    virtual void OnBuildingDestroyed(GameState& state, Unit& building);

    // Victory/defeat checking
    virtual void CheckVictoryConditions(GameState& state);
    virtual void CheckDefeatConditions(GameState& state);
    virtual void OnPlayerVictory(GameState& state, int playerId);
    virtual void OnPlayerDefeat(GameState& state, int playerId);
    virtual void OnGameEnd(GameState& state);

    // Rules
    const std::vector<ModeRule>& GetRules() const { return m_rules; }
    bool SetRule(const std::string& ruleId, const std::variant<bool, int, float, std::string>& value);
    std::variant<bool, int, float, std::string> GetRule(const std::string& ruleId) const;
    bool GetRuleBool(const std::string& ruleId) const;
    int GetRuleInt(const std::string& ruleId) const;
    float GetRuleFloat(const std::string& ruleId) const;
    std::string GetRuleString(const std::string& ruleId) const;

    // Victory conditions
    const std::vector<VictoryCondition>& GetVictoryConditions() const { return m_victoryConditions; }
    void AddVictoryCondition(const VictoryCondition& condition);
    void RemoveVictoryCondition(const std::string& id);
    void SetVictoryConditionEnabled(const std::string& id, bool enabled);

    // Defeat conditions
    const std::vector<DefeatCondition>& GetDefeatConditions() const { return m_defeatConditions; }
    void AddDefeatCondition(const DefeatCondition& condition);
    void RemoveDefeatCondition(const std::string& id);
    void SetDefeatConditionEnabled(const std::string& id, bool enabled);

    // Team configuration
    const std::vector<TeamConfig>& GetTeams() const { return m_teams; }
    void SetTeams(const std::vector<TeamConfig>& teams);
    void AddTeam(const TeamConfig& team);
    void RemoveTeam(int teamId);

    // Player slots
    const std::vector<PlayerSlot>& GetPlayerSlots() const { return m_playerSlots; }
    void SetPlayerSlots(const std::vector<PlayerSlot>& slots);
    void ConfigurePlayerSlot(int slotId, const PlayerSlot& config);

    // Serialization
    virtual std::string Serialize() const;
    virtual bool Deserialize(const std::string& data);

    // Validation
    virtual bool Validate(std::vector<std::string>& errors) const;

protected:
    void AddRule(const ModeRule& rule);
    void AddDefaultRules();
    void AddDefaultVictoryConditions();
    void AddDefaultDefeatConditions();

    std::vector<ModeRule> m_rules;
    std::vector<VictoryCondition> m_victoryConditions;
    std::vector<DefeatCondition> m_defeatConditions;
    std::vector<TeamConfig> m_teams;
    std::vector<PlayerSlot> m_playerSlots;

    bool m_isInitialized = false;
    float m_gameTime = 0.0f;
    int m_winningPlayer = -1;
    std::vector<int> m_defeatedPlayers;
};

/**
 * @brief Factory for creating game modes
 */
using GameModeFactory = std::function<std::unique_ptr<GameMode>()>;

} // namespace Vehement
