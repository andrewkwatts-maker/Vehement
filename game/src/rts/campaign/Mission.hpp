#pragma once

#include "Objective.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace Vehement {
namespace RTS {
namespace Campaign {

/**
 * @brief Difficulty settings for missions
 */
enum class MissionDifficulty : uint8_t {
    Easy,
    Normal,
    Hard,
    Brutal,
    Custom
};

/**
 * @brief Current state of a mission
 */
enum class MissionState : uint8_t {
    Locked,         ///< Not yet available
    Available,      ///< Can be started
    InProgress,     ///< Currently playing
    Completed,      ///< Successfully completed
    Failed          ///< Failed (can retry)
};

/**
 * @brief Victory condition types
 */
enum class VictoryCondition : uint8_t {
    AllPrimaryObjectives,   ///< Complete all primary objectives
    AnyPrimaryObjective,    ///< Complete any primary objective
    SurvivalTime,           ///< Survive for duration
    EliminateAll,           ///< Kill all enemies
    Custom                  ///< Script-defined condition
};

/**
 * @brief Defeat condition types
 */
enum class DefeatCondition : uint8_t {
    HeroKilled,             ///< Hero unit dies
    BaseDestroyed,          ///< Main base destroyed
    AllUnitsLost,           ///< All units killed
    TimeExpired,            ///< Time limit exceeded
    ObjectiveFailed,        ///< Primary objective failed
    Custom                  ///< Script-defined condition
};

/**
 * @brief Starting resources for a mission
 */
struct MissionResources {
    int32_t gold = 500;
    int32_t wood = 200;
    int32_t stone = 100;
    int32_t metal = 50;
    int32_t food = 100;
    int32_t supply = 10;
    int32_t maxSupply = 100;
};

/**
 * @brief Unit availability restrictions
 */
struct UnitRestrictions {
    std::vector<std::string> availableUnits;        ///< Units player can train
    std::vector<std::string> disabledUnits;         ///< Units explicitly disabled
    std::vector<std::string> startingUnits;         ///< Units given at mission start
    std::map<std::string, int32_t> unitLimits;      ///< Max count per unit type
    bool allowAllUnits = true;                       ///< If false, only available list
};

/**
 * @brief Building availability restrictions
 */
struct BuildingRestrictions {
    std::vector<std::string> availableBuildings;    ///< Buildings player can build
    std::vector<std::string> disabledBuildings;     ///< Buildings explicitly disabled
    std::vector<std::string> startingBuildings;     ///< Buildings given at start
    std::map<std::string, int32_t> buildingLimits;  ///< Max count per building type
    bool allowAllBuildings = true;
};

/**
 * @brief Technology/research restrictions
 */
struct TechRestrictions {
    std::vector<std::string> availableTech;         ///< Researchable technologies
    std::vector<std::string> disabledTech;          ///< Disabled technologies
    std::vector<std::string> preresearchedTech;     ///< Already researched at start
    bool allowAllTech = true;
};

/**
 * @brief Mission briefing content
 */
struct MissionBriefing {
    std::string title;
    std::string subtitle;
    std::string storyText;                          ///< Full story/context
    std::string objectiveSummary;                   ///< Brief objective description
    std::vector<std::string> tips;                  ///< Gameplay tips
    std::string mapPreviewImage;                    ///< Preview image path
    std::string briefingVoiceover;                  ///< Audio file path
    std::string briefingMusic;                      ///< Background music
    std::vector<std::pair<std::string, std::string>> intelReports; ///< Intel name/text pairs
};

/**
 * @brief Mission debriefing content
 */
struct MissionDebriefing {
    std::string victoryTitle;
    std::string victoryText;
    std::string defeatTitle;
    std::string defeatText;
    std::string victoryVoiceover;
    std::string defeatVoiceover;
    std::string nextMissionTeaser;
};

/**
 * @brief AI player configuration for mission
 */
struct MissionAI {
    std::string aiId;                               ///< AI player identifier
    std::string faction;                            ///< AI faction/race
    MissionDifficulty difficulty = MissionDifficulty::Normal;
    std::string personality;                        ///< AI behavior type
    int32_t handicap = 0;                           ///< Resource/time bonus percent
    std::string startingPosition;                   ///< Map position ID
    MissionResources resources;
    bool isAlly = false;
    bool canBeDefeated = true;
    std::string defeatTrigger;                      ///< Script on AI defeat
};

/**
 * @brief Mission completion rewards
 */
struct MissionRewards {
    int32_t experienceBase = 100;
    int32_t experiencePerObjective = 25;
    int32_t goldBonus = 0;
    std::vector<std::string> unlockedMissions;
    std::vector<std::string> unlockedUnits;
    std::vector<std::string> unlockedBuildings;
    std::vector<std::string> unlockedHeroes;
    std::vector<std::string> items;
    std::map<std::string, bool> storyFlags;         ///< Flags to set on completion
    std::string achievement;                         ///< Achievement to unlock
};

/**
 * @brief Difficulty-specific modifiers
 */
struct DifficultyModifiers {
    float playerDamageMultiplier = 1.0f;
    float enemyDamageMultiplier = 1.0f;
    float playerResourceMultiplier = 1.0f;
    float enemyResourceMultiplier = 1.0f;
    float timeLimitMultiplier = 1.0f;
    float experienceMultiplier = 1.0f;
    int32_t extraEnemyUnits = 0;
    bool showHints = true;
    bool enableAutoSave = true;
};

/**
 * @brief Mission statistics tracking
 */
struct MissionStatistics {
    float completionTime = 0.0f;
    int32_t unitsCreated = 0;
    int32_t unitsLost = 0;
    int32_t enemiesKilled = 0;
    int32_t buildingsBuilt = 0;
    int32_t buildingsLost = 0;
    int32_t resourcesGathered = 0;
    int32_t resourcesSpent = 0;
    int32_t objectivesCompleted = 0;
    int32_t objectivesFailed = 0;
    int32_t bonusObjectivesCompleted = 0;
    int32_t secretsFound = 0;
    MissionDifficulty difficulty = MissionDifficulty::Normal;
    int32_t score = 0;
    std::string grade;                              ///< S, A, B, C, D, F
};

/**
 * @brief Individual mission definition
 */
class Mission {
public:
    Mission() = default;
    explicit Mission(const std::string& missionId);
    ~Mission() = default;

