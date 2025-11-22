#include "CampaignManager.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>

namespace Vehement {
namespace RTS {
namespace Campaign {

CampaignManager::CampaignManager() = default;
CampaignManager::~CampaignManager() = default;

CampaignManager& CampaignManager::Instance() {
    static CampaignManager instance;
    return instance;
}

bool CampaignManager::Initialize() {
    if (m_initialized) return true;

    m_campaigns.clear();
    m_currentCampaign = nullptr;
    m_globalProgress = GlobalProgress();

    LoadGlobalProgress();
    m_initialized = true;
    return true;
}

void CampaignManager::Shutdown() {
    SaveGlobalProgress();

    m_currentCampaign = nullptr;
    m_campaigns.clear();
    m_initialized = false;
}

void CampaignManager::LoadAllCampaigns(const std::string& campaignsDir) {
    // TODO: Iterate through campaigns directory and load each campaign
    // For now, create placeholder campaigns for each race
    for (int i = 0; i < static_cast<int>(RaceType::Count); ++i) {
        auto race = static_cast<RaceType>(i);
        auto campaign = CampaignFactory::CreateForRace(race);
        campaign->id = std::string(RaceTypeToString(race)) + "_campaign";
        campaign->title = std::string(RaceTypeToString(race)) + " Campaign";
        m_campaigns[campaign->id] = std::move(campaign);
    }
}

void CampaignManager::LoadCampaign(const std::string& campaignPath) {
    auto campaign = CampaignFactory::CreateFromConfig(campaignPath);
    if (campaign) {
        m_campaigns[campaign->id] = std::move(campaign);
    }
}

void CampaignManager::UnloadCampaign(const std::string& campaignId) {
    if (m_currentCampaign && m_currentCampaign->id == campaignId) {
        m_currentCampaign = nullptr;
    }
    m_campaigns.erase(campaignId);
}

void CampaignManager::ReloadCampaigns() {
    std::string currentId = m_currentCampaign ? m_currentCampaign->id : "";
    m_campaigns.clear();
    LoadAllCampaigns("game/assets/configs/campaigns/");

    if (!currentId.empty()) {
        m_currentCampaign = GetCampaign(currentId);
    }
}

Campaign* CampaignManager::GetCampaign(const std::string& campaignId) {
    auto it = m_campaigns.find(campaignId);
    return (it != m_campaigns.end()) ? it->second.get() : nullptr;
}

const Campaign* CampaignManager::GetCampaign(const std::string& campaignId) const {
    auto it = m_campaigns.find(campaignId);
    return (it != m_campaigns.end()) ? it->second.get() : nullptr;
}

Campaign* CampaignManager::GetCampaignForRace(RaceType race) {
    std::string raceId = RaceTypeToString(race);
    for (auto& [id, campaign] : m_campaigns) {
        if (campaign->race == race) {
            return campaign.get();
        }
    }
    return nullptr;
}

std::vector<Campaign*> CampaignManager::GetAllCampaigns() {
    std::vector<Campaign*> result;
    for (auto& [id, campaign] : m_campaigns) {
        result.push_back(campaign.get());
    }
    return result;
}

std::vector<Campaign*> CampaignManager::GetUnlockedCampaigns() {
    std::vector<Campaign*> result;
    for (auto& [id, campaign] : m_campaigns) {
        if (campaign->isUnlocked) {
            result.push_back(campaign.get());
        }
    }
    return result;
}

std::vector<Campaign*> CampaignManager::GetAvailableCampaigns() {
    std::vector<Campaign*> result;
    std::vector<Campaign> allCampaignsRef;

    for (auto& [id, campaign] : m_campaigns) {
        Campaign ref;
        ref.id = campaign->id;
        ref.state = campaign->state;
        allCampaignsRef.push_back(ref);
    }

    for (auto& [id, campaign] : m_campaigns) {
        if (campaign->isUnlocked ||
            campaign->CanUnlock(allCampaignsRef, m_globalProgress.globalFlags, m_globalProgress.playerLevel)) {
            result.push_back(campaign.get());
        }
    }
    return result;
}

Chapter* CampaignManager::GetCurrentChapter() {
    return m_currentCampaign ? m_currentCampaign->GetCurrentChapter() : nullptr;
}

Mission* CampaignManager::GetCurrentMission() {
    return m_currentCampaign ? m_currentCampaign->GetCurrentMission() : nullptr;
}

void CampaignManager::SetCurrentCampaign(const std::string& campaignId) {
    m_currentCampaign = GetCampaign(campaignId);
}

void CampaignManager::StartCampaign(const std::string& campaignId, CampaignDifficulty difficulty) {
    auto* campaign = GetCampaign(campaignId);
    if (!campaign) return;

    m_currentCampaign = campaign;
    campaign->Start(difficulty);

    if (m_onCampaignStart) {
        m_onCampaignStart(*campaign);
    }

    // Play intro cinematic if available
    if (!campaign->introCinematic.empty()) {
        PlayCampaignIntro();
    }
}

void CampaignManager::ResumeCampaign(const std::string& campaignId) {
    auto* campaign = GetCampaign(campaignId);
    if (!campaign || campaign->state == CampaignState::NotStarted) return;

    m_currentCampaign = campaign;
}

void CampaignManager::CompleteCampaign() {
    if (!m_currentCampaign) return;

    m_currentCampaign->Complete();
    GrantCampaignRewards(m_currentCampaign->rewards);

    m_globalProgress.totalCampaignsCompleted++;
    UpdateGlobalProgress();
    SaveGlobalProgress();

    if (m_onCampaignComplete) {
        m_onCampaignComplete(*m_currentCampaign);
    }

    // Play outro cinematic
    if (!m_currentCampaign->outroCinematic.empty()) {
        PlayCampaignOutro();
    }
}

void CampaignManager::AbandonCampaign() {
    if (!m_currentCampaign) return;

    m_currentCampaign->Reset();
    m_currentCampaign = nullptr;
}

void CampaignManager::StartMission(const std::string& missionId) {
    if (!m_currentCampaign) return;

    // Find mission in current campaign
    for (auto& chapter : m_currentCampaign->chapters) {
        if (auto* mission = chapter->GetMission(missionId)) {
            mission->Start(m_currentCampaign->difficulty == CampaignDifficulty::Story
                          ? MissionDifficulty::Easy
                          : static_cast<MissionDifficulty>(static_cast<int>(m_currentCampaign->difficulty)));

            if (m_onMissionStart) {
                m_onMissionStart(*mission);
            }

            // Play mission intro
            if (!mission->introCinematic.empty()) {
                PlayMissionIntro();
            }
            return;
        }
    }
}

void CampaignManager::CompleteMission() {
    auto* mission = GetCurrentMission();
    if (!mission) return;

    mission->Complete();
    GrantMissionRewards(mission->rewards);

    m_globalProgress.totalMissionsCompleted++;
    m_currentCampaign->AddMissionStatistics(mission->statistics);

    if (m_onMissionComplete) {
        m_onMissionComplete(*mission);
    }

    // Check chapter completion
    auto* chapter = GetCurrentChapter();
    if (chapter && chapter->IsComplete()) {
        CompleteChapter();
    }

    AutoSave();
}

void CampaignManager::FailMission() {
    auto* mission = GetCurrentMission();
    if (!mission) return;

    mission->Fail();

    if (m_onMissionFail) {
        m_onMissionFail(*mission);
    }
}

void CampaignManager::RestartMission() {
    auto* mission = GetCurrentMission();
    if (!mission) return;

    mission->Reset();
    mission->Start(mission->currentDifficulty);
}

void CampaignManager::SkipMission() {
    // Debug function
    auto* mission = GetCurrentMission();
    if (!mission) return;

    mission->Complete();
    AdvanceToNextMission();
}

void CampaignManager::AdvanceToNextMission() {
    if (!m_currentCampaign) return;

    m_currentCampaign->AdvanceToNextMission();

    auto* nextMission = GetCurrentMission();
    if (nextMission && nextMission->state == MissionState::Available) {
        // Play inter-mission cinematic if available
        auto* chapter = GetCurrentChapter();
        if (chapter && m_currentCampaign->currentMission > 0 &&
            m_currentCampaign->currentMission - 1 < static_cast<int32_t>(chapter->interMissionCinematics.size())) {
            PlayCinematic(chapter->interMissionCinematics[m_currentCampaign->currentMission - 1]);
        }
    }
}

void CampaignManager::StartChapter(const std::string& chapterId) {
    if (!m_currentCampaign) return;

    auto* chapter = m_currentCampaign->GetChapter(chapterId);
    if (!chapter) return;

    chapter->Start();

    if (m_onChapterStart) {
        m_onChapterStart(*chapter);
    }

    // Play chapter intro
    if (!chapter->introCinematic.empty()) {
        PlayChapterIntro();
    }
}

void CampaignManager::CompleteChapter() {
    auto* chapter = GetCurrentChapter();
    if (!chapter) return;

    chapter->Complete();
    GrantChapterRewards(chapter->rewards);

    if (m_onChapterComplete) {
        m_onChapterComplete(*chapter);
    }

    // Play chapter outro
    if (!chapter->outroCinematic.empty()) {
        PlayChapterOutro();
    }

    // Check campaign completion
    if (m_currentCampaign->IsComplete()) {
        CompleteCampaign();
    } else {
        AdvanceToNextChapter();
    }
}

void CampaignManager::AdvanceToNextChapter() {
    if (!m_currentCampaign) return;

    m_currentCampaign->AdvanceToNextChapter();
    UpdateCampaignUnlocks();
}

void CampaignManager::UnlockChapter(const std::string& chapterId) {
    if (!m_currentCampaign) return;

    if (auto* chapter = m_currentCampaign->GetChapter(chapterId)) {
        chapter->Unlock();
    }
}

void CampaignManager::SetCampaignFlag(const std::string& flag, bool value) {
    if (m_currentCampaign) {
        m_currentCampaign->SetFlag(flag, value);
    }
}

bool CampaignManager::GetCampaignFlag(const std::string& flag) const {
    return m_currentCampaign ? m_currentCampaign->GetFlag(flag) : false;
}

void CampaignManager::SetGlobalFlag(const std::string& flag, bool value) {
    m_globalProgress.globalFlags[flag] = value;
    SaveGlobalProgress();
}

bool CampaignManager::GetGlobalFlag(const std::string& flag) const {
    auto it = m_globalProgress.globalFlags.find(flag);
    return (it != m_globalProgress.globalFlags.end()) ? it->second : false;
}

void CampaignManager::GrantMissionRewards(const MissionRewards& rewards) {
    AddExperience(rewards.experienceBase);

    // Set story flags
    for (const auto& [flag, value] : rewards.storyFlags) {
        SetCampaignFlag(flag, value);
    }

    // Unlock achievement
    if (!rewards.achievement.empty()) {
        UnlockAchievement(rewards.achievement);
    }
}

void CampaignManager::GrantChapterRewards(const ChapterRewards& rewards) {
    AddExperience(rewards.experienceBonus);

    // Set story flags
    for (const auto& [flag, value] : rewards.storyFlags) {
        SetCampaignFlag(flag, value);
    }

    // Unlock achievement
    if (!rewards.achievement.empty()) {
        UnlockAchievement(rewards.achievement);
    }
}

void CampaignManager::GrantCampaignRewards(const CampaignRewards& rewards) {
    AddExperience(rewards.experienceTotal);

    // Unlock campaigns
    for (const auto& campaignId : rewards.unlockedCampaigns) {
        UnlockCampaign(campaignId);
    }

    // Unlock races
    for (const auto& raceId : rewards.unlockedRaces) {
        m_globalProgress.unlockedRaces.push_back(raceId);
    }

    // Unlock achievements
    for (const auto& achievement : rewards.achievements) {
        UnlockAchievement(achievement);
    }
}

void CampaignManager::AddExperience(int32_t amount) {
    m_globalProgress.playerExperience += amount;
    CheckLevelUp();
    SaveGlobalProgress();
}

void CampaignManager::UnlockAchievement(const std::string& achievementId) {
    if (std::find(m_globalProgress.achievements.begin(),
                  m_globalProgress.achievements.end(),
                  achievementId) == m_globalProgress.achievements.end()) {
        m_globalProgress.achievements.push_back(achievementId);
        // TODO: Trigger achievement notification
    }
}

void CampaignManager::UpdateGlobalProgress() {
    m_globalProgress.totalPlayTime = 0.0f;
    m_globalProgress.totalMissionsCompleted = 0;

    for (const auto& [id, campaign] : m_campaigns) {
        m_globalProgress.totalPlayTime += campaign->statistics.totalPlayTime;
        m_globalProgress.totalMissionsCompleted += campaign->GetCompletedMissions();
    }
}

int32_t CampaignManager::GetRequiredExperienceForLevel(int32_t level) const {
    // Exponential curve: 100 * level^1.5
    return static_cast<int32_t>(100.0f * std::pow(static_cast<float>(level), 1.5f));
}

bool CampaignManager::CheckLevelUp() {
    int32_t required = GetRequiredExperienceForLevel(m_globalProgress.playerLevel);

    while (m_globalProgress.playerExperience >= required) {
        m_globalProgress.playerExperience -= required;
        m_globalProgress.playerLevel++;

        if (m_onLevelUp) {
            m_onLevelUp(m_globalProgress.playerLevel);
        }

        required = GetRequiredExperienceForLevel(m_globalProgress.playerLevel);
    }

    return false;
}

std::vector<SaveSlot> CampaignManager::GetSaveSlots() {
    std::vector<SaveSlot> slots;
    for (int32_t i = 0; i < m_maxSaveSlots; ++i) {
        slots.push_back(GetSaveSlotInfo(i));
    }
    return slots;
}

bool CampaignManager::SaveGame(int32_t slotIndex) {
    if (!m_currentCampaign) return false;

    std::string savePath = GenerateSavePath(slotIndex);

    // Create save data
    std::ostringstream oss;
    oss << "{";
    oss << "\"campaignId\":\"" << m_currentCampaign->id << "\",";
    oss << "\"timestamp\":\"" << std::time(nullptr) << "\",";
    oss << "\"campaignProgress\":" << m_currentCampaign->SerializeProgress() << ",";
    oss << "\"globalProgress\":{";
    oss << "\"playerLevel\":" << m_globalProgress.playerLevel << ",";
    oss << "\"playerExperience\":" << m_globalProgress.playerExperience;
    oss << "}}";

    std::ofstream file(savePath);
    if (!file.is_open()) return false;
    file << oss.str();
    return true;
}

bool CampaignManager::LoadGame(int32_t slotIndex) {
    std::string savePath = GenerateSavePath(slotIndex);

    std::ifstream file(savePath);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();

    // TODO: Parse save data and restore state
    return true;
}

bool CampaignManager::QuickSave() {
    return SaveGame(m_quickSaveSlot);
}

bool CampaignManager::QuickLoad() {
    return LoadGame(m_quickSaveSlot);
}

bool CampaignManager::AutoSave() {
    return SaveGame(m_autoSaveSlot);
}

bool CampaignManager::DeleteSave(int32_t slotIndex) {
    std::string savePath = GenerateSavePath(slotIndex);
    return std::filesystem::remove(savePath);
}

SaveSlot CampaignManager::GetSaveSlotInfo(int32_t slotIndex) const {
    SaveSlot slot;
    slot.slotIndex = slotIndex;
    slot.savePath = GenerateSavePath(slotIndex);
    slot.isEmpty = !std::filesystem::exists(slot.savePath);

    if (!slot.isEmpty) {
        // TODO: Load save metadata
    }

    return slot;
}

void CampaignManager::UpdateCampaignUnlocks() {
    std::vector<Campaign> allCampaignsRef;
    for (const auto& [id, campaign] : m_campaigns) {
        Campaign ref;
        ref.id = campaign->id;
        ref.state = campaign->state;
        allCampaignsRef.push_back(ref);
    }

    for (auto& [id, campaign] : m_campaigns) {
        if (!campaign->isUnlocked &&
            campaign->CanUnlock(allCampaignsRef, m_globalProgress.globalFlags, m_globalProgress.playerLevel)) {
            campaign->isUnlocked = true;
            m_globalProgress.unlockedCampaigns.push_back(id);
        }
    }
}

bool CampaignManager::IsCampaignUnlocked(const std::string& campaignId) const {
    const auto* campaign = GetCampaign(campaignId);
    return campaign && campaign->isUnlocked;
}

bool CampaignManager::IsChapterUnlocked(const std::string& chapterId) const {
    if (!m_currentCampaign) return false;
    const auto* chapter = m_currentCampaign->GetChapter(chapterId);
    return chapter && !chapter->IsLocked();
}

bool CampaignManager::IsMissionUnlocked(const std::string& missionId) const {
    if (!m_currentCampaign) return false;
    for (const auto& chapter : m_currentCampaign->chapters) {
        const auto* mission = chapter->GetMission(missionId);
        if (mission && mission->state != MissionState::Locked) {
            return true;
        }
    }
    return false;
}

void CampaignManager::UnlockCampaign(const std::string& campaignId) {
    if (auto* campaign = GetCampaign(campaignId)) {
        campaign->isUnlocked = true;
        if (std::find(m_globalProgress.unlockedCampaigns.begin(),
                      m_globalProgress.unlockedCampaigns.end(),
                      campaignId) == m_globalProgress.unlockedCampaigns.end()) {
            m_globalProgress.unlockedCampaigns.push_back(campaignId);
        }
    }
}

void CampaignManager::PlayCinematic(const std::string& cinematicId) {
    if (!m_currentCampaign) return;

    if (auto* cinematic = m_currentCampaign->GetCinematic(cinematicId)) {
        TriggerCinematic(cinematic);
    }
}

void CampaignManager::PlayCampaignIntro() {
    if (m_currentCampaign && !m_currentCampaign->introCinematic.empty()) {
        PlayCinematic(m_currentCampaign->introCinematic);
    }
}

void CampaignManager::PlayCampaignOutro() {
    if (m_currentCampaign && !m_currentCampaign->outroCinematic.empty()) {
        PlayCinematic(m_currentCampaign->outroCinematic);
    }
}

void CampaignManager::PlayChapterIntro() {
    auto* chapter = GetCurrentChapter();
    if (chapter && !chapter->introCinematic.empty()) {
        PlayCinematic(chapter->introCinematic);
    }
}

void CampaignManager::PlayChapterOutro() {
    auto* chapter = GetCurrentChapter();
    if (chapter && !chapter->outroCinematic.empty()) {
        PlayCinematic(chapter->outroCinematic);
    }
}

void CampaignManager::PlayMissionIntro() {
    auto* mission = GetCurrentMission();
    if (mission && !mission->introCinematic.empty()) {
        PlayCinematic(mission->introCinematic);
    }
}

void CampaignManager::PlayMissionOutro() {
    auto* mission = GetCurrentMission();
    if (mission && !mission->outroCinematic.empty()) {
        PlayCinematic(mission->outroCinematic);
    }
}

void CampaignManager::Update(float deltaTime) {
    if (!m_initialized) return;

    if (m_currentCampaign) {
        m_currentCampaign->Update(deltaTime);
        m_globalProgress.totalPlayTime += deltaTime;
    }
}

void CampaignManager::LoadGlobalProgress() {
    std::string progressPath = m_saveDirectory + "global_progress.json";
    std::ifstream file(progressPath);
    if (!file.is_open()) return;

    // TODO: Parse JSON and load progress
}

void CampaignManager::SaveGlobalProgress() {
    std::string progressPath = m_saveDirectory + "global_progress.json";

    std::ostringstream oss;
    oss << "{";
    oss << "\"playerLevel\":" << m_globalProgress.playerLevel << ",";
    oss << "\"playerExperience\":" << m_globalProgress.playerExperience << ",";
    oss << "\"totalPlayTime\":" << m_globalProgress.totalPlayTime << ",";
    oss << "\"totalMissionsCompleted\":" << m_globalProgress.totalMissionsCompleted << ",";
    oss << "\"totalCampaignsCompleted\":" << m_globalProgress.totalCampaignsCompleted;
    oss << "}";

    std::filesystem::create_directories(m_saveDirectory);
    std::ofstream file(progressPath);
    if (file.is_open()) {
        file << oss.str();
    }
}

std::string CampaignManager::GenerateSavePath(int32_t slotIndex) const {
    std::ostringstream oss;
    oss << m_saveDirectory << "save_" << slotIndex << ".json";
    return oss.str();
}

void CampaignManager::TriggerCinematic(Cinematic* cinematic) {
    if (!cinematic) return;

    auto& player = CinematicPlayer::Instance();
    player.SetOnEnd([this]() { HandleCinematicComplete(); });
    player.Play(cinematic);
}

void CampaignManager::HandleCinematicComplete() {
    // Continue with mission/chapter flow after cinematic
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
