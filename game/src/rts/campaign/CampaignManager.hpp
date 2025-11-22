#pragma once

#include "Campaign.hpp"
#include "CinematicPlayer.hpp"
#include "DialogSystem.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace Vehement {
namespace RTS {
namespace Campaign {

/**
 * @brief Save slot information
 */
struct SaveSlot {
    int32_t slotIndex = 0;
    std::string savePath;
    std::string campaignId;
    std::string campaignTitle;
    int32_t chapterNumber = 0;
    int32_t missionNumber = 0;
    std::string missionTitle;
    float playTime = 0.0f;
    std::string timestamp;
    std::string screenshotPath;
    CampaignDifficulty difficulty = CampaignDifficulty::Normal;
    bool isEmpty = true;
};

/**
 * @brief Global campaign progress
 */
struct GlobalProgress {
    int32_t totalMissionsCompleted = 0;
    int32_t totalCampaignsCompleted = 0;
    float totalPlayTime = 0.0f;
    int32_t playerLevel = 1;
    int32_t playerExperience = 0;
    std::map<std::string, bool> globalFlags;
    std::vector<std::string> unlockedCampaigns;
    std::vector<std::string> unlockedRaces;
    std::vector<std::string> achievements;
};

/**
 * @brief Campaign manager - central controller for all campaign functionality
 */
class CampaignManager {
public:
    CampaignManager();
    ~CampaignManager();

    // Singleton access
    [[nodiscard]] static CampaignManager& Instance();

    // Delete copy/move
    CampaignManager(const CampaignManager&) = delete;
    CampaignManager& operator=(const CampaignManager&) = delete;
    CampaignManager(CampaignManager&&) = delete;
    CampaignManager& operator=(CampaignManager&&) = delete;

    // Initialization
    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // Campaign loading
    void LoadAllCampaigns(const std::string& campaignsDir);
    void LoadCampaign(const std::string& campaignPath);
    void UnloadCampaign(const std::string& campaignId);
    void ReloadCampaigns();

    // Campaign access
    [[nodiscard]] Campaign* GetCampaign(const std::string& campaignId);
    [[nodiscard]] const Campaign* GetCampaign(const std::string& campaignId) const;
    [[nodiscard]] Campaign* GetCampaignForRace(RaceType race);
    [[nodiscard]] std::vector<Campaign*> GetAllCampaigns();
    [[nodiscard]] std::vector<Campaign*> GetUnlockedCampaigns();
    [[nodiscard]] std::vector<Campaign*> GetAvailableCampaigns();

    // Current campaign
    [[nodiscard]] Campaign* GetCurrentCampaign() { return m_currentCampaign; }
    [[nodiscard]] Chapter* GetCurrentChapter();
    [[nodiscard]] Mission* GetCurrentMission();
    void SetCurrentCampaign(const std::string& campaignId);

    // Campaign control
    void StartCampaign(const std::string& campaignId, CampaignDifficulty difficulty);
    void ResumeCampaign(const std::string& campaignId);
    void CompleteCampaign();
    void AbandonCampaign();

    // Mission control
    void StartMission(const std::string& missionId);
    void CompleteMission();
    void FailMission();
    void RestartMission();
    void SkipMission();   // Debug/cheat
    void AdvanceToNextMission();

    // Chapter control
    void StartChapter(const std::string& chapterId);
    void CompleteChapter();
    void AdvanceToNextChapter();
    void UnlockChapter(const std::string& chapterId);

    // Flag management
    void SetCampaignFlag(const std::string& flag, bool value);
    [[nodiscard]] bool GetCampaignFlag(const std::string& flag) const;
    void SetGlobalFlag(const std::string& flag, bool value);
    [[nodiscard]] bool GetGlobalFlag(const std::string& flag) const;

