#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <variant>
#include <deque>
#include <nlohmann/json.hpp>

namespace Nova {

using json = nlohmann::json;

// Forward declarations
class AnimationController;
class AnimationEventSystem;

/**
 * @brief Animation event triggered at specific times during animation playback
 */
struct AnimationEvent {
    float time = 0.0f;              // Normalized time 0-1
    std::string eventName;
    json eventData;
    bool triggered = false;         // Runtime flag

    // Serialization
    [[nodiscard]] json ToJson() const;
    static AnimationEvent FromJson(const json& j);
};

/**
 * @brief Condition for state transitions
 */
struct TransitionCondition {
    enum class Mode {
        IfTrue,
        IfFalse,
        Greater,
        Less,
        Equals,
        NotEquals,
        GreaterOrEqual,
        LessOrEqual
    };

    std::string parameter;
    Mode mode = Mode::IfTrue;
    float threshold = 0.0f;

    [[nodiscard]] json ToJson() const;
    static TransitionCondition FromJson(const json& j);
};

/**
 * @brief Transition between animation states
 */
struct StateTransition {
    std::string targetState;
    std::string condition;              // Expression like "speed > 0.1"
    std::vector<TransitionCondition> conditions;
    float blendDuration = 0.2f;
    int priority = 0;
    float exitTime = -1.0f;             // -1 = no exit time requirement
    bool hasExitTime = false;
    bool canTransitionToSelf = false;

    [[nodiscard]] json ToJson() const;
    static StateTransition FromJson(const json& j);
};

/**
 * @brief State behavior callbacks
 */
struct StateBehavior {
    std::string type;
    std::vector<json> onEnter;
    std::vector<json> onExit;
    std::vector<json> onUpdate;

    [[nodiscard]] json ToJson() const;
    static StateBehavior FromJson(const json& j);
};

/**
 * @brief Single animation state in the state machine
 */
struct AnimationState {
    std::string name;
    std::string animationClip;
    float speed = 1.0f;
    bool loop = true;
    bool mirror = false;
    float cycleOffset = 0.0f;
    std::string speedMultiplierParameter;
    std::string timeParameter;
    std::string mirrorParameter;

    std::vector<AnimationEvent> events;
    std::vector<StateTransition> transitions;
    std::vector<StateBehavior> behaviors;

    // Blend tree reference (alternative to clip)
    std::string blendTreeId;
    json blendTreeConfig;

    [[nodiscard]] json ToJson() const;
    static AnimationState FromJson(const json& j);
};

/**
 * @brief Animation parameter types
 */
struct AnimationParameter {
    enum class Type {
        Float,
        Int,
        Bool,
        Trigger
    };

    std::string name;
    Type type = Type::Float;
    std::variant<float, int, bool> defaultValue;
    std::variant<float, int, bool> currentValue;

    [[nodiscard]] json ToJson() const;
    static AnimationParameter FromJson(const json& j);
};

/**
 * @brief Animation layer for blending
 */
struct AnimationLayer {
    std::string name;
    float weight = 1.0f;
    std::string blendingMode = "override";  // override, additive
    std::string maskId;
    std::vector<AnimationState> states;
    std::string defaultState;
    int syncedLayerIndex = -1;
    bool ikPass = false;

    // Runtime state
    std::string currentState;
    float stateTime = 0.0f;

    [[nodiscard]] json ToJson() const;
    static AnimationLayer FromJson(const json& j);
};

/**
 * @brief State history entry for debugging
 */
struct StateHistoryEntry {
    std::string fromState;
    std::string toState;
    std::string trigger;
    float timestamp = 0.0f;
    json parameters;
};

/**
 * @brief Expression evaluator for condition parsing
 */
class ConditionExpressionParser {
public:
    ConditionExpressionParser() = default;

    /**
     * @brief Parse and evaluate a condition expression
     * @param expression Expression string like "speed > 0.1 && isGrounded"
     * @param parameters Current parameter values
     * @return True if condition is met
     */
    [[nodiscard]] bool Evaluate(const std::string& expression,
                                 const std::unordered_map<std::string, AnimationParameter>& parameters) const;