    // Identification
    std::string id;
    std::string title;
    std::string description;
    int32_t missionNumber = 1;

    // State
    MissionState state = MissionState::Locked;
    MissionDifficulty currentDifficulty = MissionDifficulty::Normal;

    // Map
    std::string mapFile;                            ///< Map file to load
    std::string mapName;                            ///< Display name
    std::string mapDescription;
    std::string playerStartPosition;                ///< Start location ID

    // Objectives
    std::vector<Objective> objectives;
    std::vector<std::string> primaryObjectiveIds;
    std::vector<std::string> secondaryObjectiveIds;
    std::vector<std::string> bonusObjectiveIds;

    // Win/Lose conditions
    VictoryCondition victoryCondition = VictoryCondition::AllPrimaryObjectives;
    DefeatCondition defeatCondition = DefeatCondition::HeroKilled;
    std::string customVictoryScript;
    std::string customDefeatScript;

    // Time
    float timeLimit = -1.0f;                        ///< -1 = no limit
    float parTime = 0.0f;                           ///< Target completion time
    bool showTimer = false;

    // Resources and restrictions
    MissionResources startingResources;
    UnitRestrictions unitRestrictions;
    BuildingRestrictions buildingRestrictions;
    TechRestrictions techRestrictions;

    // Content
    MissionBriefing briefing;
    MissionDebriefing debriefing;

    // AI configuration
    std::vector<MissionAI> aiPlayers;

    // Rewards
    MissionRewards rewards;

    // Difficulty modifiers
    std::map<MissionDifficulty, DifficultyModifiers> difficultySettings;

    // Cinematics
    std::string introCinematic;                     ///< Cinematic before mission
    std::string outroCinematic;                     ///< Cinematic after victory
    std::string defeatCinematic;                    ///< Cinematic after defeat
    std::vector<std::string> inMissionCinematics;   ///< Triggered during mission

    // Scripts
    std::string initScript;                         ///< Run at mission start
    std::string updateScript;                       ///< Run each frame
    std::string victoryScript;                      ///< Run on victory
    std::string defeatScript;                       ///< Run on defeat

    // Statistics
    MissionStatistics statistics;
    MissionStatistics bestStatistics;               ///< Best run stats

    // Unlock requirements
    std::vector<std::string> prerequisiteMissions;
    std::map<std::string, bool> requiredFlags;      ///< Story flags required
    int32_t requiredCompletions = 0;                ///< Other missions completed

    // Audio
    std::string ambientMusic;
    std::string combatMusic;
    std::string victoryMusic;
    std::string defeatMusic;

    // UI
    std::string thumbnailImage;
    std::string loadingScreenImage;
    std::string loadingScreenTip;
    bool showMinimap = true;
    bool allowSave = true;
    bool allowPause = true;

    // Methods
    void Initialize();
    void Start(MissionDifficulty difficulty);
    void Update(float deltaTime);
    void Complete();
    void Fail();
    void Reset();

    [[nodiscard]] Objective* GetObjective(const std::string& objectiveId);
    [[nodiscard]] const Objective* GetObjective(const std::string& objectiveId) const;
    void ActivateObjective(const std::string& objectiveId);
    void CompleteObjective(const std::string& objectiveId);
    void FailObjective(const std::string& objectiveId);
    void UpdateObjectiveProgress(const std::string& objectiveId, int32_t delta);

    [[nodiscard]] bool CheckVictoryCondition() const;
    [[nodiscard]] bool CheckDefeatCondition() const;
    [[nodiscard]] bool AreAllPrimaryObjectivesComplete() const;
    [[nodiscard]] bool AnyPrimaryObjectiveFailed() const;

    [[nodiscard]] int32_t GetCompletedObjectiveCount() const;
    [[nodiscard]] int32_t GetTotalObjectiveCount() const;
    [[nodiscard]] float GetCompletionPercentage() const;

    [[nodiscard]] DifficultyModifiers GetDifficultyModifiers() const;
    [[nodiscard]] MissionResources GetAdjustedResources() const;

    void CalculateScore();
    void CalculateGrade();

    // Serialization
    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(const std::string& json);
    [[nodiscard]] std::string SerializeProgress() const;
    bool DeserializeProgress(const std::string& json);

private:
    void CheckObjectiveDependencies();
    void UpdateStatistics(float deltaTime);
};

/**
 * @brief Factory for creating missions from config
 */
class MissionFactory {
public:
    [[nodiscard]] static std::unique_ptr<Mission> CreateFromJson(const std::string& jsonPath);
    [[nodiscard]] static std::unique_ptr<Mission> CreateFromConfig(const std::string& configPath);
    static void PopulateObjectives(Mission& mission, const std::string& objectivesJson);
    static void PopulateAI(Mission& mission, const std::string& aiJson);
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
