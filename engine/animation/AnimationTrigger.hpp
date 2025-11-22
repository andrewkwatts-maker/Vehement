#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <variant>
#include <chrono>
#include <nlohmann/json.hpp>

namespace Nova {

using json = nlohmann::json;

/**
 * @brief Trigger types for animation events
 */
enum class TriggerType {
    Time,           // At specific time/frame
    StateEnter,     // On entering a state
    StateExit,      // On exiting a state
    Property,       // When property crosses threshold
    Combo,          // Sequence of inputs
    Custom          // Custom callback
};

/**
 * @brief Property comparison mode
 */
enum class PropertyCompareMode {
    CrossAbove,     // Value crosses above threshold
    CrossBelow,     // Value crosses below threshold
    WhileAbove,     // While value is above threshold
    WhileBelow,     // While value is below threshold
    OnChange        // When value changes
};

/**
 * @brief Combo input entry
 */
struct ComboInput {
    std::string inputName;          // Input identifier (e.g., "attack", "jump")
    float maxDelay = 0.5f;          // Max time since previous input
    bool mustRelease = false;       // Whether input must be released first

    [[nodiscard]] json ToJson() const;
    static ComboInput FromJson(const json& j);
};

/**
 * @brief Callback signature for trigger actions
 */
using TriggerCallback = std::function<void(const json& context)>;

/**
 * @brief Callback signature for Python/script triggers
 */
using ScriptTriggerCallback = std::function<void(const std::string& functionName, const json& args)>;

/**
 * @brief Base trigger configuration
 */
struct TriggerConfig {
    std::string id;
    std::string name;
    TriggerType type = TriggerType::Time;
    bool enabled = true;
    int priority = 0;
    float cooldown = 0.0f;          // Minimum time between activations

    // For time-based triggers
    float triggerTime = 0.0f;       // Normalized time (0-1) or frame number
    bool isFrameBased = false;

    // For state triggers
    std::string targetState;

    // For property triggers
    std::string propertyName;
    float threshold = 0.0f;
    PropertyCompareMode compareMode = PropertyCompareMode::CrossAbove;

    // For combo triggers
    std::vector<ComboInput> comboSequence;
    float comboWindow = 1.0f;       // Total time allowed for combo

    // Action to execute
    std::string eventName;          // Event to dispatch
    json eventData;                 // Data to pass with event
    std::string scriptFunction;     // Python/native function to call
    json scriptArgs;                // Arguments for script

    [[nodiscard]] json ToJson() const;
    static TriggerConfig FromJson(const json& j);
};

/**
 * @brief Runtime state for a trigger
 */
struct TriggerState {
    bool wasTriggered = false;
    float lastTriggerTime = -1000.0f;
    float previousValue = 0.0f;     // For property triggers

    // For combo triggers
    size_t comboProgress = 0;
    float lastInputTime = 0.0f;
    std::vector<float> inputTimes;
};

/**
 * @brief Individual trigger instance
 */
class AnimationTrigger {
public:
    AnimationTrigger() = default;
    explicit AnimationTrigger(const TriggerConfig& config);

    /**
     * @brief Load from JSON
     */
    bool LoadFromJson(const json& j);

    /**
     * @brief Export to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Check if trigger should fire
     * @param context Current animation context
     * @param currentTime Current time in seconds
     * @return True if trigger should fire
     */
    [[nodiscard]] bool ShouldTrigger(const json& context, float currentTime);

    /**
     * @brief Reset trigger state
     */
    void Reset();

    /**
     * @brief Process input for combo triggers
     */
    void ProcessInput(const std::string& inputName, float currentTime);

    /**
     * @brief Set property value for property triggers
     */
    void SetPropertyValue(float value);

    /**
     * @brief Get trigger configuration
     */
    [[nodiscard]] const TriggerConfig& GetConfig() const { return m_config; }
    [[nodiscard]] TriggerConfig& GetConfig() { return m_config; }

    /**
     * @brief Enable/disable trigger
     */
    void SetEnabled(bool enabled) { m_config.enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_config.enabled; }

    /**
     * @brief Get trigger state
     */
    [[nodiscard]] const TriggerState& GetState() const { return m_state; }

private:
    bool CheckTimeTrigger(float normalizedTime, float currentTime);
    bool CheckStateTrigger(const std::string& currentState, const std::string& previousState);
    bool CheckPropertyTrigger(float currentTime);
    bool CheckComboTrigger(float currentTime);

    TriggerConfig m_config;
    TriggerState m_state;
    float m_currentPropertyValue = 0.0f;
};

/**
 * @brief Manages multiple triggers for an animation system
 */
class AnimationTriggerSystem {
public:
    AnimationTriggerSystem() = default;

    /**
     * @brief Load triggers from JSON configuration
     */
    bool LoadFromJson(const json& config);

    /**
     * @brief Save triggers to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Add a trigger
     */
    void AddTrigger(const TriggerConfig& config);
    void AddTrigger(std::unique_ptr<AnimationTrigger> trigger);

