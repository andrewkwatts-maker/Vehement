#include "GameMode.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace Vehement {

GameMode::GameMode() {
    AddDefaultRules();
    AddDefaultVictoryConditions();
    AddDefaultDefeatConditions();
}

void GameMode::Initialize(GameState& state) {
    m_isInitialized = true;
    m_gameTime = 0.0f;
    m_winningPlayer = -1;
    m_defeatedPlayers.clear();
}

void GameMode::SetupPlayers(GameState& state) {
    // Default implementation - assign players to slots
}

void GameMode::SetupTeams(GameState& state) {
    // Default implementation - create teams from config
}

void GameMode::OnGameStart(GameState& state) {
    // Hook for subclasses
}

void GameMode::OnUpdate(GameState& state, float deltaTime) {
    m_gameTime += deltaTime;

    // Check victory/defeat periodically
    CheckVictoryConditions(state);
    CheckDefeatConditions(state);
}

void GameMode::OnPlayerJoin(GameState& state, int playerId) {
    // Hook for subclasses
}

void GameMode::OnPlayerLeave(GameState& state, int playerId) {
    // Default: treat leaving as defeat
    OnPlayerDefeat(state, playerId);
}

void GameMode::OnUnitCreated(GameState& state, Unit& unit) {
    // Hook for subclasses
}

void GameMode::OnUnitDestroyed(GameState& state, Unit& unit) {
    // Hook for subclasses
}

void GameMode::OnBuildingCreated(GameState& state, Unit& building) {
    // Hook for subclasses
}

void GameMode::OnBuildingDestroyed(GameState& state, Unit& building) {
    // Hook for subclasses
}

void GameMode::CheckVictoryConditions(GameState& state) {
    if (m_winningPlayer >= 0) return;  // Already have a winner

    for (auto& condition : m_victoryConditions) {
        if (!condition.enabled || !condition.checkFunction) continue;

        // Check each active player
        for (const auto& slot : m_playerSlots) {
            if (std::find(m_defeatedPlayers.begin(), m_defeatedPlayers.end(), slot.slotId)
                != m_defeatedPlayers.end()) {
                continue;  // Skip defeated players
            }

            if (condition.checkFunction(state, slot.slotId)) {
                OnPlayerVictory(state, slot.slotId);
                return;
            }
        }
    }
}

void GameMode::CheckDefeatConditions(GameState& state) {
    for (auto& condition : m_defeatConditions) {
        if (!condition.enabled || !condition.checkFunction) continue;

        // Check each active player
        for (const auto& slot : m_playerSlots) {
            if (std::find(m_defeatedPlayers.begin(), m_defeatedPlayers.end(), slot.slotId)
                != m_defeatedPlayers.end()) {
                continue;  // Skip already defeated players
            }

            if (condition.checkFunction(state, slot.slotId)) {
                OnPlayerDefeat(state, slot.slotId);
            }
        }
    }
}

void GameMode::OnPlayerVictory(GameState& state, int playerId) {
    m_winningPlayer = playerId;

    // Mark all other players as defeated
    for (const auto& slot : m_playerSlots) {
        if (slot.slotId != playerId) {
            if (std::find(m_defeatedPlayers.begin(), m_defeatedPlayers.end(), slot.slotId)
                == m_defeatedPlayers.end()) {
                m_defeatedPlayers.push_back(slot.slotId);
            }
        }
    }

    OnGameEnd(state);
}

void GameMode::OnPlayerDefeat(GameState& state, int playerId) {
    if (std::find(m_defeatedPlayers.begin(), m_defeatedPlayers.end(), playerId)
        != m_defeatedPlayers.end()) {
        return;  // Already defeated
    }

    m_defeatedPlayers.push_back(playerId);

    // Check if only one player/team remains
    int activePlayers = 0;
    int lastActivePlayer = -1;

    for (const auto& slot : m_playerSlots) {
        if (std::find(m_defeatedPlayers.begin(), m_defeatedPlayers.end(), slot.slotId)
            == m_defeatedPlayers.end()) {
            activePlayers++;
            lastActivePlayer = slot.slotId;
        }
    }

    if (activePlayers == 1) {
        OnPlayerVictory(state, lastActivePlayer);
    }
}

void GameMode::OnGameEnd(GameState& state) {
    // Hook for subclasses - show results, save stats, etc.
}

