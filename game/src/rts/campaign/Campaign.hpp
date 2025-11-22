#pragma once

#include "Chapter.hpp"
#include "Cinematic.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace Vehement {
namespace RTS {
namespace Campaign {

// Forward declarations
class Cinematic;

/**
 * @brief Race/faction types for campaigns
 */
enum class RaceType : uint8_t {
    Human,
    Naga,
    Alien,
    Undead,
    Fairy,
    Vampire,
    Cryptid,
    Count
};

/**
 * @brief Convert race type to string
 */
inline const char* RaceTypeToString(RaceType type) {
    switch (type) {
        case RaceType::Human:   return "Human";
        case RaceType::Naga:    return "Naga";
        case RaceType::Alien:   return "Alien";
        case RaceType::Undead:  return "Undead";
        case RaceType::Fairy:   return "Fairy";
        case RaceType::Vampire: return "Vampire";
        case RaceType::Cryptid: return "Cryptid";
        default:                return "Unknown";
    }
}

/**
 * @brief Campaign difficulty modes
 */
enum class CampaignDifficulty : uint8_t {
    Story,      ///< Easy mode, focus on narrative
    Normal,     ///< Standard difficulty
    Veteran,    ///< Challenging gameplay
    Legendary   ///< Maximum difficulty
};

/**
 * @brief Campaign completion state
 */
enum class CampaignState : uint8_t {
    NotStarted,
    InProgress,
    Completed
};

/**
 * @brief Campaign statistics
 */
struct CampaignStatistics {
    float totalPlayTime = 0.0f;
    int32_t totalMissionsCompleted = 0;
    int32_t totalMissionsFailed = 0;
    int32_t totalObjectivesCompleted = 0;
    int32_t totalScore = 0;
    int32_t unitsCreated = 0;
    int32_t unitsLost = 0;
    int32_t enemiesDefeated = 0;
    int32_t buildingsBuilt = 0;
    int32_t resourcesGathered = 0;
    std::string fastestMission;
    float fastestMissionTime = 0.0f;
    std::string highestScoreMission;
    int32_t highestScore = 0;
};

/**
 * @brief Campaign metadata
 */
struct CampaignInfo {
    std::string author;
    std::string version;
    std::string createdDate;
    std::string lastModifiedDate;
    std::vector<std::string> tags;
    int32_t estimatedPlaytime = 0;              ///< Hours
    std::string minimumGameVersion;
    std::vector<std::string> requiredDLC;
};

/**
 * @brief Campaign unlock rewards
 */
struct CampaignRewards {
    int32_t experienceTotal = 0;
    std::vector<std::string> unlockedCampaigns;
    std::vector<std::string> unlockedRaces;
    std::vector<std::string> unlockedHeroes;
    std::vector<std::string> unlockedMaps;
    std::vector<std::string> achievements;
    std::string epilogueCinematic;
    std::string specialEnding;
};

/**
 * @brief Full campaign definition
 */
class Campaign {
public:
    Campaign() = default;
    explicit Campaign(const std::string& campaignId);
    ~Campaign() = default;

    // Identification
    std::string id;
    std::string raceId;
    RaceType race = RaceType::Human;
    std::string title;
    std::string subtitle;
    std::string description;
    std::string shortDescription;

    // State
    CampaignState state = CampaignState::NotStarted;
    CampaignDifficulty difficulty = CampaignDifficulty::Normal;

    // Content
    std::vector<std::unique_ptr<Chapter>> chapters;
    std::vector<std::unique_ptr<Cinematic>> cinematics;
    std::map<std::string, bool> flags;              ///< Story flags

    // Progress tracking
    int32_t currentChapter = 0;
    int32_t currentMission = 0;
    CampaignStatistics statistics;

    // Metadata
    CampaignInfo info;

    // Rewards
    CampaignRewards rewards;

    // Story
    std::string prologueText;
    std::string epilogueText;
    std::vector<std::string> keyCharacters;
    std::string setting;
    std::string timeframe;

    // Cinematics
    std::string introCinematic;                     ///< Campaign intro
    std::string outroCinematic;                     ///< Campaign outro
    std::string creditsSequence;

