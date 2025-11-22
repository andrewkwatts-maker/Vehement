#pragma once

#include <string>
#include <vector>
#include <functional>

namespace Vehement {
namespace UI {
namespace Campaign {

/**
 * @brief Objective preview for briefing
 */
struct BriefingObjective {
    std::string title;
    std::string description;
    bool isPrimary = true;
    std::string icon;
};

/**
 * @brief Tip for mission briefing
 */
struct BriefingTip {
    std::string text;
    std::string icon;
    std::string category;
};

/**
 * @brief Intel report for briefing
 */
struct IntelReport {
    std::string title;
    std::string text;
    std::string image;
    bool isNew = true;
};

/**
 * @brief Full briefing data
 */
struct BriefingData {
    std::string missionId;
    std::string missionTitle;
    std::string missionSubtitle;
    std::string storyText;
    std::string mapPreviewImage;
    std::string mapName;
    std::vector<BriefingObjective> objectives;
    std::vector<BriefingTip> tips;
    std::vector<IntelReport> intelReports;
    std::string voiceoverFile;
    std::string backgroundMusic;
    int32_t estimatedTime = 0;          ///< Minutes
    int32_t parTime = 0;                ///< Minutes for bonus
    std::string difficultyDescription;
};

/**
 * @brief Configuration for briefing UI
 */
struct BriefingUIConfig {
    bool enableVoiceover = true;
    bool autoPlayVoiceover = true;
    bool showObjectives = true;
    bool showTips = true;
    bool showIntel = true;
    bool showDifficultySelect = true;
    bool showEstimatedTime = true;
    float textScrollSpeed = 100.0f;     ///< Characters per second
    std::string defaultDifficulty;
};

/**
 * @brief Mission briefing screen UI
 */
class BriefingUI {
public:
    BriefingUI();
    ~BriefingUI();

    [[nodiscard]] static BriefingUI& Instance();

    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // Configuration
    void SetConfig(const BriefingUIConfig& config);
    [[nodiscard]] const BriefingUIConfig& GetConfig() const { return m_config; }

    // Visibility
    void Show();
    void Hide();
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    // Briefing content
    void SetBriefingData(const BriefingData& data);
    [[nodiscard]] const BriefingData& GetBriefingData() const { return m_briefingData; }
    void ClearBriefing();

    // Panels
    void ShowObjectivesPanel();
    void ShowTipsPanel();
    void ShowIntelPanel();
    void SetActivePanel(const std::string& panelId);

    // Difficulty
    void SetDifficulty(int32_t difficulty);
    [[nodiscard]] int32_t GetSelectedDifficulty() const { return m_selectedDifficulty; }

    // Voiceover
    void PlayVoiceover();
    void StopVoiceover();
    void PauseVoiceover();
    void ResumeVoiceover();
    [[nodiscard]] bool IsVoiceoverPlaying() const { return m_voiceoverPlaying; }

    // Text scroll
    void ScrollTextToTop();
    void ScrollTextToBottom();
    void SetTextScrollPosition(float position);

    // Actions
    void StartMission();
    void GoBack();

    // Update
    void Update(float deltaTime);
    void Render();

    // Callbacks
    void SetOnStartMission(std::function<void(int32_t)> callback) { m_onStartMission = callback; }
    void SetOnBack(std::function<void()> callback) { m_onBack = callback; }
    void SetOnDifficultyChange(std::function<void(int32_t)> callback) { m_onDifficultyChange = callback; }

    // HTML bindings
    void BindToHTML();
    void HandleHTMLEvent(const std::string& eventName, const std::string& data);

private:
    bool m_initialized = false;
    bool m_visible = false;
    BriefingUIConfig m_config;

    BriefingData m_briefingData;
    int32_t m_selectedDifficulty = 1;
    std::string m_activePanel = "objectives";

    bool m_voiceoverPlaying = false;
    float m_textScrollPosition = 0.0f;

    std::function<void(int32_t)> m_onStartMission;
    std::function<void()> m_onBack;
    std::function<void(int32_t)> m_onDifficultyChange;

    void SendDataToHTML();
    void UpdateVoiceover(float deltaTime);
};

} // namespace Campaign
} // namespace UI
} // namespace Vehement
