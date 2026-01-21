#include "Campaign.hpp"
#include "engine/core/json_wrapper.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace Vehement {
namespace RTS {
namespace Campaign {

Campaign::Campaign(const std::string& campaignId) : id(campaignId) {}

void Campaign::Initialize() {
    currentChapter = 0;
    currentMission = 0;
    statistics = CampaignStatistics();
    flags.clear();

    // Initialize all chapters
    for (auto& chapter : chapters) {
        chapter->Reset();
    }

    // Unlock first chapter
    if (!chapters.empty()) {
        chapters[0]->Unlock();
    }
}

void Campaign::Start(CampaignDifficulty selectedDifficulty) {
    difficulty = selectedDifficulty;
    state = CampaignState::InProgress;
    Initialize();
}

void Campaign::Update(float deltaTime) {
    if (state != CampaignState::InProgress) return;

    statistics.totalPlayTime += deltaTime;
    UpdateChapterUnlocks();
    CheckCampaignComplete();
}

void Campaign::Complete() {
    state = CampaignState::Completed;
    UpdateStatistics();
}

void Campaign::Reset() {
    state = CampaignState::NotStarted;
    currentChapter = 0;
    currentMission = 0;
    statistics = CampaignStatistics();
    flags.clear();

    for (auto& chapter : chapters) {
        chapter->Reset();
    }
}

Chapter* Campaign::GetChapter(const std::string& chapterId) {
    auto it = std::find_if(chapters.begin(), chapters.end(),
        [&chapterId](const std::unique_ptr<Chapter>& c) { return c->id == chapterId; });
    return (it != chapters.end()) ? it->get() : nullptr;
}

const Chapter* Campaign::GetChapter(const std::string& chapterId) const {
    auto it = std::find_if(chapters.begin(), chapters.end(),
        [&chapterId](const std::unique_ptr<Chapter>& c) { return c->id == chapterId; });
    return (it != chapters.end()) ? it->get() : nullptr;
}

Chapter* Campaign::GetChapterByIndex(size_t index) {
    return (index < chapters.size()) ? chapters[index].get() : nullptr;
}

const Chapter* Campaign::GetChapterByIndex(size_t index) const {
    return (index < chapters.size()) ? chapters[index].get() : nullptr;
}

Chapter* Campaign::GetCurrentChapter() {
    return GetChapterByIndex(static_cast<size_t>(currentChapter));
}

Mission* Campaign::GetCurrentMission() {
    if (auto* chapter = GetCurrentChapter()) {
        return chapter->GetMissionByIndex(static_cast<size_t>(currentMission));
    }
    return nullptr;
}

void Campaign::AddChapter(std::unique_ptr<Chapter> chapter) {
    chapters.push_back(std::move(chapter));
}

void Campaign::AdvanceToNextMission() {
    if (auto* chapter = GetCurrentChapter()) {
        currentMission++;
        if (currentMission >= chapter->GetTotalMissionCount()) {
            AdvanceToNextChapter();
        }
    }
}

void Campaign::AdvanceToNextChapter() {
    currentChapter++;
    currentMission = 0;

    if (currentChapter >= static_cast<int32_t>(chapters.size())) {
        CheckCampaignComplete();
    } else {
        if (auto* chapter = GetCurrentChapter()) {
            chapter->Start();
        }
    }
}

void Campaign::SetCurrentMission(const std::string& chapterId, const std::string& missionId) {
    for (size_t i = 0; i < chapters.size(); ++i) {
        if (chapters[i]->id == chapterId) {
            currentChapter = static_cast<int32_t>(i);
            auto* chapter = chapters[i].get();

            for (size_t j = 0; j < chapter->missions.size(); ++j) {
                if (chapter->missions[j]->id == missionId) {
                    currentMission = static_cast<int32_t>(j);
                    break;
                }
            }
            break;
        }
    }
}

Cinematic* Campaign::GetCinematic(const std::string& cinematicId) {
    auto it = std::find_if(cinematics.begin(), cinematics.end(),
        [&cinematicId](const std::unique_ptr<Cinematic>& c) { return c->id == cinematicId; });
    return (it != cinematics.end()) ? it->get() : nullptr;
}

void Campaign::AddCinematic(std::unique_ptr<Cinematic> cinematic) {
    cinematics.push_back(std::move(cinematic));
}

void Campaign::SetFlag(const std::string& flagName, bool value) {
    flags[flagName] = value;
}

bool Campaign::GetFlag(const std::string& flagName) const {
    auto it = flags.find(flagName);
    return (it != flags.end()) ? it->second : false;
}

bool Campaign::HasFlag(const std::string& flagName) const {
    return flags.find(flagName) != flags.end();
}

void Campaign::ClearFlag(const std::string& flagName) {
    flags.erase(flagName);
}

bool Campaign::IsComplete() const {
    return state == CampaignState::Completed ||
           (GetCompletedChapters() >= GetTotalChapters() && GetTotalChapters() > 0);
}

bool Campaign::CanUnlock(const std::vector<Campaign>& allCampaigns,
                          const std::map<std::string, bool>& globalFlags,
                          int32_t playerLevel) const {
    // Check player level
    if (playerLevel < requiredPlayerLevel) {
        return false;
    }

    // Check prerequisite campaigns
    for (const auto& prereq : prerequisiteCampaigns) {
        auto it = std::find_if(allCampaigns.begin(), allCampaigns.end(),
            [&prereq](const Campaign& c) { return c.id == prereq; });

        if (it == allCampaigns.end() || !it->IsComplete()) {
            return false;
        }
    }

    // Check required global flags
    for (const auto& [flagName, requiredValue] : requiredGlobalFlags) {
        auto it = globalFlags.find(flagName);
        if (it == globalFlags.end() || it->second != requiredValue) {
            return false;
        }
    }

    return true;
}

int32_t Campaign::GetTotalChapters() const {
    return static_cast<int32_t>(chapters.size());
}

int32_t Campaign::GetCompletedChapters() const {
    return static_cast<int32_t>(std::count_if(chapters.begin(), chapters.end(),
        [](const std::unique_ptr<Chapter>& c) {
            return c->state == ChapterState::Completed;
        }));
}

int32_t Campaign::GetTotalMissions() const {
    int32_t total = 0;
    for (const auto& chapter : chapters) {
        total += chapter->GetTotalMissionCount();
    }
    return total;
}

int32_t Campaign::GetCompletedMissions() const {
    int32_t completed = 0;
    for (const auto& chapter : chapters) {
        completed += chapter->GetCompletedMissionCount();
    }
    return completed;
}

float Campaign::GetCompletionPercentage() const {
    int32_t total = GetTotalMissions();
    if (total == 0) return 0.0f;
    return static_cast<float>(GetCompletedMissions()) / static_cast<float>(total);
}

void Campaign::UpdateStatistics() {
    statistics.totalMissionsCompleted = GetCompletedMissions();
    statistics.totalScore = 0;
    statistics.totalObjectivesCompleted = 0;

    for (const auto& chapter : chapters) {
        statistics.totalScore += chapter->progress.totalScore;
        for (const auto& mission : chapter->missions) {
            statistics.totalObjectivesCompleted += mission->statistics.objectivesCompleted;
            statistics.unitsCreated += mission->statistics.unitsCreated;
            statistics.unitsLost += mission->statistics.unitsLost;
            statistics.enemiesDefeated += mission->statistics.enemiesKilled;
            statistics.buildingsBuilt += mission->statistics.buildingsBuilt;
            statistics.resourcesGathered += mission->statistics.resourcesGathered;

            // Track fastest/highest
            if (mission->state == MissionState::Completed) {
                if (statistics.fastestMission.empty() ||
                    mission->statistics.completionTime < statistics.fastestMissionTime) {
                    statistics.fastestMission = mission->id;
                    statistics.fastestMissionTime = mission->statistics.completionTime;
                }

                if (mission->statistics.score > statistics.highestScore) {
                    statistics.highestScoreMission = mission->id;
                    statistics.highestScore = mission->statistics.score;
                }
            }
        }
    }
}

void Campaign::AddMissionStatistics(const MissionStatistics& missionStats) {
    statistics.totalMissionsCompleted++;
    statistics.totalScore += missionStats.score;
    statistics.totalObjectivesCompleted += missionStats.objectivesCompleted;
    statistics.unitsCreated += missionStats.unitsCreated;
    statistics.unitsLost += missionStats.unitsLost;
    statistics.enemiesDefeated += missionStats.enemiesKilled;
    statistics.buildingsBuilt += missionStats.buildingsBuilt;
    statistics.resourcesGathered += missionStats.resourcesGathered;
}

void Campaign::UpdateChapterUnlocks() {
    std::vector<Chapter> chapterRefs;
    for (const auto& chapter : chapters) {
        // Create temporary references for unlock checking
        Chapter tempChapter;
        tempChapter.id = chapter->id;
        tempChapter.state = chapter->state;
        chapterRefs.push_back(tempChapter);
    }

    for (auto& chapter : chapters) {
        if (chapter->IsLocked() && chapter->CanUnlock(chapterRefs, flags)) {
            chapter->Unlock();
        }
    }
}

void Campaign::CheckCampaignComplete() {
    if (IsComplete() && state != CampaignState::Completed) {
        Complete();
    }
}

std::string Campaign::Serialize() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":\"" << id << "\",";
    oss << "\"race\":\"" << raceId << "\",";
    oss << "\"title\":\"" << title << "\",";
    oss << "\"state\":" << static_cast<int>(state) << ",";
    oss << "\"difficulty\":" << static_cast<int>(difficulty) << ",";
    oss << "\"currentChapter\":" << currentChapter << ",";
    oss << "\"currentMission\":" << currentMission << ",";
    oss << "\"chapters\":[";
    for (size_t i = 0; i < chapters.size(); ++i) {
        if (i > 0) oss << ",";
        oss << chapters[i]->Serialize();
    }
    oss << "],";
    oss << "\"flags\":{";
    bool first = true;
    for (const auto& [name, value] : flags) {
        if (!first) oss << ",";
        oss << "\"" << name << "\":" << (value ? "true" : "false");
        first = false;
    }
    oss << "}}";
    return oss.str();
}

bool Campaign::Deserialize(const std::string& jsonStr) {
    auto parsed = Nova::Json::TryParse(jsonStr);
    if (!parsed) {
        return false;
    }

    const auto& json = *parsed;

    // Parse identification
    id = Nova::Json::Get<std::string>(json, "id", "");
    raceId = Nova::Json::Get<std::string>(json, "race", "");
    title = Nova::Json::Get<std::string>(json, "title", "");
    subtitle = Nova::Json::Get<std::string>(json, "subtitle", "");
    description = Nova::Json::Get<std::string>(json, "description", "");

    // Parse state
    state = static_cast<CampaignState>(Nova::Json::Get<int>(json, "state", 0));
    difficulty = static_cast<CampaignDifficulty>(Nova::Json::Get<int>(json, "difficulty", 1));
    currentChapter = Nova::Json::Get<int32_t>(json, "currentChapter", 0);
    currentMission = Nova::Json::Get<int32_t>(json, "currentMission", 0);

    // Parse flags
    flags.clear();
    if (json.contains("flags") && json["flags"].is_object()) {
        for (auto it = json["flags"].begin(); it != json["flags"].end(); ++it) {
            flags[it.key()] = it.value().get<bool>();
        }
    }

    // Parse chapters
    chapters.clear();
    if (json.contains("chapters") && json["chapters"].is_array()) {
        for (const auto& chapterJson : json["chapters"]) {
            auto chapter = std::make_unique<Chapter>();
            chapter->Deserialize(chapterJson.dump());
            chapters.push_back(std::move(chapter));
        }
    }

    return true;
}

std::string Campaign::SerializeProgress() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"campaignId\":\"" << id << "\",";
    oss << "\"state\":" << static_cast<int>(state) << ",";
    oss << "\"difficulty\":" << static_cast<int>(difficulty) << ",";
    oss << "\"currentChapter\":" << currentChapter << ",";
    oss << "\"currentMission\":" << currentMission << ",";
    oss << "\"totalPlayTime\":" << statistics.totalPlayTime << ",";
    oss << "\"totalScore\":" << statistics.totalScore << ",";
    oss << "\"flags\":{";
    bool first = true;
    for (const auto& [name, value] : flags) {
        if (!first) oss << ",";
        oss << "\"" << name << "\":" << (value ? "true" : "false");
        first = false;
    }
    oss << "},";
    oss << "\"chapterProgress\":[";
    for (size_t i = 0; i < chapters.size(); ++i) {
        if (i > 0) oss << ",";
        oss << chapters[i]->SerializeProgress();
    }
    oss << "]}";
    return oss.str();
}

