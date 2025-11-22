#include "ScenarioSettings.hpp"
#include "InGameEditor.hpp"
#include "MapFile.hpp"

#include <imgui.h>
#include <algorithm>

namespace Vehement {

const char* GetVictoryTypeName(VictoryType type) {
    switch (type) {
        case VictoryType::LastManStanding: return "Last Man Standing";
        case VictoryType::KillLeader: return "Kill Leader";
        case VictoryType::ControlPoints: return "Control Points";
        case VictoryType::ResourceGoal: return "Resource Goal";
        case VictoryType::BuildWonder: return "Build Wonder";
        case VictoryType::TimeLimit: return "Survive Time Limit";
        case VictoryType::Score: return "Score Threshold";
        case VictoryType::Custom: return "Custom";
        default: return "Unknown";
    }
}

const char* GetTechLevelName(TechLevel level) {
    switch (level) {
        case TechLevel::None: return "No Restrictions";
        case TechLevel::Age1: return "Age 1 Only";
        case TechLevel::Age2: return "Up to Age 2";
        case TechLevel::Age3: return "Up to Age 3";
        case TechLevel::Age4: return "All Ages";
        case TechLevel::Custom: return "Custom";
        default: return "Unknown";
    }
}

ScenarioSettings::ScenarioSettings() = default;
ScenarioSettings::~ScenarioSettings() = default;

bool ScenarioSettings::Initialize(InGameEditor& parent) {
    if (m_initialized) return true;
    m_parent = &parent;
    InitializeSpecialRules();
    LoadDefaults();
    m_initialized = true;
    return true;
}

void ScenarioSettings::Shutdown() {
    m_initialized = false;
    m_parent = nullptr;
}

void ScenarioSettings::LoadFromFile(const MapFile& file) {
    m_config = file.GetScenarioConfig();
}

void ScenarioSettings::SaveToFile(MapFile& file) const {
    file.SetScenarioConfig(m_config);
}

void ScenarioSettings::LoadDefaults() {
    m_config = ScenarioConfig{};
    m_config.gameName = "Custom Game";
    m_config.maxPlayers = 8;

    // Create default player slots
    for (int i = 0; i < 8; ++i) {
        PlayerSlot slot;
        slot.playerId = i;
        slot.name = "Player " + std::to_string(i + 1);
        slot.teamId = i;
        slot.isActive = (i < 2);
        slot.startingGold = 500;
        slot.startingWood = 200;

        static const char* colors[] = {
            "#FF0000", "#0000FF", "#00FF00", "#FFFF00",
            "#FF00FF", "#00FFFF", "#FF8000", "#8000FF"
        };
        slot.color = colors[i];

        m_config.playerSlots.push_back(slot);
    }

    // Default victory condition
    VictoryCondition victory;
    victory.type = VictoryType::LastManStanding;
    victory.description = "Destroy all enemy buildings and units";
    m_config.victoryConditions.push_back(victory);
}

void ScenarioSettings::ApplyPreset(const std::string& presetName) {
    if (presetName == "Standard Melee") {
        LoadDefaults();
    } else if (presetName == "Free For All") {
        LoadDefaults();
        for (int i = 0; i < 8; ++i) {
            m_config.playerSlots[i].teamId = i;
        }
    } else if (presetName == "Team Battle") {
        LoadDefaults();
        for (int i = 0; i < 4; ++i) {
            m_config.playerSlots[i].teamId = 0;
        }
        for (int i = 4; i < 8; ++i) {
            m_config.playerSlots[i].teamId = 1;
        }
    } else if (presetName == "Rich Start") {
        LoadDefaults();
        m_config.startingGold = 2000;
        m_config.startingWood = 1000;
        for (auto& slot : m_config.playerSlots) {
            slot.startingGold = 2000;
            slot.startingWood = 1000;
        }
    } else if (presetName == "Quick Match") {
        LoadDefaults();
        m_config.hasTimeLimit = true;
        m_config.timeLimitMinutes = 15.0f;
        m_config.startingGold = 1500;
        m_config.startingWood = 750;
    }
}

void ScenarioSettings::Update(float deltaTime) {
    if (!m_initialized) return;
}

void ScenarioSettings::Render() {
    if (!m_initialized) return;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Scenario Settings")) {
        // Preset selector
        if (ImGui::BeginCombo("Preset", "Select Preset...")) {
            for (const auto& preset : GetPresetNames()) {
                if (ImGui::Selectable(preset.c_str())) {
                    ApplyPreset(preset);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        // Tabs
        if (ImGui::BeginTabBar("SettingsTabs")) {
            if (ImGui::BeginTabItem("General")) {
                RenderGeneralSettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Players")) {
                RenderPlayerSettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Teams")) {
                RenderTeamSettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Alliances")) {
                RenderAllianceMatrix();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Resources")) {
                RenderResourceSettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tech Tree")) {
                RenderTechSettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Victory")) {
                RenderVictorySettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Time")) {
                RenderTimeSettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Special Rules")) {
                RenderSpecialRules();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void ScenarioSettings::ProcessInput() {
    if (!m_initialized) return;
}

void ScenarioSettings::AddPlayerSlot() {
    if (m_config.playerSlots.size() >= 16) return;

    PlayerSlot slot;
    slot.playerId = static_cast<int>(m_config.playerSlots.size());
    slot.name = "Player " + std::to_string(slot.playerId + 1);
    slot.teamId = slot.playerId;
    m_config.playerSlots.push_back(slot);
}

void ScenarioSettings::RemovePlayerSlot(int index) {
    if (index >= 0 && index < static_cast<int>(m_config.playerSlots.size()) &&
        m_config.playerSlots.size() > 2) {
        m_config.playerSlots.erase(m_config.playerSlots.begin() + index);
    }
}

void ScenarioSettings::SetPlayerSlot(int index, const PlayerSlot& slot) {
    if (index >= 0 && index < static_cast<int>(m_config.playerSlots.size())) {
        m_config.playerSlots[index] = slot;
    }
}

void ScenarioSettings::AddTeam(const std::string& name) {
    Team team;
    team.teamId = static_cast<int>(m_config.teams.size());
    team.name = name;
    m_config.teams.push_back(team);
}

void ScenarioSettings::RemoveTeam(int teamId) {
    m_config.teams.erase(
        std::remove_if(m_config.teams.begin(), m_config.teams.end(),
                      [teamId](const Team& t) { return t.teamId == teamId; }),
        m_config.teams.end());
}

void ScenarioSettings::AssignPlayerToTeam(int playerId, int teamId) {
    for (auto& slot : m_config.playerSlots) {
        if (slot.playerId == playerId) {
            slot.teamId = teamId;
            break;
        }
    }
}

void ScenarioSettings::SetAlliance(int player1, int player2, bool allied, bool sharedVision) {
    // Find existing alliance setting
    for (auto& alliance : m_config.alliances) {
        if ((alliance.player1 == player1 && alliance.player2 == player2) ||
            (alliance.player1 == player2 && alliance.player2 == player1)) {
            alliance.allied = allied;
            alliance.sharedVision = sharedVision;
            return;
        }
    }

    // Create new alliance setting
    AllianceSetting alliance;
    alliance.player1 = player1;
    alliance.player2 = player2;
    alliance.allied = allied;
    alliance.sharedVision = sharedVision;
    m_config.alliances.push_back(alliance);
}

bool ScenarioSettings::AreAllied(int player1, int player2) const {
    if (player1 == player2) return true;

    // Check team
    int team1 = -1, team2 = -1;
    for (const auto& slot : m_config.playerSlots) {
        if (slot.playerId == player1) team1 = slot.teamId;
        if (slot.playerId == player2) team2 = slot.teamId;
    }
    if (team1 >= 0 && team1 == team2) return true;

    // Check explicit alliance
    for (const auto& alliance : m_config.alliances) {
        if ((alliance.player1 == player1 && alliance.player2 == player2) ||
            (alliance.player1 == player2 && alliance.player2 == player1)) {
            return alliance.allied;
        }
    }

    return false;
}

void ScenarioSettings::AddVictoryCondition(const VictoryCondition& condition) {
    m_config.victoryConditions.push_back(condition);
}

void ScenarioSettings::RemoveVictoryCondition(int index) {
    if (index >= 0 && index < static_cast<int>(m_config.victoryConditions.size())) {
        m_config.victoryConditions.erase(m_config.victoryConditions.begin() + index);
    }
}

void ScenarioSettings::UpdateVictoryCondition(int index, const VictoryCondition& condition) {
    if (index >= 0 && index < static_cast<int>(m_config.victoryConditions.size())) {
        m_config.victoryConditions[index] = condition;
    }
}

void ScenarioSettings::SetTechLevel(TechLevel level) {
    m_config.maxTechLevel = level;
}

void ScenarioSettings::DisableUnit(const std::string& unitId) {
    if (std::find(m_config.disabledUnits.begin(), m_config.disabledUnits.end(), unitId) ==
        m_config.disabledUnits.end()) {
        m_config.disabledUnits.push_back(unitId);
    }
}

void ScenarioSettings::EnableUnit(const std::string& unitId) {
    m_config.disabledUnits.erase(
        std::remove(m_config.disabledUnits.begin(), m_config.disabledUnits.end(), unitId),
        m_config.disabledUnits.end());
}

void ScenarioSettings::DisableBuilding(const std::string& buildingId) {
    if (std::find(m_config.disabledBuildings.begin(), m_config.disabledBuildings.end(), buildingId) ==
        m_config.disabledBuildings.end()) {
        m_config.disabledBuildings.push_back(buildingId);
    }
}

void ScenarioSettings::EnableBuilding(const std::string& buildingId) {
    m_config.disabledBuildings.erase(
        std::remove(m_config.disabledBuildings.begin(), m_config.disabledBuildings.end(), buildingId),
        m_config.disabledBuildings.end());
}

void ScenarioSettings::DisableTech(const std::string& techId) {
    if (std::find(m_config.disabledTechs.begin(), m_config.disabledTechs.end(), techId) ==
        m_config.disabledTechs.end()) {
        m_config.disabledTechs.push_back(techId);
    }
}

void ScenarioSettings::EnableTech(const std::string& techId) {
    m_config.disabledTechs.erase(
        std::remove(m_config.disabledTechs.begin(), m_config.disabledTechs.end(), techId),
        m_config.disabledTechs.end());
}

void ScenarioSettings::EnableSpecialRule(const std::string& ruleId) {
    for (auto& rule : m_config.specialRules) {
        if (rule.id == ruleId) {
            rule.enabled = true;
            return;
        }
    }

    // Find in available rules and add
    for (const auto& rule : m_availableRules) {
        if (rule.id == ruleId) {
            SpecialRule newRule = rule;
            newRule.enabled = true;
            m_config.specialRules.push_back(newRule);
            return;
        }
    }
}

void ScenarioSettings::DisableSpecialRule(const std::string& ruleId) {
    for (auto& rule : m_config.specialRules) {
        if (rule.id == ruleId) {
            rule.enabled = false;
            return;
        }
    }
}

void ScenarioSettings::SetRuleParameter(const std::string& ruleId, const std::string& param, const std::string& value) {
    for (auto& rule : m_config.specialRules) {
        if (rule.id == ruleId) {
            rule.parameters[param] = value;
            return;
        }
    }
}

std::vector<std::string> ScenarioSettings::GetPresetNames() const {
    return {
        "Standard Melee",
        "Free For All",
        "Team Battle",
        "Rich Start",
        "Quick Match"
    };
}

void ScenarioSettings::RenderGeneralSettings() {
    char nameBuffer[256];
    strncpy(nameBuffer, m_config.gameName.c_str(), sizeof(nameBuffer) - 1);
    if (ImGui::InputText("Game Name", nameBuffer, sizeof(nameBuffer))) {
        m_config.gameName = nameBuffer;
    }

    char descBuffer[1024];
    strncpy(descBuffer, m_config.gameDescription.c_str(), sizeof(descBuffer) - 1);
    if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer), ImVec2(0, 60))) {
        m_config.gameDescription = descBuffer;
    }

    ImGui::SliderInt("Max Players", &m_config.maxPlayers, 2, 16);
    ImGui::Checkbox("Allow Observers", &m_config.allowObservers);

    ImGui::Separator();
    ImGui::Text("Fog of War");
    ImGui::Checkbox("Fog of War Enabled", &m_config.fogOfWarEnabled);
    ImGui::Checkbox("Explored Map at Start", &m_config.exploredMapStart);
    ImGui::Checkbox("Revealed Map at Start", &m_config.revealedMapStart);

    ImGui::Separator();
    ImGui::Text("Game Speed");
    ImGui::SliderFloat("Speed Multiplier", &m_config.gameSpeed, 0.5f, 3.0f);
    ImGui::Checkbox("Allow Pause", &m_config.allowPause);
    ImGui::Checkbox("Allow Speed Change", &m_config.allowSpeedChange);
}

void ScenarioSettings::RenderPlayerSettings() {
    ImGui::Text("Player Slots");
    ImGui::Separator();

    for (size_t i = 0; i < m_config.playerSlots.size(); ++i) {
        PlayerSlot& slot = m_config.playerSlots[i];
        ImGui::PushID(static_cast<int>(i));

        bool isSelected = (m_selectedPlayer == static_cast<int>(i));
        if (ImGui::CollapsingHeader(slot.name.c_str(),
            isSelected ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {

            char nameBuffer[128];
            strncpy(nameBuffer, slot.name.c_str(), sizeof(nameBuffer) - 1);
            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
                slot.name = nameBuffer;
            }

            ImGui::Checkbox("Active", &slot.isActive);
            ImGui::SameLine();
            ImGui::Checkbox("Human", &slot.isHuman);

            if (!slot.isHuman) {
                const char* difficulties[] = {"easy", "normal", "hard", "insane"};
                int diffIndex = 1;
                for (int d = 0; d < 4; ++d) {
                    if (slot.aiDifficulty == difficulties[d]) {
                        diffIndex = d;
                        break;
                    }
                }
                if (ImGui::Combo("AI Difficulty", &diffIndex, difficulties, 4)) {
                    slot.aiDifficulty = difficulties[diffIndex];
                }
            }

            ImGui::SliderInt("Team", &slot.teamId, 0, 7);

            // Color picker (simplified)
            ImGui::Text("Color: %s", slot.color.c_str());
        }

        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("Add Player") && m_config.playerSlots.size() < 16) {
        AddPlayerSlot();
    }
}

void ScenarioSettings::RenderTeamSettings() {
    ImGui::Text("Team Configuration");
    ImGui::Separator();

    // Display team assignments
    std::unordered_map<int, std::vector<PlayerSlot*>> teamMembers;
    for (auto& slot : m_config.playerSlots) {
        if (slot.isActive) {
            teamMembers[slot.teamId].push_back(&slot);
        }
    }

    for (auto& [teamId, members] : teamMembers) {
        if (ImGui::TreeNode(("Team " + std::to_string(teamId + 1)).c_str())) {
            for (auto* member : members) {
                ImGui::BulletText("%s", member->name.c_str());
            }
            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    ImGui::Text("Team Options");

    // Find or create team settings
    for (auto& team : m_config.teams) {
        ImGui::PushID(team.teamId);
        if (ImGui::TreeNode(team.name.c_str())) {
            ImGui::Checkbox("Shared Vision", &team.sharedVision);
            ImGui::Checkbox("Shared Control", &team.sharedControl);
            ImGui::Checkbox("Shared Resources", &team.sharedResources);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (ImGui::Button("Add Team")) {
        AddTeam("Team " + std::to_string(m_config.teams.size() + 1));
    }
}

void ScenarioSettings::RenderAllianceMatrix() {
    ImGui::Text("Alliance Matrix");
    ImGui::Separator();

    int numPlayers = 0;
    for (const auto& slot : m_config.playerSlots) {
        if (slot.isActive) numPlayers++;
    }

    if (numPlayers < 2) {
        ImGui::Text("Need at least 2 active players");
        return;
    }

    // Draw header row
    ImGui::Text("     ");
    for (const auto& slot : m_config.playerSlots) {
        if (slot.isActive) {
            ImGui::SameLine();
            ImGui::Text("P%d", slot.playerId + 1);
        }
    }

    // Draw matrix
    for (const auto& slot1 : m_config.playerSlots) {
        if (!slot1.isActive) continue;

        ImGui::Text("P%d", slot1.playerId + 1);

        for (const auto& slot2 : m_config.playerSlots) {
            if (!slot2.isActive) continue;

            ImGui::SameLine();
            ImGui::PushID(slot1.playerId * 100 + slot2.playerId);

            if (slot1.playerId == slot2.playerId) {
                ImGui::Text("  -");
            } else {
                bool allied = AreAllied(slot1.playerId, slot2.playerId);
                if (ImGui::Checkbox("##allied", &allied)) {
                    SetAlliance(slot1.playerId, slot2.playerId, allied);
                }
            }

            ImGui::PopID();
        }
    }
}

void ScenarioSettings::RenderResourceSettings() {
    ImGui::Text("Starting Resources");
    ImGui::Separator();

    ImGui::Text("Default Starting Resources:");
    ImGui::SliderInt("Gold##default", &m_config.startingGold, 0, 10000);
    ImGui::SliderInt("Wood##default", &m_config.startingWood, 0, 10000);
    ImGui::SliderInt("Food##default", &m_config.startingFood, 0, 1000);
    ImGui::SliderInt("Population Cap", &m_config.startingPopCap, 10, 200);

    if (ImGui::Button("Apply to All Players")) {
        for (auto& slot : m_config.playerSlots) {
            slot.startingGold = m_config.startingGold;
            slot.startingWood = m_config.startingWood;
            slot.startingFood = m_config.startingFood;
        }
    }

    ImGui::Separator();
    ImGui::Text("Per-Player Resources:");

    for (auto& slot : m_config.playerSlots) {
        if (!slot.isActive) continue;

        ImGui::PushID(slot.playerId);
        if (ImGui::TreeNode(slot.name.c_str())) {
            ImGui::SliderInt("Gold", &slot.startingGold, 0, 10000);
            ImGui::SliderInt("Wood", &slot.startingWood, 0, 10000);
            ImGui::SliderInt("Food", &slot.startingFood, 0, 1000);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void ScenarioSettings::RenderTechSettings() {
    ImGui::Text("Technology Restrictions");
    ImGui::Separator();

    // Tech level
    int techLevel = static_cast<int>(m_config.maxTechLevel);
    const char* techLevels[] = {"No Restrictions", "Age 1 Only", "Up to Age 2", "Up to Age 3", "All Ages", "Custom"};
    if (ImGui::Combo("Max Tech Level", &techLevel, techLevels, 6)) {
        m_config.maxTechLevel = static_cast<TechLevel>(techLevel);
    }

    ImGui::Separator();

    // Disabled units
    if (ImGui::CollapsingHeader("Disabled Units")) {
        for (const auto& unitId : m_config.disabledUnits) {
            ImGui::BulletText("%s", unitId.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(("Enable##" + unitId).c_str())) {
                EnableUnit(unitId);
            }
        }
    }

    // Disabled buildings
    if (ImGui::CollapsingHeader("Disabled Buildings")) {
        for (const auto& buildingId : m_config.disabledBuildings) {
            ImGui::BulletText("%s", buildingId.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(("Enable##" + buildingId).c_str())) {
                EnableBuilding(buildingId);
            }
        }
    }

    // Disabled techs
    if (ImGui::CollapsingHeader("Disabled Technologies")) {
        for (const auto& techId : m_config.disabledTechs) {
            ImGui::BulletText("%s", techId.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(("Enable##" + techId).c_str())) {
                EnableTech(techId);
            }
        }
    }
}

void ScenarioSettings::RenderVictorySettings() {
    ImGui::Text("Victory Conditions");
    ImGui::Separator();

    if (ImGui::Button("+ Add Condition")) {
        VictoryCondition cond;
        cond.type = VictoryType::LastManStanding;
        cond.description = "New victory condition";
        AddVictoryCondition(cond);
    }

    ImGui::Separator();

    for (size_t i = 0; i < m_config.victoryConditions.size(); ++i) {
        VictoryCondition& cond = m_config.victoryConditions[i];
        ImGui::PushID(static_cast<int>(i));

        if (ImGui::CollapsingHeader(GetVictoryTypeName(cond.type), ImGuiTreeNodeFlags_DefaultOpen)) {
            // Type selector
            int typeIndex = static_cast<int>(cond.type);
            const char* types[] = {
                "Last Man Standing", "Kill Leader", "Control Points",
                "Resource Goal", "Build Wonder", "Survive Time", "Score", "Custom"
            };
            if (ImGui::Combo("Type", &typeIndex, types, 8)) {
                cond.type = static_cast<VictoryType>(typeIndex);
            }

            // Type-specific parameters
            switch (cond.type) {
                case VictoryType::ControlPoints:
                    ImGui::InputInt("Points Needed", &cond.targetAmount);
                    break;
                case VictoryType::ResourceGoal:
                    ImGui::InputInt("Resource Amount", &cond.targetAmount);
                    break;
                case VictoryType::TimeLimit:
                    ImGui::InputFloat("Time (minutes)", &cond.timeLimit);
                    break;
                case VictoryType::Score:
                    ImGui::InputInt("Score Threshold", &cond.targetAmount);
                    break;
                default:
                    break;
            }

            char descBuffer[256];
            strncpy(descBuffer, cond.description.c_str(), sizeof(descBuffer) - 1);
            if (ImGui::InputText("Description", descBuffer, sizeof(descBuffer))) {
                cond.description = descBuffer;
            }

            ImGui::Checkbox("Enabled", &cond.enabled);
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                RemoveVictoryCondition(static_cast<int>(i));
            }
        }

        ImGui::PopID();
    }
}

void ScenarioSettings::RenderTimeSettings() {
    ImGui::Text("Time Settings");
    ImGui::Separator();

    ImGui::Checkbox("Time Limit", &m_config.hasTimeLimit);
    if (m_config.hasTimeLimit) {
        ImGui::SliderFloat("Time Limit (minutes)", &m_config.timeLimitMinutes, 5.0f, 120.0f);
    }

    ImGui::Separator();
    ImGui::Text("Day/Night Cycle");
    ImGui::SliderFloat("Cycle Speed", &m_config.dayNightCycleSpeed, 0.0f, 5.0f);
    ImGui::SliderFloat("Starting Time of Day", &m_config.startTimeOfDay, 0.0f, 1.0f);

    // Time of day preview
    const char* timeDesc = "Night";
    if (m_config.startTimeOfDay > 0.25f && m_config.startTimeOfDay < 0.75f) {
        timeDesc = "Day";
    } else if (m_config.startTimeOfDay >= 0.2f && m_config.startTimeOfDay <= 0.25f) {
        timeDesc = "Dawn";
    } else if (m_config.startTimeOfDay >= 0.75f && m_config.startTimeOfDay <= 0.8f) {
        timeDesc = "Dusk";
    }
    ImGui::Text("Starting at: %s", timeDesc);
}

void ScenarioSettings::RenderSpecialRules() {
    ImGui::Text("Special Rules");
    ImGui::Separator();

    for (auto& rule : m_availableRules) {
        ImGui::PushID(rule.id.c_str());

        // Find if this rule is enabled
        bool enabled = false;
        for (const auto& activeRule : m_config.specialRules) {
            if (activeRule.id == rule.id && activeRule.enabled) {
                enabled = true;
                break;
            }
        }

        if (ImGui::Checkbox(rule.name.c_str(), &enabled)) {
            if (enabled) {
                EnableSpecialRule(rule.id);
            } else {
                DisableSpecialRule(rule.id);
            }
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "- %s", rule.description.c_str());

        ImGui::PopID();
    }
}

void ScenarioSettings::InitializeSpecialRules() {
    m_availableRules.clear();

    {
        SpecialRule rule;
        rule.id = "no_rush";
        rule.name = "No Rush";
        rule.description = "Players cannot attack for the first 10 minutes";
        rule.parameters["duration"] = "10";
        m_availableRules.push_back(rule);
    }
    {
        SpecialRule rule;
        rule.id = "reveal_map";
        rule.name = "Revealed Map";
        rule.description = "Entire map is visible from start";
        m_availableRules.push_back(rule);
    }
    {
        SpecialRule rule;
        rule.id = "sudden_death";
        rule.name = "Sudden Death";
        rule.description = "Lose when your main building is destroyed";
        m_availableRules.push_back(rule);
    }
    {
        SpecialRule rule;
        rule.id = "hero_mode";
        rule.name = "Hero Mode";
        rule.description = "Each player starts with a powerful hero unit";
        m_availableRules.push_back(rule);
    }
    {
        SpecialRule rule;
        rule.id = "resource_rich";
        rule.name = "Resource Rich";
        rule.description = "All resource nodes have double capacity";
        m_availableRules.push_back(rule);
    }
    {
        SpecialRule rule;
        rule.id = "fast_build";
        rule.name = "Fast Build";
        rule.description = "Buildings construct 50% faster";
        m_availableRules.push_back(rule);
    }
    {
        SpecialRule rule;
        rule.id = "regicide";
        rule.name = "Regicide";
        rule.description = "Each player has a King unit that must be protected";
        m_availableRules.push_back(rule);
    }
}

} // namespace Vehement
