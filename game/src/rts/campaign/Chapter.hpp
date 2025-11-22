#pragma once

#include "Mission.hpp"
#include <string>
#include <vector>
#include <memory>

namespace Vehement {
namespace RTS {
namespace Campaign {

/**
 * @brief Chapter completion state
 */
enum class ChapterState : uint8_t {
    Locked,         ///< Not yet available
    Available,      ///< Can be started
    InProgress,     ///< Currently playing missions
    Completed       ///< All required missions complete
};

/**
 * @brief Unlock requirements for a chapter
 */
struct ChapterUnlockRequirements {
    std::vector<std::string> previousChapters;      ///< Chapters that must be completed
    std::map<std::string, bool> requiredFlags;      ///< Story flags required
    int32_t minimumMissionsCompleted = 0;           ///< Total missions completed requirement
    int32_t minimumExperience = 0;                  ///< Experience level requirement
    std::string unlockCinematic;                    ///< Cinematic to play when unlocked
};

/**
 * @brief Rewards granted on chapter completion
 */
struct ChapterRewards {
    int32_t experienceBonus = 500;
    int32_t goldBonus = 1000;
    std::vector<std::string> unlockedHeroes;        ///< Heroes unlocked
    std::vector<std::string> unlockedUnits;         ///< Unit types unlocked
    std::vector<std::string> unlockedBuildings;     ///< Building types unlocked
    std::vector<std::string> unlockedAbilities;     ///< Abilities unlocked
    std::vector<std::string> unlockedTech;          ///< Technologies unlocked
    std::vector<std::string> items;                 ///< Items rewarded
    std::string achievement;                         ///< Achievement to unlock
    std::map<std::string, bool> storyFlags;         ///< Story flags to set
    std::string nextChapterUnlock;                  ///< Chapter to unlock
};

/**
 * @brief Chapter story context
 */
struct ChapterStory {
    std::string synopsis;                           ///< Brief story summary
    std::string fullDescription;                    ///< Detailed story context
    std::string openingNarration;                   ///< Text narration at start
    std::string closingNarration;                   ///< Text narration at end
    std::vector<std::string> keyCharacters;         ///< Important characters
    std::vector<std::string> previousEvents;        ///< What happened before
    std::string timeframe;                          ///< When chapter takes place
    std::string location;                           ///< Where chapter takes place
};

/**
 * @brief Chapter progress tracking
 */
struct ChapterProgress {
    int32_t missionsCompleted = 0;
    int32_t missionsTotal = 0;
    int32_t secretsFound = 0;
    int32_t secretsTotal = 0;
    float completionPercentage = 0.0f;
    float timeSpent = 0.0f;
    int32_t totalScore = 0;
    std::string bestMissionGrade;
};

/**
 * @brief Campaign chapter containing multiple missions
 */
class Chapter {
public:
    Chapter() = default;
    explicit Chapter(const std::string& chapterId);
    ~Chapter() = default;

    // Identification
    std::string id;
    std::string title;
    std::string subtitle;
    std::string description;
    int32_t chapterNumber = 1;

    // State
    ChapterState state = ChapterState::Locked;

    // Content
    std::vector<std::unique_ptr<Mission>> missions;
    std::vector<std::string> missionOrder;          ///< Mission IDs in order

    // Story
    ChapterStory story;

    // Cinematics
    std::string introCinematic;                     ///< Play when chapter starts
    std::string outroCinematic;                     ///< Play when chapter completes
    std::vector<std::string> interMissionCinematics; ///< Between missions

    // Unlock requirements
    ChapterUnlockRequirements requirements;

    // Rewards
    ChapterRewards rewards;

    // Progress
    ChapterProgress progress;

    // UI
    std::string thumbnailImage;
    std::string headerImage;
    std::string backgroundImage;
    std::string iconUnlocked;
    std::string iconLocked;
    std::string themeColor;                         ///< UI accent color

    // Audio
    std::string menuMusic;                          ///< Music on chapter select
    std::string briefingMusic;                      ///< Default briefing music

    // Mission requirements
    int32_t requiredMissionsToComplete = -1;        ///< -1 = all missions
    bool allowMissionSkip = false;                  ///< Can skip to next mission
    bool lockCompletedMissions = false;             ///< Prevent replay
    bool requireSequentialCompletion = true;        ///< Must complete in order

    // Methods
    void Initialize();
    void Unlock();
    void Start();
    void Complete();
    void Reset();

    [[nodiscard]] Mission* GetMission(const std::string& missionId);
    [[nodiscard]] const Mission* GetMission(const std::string& missionId) const;
    [[nodiscard]] Mission* GetMissionByIndex(size_t index);
    [[nodiscard]] const Mission* GetMissionByIndex(size_t index) const;
    [[nodiscard]] Mission* GetCurrentMission();
    [[nodiscard]] Mission* GetNextAvailableMission();

    void AddMission(std::unique_ptr<Mission> mission);
    void RemoveMission(const std::string& missionId);

    [[nodiscard]] bool CanUnlock(const std::vector<Chapter>& allChapters,
                                  const std::map<std::string, bool>& flags) const;
    [[nodiscard]] bool IsComplete() const;
    [[nodiscard]] bool IsLocked() const { return state == ChapterState::Locked; }
    [[nodiscard]] bool IsAvailable() const { return state == ChapterState::Available; }
    [[nodiscard]] bool IsInProgress() const { return state == ChapterState::InProgress; }

    [[nodiscard]] int32_t GetCompletedMissionCount() const;
    [[nodiscard]] int32_t GetTotalMissionCount() const;
    [[nodiscard]] float GetCompletionPercentage() const;

    void UpdateProgress();
    void CalculateTotalScore();

    // Serialization
    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(const std::string& json);
    [[nodiscard]] std::string SerializeProgress() const;
    bool DeserializeProgress(const std::string& json);

private:
    size_t currentMissionIndex = 0;

    void UpdateMissionAvailability();
};

/**
 * @brief Factory for creating chapters from config
 */
class ChapterFactory {
public:
    [[nodiscard]] static std::unique_ptr<Chapter> CreateFromJson(const std::string& jsonPath);
    [[nodiscard]] static std::unique_ptr<Chapter> CreateFromConfig(const std::string& configDir);
    static void LoadMissions(Chapter& chapter, const std::string& missionsDir);
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