bool GameMode::SetRule(const std::string& ruleId, const std::variant<bool, int, float, std::string>& value) {
    for (auto& rule : m_rules) {
        if (rule.id == ruleId) {
            rule.currentValue = value;
            return true;
        }
    }
    return false;
}

std::variant<bool, int, float, std::string> GameMode::GetRule(const std::string& ruleId) const {
    for (const auto& rule : m_rules) {
        if (rule.id == ruleId) {
            return rule.currentValue;
        }
    }
    return std::string{};
}

bool GameMode::GetRuleBool(const std::string& ruleId) const {
    auto value = GetRule(ruleId);
    return std::holds_alternative<bool>(value) ? std::get<bool>(value) : false;
}

int GameMode::GetRuleInt(const std::string& ruleId) const {
    auto value = GetRule(ruleId);
    return std::holds_alternative<int>(value) ? std::get<int>(value) : 0;
}

float GameMode::GetRuleFloat(const std::string& ruleId) const {
    auto value = GetRule(ruleId);
    return std::holds_alternative<float>(value) ? std::get<float>(value) : 0.0f;
}

std::string GameMode::GetRuleString(const std::string& ruleId) const {
    auto value = GetRule(ruleId);
    return std::holds_alternative<std::string>(value) ? std::get<std::string>(value) : "";
}

void GameMode::AddVictoryCondition(const VictoryCondition& condition) {
    m_victoryConditions.push_back(condition);
}

void GameMode::RemoveVictoryCondition(const std::string& id) {
    m_victoryConditions.erase(
        std::remove_if(m_victoryConditions.begin(), m_victoryConditions.end(),
            [&id](const VictoryCondition& c) { return c.id == id; }),
        m_victoryConditions.end());
}

void GameMode::SetVictoryConditionEnabled(const std::string& id, bool enabled) {
    for (auto& condition : m_victoryConditions) {
        if (condition.id == id) {
            condition.enabled = enabled;
            break;
        }
    }
}

void GameMode::AddDefeatCondition(const DefeatCondition& condition) {
    m_defeatConditions.push_back(condition);
}

void GameMode::RemoveDefeatCondition(const std::string& id) {
    m_defeatConditions.erase(
        std::remove_if(m_defeatConditions.begin(), m_defeatConditions.end(),
            [&id](const DefeatCondition& c) { return c.id == id; }),
        m_defeatConditions.end());
}

void GameMode::SetDefeatConditionEnabled(const std::string& id, bool enabled) {
    for (auto& condition : m_defeatConditions) {
        if (condition.id == id) {
            condition.enabled = enabled;
            break;
        }
    }
}

void GameMode::SetTeams(const std::vector<TeamConfig>& teams) {
    m_teams = teams;
}

void GameMode::AddTeam(const TeamConfig& team) {
    m_teams.push_back(team);
}

void GameMode::RemoveTeam(int teamId) {
    m_teams.erase(
        std::remove_if(m_teams.begin(), m_teams.end(),
            [teamId](const TeamConfig& t) { return t.teamId == teamId; }),
        m_teams.end());
}

void GameMode::SetPlayerSlots(const std::vector<PlayerSlot>& slots) {
    m_playerSlots = slots;
}

void GameMode::ConfigurePlayerSlot(int slotId, const PlayerSlot& config) {
    for (auto& slot : m_playerSlots) {
        if (slot.slotId == slotId) {
            slot = config;
            return;
        }
    }
    m_playerSlots.push_back(config);
}