    /**
     * @brief Evaluate a single transition condition
     */
    [[nodiscard]] bool EvaluateCondition(const TransitionCondition& condition,
                                          const std::unordered_map<std::string, AnimationParameter>& parameters) const;

private:
    struct Token {
        enum Type { Number, Identifier, Operator, LeftParen, RightParen, End };
        Type type;
        std::string value;
        float numValue = 0.0f;
    };

    [[nodiscard]] std::vector<Token> Tokenize(const std::string& expr) const;
    [[nodiscard]] float ParseExpression(std::vector<Token>::const_iterator& it,
                                         std::vector<Token>::const_iterator end,
                                         const std::unordered_map<std::string, AnimationParameter>& params) const;
    [[nodiscard]] float ParseTerm(std::vector<Token>::const_iterator& it,
                                   std::vector<Token>::const_iterator end,
                                   const std::unordered_map<std::string, AnimationParameter>& params) const;
    [[nodiscard]] float ParseFactor(std::vector<Token>::const_iterator& it,
                                     std::vector<Token>::const_iterator end,
                                     const std::unordered_map<std::string, AnimationParameter>& params) const;
    [[nodiscard]] float GetParameterValue(const std::string& name,
                                           const std::unordered_map<std::string, AnimationParameter>& params) const;
};

/**
 * @brief Data-driven animation state machine
 *
 * Supports JSON configuration for states, transitions, and events.
 * Can be edited in the visual editor and hot-reloaded at runtime.
 */
class DataDrivenStateMachine {
public:
    DataDrivenStateMachine() = default;
    explicit DataDrivenStateMachine(AnimationController* controller);

    DataDrivenStateMachine(const DataDrivenStateMachine&) = delete;
    DataDrivenStateMachine& operator=(const DataDrivenStateMachine&) = delete;
    DataDrivenStateMachine(DataDrivenStateMachine&&) noexcept = default;
    DataDrivenStateMachine& operator=(DataDrivenStateMachine&&) noexcept = default;

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Load state machine from JSON file
     */
    bool LoadFromFile(const std::string& filepath);

    /**
     * @brief Load state machine from JSON object
     */
    bool LoadFromJson(const json& config);

    /**
     * @brief Save state machine to JSON file
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * @brief Export state machine to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Hot-reload configuration
     */
    bool Reload();

    // -------------------------------------------------------------------------
    // State Management
    // -------------------------------------------------------------------------

    /**
     * @brief Add a state to the machine
     */
    void AddState(const AnimationState& state);
    void AddState(AnimationState&& state);

    /**
     * @brief Remove a state by name
     */
    bool RemoveState(const std::string& name);

    /**
     * @brief Get state by name
     */
    [[nodiscard]] AnimationState* GetState(const std::string& name);
    [[nodiscard]] const AnimationState* GetState(const std::string& name) const;

    /**
     * @brief Get all states
     */
    [[nodiscard]] const std::vector<AnimationState>& GetStates() const { return m_states; }

    /**
     * @brief Set the default/initial state
     */
    void SetDefaultState(const std::string& stateName);
    [[nodiscard]] const std::string& GetDefaultState() const { return m_defaultState; }

    // -------------------------------------------------------------------------
    // Layer Management
    // -------------------------------------------------------------------------

    /**
     * @brief Add an animation layer
     */
    void AddLayer(const AnimationLayer& layer);

    /**
     * @brief Get layer by name
     */
    [[nodiscard]] AnimationLayer* GetLayer(const std::string& name);
    [[nodiscard]] const AnimationLayer* GetLayer(const std::string& name) const;

    /**
     * @brief Get all layers
     */
    [[nodiscard]] const std::vector<AnimationLayer>& GetLayers() const { return m_layers; }

    /**
     * @brief Set layer weight at runtime
     */
    void SetLayerWeight(const std::string& name, float weight);

    // -------------------------------------------------------------------------
    // Parameters
    // -------------------------------------------------------------------------

    /**
     * @brief Add a parameter
     */
    void AddParameter(const AnimationParameter& param);

    /**
     * @brief Set parameter value
     */
    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetBool(const std::string& name, bool value);
    void SetTrigger(const std::string& name);
    void ResetTrigger(const std::string& name);

