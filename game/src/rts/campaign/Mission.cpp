#include "Mission.hpp"
#include "engine/core/json_wrapper.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>

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
            // Check if all enemy AI players have been defeated
            for (const auto& ai : aiPlayers) {
                if (!ai.isAlly && ai.canBeDefeated) {
                    // If any enemy AI is still active, victory not achieved
                    // This is checked via external game state - return false by default
                    // The actual elimination tracking happens through MissionManager events
                    return false;
                }
            }
            // All enemy AIs defeated (or none exist)
            return !aiPlayers.empty();

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
            // Hero death is tracked externally through MissionManager::RecordUnitKilled
            // Return false here - defeat triggered by event system when hero dies
            return false;

        case DefeatCondition::BaseDestroyed:
            // Base destruction tracked externally through MissionManager::RecordBuildingDestroyed
            // Return false here - defeat triggered by event system when main base is destroyed
            return false;

        case DefeatCondition::AllUnitsLost:
            // Check if player has lost all units - tracked through statistics
            // If units were created but all were lost, player is defeated
            return statistics.unitsCreated > 0 &&
                   (statistics.unitsCreated - statistics.unitsLost) <= 0;

        case DefeatCondition::Custom:
            // Custom defeat conditions are handled by mission scripts
            // Return false here - defeat triggered by ExecuteScript setting mission state
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

