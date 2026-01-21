#include "Chapter.hpp"
#include "engine/core/json_wrapper.hpp"
#include <algorithm>
#include <sstream>
#include <filesystem>

namespace Vehement {
namespace RTS {
namespace Campaign {

Chapter::Chapter(const std::string& chapterId) : id(chapterId) {}

void Chapter::Initialize() {
    currentMissionIndex = 0;
    progress = ChapterProgress();
    progress.missionsTotal = static_cast<int32_t>(missions.size());

    // Initialize all missions
    for (auto& mission : missions) {
        mission->Reset();
    }

    // Make first mission available if sequential
    if (requireSequentialCompletion && !missions.empty()) {
        missions[0]->state = MissionState::Available;
    } else {
        // All missions available
        for (auto& mission : missions) {
            mission->state = MissionState::Available;
        }
    }
}

void Chapter::Unlock() {
    if (state == ChapterState::Locked) {
        state = ChapterState::Available;
    }
}

void Chapter::Start() {
    if (state == ChapterState::Available) {
        state = ChapterState::InProgress;
        Initialize();
    }
}

void Chapter::Complete() {
    state = ChapterState::Completed;
    UpdateProgress();
    CalculateTotalScore();
}

void Chapter::Reset() {
    state = ChapterState::Available;
    progress = ChapterProgress();
    progress.missionsTotal = static_cast<int32_t>(missions.size());
    currentMissionIndex = 0;

    for (auto& mission : missions) {
        mission->Reset();
    }
}

Mission* Chapter::GetMission(const std::string& missionId) {
    auto it = std::find_if(missions.begin(), missions.end(),
        [&missionId](const std::unique_ptr<Mission>& m) { return m->id == missionId; });
    return (it != missions.end()) ? it->get() : nullptr;
}

const Mission* Chapter::GetMission(const std::string& missionId) const {
    auto it = std::find_if(missions.begin(), missions.end(),
        [&missionId](const std::unique_ptr<Mission>& m) { return m->id == missionId; });
    return (it != missions.end()) ? it->get() : nullptr;
}

Mission* Chapter::GetMissionByIndex(size_t index) {
    return (index < missions.size()) ? missions[index].get() : nullptr;
}

const Mission* Chapter::GetMissionByIndex(size_t index) const {
    return (index < missions.size()) ? missions[index].get() : nullptr;
}

Mission* Chapter::GetCurrentMission() {
    return GetMissionByIndex(currentMissionIndex);
}

Mission* Chapter::GetNextAvailableMission() {
    for (auto& mission : missions) {
        if (mission->state == MissionState::Available) {
            return mission.get();
        }
    }
    return nullptr;
}

void Chapter::AddMission(std::unique_ptr<Mission> mission) {
    missionOrder.push_back(mission->id);
    missions.push_back(std::move(mission));
    progress.missionsTotal = static_cast<int32_t>(missions.size());
}

void Chapter::RemoveMission(const std::string& missionId) {
    auto it = std::find_if(missions.begin(), missions.end(),
        [&missionId](const std::unique_ptr<Mission>& m) { return m->id == missionId; });

    if (it != missions.end()) {
        missions.erase(it);
    }

    auto orderIt = std::find(missionOrder.begin(), missionOrder.end(), missionId);
    if (orderIt != missionOrder.end()) {
        missionOrder.erase(orderIt);
    }

    progress.missionsTotal = static_cast<int32_t>(missions.size());
}

bool Chapter::CanUnlock(const std::vector<Chapter>& allChapters,
                         const std::map<std::string, bool>& flags) const {
    // Check previous chapters
    for (const auto& prevChapterId : requirements.previousChapters) {
        auto it = std::find_if(allChapters.begin(), allChapters.end(),
            [&prevChapterId](const Chapter& c) { return c.id == prevChapterId; });

        if (it == allChapters.end() || it->state != ChapterState::Completed) {
            return false;
        }
    }

    // Check required flags
    for (const auto& [flagName, requiredValue] : requirements.requiredFlags) {
        auto it = flags.find(flagName);
        if (it == flags.end() || it->second != requiredValue) {
            return false;
        }
    }

    // Check minimum missions completed
    if (requirements.minimumMissionsCompleted > 0) {
        int32_t totalCompleted = 0;
        for (const auto& chapter : allChapters) {
            totalCompleted += chapter.GetCompletedMissionCount();
        }
        if (totalCompleted < requirements.minimumMissionsCompleted) {
            return false;
        }
    }

    return true;
}

bool Chapter::IsComplete() const {
    int32_t required = requiredMissionsToComplete;
    if (required < 0) {
        required = static_cast<int32_t>(missions.size());
    }

    int32_t completed = GetCompletedMissionCount();
    return completed >= required;
}

int32_t Chapter::GetCompletedMissionCount() const {
    return static_cast<int32_t>(std::count_if(missions.begin(), missions.end(),
        [](const std::unique_ptr<Mission>& m) {
            return m->state == MissionState::Completed;
        }));
}

int32_t Chapter::GetTotalMissionCount() const {
    return static_cast<int32_t>(missions.size());
}

float Chapter::GetCompletionPercentage() const {
    if (missions.empty()) return 0.0f;
    return static_cast<float>(GetCompletedMissionCount()) /
           static_cast<float>(missions.size());
}

void Chapter::UpdateProgress() {
    progress.missionsCompleted = GetCompletedMissionCount();
    progress.missionsTotal = GetTotalMissionCount();
    progress.completionPercentage = GetCompletionPercentage();

    // Update time spent
    progress.timeSpent = 0.0f;
    for (const auto& mission : missions) {
        if (mission->state == MissionState::Completed) {
            progress.timeSpent += mission->statistics.completionTime;
        }
    }

    UpdateMissionAvailability();

    // Check if chapter is complete
    if (IsComplete() && state == ChapterState::InProgress) {
        Complete();
    }
}

void Chapter::CalculateTotalScore() {
    progress.totalScore = 0;
    for (const auto& mission : missions) {
        if (mission->state == MissionState::Completed) {
            progress.totalScore += mission->bestStatistics.score;
        }
    }

    // Find best grade
    char bestGrade = 'F';
    for (const auto& mission : missions) {
        if (!mission->bestStatistics.grade.empty()) {
            char grade = mission->bestStatistics.grade[0];
            if (grade < bestGrade || grade == 'S') {
                bestGrade = grade;
            }
        }
    }
    progress.bestMissionGrade = std::string(1, bestGrade);
}

void Chapter::UpdateMissionAvailability() {
    if (!requireSequentialCompletion) return;

    bool previousCompleted = true;
    for (size_t i = 0; i < missions.size(); ++i) {
        auto& mission = missions[i];

        if (mission->state == MissionState::Locked && previousCompleted) {
            mission->state = MissionState::Available;
        }

        if (mission->state != MissionState::Completed) {
            previousCompleted = false;
        }
    }
}

std::string Chapter::Serialize() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":\"" << id << "\",";
    oss << "\"title\":\"" << title << "\",";
    oss << "\"state\":" << static_cast<int>(state) << ",";
    oss << "\"missions\":[";
    for (size_t i = 0; i < missions.size(); ++i) {
        if (i > 0) oss << ",";
        oss << missions[i]->Serialize();
    }
    oss << "]}";
    return oss.str();
}

bool Chapter::Deserialize(const std::string& jsonStr) {
    using namespace Nova::Json;

    auto parsed = TryParse(jsonStr);
    if (!parsed) {
        return false;
    }

    const JsonValue& json = *parsed;

    // Parse identification
    id = Get<std::string>(json, "id", id);
    title = Get<std::string>(json, "title", title);
    subtitle = Get<std::string>(json, "subtitle", subtitle);
    description = Get<std::string>(json, "description", description);
    chapterNumber = Get<int32_t>(json, "chapterNumber", chapterNumber);

    // Parse state
    if (json.contains("state")) {
        state = static_cast<ChapterState>(json["state"].get<int>());
    }

    // Parse story
    if (json.contains("story") && json["story"].is_object()) {
        const auto& s = json["story"];
        story.synopsis = Get<std::string>(s, "synopsis", "");
        story.fullDescription = Get<std::string>(s, "fullDescription", "");
        story.openingNarration = Get<std::string>(s, "openingNarration", "");
        story.closingNarration = Get<std::string>(s, "closingNarration", "");
        story.timeframe = Get<std::string>(s, "timeframe", "");
        story.location = Get<std::string>(s, "location", "");

        if (s.contains("keyCharacters") && s["keyCharacters"].is_array()) {
            story.keyCharacters.clear();
            for (const auto& c : s["keyCharacters"]) {
                story.keyCharacters.push_back(c.get<std::string>());
            }
        }
        if (s.contains("previousEvents") && s["previousEvents"].is_array()) {
            story.previousEvents.clear();
            for (const auto& e : s["previousEvents"]) {
                story.previousEvents.push_back(e.get<std::string>());
            }
        }
    }

    // Parse cinematics
    introCinematic = Get<std::string>(json, "introCinematic", introCinematic);
    outroCinematic = Get<std::string>(json, "outroCinematic", outroCinematic);
    if (json.contains("interMissionCinematics") && json["interMissionCinematics"].is_array()) {
        interMissionCinematics.clear();
        for (const auto& c : json["interMissionCinematics"]) {
            interMissionCinematics.push_back(c.get<std::string>());
        }
    }

    // Parse requirements
    if (json.contains("requirements") && json["requirements"].is_object()) {
        const auto& r = json["requirements"];
        if (r.contains("previousChapters") && r["previousChapters"].is_array()) {
            requirements.previousChapters.clear();
            for (const auto& c : r["previousChapters"]) {
                requirements.previousChapters.push_back(c.get<std::string>());
            }
        }
        if (r.contains("requiredFlags") && r["requiredFlags"].is_object()) {
            requirements.requiredFlags.clear();
            for (auto it = r["requiredFlags"].begin(); it != r["requiredFlags"].end(); ++it) {
                requirements.requiredFlags[it.key()] = it.value().get<bool>();
            }
        }
        requirements.minimumMissionsCompleted = Get<int32_t>(r, "minimumMissionsCompleted", 0);
        requirements.minimumExperience = Get<int32_t>(r, "minimumExperience", 0);
        requirements.unlockCinematic = Get<std::string>(r, "unlockCinematic", "");
    }

    // Parse rewards
    if (json.contains("rewards") && json["rewards"].is_object()) {
        const auto& rw = json["rewards"];
        rewards.experienceBonus = Get<int32_t>(rw, "experienceBonus", 500);
        rewards.goldBonus = Get<int32_t>(rw, "goldBonus", 1000);
        rewards.achievement = Get<std::string>(rw, "achievement", "");
        rewards.nextChapterUnlock = Get<std::string>(rw, "nextChapterUnlock", "");

        auto parseStringArray = [](const JsonValue& arr, std::vector<std::string>& out) {
            out.clear();
            for (const auto& item : arr) {
                out.push_back(item.get<std::string>());
            }
        };

        if (rw.contains("unlockedHeroes") && rw["unlockedHeroes"].is_array()) {
            parseStringArray(rw["unlockedHeroes"], rewards.unlockedHeroes);
        }
        if (rw.contains("unlockedUnits") && rw["unlockedUnits"].is_array()) {
            parseStringArray(rw["unlockedUnits"], rewards.unlockedUnits);
        }
        if (rw.contains("unlockedBuildings") && rw["unlockedBuildings"].is_array()) {
            parseStringArray(rw["unlockedBuildings"], rewards.unlockedBuildings);
        }
        if (rw.contains("unlockedAbilities") && rw["unlockedAbilities"].is_array()) {
            parseStringArray(rw["unlockedAbilities"], rewards.unlockedAbilities);
        }
        if (rw.contains("unlockedTech") && rw["unlockedTech"].is_array()) {
            parseStringArray(rw["unlockedTech"], rewards.unlockedTech);
        }
        if (rw.contains("items") && rw["items"].is_array()) {
            parseStringArray(rw["items"], rewards.items);
        }
        if (rw.contains("storyFlags") && rw["storyFlags"].is_object()) {
            rewards.storyFlags.clear();
            for (auto it = rw["storyFlags"].begin(); it != rw["storyFlags"].end(); ++it) {
                rewards.storyFlags[it.key()] = it.value().get<bool>();
            }
        }
    }

    // Parse UI settings
    thumbnailImage = Get<std::string>(json, "thumbnailImage", thumbnailImage);
    headerImage = Get<std::string>(json, "headerImage", headerImage);
    backgroundImage = Get<std::string>(json, "backgroundImage", backgroundImage);
    iconUnlocked = Get<std::string>(json, "iconUnlocked", iconUnlocked);
    iconLocked = Get<std::string>(json, "iconLocked", iconLocked);
    themeColor = Get<std::string>(json, "themeColor", themeColor);

    // Parse audio
    menuMusic = Get<std::string>(json, "menuMusic", menuMusic);
    briefingMusic = Get<std::string>(json, "briefingMusic", briefingMusic);

    // Parse mission settings
    requiredMissionsToComplete = Get<int32_t>(json, "requiredMissionsToComplete", -1);
    allowMissionSkip = Get<bool>(json, "allowMissionSkip", false);
    lockCompletedMissions = Get<bool>(json, "lockCompletedMissions", false);
    requireSequentialCompletion = Get<bool>(json, "requireSequentialCompletion", true);

    // Parse missions (if embedded)
    if (json.contains("missions") && json["missions"].is_array()) {
        missions.clear();
        missionOrder.clear();
        for (const auto& m : json["missions"]) {
            auto mission = std::make_unique<Mission>();
            mission->Deserialize(Stringify(m));
            missionOrder.push_back(mission->id);
            missions.push_back(std::move(mission));
        }
        progress.missionsTotal = static_cast<int32_t>(missions.size());
    }

    return true;
}

std::string Chapter::SerializeProgress() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"chapterId\":\"" << id << "\",";
    oss << "\"state\":" << static_cast<int>(state) << ",";
    oss << "\"missionsCompleted\":" << progress.missionsCompleted << ",";
    oss << "\"totalScore\":" << progress.totalScore << ",";
    oss << "\"timeSpent\":" << progress.timeSpent << ",";
    oss << "\"missionProgress\":[";
    for (size_t i = 0; i < missions.size(); ++i) {
        if (i > 0) oss << ",";
        oss << missions[i]->SerializeProgress();
    }
    oss << "]}";
    return oss.str();
}

bool Chapter::DeserializeProgress(const std::string& jsonStr) {
    using namespace Nova::Json;

    auto parsed = TryParse(jsonStr);
    if (!parsed) {
        return false;
    }

    const JsonValue& json = *parsed;

    // Verify chapter ID matches
    std::string loadedId = Get<std::string>(json, "chapterId", "");
    if (!loadedId.empty() && loadedId != id) {
        return false; // Progress data doesn't match this chapter
    }

    // Parse state
    if (json.contains("state")) {
        state = static_cast<ChapterState>(json["state"].get<int>());
    }

    // Parse progress data
    progress.missionsCompleted = Get<int32_t>(json, "missionsCompleted", 0);
    progress.totalScore = Get<int32_t>(json, "totalScore", 0);
    progress.timeSpent = Get<float>(json, "timeSpent", 0.0f);
    progress.secretsFound = Get<int32_t>(json, "secretsFound", 0);
    progress.secretsTotal = Get<int32_t>(json, "secretsTotal", 0);
    progress.completionPercentage = Get<float>(json, "completionPercentage", 0.0f);
    progress.bestMissionGrade = Get<std::string>(json, "bestMissionGrade", "");

    // Parse mission progress
    if (json.contains("missionProgress") && json["missionProgress"].is_array()) {
        const auto& missionProgressArr = json["missionProgress"];

        for (const auto& mp : missionProgressArr) {
            std::string missionId = Get<std::string>(mp, "missionId", "");
            if (missionId.empty()) continue;

            // Find matching mission
            Mission* mission = GetMission(missionId);
            if (mission) {
                mission->DeserializeProgress(Stringify(mp));
            }
        }
    }

    // Update mission count
    progress.missionsTotal = static_cast<int32_t>(missions.size());

    return true;
}

// ChapterFactory implementations

std::unique_ptr<Chapter> ChapterFactory::CreateFromJson(const std::string& jsonPath) {
    using namespace Nova::Json;

    auto jsonOpt = TryParseFile(jsonPath);
    if (!jsonOpt) {
        return nullptr;
    }

    auto chapter = std::make_unique<Chapter>();
    if (!chapter->Deserialize(Stringify(*jsonOpt))) {
        return nullptr;
    }

    return chapter;
}

std::unique_ptr<Chapter> ChapterFactory::CreateFromConfig(const std::string& configDir) {
    namespace fs = std::filesystem;
    using namespace Nova::Json;

    // Look for chapter.json in the config directory
    fs::path configPath(configDir);
    fs::path chapterFile = configPath / "chapter.json";

    if (!fs::exists(chapterFile)) {
        return nullptr;
    }

    auto chapter = CreateFromJson(chapterFile.string());
    if (!chapter) {
        return nullptr;
    }

    // Load missions from missions subdirectory if it exists
    fs::path missionsDir = configPath / "missions";
    if (fs::exists(missionsDir) && fs::is_directory(missionsDir)) {
        LoadMissions(*chapter, missionsDir.string());
    }

    return chapter;
}

void ChapterFactory::LoadMissions(Chapter& chapter, const std::string& missionsDir) {
    namespace fs = std::filesystem;
    using namespace Nova::Json;

    fs::path dirPath(missionsDir);
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return;
    }

    // Collect all JSON files in the missions directory
    std::vector<fs::path> missionFiles;
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            missionFiles.push_back(entry.path());
        }
    }

    // Sort by filename to maintain consistent order
    std::sort(missionFiles.begin(), missionFiles.end());

    // Load each mission
    for (const auto& missionFile : missionFiles) {
        auto jsonOpt = TryParseFile(missionFile.string());
        if (!jsonOpt) {
            continue;
        }

        auto mission = std::make_unique<Mission>();
        if (mission->Deserialize(Stringify(*jsonOpt))) {
            chapter.AddMission(std::move(mission));
        }
    }
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
