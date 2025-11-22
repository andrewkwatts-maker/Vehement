#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Engine { namespace UI { class RuntimeUIManager; class UIWindow; } }

namespace Game {
namespace UI {
namespace Menu {

/**
 * @brief Difficulty levels for campaign
 */
enum class CampaignDifficulty {
    Easy,
    Normal,
    Hard,
    Brutal
};

/**
 * @brief Campaign chapter status
 */
enum class ChapterStatus {
    Locked,
    Available,
    Completed
};

/**
 * @brief Campaign chapter data
 */
struct CampaignChapter {
    int id;
    std::string name;
    std::string description;
    std::string mapPath;
    std::string previewImage;
    std::string cinematicPath;
    ChapterStatus status = ChapterStatus::Locked;
    bool hasCinematic = false;
    int bestTime = 0;          // Best completion time in seconds
    int bestScore = 0;
    CampaignDifficulty highestCompleted = CampaignDifficulty::Easy;

    struct Objective {
        std::string description;
        bool isPrimary;
        bool isBonus;
    };
    std::vector<Objective> objectives;
};

/**
 * @brief Race/faction trait
 */
struct RaceTrait {
    std::string name;
    std::string description;
    std::string iconPath;
};

/**
 * @brief Race campaign data
 */
struct RaceCampaign {
    std::string id;
    std::string name;
    std::string description;
    std::string lore;
    std::string iconPath;
    std::string backgroundImage;
    std::vector<RaceTrait> traits;
    std::vector<CampaignChapter> chapters;
    bool locked = false;
    std::string lockReason;
    int progressPercent = 0;
    int completedChapters = 0;
};

/**
 * @brief Campaign progress save data
 */
struct CampaignProgress {
    std::string raceId;
    int currentChapter = 0;
    std::unordered_map<int, ChapterStatus> chapterStatus;
    std::unordered_map<int, int> chapterScores;
    std::unordered_map<int, int> chapterTimes;
    int totalPlayTime = 0;
    std::string lastPlayed;
};

/**
 * @brief Campaign Manager
 *
 * Manages campaign selection, progress tracking, chapter unlocks,
 * and save/load functionality.
 */
class CampaignManager {
public:
    CampaignManager();
    ~CampaignManager();

    /**
     * @brief Initialize the campaign manager
     */
    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Update (call every frame when campaign menu is active)
     */
    void Update(float deltaTime);

    // Race Management

    /**
     * @brief Add a race campaign
     */
    void AddRaceCampaign(const RaceCampaign& race);

    /**
     * @brief Get all race campaigns
     */
    const std::vector<RaceCampaign>& GetRaces() const { return m_races; }

    /**
     * @brief Get race by ID
     */
    RaceCampaign* GetRace(const std::string& raceId);

    /**
     * @brief Select a race
     */
    void SelectRace(const std::string& raceId);

    /**
     * @brief Get currently selected race
     */
    const RaceCampaign* GetSelectedRace() const;

    /**
     * @brief Unlock a race
     */
    void UnlockRace(const std::string& raceId);

    // Chapter Management

    /**
     * @brief Select a chapter
     */
    void SelectChapter(int chapterId);

    /**
     * @brief Get selected chapter
     */
    const CampaignChapter* GetSelectedChapter() const;

    /**
     * @brief Unlock a chapter
     */
    void UnlockChapter(const std::string& raceId, int chapterId);

    /**
     * @brief Mark chapter as completed
     */
    void CompleteChapter(const std::string& raceId, int chapterId, int score, int time);

    /**
     * @brief Get next available chapter
     */
    const CampaignChapter* GetNextAvailableChapter(const std::string& raceId) const;

    // Difficulty

    /**
     * @brief Set selected difficulty
     */
    void SetDifficulty(CampaignDifficulty difficulty);

    /**
     * @brief Get selected difficulty
     */
    CampaignDifficulty GetDifficulty() const { return m_selectedDifficulty; }

    // Progress Management

    /**
     * @brief Save campaign progress
     */
    bool SaveProgress(const std::string& savePath = "");

    /**
     * @brief Load campaign progress
     */
    bool LoadProgress(const std::string& savePath = "");

    /**
     * @brief Get progress for a race
     */
    CampaignProgress* GetProgress(const std::string& raceId);

    /**
     * @brief Reset progress for a race
     */
    void ResetProgress(const std::string& raceId);

    /**
     * @brief Reset all campaign progress
     */
    void ResetAllProgress();

    // Game Actions

    /**
     * @brief Start selected chapter
     */
    void StartChapter();

    /**
     * @brief Continue campaign (start next available chapter)
     */
    void ContinueCampaign();

    /**
     * @brief Play cinematic for chapter
     */
    void PlayCinematic(int chapterId);

    // Callbacks

    void SetOnRaceSelect(std::function<void(const std::string&)> callback);
    void SetOnChapterSelect(std::function<void(int)> callback);
    void SetOnStartMission(std::function<void(const std::string&, int, CampaignDifficulty)> callback);
    void SetOnPlayCinematic(std::function<void(const std::string&, int)> callback);

private:
    void SetupEventHandlers();
    void UpdateRaceProgress(const std::string& raceId);
    void RefreshUI();
    std::string DifficultyToString(CampaignDifficulty diff) const;

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;

    std::vector<RaceCampaign> m_races;
    std::unordered_map<std::string, CampaignProgress> m_progress;

    std::string m_selectedRaceId;
    int m_selectedChapterId = -1;
    CampaignDifficulty m_selectedDifficulty = CampaignDifficulty::Normal;

    std::function<void(const std::string&)> m_onRaceSelect;
    std::function<void(int)> m_onChapterSelect;
    std::function<void(const std::string&, int, CampaignDifficulty)> m_onStartMission;
    std::function<void(const std::string&, int)> m_onPlayCinematic;
};

} // namespace Menu
} // namespace UI
} // namespace Game
