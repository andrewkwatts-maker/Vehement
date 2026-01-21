#include "ObjectiveUI.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include <algorithm>

namespace Vehement {
namespace UI {
namespace Campaign {

ObjectiveUI::ObjectiveUI() = default;
ObjectiveUI::~ObjectiveUI() = default;

ObjectiveUI& ObjectiveUI::Instance() {
    static ObjectiveUI instance;
    return instance;
}

bool ObjectiveUI::Initialize() {
    if (m_initialized) return true;

    m_config = ObjectiveUIConfig();
    m_visible = true;
    m_expanded = true;
    m_objectives.clear();
    m_initialized = true;
    return true;
}

void ObjectiveUI::Shutdown() {
    m_objectives.clear();
    m_alertQueue.clear();
    m_initialized = false;
}

void ObjectiveUI::SetConfig(const ObjectiveUIConfig& config) {
    m_config = config;
}

void ObjectiveUI::Show() {
    m_visible = true;
    SendDataToHTML();
}

void ObjectiveUI::Hide() {
    m_visible = false;
}

void ObjectiveUI::Toggle() {
    if (m_visible) Hide();
    else Show();
}

void ObjectiveUI::Expand() {
    m_expanded = true;
    SendDataToHTML();
}

void ObjectiveUI::Collapse() {
    m_expanded = false;
    SendDataToHTML();
}

void ObjectiveUI::ToggleExpand() {
    if (m_expanded) Collapse();
    else Expand();
}

void ObjectiveUI::SetObjectives(const std::vector<ObjectiveDisplayData>& objectives) {
    m_objectives = objectives;
    SendDataToHTML();
}

void ObjectiveUI::AddObjective(const ObjectiveDisplayData& objective) {
    m_objectives.push_back(objective);

    if (m_config.animateNewObjectives) {
        m_objectives.back().isNew = true;
        ShowNewObjectiveAlert(objective.id);
    }

    SendDataToHTML();
}

void ObjectiveUI::UpdateObjective(const std::string& objectiveId, const ObjectiveDisplayData& data) {
    auto it = std::find_if(m_objectives.begin(), m_objectives.end(),
        [&objectiveId](const ObjectiveDisplayData& obj) { return obj.id == objectiveId; });

    if (it != m_objectives.end()) {
        *it = data;
        SendDataToHTML();
    }
}

void ObjectiveUI::CompleteObjective(const std::string& objectiveId) {
    auto it = std::find_if(m_objectives.begin(), m_objectives.end(),
        [&objectiveId](const ObjectiveDisplayData& obj) { return obj.id == objectiveId; });

    if (it != m_objectives.end()) {
        it->isCompleted = true;
        it->progress = 1.0f;
        it->currentCount = it->requiredCount;

        if (m_config.animateCompletion) {
            ShowCompletionAlert(objectiveId);
        }

        if (m_config.playSoundOnComplete && !m_config.completeSound.empty()) {
            PlaySound(m_config.completeSound);
        }

        SendDataToHTML();
    }
}

void ObjectiveUI::FailObjective(const std::string& objectiveId) {
    auto it = std::find_if(m_objectives.begin(), m_objectives.end(),
        [&objectiveId](const ObjectiveDisplayData& obj) { return obj.id == objectiveId; });

    if (it != m_objectives.end()) {
        it->isFailed = true;
        ShowFailureAlert(objectiveId);

        if (!m_config.failSound.empty()) {
            PlaySound(m_config.failSound);
        }

        SendDataToHTML();
    }
}

void ObjectiveUI::RemoveObjective(const std::string& objectiveId) {
    auto it = std::find_if(m_objectives.begin(), m_objectives.end(),
        [&objectiveId](const ObjectiveDisplayData& obj) { return obj.id == objectiveId; });

    if (it != m_objectives.end()) {
        m_objectives.erase(it);
        SendDataToHTML();
    }
}

void ObjectiveUI::ClearObjectives() {
    m_objectives.clear();
    SendDataToHTML();
}

void ObjectiveUI::UpdateProgress(const std::string& objectiveId, float progress) {
    auto it = std::find_if(m_objectives.begin(), m_objectives.end(),
        [&objectiveId](const ObjectiveDisplayData& obj) { return obj.id == objectiveId; });

    if (it != m_objectives.end()) {
        it->progress = progress;
        SendDataToHTML();
    }
}

void ObjectiveUI::UpdateProgress(const std::string& objectiveId, int32_t current, int32_t required) {
    auto it = std::find_if(m_objectives.begin(), m_objectives.end(),
        [&objectiveId](const ObjectiveDisplayData& obj) { return obj.id == objectiveId; });

    if (it != m_objectives.end()) {
        it->currentCount = current;
        it->requiredCount = required;
        it->progress = required > 0 ? static_cast<float>(current) / static_cast<float>(required) : 0.0f;
        SendDataToHTML();
    }
}

void ObjectiveUI::UpdateTimer(const std::string& objectiveId, float timeRemaining) {
    auto it = std::find_if(m_objectives.begin(), m_objectives.end(),
        [&objectiveId](const ObjectiveDisplayData& obj) { return obj.id == objectiveId; });

    if (it != m_objectives.end()) {
        it->timeRemaining = timeRemaining;
        SendDataToHTML();
    }
}

void ObjectiveUI::ShowNewObjectiveAlert(const std::string& objectiveId) {
    m_alertQueue.push_back("new:" + objectiveId);
}

void ObjectiveUI::ShowCompletionAlert(const std::string& objectiveId) {
    m_alertQueue.push_back("complete:" + objectiveId);
}

void ObjectiveUI::ShowFailureAlert(const std::string& objectiveId) {
    m_alertQueue.push_back("fail:" + objectiveId);
}

void ObjectiveUI::ShowHint(const std::string& objectiveId, const std::string& hint) {
    auto it = std::find_if(m_objectives.begin(), m_objectives.end(),
        [&objectiveId](const ObjectiveDisplayData& obj) { return obj.id == objectiveId; });

    if (it != m_objectives.end()) {
        it->hint = hint;
        SendDataToHTML();
    }
}

void ObjectiveUI::Update(float deltaTime) {
    if (!m_visible) return;

    UpdateAlerts(deltaTime);

    // Clear "new" flag after animation
    for (auto& obj : m_objectives) {
        if (obj.isNew) {
            // Track animation elapsed time
            m_newObjectiveTimers[obj.id] += deltaTime;

            // Clear flag when animation duration completes
            if (m_newObjectiveTimers[obj.id] >= m_config.newObjectiveFlashDuration) {
                obj.isNew = false;
                m_newObjectiveTimers.erase(obj.id);
                SendDataToHTML();
            }
        }
    }
}

void ObjectiveUI::Render() {
    // HTML-based rendering
}

void ObjectiveUI::BindToHTML() {
    // Register JavaScript callbacks for objective interactions
    Engine::UI::UIBinding binding;

    binding.ExposeFunction("Objectives.onClick", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            HandleHTMLEvent("objectiveClick", args["id"].get<std::string>());
        }
        return nullptr;
    });

