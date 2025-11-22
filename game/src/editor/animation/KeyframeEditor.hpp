#pragma once

#include "BoneAnimationEditor.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Vehement {

/**
 * @brief Keyframe interpolation mode
 */
enum class InterpolationMode {
    Linear,
    Step,
    Bezier,
    Hermite,
    CatmullRom
};

/**
 * @brief Tangent handle type for bezier curves
 */
enum class TangentMode {
    Free,        // Independent handles
    Aligned,     // Handles aligned but can differ in length
    Mirrored,    // Handles mirrored
    Auto,        // Automatically calculated
    Flat,        // Horizontal tangents
    Linear       // Linear interpolation to neighbors
};

/**
 * @brief Bezier tangent handle
 */
struct TangentHandle {
    glm::vec2 inTangent{-0.1f, 0.0f};
    glm::vec2 outTangent{0.1f, 0.0f};
    TangentMode mode = TangentMode::Auto;
    float inWeight = 1.0f;
    float outWeight = 1.0f;
};

/**
 * @brief Single keyframe data
 */
struct Keyframe {
    float time = 0.0f;
    BoneTransform transform;
    InterpolationMode interpolation = InterpolationMode::Linear;
    TangentHandle tangent;
    bool selected = false;

    // Per-component interpolation override
    InterpolationMode positionInterp = InterpolationMode::Linear;
    InterpolationMode rotationInterp = InterpolationMode::Linear;
    InterpolationMode scaleInterp = InterpolationMode::Linear;
};

/**
 * @brief Animation curve for a single value
 */
struct AnimationCurve {
    std::string name;
    std::vector<std::pair<float, float>> keys;  // time, value pairs
    std::vector<TangentHandle> tangents;
    InterpolationMode defaultInterpolation = InterpolationMode::Linear;

    [[nodiscard]] float Evaluate(float time) const;
    void AddKey(float time, float value);
    void RemoveKey(size_t index);
    void SetKey(size_t index, float time, float value);
};

/**
 * @brief Track containing keyframes for one bone
 */
struct BoneTrack {
    std::string boneName;
    std::vector<Keyframe> keyframes;
    bool visible = true;
    bool locked = false;
    uint32_t color = 0xFF8844FF;

    // Separate curves for each component
    AnimationCurve positionX, positionY, positionZ;
    AnimationCurve rotationX, rotationY, rotationZ, rotationW;
    AnimationCurve scaleX, scaleY, scaleZ;
};

/**
 * @brief Ghost/onion skin settings
 */
struct OnionSkinSettings {
    bool enabled = false;
    int framesBefore = 3;
    int framesAfter = 3;
    float opacity = 0.3f;
    glm::vec4 beforeColor{0.5f, 0.5f, 1.0f, 0.3f};
    glm::vec4 afterColor{1.0f, 0.5f, 0.5f, 0.3f};
    bool showEveryNth = false;
    int nthFrame = 2;
};

/**
 * @brief Keyframe selection info
 */
struct KeyframeSelection {
    std::string trackName;
    size_t keyframeIndex = 0;
    bool selected = false;
};

/**
 * @brief Keyframe editor for managing animation keyframes
 *
 * Features:
 * - Timeline with frame markers
 * - Add/remove keyframes
 * - Keyframe interpolation modes
 * - Copy/paste keyframes
 * - Keyframe curves editor
 * - Ghost/onion skinning
 */
class KeyframeEditor {
public:
    struct Config {
        float defaultDuration = 1.0f;
        float frameRate = 30.0f;
        float snapThreshold = 0.01f;
        bool snapToFrames = true;
        bool showCurveEditor = true;
        glm::vec4 keyframeColor{1.0f, 0.8f, 0.2f, 1.0f};
        glm::vec4 selectedKeyframeColor{1.0f, 1.0f, 1.0f, 1.0f};
    };

    KeyframeEditor();
    ~KeyframeEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Set the bone animation editor reference
     */
    void SetBoneEditor(BoneAnimationEditor* editor) { m_boneEditor = editor; }

    // =========================================================================
    // Track Management
    // =========================================================================

    /**
     * @brief Add track for bone
     */
    BoneTrack* AddTrack(const std::string& boneName);

    /**
     * @brief Remove track
     */
    void RemoveTrack(const std::string& boneName);

