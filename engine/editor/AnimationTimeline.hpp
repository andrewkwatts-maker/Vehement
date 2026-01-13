/**
 * @file AnimationTimeline.hpp
 * @brief Animation Timeline Editor for Nova3D/Vehement Engine
 *
 * Production-quality animation timeline editor with:
 * - Multi-track keyframe editing (Transform, Property, Event, SDF Morph)
 * - Dopesheet and curve editor views
 * - Playback controls with variable speed
 * - Auto-key and manual keyframe insertion
 * - Full undo/redo support
 * - Animation clip management and layers
 *
 * @section architecture Architecture
 *
 * The timeline follows SOLID principles:
 * - Single Responsibility: Each track type handles its own keyframe logic
 * - Open/Closed: New track types can be added via IAnimationTrack interface
 * - Liskov Substitution: All track types are interchangeable in the UI
 * - Interface Segregation: Separate interfaces for editing vs playback
 * - Dependency Inversion: Timeline depends on abstractions, not concrete animations
 *
 * @see Animation.hpp for animation data structures
 * @see SDFAnimation.hpp for SDF-specific animation support
 * @see EditorCommand.hpp for undo/redo system
 */

#pragma once

#include "../ui/EditorPanel.hpp"
#include "../animation/Animation.hpp"
#include "../sdf/SDFAnimation.hpp"
#include "EditorCommand.hpp"
#include "CommandHistory.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <optional>
#include <chrono>

namespace Nova {

// Forward declarations
class AnimationTimeline;
class IAnimationTrack;
class AnimationClipAsset;

// =============================================================================
// Keyframe Types
// =============================================================================

/**
 * @brief Interpolation mode for individual keyframes
 */
enum class KeyframeInterpolation : uint8_t {
    Linear,     ///< Linear interpolation between keyframes
    Bezier,     ///< Bezier curve with tangent handles
    Step,       ///< Instant jump to next value (no interpolation)
    Smooth      ///< Automatic smooth tangents (Catmull-Rom style)
};

/**
 * @brief Tangent mode for bezier keyframes
 */
enum class TangentMode : uint8_t {
    Free,       ///< Tangents can be adjusted independently
    Aligned,    ///< Tangents are aligned (same direction, different lengths)
    Mirrored,   ///< Tangents are mirrored (same direction and length)
    Flat,       ///< Tangents are horizontal
    Auto        ///< Tangents are auto-calculated for smoothness
};

/**
 * @brief Bezier tangent handle for curve editing
 */
struct BezierTangent {
    glm::vec2 inTangent{-0.1f, 0.0f};   ///< Incoming tangent (time, value)
    glm::vec2 outTangent{0.1f, 0.0f};   ///< Outgoing tangent (time, value)
    TangentMode mode = TangentMode::Auto;
};

/**
 * @brief Generic keyframe value that can hold different types
 */
using KeyframeValue = std::variant<
    float,              // Single float value
    glm::vec2,          // 2D vector
    glm::vec3,          // 3D vector (position, scale, euler)
    glm::vec4,          // 4D vector (color with alpha)
    glm::quat,          // Quaternion rotation
    bool,               // Boolean for events
    std::string         // String for event names
>;

/**
 * @brief A single keyframe in the animation timeline
 */
struct TimelineKeyframe {
    float time = 0.0f;                              ///< Time in seconds
    KeyframeValue value;                            ///< Keyframe value
    KeyframeInterpolation interpolation = KeyframeInterpolation::Linear;
    BezierTangent tangent;                          ///< Tangent handles for bezier mode

    // UI state (not serialized)
    bool selected = false;
    bool hovered = false;

    bool operator<(const TimelineKeyframe& other) const {
        return time < other.time;
    }

    bool operator<(float t) const {
        return time < t;
    }
};

// =============================================================================
// Animation Track Interface
// =============================================================================

/**
 * @brief Type identifier for tracks
 */
enum class TrackType : uint8_t {
    Transform,      ///< Position, rotation, scale
    Property,       ///< Generic property (float, vec3, color, etc.)
    Event,          ///< Trigger events at specific times
    SDFMorph        ///< SDF shape morphing parameters
};

/**
 * @brief Abstract base class for animation tracks
 *
 * Each track manages keyframes for a single property or group of related
 * properties. Tracks are polymorphic to support different value types and
 * interpolation behaviors.
 */
class IAnimationTrack {
public:
    virtual ~IAnimationTrack() = default;

    // ==========================================================================
    // Identity
    // ==========================================================================

    /**
     * @brief Get track type identifier
     */
    [[nodiscard]] virtual TrackType GetType() const = 0;

    /**
     * @brief Get track display name
     */
    [[nodiscard]] virtual const std::string& GetName() const = 0;

    /**
     * @brief Set track display name
     */
    virtual void SetName(const std::string& name) = 0;

