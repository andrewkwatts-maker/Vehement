#pragma once

#include <string>
#include <functional>

namespace Vehement {
namespace UI {
namespace Campaign {

/**
 * @brief Configuration for cinematic overlay
 */
struct CinematicUIConfig {
    float letterboxHeight = 0.12f;
    bool showSubtitles = true;
    float subtitleFontSize = 24.0f;
    std::string subtitleFont;
    std::string subtitleColor = "#FFFFFF";
    std::string subtitleBackgroundColor = "rgba(0,0,0,0.7)";
    bool showSkipPrompt = true;
    float skipPromptDelay = 2.0f;
    bool showChapterTitle = true;
    float chapterTitleDuration = 4.0f;
    bool showProgress = false;
    float fadeSpeed = 0.5f;
};

/**
 * @brief Cinematic overlay UI for letterbox, subtitles, and controls
 */
class CinematicUI {
public:
    CinematicUI();
    ~CinematicUI();

    [[nodiscard]] static CinematicUI& Instance();

    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // Configuration
    void SetConfig(const CinematicUIConfig& config);
    [[nodiscard]] const CinematicUIConfig& GetConfig() const { return m_config; }

    // Visibility
    void Show();
    void Hide();
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    // Letterbox
    void ShowLetterbox();
    void HideLetterbox();
    void SetLetterboxHeight(float height);
    [[nodiscard]] float GetLetterboxAmount() const { return m_letterboxAmount; }

    // Subtitles
    void ShowSubtitle(const std::string& text);
    void ShowSubtitle(const std::string& speaker, const std::string& text);
    void HideSubtitle();
    void SetSubtitleProgress(float progress);
    [[nodiscard]] std::string GetCurrentSubtitle() const { return m_currentSubtitle; }

    // Skip prompt
    void ShowSkipPrompt();
    void HideSkipPrompt();
    void SetSkipProgress(float progress);
    [[nodiscard]] bool IsSkipPromptVisible() const { return m_skipPromptVisible; }
    [[nodiscard]] float GetSkipProgress() const { return m_skipProgress; }

    // Chapter title card
    void ShowChapterTitle(const std::string& title, const std::string& subtitle = "");
    void HideChapterTitle();
    [[nodiscard]] bool IsChapterTitleVisible() const { return m_chapterTitleVisible; }

    // Loading
    void ShowLoading(const std::string& message = "");
    void HideLoading();
    void SetLoadingProgress(float progress);

    // Update and render
    void Update(float deltaTime);
    void Render();

    // Callbacks
    void SetOnSkip(std::function<void()> callback) { m_onSkip = callback; }
    void SetOnSkipHoldStart(std::function<void()> callback) { m_onSkipHoldStart = callback; }
    void SetOnSkipHoldEnd(std::function<void()> callback) { m_onSkipHoldEnd = callback; }

    // HTML bindings
    void BindToHTML();
    void HandleHTMLEvent(const std::string& eventName, const std::string& data);

private:
    bool m_initialized = false;
    bool m_visible = false;
    CinematicUIConfig m_config;

    // Letterbox
    float m_letterboxAmount = 0.0f;
    float m_targetLetterbox = 0.0f;

    // Subtitles
    std::string m_currentSubtitle;
    std::string m_currentSpeaker;
    float m_subtitleProgress = 1.0f;

    // Skip
    bool m_skipPromptVisible = false;
    float m_skipProgress = 0.0f;

    // Chapter title
    bool m_chapterTitleVisible = false;
    std::string m_chapterTitle;
    std::string m_chapterSubtitle;
    float m_chapterTitleTimer = 0.0f;

    // Loading
    bool m_loadingVisible = false;
    std::string m_loadingMessage;
    float m_loadingProgress = 0.0f;

    // Callbacks
    std::function<void()> m_onSkip;
    std::function<void()> m_onSkipHoldStart;
    std::function<void()> m_onSkipHoldEnd;

    void UpdateLetterbox(float deltaTime);
    void UpdateChapterTitle(float deltaTime);
    void SendDataToHTML();
};

} // namespace Campaign
} // namespace UI
} // namespace Vehement