    /**
     * @brief Get track by bone name
     */
    [[nodiscard]] BoneTrack* GetTrack(const std::string& boneName);
    [[nodiscard]] const BoneTrack* GetTrack(const std::string& boneName) const;

    /**
     * @brief Get all tracks
     */
    [[nodiscard]] std::vector<BoneTrack>& GetTracks() { return m_tracks; }
    [[nodiscard]] const std::vector<BoneTrack>& GetTracks() const { return m_tracks; }

    /**
     * @brief Clear all tracks
     */
    void ClearAllTracks();

    /**
     * @brief Create tracks for all bones in skeleton
     */
    void CreateTracksFromSkeleton();

    // =========================================================================
    // Keyframe Operations
    // =========================================================================

    /**
     * @brief Add keyframe at current time for bone
     */
    Keyframe* AddKeyframe(const std::string& boneName, float time);

    /**
     * @brief Add keyframe with specific transform
     */
    Keyframe* AddKeyframe(const std::string& boneName, float time, const BoneTransform& transform);

    /**
     * @brief Remove keyframe
     */
    void RemoveKeyframe(const std::string& boneName, size_t index);

    /**
     * @brief Remove keyframe at time
     */
    void RemoveKeyframeAtTime(const std::string& boneName, float time);

    /**
     * @brief Get keyframe at index
     */
    [[nodiscard]] Keyframe* GetKeyframe(const std::string& boneName, size_t index);

    /**
     * @brief Get keyframe at time (or nearest)
     */
    [[nodiscard]] Keyframe* GetKeyframeAtTime(const std::string& boneName, float time, float tolerance = 0.001f);

    /**
     * @brief Move keyframe to new time
     */
    void MoveKeyframe(const std::string& boneName, size_t index, float newTime);

    /**
     * @brief Set keyframe value
     */
    void SetKeyframeTransform(const std::string& boneName, size_t index, const BoneTransform& transform);

    /**
     * @brief Set keyframe interpolation mode
     */
    void SetKeyframeInterpolation(const std::string& boneName, size_t index, InterpolationMode mode);

    /**
     * @brief Auto-key: record current pose as keyframe
     */
    void AutoKey(float time);

    /**
     * @brief Insert keyframe for all selected bones
     */
    void InsertKeyframeForSelection(float time);

    // =========================================================================
    // Keyframe Selection
    // =========================================================================

    /**
     * @brief Select keyframe
     */
    void SelectKeyframe(const std::string& boneName, size_t index, bool addToSelection = false);

    /**
     * @brief Deselect keyframe
     */
    void DeselectKeyframe(const std::string& boneName, size_t index);

    /**
     * @brief Clear keyframe selection
     */
    void ClearKeyframeSelection();

    /**
     * @brief Select all keyframes in time range
     */
    void SelectKeyframesInRange(float startTime, float endTime);

    /**
     * @brief Select all keyframes for bone
     */
    void SelectAllKeyframesForBone(const std::string& boneName);

    /**
     * @brief Get selected keyframes
     */
    [[nodiscard]] const std::vector<KeyframeSelection>& GetSelectedKeyframes() const {
        return m_selectedKeyframes;
    }

    /**
     * @brief Box select keyframes
     */
    void BoxSelectKeyframes(float startTime, float endTime,
                            const std::vector<std::string>& bones);

    // =========================================================================
    // Copy/Paste
    // =========================================================================

    /**
     * @brief Copy selected keyframes to clipboard
     */
    void CopySelectedKeyframes();

    /**
     * @brief Paste keyframes from clipboard at time
     */
    void PasteKeyframes(float time);

    /**
     * @brief Cut selected keyframes
     */
    void CutSelectedKeyframes();

    /**
     * @brief Duplicate selected keyframes at offset
     */
    void DuplicateSelectedKeyframes(float timeOffset);

    /**
     * @brief Check if clipboard has data
     */
    [[nodiscard]] bool HasClipboardData() const { return !m_clipboard.empty(); }

    // =========================================================================
    // Interpolation
    // =========================================================================

    /**
     * @brief Evaluate bone transform at time
     */
    [[nodiscard]] BoneTransform EvaluateTransform(const std::string& boneName, float time) const;

    /**
     * @brief Evaluate all bone transforms at time
     */
    [[nodiscard]] std::unordered_map<std::string, BoneTransform> EvaluateAllTransforms(float time) const;