    /**
     * @brief Get target object/node identifier
     */
    [[nodiscard]] virtual const std::string& GetTargetId() const = 0;

    /**
     * @brief Set target object/node identifier
     */
    virtual void SetTargetId(const std::string& id) = 0;

    /**
     * @brief Get target property path (e.g., "transform.position.x")
     */
    [[nodiscard]] virtual const std::string& GetPropertyPath() const = 0;

    // ==========================================================================
    // Keyframe Management
    // ==========================================================================

    /**
     * @brief Get number of keyframes
     */
    [[nodiscard]] virtual size_t GetKeyframeCount() const = 0;

    /**
     * @brief Get keyframe at index
     */
    [[nodiscard]] virtual TimelineKeyframe* GetKeyframe(size_t index) = 0;
    [[nodiscard]] virtual const TimelineKeyframe* GetKeyframe(size_t index) const = 0;

    /**
     * @brief Get all keyframes (read-only)
     */
    [[nodiscard]] virtual const std::vector<TimelineKeyframe>& GetKeyframes() const = 0;

    /**
     * @brief Add a keyframe at the specified time
     * @param time Time in seconds
     * @param value Keyframe value
     * @return Index of the added keyframe
     */
    virtual size_t AddKeyframe(float time, const KeyframeValue& value) = 0;

    /**
     * @brief Remove keyframe at index
     */
    virtual void RemoveKeyframe(size_t index) = 0;

    /**
     * @brief Remove keyframe at or near time
     */
    virtual bool RemoveKeyframeAtTime(float time, float tolerance = 0.001f) = 0;

    /**
     * @brief Move keyframe to new time
     */
    virtual void MoveKeyframe(size_t index, float newTime) = 0;

    /**
     * @brief Set keyframe value
     */
    virtual void SetKeyframeValue(size_t index, const KeyframeValue& value) = 0;

    /**
     * @brief Set keyframe interpolation mode
     */
    virtual void SetKeyframeInterpolation(size_t index, KeyframeInterpolation interp) = 0;

    /**
     * @brief Find keyframe at or near time
     * @return Index or nullopt if not found
     */
    [[nodiscard]] virtual std::optional<size_t> FindKeyframeAtTime(float time, float tolerance = 0.001f) const = 0;

    /**
     * @brief Get keyframes in time range
     */
    [[nodiscard]] virtual std::vector<size_t> GetKeyframesInRange(float startTime, float endTime) const = 0;

    // ==========================================================================
    // Evaluation
    // ==========================================================================

    /**
     * @brief Evaluate track at time
     * @param time Time in seconds
     * @return Interpolated value at time
     */
    [[nodiscard]] virtual KeyframeValue Evaluate(float time) const = 0;

    /**
     * @brief Get track duration (time of last keyframe)
     */
    [[nodiscard]] virtual float GetDuration() const = 0;

    // ==========================================================================
    // Track State
    // ==========================================================================

    /**
     * @brief Check if track is muted (skipped during playback)
     */
    [[nodiscard]] virtual bool IsMuted() const = 0;
    virtual void SetMuted(bool muted) = 0;

    /**
     * @brief Check if track is locked (prevents editing)
     */
    [[nodiscard]] virtual bool IsLocked() const = 0;
    virtual void SetLocked(bool locked) = 0;

    /**
     * @brief Check if track is solo (only this track plays)
     */
    [[nodiscard]] virtual bool IsSolo() const = 0;
    virtual void SetSolo(bool solo) = 0;

    /**
     * @brief Check if track is expanded in UI
     */
    [[nodiscard]] virtual bool IsExpanded() const = 0;
    virtual void SetExpanded(bool expanded) = 0;

    // ==========================================================================
    // UI Color
    // ==========================================================================

    /**
     * @brief Get track color for UI display
     */
    [[nodiscard]] virtual glm::vec4 GetColor() const = 0;
    virtual void SetColor(const glm::vec4& color) = 0;

    // ==========================================================================
    // Cloning
    // ==========================================================================

    /**
     * @brief Create a deep copy of this track
     */
    [[nodiscard]] virtual std::unique_ptr<IAnimationTrack> Clone() const = 0;
};

// =============================================================================
// Concrete Track Implementations
// =============================================================================

/**
 * @brief Base implementation with common track functionality
 */
class AnimationTrackBase : public IAnimationTrack {
public:
    AnimationTrackBase(const std::string& name, const std::string& targetId);
    ~AnimationTrackBase() override = default;

    // Identity
    [[nodiscard]] const std::string& GetName() const override { return m_name; }
    void SetName(const std::string& name) override { m_name = name; }
    [[nodiscard]] const std::string& GetTargetId() const override { return m_targetId; }
    void SetTargetId(const std::string& id) override { m_targetId = id; }
    [[nodiscard]] const std::string& GetPropertyPath() const override { return m_propertyPath; }

