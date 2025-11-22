#include "CampaignUI.hpp"
#include <algorithm>

namespace Vehement {
namespace UI {
namespace Campaign {

CampaignUI::CampaignUI() = default;
CampaignUI::~CampaignUI() = default;

CampaignUI& CampaignUI::Instance() {
    static CampaignUI instance;
    return instance;
}

bool CampaignUI::Initialize() {
    if (m_initialized) return true;

    m_config = CampaignUIConfig();
    m_viewMode = CampaignViewMode::RaceSelect;
    m_visible = false;
    m_initialized = true;
    return true;
}

void CampaignUI::Shutdown() {
    m_campaigns.clear();
    m_chapters.clear();
    m_missions.clear();
    m_initialized = false;
}

void CampaignUI::SetConfig(const CampaignUIConfig& config) {
    m_config = config;
}

void CampaignUI::Show() {
    m_visible = true;
    RefreshData();
    UpdateHTMLView();
}

void CampaignUI::Hide() {
    m_visible = false;
}

void CampaignUI::Toggle() {
    if (m_visible) Hide();
    else Show();
}

void CampaignUI::SetViewMode(CampaignViewMode mode) {
    m_viewMode = mode;
    m_isTransitioning = true;
    m_transitionProgress = 0.0f;
    UpdateHTMLView();
}

void CampaignUI::NavigateBack() {
    switch (m_viewMode) {
        case CampaignViewMode::Briefing:
            SetViewMode(CampaignViewMode::MissionSelect);
            break;
        case CampaignViewMode::MissionSelect:
            SetViewMode(CampaignViewMode::ChapterSelect);
            break;
        case CampaignViewMode::ChapterSelect:
            SetViewMode(CampaignViewMode::RaceSelect);
            break;
        case CampaignViewMode::RaceSelect:
            if (m_onBack) m_onBack();
            break;
    }
}

void CampaignUI::NavigateToRaceSelect() {
    SetViewMode(CampaignViewMode::RaceSelect);
}

void CampaignUI::NavigateToChapters(const std::string& campaignId) {
    m_selectedCampaignId = campaignId;
    SetViewMode(CampaignViewMode::ChapterSelect);

    if (m_onCampaignSelect) {
        m_onCampaignSelect(campaignId);
    }
}

void CampaignUI::NavigateToMissions(const std::string& chapterId) {
    m_selectedChapterId = chapterId;
    SetViewMode(CampaignViewMode::MissionSelect);

    if (m_onChapterSelect) {
        m_onChapterSelect(chapterId);
    }
}

void CampaignUI::NavigateToBriefing(const std::string& missionId) {
    m_selectedMissionId = missionId;
    SetViewMode(CampaignViewMode::Briefing);

    if (m_onMissionSelect) {
        m_onMissionSelect(missionId);
    }
}

void CampaignUI::SelectCampaign(const std::string& campaignId) {
    m_selectedCampaignId = campaignId;

    // Update carousel index
    for (size_t i = 0; i < m_campaigns.size(); ++i) {
        if (m_campaigns[i].id == campaignId) {
            m_carouselIndex = static_cast<int32_t>(i);
            break;
        }
    }
}

void CampaignUI::SelectChapter(const std::string& chapterId) {
    m_selectedChapterId = chapterId;
}

void CampaignUI::SelectMission(const std::string& missionId) {
    m_selectedMissionId = missionId;
}

void CampaignUI::SelectDifficulty(int32_t difficulty) {
    m_selectedDifficulty = difficulty;
}

void CampaignUI::SetCampaigns(const std::vector<CampaignCardData>& campaigns) {
    m_campaigns = campaigns;
    SendDataToHTML();
}

void CampaignUI::SetChapters(const std::vector<ChapterCardData>& chapters) {
    m_chapters = chapters;
    SendDataToHTML();
}

void CampaignUI::SetMissions(const std::vector<MissionCardData>& missions) {
    m_missions = missions;
    SendDataToHTML();
}

void CampaignUI::RefreshData() {
    // TODO: Request data from CampaignManager
    SendDataToHTML();
}

void CampaignUI::NextCampaign() {
    if (!m_campaigns.empty()) {
        m_carouselIndex = (m_carouselIndex + 1) % static_cast<int32_t>(m_campaigns.size());
        m_selectedCampaignId = m_campaigns[m_carouselIndex].id;
        SendDataToHTML();
    }
}

void CampaignUI::PreviousCampaign() {
    if (!m_campaigns.empty()) {
        m_carouselIndex = (m_carouselIndex - 1 + static_cast<int32_t>(m_campaigns.size())) %
                          static_cast<int32_t>(m_campaigns.size());
        m_selectedCampaignId = m_campaigns[m_carouselIndex].id;
        SendDataToHTML();
    }
}

void CampaignUI::SetCarouselIndex(int32_t index) {
    if (index >= 0 && index < static_cast<int32_t>(m_campaigns.size())) {
        m_carouselIndex = index;
        m_selectedCampaignId = m_campaigns[m_carouselIndex].id;
        SendDataToHTML();
    }
}

void CampaignUI::StartSelectedMission() {
    if (!m_selectedMissionId.empty() && m_onStartMission) {
        m_onStartMission(m_selectedMissionId, m_selectedDifficulty);
    }
}

void CampaignUI::ContinueCampaign() {
    // Navigate to current mission in current chapter
    if (!m_selectedCampaignId.empty()) {
        // TODO: Get current chapter/mission from campaign manager
    }
}

void CampaignUI::StartNewCampaign() {
    if (!m_selectedCampaignId.empty()) {
        NavigateToChapters(m_selectedCampaignId);
    }
}

void CampaignUI::Update(float deltaTime) {
    if (!m_visible) return;

    UpdateCarouselAnimation(deltaTime);

    if (m_isTransitioning) {
        m_transitionProgress += deltaTime / m_config.animationSpeed;
        if (m_transitionProgress >= 1.0f) {
            m_transitionProgress = 1.0f;
            m_isTransitioning = false;
        }
    }
}

void CampaignUI::Render() {
    if (!m_visible) return;
    // HTML-based rendering handled by browser
}

void CampaignUI::BindToHTML() {
    // TODO: Register JavaScript callbacks
}

void CampaignUI::HandleHTMLEvent(const std::string& eventName, const std::string& data) {
    if (eventName == "selectCampaign") {
        NavigateToChapters(data);
    } else if (eventName == "selectChapter") {
        NavigateToMissions(data);
    } else if (eventName == "selectMission") {
        NavigateToBriefing(data);
    } else if (eventName == "startMission") {
        StartSelectedMission();
    } else if (eventName == "back") {
        NavigateBack();
    } else if (eventName == "nextCampaign") {
        NextCampaign();
    } else if (eventName == "prevCampaign") {
        PreviousCampaign();
    } else if (eventName == "selectDifficulty") {
        SelectDifficulty(std::stoi(data));
    }
}

void CampaignUI::UpdateCarouselAnimation(float /*deltaTime*/) {
    // TODO: Implement smooth carousel animation
}

void CampaignUI::SendDataToHTML() {
    // TODO: Send current data to HTML UI via JavaScript bindings
}

void CampaignUI::UpdateHTMLView() {
    // TODO: Update HTML view based on current mode
}

} // namespace Campaign
} // namespace UI
} // namespace Vehement