bool Mission::Deserialize(const std::string& jsonStr) {
    using namespace Nova::Json;

    auto parsed = TryParse(jsonStr);
    if (!parsed) {
        return false;
    }

    const auto& json = *parsed;

    // Parse basic fields
    id = Get<std::string>(json, "id", id);
    state = static_cast<MissionState>(Get<int>(json, "state", static_cast<int>(state)));
    currentDifficulty = static_cast<MissionDifficulty>(Get<int>(json, "difficulty", static_cast<int>(currentDifficulty)));

    // Parse objectives
    if (json.contains("objectives") && json["objectives"].is_array()) {
        objectives.clear();
        for (const auto& objJson : json["objectives"]) {
            Objective obj;
            obj.Deserialize(objJson.dump());
            objectives.push_back(std::move(obj));
        }
    }

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

bool Mission::DeserializeProgress(const std::string& jsonStr) {
    using namespace Nova::Json;

    auto parsed = TryParse(jsonStr);
    if (!parsed) {
        return false;
    }

    const auto& json = *parsed;

    // Verify mission ID matches
    std::string missionId = Get<std::string>(json, "missionId", "");
    if (!missionId.empty() && missionId != id) {
        return false; // Progress data is for a different mission
    }

    // Parse state and best statistics
    state = static_cast<MissionState>(Get<int>(json, "state", static_cast<int>(state)));
    bestStatistics.score = Get<int>(json, "bestScore", bestStatistics.score);
    bestStatistics.grade = Get<std::string>(json, "bestGrade", bestStatistics.grade);
    bestStatistics.completionTime = Get<float>(json, "bestTime", bestStatistics.completionTime);

    return true;
}

// MissionFactory implementations

std::unique_ptr<Mission> MissionFactory::CreateFromJson(const std::string& jsonPath) {
    using namespace Nova::Json;

    auto jsonOpt = TryParseFile(jsonPath);
    if (!jsonOpt) {
        return nullptr;
    }

    const auto& json = *jsonOpt;
    auto mission = std::make_unique<Mission>();

    // Parse identification
    mission->id = Get<std::string>(json, "id", "");
    mission->title = Get<std::string>(json, "title", "");
    mission->description = Get<std::string>(json, "description", "");
    mission->missionNumber = Get<int>(json, "missionNumber", 1);

    // Parse map info
    mission->mapFile = Get<std::string>(json, "mapFile", "");
    mission->mapName = Get<std::string>(json, "mapName", "");
    mission->mapDescription = Get<std::string>(json, "mapDescription", "");
    mission->playerStartPosition = Get<std::string>(json, "playerStartPosition", "");

    // Parse victory/defeat conditions
    mission->victoryCondition = static_cast<VictoryCondition>(Get<int>(json, "victoryCondition", 0));
    mission->defeatCondition = static_cast<DefeatCondition>(Get<int>(json, "defeatCondition", 0));
    mission->customVictoryScript = Get<std::string>(json, "customVictoryScript", "");
    mission->customDefeatScript = Get<std::string>(json, "customDefeatScript", "");

    // Parse time settings
    mission->timeLimit = Get<float>(json, "timeLimit", -1.0f);
    mission->parTime = Get<float>(json, "parTime", 0.0f);
    mission->showTimer = Get<bool>(json, "showTimer", false);

    // Parse starting resources
    if (json.contains("startingResources") && json["startingResources"].is_object()) {
        const auto& res = json["startingResources"];
        mission->startingResources.gold = Get<int>(res, "gold", 500);
        mission->startingResources.wood = Get<int>(res, "wood", 200);
        mission->startingResources.stone = Get<int>(res, "stone", 100);
        mission->startingResources.metal = Get<int>(res, "metal", 50);
        mission->startingResources.food = Get<int>(res, "food", 100);
        mission->startingResources.supply = Get<int>(res, "supply", 10);
        mission->startingResources.maxSupply = Get<int>(res, "maxSupply", 100);
    }

    // Parse scripts
    mission->initScript = Get<std::string>(json, "initScript", "");
    mission->updateScript = Get<std::string>(json, "updateScript", "");
    mission->victoryScript = Get<std::string>(json, "victoryScript", "");
    mission->defeatScript = Get<std::string>(json, "defeatScript", "");

    // Parse cinematics
    mission->introCinematic = Get<std::string>(json, "introCinematic", "");
    mission->outroCinematic = Get<std::string>(json, "outroCinematic", "");
    mission->defeatCinematic = Get<std::string>(json, "defeatCinematic", "");

    // Parse audio
    mission->ambientMusic = Get<std::string>(json, "ambientMusic", "");
    mission->combatMusic = Get<std::string>(json, "combatMusic", "");
    mission->victoryMusic = Get<std::string>(json, "victoryMusic", "");
    mission->defeatMusic = Get<std::string>(json, "defeatMusic", "");

    // Parse UI settings
    mission->thumbnailImage = Get<std::string>(json, "thumbnailImage", "");
    mission->loadingScreenImage = Get<std::string>(json, "loadingScreenImage", "");
    mission->loadingScreenTip = Get<std::string>(json, "loadingScreenTip", "");
    mission->showMinimap = Get<bool>(json, "showMinimap", true);
    mission->allowSave = Get<bool>(json, "allowSave", true);
    mission->allowPause = Get<bool>(json, "allowPause", true);

    // Parse objectives
    if (json.contains("objectives") && json["objectives"].is_string()) {
        PopulateObjectives(*mission, json["objectives"].get<std::string>());
    } else if (json.contains("objectives") && json["objectives"].is_array()) {
        PopulateObjectives(*mission, json["objectives"].dump());
    }

    // Parse AI players
    if (json.contains("aiPlayers") && json["aiPlayers"].is_string()) {
        PopulateAI(*mission, json["aiPlayers"].get<std::string>());
    } else if (json.contains("aiPlayers") && json["aiPlayers"].is_array()) {
        PopulateAI(*mission, json["aiPlayers"].dump());
    }

    // Parse prerequisite missions
    if (json.contains("prerequisiteMissions") && json["prerequisiteMissions"].is_array()) {
        for (const auto& prereq : json["prerequisiteMissions"]) {
            if (prereq.is_string()) {
                mission->prerequisiteMissions.push_back(prereq.get<std::string>());
            }
        }
    }

    return mission;
}

std::unique_ptr<Mission> MissionFactory::CreateFromConfig(const std::string& configPath) {
    // Config path is a directory containing mission.json and other assets
    // Try to load the main mission definition file
    std::string missionJsonPath = configPath;

    // Check if path is a directory (ends with / or \) or a file
    if (!missionJsonPath.empty()) {
        char lastChar = missionJsonPath.back();
        if (lastChar == '/' || lastChar == '\\') {
            missionJsonPath += "mission.json";
        } else if (missionJsonPath.find(".json") == std::string::npos) {
            // Assume it's a directory without trailing separator
            missionJsonPath += "/mission.json";
        }
    }

    return CreateFromJson(missionJsonPath);
}

void MissionFactory::PopulateObjectives(Mission& mission, const std::string& objectivesJson) {
    using namespace Nova::Json;

    auto parsed = TryParse(objectivesJson);
    if (!parsed || !parsed->is_array()) {
        return;
    }

    mission.objectives.clear();
    mission.primaryObjectiveIds.clear();
    mission.secondaryObjectiveIds.clear();
    mission.bonusObjectiveIds.clear();

    for (const auto& objJson : *parsed) {
        Objective objective;

        // Parse basic fields
        objective.id = Get<std::string>(objJson, "id", "");
        objective.title = Get<std::string>(objJson, "title", "");
        objective.description = Get<std::string>(objJson, "description", "");
        objective.shortDescription = Get<std::string>(objJson, "shortDescription", "");

        // Parse type and priority
        objective.type = static_cast<ObjectiveType>(Get<int>(objJson, "type", 0));
        objective.priority = static_cast<ObjectivePriority>(Get<int>(objJson, "priority", 0));

        // Parse target
        if (objJson.contains("target") && objJson["target"].is_object()) {
            const auto& target = objJson["target"];
            objective.target.targetType = Get<std::string>(target, "targetType", "");
            objective.target.targetId = Get<std::string>(target, "targetId", "");
            objective.target.count = Get<int>(target, "count", 1);
            objective.target.x = Get<float>(target, "x", 0.0f);
            objective.target.y = Get<float>(target, "y", 0.0f);
            objective.target.radius = Get<float>(target, "radius", 0.0f);
            objective.target.duration = Get<float>(target, "duration", 0.0f);
            objective.target.resourceType = Get<std::string>(target, "resourceType", "");
            objective.target.resourceAmount = Get<int>(target, "resourceAmount", 0);
        }

        // Parse progress requirement
        objective.progress.required = Get<int>(objJson, "requiredCount", 1);

        // Parse timing
        objective.timeLimit = Get<float>(objJson, "timeLimit", -1.0f);
        objective.failOnTimeout = Get<bool>(objJson, "failOnTimeout", false);

        // Parse prerequisites
        if (objJson.contains("prerequisites") && objJson["prerequisites"].is_array()) {
            for (const auto& prereq : objJson["prerequisites"]) {
                if (prereq.is_string()) {
                    objective.prerequisites.push_back(prereq.get<std::string>());
                }
            }
        }

        // Parse reward
        if (objJson.contains("reward") && objJson["reward"].is_object()) {
            const auto& reward = objJson["reward"];
            objective.reward.gold = Get<int>(reward, "gold", 0);
            objective.reward.wood = Get<int>(reward, "wood", 0);
            objective.reward.stone = Get<int>(reward, "stone", 0);
            objective.reward.metal = Get<int>(reward, "metal", 0);
            objective.reward.food = Get<int>(reward, "food", 0);
            objective.reward.experience = Get<int>(reward, "experience", 0);
        }

        // Parse UI settings
        objective.icon = Get<std::string>(objJson, "icon", "");
        objective.showNotification = Get<bool>(objJson, "showNotification", true);
        objective.showOnMinimap = Get<bool>(objJson, "showOnMinimap", true);

        // Track objective IDs by priority
        if (!objective.id.empty()) {
            switch (objective.priority) {
                case ObjectivePriority::Primary:
                    mission.primaryObjectiveIds.push_back(objective.id);
                    break;
                case ObjectivePriority::Secondary:
                    mission.secondaryObjectiveIds.push_back(objective.id);
                    break;
                case ObjectivePriority::Bonus:
                    mission.bonusObjectiveIds.push_back(objective.id);
                    break;
                case ObjectivePriority::Hidden:
                    // Hidden objectives not tracked in lists
                    break;
            }
        }

        mission.objectives.push_back(std::move(objective));
    }
}

void MissionFactory::PopulateAI(Mission& mission, const std::string& aiJson) {
    using namespace Nova::Json;

    auto parsed = TryParse(aiJson);
    if (!parsed || !parsed->is_array()) {
        return;
    }

    mission.aiPlayers.clear();

    for (const auto& aiJsonObj : *parsed) {
        MissionAI ai;

        // Parse basic fields
        ai.aiId = Get<std::string>(aiJsonObj, "aiId", "");
        ai.faction = Get<std::string>(aiJsonObj, "faction", "");
        ai.difficulty = static_cast<MissionDifficulty>(Get<int>(aiJsonObj, "difficulty", 1));
        ai.personality = Get<std::string>(aiJsonObj, "personality", "balanced");
        ai.handicap = Get<int>(aiJsonObj, "handicap", 0);
        ai.startingPosition = Get<std::string>(aiJsonObj, "startingPosition", "");
        ai.isAlly = Get<bool>(aiJsonObj, "isAlly", false);
        ai.canBeDefeated = Get<bool>(aiJsonObj, "canBeDefeated", true);
        ai.defeatTrigger = Get<std::string>(aiJsonObj, "defeatTrigger", "");

        // Parse AI resources
        if (aiJsonObj.contains("resources") && aiJsonObj["resources"].is_object()) {
            const auto& res = aiJsonObj["resources"];
            ai.resources.gold = Get<int>(res, "gold", 500);
            ai.resources.wood = Get<int>(res, "wood", 200);
            ai.resources.stone = Get<int>(res, "stone", 100);
            ai.resources.metal = Get<int>(res, "metal", 50);
            ai.resources.food = Get<int>(res, "food", 100);
            ai.resources.supply = Get<int>(res, "supply", 10);
            ai.resources.maxSupply = Get<int>(res, "maxSupply", 100);
        }

        mission.aiPlayers.push_back(std::move(ai));
    }
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
