#include "MissionManager.hpp"
#include <algorithm>

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

void MissionManager::ExecuteScript(const std::string& /*script*/) {
    // TODO: Execute script through scripting system
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
    // TODO: Apply modifiers to game systems
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

void MissionManager::UpdateTriggers() {
    // TODO: Evaluate trigger conditions and fire if met
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

void MissionManager::LoadMapFile(const std::string& /*mapFile*/) {
    // TODO: Load map through world/map system
}

void MissionManager::SetupStartingUnits() {
    // TODO: Spawn starting units based on mission config
}

void MissionManager::SetupStartingResources() {
    // TODO: Apply starting resources from mission config
}

void MissionManager::SetupAIPlayers() {
    // TODO: Initialize AI players based on mission config
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