    binding.ExposeFunction("Objectives.requestHint", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            HandleHTMLEvent("requestHint", args["id"].get<std::string>());
        }
        return nullptr;
    });

    binding.ExposeFunction("Objectives.toggleExpand", [this](const nlohmann::json&) -> nlohmann::json {
        HandleHTMLEvent("toggleExpand", "");
        return nullptr;
    });
}

void ObjectiveUI::HandleHTMLEvent(const std::string& eventName, const std::string& data) {
    if (eventName == "objectiveClick" && m_onObjectiveClick) {
        m_onObjectiveClick(data);
    } else if (eventName == "requestHint" && m_onHintRequest) {
        m_onHintRequest(data);
    } else if (eventName == "toggleExpand") {
        ToggleExpand();
    }
}

void ObjectiveUI::SendDataToHTML() {
    // Send objective data to HTML via JavaScript bindings
    Engine::UI::UIBinding binding;

    nlohmann::json data;
    data["visible"] = m_visible;
    data["expanded"] = m_expanded;

    nlohmann::json objectivesArray = nlohmann::json::array();
    for (const auto& obj : m_objectives) {
        nlohmann::json objJson;
        objJson["id"] = obj.id;
        objJson["title"] = obj.title;
        objJson["description"] = obj.description;
        objJson["icon"] = obj.icon;
        objJson["isPrimary"] = obj.isPrimary;
        objJson["isCompleted"] = obj.isCompleted;
        objJson["isFailed"] = obj.isFailed;
        objJson["isNew"] = obj.isNew;
        objJson["hasTimer"] = obj.hasTimer;
        objJson["progress"] = obj.progress;
        objJson["currentCount"] = obj.currentCount;
        objJson["requiredCount"] = obj.requiredCount;
        objJson["timeRemaining"] = obj.timeRemaining;
        objJson["hint"] = obj.hint;
        objectivesArray.push_back(objJson);
    }
    data["objectives"] = objectivesArray;

    // Include current alert if any
    if (!m_alertQueue.empty()) {
        data["currentAlert"] = m_alertQueue.front();
    }

    binding.CallJS("Objectives.updateData", data);
}

void ObjectiveUI::UpdateAlerts(float deltaTime) {
    if (!m_alertQueue.empty()) {
        m_alertTimer -= deltaTime;
        if (m_alertTimer <= 0.0f) {
            m_alertQueue.erase(m_alertQueue.begin());
            m_alertTimer = 2.0f; // Display each alert for 2 seconds
        }
    }
}

void ObjectiveUI::PlaySound(const std::string& soundFile) {
    // Play sound through audio system via JavaScript bindings
    if (soundFile.empty()) return;

    Engine::UI::UIBinding binding;
    nlohmann::json args;
    args["path"] = soundFile;
    args["volume"] = 1.0f;
    binding.CallJS("Audio.playSound", args);
}

} // namespace Campaign
} // namespace UI
} // namespace Vehement
