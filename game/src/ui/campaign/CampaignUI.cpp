#include "CampaignUI.hpp"
#include "game/src/rts/campaign/CampaignManager.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

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
    // Request data from CampaignManager
    auto& campaignManager = RTS::Campaign::CampaignManager::Instance();

    // Get available campaigns and convert to card data
    auto campaigns = campaignManager.GetAvailableCampaigns();
    std::vector<CampaignCardData> campaignCards;
    campaignCards.reserve(campaigns.size());

    for (auto* campaign : campaigns) {
        if (!campaign) continue;

        CampaignCardData card;
        card.id = campaign->id;
        card.title = campaign->title;
        card.description = campaign->description;
        card.raceId = campaign->raceId;
        card.thumbnailImage = campaign->thumbnailImage;
        card.bannerImage = campaign->bannerImage;
        card.isLocked = !campaignManager.IsCampaignUnlocked(card.id);
        card.isCompleted = campaign->IsComplete();

        // Calculate completion percentage
        card.chaptersCompleted = campaign->GetCompletedChapters();
        card.chaptersTotal = campaign->GetTotalChapters();
        card.completionPercent = campaign->GetCompletionPercentage();

        campaignCards.push_back(std::move(card));
    }

    SetCampaigns(campaignCards);

    // Load chapters if a campaign is selected
    if (!m_selectedCampaignId.empty()) {
        auto* campaign = campaignManager.GetCampaign(m_selectedCampaignId);
        if (campaign) {
            std::vector<ChapterCardData> chapterCards;

            for (size_t i = 0; i < campaign->chapters.size(); ++i) {
                const auto& chapter = campaign->chapters[i];
                if (!chapter) continue;

                ChapterCardData card;
                card.id = chapter->id;
                card.title = chapter->title;
                card.subtitle = chapter->subtitle;
                card.description = chapter->description;
                card.thumbnailImage = chapter->thumbnailImage;
                card.chapterNumber = chapter->chapterNumber;
                card.isLocked = !campaignManager.IsChapterUnlocked(card.id);
                card.isCompleted = chapter->IsComplete();
                card.isCurrent = (campaign->currentChapter == static_cast<int32_t>(i));
                card.missionsTotal = chapter->GetTotalMissionCount();
                card.missionsCompleted = chapter->GetCompletedMissionCount();
                card.completionPercent = chapter->GetCompletionPercentage();

                chapterCards.push_back(std::move(card));
            }
            SetChapters(chapterCards);
        }
    }

    // Load missions if a chapter is selected
    if (!m_selectedChapterId.empty() && !m_selectedCampaignId.empty()) {
        auto* campaign = campaignManager.GetCampaign(m_selectedCampaignId);
        if (campaign) {
            auto* chapter = campaign->GetChapter(m_selectedChapterId);
            if (chapter) {
                std::vector<MissionCardData> missionCards;

                for (size_t i = 0; i < chapter->missions.size(); ++i) {
                    const auto& mission = chapter->missions[i];
                    if (!mission) continue;

                    MissionCardData card;
                    card.id = mission->id;
                    card.title = mission->title;
                    card.description = mission->description;
                    card.mapPreview = mission->briefing.mapPreviewImage;
                    card.missionNumber = mission->missionNumber;
                    card.isLocked = !campaignManager.IsMissionUnlocked(card.id);
                    card.isCompleted = (mission->state == RTS::Campaign::MissionState::Completed);
                    // Check if this is the current mission by comparing with the chapter's current mission
                    auto* currentMission = chapter->GetCurrentMission();
                    card.isCurrent = (currentMission && currentMission->id == mission->id);
                    card.bestGrade = mission->statistics.grade;
                    card.bestScore = mission->statistics.score;
                    card.bestTime = mission->statistics.completionTime;

                    missionCards.push_back(std::move(card));
                }
                SetMissions(missionCards);
            }
        }
    }

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
        // Get current chapter/mission from campaign manager
        auto& campaignManager = RTS::Campaign::CampaignManager::Instance();
        campaignManager.SetCurrentCampaign(m_selectedCampaignId);

        // Get current chapter from campaign
        auto* currentChapter = campaignManager.GetCurrentChapter();
        if (currentChapter) {
            m_selectedChapterId = currentChapter->id;

            // Get current mission within the chapter
            auto* currentMission = campaignManager.GetCurrentMission();
            if (currentMission) {
                m_selectedMissionId = currentMission->id;
                // Navigate directly to briefing for current mission
                NavigateToBriefing(m_selectedMissionId);
            } else {
                // No current mission, navigate to mission select for this chapter
                NavigateToMissions(m_selectedChapterId);
            }
        } else {
            // No current chapter, navigate to chapter select
            NavigateToChapters(m_selectedCampaignId);
        }
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
    // Register JavaScript callbacks for mission selection and campaign navigation
    // Note: This uses the global UIBinding system pattern from the codebase
    // The JavaScript side calls these handlers when user interacts with the UI

    // Callbacks are exposed via HandleHTMLEvent which processes:
    // - "selectCampaign": User selects a campaign from carousel
    // - "selectChapter": User selects a chapter
    // - "selectMission": User selects a mission
    // - "startMission": User clicks start mission button
    // - "back": User clicks back button
    // - "nextCampaign": User navigates to next campaign in carousel
    // - "prevCampaign": User navigates to previous campaign in carousel
    // - "selectDifficulty": User changes difficulty setting
    // The actual callback invocations are handled in HandleHTMLEvent()
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