    // Keyframe management
    [[nodiscard]] size_t GetKeyframeCount() const override { return m_keyframes.size(); }
    [[nodiscard]] TimelineKeyframe* GetKeyframe(size_t index) override;
    [[nodiscard]] const TimelineKeyframe* GetKeyframe(size_t index) const override;
    [[nodiscard]] const std::vector<TimelineKeyframe>& GetKeyframes() const override { return m_keyframes; }

    size_t AddKeyframe(float time, const KeyframeValue& value) override;
    void RemoveKeyframe(size_t index) override;
    bool RemoveKeyframeAtTime(float time, float tolerance = 0.001f) override;
    void MoveKeyframe(size_t index, float newTime) override;
    void SetKeyframeValue(size_t index, const KeyframeValue& value) override;
    void SetKeyframeInterpolation(size_t index, KeyframeInterpolation interp) override;
    [[nodiscard]] std::optional<size_t> FindKeyframeAtTime(float time, float tolerance = 0.001f) const override;
    [[nodiscard]] std::vector<size_t> GetKeyframesInRange(float startTime, float endTime) const override;

    [[nodiscard]] float GetDuration() const override;

    // Track state
    [[nodiscard]] bool IsMuted() const override { return m_muted; }
    void SetMuted(bool muted) override { m_muted = muted; }
    [[nodiscard]] bool IsLocked() const override { return m_locked; }
    void SetLocked(bool locked) override { m_locked = locked; }
    [[nodiscard]] bool IsSolo() const override { return m_solo; }
    void SetSolo(bool solo) override { m_solo = solo; }
    [[nodiscard]] bool IsExpanded() const override { return m_expanded; }
    void SetExpanded(bool expanded) override { m_expanded = expanded; }

    // Color
    [[nodiscard]] glm::vec4 GetColor() const override { return m_color; }
    void SetColor(const glm::vec4& color) override { m_color = color; }

protected:
    void SetPropertyPath(const std::string& path) { m_propertyPath = path; }
    void SortKeyframes();

    std::string m_name;
    std::string m_targetId;
    std::string m_propertyPath;
    std::vector<TimelineKeyframe> m_keyframes;
    glm::vec4 m_color{0.6f, 0.6f, 0.9f, 1.0f};

    bool m_muted = false;
    bool m_locked = false;
    bool m_solo = false;
    bool m_expanded = true;
};

/**
 * @brief Track for transform animation (position, rotation, scale)
 */
class TransformTrack : public AnimationTrackBase {
public:
    enum class Component : uint8_t {
        Position,
        Rotation,
        Scale,
        All         ///< Combined transform
    };

    TransformTrack(const std::string& name, const std::string& targetId, Component component = Component::All);

    [[nodiscard]] TrackType GetType() const override { return TrackType::Transform; }
    [[nodiscard]] KeyframeValue Evaluate(float time) const override;
    [[nodiscard]] std::unique_ptr<IAnimationTrack> Clone() const override;

    [[nodiscard]] Component GetComponent() const { return m_component; }

    /**
     * @brief Add transform keyframe
     */
    size_t AddTransformKeyframe(float time, const glm::vec3& position,
                                 const glm::quat& rotation, const glm::vec3& scale);

    /**
     * @brief Evaluate to get full transform
     */
    [[nodiscard]] Keyframe EvaluateTransform(float time) const;

private:
    Component m_component;
};

/**
 * @brief Track for generic property animation
 */
class PropertyTrack : public AnimationTrackBase {
public:
    enum class ValueType : uint8_t {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Color,
        Quaternion
    };

    PropertyTrack(const std::string& name, const std::string& targetId,
                  const std::string& propertyPath, ValueType valueType);

    [[nodiscard]] TrackType GetType() const override { return TrackType::Property; }
    [[nodiscard]] KeyframeValue Evaluate(float time) const override;
    [[nodiscard]] std::unique_ptr<IAnimationTrack> Clone() const override;

    [[nodiscard]] ValueType GetValueType() const { return m_valueType; }

    // Value clamping
    void SetValueRange(float min, float max) { m_minValue = min; m_maxValue = max; m_hasRange = true; }
    void ClearValueRange() { m_hasRange = false; }
    [[nodiscard]] bool HasValueRange() const { return m_hasRange; }
    [[nodiscard]] float GetMinValue() const { return m_minValue; }
    [[nodiscard]] float GetMaxValue() const { return m_maxValue; }

private:
    ValueType m_valueType;
    float m_minValue = 0.0f;
    float m_maxValue = 1.0f;
    bool m_hasRange = false;
};

/**
 * @brief Track for triggering events at specific times
 */
class EventTrack : public AnimationTrackBase {
public:
    struct AnimationEvent {
        std::string name;
        std::string parameter;  ///< Optional parameter string
    };

    EventTrack(const std::string& name, const std::string& targetId);

