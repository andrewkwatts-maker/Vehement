#pragma once

#include "engine/animation/AnimationStateMachine.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Vehement {

using Nova::json;
using Nova::AnimationState;
using Nova::StateTransition;
using Nova::AnimationEvent;
using Nova::AnimationParameter;
using Nova::DataDrivenStateMachine;

/**
 * @brief Visual node in the state machine editor
 */
struct StateNode {
    std::string stateName;
    glm::vec2 position{0.0f};
    glm::vec2 size{150.0f, 60.0f};
    bool selected = false;
    bool isDefault = false;
    uint32_t color = 0x4488FFFF;
};

/**
 * @brief Visual connection in the state machine editor
 */
struct TransitionConnection {
    std::string fromState;
    std::string toState;
    std::string condition;
    bool selected = false;
    glm::vec2 controlPoint{0.0f};  // For curved lines
};

/**
 * @brief Editor action for undo/redo
 */
struct EditorAction {
    enum class Type {
        AddState,
        RemoveState,
        MoveState,
        ModifyState,
        AddTransition,
        RemoveTransition,
        ModifyTransition,
        AddEvent,
        RemoveEvent,
        AddParameter,
        RemoveParameter
    };

    Type type;
    json beforeData;
    json afterData;
    std::string targetName;
};

/**
 * @brief Visual state machine editor
 *
 * Features:
 * - Drag-drop state nodes
 * - Transition arrows with condition labels
 * - Condition editor on transitions
 * - Event timeline per state
 * - Preview playback
 * - Undo/redo support
 */
class StateMachineEditor {
public:
    struct Config {
        glm::vec2 gridSize{20.0f};
        bool snapToGrid = true;
        bool showGrid = true;
        float zoomMin = 0.25f;
        float zoomMax = 4.0f;
        glm::vec2 canvasSize{2000.0f, 2000.0f};
    };

    StateMachineEditor();
    ~StateMachineEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Load state machine for editing
     */
    bool LoadStateMachine(const std::string& filepath);
    bool LoadStateMachine(DataDrivenStateMachine* stateMachine);

    /**
     * @brief Save state machine
     */
    bool SaveStateMachine(const std::string& filepath);
    bool SaveStateMachine();

    /**
     * @brief Create new state machine
     */
    void NewStateMachine();

    /**
     * @brief Export to JSON
     */
    [[nodiscard]] json ExportToJson() const;

    /**
     * @brief Import from JSON
     */
    bool ImportFromJson(const json& data);

    // -------------------------------------------------------------------------
    // State Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Add a new state at position
     */
    StateNode* AddState(const std::string& name, const glm::vec2& position);

    /**
     * @brief Remove state by name
     */
    bool RemoveState(const std::string& name);

    /**
     * @brief Get state node
     */
    [[nodiscard]] StateNode* GetStateNode(const std::string& name);

    /**
     * @brief Get all state nodes
     */
    [[nodiscard]] const std::vector<StateNode>& GetStateNodes() const { return m_stateNodes; }

    /**
     * @brief Set default state
     */
    void SetDefaultState(const std::string& name);

    /**
     * @brief Rename state
     */
    bool RenameState(const std::string& oldName, const std::string& newName);

    // -------------------------------------------------------------------------
    // Transition Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Add transition between states
     */
    TransitionConnection* AddTransition(const std::string& from, const std::string& to);

    /**
     * @brief Remove transition
     */
    bool RemoveTransition(const std::string& from, const std::string& to);

    /**
     * @brief Get all transitions
     */
    [[nodiscard]] const std::vector<TransitionConnection>& GetTransitions() const {
        return m_transitions;
    }

    /**
     * @brief Set transition condition
     */
    void SetTransitionCondition(const std::string& from, const std::string& to,
                                 const std::string& condition);

    // -------------------------------------------------------------------------
    // Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Select state
     */
    void SelectState(const std::string& name);

    /**
     * @brief Select transition
     */
    void SelectTransition(const std::string& from, const std::string& to);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected state name
     */
    [[nodiscard]] const std::string& GetSelectedState() const { return m_selectedState; }

    /**
     * @brief Check if state is selected
     */
    [[nodiscard]] bool IsStateSelected(const std::string& name) const {
        return m_selectedState == name;
    }

    // -------------------------------------------------------------------------
    // View Control
    // -------------------------------------------------------------------------

    /**
     * @brief Set view offset
     */
    void SetViewOffset(const glm::vec2& offset);

    /**
     * @brief Get view offset
     */
    [[nodiscard]] const glm::vec2& GetViewOffset() const { return m_viewOffset; }

    /**
     * @brief Set zoom level
     */
    void SetZoom(float zoom);

    /**
     * @brief Get zoom level
     */
    [[nodiscard]] float GetZoom() const { return m_zoom; }

    /**
     * @brief Zoom to fit all nodes
     */
    void ZoomToFit();

