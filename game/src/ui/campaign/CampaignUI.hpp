#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Vehement {
namespace UI {
namespace Campaign {

/**
 * @brief Campaign selection view mode
 */
enum class CampaignViewMode : uint8_t {
    RaceSelect,     ///< Showing race/campaign carousel
    ChapterSelect,  ///< Showing chapters for selected campaign
    MissionSelect,  ///< Showing missions for selected chapter
    Briefing        ///< Showing mission briefing
};

/**
 * @brief Display data for campaign card
 */
struct CampaignCardData {
    std::string id;
    std::string title;
    std::string description;
    std::string raceId;
    std::string thumbnailImage;
    std::string bannerImage;
    float completionPercent = 0.0f;
    int32_t chaptersCompleted = 0;
    int32_t chaptersTotal = 0;
    bool isLocked = false;
    bool isCompleted = false;
    std::string unlockRequirement;
};

/**
 * @brief Display data for chapter card
 */
struct ChapterCardData {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string description;
    std::string thumbnailImage;
    int32_t chapterNumber = 1;
    float completionPercent = 0.0f;
    int32_t missionsCompleted = 0;
    int32_t missionsTotal = 0;
    bool isLocked = false;
    bool isCompleted = false;
    bool isCurrent = false;
    std::string unlockRequirement;
};

/**
 * @brief Display data for mission card
 */
struct MissionCardData {
    std::string id;
    std::string title;
    std::string description;
    std::string mapPreview;
    int32_t missionNumber = 1;
    bool isLocked = false;
    bool isCompleted = false;
    bool isCurrent = false;
    std::string bestGrade;
    int32_t bestScore = 0;
    float bestTime = 0.0f;
    std::string difficulty;
};

/**
 * @brief Configuration for campaign UI
 */
struct CampaignUIConfig {
    bool showLockedContent = true;
    bool showCompletionStats = true;
    bool enableChapterSelect = true;
    bool enableMissionReplay = true;
    bool showDifficultySelect = true;
    float animationSpeed = 0.3f;
    std::string themeStyle;
};

/**
 * @brief Campaign selection and navigation UI
 */
class CampaignUI {
public:
    CampaignUI();
    ~CampaignUI();

    // Singleton access
    [[nodiscard]] static CampaignUI& Instance();

    // Initialization
    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // Configuration
    void SetConfig(const CampaignUIConfig& config);
    [[nodiscard]] const CampaignUIConfig& GetConfig() const { return m_config; }

    // Visibility
    void Show();
    void Hide();
    [[nodiscard]] bool IsVisible() const { return m_visible; }
    void Toggle();

    // Navigation
    void SetViewMode(CampaignViewMode mode);
    [[nodiscard]] CampaignViewMode GetViewMode() const { return m_viewMode; }
    void NavigateBack();
    void NavigateToRaceSelect();
    void NavigateToChapters(const std::string& campaignId);
    void NavigateToMissions(const std::string& chapterId);
    void NavigateToBriefing(const std::string& missionId);

    // Selection
    void SelectCampaign(const std::string& campaignId);
    void SelectChapter(const std::string& chapterId);
    void SelectMission(const std::string& missionId);
    void SelectDifficulty(int32_t difficulty);
    [[nodiscard]] std::string GetSelectedCampaignId() const { return m_selectedCampaignId; }
    [[nodiscard]] std::string GetSelectedChapterId() const { return m_selectedChapterId; }
    [[nodiscard]] std::string GetSelectedMissionId() const { return m_selectedMissionId; }
    [[nodiscard]] int32_t GetSelectedDifficulty() const { return m_selectedDifficulty; }

    // Data population
    void SetCampaigns(const std::vector<CampaignCardData>& campaigns);
    void SetChapters(const std::vector<ChapterCardData>& chapters);
    void SetMissions(const std::vector<MissionCardData>& missions);
    void RefreshData();

    // Carousel control
    void NextCampaign();
    void PreviousCampaign();
    void SetCarouselIndex(int32_t index);
    [[nodiscard]] int32_t GetCarouselIndex() const { return m_carouselIndex; }

    // Actions
    void StartSelectedMission();
    void ContinueCampaign();
    void StartNewCampaign();

    // Update
    void Update(float deltaTime);
    void Render();

    // Callbacks
    void SetOnCampaignSelect(std::function<void(const std::string&)> callback) { m_onCampaignSelect = callback; }
    void SetOnChapterSelect(std::function<void(const std::string&)> callback) { m_onChapterSelect = callback; }
    void SetOnMissionSelect(std::function<void(const std::string&)> callback) { m_onMissionSelect = callback; }
    void SetOnStartMission(std::function<void(const std::string&, int32_t)> callback) { m_onStartMission = callback; }
    void SetOnBack(std::function<void()> callback) { m_onBack = callback; }

    // HTML bindings
    void BindToHTML();
    void HandleHTMLEvent(const std::string& eventName, const std::string& data);

private:
    bool m_initialized = false;
    bool m_visible = false;
    CampaignUIConfig m_config;
    CampaignViewMode m_viewMode = CampaignViewMode::RaceSelect;

    // Data
    std::vector<CampaignCardData> m_campaigns;
    std::vector<ChapterCardData> m_chapters;
    std::vector<MissionCardData> m_missions;

    // Selection state
    std::string m_selectedCampaignId;
    std::string m_selectedChapterId;
    std::string m_selectedMissionId;
    int32_t m_selectedDifficulty = 1;
    int32_t m_carouselIndex = 0;

    // Animation
    float m_transitionProgress = 0.0f;
    bool m_isTransitioning = false;
    float m_carouselPosition = 0.0f;  // Current visual position (lerps toward m_carouselIndex)
    float m_carouselVelocity = 0.0f;  // For smooth deceleration

    // Callbacks
    std::function<void(const std::string&)> m_onCampaignSelect;
    std::function<void(const std::string&)> m_onChapterSelect;
    std::function<void(const std::string&)> m_onMissionSelect;
    std::function<void(const std::string&, int32_t)> m_onStartMission;
    std::function<void()> m_onBack;

    // Internal methods
    void UpdateCarouselAnimation(float deltaTime);
    void SendDataToHTML();
    void UpdateHTMLView();
};

} // namespace Campaign
} // namespace UI
} // namespace Vehement
