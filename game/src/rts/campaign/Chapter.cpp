#include "Chapter.hpp"
#include <algorithm>
#include <sstream>

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

bool Chapter::Deserialize(const std::string& /*json*/) {
    // TODO: Implement JSON parsing
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

bool Chapter::DeserializeProgress(const std::string& /*json*/) {
    // TODO: Implement JSON parsing
    return true;
}

// ChapterFactory implementations

std::unique_ptr<Chapter> ChapterFactory::CreateFromJson(const std::string& /*jsonPath*/) {
    // TODO: Load and parse JSON file
    return std::make_unique<Chapter>();
}

std::unique_ptr<Chapter> ChapterFactory::CreateFromConfig(const std::string& /*configDir*/) {
    // TODO: Load chapter from config directory
    return std::make_unique<Chapter>();
}

void ChapterFactory::LoadMissions(Chapter& /*chapter*/, const std::string& /*missionsDir*/) {
    // TODO: Load all missions from directory
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
