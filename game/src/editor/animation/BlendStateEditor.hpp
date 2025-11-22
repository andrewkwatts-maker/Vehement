#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Vehement {

/**
 * @brief Visual node for blend state
 */
struct BlendStateNode {
    std::string id;
    std::string name;
    std::string animationClip;
    glm::vec2 position{0.0f};
    glm::vec2 size{150.0f, 80.0f};
    glm::vec4 color{0.3f, 0.5f, 0.8f, 1.0f};
    bool selected = false;
    bool isDefault = false;
    bool isAnyState = false;

    // Blend tree specific
    bool isBlendTree = false;
    std::string blendTreeType;  // "1D", "2D", "direct"
    std::string blendParameter;
    std::string blendParameterX;
    std::string blendParameterY;
};

/**
 * @brief Transition condition
 */
struct TransitionCondition {
    std::string parameter;
    std::string comparison;  // "greater", "less", "equals", "notEquals", "trigger"
    float threshold = 0.0f;
    bool boolValue = false;
};

/**
 * @brief State transition connection
 */
struct StateTransitionConnection {
    std::string id;
    std::string fromState;
    std::string toState;
    float duration = 0.2f;
    float exitTime = 0.0f;
    bool hasExitTime = false;
    bool canTransitionToSelf = false;
    std::vector<TransitionCondition> conditions;
    glm::vec2 controlPoint{0.0f};
    bool selected = false;
    int priority = 0;
};

/**
 * @brief Animation parameter definition
 */
struct AnimationParameter {
    std::string name;
    std::string type;  // "float", "int", "bool", "trigger"
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
    float minValue = 0.0f;
    float maxValue = 1.0f;
};

/**
 * @brief Blend tree child node
 */
struct BlendTreeChild {
    std::string animationClip;
    float threshold = 0.0f;  // For 1D
    glm::vec2 position{0.0f};  // For 2D
    float directWeight = 0.0f;  // For direct blend
    float timeScale = 1.0f;
    bool mirror = false;
};

/**
 * @brief Blend tree configuration
 */
struct BlendTreeConfig {
    std::string type = "1D";  // "1D", "2D", "freeform", "direct"
    std::string parameterX;
    std::string parameterY;
    std::vector<BlendTreeChild> children;
    bool normalizeWeights = true;
};

/**
 * @brief Blend state editor for visual state machine creation
 *
 * Features:
 * - Blend tree visualization
 * - State connections
 * - Transition conditions
 * - Blend weights
 * - Test blend parameters
 */
class BlendStateEditor {
public:
    struct Config {
        glm::vec2 gridSize{20.0f};
        bool snapToGrid = true;
        bool showGrid = true;
        float zoomMin = 0.25f;
        float zoomMax = 4.0f;
        glm::vec4 transitionColor{0.8f, 0.8f, 0.8f, 0.8f};
        glm::vec4 selectedTransitionColor{1.0f, 0.8f, 0.2f, 1.0f};
        glm::vec4 defaultStateColor{0.2f, 0.8f, 0.3f, 1.0f};
        glm::vec4 anyStateColor{0.8f, 0.5f, 0.2f, 1.0f};
    };

    BlendStateEditor();
    ~BlendStateEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(const Config& config = {});

    // =========================================================================
    // File Operations
    // =========================================================================

    /**
     * @brief Create new state machine
     */
    void NewStateMachine(const std::string& name = "NewStateMachine");

    /**
     * @brief Load state machine from file
     */
    bool LoadStateMachine(const std::string& filePath);

    /**
     * @brief Save state machine
     */
    bool SaveStateMachine(const std::string& filePath = "");

    /**
     * @brief Export to JSON
     */
    [[nodiscard]] std::string ExportToJson() const;

    /**
     * @brief Import from JSON
     */
    bool ImportFromJson(const std::string& jsonStr);

    // =========================================================================
    // State Management
    // =========================================================================

    /**
     * @brief Add state node
     */
    BlendStateNode* AddState(const std::string& name, const glm::vec2& position);

