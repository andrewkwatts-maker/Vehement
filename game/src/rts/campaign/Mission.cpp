#include "Mission.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Vehement {
namespace RTS {
namespace Campaign {

Mission::Mission(const std::string& missionId) : id(missionId) {
    // Set up default difficulty modifiers
    difficultySettings[MissionDifficulty::Easy] = {
        .playerDamageMultiplier = 1.25f,
        .enemyDamageMultiplier = 0.75f,
        .playerResourceMultiplier = 1.5f,
        .enemyResourceMultiplier = 0.75f,
        .timeLimitMultiplier = 1.5f,
        .experienceMultiplier = 0.75f,
        .extraEnemyUnits = -2,
        .showHints = true,
        .enableAutoSave = true
    };

    difficultySettings[MissionDifficulty::Normal] = {
        .playerDamageMultiplier = 1.0f,
        .enemyDamageMultiplier = 1.0f,
        .playerResourceMultiplier = 1.0f,
        .enemyResourceMultiplier = 1.0f,
        .timeLimitMultiplier = 1.0f,
        .experienceMultiplier = 1.0f,
        .extraEnemyUnits = 0,
        .showHints = true,
        .enableAutoSave = true
    };

    difficultySettings[MissionDifficulty::Hard] = {
        .playerDamageMultiplier = 0.85f,
        .enemyDamageMultiplier = 1.25f,
        .playerResourceMultiplier = 0.75f,
        .enemyResourceMultiplier = 1.25f,
        .timeLimitMultiplier = 0.85f,
        .experienceMultiplier = 1.25f,
        .extraEnemyUnits = 3,
        .showHints = false,
        .enableAutoSave = true
    };

    difficultySettings[MissionDifficulty::Brutal] = {
        .playerDamageMultiplier = 0.7f,
        .enemyDamageMultiplier = 1.5f,
        .playerResourceMultiplier = 0.5f,
        .enemyResourceMultiplier = 1.5f,
        .timeLimitMultiplier = 0.7f,
        .experienceMultiplier = 1.5f,
        .extraEnemyUnits = 5,
        .showHints = false,
        .enableAutoSave = false
    };
}

void Mission::Initialize() {
    // Reset all objectives
    for (auto& objective : objectives) {
        objective.Reset();
    }

    // Reset statistics
    statistics = MissionStatistics();
    statistics.difficulty = currentDifficulty;

    // Activate initial objectives (those with no prerequisites)
    for (auto& objective : objectives) {
        if (objective.prerequisites.empty() &&
            objective.priority != ObjectivePriority::Hidden) {
            objective.Activate();
        }
    }
}

void Mission::Start(MissionDifficulty difficulty) {
    currentDifficulty = difficulty;
    state = MissionState::InProgress;
    Initialize();
}

void Mission::Update(float deltaTime) {
    if (state != MissionState::InProgress) return;

    // Update all active objectives
    for (auto& objective : objectives) {
        if (objective.IsActive()) {
            objective.Update(deltaTime);
        }
    }

    // Check objective dependencies
    CheckObjectiveDependencies();

    // Update statistics
    UpdateStatistics(deltaTime);

    // Check win/lose conditions
    if (CheckVictoryCondition()) {
        Complete();
    } else if (CheckDefeatCondition()) {
        Fail();
    }
}

void Mission::Complete() {
    state = MissionState::Completed;
    CalculateScore();
    CalculateGrade();

    // Update best statistics if this run was better
    if (statistics.score > bestStatistics.score) {
        bestStatistics = statistics;
    }
}

void Mission::Fail() {
    state = MissionState::Failed;
}

void Mission::Reset() {
    state = MissionState::Available;
    statistics = MissionStatistics();
    for (auto& objective : objectives) {
        objective.Reset();
    }
}

Objective* Mission::GetObjective(const std::string& objectiveId) {
    auto it = std::find_if(objectives.begin(), objectives.end(),
        [&objectiveId](const Objective& obj) { return obj.id == objectiveId; });
    return (it != objectives.end()) ? &(*it) : nullptr;
}

const Objective* Mission::GetObjective(const std::string& objectiveId) const {
    auto it = std::find_if(objectives.begin(), objectives.end(),
        [&objectiveId](const Objective& obj) { return obj.id == objectiveId; });
    return (it != objectives.end()) ? &(*it) : nullptr;
}

void Mission::ActivateObjective(const std::string& objectiveId) {
    if (auto* obj = GetObjective(objectiveId)) {
        obj->Activate();
    }
}

void Mission::CompleteObjective(const std::string& objectiveId) {
    if (auto* obj = GetObjective(objectiveId)) {
        obj->Complete();
        statistics.objectivesCompleted++;
        if (obj->priority == ObjectivePriority::Bonus) {
            statistics.bonusObjectivesCompleted++;
        }
    }
}

void Mission::FailObjective(const std::string& objectiveId) {
    if (auto* obj = GetObjective(objectiveId)) {
        obj->Fail();
        statistics.objectivesFailed++;
    }
}

void Mission::UpdateObjectiveProgress(const std::string& objectiveId, int32_t delta) {
    if (auto* obj = GetObjective(objectiveId)) {
        obj->UpdateProgress(delta);
    }
}

bool Mission::CheckVictoryCondition() const {
    switch (victoryCondition) {
        case VictoryCondition::AllPrimaryObjectives:
            return AreAllPrimaryObjectivesComplete();

        case VictoryCondition::AnyPrimaryObjective:
            for (const auto& objId : primaryObjectiveIds) {
                if (auto* obj = GetObjective(objId)) {
                    if (obj->IsCompleted()) return true;
                }
            }
            return false;

        case VictoryCondition::SurvivalTime:
            return timeLimit > 0 && statistics.completionTime >= timeLimit;

        case VictoryCondition::EliminateAll:
            // TODO: Check if all enemies eliminated
            return false;

        case VictoryCondition::Custom:
            // Handled by script
            return false;
    }
    return false;
}

bool Mission::CheckDefeatCondition() const {
    switch (defeatCondition) {
        case DefeatCondition::ObjectiveFailed:
            return AnyPrimaryObjectiveFailed();

        case DefeatCondition::TimeExpired:
            return timeLimit > 0 && statistics.completionTime > timeLimit;

        case DefeatCondition::HeroKilled:
        case DefeatCondition::BaseDestroyed:
        case DefeatCondition::AllUnitsLost:
        case DefeatCondition::Custom:
            // TODO: Implement these checks
            return false;
    }
    return false;
}

bool Mission::AreAllPrimaryObjectivesComplete() const {
    for (const auto& objId : primaryObjectiveIds) {
        if (auto* obj = GetObjective(objId)) {
            if (!obj->IsCompleted()) return false;
        }
    }
    return !primaryObjectiveIds.empty();
}

bool Mission::AnyPrimaryObjectiveFailed() const {
    for (const auto& objId : primaryObjectiveIds) {
        if (auto* obj = GetObjective(objId)) {
            if (obj->IsFailed()) return true;
        }
    }
    return false;
}

int32_t Mission::GetCompletedObjectiveCount() const {
    return static_cast<int32_t>(std::count_if(objectives.begin(), objectives.end(),
        [](const Objective& obj) { return obj.IsCompleted(); }));
}

int32_t Mission::GetTotalObjectiveCount() const {
    return static_cast<int32_t>(std::count_if(objectives.begin(), objectives.end(),
        [](const Objective& obj) { return obj.priority != ObjectivePriority::Hidden; }));
}

float Mission::GetCompletionPercentage() const {
    int32_t total = GetTotalObjectiveCount();
    if (total == 0) return 0.0f;
    return static_cast<float>(GetCompletedObjectiveCount()) / static_cast<float>(total);
}

DifficultyModifiers Mission::GetDifficultyModifiers() const {
    auto it = difficultySettings.find(currentDifficulty);
    if (it != difficultySettings.end()) {
        return it->second;
    }
    return DifficultyModifiers();
}

MissionResources Mission::GetAdjustedResources() const {
    auto modifiers = GetDifficultyModifiers();
    MissionResources adjusted = startingResources;
    adjusted.gold = static_cast<int32_t>(adjusted.gold * modifiers.playerResourceMultiplier);
    adjusted.wood = static_cast<int32_t>(adjusted.wood * modifiers.playerResourceMultiplier);
    adjusted.stone = static_cast<int32_t>(adjusted.stone * modifiers.playerResourceMultiplier);
    adjusted.metal = static_cast<int32_t>(adjusted.metal * modifiers.playerResourceMultiplier);
    adjusted.food = static_cast<int32_t>(adjusted.food * modifiers.playerResourceMultiplier);
    return adjusted;
}

void Mission::CalculateScore() {
    // Base score from objectives
    statistics.score = statistics.objectivesCompleted * 100;
    statistics.score += statistics.bonusObjectivesCompleted * 50;

    // Time bonus
    if (parTime > 0 && statistics.completionTime < parTime) {
        float timeBonus = (parTime - statistics.completionTime) / parTime;
        statistics.score += static_cast<int32_t>(timeBonus * 500);
    }

    // Efficiency bonuses
    if (statistics.unitsLost == 0) {
        statistics.score += 200; // No casualties bonus
    }

    // Kill ratio
    if (statistics.unitsCreated > 0) {
        float efficiency = static_cast<float>(statistics.enemiesKilled) /
                          static_cast<float>(statistics.unitsCreated);
        statistics.score += static_cast<int32_t>(efficiency * 100);
    }

    // Apply difficulty multiplier
    auto modifiers = GetDifficultyModifiers();
    statistics.score = static_cast<int32_t>(statistics.score * modifiers.experienceMultiplier);
}

void Mission::CalculateGrade() {
    // Grade based on percentage of max possible score
    float percentage = static_cast<float>(statistics.score) / 2000.0f; // Assume 2000 is max

    if (percentage >= 0.95f) statistics.grade = "S";
    else if (percentage >= 0.85f) statistics.grade = "A";
    else if (percentage >= 0.70f) statistics.grade = "B";
    else if (percentage >= 0.55f) statistics.grade = "C";
    else if (percentage >= 0.40f) statistics.grade = "D";
    else statistics.grade = "F";
}

void Mission::CheckObjectiveDependencies() {
    for (auto& objective : objectives) {
        if (objective.CanActivate(objectives)) {
            objective.Activate();
        }
    }
}

void Mission::UpdateStatistics(float deltaTime) {
    statistics.completionTime += deltaTime;
}

std::string Mission::Serialize() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":\"" << id << "\",";
    oss << "\"state\":" << static_cast<int>(state) << ",";
    oss << "\"difficulty\":" << static_cast<int>(currentDifficulty) << ",";
    oss << "\"objectives\":[";
    for (size_t i = 0; i < objectives.size(); ++i) {
        if (i > 0) oss << ",";
        oss << objectives[i].Serialize();
    }
    oss << "]}";
    return oss.str();
}

bool Mission::Deserialize(const std::string& /*json*/) {
    // TODO: Implement JSON parsing
    return true;
}

std::string Mission::SerializeProgress() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"missionId\":\"" << id << "\",";
    oss << "\"state\":" << static_cast<int>(state) << ",";
    oss << "\"bestScore\":" << bestStatistics.score << ",";
    oss << "\"bestGrade\":\"" << bestStatistics.grade << "\",";
    oss << "\"bestTime\":" << bestStatistics.completionTime;
    oss << "}";
    return oss.str();
}

bool Mission::DeserializeProgress(const std::string& /*json*/) {
    // TODO: Implement JSON parsing
    return true;
}

// MissionFactory implementations

std::unique_ptr<Mission> MissionFactory::CreateFromJson(const std::string& /*jsonPath*/) {
    // TODO: Load and parse JSON file
    return std::make_unique<Mission>();
}

std::unique_ptr<Mission> MissionFactory::CreateFromConfig(const std::string& /*configPath*/) {
    // TODO: Load mission from config directory
    return std::make_unique<Mission>();
}

void MissionFactory::PopulateObjectives(Mission& /*mission*/, const std::string& /*objectivesJson*/) {
    // TODO: Parse objectives JSON and populate mission
}

void MissionFactory::PopulateAI(Mission& /*mission*/, const std::string& /*aiJson*/) {
    // TODO: Parse AI config JSON and populate mission
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
