#pragma once

#include "EditableSkeleton.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <stack>

namespace Nova {

/**
 * @brief Keyframe data
 */
struct EditableKeyframe {
    float time = 0.0f;
    EditableBoneTransform transform;
    std::string interpolation = "linear";  // linear, bezier, step

    // Bezier tangents
    glm::vec2 inTangent{-0.1f, 0.0f};
    glm::vec2 outTangent{0.1f, 0.0f};
};

/**
 * @brief Animation event
 */
struct EditableAnimationEvent {
    float time = 0.0f;
    std::string name;
    std::string functionName;
    std::string stringParam;
    float floatParam = 0.0f;
    int intParam = 0;
};

/**
 * @brief Bone animation track
 */
struct EditableBoneTrack {
    std::string boneName;
    std::vector<EditableKeyframe> keyframes;
    bool enabled = true;
    bool locked = false;
};

/**
 * @brief Undo/redo action
 */
struct AnimationEditAction {
    enum class Type {
        AddKeyframe,
        RemoveKeyframe,
        ModifyKeyframe,
        MoveKeyframe,
        AddEvent,
        RemoveEvent,
        ModifyEvent,
        ModifyDuration,
        BatchTransform
    };

    Type type;
    std::string description;

    // Undo data
    std::string boneName;
    size_t keyframeIndex = 0;
    float time = 0.0f;
    EditableKeyframe oldKeyframe;
    EditableKeyframe newKeyframe;
    EditableAnimationEvent oldEvent;
    EditableAnimationEvent newEvent;
    float oldDuration = 0.0f;
    float newDuration = 0.0f;

    // Batch data
    std::unordered_map<std::string, std::vector<EditableKeyframe>> oldTracks;
    std::unordered_map<std::string, std::vector<EditableKeyframe>> newTracks;
};

/**
 * @brief Editable animation with undo/redo support
 *
 * Features:
 * - Keyframe data management
 * - Curve data
 * - Event data
 * - Undo/redo support
 */
class EditableAnimation {
public:
    EditableAnimation();
    ~EditableAnimation();

    // =========================================================================
    // Basic Properties
    // =========================================================================

    /**
     * @brief Set animation name
     */
    void SetName(const std::string& name) { m_name = name; MarkDirty(); }

    /**
     * @brief Get animation name
     */
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    /**
     * @brief Set duration
     */
    void SetDuration(float duration);

    /**
     * @brief Get duration
     */
    [[nodiscard]] float GetDuration() const { return m_duration; }

    /**
     * @brief Set frame rate
     */
    void SetFrameRate(float fps) { m_frameRate = fps; }

    /**
     * @brief Get frame rate
     */
    [[nodiscard]] float GetFrameRate() const { return m_frameRate; }

    /**
     * @brief Set looping
     */
    void SetLooping(bool looping) { m_looping = looping; MarkDirty(); }

    /**
     * @brief Is looping
     */
    [[nodiscard]] bool IsLooping() const { return m_looping; }

    // =========================================================================
    // Track Management
    // =========================================================================

    /**
     * @brief Add track
     */
    EditableBoneTrack* AddTrack(const std::string& boneName);

    /**
     * @brief Remove track
     */
    void RemoveTrack(const std::string& boneName);

    /**
     * @brief Get track
     */
    [[nodiscard]] EditableBoneTrack* GetTrack(const std::string& boneName);
    [[nodiscard]] const EditableBoneTrack* GetTrack(const std::string& boneName) const;

    /**
     * @brief Get all tracks
     */
    [[nodiscard]] std::vector<EditableBoneTrack>& GetTracks() { return m_tracks; }
    [[nodiscard]] const std::vector<EditableBoneTrack>& GetTracks() const { return m_tracks; }

    /**
     * @brief Clear all tracks
     */
    void ClearTracks();

    // =========================================================================
    // Keyframe Operations
    // =========================================================================

    /**
     * @brief Add keyframe
     */
    EditableKeyframe* AddKeyframe(const std::string& boneName, float time, const EditableBoneTransform& transform);