std::string GameMode::Serialize() const {
    nlohmann::json j;

    j["id"] = GetId();
    j["name"] = GetName();

    // Rules
    nlohmann::json rulesJson = nlohmann::json::array();
    for (const auto& rule : m_rules) {
        nlohmann::json ruleJson;
        ruleJson["id"] = rule.id;

        if (std::holds_alternative<bool>(rule.currentValue)) {
            ruleJson["value"] = std::get<bool>(rule.currentValue);
        } else if (std::holds_alternative<int>(rule.currentValue)) {
            ruleJson["value"] = std::get<int>(rule.currentValue);
        } else if (std::holds_alternative<float>(rule.currentValue)) {
            ruleJson["value"] = std::get<float>(rule.currentValue);
        } else if (std::holds_alternative<std::string>(rule.currentValue)) {
            ruleJson["value"] = std::get<std::string>(rule.currentValue);
        }
        rulesJson.push_back(ruleJson);
    }
    j["rules"] = rulesJson;

    // Victory conditions
    nlohmann::json victoryJson = nlohmann::json::array();
    for (const auto& vc : m_victoryConditions) {
        nlohmann::json vcJson;
        vcJson["id"] = vc.id;
        vcJson["enabled"] = vc.enabled;
        victoryJson.push_back(vcJson);
    }
    j["victoryConditions"] = victoryJson;

    // Teams
    nlohmann::json teamsJson = nlohmann::json::array();
    for (const auto& team : m_teams) {
        nlohmann::json teamJson;
        teamJson["id"] = team.teamId;
        teamJson["name"] = team.name;
        teamJson["color"] = {team.color.r, team.color.g, team.color.b, team.color.a};
        teamJson["players"] = team.playerSlots;
        teamJson["sharedVision"] = team.sharedVision;
        teamJson["sharedControl"] = team.sharedControl;
        teamJson["sharedResources"] = team.sharedResources;
        teamsJson.push_back(teamJson);
    }
    j["teams"] = teamsJson;

    // Player slots
    nlohmann::json slotsJson = nlohmann::json::array();
    for (const auto& slot : m_playerSlots) {
        nlohmann::json slotJson;
        slotJson["id"] = slot.slotId;
        slotJson["name"] = slot.name;
        slotJson["team"] = slot.teamId;
        slotJson["color"] = {slot.color.r, slot.color.g, slot.color.b, slot.color.a};
        slotJson["race"] = slot.race;
        slotJson["startLocation"] = slot.startLocation;
        slotJson["isComputer"] = slot.isComputer;
        slotJson["aiProfile"] = slot.aiProfile;
        slotJson["handicap"] = slot.handicap;
        slotsJson.push_back(slotJson);
    }
    j["playerSlots"] = slotsJson;

    return j.dump(2);
}