void CampaignUI::UpdateCarouselAnimation(float deltaTime) {
    // Implement smooth carousel animation using lerp-based scrolling
    // This creates a smooth, spring-like animation toward the target index

    const float targetPosition = static_cast<float>(m_carouselIndex);
    const float positionDifference = targetPosition - m_carouselPosition;

    // Early exit if already at target
    if (std::abs(positionDifference) < 0.001f && std::abs(m_carouselVelocity) < 0.001f) {
        m_carouselPosition = targetPosition;
        m_carouselVelocity = 0.0f;
        return;
    }

    // Spring-damper animation parameters
    const float springStiffness = 15.0f;  // How quickly it accelerates toward target
    const float damping = 8.0f;           // How quickly velocity is dampened

    // Calculate spring force toward target
    const float springForce = positionDifference * springStiffness;

    // Apply damping to velocity
    const float dampingForce = -m_carouselVelocity * damping;

    // Update velocity with forces
    m_carouselVelocity += (springForce + dampingForce) * deltaTime;

    // Update position
    m_carouselPosition += m_carouselVelocity * deltaTime;

    // Clamp to valid range (with small overshoot allowed for bounce effect)
    const float minPos = -0.2f;
    const float maxPos = static_cast<float>(m_campaigns.size()) - 0.8f;
    if (m_carouselPosition < minPos) {
        m_carouselPosition = minPos;
        m_carouselVelocity = std::abs(m_carouselVelocity) * 0.3f; // Bounce back
    } else if (m_carouselPosition > maxPos) {
        m_carouselPosition = maxPos;
        m_carouselVelocity = -std::abs(m_carouselVelocity) * 0.3f; // Bounce back
    }

    // Send updated position to HTML for visual update
    SendDataToHTML();
}

