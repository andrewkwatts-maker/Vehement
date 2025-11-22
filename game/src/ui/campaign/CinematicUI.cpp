#include "CinematicUI.hpp"
#include <cmath>

namespace Vehement {
namespace UI {
namespace Campaign {

CinematicUI::CinematicUI() = default;
CinematicUI::~CinematicUI() = default;

CinematicUI& CinematicUI::Instance() {
    static CinematicUI instance;
    return instance;
}

bool CinematicUI::Initialize() {
    if (m_initialized) return true;
    m_config = CinematicUIConfig();
    m_visible = false;
    m_letterboxAmount = 0.0f;
    m_initialized = true;
    return true;
}

void CinematicUI::Shutdown() {
    m_initialized = false;
}

void CinematicUI::SetConfig(const CinematicUIConfig& config) {
    m_config = config;
}

void CinematicUI::Show() {
    m_visible = true;
    SendDataToHTML();
}

void CinematicUI::Hide() {
    m_visible = false;
    HideLetterbox();
    HideSubtitle();
    HideSkipPrompt();
    HideChapterTitle();
    HideLoading();
}

void CinematicUI::ShowLetterbox() {
    m_targetLetterbox = m_config.letterboxHeight;
}

void CinematicUI::HideLetterbox() {
    m_targetLetterbox = 0.0f;
}

void CinematicUI::SetLetterboxHeight(float height) {
    m_targetLetterbox = height;
}

void CinematicUI::ShowSubtitle(const std::string& text) {
    m_currentSubtitle = text;
    m_currentSpeaker.clear();
    m_subtitleProgress = 0.0f;
    SendDataToHTML();
}

void CinematicUI::ShowSubtitle(const std::string& speaker, const std::string& text) {
    m_currentSpeaker = speaker;
    m_currentSubtitle = text;
    m_subtitleProgress = 0.0f;
    SendDataToHTML();
}

void CinematicUI::HideSubtitle() {
    m_currentSubtitle.clear();
    m_currentSpeaker.clear();
    m_subtitleProgress = 1.0f;
    SendDataToHTML();
}

void CinematicUI::SetSubtitleProgress(float progress) {
    m_subtitleProgress = progress;
    SendDataToHTML();
}

void CinematicUI::ShowSkipPrompt() {
    m_skipPromptVisible = true;
    m_skipProgress = 0.0f;
    SendDataToHTML();
}

void CinematicUI::HideSkipPrompt() {
    m_skipPromptVisible = false;
    m_skipProgress = 0.0f;
    SendDataToHTML();
}

void CinematicUI::SetSkipProgress(float progress) {
    m_skipProgress = progress;
    SendDataToHTML();
}

void CinematicUI::ShowChapterTitle(const std::string& title, const std::string& subtitle) {
    m_chapterTitle = title;
    m_chapterSubtitle = subtitle;
    m_chapterTitleVisible = true;
    m_chapterTitleTimer = m_config.chapterTitleDuration;
    SendDataToHTML();
}

void CinematicUI::HideChapterTitle() {
    m_chapterTitleVisible = false;
    SendDataToHTML();
}

void CinematicUI::ShowLoading(const std::string& message) {
    m_loadingVisible = true;
    m_loadingMessage = message;
    m_loadingProgress = 0.0f;
    SendDataToHTML();
}

void CinematicUI::HideLoading() {
    m_loadingVisible = false;
    SendDataToHTML();
}

void CinematicUI::SetLoadingProgress(float progress) {
    m_loadingProgress = progress;
    SendDataToHTML();
}

void CinematicUI::Update(float deltaTime) {
    if (!m_visible) return;

    UpdateLetterbox(deltaTime);
    UpdateChapterTitle(deltaTime);
}

void CinematicUI::Render() {
    // HTML-based rendering
}

void CinematicUI::BindToHTML() {
    // TODO: Register JavaScript callbacks
}

void CinematicUI::HandleHTMLEvent(const std::string& eventName, const std::string& /*data*/) {
    if (eventName == "skipHoldStart" && m_onSkipHoldStart) {
        m_onSkipHoldStart();
    } else if (eventName == "skipHoldEnd" && m_onSkipHoldEnd) {
        m_onSkipHoldEnd();
    } else if (eventName == "skip" && m_onSkip) {
        m_onSkip();
    }
}

void CinematicUI::UpdateLetterbox(float deltaTime) {
    float speed = 1.0f / m_config.fadeSpeed;

    if (m_letterboxAmount < m_targetLetterbox) {
        m_letterboxAmount += speed * deltaTime;
        if (m_letterboxAmount > m_targetLetterbox) {
            m_letterboxAmount = m_targetLetterbox;
        }
    } else if (m_letterboxAmount > m_targetLetterbox) {
        m_letterboxAmount -= speed * deltaTime;
        if (m_letterboxAmount < m_targetLetterbox) {
            m_letterboxAmount = m_targetLetterbox;
        }
    }
}

void CinematicUI::UpdateChapterTitle(float deltaTime) {
    if (m_chapterTitleVisible && m_chapterTitleTimer > 0.0f) {
        m_chapterTitleTimer -= deltaTime;
        if (m_chapterTitleTimer <= 0.0f) {
            HideChapterTitle();
        }
    }
}

void CinematicUI::SendDataToHTML() {
    // TODO: Send UI state to HTML via JavaScript bindings
}

} // namespace Campaign
} // namespace UI
} // namespace Vehement