    /**
     * @brief Sample animation at time and apply to bone editor
     */
    void SampleAnimation(float time);

    /**
     * @brief Set default interpolation mode for new keyframes
     */
    void SetDefaultInterpolation(InterpolationMode mode) { m_defaultInterpolation = mode; }

    // =========================================================================
    // Curve Editing
    // =========================================================================

    /**
     * @brief Get curve for bone property
     */
    [[nodiscard]] AnimationCurve* GetCurve(const std::string& boneName, const std::string& property);

    /**
     * @brief Set curve tangent
     */
    void SetCurveTangent(const std::string& boneName, const std::string& property,
                         size_t keyIndex, const TangentHandle& tangent);

    /**
     * @brief Flatten tangents (make horizontal)
     */
    void FlattenTangents(const std::string& boneName, size_t keyIndex);

    /**
     * @brief Auto-smooth tangents
     */
    void AutoSmoothTangents(const std::string& boneName, size_t keyIndex);

    /**
     * @brief Apply preset curve (ease in, ease out, etc.)
     */
    void ApplyCurvePreset(const std::string& boneName, size_t keyIndex, const std::string& preset);

    // =========================================================================
    // Onion Skinning
    // =========================================================================

    /**
     * @brief Get onion skin settings
     */
    [[nodiscard]] OnionSkinSettings& GetOnionSkinSettings() { return m_onionSkin; }
    [[nodiscard]] const OnionSkinSettings& GetOnionSkinSettings() const { return m_onionSkin; }

    /**
     * @brief Get onion skin poses for rendering
     */
    [[nodiscard]] std::vector<std::pair<float, std::unordered_map<std::string, BoneTransform>>>
    GetOnionSkinPoses(float currentTime) const;

    // =========================================================================
    // Time Management
    // =========================================================================

    /**
     * @brief Set animation duration
     */
    void SetDuration(float duration) { m_duration = duration; }

    /**
     * @brief Get animation duration
     */
    [[nodiscard]] float GetDuration() const { return m_duration; }

    /**
     * @brief Set frame rate
     */
    void SetFrameRate(float fps) { m_config.frameRate = fps; }

    /**
     * @brief Get frame rate
     */
    [[nodiscard]] float GetFrameRate() const { return m_config.frameRate; }

    /**
     * @brief Convert time to frame number
     */
    [[nodiscard]] int TimeToFrame(float time) const;

    /**
     * @brief Convert frame number to time
     */
    [[nodiscard]] float FrameToTime(int frame) const;

    /**
     * @brief Snap time to nearest frame
     */
    [[nodiscard]] float SnapToFrame(float time) const;

    /**
     * @brief Get total frame count
     */
    [[nodiscard]] int GetFrameCount() const;

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Reduce keyframes (remove redundant)
     */
    void ReduceKeyframes(float tolerance = 0.001f);

    /**
     * @brief Bake animation at frame rate
     */
    void BakeAnimation();

    /**
     * @brief Reverse animation
     */
    void ReverseAnimation();

    /**
     * @brief Scale animation time
     */
    void ScaleAnimationTime(float factor);

    /**
     * @brief Shift all keyframes by offset
     */
    void ShiftKeyframes(float timeOffset);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&, float)> OnKeyframeAdded;
    std::function<void(const std::string&, size_t)> OnKeyframeRemoved;
    std::function<void(const std::string&, size_t)> OnKeyframeModified;
    std::function<void()> OnSelectionChanged;
    std::function<void(float)> OnDurationChanged;

private:
    BoneTransform InterpolateTransform(const Keyframe& a, const Keyframe& b, float t) const;
    float EvaluateBezier(float t, float p0, float p1, float p2, float p3) const;
    void SortKeyframes(BoneTrack& track);
    void UpdateCurvesFromKeyframes(BoneTrack& track);

    Config m_config;
    BoneAnimationEditor* m_boneEditor = nullptr;

    std::vector<BoneTrack> m_tracks;
    float m_duration = 1.0f;

    // Selection
    std::vector<KeyframeSelection> m_selectedKeyframes;

    // Clipboard
    std::vector<std::pair<std::string, Keyframe>> m_clipboard;
    float m_clipboardBaseTime = 0.0f;

    // Settings
    InterpolationMode m_defaultInterpolation = InterpolationMode::Linear;
    OnionSkinSettings m_onionSkin;

    bool m_initialized = false;
};

} // namespace Vehement