    [[nodiscard]] TrackType GetType() const override { return TrackType::Event; }
    [[nodiscard]] KeyframeValue Evaluate(float time) const override;
    [[nodiscard]] std::unique_ptr<IAnimationTrack> Clone() const override;

    /**
     * @brief Add event at time
     */
    size_t AddEvent(float time, const std::string& eventName, const std::string& parameter = "");

    /**
     * @brief Get events that fire between two times
     */
    [[nodiscard]] std::vector<AnimationEvent> GetEventsInRange(float startTime, float endTime) const;

    // Event callback
    std::function<void(const AnimationEvent&)> OnEventFired;

private:
    std::vector<AnimationEvent> m_events;
};

/**
 * @brief Track for SDF shape morphing animation
 */
class SDFMorphTrack : public AnimationTrackBase {
public:
    enum class SDFParameter : uint8_t {
        BlendFactor,    ///< CSG blend amount
        Radius,         ///< Shape radius/size
        Rounding,       ///< Edge rounding
        Displacement,   ///< Displacement amount
        Custom          ///< Custom parameter by name
    };

    SDFMorphTrack(const std::string& name, const std::string& targetId, SDFParameter param);
    SDFMorphTrack(const std::string& name, const std::string& targetId, const std::string& customParam);

    [[nodiscard]] TrackType GetType() const override { return TrackType::SDFMorph; }
    [[nodiscard]] KeyframeValue Evaluate(float time) const override;
    [[nodiscard]] std::unique_ptr<IAnimationTrack> Clone() const override;

    [[nodiscard]] SDFParameter GetParameter() const { return m_parameter; }
    [[nodiscard]] const std::string& GetCustomParameterName() const { return m_customParamName; }

    /**
     * @brief Apply evaluated value to SDF model
     */
    void ApplyToModel(class SDFModel* model, float time) const;

private:
    SDFParameter m_parameter;
    std::string m_customParamName;
};

// =============================================================================
// Track Group
// =============================================================================

/**
 * @brief Group of tracks for organizing by object
 */
struct TrackGroup {
    std::string name;
    std::string objectId;
    std::vector<std::shared_ptr<IAnimationTrack>> tracks;
    bool expanded = true;
    bool locked = false;
    glm::vec4 color{0.4f, 0.4f, 0.4f, 1.0f};
};

// =============================================================================
// Animation Clip
// =============================================================================

/**
 * @brief An animation clip containing multiple tracks
 */
class TimelineAnimationClip {
public:
    TimelineAnimationClip() = default;
    explicit TimelineAnimationClip(const std::string& name);

    // Properties
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] float GetDuration() const { return m_duration; }
    void SetDuration(float duration) { m_duration = duration; }

    [[nodiscard]] float GetFrameRate() const { return m_frameRate; }
    void SetFrameRate(float fps) { m_frameRate = fps; }

    [[nodiscard]] bool IsLooping() const { return m_looping; }
    void SetLooping(bool looping) { m_looping = looping; }

    // Track management
    void AddTrack(std::shared_ptr<IAnimationTrack> track);
    void RemoveTrack(size_t index);
    void RemoveTrack(const std::string& name);
    void MoveTrack(size_t fromIndex, size_t toIndex);

    [[nodiscard]] size_t GetTrackCount() const { return m_tracks.size(); }
    [[nodiscard]] IAnimationTrack* GetTrack(size_t index);
    [[nodiscard]] const IAnimationTrack* GetTrack(size_t index) const;
    [[nodiscard]] IAnimationTrack* GetTrack(const std::string& name);
    [[nodiscard]] const std::vector<std::shared_ptr<IAnimationTrack>>& GetTracks() const { return m_tracks; }

    // Group management
    void AddGroup(const TrackGroup& group);
    void RemoveGroup(size_t index);
    [[nodiscard]] size_t GetGroupCount() const { return m_groups.size(); }
    [[nodiscard]] TrackGroup* GetGroup(size_t index);
    [[nodiscard]] const std::vector<TrackGroup>& GetGroups() const { return m_groups; }

    // Utility
    void RecalculateDuration();
    [[nodiscard]] std::unique_ptr<TimelineAnimationClip> Clone() const;

    // Conversion to/from engine Animation
    [[nodiscard]] Animation ToAnimation() const;
    void FromAnimation(const Animation& animation);

    // Conversion to/from SDF animation
    [[nodiscard]] SDFAnimationClip ToSDFAnimation() const;
    void FromSDFAnimation(const SDFAnimationClip& sdfAnim);

private:
    std::string m_name = "New Animation";
    float m_duration = 1.0f;
    float m_frameRate = 30.0f;
    bool m_looping = true;

    std::vector<std::shared_ptr<IAnimationTrack>> m_tracks;
    std::vector<TrackGroup> m_groups;
};

// =============================================================================
// Animation Layer
// =============================================================================

