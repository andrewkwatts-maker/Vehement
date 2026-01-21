#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Vehement {
namespace UI {
namespace Campaign {

/**
 * @brief Display data for an objective
 */
struct ObjectiveDisplayData {
    std::string id;
    std::string title;
    std::string description;
    std::string icon;
    bool isPrimary = true;
    bool isCompleted = false;
    bool isFailed = false;
    bool isNew = false;
    bool hasTimer = false;
    float progress = 0.0f;
    int32_t currentCount = 0;
    int32_t requiredCount = 1;
    float timeRemaining = -1.0f;
    std::string hint;
};

/**
 * @brief Configuration for objective UI
 */
struct ObjectiveUIConfig {
    bool showSecondaryObjectives = true;
    bool showBonusObjectives = true;
    bool showProgress = true;
    bool showTimers = true;
    bool showHints = true;
    bool autoCollapseCompleted = true;
    bool animateNewObjectives = true;
    bool animateCompletion = true;
    float newObjectiveFlashDuration = 3.0f;
    float completionAnimationDuration = 1.5f;
    bool playSoundOnComplete = true;
    bool playSoundOnNew = true;
    std::string completeSound;
    std::string newObjectiveSound;
    std::string failSound;
};

/**
 * @brief In-game objectives HUD
 */
class ObjectiveUI {
public:
    ObjectiveUI();
    ~ObjectiveUI();

    [[nodiscard]] static ObjectiveUI& Instance();

    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // Configuration
    void SetConfig(const ObjectiveUIConfig& config);
    [[nodiscard]] const ObjectiveUIConfig& GetConfig() const { return m_config; }

    // Visibility
    void Show();
    void Hide();
    [[nodiscard]] bool IsVisible() const { return m_visible; }
    void Toggle();

    // Panel state
    void Expand();
    void Collapse();
    [[nodiscard]] bool IsExpanded() const { return m_expanded; }
    void ToggleExpand();

    // Objective management
    void SetObjectives(const std::vector<ObjectiveDisplayData>& objectives);
    void AddObjective(const ObjectiveDisplayData& objective);
    void UpdateObjective(const std::string& objectiveId, const ObjectiveDisplayData& data);
    void CompleteObjective(const std::string& objectiveId);
    void FailObjective(const std::string& objectiveId);
    void RemoveObjective(const std::string& objectiveId);
    void ClearObjectives();

    // Progress updates
    void UpdateProgress(const std::string& objectiveId, float progress);
    void UpdateProgress(const std::string& objectiveId, int32_t current, int32_t required);
    void UpdateTimer(const std::string& objectiveId, float timeRemaining);

    // Notifications
    void ShowNewObjectiveAlert(const std::string& objectiveId);
    void ShowCompletionAlert(const std::string& objectiveId);
    void ShowFailureAlert(const std::string& objectiveId);
    void ShowHint(const std::string& objectiveId, const std::string& hint);

    // Update and render
    void Update(float deltaTime);
    void Render();

    // Callbacks
    void SetOnObjectiveClick(std::function<void(const std::string&)> callback) { m_onObjectiveClick = callback; }
    void SetOnHintRequest(std::function<void(const std::string&)> callback) { m_onHintRequest = callback; }

    // HTML bindings
    void BindToHTML();
    void HandleHTMLEvent(const std::string& eventName, const std::string& data);

private:
    bool m_initialized = false;
    bool m_visible = true;
    bool m_expanded = true;
    ObjectiveUIConfig m_config;

    std::vector<ObjectiveDisplayData> m_objectives;
    std::vector<std::string> m_alertQueue;
    float m_alertTimer = 0.0f;
    std::unordered_map<std::string, float> m_newObjectiveTimers; ///< Tracks animation time for new objectives

    std::function<void(const std::string&)> m_onObjectiveClick;
    std::function<void(const std::string&)> m_onHintRequest;

    void SendDataToHTML();
    void UpdateAlerts(float deltaTime);
    void PlaySound(const std::string& soundFile);
};

} // namespace Campaign
} // namespace UI
} // namespace Vehement