    // UI
    std::string thumbnailImage;
    std::string bannerImage;
    std::string backgroundImage;
    std::string logoImage;
    std::string iconImage;
    std::string themeColor;

    // Audio
    std::string menuMusic;
    std::string mainTheme;
    std::string victoryMusic;
    std::string defeatMusic;

    // Unlock requirements
    std::vector<std::string> prerequisiteCampaigns;
    std::map<std::string, bool> requiredGlobalFlags;
    int32_t requiredPlayerLevel = 0;
    bool isUnlocked = false;

    // Settings
    bool allowChapterSelect = true;                 ///< Can select chapters freely
    bool allowMissionReplay = true;                 ///< Can replay completed missions
    bool carryOverResources = false;                ///< Resources persist between missions
    bool carryOverUnits = false;                    ///< Hero/special units persist
    bool persistentUpgrades = true;                 ///< Tech upgrades carry over
    int32_t maxSaveSlots = 10;

    // Methods
    void Initialize();
    void Start(CampaignDifficulty selectedDifficulty);
    void Update(float deltaTime);
    void Complete();
    void Reset();

    // Chapter management
    [[nodiscard]] Chapter* GetChapter(const std::string& chapterId);
    [[nodiscard]] const Chapter* GetChapter(const std::string& chapterId) const;
    [[nodiscard]] Chapter* GetChapterByIndex(size_t index);
    [[nodiscard]] const Chapter* GetChapterByIndex(size_t index) const;
    [[nodiscard]] Chapter* GetCurrentChapter();
    [[nodiscard]] Mission* GetCurrentMission();

    void AddChapter(std::unique_ptr<Chapter> chapter);
    void AdvanceToNextMission();
    void AdvanceToNextChapter();
    void SetCurrentMission(const std::string& chapterId, const std::string& missionId);

    // Cinematic management
    [[nodiscard]] Cinematic* GetCinematic(const std::string& cinematicId);
    void AddCinematic(std::unique_ptr<Cinematic> cinematic);

    // Flag management
    void SetFlag(const std::string& flagName, bool value);
    [[nodiscard]] bool GetFlag(const std::string& flagName) const;
    [[nodiscard]] bool HasFlag(const std::string& flagName) const;
    void ClearFlag(const std::string& flagName);

    // Progress queries
    [[nodiscard]] bool IsComplete() const;
    [[nodiscard]] bool IsStarted() const { return state != CampaignState::NotStarted; }
    [[nodiscard]] bool CanUnlock(const std::vector<Campaign>& allCampaigns,
                                  const std::map<std::string, bool>& globalFlags,
                                  int32_t playerLevel) const;

    [[nodiscard]] int32_t GetTotalChapters() const;
    [[nodiscard]] int32_t GetCompletedChapters() const;
    [[nodiscard]] int32_t GetTotalMissions() const;
    [[nodiscard]] int32_t GetCompletedMissions() const;
    [[nodiscard]] float GetCompletionPercentage() const;

    // Statistics
    void UpdateStatistics();
    void AddMissionStatistics(const MissionStatistics& missionStats);

    // Serialization
    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(const std::string& json);
    [[nodiscard]] std::string SerializeProgress() const;
    bool DeserializeProgress(const std::string& json);

    // Save/Load
    bool SaveProgress(const std::string& savePath) const;
    bool LoadProgress(const std::string& savePath);

private:
    void UpdateChapterUnlocks();
    void CheckCampaignComplete();
};

/**
 * @brief Factory for creating campaigns from config
 */
class CampaignFactory {
public:
    [[nodiscard]] static std::unique_ptr<Campaign> CreateFromJson(const std::string& jsonPath);
    [[nodiscard]] static std::unique_ptr<Campaign> CreateFromConfig(const std::string& configDir);
    [[nodiscard]] static std::unique_ptr<Campaign> CreateForRace(RaceType race);
    static void LoadChapters(Campaign& campaign, const std::string& chaptersDir);
    static void LoadCinematics(Campaign& campaign, const std::string& cinematicsDir);
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