/**
 * @brief Animation layer for timeline blending multiple clips
 * @note Named TimelineAnimationLayer to avoid conflict with Nova::AnimationLayer in Animation.hpp
 */
struct TimelineAnimationLayer {
    std::string name;
    std::shared_ptr<TimelineAnimationClip> clip;
    float weight = 1.0f;
    BlendMode blendMode = BlendMode::Override;
    bool active = true;
    std::vector<std::string> boneMask;  ///< Empty = affect all
};

// =============================================================================
// Editor Commands for Undo/Redo
// =============================================================================

/**
 * @brief Command for adding a keyframe
 */
class AddKeyframeCommand : public ICommand {
public:
    AddKeyframeCommand(AnimationTimeline* timeline, IAnimationTrack* track,
                       float time, const KeyframeValue& value);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override { return "Add Keyframe"; }
    [[nodiscard]] CommandTypeId GetTypeId() const override { return GetCommandTypeId<AddKeyframeCommand>(); }

private:
    AnimationTimeline* m_timeline;
    IAnimationTrack* m_track;
    float m_time;
    KeyframeValue m_value;
    size_t m_keyframeIndex = 0;
};

/**
 * @brief Command for removing a keyframe
 */
class RemoveKeyframeCommand : public ICommand {
public:
    RemoveKeyframeCommand(AnimationTimeline* timeline, IAnimationTrack* track, size_t keyframeIndex);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override { return "Remove Keyframe"; }
    [[nodiscard]] CommandTypeId GetTypeId() const override { return GetCommandTypeId<RemoveKeyframeCommand>(); }

private:
    AnimationTimeline* m_timeline;
    IAnimationTrack* m_track;
    size_t m_keyframeIndex;
    TimelineKeyframe m_removedKeyframe;
};

/**
 * @brief Command for moving a keyframe in time
 */
class MoveKeyframeCommand : public ICommand {
public:
    MoveKeyframeCommand(AnimationTimeline* timeline, IAnimationTrack* track,
                        size_t keyframeIndex, float newTime);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override { return "Move Keyframe"; }
    [[nodiscard]] CommandTypeId GetTypeId() const override { return GetCommandTypeId<MoveKeyframeCommand>(); }

    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

private:
    AnimationTimeline* m_timeline;
    IAnimationTrack* m_track;
    size_t m_keyframeIndex;
    float m_oldTime;
    float m_newTime;
};

/**
 * @brief Command for changing keyframe value
 */
class ChangeKeyframeValueCommand : public ICommand {
public:
    ChangeKeyframeValueCommand(AnimationTimeline* timeline, IAnimationTrack* track,
                                size_t keyframeIndex, const KeyframeValue& newValue);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override { return "Change Keyframe Value"; }
    [[nodiscard]] CommandTypeId GetTypeId() const override { return GetCommandTypeId<ChangeKeyframeValueCommand>(); }

    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

private:
    AnimationTimeline* m_timeline;
    IAnimationTrack* m_track;
    size_t m_keyframeIndex;
    KeyframeValue m_oldValue;
    KeyframeValue m_newValue;
};

/**
 * @brief Command for adding a track
 */
class AddTrackCommand : public ICommand {
public:
    AddTrackCommand(AnimationTimeline* timeline, std::shared_ptr<IAnimationTrack> track);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override { return "Add Track"; }
    [[nodiscard]] CommandTypeId GetTypeId() const override { return GetCommandTypeId<AddTrackCommand>(); }

private:
    AnimationTimeline* m_timeline;
    std::shared_ptr<IAnimationTrack> m_track;
    size_t m_trackIndex = 0;
};

/**
 * @brief Command for removing a track
 */
class RemoveTrackCommand : public ICommand {
public:
    RemoveTrackCommand(AnimationTimeline* timeline, size_t trackIndex);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override { return "Remove Track"; }
    [[nodiscard]] CommandTypeId GetTypeId() const override { return GetCommandTypeId<RemoveTrackCommand>(); }

private:
    AnimationTimeline* m_timeline;
    size_t m_trackIndex;
    std::shared_ptr<IAnimationTrack> m_removedTrack;
};

/**
 * @brief Command for batch keyframe operations
 */
class BatchKeyframeCommand : public ICommand {
public:
    struct KeyframeOperation {
        IAnimationTrack* track;
        size_t index;
        TimelineKeyframe oldKeyframe;
        TimelineKeyframe newKeyframe;
        enum class Type { Move, ChangeValue, Delete, Add } type;
    };

    BatchKeyframeCommand(AnimationTimeline* timeline, const std::string& name,
                          std::vector<KeyframeOperation> operations);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override { return m_name; }
    [[nodiscard]] CommandTypeId GetTypeId() const override { return GetCommandTypeId<BatchKeyframeCommand>(); }

private:
    AnimationTimeline* m_timeline;
    std::string m_name;
    std::vector<KeyframeOperation> m_operations;
};

