#pragma once

#include "EditableSkeleton.hpp"
#include "EditableAnimation.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace Nova {

/**
 * @brief Clipboard content type
 */
enum class ClipboardContentType {
    None,
    Pose,
    Keyframes,
    Animation,
    Events
};

/**
 * @brief Animation clipboard for copy/paste operations
 *
 * Features:
 * - Pose clipboard
 * - Keyframe clipboard
 * - Animation clipboard
 */
class AnimationClipboard {
public:
    AnimationClipboard();
    ~AnimationClipboard();

    // =========================================================================
    // Pose Operations
    // =========================================================================

    /**
     * @brief Copy pose to clipboard
     */
    void CopyPose(const std::unordered_map<std::string, EditableBoneTransform>& pose,
                  const std::vector<std::string>& selectedBones = {});

    /**
     * @brief Paste pose from clipboard
     */
    [[nodiscard]] std::unordered_map<std::string, EditableBoneTransform> PastePose() const;

    /**
     * @brief Check if has pose
     */
    [[nodiscard]] bool HasPose() const { return m_contentType == ClipboardContentType::Pose; }

    // =========================================================================
    // Keyframe Operations
    // =========================================================================

    /**
     * @brief Copy keyframes to clipboard
     */
    void CopyKeyframes(const std::vector<std::pair<std::string, EditableKeyframe>>& keyframes);

    /**
     * @brief Copy keyframes from tracks at time range
     */
    void CopyKeyframesFromTracks(const std::vector<EditableBoneTrack>& tracks,
                                  float startTime, float endTime,
                                  const std::vector<std::string>& selectedBones = {});

    /**
     * @brief Paste keyframes
     * @param timeOffset Offset to apply to pasted keyframe times
     * @return Vector of (boneName, keyframe) pairs
     */
    [[nodiscard]] std::vector<std::pair<std::string, EditableKeyframe>> PasteKeyframes(float timeOffset = 0.0f) const;

    /**
     * @brief Check if has keyframes
     */
    [[nodiscard]] bool HasKeyframes() const { return m_contentType == ClipboardContentType::Keyframes; }

    /**
     * @brief Get keyframe count in clipboard
     */
    [[nodiscard]] size_t GetKeyframeCount() const { return m_keyframes.size(); }

    /**
     * @brief Get time range of copied keyframes
     */
    [[nodiscard]] std::pair<float, float> GetKeyframeTimeRange() const;

    // =========================================================================
    // Animation Operations
    // =========================================================================

    /**
     * @brief Copy entire animation
     */
    void CopyAnimation(const EditableAnimation& animation);

    /**
     * @brief Paste animation
     */
    [[nodiscard]] bool PasteAnimation(EditableAnimation& target) const;

    /**
     * @brief Check if has animation
     */
    [[nodiscard]] bool HasAnimation() const { return m_contentType == ClipboardContentType::Animation; }

    // =========================================================================
    // Event Operations
    // =========================================================================

    /**
     * @brief Copy events
     */
    void CopyEvents(const std::vector<EditableAnimationEvent>& events);

    /**
     * @brief Paste events
     */
    [[nodiscard]] std::vector<EditableAnimationEvent> PasteEvents(float timeOffset = 0.0f) const;

    /**
     * @brief Check if has events
     */
    [[nodiscard]] bool HasEvents() const { return m_contentType == ClipboardContentType::Events; }

    // =========================================================================
    // General
    // =========================================================================

    /**
     * @brief Clear clipboard
     */
    void Clear();

    /**
     * @brief Get content type
     */
    [[nodiscard]] ClipboardContentType GetContentType() const { return m_contentType; }

    /**
     * @brief Check if empty
     */
    [[nodiscard]] bool IsEmpty() const { return m_contentType == ClipboardContentType::None; }

    /**
     * @brief Get clipboard description
     */
    [[nodiscard]] std::string GetDescription() const;

    // =========================================================================
    // Mirror Operations
    // =========================================================================

    /**
     * @brief Set mirror patterns for bone name mirroring
     */
    void SetMirrorPatterns(const std::string& left, const std::string& right);

    /**
     * @brief Paste pose with mirroring
     */
    [[nodiscard]] std::unordered_map<std::string, EditableBoneTransform> PastePoseMirrored() const;

    /**
     * @brief Paste keyframes with mirroring
     */
    [[nodiscard]] std::vector<std::pair<std::string, EditableKeyframe>> PasteKeyframesMirrored(float timeOffset = 0.0f) const;

private:
    std::string GetMirroredBoneName(const std::string& boneName) const;
    EditableBoneTransform MirrorTransform(const EditableBoneTransform& transform) const;

    ClipboardContentType m_contentType = ClipboardContentType::None;

    // Pose data
    std::unordered_map<std::string, EditableBoneTransform> m_pose;
    std::vector<std::string> m_poseSelectedBones;

    // Keyframe data
    std::vector<std::pair<std::string, EditableKeyframe>> m_keyframes;
    float m_keyframeBaseTime = 0.0f;

    // Animation data
    std::string m_animationName;
    float m_animationDuration = 0.0f;
    float m_animationFrameRate = 30.0f;
    bool m_animationLooping = true;
    std::vector<EditableBoneTrack> m_animationTracks;
    std::vector<EditableAnimationEvent> m_animationEvents;

    // Event data
    std::vector<EditableAnimationEvent> m_events;
    float m_eventBaseTime = 0.0f;

    // Mirror patterns
    std::string m_mirrorLeftPattern = "_L";
    std::string m_mirrorRightPattern = "_R";
};

// Global clipboard instance
AnimationClipboard& GetAnimationClipboard();

} // namespace Nova