    /**
     * @brief Add blend tree state
     */
    BlendStateNode* AddBlendTreeState(const std::string& name, const glm::vec2& position,
                                       const std::string& type = "1D");

    /**
     * @brief Remove state
     */
    void RemoveState(const std::string& id);

    /**
     * @brief Get state by ID
     */
    [[nodiscard]] BlendStateNode* GetState(const std::string& id);
    [[nodiscard]] const BlendStateNode* GetState(const std::string& id) const;

    /**
     * @brief Get all states
     */
    [[nodiscard]] const std::vector<BlendStateNode>& GetStates() const { return m_states; }

    /**
     * @brief Set default state
     */
    void SetDefaultState(const std::string& id);

    /**
     * @brief Get default state
     */
    [[nodiscard]] std::string GetDefaultState() const;

    /**
     * @brief Rename state
     */
    bool RenameState(const std::string& id, const std::string& newName);

    /**
     * @brief Duplicate state
     */
    BlendStateNode* DuplicateState(const std::string& id);

    // =========================================================================
    // Transition Management
    // =========================================================================

    /**
     * @brief Add transition
     */
    StateTransitionConnection* AddTransition(const std::string& fromState, const std::string& toState);

    /**
     * @brief Remove transition
     */
    void RemoveTransition(const std::string& id);

    /**
     * @brief Get transition by ID
     */
    [[nodiscard]] StateTransitionConnection* GetTransition(const std::string& id);

    /**
     * @brief Get all transitions
     */
    [[nodiscard]] const std::vector<StateTransitionConnection>& GetTransitions() const {
        return m_transitions;
    }

    /**
     * @brief Get transitions from state
     */
    [[nodiscard]] std::vector<StateTransitionConnection*> GetTransitionsFromState(const std::string& stateId);

    /**
     * @brief Get transitions to state
     */
    [[nodiscard]] std::vector<StateTransitionConnection*> GetTransitionsToState(const std::string& stateId);

    /**
     * @brief Add condition to transition
     */
    void AddTransitionCondition(const std::string& transitionId, const TransitionCondition& condition);

    /**
     * @brief Remove condition from transition
     */
    void RemoveTransitionCondition(const std::string& transitionId, size_t conditionIndex);

    /**
     * @brief Update transition settings
     */
    void UpdateTransition(const std::string& id, float duration, float exitTime,
                          bool hasExitTime, bool canTransitionToSelf);

    // =========================================================================
    // Parameter Management
    // =========================================================================

    /**
     * @brief Add parameter
     */
    AnimationParameter* AddParameter(const std::string& name, const std::string& type);

    /**
     * @brief Remove parameter
     */
    void RemoveParameter(const std::string& name);

    /**
     * @brief Get parameter
     */
    [[nodiscard]] AnimationParameter* GetParameter(const std::string& name);

    /**
     * @brief Get all parameters
     */
    [[nodiscard]] const std::vector<AnimationParameter>& GetParameters() const { return m_parameters; }

    /**
     * @brief Set parameter value (for testing)
     */
    void SetParameterValue(const std::string& name, float value);
    void SetParameterValue(const std::string& name, int value);
    void SetParameterValue(const std::string& name, bool value);

    /**
     * @brief Trigger a trigger parameter
     */
    void TriggerParameter(const std::string& name);

    // =========================================================================
    // Blend Tree Configuration
    // =========================================================================

    /**
     * @brief Get blend tree config for state
     */
    [[nodiscard]] BlendTreeConfig* GetBlendTreeConfig(const std::string& stateId);

    /**
     * @brief Set blend tree config
     */
    void SetBlendTreeConfig(const std::string& stateId, const BlendTreeConfig& config);

    /**
     * @brief Add child to blend tree
     */
    void AddBlendTreeChild(const std::string& stateId, const BlendTreeChild& child);

    /**
     * @brief Remove child from blend tree
     */
    void RemoveBlendTreeChild(const std::string& stateId, size_t index);