// =============================================================================
// Playback State
// =============================================================================

/**
 * @brief Playback state for animation preview
 */
struct PlaybackState {
    bool isPlaying = false;
    bool isLooping = true;
    float currentTime = 0.0f;
    float playbackSpeed = 1.0f;
    float startTime = 0.0f;
    float endTime = 1.0f;

    // Timing
    std::chrono::steady_clock::time_point lastUpdateTime;
};

// =============================================================================
// Selection State
// =============================================================================

/**
 * @brief Keyframe selection identifier
 */
struct KeyframeSelection {
    IAnimationTrack* track = nullptr;
    size_t keyframeIndex = 0;

    bool operator==(const KeyframeSelection& other) const {
        return track == other.track && keyframeIndex == other.keyframeIndex;
    }
};

/**
 * @brief Hash function for KeyframeSelection
 */
struct KeyframeSelectionHash {
    size_t operator()(const KeyframeSelection& sel) const {
        return std::hash<void*>()(sel.track) ^ (std::hash<size_t>()(sel.keyframeIndex) << 1);
    }
};

// =============================================================================
// Animation Timeline Panel
// =============================================================================

/**
 * @brief Main Animation Timeline editor panel
 *
 * Provides a professional animation editing interface with:
 * - Track list with hierarchy
 * - Keyframe dopesheet
 * - Curve editor (toggle)
 * - Playback controls
 * - Auto-key mode
 */
class AnimationTimeline : public EditorPanel {
public:
    AnimationTimeline();
    ~AnimationTimeline() override;

    // Non-copyable
    AnimationTimeline(const AnimationTimeline&) = delete;
    AnimationTimeline& operator=(const AnimationTimeline&) = delete;

    // ==========================================================================
    // EditorPanel Interface
    // ==========================================================================

    bool Initialize(const Config& config) override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    void OnUndo() override;
    void OnRedo() override;
    [[nodiscard]] bool CanUndo() const override;
    [[nodiscard]] bool CanRedo() const override;

    // ==========================================================================
    // Animation Clip Management
    // ==========================================================================

    /**
     * @brief Create a new empty animation clip
     */
    void NewAnimation(const std::string& name = "New Animation");

    /**
     * @brief Load animation clip for editing
     */
    void SetAnimation(std::shared_ptr<TimelineAnimationClip> clip);

    /**
     * @brief Get current animation clip
     */
    [[nodiscard]] TimelineAnimationClip* GetAnimation() { return m_clip.get(); }
    [[nodiscard]] const TimelineAnimationClip* GetAnimation() const { return m_clip.get(); }

    /**
     * @brief Import animation from engine Animation
     */
    void ImportAnimation(const Animation& animation);

    /**
     * @brief Import animation from SDF animation
     */
    void ImportSDFAnimation(const SDFAnimationClip& sdfAnim);

    /**
     * @brief Export to engine Animation
     */
    [[nodiscard]] Animation ExportAnimation() const;

    /**
     * @brief Export to SDF animation
     */
    [[nodiscard]] SDFAnimationClip ExportSDFAnimation() const;

    // ==========================================================================
    // Track Management
    // ==========================================================================

    /**
     * @brief Add a new track
     */
    void AddTrack(std::shared_ptr<IAnimationTrack> track);

    /**
     * @brief Remove track by index
     */
    void RemoveTrack(size_t index);

    /**
     * @brief Get selected track
     */
    [[nodiscard]] IAnimationTrack* GetSelectedTrack();

    // ==========================================================================
    // Keyframe Operations
    // ==========================================================================

    /**
     * @brief Set keyframe at current time for selected track
     */
    void SetKeyframe();

    /**
     * @brief Set keyframe at time for track
     */
    void SetKeyframe(IAnimationTrack* track, float time, const KeyframeValue& value);

    /**
     * @brief Delete selected keyframes
     */
    void DeleteSelectedKeyframes();

    /**
     * @brief Copy selected keyframes to clipboard
     */
    void CopyKeyframes();

    /**
     * @brief Paste keyframes from clipboard at current time
     */
    void PasteKeyframes();

    /**
     * @brief Duplicate selected keyframes
     */
    void DuplicateKeyframes(float timeOffset = 0.1f);

    /**
     * @brief Select all keyframes
     */
    void SelectAllKeyframes();

    /**
     * @brief Clear keyframe selection
     */
    void ClearSelection();

    /**
     * @brief Get selected keyframe count
     */
    [[nodiscard]] size_t GetSelectedKeyframeCount() const { return m_selectedKeyframes.size(); }

    // ==========================================================================
    // Playback Controls
    // ==========================================================================

    /**
     * @brief Start/resume playback
     */
    void Play();

    /**
     * @brief Pause playback
     */
    void Pause();