bool Campaign::DeserializeProgress(const std::string& jsonStr) {
    auto parsed = Nova::Json::TryParse(jsonStr);
    if (!parsed) {
        return false;
    }

    const auto& json = *parsed;

    // Parse progress state
    state = static_cast<CampaignState>(Nova::Json::Get<int>(json, "state", 0));
    difficulty = static_cast<CampaignDifficulty>(Nova::Json::Get<int>(json, "difficulty", 1));
    currentChapter = Nova::Json::Get<int32_t>(json, "currentChapter", 0);
    currentMission = Nova::Json::Get<int32_t>(json, "currentMission", 0);

    // Parse statistics
    statistics.totalPlayTime = Nova::Json::Get<float>(json, "totalPlayTime", 0.0f);
    statistics.totalScore = Nova::Json::Get<int32_t>(json, "totalScore", 0);

    // Parse flags
    flags.clear();
    if (json.contains("flags") && json["flags"].is_object()) {
        for (auto it = json["flags"].begin(); it != json["flags"].end(); ++it) {
            flags[it.key()] = it.value().get<bool>();
        }
    }

    // Parse chapter progress
    if (json.contains("chapterProgress") && json["chapterProgress"].is_array()) {
        size_t index = 0;
        for (const auto& chapterProgressJson : json["chapterProgress"]) {
            if (index < chapters.size()) {
                chapters[index]->DeserializeProgress(chapterProgressJson.dump());
            }
            ++index;
        }
    }

    return true;
}