    /**
     * @brief Update blend tree child
     */
    void UpdateBlendTreeChild(const std::string& stateId, size_t index, const BlendTreeChild& child);

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select state
     */
    void SelectState(const std::string& id);

    /**
     * @brief Select transition
     */
    void SelectTransition(const std::string& id);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected state
     */
    [[nodiscard]] std::string GetSelectedState() const { return m_selectedState; }

    /**
     * @brief Get selected transition
     */
    [[nodiscard]] std::string GetSelectedTransition() const { return m_selectedTransition; }

    /**
     * @brief Multi-select states
     */
    void AddToSelection(const std::string& stateId);
    void RemoveFromSelection(const std::string& stateId);

    [[nodiscard]] const std::vector<std::string>& GetMultiSelection() const { return m_multiSelection; }

    // =========================================================================
    // View Control
    // =========================================================================

    /**
     * @brief Set view offset
     */
    void SetViewOffset(const glm::vec2& offset) { m_viewOffset = offset; }

    /**
     * @brief Get view offset
     */
    [[nodiscard]] const glm::vec2& GetViewOffset() const { return m_viewOffset; }

    /**
     * @brief Set zoom
     */
    void SetZoom(float zoom);

    /**
     * @brief Get zoom
     */
    [[nodiscard]] float GetZoom() const { return m_zoom; }

    /**
     * @brief Zoom to fit all nodes
     */
    void ZoomToFit();

    /**
     * @brief Center on state
     */
    void CenterOnState(const std::string& id);

    // =========================================================================
    // Testing / Preview
    // =========================================================================

    /**
     * @brief Start test mode
     */
    void StartTestMode();

    /**
     * @brief Stop test mode
     */
    void StopTestMode();

    /**
     * @brief Is in test mode
     */
    [[nodiscard]] bool IsTestMode() const { return m_testMode; }

    /**
     * @brief Update test mode
     */
    void UpdateTestMode(float deltaTime);

    /**
     * @brief Get current test state
     */
    [[nodiscard]] std::string GetCurrentTestState() const { return m_currentTestState; }

    /**
     * @brief Calculate blend weights for current parameters
     */
    [[nodiscard]] std::unordered_map<std::string, float> CalculateBlendWeights(const std::string& stateId) const;

    // =========================================================================
    // Layout
    // =========================================================================

    /**
     * @brief Auto-arrange states
     */
    void AutoLayout();

    /**
     * @brief Align selected states
     */
    void AlignSelected(const std::string& alignment);

    /**
     * @brief Distribute selected states
     */
    void DistributeSelected(const std::string& direction);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnStateSelected;
    std::function<void(const std::string&)> OnTransitionSelected;
    std::function<void()> OnModified;
    std::function<void(const std::string&)> OnTestStateChanged;

    // =========================================================================
    // Dirty State
    // =========================================================================

    [[nodiscard]] bool IsDirty() const { return m_dirty; }
    void ClearDirty() { m_dirty = false; }

private:
    std::string GenerateId() const;
    void MarkDirty();
    bool EvaluateCondition(const TransitionCondition& condition) const;
    bool EvaluateTransition(const StateTransitionConnection& transition) const;
    glm::vec2 SnapToGrid(const glm::vec2& pos) const;

    Config m_config;
    std::string m_name = "NewStateMachine";
    std::string m_filePath;

    // States and transitions
    std::vector<BlendStateNode> m_states;
    std::vector<StateTransitionConnection> m_transitions;
    std::vector<AnimationParameter> m_parameters;
    std::unordered_map<std::string, BlendTreeConfig> m_blendTrees;

    // Selection
    std::string m_selectedState;
    std::string m_selectedTransition;
    std::vector<std::string> m_multiSelection;

    // View
    glm::vec2 m_viewOffset{0.0f};
    float m_zoom = 1.0f;

    // Test mode
    bool m_testMode = false;
    std::string m_currentTestState;
    float m_testStateTime = 0.0f;
    std::string m_pendingTransition;
    float m_transitionProgress = 0.0f;

    bool m_dirty = false;
    bool m_initialized = false;
};

} // namespace Vehement