    /**
     * @brief Stop playback (go to start)
     */
    void Stop();

    /**
     * @brief Toggle play/pause
     */
    void TogglePlayback();

    /**
     * @brief Step forward one frame
     */
    void StepForward();

    /**
     * @brief Step backward one frame
     */
    void StepBackward();

    /**
     * @brief Go to start of animation
     */
    void GoToStart();

    /**
     * @brief Go to end of animation
     */
    void GoToEnd();

    /**
     * @brief Go to next keyframe
     */
    void GoToNextKeyframe();

    /**
     * @brief Go to previous keyframe
     */
    void GoToPreviousKeyframe();

    /**
     * @brief Set current time
     */
    void SetCurrentTime(float time);

    /**
     * @brief Get current time
     */
    [[nodiscard]] float GetCurrentTime() const { return m_playback.currentTime; }

    /**
     * @brief Check if playing
     */
    [[nodiscard]] bool IsPlaying() const { return m_playback.isPlaying; }

    /**
     * @brief Set playback speed
     */
    void SetPlaybackSpeed(float speed) { m_playback.playbackSpeed = speed; }

    /**
     * @brief Get playback speed
     */
    [[nodiscard]] float GetPlaybackSpeed() const { return m_playback.playbackSpeed; }

    /**
     * @brief Set loop mode
     */
    void SetLooping(bool loop) { m_playback.isLooping = loop; }

    /**
     * @brief Get loop mode
     */
    [[nodiscard]] bool IsLooping() const { return m_playback.isLooping; }

    // ==========================================================================
    // Auto-Key Mode
    // ==========================================================================

    /**
     * @brief Enable/disable auto-key mode
     */
    void SetAutoKeyEnabled(bool enabled) { m_autoKeyEnabled = enabled; }

    /**
     * @brief Check if auto-key is enabled
     */
    [[nodiscard]] bool IsAutoKeyEnabled() const { return m_autoKeyEnabled; }

    /**
     * @brief Record a value change (for auto-key)
     */
    void RecordValueChange(const std::string& targetId, const std::string& propertyPath,
                           const KeyframeValue& value);

    // ==========================================================================
    // View Controls
    // ==========================================================================

    /**
     * @brief Toggle between dopesheet and curve editor
     */
    void ToggleCurveEditor();

    /**
     * @brief Check if curve editor is visible
     */
    [[nodiscard]] bool IsCurveEditorVisible() const { return m_showCurveEditor; }

    /**
     * @brief Set visible time range
     */
    void SetVisibleTimeRange(float startTime, float endTime);

    /**
     * @brief Zoom to fit all keyframes
     */
    void ZoomToFit();

    /**
     * @brief Zoom to fit selection
     */
    void ZoomToSelection();

    /**
     * @brief Frame selection in view
     */
    void FrameSelection();

    // ==========================================================================
    // Command History Access
    // ==========================================================================

    /**
     * @brief Get command history for undo/redo
     */
    [[nodiscard]] CommandHistory& GetCommandHistory() { return m_commandHistory; }

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    /// Called when current time changes
    std::function<void(float)> OnTimeChanged;

    /// Called when keyframe selection changes
    std::function<void()> OnSelectionChanged;

    /// Called when animation is modified
    std::function<void()> OnAnimationModified;

    /// Called when playback state changes
    std::function<void(bool isPlaying)> OnPlaybackStateChanged;

    /// Called when event track fires an event
    std::function<void(const EventTrack::AnimationEvent&)> OnAnimationEvent;

protected:
    // ==========================================================================
    // EditorPanel Overrides
    // ==========================================================================

    void OnRender() override;
    void OnRenderToolbar() override;
    void OnRenderMenuBar() override;
    void OnRenderStatusBar() override;
    void OnInitialize() override;
    void OnShutdown() override;

private:
    // ==========================================================================
    // Rendering
    // ==========================================================================

    void RenderTrackList();
    void RenderTimelineArea();
    void RenderTimeRuler();
    void RenderPlayhead();
    void RenderTracks();
    void RenderTrack(IAnimationTrack* track, float yOffset, float trackHeight);
    void RenderKeyframe(const TimelineKeyframe& keyframe, IAnimationTrack* track,
                        size_t index, const glm::vec2& pos, float size);
    void RenderCurveEditor();
    void RenderCurveForTrack(IAnimationTrack* track, float yOffset, float height);
    void RenderBezierCurve(const TimelineKeyframe& kf1, const TimelineKeyframe& kf2,
                           float yOffset, float height, const glm::vec4& color);
    void RenderTangentHandle(const TimelineKeyframe& keyframe, IAnimationTrack* track,
                             size_t index, const glm::vec2& keyPos, float scale);
    void RenderPlaybackControls();
    void RenderTrackControls(IAnimationTrack* track, float yOffset);
    void RenderAddTrackMenu();