bool Campaign::SaveProgress(const std::string& savePath) const {
    std::ofstream file(savePath);
    if (!file.is_open()) return false;
    file << SerializeProgress();
    return true;
}

bool Campaign::LoadProgress(const std::string& savePath) {
    std::ifstream file(savePath);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    return DeserializeProgress(buffer.str());
}

// CampaignFactory implementations

std::unique_ptr<Campaign> CampaignFactory::CreateFromJson(const std::string& jsonPath) {
    auto jsonOpt = Nova::Json::TryParseFile(jsonPath);
    if (!jsonOpt) {
        return nullptr;
    }

    auto campaign = std::make_unique<Campaign>();
    if (!campaign->Deserialize(jsonOpt->dump())) {
        return nullptr;
    }

    return campaign;
}

std::unique_ptr<Campaign> CampaignFactory::CreateFromConfig(const std::string& configDir) {
    // Look for campaign.json in the config directory
    std::string campaignJsonPath = configDir + "/campaign.json";
    if (!std::filesystem::exists(campaignJsonPath)) {
        // Try without trailing slash
        campaignJsonPath = configDir + "campaign.json";
        if (!std::filesystem::exists(campaignJsonPath)) {
            return nullptr;
        }
    }

    auto campaign = CreateFromJson(campaignJsonPath);
    if (!campaign) {
        return nullptr;
    }

    // Load chapters from chapters subdirectory
    std::string chaptersDir = configDir + "/chapters";
    if (std::filesystem::exists(chaptersDir)) {
        LoadChapters(*campaign, chaptersDir);
    }

    // Load cinematics from cinematics subdirectory
    std::string cinematicsDir = configDir + "/cinematics";
    if (std::filesystem::exists(cinematicsDir)) {
        LoadCinematics(*campaign, cinematicsDir);
    }

    return campaign;
}

