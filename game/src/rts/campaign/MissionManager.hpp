#pragma once

#include "Mission.hpp"
#include <string>
#include <functional>
#include <memory>
#include <map>

namespace Vehement {
namespace RTS {
namespace Campaign {

/**
 * @brief Mission execution state
 */
enum class MissionExecutionState : uint8_t {
    NotLoaded,
    Loading,
    Briefing,
    Playing,
    Paused,
    Victory,
    Defeat,
    Transitioning
};

/**
 * @brief Mission event types for triggers
 */
enum class MissionEventType : uint8_t {
    UnitKilled,
    UnitCreated,
    BuildingDestroyed,
    BuildingCompleted,
    ResourceCollected,
    LocationReached,
    TimerExpired,
    ObjectiveCompleted,
    ObjectiveFailed,
    TriggerActivated,
    Custom
};

/**
 * @brief Mission event data
 */
struct MissionEvent {
    MissionEventType type;
    std::string sourceId;
    std::string targetId;
    float x = 0.0f, y = 0.0f;
    int32_t value = 0;
    std::string customData;
};

/**
 * @brief Mission trigger condition
 */
struct MissionTrigger {
    std::string id;
    std::string condition;              ///< Condition expression
    std::string action;                 ///< Action to execute
    bool repeatable = false;
    bool triggered = false;
    int32_t triggerCount = 0;
    int32_t maxTriggers = 1;            ///< -1 for unlimited
};

/**
 * @brief Mission manager - handles active mission execution
 */
class MissionManager {
public:
    MissionManager();
    ~MissionManager();

    // Singleton access
    [[nodiscard]] static MissionManager& Instance();

    // Delete copy/move
    MissionManager(const MissionManager&) = delete;
    MissionManager& operator=(const MissionManager&) = delete;
    MissionManager(MissionManager&&) = delete;
    MissionManager& operator=(MissionManager&&) = delete;

    // Initialization
    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // Mission loading
    bool LoadMission(Mission* mission);
    bool LoadMissionFromFile(const std::string& missionPath);
    void UnloadMission();
    [[nodiscard]] bool IsMissionLoaded() const { return m_currentMission != nullptr; }

    // Mission execution
    void StartMission();
    void PauseMission();
    void ResumeMission();
    void RestartMission();
    void EndMission(bool victory);
    void AbortMission();

    // Update
    void Update(float deltaTime);

    // State queries
    [[nodiscard]] MissionExecutionState GetExecutionState() const { return m_executionState; }
    [[nodiscard]] bool IsPlaying() const { return m_executionState == MissionExecutionState::Playing; }
    [[nodiscard]] bool IsPaused() const { return m_executionState == MissionExecutionState::Paused; }
    [[nodiscard]] bool IsComplete() const {
        return m_executionState == MissionExecutionState::Victory ||
               m_executionState == MissionExecutionState::Defeat;
    }

    // Current mission access
    [[nodiscard]] Mission* GetCurrentMission() { return m_currentMission; }
    [[nodiscard]] const Mission* GetCurrentMission() const { return m_currentMission; }
    [[nodiscard]] float GetMissionTime() const { return m_missionTime; }
    [[nodiscard]] float GetTimeLimit() const;
    [[nodiscard]] float GetTimeRemaining() const;
    [[nodiscard]] bool HasTimeLimit() const;

    // Objective management
    void ActivateObjective(const std::string& objectiveId);
    void CompleteObjective(const std::string& objectiveId);
    void FailObjective(const std::string& objectiveId);
    void UpdateObjectiveProgress(const std::string& objectiveId, int32_t progress);
    void SetObjectiveProgress(const std::string& objectiveId, int32_t progress);
    [[nodiscard]] Objective* GetObjective(const std::string& objectiveId);
    [[nodiscard]] std::vector<Objective*> GetActiveObjectives();
    [[nodiscard]] std::vector<Objective*> GetPrimaryObjectives();
    [[nodiscard]] std::vector<Objective*> GetSecondaryObjectives();
    [[nodiscard]] std::vector<Objective*> GetBonusObjectives();

    // Events
    void PostEvent(const MissionEvent& event);
    void RegisterEventHandler(MissionEventType type, std::function<void(const MissionEvent&)> handler);
    void UnregisterEventHandlers(MissionEventType type);

    // Triggers
    void AddTrigger(const MissionTrigger& trigger);
    void RemoveTrigger(const std::string& triggerId);
    void ActivateTrigger(const std::string& triggerId);
    void ResetTrigger(const std::string& triggerId);
    [[nodiscard]] bool IsTriggerActive(const std::string& triggerId) const;

    // Scripting
    void ExecuteScript(const std::string& script);
    void SetScriptVariable(const std::string& name, const std::string& value);
    [[nodiscard]] std::string GetScriptVariable(const std::string& name) const;

    // Statistics tracking
    void RecordUnitCreated(const std::string& unitType);
    void RecordUnitKilled(const std::string& unitType, bool isEnemy);
    void RecordBuildingBuilt(const std::string& buildingType);
    void RecordBuildingDestroyed(const std::string& buildingType, bool isEnemy);
    void RecordResourceGathered(const std::string& resourceType, int32_t amount);
    void RecordResourceSpent(const std::string& resourceType, int32_t amount);

    // AI difficulty
    void SetAIDifficulty(const std::string& aiId, MissionDifficulty difficulty);
    void ApplyDifficultyModifiers();

    // Map/level queries
    [[nodiscard]] std::string GetMapFile() const;
    [[nodiscard]] std::string GetPlayerStartPosition() const;

    // Callbacks
    void SetOnMissionStart(std::function<void()> callback) { m_onMissionStart = callback; }
    void SetOnMissionEnd(std::function<void(bool)> callback) { m_onMissionEnd = callback; }
    void SetOnObjectiveActivate(std::function<void(const Objective&)> callback) { m_onObjectiveActivate = callback; }
    void SetOnObjectiveComplete(std::function<void(const Objective&)> callback) { m_onObjectiveComplete = callback; }
    void SetOnObjectiveFail(std::function<void(const Objective&)> callback) { m_onObjectiveFail = callback; }
    void SetOnObjectiveProgress(std::function<void(const Objective&)> callback) { m_onObjectiveProgress = callback; }

private:
    bool m_initialized = false;
    MissionExecutionState m_executionState = MissionExecutionState::NotLoaded;

    // Current mission
    Mission* m_currentMission = nullptr;
    float m_missionTime = 0.0f;
    MissionDifficulty m_currentDifficulty = MissionDifficulty::Normal;

    // Triggers
    std::vector<MissionTrigger> m_triggers;

    // Event handlers
    std::map<MissionEventType, std::vector<std::function<void(const MissionEvent&)>>> m_eventHandlers;

    // Script variables
    std::map<std::string, std::string> m_scriptVariables;

    // Callbacks
    std::function<void()> m_onMissionStart;
    std::function<void(bool)> m_onMissionEnd;
    std::function<void(const Objective&)> m_onObjectiveActivate;
    std::function<void(const Objective&)> m_onObjectiveComplete;
    std::function<void(const Objective&)> m_onObjectiveFail;
    std::function<void(const Objective&)> m_onObjectiveProgress;

    // Internal methods
    void UpdateObjectives(float deltaTime);
    void UpdateTriggers();
    void CheckVictoryConditions();
    void CheckDefeatConditions();
    void ProcessEvent(const MissionEvent& event);
    void InitializeMissionState();
    void LoadMapFile(const std::string& mapFile);
    void SetupStartingUnits();
    void SetupStartingResources();
    void SetupAIPlayers();
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
