#include "MissionManager.hpp"
#include "engine/core/json_wrapper.hpp"
#include "../Resource.hpp"
#include "../ai/AIPlayer.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>

namespace Vehement {
namespace RTS {
namespace Campaign {

MissionManager::MissionManager() = default;
MissionManager::~MissionManager() = default;

MissionManager& MissionManager::Instance() {
    static MissionManager instance;
    return instance;
}

bool MissionManager::Initialize() {
    if (m_initialized) return true;

    m_executionState = MissionExecutionState::NotLoaded;
    m_currentMission = nullptr;
    m_missionTime = 0.0f;
    m_triggers.clear();
    m_eventHandlers.clear();
    m_scriptVariables.clear();

    m_initialized = true;
    return true;
}

void MissionManager::Shutdown() {
    UnloadMission();
    m_eventHandlers.clear();
    m_initialized = false;
}

bool MissionManager::LoadMission(Mission* mission) {
    if (!mission) return false;

    UnloadMission();
    m_currentMission = mission;
    m_executionState = MissionExecutionState::Loading;

    // Load map file
    LoadMapFile(mission->mapFile);

    // Initialize mission state
    InitializeMissionState();

    m_executionState = MissionExecutionState::Briefing;
    return true;
}

bool MissionManager::LoadMissionFromFile(const std::string& missionPath) {
    auto mission = MissionFactory::CreateFromConfig(missionPath);
    if (mission) {
        // Note: This creates a memory management issue - would need proper handling
        return LoadMission(mission.release());
    }
    return false;
}

void MissionManager::UnloadMission() {
    if (m_currentMission) {
        m_currentMission = nullptr;
    }

    m_executionState = MissionExecutionState::NotLoaded;
    m_missionTime = 0.0f;
    m_triggers.clear();
    m_scriptVariables.clear();
}

void MissionManager::StartMission() {
    if (!m_currentMission) return;
    if (m_executionState != MissionExecutionState::Briefing) return;

    m_executionState = MissionExecutionState::Playing;
    m_missionTime = 0.0f;

    // Start mission
    m_currentMission->Start(m_currentDifficulty);

    // Setup game state
    SetupStartingUnits();
    SetupStartingResources();
    SetupAIPlayers();

    // Apply difficulty modifiers
    ApplyDifficultyModifiers();

    // Execute init script
    if (!m_currentMission->initScript.empty()) {
        ExecuteScript(m_currentMission->initScript);
    }

    if (m_onMissionStart) {
        m_onMissionStart();
    }
}

void MissionManager::PauseMission() {
    if (m_executionState == MissionExecutionState::Playing) {
        m_executionState = MissionExecutionState::Paused;
    }
}

void MissionManager::ResumeMission() {
    if (m_executionState == MissionExecutionState::Paused) {
        m_executionState = MissionExecutionState::Playing;
    }
}

void MissionManager::RestartMission() {
    if (!m_currentMission) return;

    m_currentMission->Reset();
    m_missionTime = 0.0f;

    // Reset triggers
    for (auto& trigger : m_triggers) {
        trigger.triggered = false;
        trigger.triggerCount = 0;
    }

    m_executionState = MissionExecutionState::Briefing;
}

void MissionManager::EndMission(bool victory) {
    if (!m_currentMission) return;

    m_executionState = victory ? MissionExecutionState::Victory : MissionExecutionState::Defeat;

    if (victory) {
        m_currentMission->Complete();
        if (!m_currentMission->victoryScript.empty()) {
            ExecuteScript(m_currentMission->victoryScript);
        }
    } else {
        m_currentMission->Fail();
        if (!m_currentMission->defeatScript.empty()) {
            ExecuteScript(m_currentMission->defeatScript);
        }
    }

    if (m_onMissionEnd) {
        m_onMissionEnd(victory);
    }
}

void MissionManager::AbortMission() {
    if (!m_currentMission) return;

    m_currentMission->Reset();
    m_executionState = MissionExecutionState::NotLoaded;
}

void MissionManager::Update(float deltaTime) {
    if (!m_initialized || !m_currentMission) return;
    if (m_executionState != MissionExecutionState::Playing) return;

    m_missionTime += deltaTime;

    // Update mission
    m_currentMission->Update(deltaTime);

    // Update objectives
    UpdateObjectives(deltaTime);

    // Update triggers
    UpdateTriggers();

    // Check win/lose
    CheckVictoryConditions();
    CheckDefeatConditions();

    // Execute update script
    if (!m_currentMission->updateScript.empty()) {
        ExecuteScript(m_currentMission->updateScript);
    }
}

float MissionManager::GetTimeLimit() const {
    return m_currentMission ? m_currentMission->timeLimit : -1.0f;
}

float MissionManager::GetTimeRemaining() const {
    if (!HasTimeLimit()) return -1.0f;
    return std::max(0.0f, m_currentMission->timeLimit - m_missionTime);
}

bool MissionManager::HasTimeLimit() const {
    return m_currentMission && m_currentMission->timeLimit > 0.0f;
}

void MissionManager::ActivateObjective(const std::string& objectiveId) {
    if (!m_currentMission) return;

    m_currentMission->ActivateObjective(objectiveId);

    if (auto* obj = m_currentMission->GetObjective(objectiveId)) {
        if (m_onObjectiveActivate) {
            m_onObjectiveActivate(*obj);
        }
    }
}

void MissionManager::CompleteObjective(const std::string& objectiveId) {
    if (!m_currentMission) return;

    auto* obj = m_currentMission->GetObjective(objectiveId);
    if (!obj) return;

    m_currentMission->CompleteObjective(objectiveId);

    if (m_onObjectiveComplete) {
        m_onObjectiveComplete(*obj);
    }

    // Post event
    MissionEvent event;
    event.type = MissionEventType::ObjectiveCompleted;
    event.sourceId = objectiveId;
    PostEvent(event);
}

void MissionManager::FailObjective(const std::string& objectiveId) {
    if (!m_currentMission) return;

    auto* obj = m_currentMission->GetObjective(objectiveId);
    if (!obj) return;

    m_currentMission->FailObjective(objectiveId);

    if (m_onObjectiveFail) {
        m_onObjectiveFail(*obj);
    }

    // Post event
    MissionEvent event;
    event.type = MissionEventType::ObjectiveFailed;
    event.sourceId = objectiveId;
    PostEvent(event);
}

void MissionManager::UpdateObjectiveProgress(const std::string& objectiveId, int32_t progress) {
    if (!m_currentMission) return;

    m_currentMission->UpdateObjectiveProgress(objectiveId, progress);

    if (auto* obj = m_currentMission->GetObjective(objectiveId)) {
        if (m_onObjectiveProgress) {
            m_onObjectiveProgress(*obj);
        }
    }
}

void MissionManager::SetObjectiveProgress(const std::string& objectiveId, int32_t progress) {
    if (!m_currentMission) return;

    if (auto* obj = m_currentMission->GetObjective(objectiveId)) {
        obj->SetProgress(progress);

        if (m_onObjectiveProgress) {
            m_onObjectiveProgress(*obj);
        }
    }
}

Objective* MissionManager::GetObjective(const std::string& objectiveId) {
    return m_currentMission ? m_currentMission->GetObjective(objectiveId) : nullptr;
}

std::vector<Objective*> MissionManager::GetActiveObjectives() {
    std::vector<Objective*> result;
    if (!m_currentMission) return result;

    for (auto& obj : m_currentMission->objectives) {
        if (obj.IsActive()) {
            result.push_back(&obj);
        }
    }
    return result;
}

std::vector<Objective*> MissionManager::GetPrimaryObjectives() {
    std::vector<Objective*> result;
    if (!m_currentMission) return result;

    for (auto& obj : m_currentMission->objectives) {
        if (obj.priority == ObjectivePriority::Primary) {
            result.push_back(&obj);
        }
    }
    return result;
}

std::vector<Objective*> MissionManager::GetSecondaryObjectives() {
    std::vector<Objective*> result;
    if (!m_currentMission) return result;

    for (auto& obj : m_currentMission->objectives) {
        if (obj.priority == ObjectivePriority::Secondary) {
            result.push_back(&obj);
        }
    }
    return result;
}

std::vector<Objective*> MissionManager::GetBonusObjectives() {
    std::vector<Objective*> result;
    if (!m_currentMission) return result;

    for (auto& obj : m_currentMission->objectives) {
        if (obj.priority == ObjectivePriority::Bonus) {
            result.push_back(&obj);
        }
    }
    return result;
}

void MissionManager::PostEvent(const MissionEvent& event) {
    ProcessEvent(event);
}

void MissionManager::RegisterEventHandler(MissionEventType type,
                                           std::function<void(const MissionEvent&)> handler) {
    m_eventHandlers[type].push_back(handler);
}

void MissionManager::UnregisterEventHandlers(MissionEventType type) {
    m_eventHandlers.erase(type);
}

void MissionManager::AddTrigger(const MissionTrigger& trigger) {
    m_triggers.push_back(trigger);
}

void MissionManager::RemoveTrigger(const std::string& triggerId) {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
        [&triggerId](const MissionTrigger& t) { return t.id == triggerId; });
    if (it != m_triggers.end()) {
        m_triggers.erase(it);
    }
}

void MissionManager::ActivateTrigger(const std::string& triggerId) {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
        [&triggerId](const MissionTrigger& t) { return t.id == triggerId; });

