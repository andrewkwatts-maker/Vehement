#include "BriefingUI.hpp"

namespace Vehement {
namespace UI {
namespace Campaign {

BriefingUI::BriefingUI() = default;
BriefingUI::~BriefingUI() = default;

BriefingUI& BriefingUI::Instance() {
    static BriefingUI instance;
    return instance;
}

bool BriefingUI::Initialize() {
    if (m_initialized) return true;
    m_config = BriefingUIConfig();
    m_visible = false;
    m_initialized = true;
    return true;
}

void BriefingUI::Shutdown() {
    StopVoiceover();
    m_initialized = false;
}

void BriefingUI::SetConfig(const BriefingUIConfig& config) {
    m_config = config;
}

void BriefingUI::Show() {
    m_visible = true;
    m_textScrollPosition = 0.0f;
    SendDataToHTML();

    if (m_config.autoPlayVoiceover && !m_briefingData.voiceoverFile.empty()) {
        PlayVoiceover();
    }
}

void BriefingUI::Hide() {
    m_visible = false;
    StopVoiceover();
}

void BriefingUI::SetBriefingData(const BriefingData& data) {
    m_briefingData = data;
    m_textScrollPosition = 0.0f;
    SendDataToHTML();
}

void BriefingUI::ClearBriefing() {
    m_briefingData = BriefingData();
    SendDataToHTML();
}

void BriefingUI::ShowObjectivesPanel() {
    m_activePanel = "objectives";
    SendDataToHTML();
}

void BriefingUI::ShowTipsPanel() {
    m_activePanel = "tips";
    SendDataToHTML();
}

void BriefingUI::ShowIntelPanel() {
    m_activePanel = "intel";
    SendDataToHTML();
}

void BriefingUI::SetActivePanel(const std::string& panelId) {
    m_activePanel = panelId;
    SendDataToHTML();
}

void BriefingUI::SetDifficulty(int32_t difficulty) {
    m_selectedDifficulty = difficulty;
    if (m_onDifficultyChange) {
        m_onDifficultyChange(difficulty);
    }
    SendDataToHTML();
}

void BriefingUI::PlayVoiceover() {
    if (!m_config.enableVoiceover || m_briefingData.voiceoverFile.empty()) return;
    m_voiceoverPlaying = true;
    // TODO: Play audio file
}

void BriefingUI::StopVoiceover() {
    m_voiceoverPlaying = false;
    // TODO: Stop audio
}

void BriefingUI::PauseVoiceover() {
    if (m_voiceoverPlaying) {
        // TODO: Pause audio
    }
}

void BriefingUI::ResumeVoiceover() {
    if (m_voiceoverPlaying) {
        // TODO: Resume audio
    }
}

void BriefingUI::ScrollTextToTop() {
    m_textScrollPosition = 0.0f;
    SendDataToHTML();
}

void BriefingUI::ScrollTextToBottom() {
    m_textScrollPosition = 1.0f;
    SendDataToHTML();
}

void BriefingUI::SetTextScrollPosition(float position) {
    m_textScrollPosition = position;
    SendDataToHTML();
}

void BriefingUI::StartMission() {
    if (m_onStartMission) {
        m_onStartMission(m_selectedDifficulty);
    }
}

void BriefingUI::GoBack() {
    if (m_onBack) {
        m_onBack();
    }
}

void BriefingUI::Update(float deltaTime) {
    if (!m_visible) return;
    UpdateVoiceover(deltaTime);
}

void BriefingUI::Render() {
    // HTML-based rendering
}

void BriefingUI::BindToHTML() {
    // TODO: Register JavaScript callbacks
}

void BriefingUI::HandleHTMLEvent(const std::string& eventName, const std::string& data) {
    if (eventName == "startMission") {
        StartMission();
    } else if (eventName == "back") {
        GoBack();
    } else if (eventName == "setDifficulty") {
        SetDifficulty(std::stoi(data));
    } else if (eventName == "setPanel") {
        SetActivePanel(data);
    } else if (eventName == "playVoiceover") {
        PlayVoiceover();
    } else if (eventName == "stopVoiceover") {
        StopVoiceover();
    }
}

void BriefingUI::SendDataToHTML() {
    // TODO: Send briefing data to HTML via JavaScript bindings
}

void BriefingUI::UpdateVoiceover(float /*deltaTime*/) {
    // TODO: Update voiceover playback state
}

} // namespace Campaign
} // namespace UI
} // namespace Vehement