bool GameMode::Deserialize(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);

        // Rules
        if (j.contains("rules")) {
            for (const auto& ruleJson : j["rules"]) {
                std::string ruleId = ruleJson["id"];
                if (ruleJson["value"].is_boolean()) {
                    SetRule(ruleId, ruleJson["value"].get<bool>());
                } else if (ruleJson["value"].is_number_integer()) {
                    SetRule(ruleId, ruleJson["value"].get<int>());
                } else if (ruleJson["value"].is_number_float()) {
                    SetRule(ruleId, ruleJson["value"].get<float>());
                } else if (ruleJson["value"].is_string()) {
                    SetRule(ruleId, ruleJson["value"].get<std::string>());
                }
            }
        }

        // Victory conditions
        if (j.contains("victoryConditions")) {
            for (const auto& vcJson : j["victoryConditions"]) {
                SetVictoryConditionEnabled(vcJson["id"], vcJson["enabled"]);
            }
        }

        // Teams
        if (j.contains("teams")) {
            m_teams.clear();
            for (const auto& teamJson : j["teams"]) {
                TeamConfig team;
                team.teamId = teamJson["id"];
                team.name = teamJson["name"];
                if (teamJson.contains("color") && teamJson["color"].is_array()) {
                    auto c = teamJson["color"];
                    team.color = glm::vec4(c[0], c[1], c[2], c[3]);
                }
                if (teamJson.contains("players")) {
                    team.playerSlots = teamJson["players"].get<std::vector<int>>();
                }
                team.sharedVision = teamJson.value("sharedVision", true);
                team.sharedControl = teamJson.value("sharedControl", false);
                team.sharedResources = teamJson.value("sharedResources", false);
                m_teams.push_back(team);
            }
        }

        // Player slots
        if (j.contains("playerSlots")) {
            m_playerSlots.clear();
            for (const auto& slotJson : j["playerSlots"]) {
                PlayerSlot slot;
                slot.slotId = slotJson["id"];
                slot.name = slotJson["name"];
                slot.teamId = slotJson.value("team", -1);
                if (slotJson.contains("color") && slotJson["color"].is_array()) {
                    auto c = slotJson["color"];
                    slot.color = glm::vec4(c[0], c[1], c[2], c[3]);
                }
                slot.race = slotJson.value("race", "random");
                slot.startLocation = slotJson.value("startLocation", -1);
                slot.isComputer = slotJson.value("isComputer", false);
                slot.aiProfile = slotJson.value("aiProfile", "");
                slot.handicap = slotJson.value("handicap", 100);
                m_playerSlots.push_back(slot);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool GameMode::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    // Check player count
    if (m_playerSlots.size() < static_cast<size_t>(GetMinPlayers())) {
        errors.push_back("Not enough player slots configured");
        valid = false;
    }

    if (m_playerSlots.size() > static_cast<size_t>(GetMaxPlayers())) {
        errors.push_back("Too many player slots configured");
        valid = false;
    }

    // Check for at least one victory condition
    bool hasEnabledVictory = false;
    for (const auto& vc : m_victoryConditions) {
        if (vc.enabled) {
            hasEnabledVictory = true;
            break;
        }
    }
    if (!hasEnabledVictory) {
        errors.push_back("No victory conditions enabled");
        valid = false;
    }

    return valid;
}

void GameMode::AddRule(const ModeRule& rule) {
    m_rules.push_back(rule);
}

void GameMode::AddDefaultRules() {
    // Fog of war
    AddRule({
        "fog_of_war", "Fog of War", "Hide unexplored and non-visible areas",
        "Visibility", RuleType::Boolean, true, true, {}, 0, 0, false
    });

    // Starting resources
    AddRule({
        "starting_gold", "Starting Gold", "Gold each player starts with",
        "Resources", RuleType::Integer, 500, 500, {}, 0, 10000, false
    });

    AddRule({
        "starting_wood", "Starting Wood", "Wood each player starts with",
        "Resources", RuleType::Integer, 200, 200, {}, 0, 10000, false
    });

    // Game speed
    AddRule({
        "game_speed", "Game Speed", "Game speed multiplier",
        "General", RuleType::Float, 1.0f, 1.0f, {}, 0.5f, 3.0f, true
    });

    // Time limit
    AddRule({
        "time_limit", "Time Limit (minutes)", "Maximum game duration (0 = unlimited)",
        "General", RuleType::Integer, 0, 0, {}, 0, 120, false
    });

    // Hero settings
    AddRule({
        "hero_respawn", "Hero Respawn", "Allow heroes to respawn at altars",
        "Heroes", RuleType::Boolean, true, true, {}, 0, 0, false
    });

    AddRule({
        "max_heroes", "Max Heroes", "Maximum heroes per player",
        "Heroes", RuleType::Integer, 3, 3, {}, 1, 5, false
    });
}

void GameMode::AddDefaultVictoryConditions() {
    // Destroy all enemy buildings
    VictoryCondition destroyBuildings;
    destroyBuildings.id = "destroy_buildings";
    destroyBuildings.name = "Destroy All Enemy Buildings";
    destroyBuildings.description = "Win by destroying all enemy structures";
    destroyBuildings.enabled = true;
    destroyBuildings.checkFunction = [](GameState& state, int playerId) -> bool {
        // Check if all enemy buildings are destroyed
        // Implementation depends on GameState
        return false;
    };
    AddVictoryCondition(destroyBuildings);

    // Destroy all enemy units
    VictoryCondition destroyUnits;
    destroyUnits.id = "destroy_units";
    destroyUnits.name = "Destroy All Enemy Units";
    destroyUnits.description = "Win by eliminating all enemy units";
    destroyUnits.enabled = true;
    destroyUnits.checkFunction = [](GameState& state, int playerId) -> bool {
        return false;
    };
    AddVictoryCondition(destroyUnits);
}

void GameMode::AddDefaultDefeatConditions() {
    // No buildings remaining
    DefeatCondition noBuildings;
    noBuildings.id = "no_buildings";
    noBuildings.name = "No Buildings";
    noBuildings.description = "Defeat when all buildings are destroyed";
    noBuildings.enabled = true;
    noBuildings.checkFunction = [](GameState& state, int playerId) -> bool {
        return false;
    };
    AddDefeatCondition(noBuildings);

    // No units remaining
    DefeatCondition noUnits;
    noUnits.id = "no_units";
    noUnits.name = "No Units";
    noUnits.description = "Defeat when all units are destroyed";
    noUnits.enabled = false;  // Disabled by default
    noUnits.checkFunction = [](GameState& state, int playerId) -> bool {
        return false;
    };
    AddDefeatCondition(noUnits);
}

} // namespace Vehement