    if (it != m_triggers.end() && !it->triggered) {
        it->triggered = true;
        it->triggerCount++;
        ExecuteScript(it->action);

        if (it->repeatable && (it->maxTriggers < 0 || it->triggerCount < it->maxTriggers)) {
            it->triggered = false;
        }
    }
}

void MissionManager::ResetTrigger(const std::string& triggerId) {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
        [&triggerId](const MissionTrigger& t) { return t.id == triggerId; });
    if (it != m_triggers.end()) {
        it->triggered = false;
    }
}

bool MissionManager::IsTriggerActive(const std::string& triggerId) const {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
        [&triggerId](const MissionTrigger& t) { return t.id == triggerId; });
    return (it != m_triggers.end()) ? it->triggered : false;
}

void MissionManager::ExecuteScript(const std::string& script) {
    // Execute script through mission's script variables system
    // Scripts are stored as key-value commands that can trigger mission events
    if (script.empty()) {
        return;
    }

    // Parse simple script commands (format: "command:arg1,arg2,...")
    // For complex scripts, the engine's ScriptContext system would be used
    std::istringstream ss(script);
    std::string line;

    while (std::getline(ss, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t");
        line = line.substr(start, end - start + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        // Parse command:args format
        size_t colonPos = line.find(':');
        std::string command = (colonPos != std::string::npos) ?
                             line.substr(0, colonPos) : line;
        std::string args = (colonPos != std::string::npos) ?
                          line.substr(colonPos + 1) : "";

        // Execute basic mission commands
        if (command == "activate_objective" && !args.empty()) {
            ActivateObjective(args);
        } else if (command == "complete_objective" && !args.empty()) {
            CompleteObjective(args);
        } else if (command == "fail_objective" && !args.empty()) {
            FailObjective(args);
        } else if (command == "activate_trigger" && !args.empty()) {
            ActivateTrigger(args);
        } else if (command == "set_var") {
            size_t eqPos = args.find('=');
            if (eqPos != std::string::npos) {
                std::string varName = args.substr(0, eqPos);
                std::string varValue = args.substr(eqPos + 1);
                SetScriptVariable(varName, varValue);
            }
        } else if (command == "victory") {
            EndMission(true);
        } else if (command == "defeat") {
            EndMission(false);
        }
        // Additional commands can be added as needed
    }
}

void MissionManager::SetScriptVariable(const std::string& name, const std::string& value) {
    m_scriptVariables[name] = value;
}

std::string MissionManager::GetScriptVariable(const std::string& name) const {
    auto it = m_scriptVariables.find(name);
    return (it != m_scriptVariables.end()) ? it->second : "";
}

void MissionManager::RecordUnitCreated(const std::string& /*unitType*/) {
    if (!m_currentMission) return;
    m_currentMission->statistics.unitsCreated++;
}

void MissionManager::RecordUnitKilled(const std::string& /*unitType*/, bool isEnemy) {
    if (!m_currentMission) return;
    if (isEnemy) {
        m_currentMission->statistics.enemiesKilled++;
    } else {
        m_currentMission->statistics.unitsLost++;
    }
}

void MissionManager::RecordBuildingBuilt(const std::string& /*buildingType*/) {
    if (!m_currentMission) return;
    m_currentMission->statistics.buildingsBuilt++;
}

void MissionManager::RecordBuildingDestroyed(const std::string& /*buildingType*/, bool isEnemy) {
    if (!m_currentMission) return;
    if (!isEnemy) {
        m_currentMission->statistics.buildingsLost++;
    }
}

void MissionManager::RecordResourceGathered(const std::string& /*resourceType*/, int32_t amount) {
    if (!m_currentMission) return;
    m_currentMission->statistics.resourcesGathered += amount;
}

void MissionManager::RecordResourceSpent(const std::string& /*resourceType*/, int32_t amount) {
    if (!m_currentMission) return;
    m_currentMission->statistics.resourcesSpent += amount;
}

void MissionManager::SetAIDifficulty(const std::string& aiId, MissionDifficulty difficulty) {
    if (!m_currentMission) return;

    for (auto& ai : m_currentMission->aiPlayers) {
        if (ai.aiId == aiId) {
            ai.difficulty = difficulty;
            break;
        }
    }
}

void MissionManager::ApplyDifficultyModifiers() {
    if (!m_currentMission) return;

    auto modifiers = m_currentMission->GetDifficultyModifiers();

    // Store modifiers in script variables for game systems to query
    SetScriptVariable("player_damage_mult", std::to_string(modifiers.playerDamageMultiplier));
    SetScriptVariable("enemy_damage_mult", std::to_string(modifiers.enemyDamageMultiplier));
    SetScriptVariable("player_resource_mult", std::to_string(modifiers.playerResourceMultiplier));
    SetScriptVariable("enemy_resource_mult", std::to_string(modifiers.enemyResourceMultiplier));
    SetScriptVariable("time_limit_mult", std::to_string(modifiers.timeLimitMultiplier));
    SetScriptVariable("experience_mult", std::to_string(modifiers.experienceMultiplier));
    SetScriptVariable("extra_enemy_units", std::to_string(modifiers.extraEnemyUnits));
    SetScriptVariable("show_hints", modifiers.showHints ? "true" : "false");
    SetScriptVariable("enable_autosave", modifiers.enableAutoSave ? "true" : "false");

    // Apply time limit multiplier if mission has a time limit
    if (m_currentMission->timeLimit > 0) {
        m_currentMission->timeLimit *= modifiers.timeLimitMultiplier;
    }

    // Apply par time multiplier
    if (m_currentMission->parTime > 0) {
        m_currentMission->parTime *= modifiers.timeLimitMultiplier;
    }

    // Apply difficulty to AI players
    for (auto& ai : m_currentMission->aiPlayers) {
        // Scale AI resources based on difficulty
        ai.resources.gold = static_cast<int32_t>(ai.resources.gold * modifiers.enemyResourceMultiplier);
        ai.resources.wood = static_cast<int32_t>(ai.resources.wood * modifiers.enemyResourceMultiplier);
        ai.resources.stone = static_cast<int32_t>(ai.resources.stone * modifiers.enemyResourceMultiplier);
        ai.resources.metal = static_cast<int32_t>(ai.resources.metal * modifiers.enemyResourceMultiplier);
        ai.resources.food = static_cast<int32_t>(ai.resources.food * modifiers.enemyResourceMultiplier);
    }
}

std::string MissionManager::GetMapFile() const {
    return m_currentMission ? m_currentMission->mapFile : "";
}

std::string MissionManager::GetPlayerStartPosition() const {
    return m_currentMission ? m_currentMission->playerStartPosition : "";
}

void MissionManager::UpdateObjectives(float deltaTime) {
    if (!m_currentMission) return;

    for (auto& objective : m_currentMission->objectives) {
        if (objective.IsActive()) {
            objective.Update(deltaTime);
        }
    }
}

// Helper function to evaluate trigger conditions
bool MissionManager::EvaluateTriggerCondition(const std::string& condition) {
    if (condition.empty()) {
        return false;
    }

    // Parse condition string (format: "type:value" or "type:operator:value")
    // Supported conditions:
    // - time_elapsed:>:300 (mission time > 300 seconds)
    // - objective_complete:objective_id
    // - var_equals:name:value
    // - always (always true - for manual triggers)

    size_t firstColon = condition.find(':');
    if (firstColon == std::string::npos) {
        if (condition == "always") {
            return true;
        }
        return false;
    }

    std::string condType = condition.substr(0, firstColon);
    std::string remainder = condition.substr(firstColon + 1);

    if (condType == "time_elapsed") {
        // Format: time_elapsed:>:300 or time_elapsed:300
        size_t secondColon = remainder.find(':');
        std::string op = ">";
        float targetTime = 0.0f;

        if (secondColon != std::string::npos) {
            op = remainder.substr(0, secondColon);
            targetTime = std::stof(remainder.substr(secondColon + 1));
        } else {
            targetTime = std::stof(remainder);
        }

        if (op == ">") return m_missionTime > targetTime;
        if (op == ">=") return m_missionTime >= targetTime;
        if (op == "<") return m_missionTime < targetTime;
        if (op == "<=") return m_missionTime <= targetTime;
        if (op == "==") return std::abs(m_missionTime - targetTime) < 0.1f;
        return false;
    }

    if (condType == "objective_complete") {
        if (m_currentMission) {
            auto* obj = m_currentMission->GetObjective(remainder);
            return obj && obj->IsCompleted();
        }
        return false;
    }

    if (condType == "objective_failed") {
        if (m_currentMission) {
            auto* obj = m_currentMission->GetObjective(remainder);
            return obj && obj->IsFailed();
        }
        return false;
    }

    if (condType == "objective_active") {
        if (m_currentMission) {
            auto* obj = m_currentMission->GetObjective(remainder);
            return obj && obj->IsActive();
        }
        return false;
    }

    if (condType == "var_equals") {
        // Format: var_equals:name:value
        size_t secondColon = remainder.find(':');
        if (secondColon != std::string::npos) {
            std::string varName = remainder.substr(0, secondColon);
            std::string expectedValue = remainder.substr(secondColon + 1);
            return GetScriptVariable(varName) == expectedValue;
        }
        return false;
    }

    if (condType == "var_set") {
        // Format: var_set:name (true if variable is set and not empty)
        return !GetScriptVariable(remainder).empty();
    }

    return false;
}

void MissionManager::UpdateTriggers() {
    // Evaluate trigger conditions and fire if met
    for (auto& trigger : m_triggers) {
        // Skip already triggered non-repeatable triggers
        if (trigger.triggered && !trigger.repeatable) {
            continue;
        }

        // Skip triggers that have reached max trigger count
        if (trigger.maxTriggers > 0 && trigger.triggerCount >= trigger.maxTriggers) {
            continue;
        }

        // Evaluate the trigger condition
        bool conditionMet = EvaluateTriggerCondition(trigger.condition);

        if (conditionMet && !trigger.triggered) {
            // Fire the trigger
            trigger.triggered = true;
            trigger.triggerCount++;

            // Execute the trigger action
            ExecuteScript(trigger.action);

            // Post trigger event
            MissionEvent event;
            event.type = MissionEventType::TriggerActivated;
            event.sourceId = trigger.id;
            PostEvent(event);

            // Reset for repeatable triggers
            if (trigger.repeatable && (trigger.maxTriggers < 0 || trigger.triggerCount < trigger.maxTriggers)) {
                trigger.triggered = false;
            }
        }
    }
}

void MissionManager::CheckVictoryConditions() {
    if (!m_currentMission) return;

    if (m_currentMission->CheckVictoryCondition()) {
        EndMission(true);
    }
}

void MissionManager::CheckDefeatConditions() {
    if (!m_currentMission) return;

    if (m_currentMission->CheckDefeatCondition()) {
        EndMission(false);
    }
}

void MissionManager::ProcessEvent(const MissionEvent& event) {
    auto it = m_eventHandlers.find(event.type);
    if (it != m_eventHandlers.end()) {
        for (const auto& handler : it->second) {
            handler(event);
        }
    }
}

void MissionManager::InitializeMissionState() {
    if (!m_currentMission) return;

    m_currentMission->Initialize();
    m_currentDifficulty = m_currentMission->currentDifficulty;
}

void MissionManager::LoadMapFile(const std::string& mapFile) {
    // Load map file path into script variables for game systems to access
    // The actual map loading is delegated to the WorldMap or scene loader systems
    if (mapFile.empty()) {
        return;
    }

    // Store map file path for external systems to load
    SetScriptVariable("map_file", mapFile);

    // Parse map file name for convenience
    size_t lastSlash = mapFile.find_last_of("/\\");
    std::string mapName = (lastSlash != std::string::npos) ?
                          mapFile.substr(lastSlash + 1) : mapFile;

    // Remove extension
    size_t dotPos = mapName.find_last_of('.');
    if (dotPos != std::string::npos) {
        mapName = mapName.substr(0, dotPos);
    }

    SetScriptVariable("map_name", mapName);

    // Store player start position if set
    if (m_currentMission && !m_currentMission->playerStartPosition.empty()) {
        SetScriptVariable("player_start", m_currentMission->playerStartPosition);
    }

    // Mark map as ready to load
    SetScriptVariable("map_load_requested", "true");
}

void MissionManager::SetupStartingUnits() {
    // Setup starting units from mission configuration
    // Units are spawned by exposing unit data through script variables
    // Actual spawning is handled by the game's entity/unit system
    if (!m_currentMission) {
        return;
    }

    const auto& unitRestrictions = m_currentMission->unitRestrictions;

    // Store starting units in script variables for spawning system
    int unitIndex = 0;
    for (const auto& unitType : unitRestrictions.startingUnits) {
        std::string varName = "starting_unit_" + std::to_string(unitIndex);
        SetScriptVariable(varName, unitType);
        unitIndex++;
    }
    SetScriptVariable("starting_unit_count", std::to_string(unitIndex));

    // Store available units for unit production restrictions
    int availIndex = 0;
    if (unitRestrictions.allowAllUnits) {
        SetScriptVariable("allow_all_units", "true");
    } else {
        SetScriptVariable("allow_all_units", "false");
        for (const auto& unitType : unitRestrictions.availableUnits) {
            std::string varName = "available_unit_" + std::to_string(availIndex);
            SetScriptVariable(varName, unitType);
            availIndex++;
        }
    }
    SetScriptVariable("available_unit_count", std::to_string(availIndex));

    // Store disabled units
    int disabledIndex = 0;
    for (const auto& unitType : unitRestrictions.disabledUnits) {
        std::string varName = "disabled_unit_" + std::to_string(disabledIndex);
        SetScriptVariable(varName, unitType);
        disabledIndex++;
    }
    SetScriptVariable("disabled_unit_count", std::to_string(disabledIndex));

    // Store unit limits
    for (const auto& [unitType, limit] : unitRestrictions.unitLimits) {
        SetScriptVariable("unit_limit_" + unitType, std::to_string(limit));
    }

    SetScriptVariable("units_setup_complete", "true");
}

void MissionManager::SetupStartingResources() {
    // Setup starting resources from mission configuration
    // Resources are exposed through script variables for the economy system
    if (!m_currentMission) {
        return;
    }

    // Get difficulty-adjusted resources
    MissionResources resources = m_currentMission->GetAdjustedResources();

    // Store starting resources in script variables
    SetScriptVariable("starting_gold", std::to_string(resources.gold));
    SetScriptVariable("starting_wood", std::to_string(resources.wood));
    SetScriptVariable("starting_stone", std::to_string(resources.stone));
    SetScriptVariable("starting_metal", std::to_string(resources.metal));
    SetScriptVariable("starting_food", std::to_string(resources.food));
    SetScriptVariable("starting_supply", std::to_string(resources.supply));
    SetScriptVariable("max_supply", std::to_string(resources.maxSupply));

    // Store building restrictions
    const auto& buildingRestrictions = m_currentMission->buildingRestrictions;

    if (buildingRestrictions.allowAllBuildings) {
        SetScriptVariable("allow_all_buildings", "true");
    } else {
        SetScriptVariable("allow_all_buildings", "false");
        int index = 0;
        for (const auto& buildingType : buildingRestrictions.availableBuildings) {
            SetScriptVariable("available_building_" + std::to_string(index), buildingType);
            index++;
        }
        SetScriptVariable("available_building_count", std::to_string(index));
    }

    // Store starting buildings
    int startingBuildingIndex = 0;
    for (const auto& buildingType : buildingRestrictions.startingBuildings) {
        SetScriptVariable("starting_building_" + std::to_string(startingBuildingIndex), buildingType);
        startingBuildingIndex++;
    }
    SetScriptVariable("starting_building_count", std::to_string(startingBuildingIndex));

    // Store building limits
    for (const auto& [buildingType, limit] : buildingRestrictions.buildingLimits) {
        SetScriptVariable("building_limit_" + buildingType, std::to_string(limit));
    }

    // Store tech restrictions
    const auto& techRestrictions = m_currentMission->techRestrictions;

    if (techRestrictions.allowAllTech) {
        SetScriptVariable("allow_all_tech", "true");
    } else {
        SetScriptVariable("allow_all_tech", "false");
        int techIndex = 0;
        for (const auto& tech : techRestrictions.availableTech) {
            SetScriptVariable("available_tech_" + std::to_string(techIndex), tech);
            techIndex++;
        }
        SetScriptVariable("available_tech_count", std::to_string(techIndex));
    }

    // Store pre-researched tech
    int preresearchedIndex = 0;
    for (const auto& tech : techRestrictions.preresearchedTech) {
        SetScriptVariable("preresearched_tech_" + std::to_string(preresearchedIndex), tech);
        preresearchedIndex++;
    }
    SetScriptVariable("preresearched_tech_count", std::to_string(preresearchedIndex));

    SetScriptVariable("resources_setup_complete", "true");
}

void MissionManager::SetupAIPlayers() {
    // Initialize AI players from mission configuration
    // AI configuration is exposed through script variables for the AI system
    if (!m_currentMission) {
        return;
    }

    const auto& aiPlayers = m_currentMission->aiPlayers;
    int aiCount = static_cast<int>(aiPlayers.size());

    SetScriptVariable("ai_player_count", std::to_string(aiCount));

    for (size_t i = 0; i < aiPlayers.size(); ++i) {
        const auto& ai = aiPlayers[i];
        std::string prefix = "ai_" + std::to_string(i) + "_";

        // Store AI identification
        SetScriptVariable(prefix + "id", ai.aiId);
        SetScriptVariable(prefix + "faction", ai.faction);
        SetScriptVariable(prefix + "personality", ai.personality);
        SetScriptVariable(prefix + "start_position", ai.startingPosition);

        // Store AI difficulty settings
        SetScriptVariable(prefix + "difficulty", std::to_string(static_cast<int>(ai.difficulty)));
        SetScriptVariable(prefix + "handicap", std::to_string(ai.handicap));

        // Store AI relationship flags
        SetScriptVariable(prefix + "is_ally", ai.isAlly ? "true" : "false");
        SetScriptVariable(prefix + "can_be_defeated", ai.canBeDefeated ? "true" : "false");

        // Store AI resources
        SetScriptVariable(prefix + "gold", std::to_string(ai.resources.gold));
        SetScriptVariable(prefix + "wood", std::to_string(ai.resources.wood));
        SetScriptVariable(prefix + "stone", std::to_string(ai.resources.stone));
        SetScriptVariable(prefix + "metal", std::to_string(ai.resources.metal));
        SetScriptVariable(prefix + "food", std::to_string(ai.resources.food));
        SetScriptVariable(prefix + "supply", std::to_string(ai.resources.supply));
        SetScriptVariable(prefix + "max_supply", std::to_string(ai.resources.maxSupply));

        // Store AI defeat trigger script
        if (!ai.defeatTrigger.empty()) {
            SetScriptVariable(prefix + "defeat_trigger", ai.defeatTrigger);
        }
    }

    SetScriptVariable("ai_setup_complete", "true");
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
