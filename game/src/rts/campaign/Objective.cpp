#include "Objective.hpp"
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
            // TODO: Trigger hint display in UI
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

bool Objective::Deserialize(const std::string& /*json*/) {
    // TODO: Implement JSON parsing
    return true;
}

// ObjectiveFactory implementations

std::unique_ptr<Objective> ObjectiveFactory::Create(ObjectiveType type) {
    auto obj = std::make_unique<Objective>();
    obj->type = type;
    return obj;
}

std::unique_ptr<Objective> ObjectiveFactory::CreateFromJson(const std::string& /*json*/) {
    // TODO: Parse JSON and create objective
    return std::make_unique<Objective>();
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