    /**
     * @brief Center view on state
     */
    void CenterOnState(const std::string& name);

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Handle mouse click
     */
    void OnMouseDown(const glm::vec2& position, int button);

    /**
     * @brief Handle mouse release
     */
    void OnMouseUp(const glm::vec2& position, int button);

    /**
     * @brief Handle mouse move
     */
    void OnMouseMove(const glm::vec2& position);

    /**
     * @brief Handle key press
     */
    void OnKeyDown(int key);

    // -------------------------------------------------------------------------
    // Undo/Redo
    // -------------------------------------------------------------------------

    /**
     * @brief Undo last action
     */
    void Undo();

    /**
     * @brief Redo last undone action
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    /**
     * @brief Auto-arrange states
     */
    void AutoLayout(const std::string& algorithm = "hierarchical");

    /**
     * @brief Align selected states
     */
    void AlignStates(const std::string& alignment);

    // -------------------------------------------------------------------------
    // Preview
    // -------------------------------------------------------------------------

    /**
     * @brief Start preview playback
     */
    void StartPreview();

    /**
     * @brief Stop preview playback
     */
    void StopPreview();

    /**
     * @brief Update preview
     */
    void UpdatePreview(float deltaTime);

    /**
     * @brief Check if preview is playing
     */
    [[nodiscard]] bool IsPreviewPlaying() const { return m_previewPlaying; }

    /**
     * @brief Set preview parameter
     */
    void SetPreviewParameter(const std::string& name, float value);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /**
     * @brief Callback when selection changes
     */
    std::function<void(const std::string&)> OnSelectionChanged;

    /**
     * @brief Callback when state machine is modified
     */
    std::function<void()> OnModified;

    /**
     * @brief Callback to request state details panel update
     */
    std::function<void(const AnimationState*)> OnStateSelected;

    /**
     * @brief Callback to request transition details panel update
     */
    std::function<void(const StateTransition*)> OnTransitionSelected;

    // -------------------------------------------------------------------------
    // Dirty State
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsDirty() const { return m_dirty; }
    void ClearDirty() { m_dirty = false; }

private:
    void RecordAction(EditorAction::Type type, const std::string& target,
                      const json& before, const json& after);
    StateNode* FindNodeAt(const glm::vec2& position);
    TransitionConnection* FindTransitionAt(const glm::vec2& position);
    glm::vec2 ScreenToWorld(const glm::vec2& screenPos) const;
    glm::vec2 WorldToScreen(const glm::vec2& worldPos) const;
    glm::vec2 SnapToGrid(const glm::vec2& position) const;
    void UpdateStateFromNode(const StateNode& node);
    void AutoLayoutHierarchical();
    void AutoLayoutForceDirected();

    Config m_config;
    DataDrivenStateMachine* m_stateMachine = nullptr;
    std::string m_filePath;

    // Visual elements
    std::vector<StateNode> m_stateNodes;
    std::vector<TransitionConnection> m_transitions;

    // Selection
    std::string m_selectedState;
    std::string m_selectedTransitionFrom;
    std::string m_selectedTransitionTo;

    // View
    glm::vec2 m_viewOffset{0.0f};
    float m_zoom = 1.0f;

    // Interaction state
    bool m_dragging = false;
    bool m_panning = false;
    bool m_creatingTransition = false;
    glm::vec2 m_dragStart{0.0f};
    glm::vec2 m_dragOffset{0.0f};
    std::string m_transitionStartState;

    // Undo/Redo
    std::vector<EditorAction> m_undoStack;
    std::vector<EditorAction> m_redoStack;
    static constexpr size_t MAX_UNDO_SIZE = 100;

    // Preview
    bool m_previewPlaying = false;
    float m_previewTime = 0.0f;

    // State
    bool m_dirty = false;
    bool m_initialized = false;
};

/**
 * @brief State properties panel
 */
class StatePropertiesPanel {
public:
    StatePropertiesPanel() = default;

    /**
     * @brief Set state to edit
     */
    void SetState(AnimationState* state);

    /**
     * @brief Render panel (returns true if modified)
     */
    bool Render();

    /**
     * @brief Get modified state data
     */
    [[nodiscard]] AnimationState GetModifiedState() const;

    std::function<void(const AnimationState&)> OnStateModified;

private:
    AnimationState* m_state = nullptr;
    AnimationState m_editState;
};

/**
 * @brief Transition properties panel
 */
class TransitionPropertiesPanel {
public:
    TransitionPropertiesPanel() = default;

    /**
     * @brief Set transition to edit
     */
    void SetTransition(StateTransition* transition);

    /**
     * @brief Render panel (returns true if modified)
     */
    bool Render();

    /**
     * @brief Get modified transition data
     */
    [[nodiscard]] StateTransition GetModifiedTransition() const;

    std::function<void(const StateTransition&)> OnTransitionModified;

private:
    StateTransition* m_transition = nullptr;
    StateTransition m_editTransition;
};

} // namespace Vehement
