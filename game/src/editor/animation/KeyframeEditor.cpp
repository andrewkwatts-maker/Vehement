#include "KeyframeEditor.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// AnimationCurve Implementation
// ============================================================================

float AnimationCurve::Evaluate(float time) const {
    if (keys.empty()) return 0.0f;
    if (keys.size() == 1) return keys[0].second;

    // Find surrounding keys
    if (time <= keys.front().first) return keys.front().second;
    if (time >= keys.back().first) return keys.back().second;

    size_t i = 0;
    for (; i < keys.size() - 1; ++i) {
        if (time >= keys[i].first && time <= keys[i + 1].first) {
            break;
        }
    }

    float t = (time - keys[i].first) / (keys[i + 1].first - keys[i].first);

    // Linear interpolation (could be extended for bezier)
    return keys[i].second + t * (keys[i + 1].second - keys[i].second);
}

void AnimationCurve::AddKey(float time, float value) {
    keys.emplace_back(time, value);
    tangents.push_back(TangentHandle{});

    // Sort by time
    std::vector<std::pair<size_t, float>> indices;
    for (size_t i = 0; i < keys.size(); ++i) {
        indices.emplace_back(i, keys[i].first);
    }
    std::sort(indices.begin(), indices.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    std::vector<std::pair<float, float>> newKeys;
    std::vector<TangentHandle> newTangents;
    for (const auto& idx : indices) {
        newKeys.push_back(keys[idx.first]);
        newTangents.push_back(tangents[idx.first]);
    }
    keys = std::move(newKeys);
    tangents = std::move(newTangents);
}

void AnimationCurve::RemoveKey(size_t index) {
    if (index < keys.size()) {
        keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(index));
        tangents.erase(tangents.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void AnimationCurve::SetKey(size_t index, float time, float value) {
    if (index < keys.size()) {
        keys[index] = {time, value};
    }
}

// ============================================================================
// KeyframeEditor Implementation
// ============================================================================

KeyframeEditor::KeyframeEditor() = default;
KeyframeEditor::~KeyframeEditor() = default;

void KeyframeEditor::Initialize(const Config& config) {
    m_config = config;
    m_duration = config.defaultDuration;
    m_initialized = true;
}

// ============================================================================
// Track Management
// ============================================================================

BoneTrack* KeyframeEditor::AddTrack(const std::string& boneName) {
    // Check if track already exists
    for (auto& track : m_tracks) {
        if (track.boneName == boneName) {
            return &track;
        }
    }

    BoneTrack track;
    track.boneName = boneName;
    m_tracks.push_back(track);
    return &m_tracks.back();
}

void KeyframeEditor::RemoveTrack(const std::string& boneName) {
    m_tracks.erase(
        std::remove_if(m_tracks.begin(), m_tracks.end(),
            [&boneName](const BoneTrack& t) { return t.boneName == boneName; }),
        m_tracks.end()
    );
}

BoneTrack* KeyframeEditor::GetTrack(const std::string& boneName) {
    for (auto& track : m_tracks) {
        if (track.boneName == boneName) {
            return &track;
        }
    }
    return nullptr;
}

const BoneTrack* KeyframeEditor::GetTrack(const std::string& boneName) const {
    for (const auto& track : m_tracks) {
        if (track.boneName == boneName) {
            return &track;
        }
    }
    return nullptr;
}

void KeyframeEditor::ClearAllTracks() {
    m_tracks.clear();
    m_selectedKeyframes.clear();
}

void KeyframeEditor::CreateTracksFromSkeleton() {
    if (!m_boneEditor || !m_boneEditor->GetSkeleton()) return;

    ClearAllTracks();

    for (const auto& boneName : m_boneEditor->GetBonesInHierarchyOrder()) {
        AddTrack(boneName);
    }
}

// ============================================================================
// Keyframe Operations
// ============================================================================

Keyframe* KeyframeEditor::AddKeyframe(const std::string& boneName, float time) {
    if (!m_boneEditor) return nullptr;

    BoneTransform transform = m_boneEditor->GetBoneTransform(boneName);
    return AddKeyframe(boneName, time, transform);
}

Keyframe* KeyframeEditor::AddKeyframe(const std::string& boneName, float time, const BoneTransform& transform) {
    BoneTrack* track = GetTrack(boneName);
    if (!track) {
        track = AddTrack(boneName);
    }

    // Snap to frame if enabled
    if (m_config.snapToFrames) {
        time = SnapToFrame(time);
    }

    // Check if keyframe exists at this time
    for (auto& kf : track->keyframes) {
        if (std::abs(kf.time - time) < m_config.snapThreshold) {
            kf.transform = transform;
            UpdateCurvesFromKeyframes(*track);
            if (OnKeyframeModified) {
                OnKeyframeModified(boneName, &kf - track->keyframes.data());
            }
            return &kf;
        }
    }

    // Add new keyframe
    Keyframe keyframe;
    keyframe.time = time;
    keyframe.transform = transform;
    keyframe.interpolation = m_defaultInterpolation;

    track->keyframes.push_back(keyframe);
    SortKeyframes(*track);
    UpdateCurvesFromKeyframes(*track);

    // Find index of added keyframe
    size_t index = 0;
    for (size_t i = 0; i < track->keyframes.size(); ++i) {
        if (std::abs(track->keyframes[i].time - time) < 0.0001f) {
            index = i;
            break;
        }
    }

    if (OnKeyframeAdded) {
        OnKeyframeAdded(boneName, time);
    }

    return &track->keyframes[index];
}

void KeyframeEditor::RemoveKeyframe(const std::string& boneName, size_t index) {
    BoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return;

    track->keyframes.erase(track->keyframes.begin() + static_cast<std::ptrdiff_t>(index));
    UpdateCurvesFromKeyframes(*track);

    // Update selection
    m_selectedKeyframes.erase(
        std::remove_if(m_selectedKeyframes.begin(), m_selectedKeyframes.end(),
            [&boneName, index](const KeyframeSelection& s) {
                return s.trackName == boneName && s.keyframeIndex == index;
            }),
        m_selectedKeyframes.end()
    );

    if (OnKeyframeRemoved) {
        OnKeyframeRemoved(boneName, index);
    }
}

void KeyframeEditor::RemoveKeyframeAtTime(const std::string& boneName, float time) {
    BoneTrack* track = GetTrack(boneName);
    if (!track) return;

    for (size_t i = 0; i < track->keyframes.size(); ++i) {
        if (std::abs(track->keyframes[i].time - time) < m_config.snapThreshold) {
            RemoveKeyframe(boneName, i);
            return;
        }
    }
}

Keyframe* KeyframeEditor::GetKeyframe(const std::string& boneName, size_t index) {
    BoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return nullptr;
    return &track->keyframes[index];
}

Keyframe* KeyframeEditor::GetKeyframeAtTime(const std::string& boneName, float time, float tolerance) {
    BoneTrack* track = GetTrack(boneName);
    if (!track) return nullptr;

    for (auto& kf : track->keyframes) {
        if (std::abs(kf.time - time) <= tolerance) {
            return &kf;
        }
    }
    return nullptr;
}

void KeyframeEditor::MoveKeyframe(const std::string& boneName, size_t index, float newTime) {
    BoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return;

    if (m_config.snapToFrames) {
        newTime = SnapToFrame(newTime);
    }

    track->keyframes[index].time = newTime;
    SortKeyframes(*track);
    UpdateCurvesFromKeyframes(*track);

    if (OnKeyframeModified) {
        OnKeyframeModified(boneName, index);
    }
}

void KeyframeEditor::SetKeyframeTransform(const std::string& boneName, size_t index, const BoneTransform& transform) {
    BoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return;

    track->keyframes[index].transform = transform;
    UpdateCurvesFromKeyframes(*track);

    if (OnKeyframeModified) {
        OnKeyframeModified(boneName, index);
    }
}

void KeyframeEditor::SetKeyframeInterpolation(const std::string& boneName, size_t index, InterpolationMode mode) {
    BoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return;

    track->keyframes[index].interpolation = mode;
}

void KeyframeEditor::AutoKey(float time) {
    if (!m_boneEditor) return;

    for (const auto& boneName : m_boneEditor->GetSelectedBones()) {
        AddKeyframe(boneName, time);
    }
}

void KeyframeEditor::InsertKeyframeForSelection(float time) {
    if (!m_boneEditor) return;

    for (const auto& boneName : m_boneEditor->GetSelectedBones()) {
        AddKeyframe(boneName, time);
    }
}

// ============================================================================
// Keyframe Selection
// ============================================================================

void KeyframeEditor::SelectKeyframe(const std::string& boneName, size_t index, bool addToSelection) {
    if (!addToSelection) {
        ClearKeyframeSelection();
    }

    BoneTrack* track = GetTrack(boneName);
    if (track && index < track->keyframes.size()) {
        track->keyframes[index].selected = true;

        KeyframeSelection selection;
        selection.trackName = boneName;
        selection.keyframeIndex = index;
        selection.selected = true;
        m_selectedKeyframes.push_back(selection);

        if (OnSelectionChanged) {
            OnSelectionChanged();
        }
    }
}

void KeyframeEditor::DeselectKeyframe(const std::string& boneName, size_t index) {
    BoneTrack* track = GetTrack(boneName);
    if (track && index < track->keyframes.size()) {
        track->keyframes[index].selected = false;
    }

    m_selectedKeyframes.erase(
        std::remove_if(m_selectedKeyframes.begin(), m_selectedKeyframes.end(),
            [&boneName, index](const KeyframeSelection& s) {
                return s.trackName == boneName && s.keyframeIndex == index;
            }),
        m_selectedKeyframes.end()
    );

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

void KeyframeEditor::ClearKeyframeSelection() {
    for (auto& track : m_tracks) {
        for (auto& kf : track.keyframes) {
            kf.selected = false;
        }
    }
    m_selectedKeyframes.clear();

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

void KeyframeEditor::SelectKeyframesInRange(float startTime, float endTime) {
    ClearKeyframeSelection();

    for (auto& track : m_tracks) {
        for (size_t i = 0; i < track.keyframes.size(); ++i) {
            if (track.keyframes[i].time >= startTime && track.keyframes[i].time <= endTime) {
                SelectKeyframe(track.boneName, i, true);
            }
        }
    }
}

void KeyframeEditor::SelectAllKeyframesForBone(const std::string& boneName) {
    BoneTrack* track = GetTrack(boneName);
    if (!track) return;

    for (size_t i = 0; i < track->keyframes.size(); ++i) {
        SelectKeyframe(boneName, i, true);
    }
}

void KeyframeEditor::BoxSelectKeyframes(float startTime, float endTime,
                                         const std::vector<std::string>& bones) {
    ClearKeyframeSelection();

    for (const auto& boneName : bones) {
        BoneTrack* track = GetTrack(boneName);
        if (!track) continue;

        for (size_t i = 0; i < track->keyframes.size(); ++i) {
            float time = track->keyframes[i].time;
            if (time >= startTime && time <= endTime) {
                SelectKeyframe(boneName, i, true);
            }
        }
    }
}

// ============================================================================
// Copy/Paste
// ============================================================================

void KeyframeEditor::CopySelectedKeyframes() {
    m_clipboard.clear();

    if (m_selectedKeyframes.empty()) return;

    // Find earliest time
    m_clipboardBaseTime = std::numeric_limits<float>::max();
    for (const auto& sel : m_selectedKeyframes) {
        const BoneTrack* track = GetTrack(sel.trackName);
        if (track && sel.keyframeIndex < track->keyframes.size()) {
            m_clipboardBaseTime = std::min(m_clipboardBaseTime, track->keyframes[sel.keyframeIndex].time);
        }
    }

    // Copy keyframes
    for (const auto& sel : m_selectedKeyframes) {
        const BoneTrack* track = GetTrack(sel.trackName);
        if (track && sel.keyframeIndex < track->keyframes.size()) {
            Keyframe kf = track->keyframes[sel.keyframeIndex];
            kf.time -= m_clipboardBaseTime;  // Store relative time
            m_clipboard.emplace_back(sel.trackName, kf);
        }
    }
}

void KeyframeEditor::PasteKeyframes(float time) {
    for (const auto& [boneName, kf] : m_clipboard) {
        Keyframe newKf = kf;
        newKf.time = time + kf.time;  // Add relative time to paste position
        newKf.selected = false;
        AddKeyframe(boneName, newKf.time, newKf.transform);
    }
}

void KeyframeEditor::CutSelectedKeyframes() {
    CopySelectedKeyframes();

    // Remove in reverse order to preserve indices
    std::vector<KeyframeSelection> toRemove = m_selectedKeyframes;
    std::sort(toRemove.begin(), toRemove.end(),
        [](const KeyframeSelection& a, const KeyframeSelection& b) {
            if (a.trackName != b.trackName) return a.trackName < b.trackName;
            return a.keyframeIndex > b.keyframeIndex;  // Reverse order
        });

    for (const auto& sel : toRemove) {
        RemoveKeyframe(sel.trackName, sel.keyframeIndex);
    }
}

void KeyframeEditor::DuplicateSelectedKeyframes(float timeOffset) {
    CopySelectedKeyframes();
    PasteKeyframes(m_clipboardBaseTime + timeOffset);
}

// ============================================================================
// Interpolation
// ============================================================================

BoneTransform KeyframeEditor::EvaluateTransform(const std::string& boneName, float time) const {
    const BoneTrack* track = GetTrack(boneName);
    if (!track || track->keyframes.empty()) {
        return BoneTransform();
    }

    const auto& keyframes = track->keyframes;

    // Before first keyframe
    if (time <= keyframes.front().time) {
        return keyframes.front().transform;
    }

    // After last keyframe
    if (time >= keyframes.back().time) {
        return keyframes.back().transform;
    }

    // Find surrounding keyframes
    size_t i = 0;
    for (; i < keyframes.size() - 1; ++i) {
        if (time >= keyframes[i].time && time <= keyframes[i + 1].time) {
            break;
        }
    }

    const Keyframe& a = keyframes[i];
    const Keyframe& b = keyframes[i + 1];

    float t = (time - a.time) / (b.time - a.time);

    return InterpolateTransform(a, b, t);
}

std::unordered_map<std::string, BoneTransform> KeyframeEditor::EvaluateAllTransforms(float time) const {
    std::unordered_map<std::string, BoneTransform> result;

    for (const auto& track : m_tracks) {
        result[track.boneName] = EvaluateTransform(track.boneName, time);
    }

    return result;
}

void KeyframeEditor::SampleAnimation(float time) {
    if (!m_boneEditor) return;

    auto transforms = EvaluateAllTransforms(time);
    m_boneEditor->SetAllTransforms(transforms);
}

// ============================================================================
// Curve Editing
// ============================================================================

AnimationCurve* KeyframeEditor::GetCurve(const std::string& boneName, const std::string& property) {
    BoneTrack* track = GetTrack(boneName);
    if (!track) return nullptr;

    if (property == "positionX") return &track->positionX;
    if (property == "positionY") return &track->positionY;
    if (property == "positionZ") return &track->positionZ;
    if (property == "rotationX") return &track->rotationX;
    if (property == "rotationY") return &track->rotationY;
    if (property == "rotationZ") return &track->rotationZ;
    if (property == "rotationW") return &track->rotationW;
    if (property == "scaleX") return &track->scaleX;
    if (property == "scaleY") return &track->scaleY;
    if (property == "scaleZ") return &track->scaleZ;

    return nullptr;
}

void KeyframeEditor::SetCurveTangent(const std::string& boneName, const std::string& property,
                                      size_t keyIndex, const TangentHandle& tangent) {
    AnimationCurve* curve = GetCurve(boneName, property);
    if (curve && keyIndex < curve->tangents.size()) {
        curve->tangents[keyIndex] = tangent;
    }
}

void KeyframeEditor::FlattenTangents(const std::string& boneName, size_t keyIndex) {
    BoneTrack* track = GetTrack(boneName);
    if (!track || keyIndex >= track->keyframes.size()) return;

    TangentHandle flat;
    flat.inTangent = glm::vec2(-0.1f, 0.0f);
    flat.outTangent = glm::vec2(0.1f, 0.0f);
    flat.mode = TangentMode::Flat;

    track->keyframes[keyIndex].tangent = flat;
}

void KeyframeEditor::AutoSmoothTangents(const std::string& boneName, size_t keyIndex) {
    BoneTrack* track = GetTrack(boneName);
    if (!track || keyIndex >= track->keyframes.size()) return;

    // Calculate smooth tangents based on neighbors
    TangentHandle& tangent = track->keyframes[keyIndex].tangent;
    tangent.mode = TangentMode::Auto;

    // In a full implementation, this would calculate tangents
    // based on neighboring keyframe values
}

void KeyframeEditor::ApplyCurvePreset(const std::string& boneName, size_t keyIndex, const std::string& preset) {
    BoneTrack* track = GetTrack(boneName);
    if (!track || keyIndex >= track->keyframes.size()) return;

    TangentHandle& tangent = track->keyframes[keyIndex].tangent;

    if (preset == "ease_in") {
        tangent.inTangent = glm::vec2(-0.1f, 0.0f);
        tangent.outTangent = glm::vec2(0.3f, 0.0f);
    } else if (preset == "ease_out") {
        tangent.inTangent = glm::vec2(-0.3f, 0.0f);
        tangent.outTangent = glm::vec2(0.1f, 0.0f);
    } else if (preset == "ease_in_out") {
        tangent.inTangent = glm::vec2(-0.3f, 0.0f);
        tangent.outTangent = glm::vec2(0.3f, 0.0f);
    } else if (preset == "linear") {
        tangent.mode = TangentMode::Linear;
    }
}

// ============================================================================
// Onion Skinning
// ============================================================================

std::vector<std::pair<float, std::unordered_map<std::string, BoneTransform>>>
KeyframeEditor::GetOnionSkinPoses(float currentTime) const {
    std::vector<std::pair<float, std::unordered_map<std::string, BoneTransform>>> poses;

    if (!m_onionSkin.enabled) return poses;

    float frameTime = 1.0f / m_config.frameRate;
    int step = m_onionSkin.showEveryNth ? m_onionSkin.nthFrame : 1;

    // Frames before
    for (int i = m_onionSkin.framesBefore; i > 0; i -= step) {
        float time = currentTime - i * frameTime;
        if (time >= 0.0f) {
            poses.emplace_back(time, EvaluateAllTransforms(time));
        }
    }

    // Frames after
    for (int i = step; i <= m_onionSkin.framesAfter; i += step) {
        float time = currentTime + i * frameTime;
        if (time <= m_duration) {
            poses.emplace_back(time, EvaluateAllTransforms(time));
        }
    }

    return poses;
}

// ============================================================================
// Time Management
// ============================================================================

int KeyframeEditor::TimeToFrame(float time) const {
    return static_cast<int>(std::round(time * m_config.frameRate));
}

float KeyframeEditor::FrameToTime(int frame) const {
    return static_cast<float>(frame) / m_config.frameRate;
}

float KeyframeEditor::SnapToFrame(float time) const {
    int frame = TimeToFrame(time);
    return FrameToTime(frame);
}

int KeyframeEditor::GetFrameCount() const {
    return TimeToFrame(m_duration);
}

// ============================================================================
// Utility
// ============================================================================

void KeyframeEditor::ReduceKeyframes(float tolerance) {
    for (auto& track : m_tracks) {
        if (track.keyframes.size() < 3) continue;

        std::vector<Keyframe> reduced;
        reduced.push_back(track.keyframes.front());

        for (size_t i = 1; i < track.keyframes.size() - 1; ++i) {
            const Keyframe& prev = reduced.back();
            const Keyframe& curr = track.keyframes[i];
            const Keyframe& next = track.keyframes[i + 1];

            // Calculate expected value from linear interpolation
            float t = (curr.time - prev.time) / (next.time - prev.time);
            BoneTransform expected = BoneTransform::Lerp(prev.transform, next.transform, t);

            // Check if current keyframe is significantly different
            float posDiff = glm::length(curr.transform.position - expected.position);
            float rotDiff = glm::dot(curr.transform.rotation, expected.rotation);

            if (posDiff > tolerance || rotDiff < (1.0f - tolerance)) {
                reduced.push_back(curr);
            }
        }

        reduced.push_back(track.keyframes.back());
        track.keyframes = std::move(reduced);
        UpdateCurvesFromKeyframes(track);
    }
}

void KeyframeEditor::BakeAnimation() {
    float frameTime = 1.0f / m_config.frameRate;
    int frameCount = GetFrameCount();

    for (auto& track : m_tracks) {
        std::vector<Keyframe> bakedKeyframes;

        for (int frame = 0; frame <= frameCount; ++frame) {
            float time = frame * frameTime;
            Keyframe kf;
            kf.time = time;
            kf.transform = EvaluateTransform(track.boneName, time);
            kf.interpolation = InterpolationMode::Linear;
            bakedKeyframes.push_back(kf);
        }

        track.keyframes = std::move(bakedKeyframes);
        UpdateCurvesFromKeyframes(track);
    }
}

void KeyframeEditor::ReverseAnimation() {
    for (auto& track : m_tracks) {
        for (auto& kf : track.keyframes) {
            kf.time = m_duration - kf.time;
        }
        SortKeyframes(track);
        UpdateCurvesFromKeyframes(track);
    }
}

void KeyframeEditor::ScaleAnimationTime(float factor) {
    m_duration *= factor;

    for (auto& track : m_tracks) {
        for (auto& kf : track.keyframes) {
            kf.time *= factor;
        }
        UpdateCurvesFromKeyframes(track);
    }

    if (OnDurationChanged) {
        OnDurationChanged(m_duration);
    }
}

void KeyframeEditor::ShiftKeyframes(float timeOffset) {
    for (auto& track : m_tracks) {
        for (auto& kf : track.keyframes) {
            kf.time = std::max(0.0f, kf.time + timeOffset);
        }
        SortKeyframes(track);
        UpdateCurvesFromKeyframes(track);
    }
}

// ============================================================================
// Private Methods
// ============================================================================

BoneTransform KeyframeEditor::InterpolateTransform(const Keyframe& a, const Keyframe& b, float t) const {
    switch (a.interpolation) {
        case InterpolationMode::Step:
            return a.transform;

        case InterpolationMode::Linear:
            return BoneTransform::Lerp(a.transform, b.transform, t);

        case InterpolationMode::Bezier: {
            // Use bezier curve for position, slerp for rotation
            float bezierT = EvaluateBezier(t, 0.0f,
                a.tangent.outTangent.y, 1.0f - b.tangent.inTangent.y, 1.0f);

            BoneTransform result;
            result.position = glm::mix(a.transform.position, b.transform.position, bezierT);
            result.rotation = glm::slerp(a.transform.rotation, b.transform.rotation, bezierT);
            result.scale = glm::mix(a.transform.scale, b.transform.scale, bezierT);
            return result;
        }

        case InterpolationMode::Hermite:
        case InterpolationMode::CatmullRom:
            // Simplified - use slerp for now
            return BoneTransform::Slerp(a.transform, b.transform, t);

        default:
            return BoneTransform::Lerp(a.transform, b.transform, t);
    }
}

float KeyframeEditor::EvaluateBezier(float t, float p0, float p1, float p2, float p3) const {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    return uuu * p0 + 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + ttt * p3;
}

void KeyframeEditor::SortKeyframes(BoneTrack& track) {
    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

void KeyframeEditor::UpdateCurvesFromKeyframes(BoneTrack& track) {
    track.positionX.keys.clear();
    track.positionY.keys.clear();
    track.positionZ.keys.clear();
    track.rotationX.keys.clear();
    track.rotationY.keys.clear();
    track.rotationZ.keys.clear();
    track.rotationW.keys.clear();
    track.scaleX.keys.clear();
    track.scaleY.keys.clear();
    track.scaleZ.keys.clear();

    for (const auto& kf : track.keyframes) {
        track.positionX.keys.emplace_back(kf.time, kf.transform.position.x);
        track.positionY.keys.emplace_back(kf.time, kf.transform.position.y);
        track.positionZ.keys.emplace_back(kf.time, kf.transform.position.z);

        track.rotationX.keys.emplace_back(kf.time, kf.transform.rotation.x);
        track.rotationY.keys.emplace_back(kf.time, kf.transform.rotation.y);
        track.rotationZ.keys.emplace_back(kf.time, kf.transform.rotation.z);
        track.rotationW.keys.emplace_back(kf.time, kf.transform.rotation.w);

        track.scaleX.keys.emplace_back(kf.time, kf.transform.scale.x);
        track.scaleY.keys.emplace_back(kf.time, kf.transform.scale.y);
        track.scaleZ.keys.emplace_back(kf.time, kf.transform.scale.z);
    }
}

} // namespace Vehement
