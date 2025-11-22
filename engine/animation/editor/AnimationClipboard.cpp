#include "AnimationClipboard.hpp"
#include <algorithm>
#include <limits>
#include <sstream>

namespace Nova {

// Global clipboard instance
AnimationClipboard& GetAnimationClipboard() {
    static AnimationClipboard clipboard;
    return clipboard;
}

AnimationClipboard::AnimationClipboard() = default;
AnimationClipboard::~AnimationClipboard() = default;

// ============================================================================
// Pose Operations
// ============================================================================

void AnimationClipboard::CopyPose(const std::unordered_map<std::string, EditableBoneTransform>& pose,
                                   const std::vector<std::string>& selectedBones) {
    Clear();
    m_contentType = ClipboardContentType::Pose;

    if (selectedBones.empty()) {
        m_pose = pose;
    } else {
        for (const auto& boneName : selectedBones) {
            auto it = pose.find(boneName);
            if (it != pose.end()) {
                m_pose[boneName] = it->second;
            }
        }
    }
    m_poseSelectedBones = selectedBones;
}

std::unordered_map<std::string, EditableBoneTransform> AnimationClipboard::PastePose() const {
    if (m_contentType != ClipboardContentType::Pose) {
        return {};
    }
    return m_pose;
}

// ============================================================================
// Keyframe Operations
// ============================================================================

void AnimationClipboard::CopyKeyframes(const std::vector<std::pair<std::string, EditableKeyframe>>& keyframes) {
    Clear();
    m_contentType = ClipboardContentType::Keyframes;
    m_keyframes = keyframes;

    // Calculate base time (earliest keyframe)
    m_keyframeBaseTime = std::numeric_limits<float>::max();
    for (const auto& [boneName, kf] : m_keyframes) {
        m_keyframeBaseTime = std::min(m_keyframeBaseTime, kf.time);
    }

    // Normalize times relative to base
    for (auto& [boneName, kf] : m_keyframes) {
        kf.time -= m_keyframeBaseTime;
    }
}

void AnimationClipboard::CopyKeyframesFromTracks(const std::vector<EditableBoneTrack>& tracks,
                                                  float startTime, float endTime,
                                                  const std::vector<std::string>& selectedBones) {
    std::vector<std::pair<std::string, EditableKeyframe>> keyframes;

    for (const auto& track : tracks) {
        // Skip if not in selected bones (when selection is specified)
        if (!selectedBones.empty()) {
            auto it = std::find(selectedBones.begin(), selectedBones.end(), track.boneName);
            if (it == selectedBones.end()) continue;
        }

        for (const auto& kf : track.keyframes) {
            if (kf.time >= startTime && kf.time <= endTime) {
                keyframes.emplace_back(track.boneName, kf);
            }
        }
    }

    CopyKeyframes(keyframes);
}

std::vector<std::pair<std::string, EditableKeyframe>> AnimationClipboard::PasteKeyframes(float timeOffset) const {
    if (m_contentType != ClipboardContentType::Keyframes) {
        return {};
    }

    std::vector<std::pair<std::string, EditableKeyframe>> result = m_keyframes;

    // Apply time offset
    for (auto& [boneName, kf] : result) {
        kf.time += timeOffset;
    }

    return result;
}

std::pair<float, float> AnimationClipboard::GetKeyframeTimeRange() const {
    if (m_keyframes.empty()) {
        return {0.0f, 0.0f};
    }

    float minTime = std::numeric_limits<float>::max();
    float maxTime = std::numeric_limits<float>::lowest();

    for (const auto& [boneName, kf] : m_keyframes) {
        minTime = std::min(minTime, kf.time);
        maxTime = std::max(maxTime, kf.time);
    }

    return {minTime, maxTime};
}

// ============================================================================
// Animation Operations
// ============================================================================

void AnimationClipboard::CopyAnimation(const EditableAnimation& animation) {
    Clear();
    m_contentType = ClipboardContentType::Animation;

    m_animationName = animation.GetName();
    m_animationDuration = animation.GetDuration();
    m_animationFrameRate = animation.GetFrameRate();
    m_animationLooping = animation.IsLooping();
    m_animationTracks = animation.GetTracks();
    m_animationEvents = animation.GetEvents();
}

bool AnimationClipboard::PasteAnimation(EditableAnimation& target) const {
    if (m_contentType != ClipboardContentType::Animation) {
        return false;
    }

    target.SetName(m_animationName + "_copy");
    target.SetDuration(m_animationDuration);
    target.SetFrameRate(m_animationFrameRate);
    target.SetLooping(m_animationLooping);

    target.ClearTracks();

    for (const auto& track : m_animationTracks) {
        auto* newTrack = target.AddTrack(track.boneName);
        if (newTrack) {
            for (const auto& kf : track.keyframes) {
                target.AddKeyframe(track.boneName, kf.time, kf.transform);
            }
        }
    }

    for (const auto& event : m_animationEvents) {
        auto* newEvent = target.AddEvent(event.time, event.name);
        if (newEvent) {
            newEvent->functionName = event.functionName;
            newEvent->stringParam = event.stringParam;
            newEvent->floatParam = event.floatParam;
            newEvent->intParam = event.intParam;
        }
    }

    return true;
}

// ============================================================================
// Event Operations
// ============================================================================

void AnimationClipboard::CopyEvents(const std::vector<EditableAnimationEvent>& events) {
    Clear();
    m_contentType = ClipboardContentType::Events;
    m_events = events;

    // Calculate base time
    m_eventBaseTime = std::numeric_limits<float>::max();
    for (const auto& event : m_events) {
        m_eventBaseTime = std::min(m_eventBaseTime, event.time);
    }

    // Normalize times
    for (auto& event : m_events) {
        event.time -= m_eventBaseTime;
    }
}

std::vector<EditableAnimationEvent> AnimationClipboard::PasteEvents(float timeOffset) const {
    if (m_contentType != ClipboardContentType::Events) {
        return {};
    }

    std::vector<EditableAnimationEvent> result = m_events;

    for (auto& event : result) {
        event.time += timeOffset;
    }

    return result;
}

// ============================================================================
// General
// ============================================================================

void AnimationClipboard::Clear() {
    m_contentType = ClipboardContentType::None;
    m_pose.clear();
    m_poseSelectedBones.clear();
    m_keyframes.clear();
    m_keyframeBaseTime = 0.0f;
    m_animationName.clear();
    m_animationTracks.clear();
    m_animationEvents.clear();
    m_events.clear();
    m_eventBaseTime = 0.0f;
}

std::string AnimationClipboard::GetDescription() const {
    std::stringstream ss;

    switch (m_contentType) {
        case ClipboardContentType::None:
            ss << "Empty";
            break;

        case ClipboardContentType::Pose:
            ss << "Pose (" << m_pose.size() << " bones)";
            break;

        case ClipboardContentType::Keyframes:
            ss << "Keyframes (" << m_keyframes.size() << " keyframes)";
            break;

        case ClipboardContentType::Animation:
            ss << "Animation \"" << m_animationName << "\" (" << m_animationDuration << "s)";
            break;

        case ClipboardContentType::Events:
            ss << "Events (" << m_events.size() << " events)";
            break;
    }

    return ss.str();
}

// ============================================================================
// Mirror Operations
// ============================================================================

void AnimationClipboard::SetMirrorPatterns(const std::string& left, const std::string& right) {
    m_mirrorLeftPattern = left;
    m_mirrorRightPattern = right;
}

std::unordered_map<std::string, EditableBoneTransform> AnimationClipboard::PastePoseMirrored() const {
    if (m_contentType != ClipboardContentType::Pose) {
        return {};
    }

    std::unordered_map<std::string, EditableBoneTransform> result;

    for (const auto& [boneName, transform] : m_pose) {
        std::string mirroredName = GetMirroredBoneName(boneName);
        if (mirroredName.empty()) {
            mirroredName = boneName;  // No mirror, keep same
        }

        result[mirroredName] = MirrorTransform(transform);
    }

    return result;
}

std::vector<std::pair<std::string, EditableKeyframe>> AnimationClipboard::PasteKeyframesMirrored(float timeOffset) const {
    if (m_contentType != ClipboardContentType::Keyframes) {
        return {};
    }

    std::vector<std::pair<std::string, EditableKeyframe>> result;

    for (const auto& [boneName, kf] : m_keyframes) {
        std::string mirroredName = GetMirroredBoneName(boneName);
        if (mirroredName.empty()) {
            mirroredName = boneName;
        }

        EditableKeyframe mirroredKf = kf;
        mirroredKf.time += timeOffset;
        mirroredKf.transform = MirrorTransform(kf.transform);

        result.emplace_back(mirroredName, mirroredKf);
    }

    return result;
}

std::string AnimationClipboard::GetMirroredBoneName(const std::string& boneName) const {
    // Check for left pattern
    size_t leftPos = boneName.find(m_mirrorLeftPattern);
    if (leftPos != std::string::npos) {
        std::string mirrored = boneName;
        mirrored.replace(leftPos, m_mirrorLeftPattern.length(), m_mirrorRightPattern);
        return mirrored;
    }

    // Check for right pattern
    size_t rightPos = boneName.find(m_mirrorRightPattern);
    if (rightPos != std::string::npos) {
        std::string mirrored = boneName;
        mirrored.replace(rightPos, m_mirrorRightPattern.length(), m_mirrorLeftPattern);
        return mirrored;
    }

    return "";  // No mirror
}

EditableBoneTransform AnimationClipboard::MirrorTransform(const EditableBoneTransform& transform) const {
    EditableBoneTransform result = transform;

    // Mirror position across YZ plane (negate X)
    result.position.x = -transform.position.x;

    // Mirror rotation
    result.rotation.y = -transform.rotation.y;
    result.rotation.z = -transform.rotation.z;

    return result;
}

} // namespace Nova