std::unique_ptr<Campaign> CampaignFactory::CreateForRace(RaceType race) {
    auto campaign = std::make_unique<Campaign>();
    campaign->race = race;
    campaign->raceId = RaceTypeToString(race);
    return campaign;
}

void CampaignFactory::LoadChapters(Campaign& campaign, const std::string& chaptersDir) {
    if (!std::filesystem::exists(chaptersDir)) {
        return;
    }

    // Collect all chapter JSON files
    std::vector<std::string> chapterFiles;
    for (const auto& entry : std::filesystem::directory_iterator(chaptersDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            chapterFiles.push_back(entry.path().string());
        }
    }

    // Sort by filename to maintain order (chapter_01.json, chapter_02.json, etc.)
    std::sort(chapterFiles.begin(), chapterFiles.end());

    // Load each chapter
    for (const auto& chapterFile : chapterFiles) {
        auto chapter = ChapterFactory::CreateFromJson(chapterFile);
        if (chapter) {
            campaign.AddChapter(std::move(chapter));
        }
    }
}

void CampaignFactory::LoadCinematics(Campaign& campaign, const std::string& cinematicsDir) {
    if (!std::filesystem::exists(cinematicsDir)) {
        return;
    }

    // Load all cinematic JSON files from the directory
    for (const auto& entry : std::filesystem::directory_iterator(cinematicsDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            auto cinematic = CinematicFactory::CreateFromJson(entry.path().string());
            if (cinematic) {
                campaign.AddCinematic(std::move(cinematic));
            }
        }
    }
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