    // Rewards
    void GrantMissionRewards(const MissionRewards& rewards);
    void GrantChapterRewards(const ChapterRewards& rewards);
    void GrantCampaignRewards(const CampaignRewards& rewards);
    void AddExperience(int32_t amount);
    void UnlockAchievement(const std::string& achievementId);

    // Progress tracking
    [[nodiscard]] const GlobalProgress& GetGlobalProgress() const { return m_globalProgress; }
    void UpdateGlobalProgress();
    [[nodiscard]] int32_t GetRequiredExperienceForLevel(int32_t level) const;
    [[nodiscard]] bool CheckLevelUp();

    // Save/Load
    [[nodiscard]] std::vector<SaveSlot> GetSaveSlots();
    bool SaveGame(int32_t slotIndex);
    bool LoadGame(int32_t slotIndex);
    bool QuickSave();
    bool QuickLoad();
    bool AutoSave();
    bool DeleteSave(int32_t slotIndex);
    [[nodiscard]] SaveSlot GetSaveSlotInfo(int32_t slotIndex) const;
    void SetSaveDirectory(const std::string& path) { m_saveDirectory = path; }

    // Unlocks
    void UpdateCampaignUnlocks();
    [[nodiscard]] bool IsCampaignUnlocked(const std::string& campaignId) const;
    [[nodiscard]] bool IsChapterUnlocked(const std::string& chapterId) const;
    [[nodiscard]] bool IsMissionUnlocked(const std::string& missionId) const;
    void UnlockCampaign(const std::string& campaignId);

    // Cinematics
    void PlayCinematic(const std::string& cinematicId);
    void PlayCampaignIntro();
    void PlayCampaignOutro();
    void PlayChapterIntro();
    void PlayChapterOutro();
    void PlayMissionIntro();
    void PlayMissionOutro();

    // Events
    void SetOnCampaignStart(std::function<void(Campaign&)> callback) { m_onCampaignStart = callback; }
    void SetOnCampaignComplete(std::function<void(Campaign&)> callback) { m_onCampaignComplete = callback; }
    void SetOnChapterStart(std::function<void(Chapter&)> callback) { m_onChapterStart = callback; }
    void SetOnChapterComplete(std::function<void(Chapter&)> callback) { m_onChapterComplete = callback; }
    void SetOnMissionStart(std::function<void(Mission&)> callback) { m_onMissionStart = callback; }
    void SetOnMissionComplete(std::function<void(Mission&)> callback) { m_onMissionComplete = callback; }
    void SetOnMissionFail(std::function<void(Mission&)> callback) { m_onMissionFail = callback; }
    void SetOnLevelUp(std::function<void(int32_t)> callback) { m_onLevelUp = callback; }

    // Update
    void Update(float deltaTime);

private:
    bool m_initialized = false;

    // Campaigns
    std::map<std::string, std::unique_ptr<Campaign>> m_campaigns;
    Campaign* m_currentCampaign = nullptr;

    // Global progress
    GlobalProgress m_globalProgress;

    // Save system
    std::string m_saveDirectory = "saves/campaigns/";
    int32_t m_maxSaveSlots = 20;
    int32_t m_quickSaveSlot = -1;
    int32_t m_autoSaveSlot = -2;

    // Callbacks
    std::function<void(Campaign&)> m_onCampaignStart;
    std::function<void(Campaign&)> m_onCampaignComplete;
    std::function<void(Chapter&)> m_onChapterStart;
    std::function<void(Chapter&)> m_onChapterComplete;
    std::function<void(Mission&)> m_onMissionStart;
    std::function<void(Mission&)> m_onMissionComplete;
    std::function<void(Mission&)> m_onMissionFail;
    std::function<void(int32_t)> m_onLevelUp;

    // Internal methods
    void LoadGlobalProgress();
    void SaveGlobalProgress();
    std::string GenerateSavePath(int32_t slotIndex) const;
    void TriggerCinematic(Cinematic* cinematic);
    void HandleCinematicComplete();
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