    /**
     * @brief Remove keyframe
     */
    void RemoveKeyframe(const std::string& boneName, size_t index);

    /**
     * @brief Get keyframe
     */
    [[nodiscard]] EditableKeyframe* GetKeyframe(const std::string& boneName, size_t index);

    /**
     * @brief Get keyframe at time
     */
    [[nodiscard]] EditableKeyframe* GetKeyframeAtTime(const std::string& boneName, float time, float tolerance = 0.001f);

    /**
     * @brief Move keyframe
     */
    void MoveKeyframe(const std::string& boneName, size_t index, float newTime);

    /**
     * @brief Modify keyframe
     */
    void ModifyKeyframe(const std::string& boneName, size_t index, const EditableKeyframe& newKeyframe);

    // =========================================================================
    // Event Operations
    // =========================================================================

    /**
     * @brief Add event
     */
    EditableAnimationEvent* AddEvent(float time, const std::string& name);

    /**
     * @brief Remove event
     */
    void RemoveEvent(size_t index);

    /**
     * @brief Get event
     */
    [[nodiscard]] EditableAnimationEvent* GetEvent(size_t index);

    /**
     * @brief Get all events
     */
    [[nodiscard]] std::vector<EditableAnimationEvent>& GetEvents() { return m_events; }
    [[nodiscard]] const std::vector<EditableAnimationEvent>& GetEvents() const { return m_events; }

    // =========================================================================
    // Evaluation
    // =========================================================================

    /**
     * @brief Evaluate transform at time
     */
    [[nodiscard]] EditableBoneTransform EvaluateTransform(const std::string& boneName, float time) const;

    /**
     * @brief Evaluate all transforms at time
     */
    [[nodiscard]] std::unordered_map<std::string, EditableBoneTransform> EvaluateAllTransforms(float time) const;

    /**
     * @brief Get events in time range
     */
    [[nodiscard]] std::vector<const EditableAnimationEvent*> GetEventsInRange(float start, float end) const;

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    /**
     * @brief Undo last action
     */
    void Undo();

    /**
     * @brief Redo last undone action
     */
    void Redo();

    /**
     * @brief Can undo
     */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Can redo
     */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    /**
     * @brief Clear history
     */
    void ClearHistory();

    /**
     * @brief Begin action group
     */
    void BeginActionGroup(const std::string& description);

    /**
     * @brief End action group
     */
    void EndActionGroup();

    // =========================================================================
    // State
    // =========================================================================

    /**
     * @brief Is dirty (has unsaved changes)
     */
    [[nodiscard]] bool IsDirty() const { return m_dirty; }

    /**
     * @brief Clear dirty flag
     */
    void ClearDirty() { m_dirty = false; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnModified;
    std::function<void(const std::string&, size_t)> OnKeyframeAdded;
    std::function<void(const std::string&, size_t)> OnKeyframeRemoved;
    std::function<void(const std::string&, size_t)> OnKeyframeModified;
    std::function<void(size_t)> OnEventAdded;
    std::function<void(size_t)> OnEventRemoved;

private:
    void RecordAction(const AnimationEditAction& action);
    void ApplyAction(const AnimationEditAction& action);
    void UnapplyAction(const AnimationEditAction& action);
    void MarkDirty();
    void SortKeyframes(EditableBoneTrack& track);
    void SortEvents();
    EditableBoneTransform InterpolateKeyframes(const EditableKeyframe& a, const EditableKeyframe& b, float t) const;

    std::string m_name = "Untitled";
    float m_duration = 1.0f;
    float m_frameRate = 30.0f;
    bool m_looping = true;

    std::vector<EditableBoneTrack> m_tracks;
    std::vector<EditableAnimationEvent> m_events;

    // Undo/redo
    std::stack<AnimationEditAction> m_undoStack;
    std::stack<AnimationEditAction> m_redoStack;
    static constexpr size_t MAX_UNDO_SIZE = 100;

    // Group actions
    bool m_inActionGroup = false;
    AnimationEditAction m_groupAction;

    bool m_dirty = false;
};

} // namespace Nova