void CampaignUI::SendDataToHTML() {
    // Send current campaign data to HTML UI via JavaScript bindings
    nlohmann::json state;

    // Current view mode
    state["viewMode"] = static_cast<int>(m_viewMode);
    state["visible"] = m_visible;

    // Selection state
    state["selection"] = {
        {"campaignId", m_selectedCampaignId},
        {"chapterId", m_selectedChapterId},
        {"missionId", m_selectedMissionId},
        {"difficulty", m_selectedDifficulty}
    };

    // Carousel state
    state["carousel"] = {
        {"index", m_carouselIndex},
        {"position", m_carouselPosition},
        {"velocity", m_carouselVelocity}
    };

    // Transition state
    state["transition"] = {
        {"active", m_isTransitioning},
        {"progress", m_transitionProgress}
    };

    // Campaign cards data
    nlohmann::json campaignsArray = nlohmann::json::array();
    for (const auto& campaign : m_campaigns) {
        campaignsArray.push_back({
            {"id", campaign.id},
            {"title", campaign.title},
            {"description", campaign.description},
            {"raceId", campaign.raceId},
            {"thumbnailImage", campaign.thumbnailImage},
            {"bannerImage", campaign.bannerImage},
            {"completionPercent", campaign.completionPercent},
            {"chaptersCompleted", campaign.chaptersCompleted},
            {"chaptersTotal", campaign.chaptersTotal},
            {"isLocked", campaign.isLocked},
            {"isCompleted", campaign.isCompleted},
            {"unlockRequirement", campaign.unlockRequirement}
        });
    }
    state["campaigns"] = campaignsArray;

    // Chapter cards data
    nlohmann::json chaptersArray = nlohmann::json::array();
    for (const auto& chapter : m_chapters) {
        chaptersArray.push_back({
            {"id", chapter.id},
            {"title", chapter.title},
            {"subtitle", chapter.subtitle},
            {"description", chapter.description},
            {"thumbnailImage", chapter.thumbnailImage},
            {"chapterNumber", chapter.chapterNumber},
            {"completionPercent", chapter.completionPercent},
            {"missionsCompleted", chapter.missionsCompleted},
            {"missionsTotal", chapter.missionsTotal},
            {"isLocked", chapter.isLocked},
            {"isCompleted", chapter.isCompleted},
            {"isCurrent", chapter.isCurrent},
            {"unlockRequirement", chapter.unlockRequirement}
        });
    }
    state["chapters"] = chaptersArray;

    // Mission cards data
    nlohmann::json missionsArray = nlohmann::json::array();
    for (const auto& mission : m_missions) {
        missionsArray.push_back({
            {"id", mission.id},
            {"title", mission.title},
            {"description", mission.description},
            {"mapPreview", mission.mapPreview},
            {"missionNumber", mission.missionNumber},
            {"isLocked", mission.isLocked},
            {"isCompleted", mission.isCompleted},
            {"isCurrent", mission.isCurrent},
            {"bestGrade", mission.bestGrade},
            {"bestScore", mission.bestScore},
            {"bestTime", mission.bestTime},
            {"difficulty", mission.difficulty}
        });
    }
    state["missions"] = missionsArray;

    // Configuration
    state["config"] = {
        {"showLockedContent", m_config.showLockedContent},
        {"showCompletionStats", m_config.showCompletionStats},
        {"enableChapterSelect", m_config.enableChapterSelect},
        {"enableMissionReplay", m_config.enableMissionReplay},
        {"showDifficultySelect", m_config.showDifficultySelect},
        {"animationSpeed", m_config.animationSpeed},
        {"themeStyle", m_config.themeStyle}
    };

    // The state JSON would be sent to JavaScript via UIBinding::EmitEvent
    // or ExecuteScript pattern used elsewhere in the codebase
    // Example: uiBinding.EmitEvent("campaign.stateUpdate", state);
}

void CampaignUI::UpdateHTMLView() {
    // Update HTML view based on current mode
    // This triggers a full view refresh when the mode changes

    nlohmann::json viewUpdate;
    viewUpdate["viewMode"] = static_cast<int>(m_viewMode);

    // View mode names for JavaScript
    switch (m_viewMode) {
        case CampaignViewMode::RaceSelect:
            viewUpdate["viewName"] = "raceSelect";
            viewUpdate["title"] = "Select Campaign";
            viewUpdate["showBackButton"] = false;
            break;
        case CampaignViewMode::ChapterSelect:
            viewUpdate["viewName"] = "chapterSelect";
            viewUpdate["title"] = "Select Chapter";
            viewUpdate["showBackButton"] = true;
            break;
        case CampaignViewMode::MissionSelect:
            viewUpdate["viewName"] = "missionSelect";
            viewUpdate["title"] = "Select Mission";
            viewUpdate["showBackButton"] = true;
            break;
        case CampaignViewMode::Briefing:
            viewUpdate["viewName"] = "briefing";
            viewUpdate["title"] = "Mission Briefing";
            viewUpdate["showBackButton"] = true;
            break;
    }

    // Include transition animation info
    viewUpdate["transition"] = {
        {"active", m_isTransitioning},
        {"progress", m_transitionProgress},
        {"animationSpeed", m_config.animationSpeed}
    };

    // Include selected IDs for context
    viewUpdate["context"] = {
        {"campaignId", m_selectedCampaignId},
        {"chapterId", m_selectedChapterId},
        {"missionId", m_selectedMissionId}
    };

    // The viewUpdate JSON would be sent to JavaScript to trigger view change
    // Example: uiBinding.EmitEvent("campaign.viewChange", viewUpdate);

    // Also send full data for the new view
    SendDataToHTML();
}

} // namespace Campaign
} // namespace UI
} // namespace Vehement