    // ==========================================================================
    // Input Handling
    // ==========================================================================

    void HandleInput();
    void HandleTimelineInput();
    void HandleTrackListInput();
    void HandleKeyboardShortcuts();
    void HandleMouseDrag();
    void HandleBoxSelection();
    void HandleKeyframeDrag();
    void HandleTangentDrag();

    // ==========================================================================
    // Selection
    // ==========================================================================

    void SelectKeyframe(IAnimationTrack* track, size_t index, bool addToSelection = false);
    void DeselectKeyframe(IAnimationTrack* track, size_t index);
    bool IsKeyframeSelected(IAnimationTrack* track, size_t index) const;
    void UpdateSelectionRect(const glm::vec2& start, const glm::vec2& end);

    // ==========================================================================
    // Utility
    // ==========================================================================

    float TimeToPixel(float time) const;
    float PixelToTime(float pixel) const;
    float ValueToPixel(float value, float minVal, float maxVal, float height) const;
    float PixelToValue(float pixel, float minVal, float maxVal, float height) const;
    glm::vec2 GetKeyframeScreenPos(IAnimationTrack* track, size_t index) const;
    IAnimationTrack* GetTrackAtPosition(float y) const;
    std::optional<std::pair<IAnimationTrack*, size_t>> GetKeyframeAtPosition(const glm::vec2& pos) const;

    void UpdatePlayback(float deltaTime);
    void FireEventsInRange(float startTime, float endTime);

    // ==========================================================================
    // State
    // ==========================================================================

    // Animation data
    std::shared_ptr<TimelineAnimationClip> m_clip;
    std::vector<TimelineAnimationLayer> m_layers;

    // Playback
    PlaybackState m_playback;

    // Selection
    std::unordered_set<KeyframeSelection, KeyframeSelectionHash> m_selectedKeyframes;
    int m_selectedTrackIndex = -1;

    // Clipboard
    struct ClipboardKeyframe {
        float relativeTime;
        KeyframeValue value;
        KeyframeInterpolation interpolation;
        BezierTangent tangent;
    };
    std::vector<ClipboardKeyframe> m_clipboard;

    // View state
    float m_viewStartTime = 0.0f;
    float m_viewEndTime = 5.0f;
    float m_zoom = 1.0f;
    float m_scrollX = 0.0f;
    float m_scrollY = 0.0f;
    float m_trackListWidth = 200.0f;
    float m_trackHeight = 24.0f;
    float m_rulerHeight = 24.0f;

    // UI state
    bool m_showCurveEditor = false;
    bool m_autoKeyEnabled = false;
    bool m_snapToFrames = true;
    bool m_showFrameNumbers = false;  // false = show time in seconds

    // Drag state
    enum class DragMode {
        None,
        Playhead,
        Keyframe,
        BoxSelect,
        Tangent,
        Pan,
        Zoom,
        TrackResize
    };
    DragMode m_dragMode = DragMode::None;
    glm::vec2 m_dragStart{0.0f};
    glm::vec2 m_dragCurrent{0.0f};
    IAnimationTrack* m_dragTrack = nullptr;
    size_t m_dragKeyframeIndex = 0;
    bool m_draggingInTangent = false;  // false = out tangent

    // Box selection
    bool m_isBoxSelecting = false;
    glm::vec2 m_boxSelectStart{0.0f};
    glm::vec2 m_boxSelectEnd{0.0f};

    // Timeline geometry (cached)
    glm::vec2 m_timelineAreaPos{0.0f};
    glm::vec2 m_timelineAreaSize{0.0f};
    glm::vec2 m_trackAreaPos{0.0f};
    glm::vec2 m_trackAreaSize{0.0f};

    // Undo/redo
    CommandHistory m_commandHistory;

    // Auto-key tracking
    std::unordered_map<std::string, KeyframeValue> m_lastRecordedValues;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Interpolate between two keyframe values
 */
KeyframeValue InterpolateKeyframeValues(const KeyframeValue& a, const KeyframeValue& b,
                                         float t, KeyframeInterpolation mode);

/**
 * @brief Evaluate bezier curve at parameter t
 */
float EvaluateBezier(float p0, float p1, float p2, float p3, float t);

/**
 * @brief Evaluate bezier curve for 2D point
 */
glm::vec2 EvaluateBezier2D(const glm::vec2& p0, const glm::vec2& c0,
                            const glm::vec2& c1, const glm::vec2& p1, float t);

/**
 * @brief Find t parameter for bezier curve at given x (time)
 */
float FindBezierT(float x, float p0x, float c0x, float c1x, float p1x, float tolerance = 0.001f);

/**
 * @brief Convert keyframe value to string for display
 */
std::string KeyframeValueToString(const KeyframeValue& value);

/**
 * @brief Get display color for track type
 */
glm::vec4 GetTrackTypeColor(TrackType type);

} // namespace Nova
