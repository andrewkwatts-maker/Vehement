#include "Objective.hpp"
#include "engine/core/json_wrapper.hpp"
#include <sstream>
#include <algorithm>

namespace Vehement {
namespace RTS {
namespace Campaign {

Objective::Objective(const std::string& objId) : id(objId) {}

void Objective::Activate() {
    if (state == ObjectiveState::Locked) {
        state = ObjectiveState::Active;
        progress.current = 0;
        progress.timeElapsed = 0.0f;
        progress.timeRemaining = timeLimit;
        progress.timerExpired = false;

        if (onActivate) {
            onActivate(*this);
        }
    }
}

void Objective::Update(float deltaTime) {
    if (state != ObjectiveState::Active) return;

    progress.timeElapsed += deltaTime;

    // Handle timer
    if (HasTimer()) {
        progress.timeRemaining -= deltaTime;
        if (progress.timeRemaining <= 0.0f) {
            progress.timeRemaining = 0.0f;
            progress.timerExpired = true;

            if (type == ObjectiveType::Survive) {
                // Survive objectives complete when timer expires
                Complete();
            } else if (failOnTimeout) {
                Fail();
            }
        }
    }

    // Check hints
    for (auto& hint : hints) {
        if (!hint.shown && progress.timeElapsed >= hint.showAfterSeconds) {
            ShowNextHint();
            break;
        }
    }
}

void Objective::UpdateProgress(int32_t delta) {
    if (state != ObjectiveState::Active) return;

    progress.current += delta;
    if (progress.current < 0) progress.current = 0;

    if (onProgress) {
        onProgress(*this);
    }

    if (progress.IsComplete()) {
        Complete();
    }
}

void Objective::SetProgress(int32_t value) {
    if (state != ObjectiveState::Active) return;

    progress.current = value;
    if (progress.current < 0) progress.current = 0;

    if (onProgress) {
        onProgress(*this);
    }

    if (progress.IsComplete()) {
        Complete();
    }
}

void Objective::Complete() {
    if (state != ObjectiveState::Active) return;

    state = ObjectiveState::Completed;

    if (onComplete) {
        onComplete(*this);
    }
}

void Objective::Fail() {
    if (state != ObjectiveState::Active) return;

    state = ObjectiveState::Failed;

    if (onFail) {
        onFail(*this);
    }
}

void Objective::Cancel() {
    state = ObjectiveState::Cancelled;
}

void Objective::Reset() {
    state = ObjectiveState::Locked;
    progress = ObjectiveProgress();
    progress.required = target.count > 0 ? target.count : 1;

    for (auto& hint : hints) {
        hint.shown = false;
    }
}

void Objective::ShowNextHint() {
    for (auto& hint : hints) {
        if (!hint.shown) {
            hint.shown = true;
            // Trigger hint display callback if registered
            if (onHintShown) {
                onHintShown(*this, hint);
            }
            break;
        }
    }
}

bool Objective::CanActivate(const std::vector<Objective>& allObjectives) const {
    if (state != ObjectiveState::Locked) return false;

    // Check prerequisites
    for (const auto& prereq : prerequisites) {
        auto it = std::find_if(allObjectives.begin(), allObjectives.end(),
            [&prereq](const Objective& obj) { return obj.id == prereq; });

        if (it == allObjectives.end() || !it->IsCompleted()) {
            return false;
        }
    }

    // Check blockers
    for (const auto& blocker : blockedBy) {
        auto it = std::find_if(allObjectives.begin(), allObjectives.end(),
            [&blocker](const Objective& obj) { return obj.id == blocker; });

        if (it != allObjectives.end() && it->IsActive()) {
            return false;
        }
    }

    return true;
}

std::string Objective::Serialize() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":\"" << id << "\",";
    oss << "\"state\":" << static_cast<int>(state) << ",";
    oss << "\"progress\":{";
    oss << "\"current\":" << progress.current << ",";
    oss << "\"required\":" << progress.required << ",";
    oss << "\"timeElapsed\":" << progress.timeElapsed;
    oss << "}}";
    return oss.str();
}

bool Objective::Deserialize(const std::string& jsonStr) {
    using namespace Nova::Json;

    auto parsed = TryParse(jsonStr);
    if (!parsed) {
        return false;
    }

    const JsonValue& json = *parsed;

    // Parse identification
    id = Get<std::string>(json, "id", id);
    title = Get<std::string>(json, "title", title);
    description = Get<std::string>(json, "description", description);
    shortDescription = Get<std::string>(json, "shortDescription", shortDescription);

    // Parse configuration
    if (json.contains("type")) {
        type = static_cast<ObjectiveType>(json["type"].get<int>());
    }
    if (json.contains("priority")) {
        priority = static_cast<ObjectivePriority>(json["priority"].get<int>());
    }
    if (json.contains("state")) {
        state = static_cast<ObjectiveState>(json["state"].get<int>());
    }

    // Parse target specification
    if (json.contains("target") && json["target"].is_object()) {
        const auto& t = json["target"];
        target.targetType = Get<std::string>(t, "targetType", "");
        target.targetId = Get<std::string>(t, "targetId", "");
        target.count = Get<int32_t>(t, "count", 1);
        target.x = Get<float>(t, "x", 0.0f);
        target.y = Get<float>(t, "y", 0.0f);
        target.radius = Get<float>(t, "radius", 0.0f);
        target.duration = Get<float>(t, "duration", 0.0f);
        target.resourceType = Get<std::string>(t, "resourceType", "");
        target.resourceAmount = Get<int32_t>(t, "resourceAmount", 0);

        if (t.contains("tags") && t["tags"].is_array()) {
            target.tags.clear();
            for (const auto& tag : t["tags"]) {
                target.tags.push_back(tag.get<std::string>());
            }
        }
    }

    // Parse progress
    if (json.contains("progress") && json["progress"].is_object()) {
        const auto& p = json["progress"];
        progress.current = Get<int32_t>(p, "current", 0);
        progress.required = Get<int32_t>(p, "required", 1);
        progress.timeRemaining = Get<float>(p, "timeRemaining", -1.0f);
        progress.timeElapsed = Get<float>(p, "timeElapsed", 0.0f);
        progress.timerExpired = Get<bool>(p, "timerExpired", false);

        if (p.contains("completed") && p["completed"].is_array()) {
            progress.completed.clear();
            for (const auto& c : p["completed"]) {
                progress.completed.push_back(c.get<std::string>());
            }
        }
    }

    // Parse timing
    timeLimit = Get<float>(json, "timeLimit", -1.0f);
    failOnTimeout = Get<bool>(json, "failOnTimeout", false);

    // Parse dependencies
    if (json.contains("prerequisites") && json["prerequisites"].is_array()) {
        prerequisites.clear();
        for (const auto& p : json["prerequisites"]) {
            prerequisites.push_back(p.get<std::string>());
        }
    }
    if (json.contains("blockedBy") && json["blockedBy"].is_array()) {
        blockedBy.clear();
        for (const auto& b : json["blockedBy"]) {
            blockedBy.push_back(b.get<std::string>());
        }
    }

    // Parse rewards
    auto parseReward = [](const JsonValue& r, ObjectiveReward& reward) {
        reward.gold = Get<int32_t>(r, "gold", 0);
        reward.wood = Get<int32_t>(r, "wood", 0);
        reward.stone = Get<int32_t>(r, "stone", 0);
        reward.metal = Get<int32_t>(r, "metal", 0);
        reward.food = Get<int32_t>(r, "food", 0);
        reward.experience = Get<int32_t>(r, "experience", 0);
        reward.storyFlag = Get<std::string>(r, "storyFlag", "");

        auto parseStringArray = [](const JsonValue& arr, std::vector<std::string>& out) {
            out.clear();
            for (const auto& item : arr) {
                out.push_back(item.get<std::string>());
            }
        };

        if (r.contains("unlockedUnits") && r["unlockedUnits"].is_array()) {
            parseStringArray(r["unlockedUnits"], reward.unlockedUnits);
        }
        if (r.contains("unlockedBuildings") && r["unlockedBuildings"].is_array()) {
            parseStringArray(r["unlockedBuildings"], reward.unlockedBuildings);
        }
        if (r.contains("unlockedAbilities") && r["unlockedAbilities"].is_array()) {
            parseStringArray(r["unlockedAbilities"], reward.unlockedAbilities);
        }
        if (r.contains("items") && r["items"].is_array()) {
            parseStringArray(r["items"], reward.items);
        }
    };

    if (json.contains("reward") && json["reward"].is_object()) {
        parseReward(json["reward"], reward);
    }
    if (json.contains("bonusReward") && json["bonusReward"].is_object()) {
        parseReward(json["bonusReward"], bonusReward);
    }
    bonusTimeThreshold = Get<float>(json, "bonusTimeThreshold", 0.0f);

    // Parse hints
    if (json.contains("hints") && json["hints"].is_array()) {
        hints.clear();
        for (const auto& h : json["hints"]) {
            ObjectiveHint hint;
            hint.text = Get<std::string>(h, "text", "");
            hint.showAfterSeconds = Get<float>(h, "showAfterSeconds", 60.0f);
            hint.shown = Get<bool>(h, "shown", false);
            hint.icon = Get<std::string>(h, "icon", "");
            hints.push_back(hint);
        }
    }

    // Parse UI settings
    icon = Get<std::string>(json, "icon", icon);
    soundOnComplete = Get<std::string>(json, "soundOnComplete", soundOnComplete);
    soundOnFail = Get<std::string>(json, "soundOnFail", soundOnFail);
    soundOnUpdate = Get<std::string>(json, "soundOnUpdate", soundOnUpdate);
    showNotification = Get<bool>(json, "showNotification", true);
    showOnMinimap = Get<bool>(json, "showOnMinimap", true);
    minimapIcon = Get<std::string>(json, "minimapIcon", minimapIcon);

    // Parse custom script
    customScript = Get<std::string>(json, "customScript", customScript);
    customCondition = Get<std::string>(json, "customCondition", customCondition);

    return true;
}

// ObjectiveFactory implementations

std::unique_ptr<Objective> ObjectiveFactory::Create(ObjectiveType type) {
    auto obj = std::make_unique<Objective>();
    obj->type = type;
    return obj;
}

std::unique_ptr<Objective> ObjectiveFactory::CreateFromJson(const std::string& jsonStr) {
    using namespace Nova::Json;

    auto parsed = TryParse(jsonStr);
    if (!parsed) {
        return nullptr;
    }

    auto objective = std::make_unique<Objective>();
    if (!objective->Deserialize(jsonStr)) {
        return nullptr;
    }

    // Set up progress.required based on target if not explicitly set
    if (objective->progress.required <= 0) {
        if (objective->target.count > 0) {
            objective->progress.required = objective->target.count;
        } else if (objective->target.resourceAmount > 0) {
            objective->progress.required = objective->target.resourceAmount;
        } else {
            objective->progress.required = 1;
        }
    }

    return objective;
}

std::unique_ptr<Objective> ObjectiveFactory::CreateKill(const std::string& id,
                                                         const std::string& targetType,
                                                         int32_t count) {
    auto obj = std::make_unique<Objective>(id);
    obj->type = ObjectiveType::Kill;
    obj->target.targetType = targetType;
    obj->target.count = count;
    obj->progress.required = count;
    return obj;
}

std::unique_ptr<Objective> ObjectiveFactory::CreateSurvive(const std::string& id,
                                                            float duration) {
    auto obj = std::make_unique<Objective>(id);
    obj->type = ObjectiveType::Survive;
    obj->target.duration = duration;
    obj->timeLimit = duration;
    obj->progress.required = 1;
    return obj;
}

std::unique_ptr<Objective> ObjectiveFactory::CreateCapture(const std::string& id,
                                                            const std::string& targetId) {
    auto obj = std::make_unique<Objective>(id);
    obj->type = ObjectiveType::Capture;
    obj->target.targetId = targetId;
    obj->progress.required = 1;
    return obj;
}

std::unique_ptr<Objective> ObjectiveFactory::CreateBuild(const std::string& id,
                                                          const std::string& buildingType,
                                                          int32_t count) {
    auto obj = std::make_unique<Objective>(id);
    obj->type = ObjectiveType::Build;
    obj->target.targetType = buildingType;
    obj->target.count = count;
    obj->progress.required = count;
    return obj;
}

std::unique_ptr<Objective> ObjectiveFactory::CreateCollect(const std::string& id,
                                                            const std::string& resourceType,
                                                            int32_t amount) {
    auto obj = std::make_unique<Objective>(id);
    obj->type = ObjectiveType::Collect;
    obj->target.resourceType = resourceType;
    obj->target.resourceAmount = amount;
    obj->progress.required = amount;
    return obj;
}

std::unique_ptr<Objective> ObjectiveFactory::CreateReach(const std::string& id,
                                                          float x, float y, float radius) {
    auto obj = std::make_unique<Objective>(id);
    obj->type = ObjectiveType::Reach;
    obj->target.x = x;
    obj->target.y = y;
    obj->target.radius = radius;
    obj->progress.required = 1;
    return obj;
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