    /**
     * @brief Remove trigger by ID
     */
    bool RemoveTrigger(const std::string& id);

    /**
     * @brief Get trigger by ID
     */
    [[nodiscard]] AnimationTrigger* GetTrigger(const std::string& id);
    [[nodiscard]] const AnimationTrigger* GetTrigger(const std::string& id) const;

    /**
     * @brief Get all triggers
     */
    [[nodiscard]] const std::vector<std::unique_ptr<AnimationTrigger>>& GetTriggers() const {
        return m_triggers;
    }

    /**
     * @brief Update triggers and fire callbacks
     * @param context Animation context (current state, time, parameters)
     * @param currentTime Current game time
     */
    void Update(const json& context, float currentTime);

    /**
     * @brief Process input event for combo triggers
     */
    void ProcessInput(const std::string& inputName, float currentTime);

    /**
     * @brief Set property value for property triggers
     */
    void SetProperty(const std::string& propertyName, float value);

    /**
     * @brief Register callback for trigger events
     */
    void SetEventCallback(TriggerCallback callback) { m_eventCallback = std::move(callback); }

    /**
     * @brief Register callback for script triggers
     */
    void SetScriptCallback(ScriptTriggerCallback callback) { m_scriptCallback = std::move(callback); }

    /**
     * @brief Reset all triggers
     */
    void ResetAll();

    /**
     * @brief Get triggers that fired last update
     */
    [[nodiscard]] const std::vector<std::string>& GetFiredTriggers() const { return m_firedTriggers; }

private:
    void FireTrigger(AnimationTrigger& trigger, const json& context);

    std::vector<std::unique_ptr<AnimationTrigger>> m_triggers;
    std::unordered_map<std::string, float> m_propertyValues;
    TriggerCallback m_eventCallback;
    ScriptTriggerCallback m_scriptCallback;
    std::vector<std::string> m_firedTriggers;
};

/**
 * @brief Combo detector for complex input sequences
 */
class ComboDetector {
public:
    struct ComboDefinition {
        std::string id;
        std::string name;
        std::vector<ComboInput> sequence;
        float windowTime = 1.0f;
        std::string onComplete;         // Event to fire
        json completionData;
    };

    ComboDetector() = default;

    /**
     * @brief Add a combo definition
     */
    void AddCombo(const ComboDefinition& combo);

    /**
     * @brief Process input
     */
    void ProcessInput(const std::string& input, float currentTime);

    /**
     * @brief Update combo states
     */
    void Update(float currentTime);

    /**
     * @brief Get completed combos this frame
     */
    [[nodiscard]] std::vector<std::string> GetCompletedCombos();

    /**
     * @brief Reset all combo progress
     */
    void Reset();

    /**
     * @brief Load combos from JSON
     */
    bool LoadFromJson(const json& config);

    /**
     * @brief Export to JSON
     */
    [[nodiscard]] json ToJson() const;

private:
    struct ComboState {
        size_t progress = 0;
        float lastInputTime = 0.0f;
        bool completed = false;
    };

    std::vector<ComboDefinition> m_combos;
    std::unordered_map<std::string, ComboState> m_states;
    std::vector<std::string> m_completedCombos;
    std::vector<std::pair<std::string, float>> m_inputHistory;
    static constexpr size_t MAX_INPUT_HISTORY = 20;
};

/**
 * @brief Predefined trigger templates
 */
namespace TriggerTemplates {
    /**
     * @brief Create footstep trigger at specific times
     */
    [[nodiscard]] TriggerConfig CreateFootstep(const std::string& id, float time,
                                                bool isLeftFoot = true);

    /**
     * @brief Create attack hit frame trigger
     */
    [[nodiscard]] TriggerConfig CreateHitFrame(const std::string& id, float time,
                                                const json& hitData = {});

    /**
     * @brief Create projectile spawn trigger
     */
    [[nodiscard]] TriggerConfig CreateProjectileSpawn(const std::string& id, float time,
                                                       const std::string& projectileType);

    /**
     * @brief Create VFX spawn trigger
     */
    [[nodiscard]] TriggerConfig CreateVFXSpawn(const std::string& id, float time,
                                                const std::string& vfxId,
                                                const std::string& bone = "");

    /**
     * @brief Create sound trigger
     */
    [[nodiscard]] TriggerConfig CreateSound(const std::string& id, float time,
                                             const std::string& soundId);

    /**
     * @brief Create state enter trigger
     */
    [[nodiscard]] TriggerConfig CreateStateEnter(const std::string& id,
                                                  const std::string& stateName,
                                                  const std::string& eventName);

    /**
     * @brief Create property threshold trigger
     */
    [[nodiscard]] TriggerConfig CreatePropertyThreshold(const std::string& id,
                                                         const std::string& property,
                                                         float threshold,
                                                         PropertyCompareMode mode);

    /**
     * @brief Create combo trigger
     */
    [[nodiscard]] TriggerConfig CreateCombo(const std::string& id,
                                             const std::vector<std::string>& inputs,
                                             float windowTime = 1.0f);
}

} // namespace Nova