    /**
     * @brief Get parameter value
     */
    [[nodiscard]] float GetFloat(const std::string& name) const;
    [[nodiscard]] int GetInt(const std::string& name) const;
    [[nodiscard]] bool GetBool(const std::string& name) const;

    /**
     * @brief Get all parameters
     */
    [[nodiscard]] const std::unordered_map<std::string, AnimationParameter>& GetParameters() const {
        return m_parameters;
    }

    // -------------------------------------------------------------------------
    // Runtime
    // -------------------------------------------------------------------------

    /**
     * @brief Start the state machine
     */
    void Start();

    /**
     * @brief Update the state machine
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Force transition to a state
     */
    void ForceState(const std::string& stateName, float blendTime = 0.2f);

    /**
     * @brief Get current state name
     */
    [[nodiscard]] const std::string& GetCurrentState() const { return m_currentState; }

    /**
     * @brief Get current normalized time in state
     */
    [[nodiscard]] float GetNormalizedTime() const { return m_normalizedTime; }

    /**
     * @brief Check if currently transitioning
     */
    [[nodiscard]] bool IsTransitioning() const { return m_isTransitioning; }

    /**
     * @brief Get transition progress (0-1)
     */
    [[nodiscard]] float GetTransitionProgress() const { return m_transitionProgress; }

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    /**
     * @brief Set event system for dispatching animation events
     */
    void SetEventSystem(AnimationEventSystem* eventSystem) { m_eventSystem = eventSystem; }

    /**
     * @brief Register callback for state enter
     */
    void OnStateEnter(const std::string& stateName, std::function<void()> callback);

    /**
     * @brief Register callback for state exit
     */
    void OnStateExit(const std::string& stateName, std::function<void()> callback);

    // -------------------------------------------------------------------------
    // Debugging
    // -------------------------------------------------------------------------

    /**
     * @brief Enable/disable state history recording
     */
    void SetRecordHistory(bool record) { m_recordHistory = record; }

    /**
     * @brief Get state history
     */
    [[nodiscard]] const std::deque<StateHistoryEntry>& GetHistory() const { return m_history; }

    /**
     * @brief Clear state history
     */
    void ClearHistory() { m_history.clear(); }

    /**
     * @brief Get debug info as JSON
     */
    [[nodiscard]] json GetDebugInfo() const;

    /**
     * @brief Set animation controller
     */
    void SetController(AnimationController* controller) { m_controller = controller; }

private:
    void EvaluateTransitions();
    void ProcessStateEvents(float previousTime, float currentTime);
    void StartTransition(const std::string& targetState, float blendDuration);
    void CompleteTransition();
    void RecordHistory(const std::string& from, const std::string& to, const std::string& trigger);
    void ExecuteBehaviors(const std::vector<StateBehavior>& behaviors, const std::string& phase);
    void ResetEventFlags();
    [[nodiscard]] float GetAnimationDuration(const std::string& clipName) const;

    AnimationController* m_controller = nullptr;
    AnimationEventSystem* m_eventSystem = nullptr;
    ConditionExpressionParser m_conditionParser;

    // Configuration
    std::string m_id;
    std::string m_name;
    std::string m_configPath;
    std::vector<AnimationState> m_states;
    std::vector<AnimationLayer> m_layers;
    std::unordered_map<std::string, AnimationParameter> m_parameters;
    std::string m_defaultState;

    // Runtime state
    std::string m_currentState;
    std::string m_previousState;
    float m_stateTime = 0.0f;
    float m_normalizedTime = 0.0f;
    bool m_isTransitioning = false;
    std::string m_transitionTarget;
    float m_transitionDuration = 0.0f;
    float m_transitionProgress = 0.0f;

    // Callbacks
    std::unordered_map<std::string, std::vector<std::function<void()>>> m_onEnterCallbacks;
    std::unordered_map<std::string, std::vector<std::function<void()>>> m_onExitCallbacks;

    // Debug
    bool m_recordHistory = false;
    std::deque<StateHistoryEntry> m_history;
    static constexpr size_t MAX_HISTORY_SIZE = 100;
    float m_totalTime = 0.0f;
};

} // namespace Nova
