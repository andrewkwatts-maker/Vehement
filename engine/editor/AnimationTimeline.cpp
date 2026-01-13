/**
 * @file AnimationTimeline.cpp
 * @brief Implementation of Animation Timeline Editor
 */

#include "AnimationTimeline.hpp"
#include "../ui/EditorTheme.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace Nova {

// =============================================================================
// Constants
// =============================================================================

namespace {
    constexpr float MIN_ZOOM = 0.1f;
    constexpr float MAX_ZOOM = 10.0f;
    constexpr float KEYFRAME_SIZE = 8.0f;
    constexpr float KEYFRAME_HIT_RADIUS = 10.0f;
    constexpr float TANGENT_HANDLE_SIZE = 6.0f;
    constexpr float TANGENT_LINE_LENGTH = 50.0f;
    constexpr float SNAP_THRESHOLD = 5.0f;
    constexpr float MIN_TRACK_HEIGHT = 20.0f;
    constexpr float MAX_TRACK_HEIGHT = 100.0f;
    constexpr float RULER_MAJOR_TICK_HEIGHT = 12.0f;
    constexpr float RULER_MINOR_TICK_HEIGHT = 6.0f;
}

// =============================================================================
// AnimationTrackBase Implementation
// =============================================================================

AnimationTrackBase::AnimationTrackBase(const std::string& name, const std::string& targetId)
    : m_name(name)
    , m_targetId(targetId)
{
}

TimelineKeyframe* AnimationTrackBase::GetKeyframe(size_t index) {
    if (index >= m_keyframes.size()) return nullptr;
    return &m_keyframes[index];
}

const TimelineKeyframe* AnimationTrackBase::GetKeyframe(size_t index) const {
    if (index >= m_keyframes.size()) return nullptr;
    return &m_keyframes[index];
}

size_t AnimationTrackBase::AddKeyframe(float time, const KeyframeValue& value) {
    TimelineKeyframe kf;
    kf.time = time;
    kf.value = value;
    kf.interpolation = KeyframeInterpolation::Linear;

    // Find insertion point to maintain sorted order
    auto it = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), time);
    size_t index = static_cast<size_t>(it - m_keyframes.begin());
    m_keyframes.insert(it, kf);

    return index;
}

void AnimationTrackBase::RemoveKeyframe(size_t index) {
    if (index >= m_keyframes.size()) return;
    m_keyframes.erase(m_keyframes.begin() + static_cast<ptrdiff_t>(index));
}

bool AnimationTrackBase::RemoveKeyframeAtTime(float time, float tolerance) {
    auto idx = FindKeyframeAtTime(time, tolerance);
    if (idx.has_value()) {
        RemoveKeyframe(idx.value());
        return true;
    }
    return false;
}

void AnimationTrackBase::MoveKeyframe(size_t index, float newTime) {
    if (index >= m_keyframes.size()) return;

    TimelineKeyframe kf = m_keyframes[index];
    kf.time = newTime;
    m_keyframes.erase(m_keyframes.begin() + static_cast<ptrdiff_t>(index));

    // Re-insert at correct sorted position
    auto it = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), newTime);
    m_keyframes.insert(it, kf);
}

void AnimationTrackBase::SetKeyframeValue(size_t index, const KeyframeValue& value) {
    if (index >= m_keyframes.size()) return;
    m_keyframes[index].value = value;
}

void AnimationTrackBase::SetKeyframeInterpolation(size_t index, KeyframeInterpolation interp) {
    if (index >= m_keyframes.size()) return;
    m_keyframes[index].interpolation = interp;
}

std::optional<size_t> AnimationTrackBase::FindKeyframeAtTime(float time, float tolerance) const {
    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        if (std::abs(m_keyframes[i].time - time) <= tolerance) {
            return i;
        }
    }
    return std::nullopt;
}

std::vector<size_t> AnimationTrackBase::GetKeyframesInRange(float startTime, float endTime) const {
    std::vector<size_t> result;
    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i].time >= startTime && m_keyframes[i].time <= endTime) {
            result.push_back(i);
        }
    }
    return result;
}

float AnimationTrackBase::GetDuration() const {
    if (m_keyframes.empty()) return 0.0f;
    return m_keyframes.back().time;
}

void AnimationTrackBase::SortKeyframes() {
    std::sort(m_keyframes.begin(), m_keyframes.end());
}

// =============================================================================
// TransformTrack Implementation
// =============================================================================

TransformTrack::TransformTrack(const std::string& name, const std::string& targetId, Component component)
    : AnimationTrackBase(name, targetId)
    , m_component(component)
{
    switch (component) {
        case Component::Position:
            SetPropertyPath("transform.position");
            SetColor(glm::vec4(0.9f, 0.3f, 0.3f, 1.0f));  // Red
            break;
        case Component::Rotation:
            SetPropertyPath("transform.rotation");
            SetColor(glm::vec4(0.3f, 0.9f, 0.3f, 1.0f));  // Green
            break;
        case Component::Scale:
            SetPropertyPath("transform.scale");
            SetColor(glm::vec4(0.3f, 0.3f, 0.9f, 1.0f));  // Blue
            break;
        case Component::All:
            SetPropertyPath("transform");
            SetColor(glm::vec4(0.9f, 0.7f, 0.3f, 1.0f));  // Orange
            break;
    }
}

KeyframeValue TransformTrack::Evaluate(float time) const {
    if (m_keyframes.empty()) {
        return glm::vec3(0.0f);
    }

    if (m_keyframes.size() == 1) {
        return m_keyframes[0].value;
    }

    // Find surrounding keyframes
    if (time <= m_keyframes.front().time) {
        return m_keyframes.front().value;
    }
    if (time >= m_keyframes.back().time) {
        return m_keyframes.back().value;
    }

    // Binary search for keyframe pair
    auto it = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), time);
    size_t idx = static_cast<size_t>(it - m_keyframes.begin());
    if (idx == 0) idx = 1;

    const auto& kf0 = m_keyframes[idx - 1];
    const auto& kf1 = m_keyframes[idx];

    float t = (time - kf0.time) / (kf1.time - kf0.time);
    return InterpolateKeyframeValues(kf0.value, kf1.value, t, kf0.interpolation);
}

std::unique_ptr<IAnimationTrack> TransformTrack::Clone() const {
    auto clone = std::make_unique<TransformTrack>(m_name, m_targetId, m_component);
    clone->m_keyframes = m_keyframes;
    clone->m_color = m_color;
    clone->m_muted = m_muted;
    clone->m_locked = m_locked;
    clone->m_solo = m_solo;
    clone->m_expanded = m_expanded;
    return clone;
}

size_t TransformTrack::AddTransformKeyframe(float time, const glm::vec3& position,
                                             const glm::quat& rotation, const glm::vec3& scale) {
    // Store as combined transform in a special variant encoding
    // For now, use position as the primary value for Component::Position, etc.
    switch (m_component) {
        case Component::Position:
            return AddKeyframe(time, position);
        case Component::Rotation:
            return AddKeyframe(time, rotation);
        case Component::Scale:
            return AddKeyframe(time, scale);
        case Component::All:
            // For combined transform, use vec3 position as primary
            // Full transform would need a custom struct, but we simplify here
            return AddKeyframe(time, position);
    }
    return 0;
}

Keyframe TransformTrack::EvaluateTransform(float time) const {
    Keyframe result;
    result.time = time;
    result.position = glm::vec3(0.0f);
    result.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    result.scale = glm::vec3(1.0f);

    KeyframeValue val = Evaluate(time);

    switch (m_component) {
        case Component::Position:
            if (std::holds_alternative<glm::vec3>(val)) {
                result.position = std::get<glm::vec3>(val);
            }
            break;
        case Component::Rotation:
            if (std::holds_alternative<glm::quat>(val)) {
                result.rotation = std::get<glm::quat>(val);
            }
            break;
        case Component::Scale:
            if (std::holds_alternative<glm::vec3>(val)) {
                result.scale = std::get<glm::vec3>(val);
            }
            break;
        case Component::All:
            if (std::holds_alternative<glm::vec3>(val)) {
                result.position = std::get<glm::vec3>(val);
            }
            break;
    }

    return result;
}

// =============================================================================
// PropertyTrack Implementation
// =============================================================================

PropertyTrack::PropertyTrack(const std::string& name, const std::string& targetId,
                             const std::string& propertyPath, ValueType valueType)
    : AnimationTrackBase(name, targetId)
    , m_valueType(valueType)
{
    SetPropertyPath(propertyPath);

    // Set color based on value type
    switch (valueType) {
        case ValueType::Float:
            SetColor(glm::vec4(0.5f, 0.8f, 0.5f, 1.0f));
            break;
        case ValueType::Vec2:
            SetColor(glm::vec4(0.8f, 0.5f, 0.8f, 1.0f));
            break;
        case ValueType::Vec3:
            SetColor(glm::vec4(0.5f, 0.5f, 0.8f, 1.0f));
            break;
        case ValueType::Vec4:
            SetColor(glm::vec4(0.8f, 0.8f, 0.5f, 1.0f));
            break;
        case ValueType::Color:
            SetColor(glm::vec4(0.9f, 0.5f, 0.2f, 1.0f));
            break;
        case ValueType::Quaternion:
            SetColor(glm::vec4(0.3f, 0.8f, 0.8f, 1.0f));
            break;
    }
}

KeyframeValue PropertyTrack::Evaluate(float time) const {
    if (m_keyframes.empty()) {
        // Return default value based on type
        switch (m_valueType) {
            case ValueType::Float: return 0.0f;
            case ValueType::Vec2: return glm::vec2(0.0f);
            case ValueType::Vec3: return glm::vec3(0.0f);
            case ValueType::Vec4: return glm::vec4(0.0f);
            case ValueType::Color: return glm::vec4(1.0f);
            case ValueType::Quaternion: return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
        return 0.0f;
    }

    if (m_keyframes.size() == 1) {
        return m_keyframes[0].value;
    }

    // Find surrounding keyframes
    if (time <= m_keyframes.front().time) {
        return m_keyframes.front().value;
    }
    if (time >= m_keyframes.back().time) {
        return m_keyframes.back().value;
    }

    auto it = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), time);
    size_t idx = static_cast<size_t>(it - m_keyframes.begin());
    if (idx == 0) idx = 1;

    const auto& kf0 = m_keyframes[idx - 1];
    const auto& kf1 = m_keyframes[idx];

    float t = (time - kf0.time) / (kf1.time - kf0.time);
    KeyframeValue result = InterpolateKeyframeValues(kf0.value, kf1.value, t, kf0.interpolation);

    // Apply clamping if enabled
    if (m_hasRange && std::holds_alternative<float>(result)) {
        float val = std::get<float>(result);
        val = std::clamp(val, m_minValue, m_maxValue);
        result = val;
    }

    return result;
}

std::unique_ptr<IAnimationTrack> PropertyTrack::Clone() const {
    auto clone = std::make_unique<PropertyTrack>(m_name, m_targetId, m_propertyPath, m_valueType);
    clone->m_keyframes = m_keyframes;
    clone->m_color = m_color;
    clone->m_muted = m_muted;
    clone->m_locked = m_locked;
    clone->m_solo = m_solo;
    clone->m_expanded = m_expanded;
    clone->m_minValue = m_minValue;
    clone->m_maxValue = m_maxValue;
    clone->m_hasRange = m_hasRange;
    return clone;
}

// =============================================================================
// EventTrack Implementation
// =============================================================================

EventTrack::EventTrack(const std::string& name, const std::string& targetId)
    : AnimationTrackBase(name, targetId)
{
    SetPropertyPath("events");
    SetColor(glm::vec4(0.95f, 0.95f, 0.4f, 1.0f));  // Yellow
}

KeyframeValue EventTrack::Evaluate(float time) const {
    // Events don't interpolate - just return event name if at exact time
    auto idx = FindKeyframeAtTime(time, 0.016f);  // ~1 frame tolerance at 60fps
    if (idx.has_value() && idx.value() < m_events.size()) {
        return m_events[idx.value()].name;
    }
    return std::string("");
}

std::unique_ptr<IAnimationTrack> EventTrack::Clone() const {
    auto clone = std::make_unique<EventTrack>(m_name, m_targetId);
    clone->m_keyframes = m_keyframes;
    clone->m_events = m_events;
    clone->m_color = m_color;
    clone->m_muted = m_muted;
    clone->m_locked = m_locked;
    clone->m_solo = m_solo;
    clone->m_expanded = m_expanded;
    return clone;
}

size_t EventTrack::AddEvent(float time, const std::string& eventName, const std::string& parameter) {
    AnimationEvent evt;
    evt.name = eventName;
    evt.parameter = parameter;

    size_t idx = AddKeyframe(time, eventName);

    // Keep events array in sync with keyframes
    if (idx <= m_events.size()) {
        m_events.insert(m_events.begin() + static_cast<ptrdiff_t>(idx), evt);
    }

    return idx;
}

std::vector<EventTrack::AnimationEvent> EventTrack::GetEventsInRange(float startTime, float endTime) const {
    std::vector<AnimationEvent> result;
    auto indices = GetKeyframesInRange(startTime, endTime);
    for (size_t idx : indices) {
        if (idx < m_events.size()) {
            result.push_back(m_events[idx]);
        }
    }
    return result;
}

// =============================================================================
// SDFMorphTrack Implementation
// =============================================================================

SDFMorphTrack::SDFMorphTrack(const std::string& name, const std::string& targetId, SDFParameter param)
    : AnimationTrackBase(name, targetId)
    , m_parameter(param)
{
    switch (param) {
        case SDFParameter::BlendFactor:
            SetPropertyPath("sdf.blendFactor");
            break;
        case SDFParameter::Radius:
            SetPropertyPath("sdf.radius");
            break;
        case SDFParameter::Rounding:
            SetPropertyPath("sdf.rounding");
            break;
        case SDFParameter::Displacement:
            SetPropertyPath("sdf.displacement");
            break;
        case SDFParameter::Custom:
            SetPropertyPath("sdf.custom");
            break;
    }
    SetColor(glm::vec4(0.7f, 0.5f, 0.9f, 1.0f));  // Purple
}

SDFMorphTrack::SDFMorphTrack(const std::string& name, const std::string& targetId, const std::string& customParam)
    : AnimationTrackBase(name, targetId)
    , m_parameter(SDFParameter::Custom)
    , m_customParamName(customParam)
{
    SetPropertyPath("sdf." + customParam);
    SetColor(glm::vec4(0.7f, 0.5f, 0.9f, 1.0f));
}

KeyframeValue SDFMorphTrack::Evaluate(float time) const {
    if (m_keyframes.empty()) {
        return 0.0f;
    }

    if (m_keyframes.size() == 1) {
        return m_keyframes[0].value;
    }

    if (time <= m_keyframes.front().time) {
        return m_keyframes.front().value;
    }
    if (time >= m_keyframes.back().time) {
        return m_keyframes.back().value;
    }

    auto it = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), time);
    size_t idx = static_cast<size_t>(it - m_keyframes.begin());
    if (idx == 0) idx = 1;

    const auto& kf0 = m_keyframes[idx - 1];
    const auto& kf1 = m_keyframes[idx];

    float t = (time - kf0.time) / (kf1.time - kf0.time);
    return InterpolateKeyframeValues(kf0.value, kf1.value, t, kf0.interpolation);
}

std::unique_ptr<IAnimationTrack> SDFMorphTrack::Clone() const {
    std::unique_ptr<SDFMorphTrack> clone;
    if (m_parameter == SDFParameter::Custom) {
        clone = std::make_unique<SDFMorphTrack>(m_name, m_targetId, m_customParamName);
    } else {
        clone = std::make_unique<SDFMorphTrack>(m_name, m_targetId, m_parameter);
    }
    clone->m_keyframes = m_keyframes;
    clone->m_color = m_color;
    clone->m_muted = m_muted;
    clone->m_locked = m_locked;
    clone->m_solo = m_solo;
    clone->m_expanded = m_expanded;
    return clone;
}

void SDFMorphTrack::ApplyToModel(SDFModel* model, float time) const {
    if (!model) return;

    KeyframeValue val = Evaluate(time);
    if (!std::holds_alternative<float>(val)) return;

    float value = std::get<float>(val);

    // Apply to model based on parameter type
    // This would integrate with SDFModel API
    // Implementation depends on SDFModel interface
    (void)value;  // Suppress unused warning for now
}

// =============================================================================
// TimelineAnimationClip Implementation
// =============================================================================

TimelineAnimationClip::TimelineAnimationClip(const std::string& name)
    : m_name(name)
{
}

void TimelineAnimationClip::AddTrack(std::shared_ptr<IAnimationTrack> track) {
    if (track) {
        m_tracks.push_back(std::move(track));
    }
}

void TimelineAnimationClip::RemoveTrack(size_t index) {
    if (index >= m_tracks.size()) return;
    m_tracks.erase(m_tracks.begin() + static_cast<ptrdiff_t>(index));
}

void TimelineAnimationClip::RemoveTrack(const std::string& name) {
    m_tracks.erase(
        std::remove_if(m_tracks.begin(), m_tracks.end(),
            [&name](const auto& track) { return track->GetName() == name; }),
        m_tracks.end()
    );
}

void TimelineAnimationClip::MoveTrack(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= m_tracks.size() || toIndex >= m_tracks.size()) return;
    if (fromIndex == toIndex) return;

    auto track = std::move(m_tracks[fromIndex]);
    m_tracks.erase(m_tracks.begin() + static_cast<ptrdiff_t>(fromIndex));

    if (toIndex > fromIndex) {
        --toIndex;
    }
    m_tracks.insert(m_tracks.begin() + static_cast<ptrdiff_t>(toIndex), std::move(track));
}

IAnimationTrack* TimelineAnimationClip::GetTrack(size_t index) {
    if (index >= m_tracks.size()) return nullptr;
    return m_tracks[index].get();
}

const IAnimationTrack* TimelineAnimationClip::GetTrack(size_t index) const {
    if (index >= m_tracks.size()) return nullptr;
    return m_tracks[index].get();
}

IAnimationTrack* TimelineAnimationClip::GetTrack(const std::string& name) {
    for (auto& track : m_tracks) {
        if (track->GetName() == name) {
            return track.get();
        }
    }
    return nullptr;
}

void TimelineAnimationClip::AddGroup(const TrackGroup& group) {
    m_groups.push_back(group);
}

void TimelineAnimationClip::RemoveGroup(size_t index) {
    if (index >= m_groups.size()) return;
    m_groups.erase(m_groups.begin() + static_cast<ptrdiff_t>(index));
}

TrackGroup* TimelineAnimationClip::GetGroup(size_t index) {
    if (index >= m_groups.size()) return nullptr;
    return &m_groups[index];
}

void TimelineAnimationClip::RecalculateDuration() {
    m_duration = 0.0f;
    for (const auto& track : m_tracks) {
        m_duration = std::max(m_duration, track->GetDuration());
    }
}

std::unique_ptr<TimelineAnimationClip> TimelineAnimationClip::Clone() const {
    auto clone = std::make_unique<TimelineAnimationClip>(m_name);
    clone->m_duration = m_duration;
    clone->m_frameRate = m_frameRate;
    clone->m_looping = m_looping;

    for (const auto& track : m_tracks) {
        clone->m_tracks.push_back(track->Clone());
    }

    clone->m_groups = m_groups;
    return clone;
}

Animation TimelineAnimationClip::ToAnimation() const {
    Animation anim(m_name);
    anim.SetDuration(m_duration);
    anim.SetTicksPerSecond(m_frameRate);
    anim.SetLooping(m_looping);

    // Convert transform tracks to animation channels
    for (const auto& track : m_tracks) {
        if (track->GetType() == TrackType::Transform) {
            AnimationChannel channel;
            channel.nodeName = track->GetTargetId();
            channel.interpolationMode = InterpolationMode::Linear;

            for (const auto& kf : track->GetKeyframes()) {
                Nova::Keyframe animKf;
                animKf.time = kf.time;

                if (std::holds_alternative<glm::vec3>(kf.value)) {
                    animKf.position = std::get<glm::vec3>(kf.value);
                } else if (std::holds_alternative<glm::quat>(kf.value)) {
                    animKf.rotation = std::get<glm::quat>(kf.value);
                }

                channel.keyframes.push_back(animKf);
            }

            anim.AddChannel(std::move(channel));
        }
    }

    return anim;
}

void TimelineAnimationClip::FromAnimation(const Animation& animation) {
    m_name = animation.GetName();
    m_duration = animation.GetDuration();
    m_frameRate = animation.GetTicksPerSecond();
    m_looping = animation.IsLooping();
    m_tracks.clear();

    for (const auto& channel : animation.GetChannels()) {
        auto track = std::make_shared<TransformTrack>(
            channel.nodeName + "_transform",
            channel.nodeName,
            TransformTrack::Component::All
        );

        for (const auto& kf : channel.keyframes) {
            track->AddKeyframe(kf.time, kf.position);
        }

        m_tracks.push_back(std::move(track));
    }
}

SDFAnimationClip TimelineAnimationClip::ToSDFAnimation() const {
    SDFAnimationClip sdfClip(m_name);
    sdfClip.SetDuration(m_duration);
    sdfClip.SetFrameRate(m_frameRate);
    sdfClip.SetLooping(m_looping);

    // Collect all unique times from all tracks
    std::vector<float> allTimes;
    for (const auto& track : m_tracks) {
        for (const auto& kf : track->GetKeyframes()) {
            allTimes.push_back(kf.time);
        }
    }
    std::sort(allTimes.begin(), allTimes.end());
    allTimes.erase(std::unique(allTimes.begin(), allTimes.end()), allTimes.end());

    // Create pose keyframes at each unique time
    for (float time : allTimes) {
        SDFPoseKeyframe* poseKf = sdfClip.AddKeyframe(time);
        if (poseKf) {
            for (const auto& track : m_tracks) {
                KeyframeValue val = track->Evaluate(time);
                SDFTransform transform;

                if (std::holds_alternative<glm::vec3>(val)) {
                    transform.position = std::get<glm::vec3>(val);
                }

                poseKf->transforms[track->GetTargetId()] = transform;
            }
        }
    }

    return sdfClip;
}

void TimelineAnimationClip::FromSDFAnimation(const SDFAnimationClip& sdfAnim) {
    m_name = sdfAnim.GetName();
    m_duration = sdfAnim.GetDuration();
    m_frameRate = sdfAnim.GetFrameRate();
    m_looping = sdfAnim.IsLooping();
    m_tracks.clear();

    // Collect all primitive names from all keyframes
    std::unordered_set<std::string> primitiveNames;
    for (const auto& kf : sdfAnim.GetKeyframes()) {
        for (const auto& [name, transform] : kf.transforms) {
            primitiveNames.insert(name);
        }
    }

    // Create a track for each primitive
    for (const std::string& primName : primitiveNames) {
        auto track = std::make_shared<SDFMorphTrack>(
            primName + "_morph",
            primName,
            SDFMorphTrack::SDFParameter::BlendFactor
        );

        for (const auto& kf : sdfAnim.GetKeyframes()) {
            auto it = kf.transforms.find(primName);
            if (it != kf.transforms.end()) {
                // Use position.x as a proxy for blend factor
                track->AddKeyframe(kf.time, it->second.position.x);
            }
        }

        m_tracks.push_back(std::move(track));
    }
}

// =============================================================================
// Command Implementations
// =============================================================================

AddKeyframeCommand::AddKeyframeCommand(AnimationTimeline* timeline, IAnimationTrack* track,
                                       float time, const KeyframeValue& value)
    : m_timeline(timeline)
    , m_track(track)
    , m_time(time)
    , m_value(value)
{
}

bool AddKeyframeCommand::Execute() {
    if (!m_track) return false;
    m_keyframeIndex = m_track->AddKeyframe(m_time, m_value);
    return true;
}

bool AddKeyframeCommand::Undo() {
    if (!m_track) return false;
    m_track->RemoveKeyframe(m_keyframeIndex);
    return true;
}

RemoveKeyframeCommand::RemoveKeyframeCommand(AnimationTimeline* timeline, IAnimationTrack* track, size_t keyframeIndex)
    : m_timeline(timeline)
    , m_track(track)
    , m_keyframeIndex(keyframeIndex)
{
}

bool RemoveKeyframeCommand::Execute() {
    if (!m_track) return false;
    const TimelineKeyframe* kf = m_track->GetKeyframe(m_keyframeIndex);
    if (!kf) return false;

    m_removedKeyframe = *kf;
    m_track->RemoveKeyframe(m_keyframeIndex);
    return true;
}

bool RemoveKeyframeCommand::Undo() {
    if (!m_track) return false;
    m_track->AddKeyframe(m_removedKeyframe.time, m_removedKeyframe.value);
    return true;
}

MoveKeyframeCommand::MoveKeyframeCommand(AnimationTimeline* timeline, IAnimationTrack* track,
                                          size_t keyframeIndex, float newTime)
    : m_timeline(timeline)
    , m_track(track)
    , m_keyframeIndex(keyframeIndex)
    , m_newTime(newTime)
{
    if (m_track) {
        const TimelineKeyframe* kf = m_track->GetKeyframe(keyframeIndex);
        if (kf) {
            m_oldTime = kf->time;
        }
    }
}

bool MoveKeyframeCommand::Execute() {
    if (!m_track) return false;
    m_track->MoveKeyframe(m_keyframeIndex, m_newTime);
    return true;
}

bool MoveKeyframeCommand::Undo() {
    if (!m_track) return false;
    // Find the keyframe at new time and move it back
    auto idx = m_track->FindKeyframeAtTime(m_newTime, 0.001f);
    if (idx.has_value()) {
        m_track->MoveKeyframe(idx.value(), m_oldTime);
    }
    return true;
}

bool MoveKeyframeCommand::CanMergeWith(const ICommand& other) const {
    if (GetTypeId() != other.GetTypeId()) return false;
    if (!IsWithinMergeWindow()) return false;

    const auto* otherMove = dynamic_cast<const MoveKeyframeCommand*>(&other);
    if (!otherMove) return false;

    return m_track == otherMove->m_track && m_keyframeIndex == otherMove->m_keyframeIndex;
}

bool MoveKeyframeCommand::MergeWith(const ICommand& other) {
    const auto* otherMove = dynamic_cast<const MoveKeyframeCommand*>(&other);
    if (!otherMove) return false;

    m_newTime = otherMove->m_newTime;
    return true;
}

ChangeKeyframeValueCommand::ChangeKeyframeValueCommand(AnimationTimeline* timeline, IAnimationTrack* track,
                                                        size_t keyframeIndex, const KeyframeValue& newValue)
    : m_timeline(timeline)
    , m_track(track)
    , m_keyframeIndex(keyframeIndex)
    , m_newValue(newValue)
{
    if (m_track) {
        const TimelineKeyframe* kf = m_track->GetKeyframe(keyframeIndex);
        if (kf) {
            m_oldValue = kf->value;
        }
    }
}

bool ChangeKeyframeValueCommand::Execute() {
    if (!m_track) return false;
    m_track->SetKeyframeValue(m_keyframeIndex, m_newValue);
    return true;
}

bool ChangeKeyframeValueCommand::Undo() {
    if (!m_track) return false;
    m_track->SetKeyframeValue(m_keyframeIndex, m_oldValue);
    return true;
}

bool ChangeKeyframeValueCommand::CanMergeWith(const ICommand& other) const {
    if (GetTypeId() != other.GetTypeId()) return false;
    if (!IsWithinMergeWindow()) return false;

    const auto* otherChange = dynamic_cast<const ChangeKeyframeValueCommand*>(&other);
    if (!otherChange) return false;

    return m_track == otherChange->m_track && m_keyframeIndex == otherChange->m_keyframeIndex;
}

bool ChangeKeyframeValueCommand::MergeWith(const ICommand& other) {
    const auto* otherChange = dynamic_cast<const ChangeKeyframeValueCommand*>(&other);
    if (!otherChange) return false;

    m_newValue = otherChange->m_newValue;
    return true;
}

AddTrackCommand::AddTrackCommand(AnimationTimeline* timeline, std::shared_ptr<IAnimationTrack> track)
    : m_timeline(timeline)
    , m_track(std::move(track))
{
}

bool AddTrackCommand::Execute() {
    if (!m_timeline || !m_track) return false;
    auto* clip = m_timeline->GetAnimation();
    if (!clip) return false;

    clip->AddTrack(m_track);
    m_trackIndex = clip->GetTrackCount() - 1;
    return true;
}

bool AddTrackCommand::Undo() {
    if (!m_timeline) return false;
    auto* clip = m_timeline->GetAnimation();
    if (!clip) return false;

    clip->RemoveTrack(m_trackIndex);
    return true;
}

RemoveTrackCommand::RemoveTrackCommand(AnimationTimeline* timeline, size_t trackIndex)
    : m_timeline(timeline)
    , m_trackIndex(trackIndex)
{
}

bool RemoveTrackCommand::Execute() {
    if (!m_timeline) return false;
    auto* clip = m_timeline->GetAnimation();
    if (!clip) return false;

    const auto& tracks = clip->GetTracks();
    if (m_trackIndex >= tracks.size()) return false;

    m_removedTrack = tracks[m_trackIndex];
    clip->RemoveTrack(m_trackIndex);
    return true;
}

bool RemoveTrackCommand::Undo() {
    if (!m_timeline || !m_removedTrack) return false;
    auto* clip = m_timeline->GetAnimation();
    if (!clip) return false;

    // Re-insert at original position
    auto& tracks = const_cast<std::vector<std::shared_ptr<IAnimationTrack>>&>(clip->GetTracks());
    tracks.insert(tracks.begin() + static_cast<ptrdiff_t>(m_trackIndex), m_removedTrack);
    return true;
}

BatchKeyframeCommand::BatchKeyframeCommand(AnimationTimeline* timeline, const std::string& name,
                                            std::vector<KeyframeOperation> operations)
    : m_timeline(timeline)
    , m_name(name)
    , m_operations(std::move(operations))
{
}

bool BatchKeyframeCommand::Execute() {
    for (auto& op : m_operations) {
        if (!op.track) continue;

        switch (op.type) {
            case KeyframeOperation::Type::Move:
                op.track->MoveKeyframe(op.index, op.newKeyframe.time);
                break;
            case KeyframeOperation::Type::ChangeValue:
                op.track->SetKeyframeValue(op.index, op.newKeyframe.value);
                break;
            case KeyframeOperation::Type::Delete:
                op.track->RemoveKeyframe(op.index);
                break;
            case KeyframeOperation::Type::Add:
                op.track->AddKeyframe(op.newKeyframe.time, op.newKeyframe.value);
                break;
        }
    }
    return true;
}

bool BatchKeyframeCommand::Undo() {
    // Process in reverse order
    for (auto it = m_operations.rbegin(); it != m_operations.rend(); ++it) {
        auto& op = *it;
        if (!op.track) continue;

        switch (op.type) {
            case KeyframeOperation::Type::Move: {
                auto idx = op.track->FindKeyframeAtTime(op.newKeyframe.time, 0.001f);
                if (idx.has_value()) {
                    op.track->MoveKeyframe(idx.value(), op.oldKeyframe.time);
                }
                break;
            }
            case KeyframeOperation::Type::ChangeValue:
                op.track->SetKeyframeValue(op.index, op.oldKeyframe.value);
                break;
            case KeyframeOperation::Type::Delete:
                op.track->AddKeyframe(op.oldKeyframe.time, op.oldKeyframe.value);
                break;
            case KeyframeOperation::Type::Add:
                op.track->RemoveKeyframeAtTime(op.newKeyframe.time);
                break;
        }
    }
    return true;
}

// =============================================================================
// AnimationTimeline Implementation
// =============================================================================

AnimationTimeline::AnimationTimeline()
    : m_clip(std::make_shared<TimelineAnimationClip>("New Animation"))
{
    m_playback.lastUpdateTime = std::chrono::steady_clock::now();
}

AnimationTimeline::~AnimationTimeline() {
    Shutdown();
}

bool AnimationTimeline::Initialize(const Config& config) {
    if (!EditorPanel::Initialize(config)) {
        return false;
    }

    // Set default view range
    m_viewStartTime = 0.0f;
    m_viewEndTime = m_clip ? m_clip->GetDuration() : 5.0f;
    if (m_viewEndTime <= m_viewStartTime) {
        m_viewEndTime = m_viewStartTime + 5.0f;
    }

    return true;
}

void AnimationTimeline::Shutdown() {
    m_selectedKeyframes.clear();
    m_clipboard.clear();
    EditorPanel::Shutdown();
}

void AnimationTimeline::Update(float deltaTime) {
    EditorPanel::Update(deltaTime);
    UpdatePlayback(deltaTime);
}

void AnimationTimeline::OnUndo() {
    m_commandHistory.Undo();
}

void AnimationTimeline::OnRedo() {
    m_commandHistory.Redo();
}

bool AnimationTimeline::CanUndo() const {
    return m_commandHistory.CanUndo();
}

bool AnimationTimeline::CanRedo() const {
    return m_commandHistory.CanRedo();
}

void AnimationTimeline::OnInitialize() {
    // Setup default configuration
}

void AnimationTimeline::OnShutdown() {
    Stop();
}

// =============================================================================
// Animation Clip Management
// =============================================================================

void AnimationTimeline::NewAnimation(const std::string& name) {
    m_clip = std::make_shared<TimelineAnimationClip>(name);
    m_selectedKeyframes.clear();
    m_selectedTrackIndex = -1;
    m_commandHistory.Clear();

    m_viewStartTime = 0.0f;
    m_viewEndTime = 5.0f;

    if (OnAnimationModified) {
        OnAnimationModified();
    }
}

void AnimationTimeline::SetAnimation(std::shared_ptr<TimelineAnimationClip> clip) {
    m_clip = std::move(clip);
    m_selectedKeyframes.clear();
    m_selectedTrackIndex = -1;
    m_commandHistory.Clear();

    if (m_clip) {
        m_viewStartTime = 0.0f;
        m_viewEndTime = std::max(m_clip->GetDuration(), 1.0f);
    }
}

void AnimationTimeline::ImportAnimation(const Animation& animation) {
    m_clip = std::make_shared<TimelineAnimationClip>();
    m_clip->FromAnimation(animation);
    m_selectedKeyframes.clear();
    m_selectedTrackIndex = -1;
    m_commandHistory.Clear();

    m_viewStartTime = 0.0f;
    m_viewEndTime = std::max(m_clip->GetDuration(), 1.0f);
}

void AnimationTimeline::ImportSDFAnimation(const SDFAnimationClip& sdfAnim) {
    m_clip = std::make_shared<TimelineAnimationClip>();
    m_clip->FromSDFAnimation(sdfAnim);
    m_selectedKeyframes.clear();
    m_selectedTrackIndex = -1;
    m_commandHistory.Clear();

    m_viewStartTime = 0.0f;
    m_viewEndTime = std::max(m_clip->GetDuration(), 1.0f);
}

Animation AnimationTimeline::ExportAnimation() const {
    if (m_clip) {
        return m_clip->ToAnimation();
    }
    return Animation();
}

SDFAnimationClip AnimationTimeline::ExportSDFAnimation() const {
    if (m_clip) {
        return m_clip->ToSDFAnimation();
    }
    return SDFAnimationClip();
}

// =============================================================================
// Track Management
// =============================================================================

void AnimationTimeline::AddTrack(std::shared_ptr<IAnimationTrack> track) {
    if (!m_clip || !track) return;

    auto cmd = std::make_unique<AddTrackCommand>(this, std::move(track));
    m_commandHistory.ExecuteCommand(std::move(cmd));

    if (OnAnimationModified) {
        OnAnimationModified();
    }
}

void AnimationTimeline::RemoveTrack(size_t index) {
    if (!m_clip || index >= m_clip->GetTrackCount()) return;

    auto cmd = std::make_unique<RemoveTrackCommand>(this, index);
    m_commandHistory.ExecuteCommand(std::move(cmd));

    if (OnAnimationModified) {
        OnAnimationModified();
    }
}

IAnimationTrack* AnimationTimeline::GetSelectedTrack() {
    if (!m_clip || m_selectedTrackIndex < 0) return nullptr;
    return m_clip->GetTrack(static_cast<size_t>(m_selectedTrackIndex));
}

// =============================================================================
// Keyframe Operations
// =============================================================================

void AnimationTimeline::SetKeyframe() {
    IAnimationTrack* track = GetSelectedTrack();
    if (!track) return;

    // Get current value (this would need to query the actual property)
    KeyframeValue value = track->Evaluate(m_playback.currentTime);
    SetKeyframe(track, m_playback.currentTime, value);
}

void AnimationTimeline::SetKeyframe(IAnimationTrack* track, float time, const KeyframeValue& value) {
    if (!track) return;

    auto cmd = std::make_unique<AddKeyframeCommand>(this, track, time, value);
    m_commandHistory.ExecuteCommand(std::move(cmd));

    if (OnAnimationModified) {
        OnAnimationModified();
    }
}

void AnimationTimeline::DeleteSelectedKeyframes() {
    if (m_selectedKeyframes.empty()) return;

    std::vector<BatchKeyframeCommand::KeyframeOperation> ops;

    for (const auto& sel : m_selectedKeyframes) {
        if (!sel.track) continue;
        const TimelineKeyframe* kf = sel.track->GetKeyframe(sel.keyframeIndex);
        if (!kf) continue;

        BatchKeyframeCommand::KeyframeOperation op;
        op.track = sel.track;
        op.index = sel.keyframeIndex;
        op.oldKeyframe = *kf;
        op.type = BatchKeyframeCommand::KeyframeOperation::Type::Delete;
        ops.push_back(op);
    }

    if (!ops.empty()) {
        auto cmd = std::make_unique<BatchKeyframeCommand>(this, "Delete Keyframes", std::move(ops));
        m_commandHistory.ExecuteCommand(std::move(cmd));
    }

    m_selectedKeyframes.clear();

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
    if (OnAnimationModified) {
        OnAnimationModified();
    }
}

void AnimationTimeline::CopyKeyframes() {
    m_clipboard.clear();

    if (m_selectedKeyframes.empty()) return;

    // Find earliest time for relative positioning
    float earliestTime = std::numeric_limits<float>::max();
    for (const auto& sel : m_selectedKeyframes) {
        if (sel.track) {
            const TimelineKeyframe* kf = sel.track->GetKeyframe(sel.keyframeIndex);
            if (kf) {
                earliestTime = std::min(earliestTime, kf->time);
            }
        }
    }

    for (const auto& sel : m_selectedKeyframes) {
        if (!sel.track) continue;
        const TimelineKeyframe* kf = sel.track->GetKeyframe(sel.keyframeIndex);
        if (!kf) continue;

        ClipboardKeyframe clipKf;
        clipKf.relativeTime = kf->time - earliestTime;
        clipKf.value = kf->value;
        clipKf.interpolation = kf->interpolation;
        clipKf.tangent = kf->tangent;
        m_clipboard.push_back(clipKf);
    }
}

void AnimationTimeline::PasteKeyframes() {
    if (m_clipboard.empty()) return;

    IAnimationTrack* track = GetSelectedTrack();
    if (!track) return;

    std::vector<BatchKeyframeCommand::KeyframeOperation> ops;

    for (const auto& clipKf : m_clipboard) {
        float time = m_playback.currentTime + clipKf.relativeTime;

        BatchKeyframeCommand::KeyframeOperation op;
        op.track = track;
        op.newKeyframe.time = time;
        op.newKeyframe.value = clipKf.value;
        op.newKeyframe.interpolation = clipKf.interpolation;
        op.newKeyframe.tangent = clipKf.tangent;
        op.type = BatchKeyframeCommand::KeyframeOperation::Type::Add;
        ops.push_back(op);
    }

    if (!ops.empty()) {
        auto cmd = std::make_unique<BatchKeyframeCommand>(this, "Paste Keyframes", std::move(ops));
        m_commandHistory.ExecuteCommand(std::move(cmd));
    }

    if (OnAnimationModified) {
        OnAnimationModified();
    }
}

void AnimationTimeline::DuplicateKeyframes(float timeOffset) {
    CopyKeyframes();

    // Offset paste time
    float originalTime = m_playback.currentTime;

    // Find latest selected keyframe time
    float latestTime = 0.0f;
    for (const auto& sel : m_selectedKeyframes) {
        if (sel.track) {
            const TimelineKeyframe* kf = sel.track->GetKeyframe(sel.keyframeIndex);
            if (kf) {
                latestTime = std::max(latestTime, kf->time);
            }
        }
    }

    m_playback.currentTime = latestTime + timeOffset;
    PasteKeyframes();
    m_playback.currentTime = originalTime;
}

void AnimationTimeline::SelectAllKeyframes() {
    m_selectedKeyframes.clear();

    if (!m_clip) return;

    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track) continue;

        for (size_t j = 0; j < track->GetKeyframeCount(); ++j) {
            KeyframeSelection sel;
            sel.track = track;
            sel.keyframeIndex = j;
            m_selectedKeyframes.insert(sel);
        }
    }

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

void AnimationTimeline::ClearSelection() {
    m_selectedKeyframes.clear();
    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

// =============================================================================
// Playback Controls
// =============================================================================

void AnimationTimeline::Play() {
    m_playback.isPlaying = true;
    m_playback.lastUpdateTime = std::chrono::steady_clock::now();

    if (OnPlaybackStateChanged) {
        OnPlaybackStateChanged(true);
    }
}

void AnimationTimeline::Pause() {
    m_playback.isPlaying = false;

    if (OnPlaybackStateChanged) {
        OnPlaybackStateChanged(false);
    }
}

void AnimationTimeline::Stop() {
    m_playback.isPlaying = false;
    m_playback.currentTime = m_playback.startTime;

    if (OnPlaybackStateChanged) {
        OnPlaybackStateChanged(false);
    }
    if (OnTimeChanged) {
        OnTimeChanged(m_playback.currentTime);
    }
}

void AnimationTimeline::TogglePlayback() {
    if (m_playback.isPlaying) {
        Pause();
    } else {
        Play();
    }
}

void AnimationTimeline::StepForward() {
    if (!m_clip) return;

    float frameTime = 1.0f / m_clip->GetFrameRate();
    SetCurrentTime(m_playback.currentTime + frameTime);
}

void AnimationTimeline::StepBackward() {
    if (!m_clip) return;

    float frameTime = 1.0f / m_clip->GetFrameRate();
    SetCurrentTime(m_playback.currentTime - frameTime);
}

void AnimationTimeline::GoToStart() {
    SetCurrentTime(m_playback.startTime);
}

void AnimationTimeline::GoToEnd() {
    if (m_clip) {
        SetCurrentTime(m_clip->GetDuration());
    }
}

void AnimationTimeline::GoToNextKeyframe() {
    if (!m_clip) return;

    float nextTime = std::numeric_limits<float>::max();

    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track) continue;

        for (const auto& kf : track->GetKeyframes()) {
            if (kf.time > m_playback.currentTime + 0.001f) {
                nextTime = std::min(nextTime, kf.time);
                break;
            }
        }
    }

    if (nextTime < std::numeric_limits<float>::max()) {
        SetCurrentTime(nextTime);
    }
}

void AnimationTimeline::GoToPreviousKeyframe() {
    if (!m_clip) return;

    float prevTime = -std::numeric_limits<float>::max();

    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track) continue;

        for (auto it = track->GetKeyframes().rbegin(); it != track->GetKeyframes().rend(); ++it) {
            if (it->time < m_playback.currentTime - 0.001f) {
                prevTime = std::max(prevTime, it->time);
                break;
            }
        }
    }

    if (prevTime > -std::numeric_limits<float>::max()) {
        SetCurrentTime(prevTime);
    }
}

void AnimationTimeline::SetCurrentTime(float time) {
    float prevTime = m_playback.currentTime;
    m_playback.currentTime = std::max(0.0f, time);

    if (m_clip && m_playback.currentTime > m_clip->GetDuration()) {
        if (m_playback.isLooping) {
            m_playback.currentTime = std::fmod(m_playback.currentTime, m_clip->GetDuration());
        } else {
            m_playback.currentTime = m_clip->GetDuration();
        }
    }

    // Fire events that occurred during this time change
    if (m_playback.isPlaying) {
        FireEventsInRange(prevTime, m_playback.currentTime);
    }

    if (OnTimeChanged) {
        OnTimeChanged(m_playback.currentTime);
    }
}

// =============================================================================
// Auto-Key Mode
// =============================================================================

void AnimationTimeline::RecordValueChange(const std::string& targetId, const std::string& propertyPath,
                                           const KeyframeValue& value) {
    if (!m_autoKeyEnabled || !m_clip) return;

    // Find matching track
    IAnimationTrack* track = nullptr;
    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* t = m_clip->GetTrack(i);
        if (t && t->GetTargetId() == targetId && t->GetPropertyPath() == propertyPath) {
            track = t;
            break;
        }
    }

    if (track) {
        SetKeyframe(track, m_playback.currentTime, value);
    }
}

// =============================================================================
// View Controls
// =============================================================================

void AnimationTimeline::ToggleCurveEditor() {
    m_showCurveEditor = !m_showCurveEditor;
}

void AnimationTimeline::SetVisibleTimeRange(float startTime, float endTime) {
    m_viewStartTime = startTime;
    m_viewEndTime = endTime;
}

void AnimationTimeline::ZoomToFit() {
    if (!m_clip || m_clip->GetTrackCount() == 0) return;

    float minTime = std::numeric_limits<float>::max();
    float maxTime = 0.0f;

    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track || track->GetKeyframeCount() == 0) continue;

        minTime = std::min(minTime, track->GetKeyframes().front().time);
        maxTime = std::max(maxTime, track->GetKeyframes().back().time);
    }

    if (minTime > maxTime) {
        minTime = 0.0f;
        maxTime = 5.0f;
    }

    // Add padding
    float padding = (maxTime - minTime) * 0.1f;
    m_viewStartTime = std::max(0.0f, minTime - padding);
    m_viewEndTime = maxTime + padding;
}

void AnimationTimeline::ZoomToSelection() {
    if (m_selectedKeyframes.empty()) return;

    float minTime = std::numeric_limits<float>::max();
    float maxTime = 0.0f;

    for (const auto& sel : m_selectedKeyframes) {
        if (!sel.track) continue;
        const TimelineKeyframe* kf = sel.track->GetKeyframe(sel.keyframeIndex);
        if (!kf) continue;

        minTime = std::min(minTime, kf->time);
        maxTime = std::max(maxTime, kf->time);
    }

    if (minTime > maxTime) return;

    float padding = (maxTime - minTime) * 0.2f;
    if (padding < 0.5f) padding = 0.5f;

    m_viewStartTime = std::max(0.0f, minTime - padding);
    m_viewEndTime = maxTime + padding;
}

void AnimationTimeline::FrameSelection() {
    ZoomToSelection();
}

// =============================================================================
// Rendering
// =============================================================================

void AnimationTimeline::OnRender() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    const auto& colors = EditorTheme::Instance().GetColors();

    // Main layout: track list on left, timeline on right
    float availWidth = ImGui::GetContentRegionAvail().x;
    float availHeight = ImGui::GetContentRegionAvail().y;

    // Track list
    ImGui::BeginChild("TrackList", ImVec2(m_trackListWidth, availHeight), true);
    RenderTrackList();
    ImGui::EndChild();

    // Splitter
    ImGui::SameLine();
    ImGui::InvisibleButton("Splitter", ImVec2(4.0f, availHeight));
    if (ImGui::IsItemActive()) {
        m_trackListWidth += ImGui::GetIO().MouseDelta.x;
        m_trackListWidth = std::clamp(m_trackListWidth, 100.0f, availWidth * 0.5f);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    // Timeline area
    ImGui::SameLine();
    ImGui::BeginChild("TimelineArea", ImVec2(0, availHeight), true);

    m_timelineAreaPos = glm::vec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    m_timelineAreaSize = glm::vec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    RenderTimelineArea();
    ImGui::EndChild();

    ImGui::PopStyleVar();

    // Handle input
    HandleInput();
}

void AnimationTimeline::RenderTrackList() {
    const auto& colors = EditorTheme::Instance().GetColors();

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorTheme::ToImVec4(colors.panelHeader));
    ImGui::BeginChild("TrackListHeader", ImVec2(0, m_rulerHeight), true);
    ImGui::Text("Tracks");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Track list
    if (!m_clip) return;

    ImGui::BeginChild("TrackListContent", ImVec2(0, 0), false);

    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track) continue;

        ImGui::PushID(static_cast<int>(i));

        bool isSelected = (static_cast<int>(i) == m_selectedTrackIndex);

        // Track row background
        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetContentRegionAvail().x, rowMin.y + m_trackHeight);

        if (isSelected) {
            ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, EditorTheme::ToImU32(colors.selection));
        }

        // Track controls
        RenderTrackControls(track, ImGui::GetCursorPosY());

        // Track name
        ImGui::SameLine(50.0f);
        if (ImGui::Selectable(track->GetName().c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, m_trackHeight - 4))) {
            m_selectedTrackIndex = static_cast<int>(i);
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

void AnimationTimeline::RenderTimelineArea() {
    // Time ruler at top
    RenderTimeRuler();

    // Track content area
    m_trackAreaPos = glm::vec2(m_timelineAreaPos.x, m_timelineAreaPos.y + m_rulerHeight);
    m_trackAreaSize = glm::vec2(m_timelineAreaSize.x, m_timelineAreaSize.y - m_rulerHeight);

    ImGui::SetCursorPosY(m_rulerHeight);
    ImGui::BeginChild("TrackContent", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

    if (m_showCurveEditor) {
        RenderCurveEditor();
    } else {
        RenderTracks();
    }

    // Render playhead
    RenderPlayhead();

    // Box selection overlay
    if (m_isBoxSelecting) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min(std::min(m_boxSelectStart.x, m_boxSelectEnd.x), std::min(m_boxSelectStart.y, m_boxSelectEnd.y));
        ImVec2 max(std::max(m_boxSelectStart.x, m_boxSelectEnd.x), std::max(m_boxSelectStart.y, m_boxSelectEnd.y));

        const auto& colors = EditorTheme::Instance().GetColors();
        drawList->AddRectFilled(min, max, EditorTheme::ToImU32(glm::vec4(colors.selection.r, colors.selection.g, colors.selection.b, 0.3f)));
        drawList->AddRect(min, max, EditorTheme::ToImU32(colors.accent));
    }

    ImGui::EndChild();
}

void AnimationTimeline::RenderTimeRuler() {
    const auto& colors = EditorTheme::Instance().GetColors();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 rulerPos = ImGui::GetCursorScreenPos();
    ImVec2 rulerSize(m_timelineAreaSize.x, m_rulerHeight);

    // Background
    drawList->AddRectFilled(rulerPos, ImVec2(rulerPos.x + rulerSize.x, rulerPos.y + rulerSize.y),
                            EditorTheme::ToImU32(colors.panelHeader));

    // Calculate tick spacing based on zoom
    float visibleDuration = m_viewEndTime - m_viewStartTime;
    float pixelsPerSecond = rulerSize.x / visibleDuration;

    // Determine tick interval (in seconds)
    float tickInterval = 0.1f;
    if (pixelsPerSecond < 20) tickInterval = 1.0f;
    else if (pixelsPerSecond < 50) tickInterval = 0.5f;
    else if (pixelsPerSecond < 100) tickInterval = 0.2f;
    else if (pixelsPerSecond < 200) tickInterval = 0.1f;
    else tickInterval = 0.05f;

    int majorEvery = 10;
    if (tickInterval >= 0.5f) majorEvery = 2;
    else if (tickInterval >= 0.2f) majorEvery = 5;

    // Draw ticks
    float startTick = std::ceil(m_viewStartTime / tickInterval) * tickInterval;
    int tickIndex = static_cast<int>(std::round(startTick / tickInterval));

    for (float t = startTick; t <= m_viewEndTime; t += tickInterval, ++tickIndex) {
        float x = rulerPos.x + TimeToPixel(t);
        bool isMajor = (tickIndex % majorEvery) == 0;

        float tickHeight = isMajor ? RULER_MAJOR_TICK_HEIGHT : RULER_MINOR_TICK_HEIGHT;
        drawList->AddLine(
            ImVec2(x, rulerPos.y + rulerSize.y - tickHeight),
            ImVec2(x, rulerPos.y + rulerSize.y),
            EditorTheme::ToImU32(colors.text),
            1.0f
        );

        // Draw time label for major ticks
        if (isMajor) {
            std::ostringstream ss;
            if (m_showFrameNumbers && m_clip) {
                ss << static_cast<int>(t * m_clip->GetFrameRate());
            } else {
                ss << std::fixed << std::setprecision(2) << t << "s";
            }
            std::string label = ss.str();
            drawList->AddText(ImVec2(x + 2, rulerPos.y + 2), EditorTheme::ToImU32(colors.text), label.c_str());
        }
    }

    // Move cursor past ruler
    ImGui::SetCursorPosY(m_rulerHeight);
}

void AnimationTimeline::RenderPlayhead() {
    const auto& colors = EditorTheme::Instance().GetColors();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float x = m_timelineAreaPos.x + TimeToPixel(m_playback.currentTime);

    // Playhead line
    drawList->AddLine(
        ImVec2(x, m_timelineAreaPos.y),
        ImVec2(x, m_timelineAreaPos.y + m_timelineAreaSize.y),
        EditorTheme::ToImU32(colors.accent),
        2.0f
    );

    // Playhead handle (triangle at top)
    float handleSize = 8.0f;
    drawList->AddTriangleFilled(
        ImVec2(x - handleSize, m_timelineAreaPos.y),
        ImVec2(x + handleSize, m_timelineAreaPos.y),
        ImVec2(x, m_timelineAreaPos.y + handleSize * 1.5f),
        EditorTheme::ToImU32(colors.accent)
    );
}

void AnimationTimeline::RenderTracks() {
    if (!m_clip) return;

    const auto& colors = EditorTheme::Instance().GetColors();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float yOffset = 0.0f;

    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track) continue;

        RenderTrack(track, yOffset, m_trackHeight);
        yOffset += m_trackHeight;
    }
}

void AnimationTimeline::RenderTrack(IAnimationTrack* track, float yOffset, float trackHeight) {
    const auto& colors = EditorTheme::Instance().GetColors();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Track background (alternating)
    ImVec2 trackMin(m_trackAreaPos.x, m_trackAreaPos.y + yOffset);
    ImVec2 trackMax(m_trackAreaPos.x + m_trackAreaSize.x, trackMin.y + trackHeight);

    // Track lane background
    drawList->AddRectFilled(trackMin, trackMax, EditorTheme::ToImU32(colors.backgroundAlt));

    // Track separator line
    drawList->AddLine(
        ImVec2(trackMin.x, trackMax.y),
        ImVec2(trackMax.x, trackMax.y),
        EditorTheme::ToImU32(colors.border)
    );

    // Render keyframes
    for (size_t j = 0; j < track->GetKeyframeCount(); ++j) {
        const TimelineKeyframe* kf = track->GetKeyframe(j);
        if (!kf) continue;

        float x = m_trackAreaPos.x + TimeToPixel(kf->time);
        float y = m_trackAreaPos.y + yOffset + trackHeight * 0.5f;

        bool isSelected = IsKeyframeSelected(track, j);
        RenderKeyframe(*kf, track, j, glm::vec2(x, y), KEYFRAME_SIZE);
    }
}

void AnimationTimeline::RenderKeyframe(const TimelineKeyframe& keyframe, IAnimationTrack* track,
                                        size_t index, const glm::vec2& pos, float size) {
    const auto& colors = EditorTheme::Instance().GetColors();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    bool isSelected = IsKeyframeSelected(track, index);
    glm::vec4 color = track->GetColor();

    if (isSelected) {
        color = colors.accent;
    }

    // Draw keyframe diamond shape
    ImVec2 points[4] = {
        ImVec2(pos.x, pos.y - size),
        ImVec2(pos.x + size, pos.y),
        ImVec2(pos.x, pos.y + size),
        ImVec2(pos.x - size, pos.y)
    };

    drawList->AddConvexPolyFilled(points, 4, EditorTheme::ToImU32(color));

    if (isSelected) {
        drawList->AddPolyline(points, 4, EditorTheme::ToImU32(colors.textHighlight), true, 2.0f);
    }
}

void AnimationTimeline::RenderCurveEditor() {
    if (!m_clip) return;

    const auto& colors = EditorTheme::Instance().GetColors();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw grid
    float gridSpacing = 50.0f;
    for (float x = m_trackAreaPos.x; x < m_trackAreaPos.x + m_trackAreaSize.x; x += gridSpacing) {
        drawList->AddLine(
            ImVec2(x, m_trackAreaPos.y),
            ImVec2(x, m_trackAreaPos.y + m_trackAreaSize.y),
            EditorTheme::ToImU32(colors.nodeGrid)
        );
    }
    for (float y = m_trackAreaPos.y; y < m_trackAreaPos.y + m_trackAreaSize.y; y += gridSpacing) {
        drawList->AddLine(
            ImVec2(m_trackAreaPos.x, y),
            ImVec2(m_trackAreaPos.x + m_trackAreaSize.x, y),
            EditorTheme::ToImU32(colors.nodeGrid)
        );
    }

    // Render curves for each track
    float yOffset = 0.0f;
    float curveHeight = 100.0f;

    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track || track->IsMuted()) continue;

        RenderCurveForTrack(track, yOffset, curveHeight);
        yOffset += curveHeight;
    }
}

void AnimationTimeline::RenderCurveForTrack(IAnimationTrack* track, float yOffset, float height) {
    if (!track || track->GetKeyframeCount() < 2) return;

    const auto& colors = EditorTheme::Instance().GetColors();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec4 curveColor = track->GetColor();

    // Draw curve segments
    const auto& keyframes = track->GetKeyframes();
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        const auto& kf0 = keyframes[i];
        const auto& kf1 = keyframes[i + 1];

        if (kf0.interpolation == KeyframeInterpolation::Bezier) {
            RenderBezierCurve(kf0, kf1, yOffset, height, curveColor);
        } else {
            // Linear interpolation
            float x0 = m_trackAreaPos.x + TimeToPixel(kf0.time);
            float x1 = m_trackAreaPos.x + TimeToPixel(kf1.time);

            float v0 = 0.5f, v1 = 0.5f;
            if (std::holds_alternative<float>(kf0.value)) {
                v0 = std::get<float>(kf0.value);
            }
            if (std::holds_alternative<float>(kf1.value)) {
                v1 = std::get<float>(kf1.value);
            }

            float y0 = m_trackAreaPos.y + yOffset + height * (1.0f - v0);
            float y1 = m_trackAreaPos.y + yOffset + height * (1.0f - v1);

            drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), EditorTheme::ToImU32(curveColor), 2.0f);
        }
    }

    // Draw keyframe points
    for (size_t i = 0; i < keyframes.size(); ++i) {
        const auto& kf = keyframes[i];

        float x = m_trackAreaPos.x + TimeToPixel(kf.time);
        float v = 0.5f;
        if (std::holds_alternative<float>(kf.value)) {
            v = std::get<float>(kf.value);
        }
        float y = m_trackAreaPos.y + yOffset + height * (1.0f - v);

        glm::vec2 pos(x, y);
        bool isSelected = IsKeyframeSelected(track, i);

        // Draw keyframe circle
        drawList->AddCircleFilled(ImVec2(pos.x, pos.y), 5.0f,
            EditorTheme::ToImU32(isSelected ? colors.accent : curveColor));

        // Draw tangent handles for bezier keyframes
        if (isSelected && kf.interpolation == KeyframeInterpolation::Bezier) {
            RenderTangentHandle(kf, track, i, pos, 1.0f);
        }
    }
}

void AnimationTimeline::RenderBezierCurve(const TimelineKeyframe& kf0, const TimelineKeyframe& kf1,
                                           float yOffset, float height, const glm::vec4& color) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float x0 = m_trackAreaPos.x + TimeToPixel(kf0.time);
    float x1 = m_trackAreaPos.x + TimeToPixel(kf1.time);

    float v0 = 0.5f, v1 = 0.5f;
    if (std::holds_alternative<float>(kf0.value)) {
        v0 = std::get<float>(kf0.value);
    }
    if (std::holds_alternative<float>(kf1.value)) {
        v1 = std::get<float>(kf1.value);
    }

    float y0 = m_trackAreaPos.y + yOffset + height * (1.0f - v0);
    float y1 = m_trackAreaPos.y + yOffset + height * (1.0f - v1);

    // Calculate control points from tangents
    float pixelsPerSecond = (x1 - x0) / (kf1.time - kf0.time);
    float cx0 = x0 + kf0.tangent.outTangent.x * pixelsPerSecond;
    float cy0 = y0 - kf0.tangent.outTangent.y * height;
    float cx1 = x1 + kf1.tangent.inTangent.x * pixelsPerSecond;
    float cy1 = y1 - kf1.tangent.inTangent.y * height;

    drawList->AddBezierCubic(
        ImVec2(x0, y0), ImVec2(cx0, cy0), ImVec2(cx1, cy1), ImVec2(x1, y1),
        EditorTheme::ToImU32(color), 2.0f
    );
}

void AnimationTimeline::RenderTangentHandle(const TimelineKeyframe& keyframe, IAnimationTrack* track,
                                             size_t index, const glm::vec2& keyPos, float scale) {
    const auto& colors = EditorTheme::Instance().GetColors();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float handleLength = TANGENT_LINE_LENGTH * scale;

    // In tangent
    glm::vec2 inHandle = keyPos + glm::normalize(glm::vec2(keyframe.tangent.inTangent.x, -keyframe.tangent.inTangent.y)) * handleLength;
    drawList->AddLine(ImVec2(keyPos.x, keyPos.y), ImVec2(inHandle.x, inHandle.y),
                      EditorTheme::ToImU32(colors.textSecondary));
    drawList->AddCircleFilled(ImVec2(inHandle.x, inHandle.y), TANGENT_HANDLE_SIZE,
                              EditorTheme::ToImU32(colors.accent));

    // Out tangent
    glm::vec2 outHandle = keyPos + glm::normalize(glm::vec2(keyframe.tangent.outTangent.x, -keyframe.tangent.outTangent.y)) * handleLength;
    drawList->AddLine(ImVec2(keyPos.x, keyPos.y), ImVec2(outHandle.x, outHandle.y),
                      EditorTheme::ToImU32(colors.textSecondary));
    drawList->AddCircleFilled(ImVec2(outHandle.x, outHandle.y), TANGENT_HANDLE_SIZE,
                              EditorTheme::ToImU32(colors.accent));
}

void AnimationTimeline::RenderTrackControls(IAnimationTrack* track, float yOffset) {
    if (!track) return;

    ImGui::SetCursorPosY(yOffset + 2);

    // Mute button
    bool muted = track->IsMuted();
    if (ImGui::SmallButton(muted ? "M" : "m")) {
        track->SetMuted(!muted);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Mute track");
    }

    ImGui::SameLine();

    // Solo button
    bool solo = track->IsSolo();
    if (ImGui::SmallButton(solo ? "S" : "s")) {
        track->SetSolo(!solo);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Solo track");
    }
}

void AnimationTimeline::OnRenderToolbar() {
    const auto& colors = EditorTheme::Instance().GetColors();

    // Playback controls
    if (ImGui::Button("|<")) GoToStart();
    ImGui::SameLine();
    if (ImGui::Button("<")) StepBackward();
    ImGui::SameLine();
    if (ImGui::Button(m_playback.isPlaying ? "||" : ">")) TogglePlayback();
    ImGui::SameLine();
    if (ImGui::Button(">|")) StepForward();
    ImGui::SameLine();
    if (ImGui::Button(">|")) GoToEnd();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Current time display
    ImGui::SetNextItemWidth(80);
    float time = m_playback.currentTime;
    if (ImGui::DragFloat("##Time", &time, 0.01f, 0.0f, m_clip ? m_clip->GetDuration() : 10.0f, "%.2fs")) {
        SetCurrentTime(time);
    }

    ImGui::SameLine();

    // Playback speed
    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat("##Speed", &m_playback.playbackSpeed, 0.1f, 0.1f, 4.0f, "%.1fx");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Playback speed");
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Loop toggle
    if (ImGui::Checkbox("Loop", &m_playback.isLooping)) {}

    ImGui::SameLine();

    // Auto-key toggle
    ImGui::PushStyleColor(ImGuiCol_Button, m_autoKeyEnabled ?
        EditorTheme::ToImVec4(colors.error) : EditorTheme::ToImVec4(colors.button));
    if (ImGui::Button("Auto")) {
        m_autoKeyEnabled = !m_autoKeyEnabled;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Auto-key mode (record property changes)");
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // View toggle
    if (ImGui::Button(m_showCurveEditor ? "Dopesheet" : "Curves")) {
        ToggleCurveEditor();
    }

    ImGui::SameLine();

    // Snap toggle
    if (ImGui::Checkbox("Snap", &m_snapToFrames)) {}

    ImGui::SameLine();

    // Zoom controls
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderFloat("##Zoom", &m_zoom, MIN_ZOOM, MAX_ZOOM, "Zoom: %.1f")) {
        // Update view range based on zoom
    }
}

void AnimationTimeline::OnRenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) {
                OnUndo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) {
                OnRedo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, !m_selectedKeyframes.empty())) {
                CopyKeyframes();
                DeleteSelectedKeyframes();
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selectedKeyframes.empty())) {
                CopyKeyframes();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) {
                PasteKeyframes();
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !m_selectedKeyframes.empty())) {
                DuplicateKeyframes();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                SelectAllKeyframes();
            }
            if (ImGui::MenuItem("Deselect All", "Escape")) {
                ClearSelection();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Track")) {
            RenderAddTrackMenu();
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Track", nullptr, false, m_selectedTrackIndex >= 0)) {
                RemoveTrack(static_cast<size_t>(m_selectedTrackIndex));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Zoom to Fit", "F")) {
                ZoomToFit();
            }
            if (ImGui::MenuItem("Zoom to Selection", "Shift+F", false, !m_selectedKeyframes.empty())) {
                ZoomToSelection();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Show Curves", nullptr, m_showCurveEditor)) {
                ToggleCurveEditor();
            }
            if (ImGui::MenuItem("Show Frame Numbers", nullptr, m_showFrameNumbers)) {
                m_showFrameNumbers = !m_showFrameNumbers;
            }
            if (ImGui::MenuItem("Snap to Frames", nullptr, m_snapToFrames)) {
                m_snapToFrames = !m_snapToFrames;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void AnimationTimeline::OnRenderStatusBar() {
    // Selection info
    ImGui::Text("Selected: %zu keyframes", m_selectedKeyframes.size());
    ImGui::SameLine(150);

    // Time info
    ImGui::Text("Time: %.2fs", m_playback.currentTime);
    ImGui::SameLine(300);

    // Duration
    if (m_clip) {
        ImGui::Text("Duration: %.2fs", m_clip->GetDuration());
        ImGui::SameLine(450);
        ImGui::Text("FPS: %.0f", m_clip->GetFrameRate());
    }
}

void AnimationTimeline::RenderAddTrackMenu() {
    if (ImGui::BeginMenu("Add Track")) {
        if (ImGui::MenuItem("Transform Track")) {
            auto track = std::make_shared<TransformTrack>("New Transform", "");
            AddTrack(std::move(track));
        }
        if (ImGui::MenuItem("Position Track")) {
            auto track = std::make_shared<TransformTrack>("Position", "", TransformTrack::Component::Position);
            AddTrack(std::move(track));
        }
        if (ImGui::MenuItem("Rotation Track")) {
            auto track = std::make_shared<TransformTrack>("Rotation", "", TransformTrack::Component::Rotation);
            AddTrack(std::move(track));
        }
        if (ImGui::MenuItem("Scale Track")) {
            auto track = std::make_shared<TransformTrack>("Scale", "", TransformTrack::Component::Scale);
            AddTrack(std::move(track));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Float Property")) {
            auto track = std::make_shared<PropertyTrack>("Float Property", "", "property", PropertyTrack::ValueType::Float);
            AddTrack(std::move(track));
        }
        if (ImGui::MenuItem("Color Property")) {
            auto track = std::make_shared<PropertyTrack>("Color Property", "", "color", PropertyTrack::ValueType::Color);
            AddTrack(std::move(track));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Event Track")) {
            auto track = std::make_shared<EventTrack>("Events", "");
            AddTrack(std::move(track));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("SDF Blend Factor")) {
            auto track = std::make_shared<SDFMorphTrack>("SDF Blend", "", SDFMorphTrack::SDFParameter::BlendFactor);
            AddTrack(std::move(track));
        }
        if (ImGui::MenuItem("SDF Radius")) {
            auto track = std::make_shared<SDFMorphTrack>("SDF Radius", "", SDFMorphTrack::SDFParameter::Radius);
            AddTrack(std::move(track));
        }
        ImGui::EndMenu();
    }
}

// =============================================================================
// Input Handling
// =============================================================================

void AnimationTimeline::HandleInput() {
    HandleKeyboardShortcuts();
    HandleTimelineInput();
}

void AnimationTimeline::HandleTimelineInput() {
    ImGuiIO& io = ImGui::GetIO();

    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) return;

    // Check if mouse is in timeline area
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

    // Handle playhead dragging (clicking on ruler)
    if (mousePos.y >= m_timelineAreaPos.y && mousePos.y < m_timelineAreaPos.y + m_rulerHeight) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_dragMode = DragMode::Playhead;
            float time = PixelToTime(mousePos.x - m_timelineAreaPos.x);
            SetCurrentTime(time);
        }
    }

    // Handle dragging
    if (m_dragMode == DragMode::Playhead && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        float time = PixelToTime(mousePos.x - m_timelineAreaPos.x);
        if (m_snapToFrames && m_clip) {
            float frameTime = 1.0f / m_clip->GetFrameRate();
            time = std::round(time / frameTime) * frameTime;
        }
        SetCurrentTime(time);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_dragMode == DragMode::Keyframe) {
            // Finalize keyframe move
        }
        m_dragMode = DragMode::None;
        m_isBoxSelecting = false;
    }

    // Check for keyframe clicks
    if (mousePos.y >= m_trackAreaPos.y && mousePos.y < m_trackAreaPos.y + m_trackAreaSize.y) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            auto hit = GetKeyframeAtPosition(mousePos);
            if (hit.has_value()) {
                bool addToSelection = io.KeyCtrl;
                SelectKeyframe(hit->first, hit->second, addToSelection);
                m_dragMode = DragMode::Keyframe;
                m_dragTrack = hit->first;
                m_dragKeyframeIndex = hit->second;
                m_dragStart = mousePos;
            } else if (!io.KeyCtrl) {
                ClearSelection();
                // Start box selection
                m_isBoxSelecting = true;
                m_boxSelectStart = mousePos;
                m_boxSelectEnd = mousePos;
                m_dragMode = DragMode::BoxSelect;
            }
        }
    }

    // Box selection update
    if (m_dragMode == DragMode::BoxSelect && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        m_boxSelectEnd = mousePos;
        UpdateSelectionRect(m_boxSelectStart, m_boxSelectEnd);
    }

    // Keyframe dragging
    if (m_dragMode == DragMode::Keyframe && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        HandleKeyframeDrag();
    }

    // Zoom with scroll
    if (io.MouseWheel != 0.0f) {
        float zoomDelta = io.MouseWheel * 0.1f;
        m_zoom = std::clamp(m_zoom + zoomDelta, MIN_ZOOM, MAX_ZOOM);

        // Adjust view range based on zoom
        float visibleDuration = (m_viewEndTime - m_viewStartTime) / m_zoom;
        float center = (m_viewStartTime + m_viewEndTime) * 0.5f;
        m_viewStartTime = center - visibleDuration * 0.5f;
        m_viewEndTime = center + visibleDuration * 0.5f;
        m_viewStartTime = std::max(0.0f, m_viewStartTime);
    }

    // Pan with middle mouse
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        float timeDelta = -io.MouseDelta.x / TimeToPixel(1.0f);
        m_viewStartTime += timeDelta;
        m_viewEndTime += timeDelta;
        m_viewStartTime = std::max(0.0f, m_viewStartTime);
    }
}

void AnimationTimeline::HandleKeyboardShortcuts() {
    ImGuiIO& io = ImGui::GetIO();

    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) return;

    // Playback shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        TogglePlayback();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        GoToStart();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_End)) {
        GoToEnd();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        if (io.KeyShift) {
            GoToPreviousKeyframe();
        } else {
            StepBackward();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        if (io.KeyShift) {
            GoToNextKeyframe();
        } else {
            StepForward();
        }
    }

    // Edit shortcuts
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (io.KeyShift) {
                OnRedo();
            } else {
                OnUndo();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
            OnRedo();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_C)) {
            CopyKeyframes();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_V)) {
            PasteKeyframes();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_X)) {
            CopyKeyframes();
            DeleteSelectedKeyframes();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_D)) {
            DuplicateKeyframes();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_A)) {
            SelectAllKeyframes();
        }
    }

    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        DeleteSelectedKeyframes();
    }

    // Set keyframe
    if (ImGui::IsKeyPressed(ImGuiKey_K)) {
        SetKeyframe();
    }

    // View shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        if (io.KeyShift) {
            ZoomToSelection();
        } else {
            ZoomToFit();
        }
    }

    // Escape - clear selection
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ClearSelection();
    }
}

void AnimationTimeline::HandleKeyframeDrag() {
    if (!m_dragTrack || m_dragMode != DragMode::Keyframe) return;

    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

    float newTime = PixelToTime(mousePos.x - m_timelineAreaPos.x);

    // Snap to frames if enabled
    if (m_snapToFrames && m_clip) {
        float frameTime = 1.0f / m_clip->GetFrameRate();
        newTime = std::round(newTime / frameTime) * frameTime;
    }

    newTime = std::max(0.0f, newTime);

    // Move all selected keyframes by the same delta
    const TimelineKeyframe* draggedKf = m_dragTrack->GetKeyframe(m_dragKeyframeIndex);
    if (!draggedKf) return;

    float timeDelta = newTime - draggedKf->time;

    if (std::abs(timeDelta) < 0.001f) return;

    std::vector<BatchKeyframeCommand::KeyframeOperation> ops;

    for (const auto& sel : m_selectedKeyframes) {
        if (!sel.track) continue;
        const TimelineKeyframe* kf = sel.track->GetKeyframe(sel.keyframeIndex);
        if (!kf) continue;

        BatchKeyframeCommand::KeyframeOperation op;
        op.track = sel.track;
        op.index = sel.keyframeIndex;
        op.oldKeyframe = *kf;
        op.newKeyframe = *kf;
        op.newKeyframe.time = std::max(0.0f, kf->time + timeDelta);
        op.type = BatchKeyframeCommand::KeyframeOperation::Type::Move;
        ops.push_back(op);
    }

    // Apply moves directly (command created on release)
    for (const auto& op : ops) {
        op.track->MoveKeyframe(op.index, op.newKeyframe.time);
    }
}

// =============================================================================
// Selection
// =============================================================================

void AnimationTimeline::SelectKeyframe(IAnimationTrack* track, size_t index, bool addToSelection) {
    if (!addToSelection) {
        m_selectedKeyframes.clear();
    }

    KeyframeSelection sel;
    sel.track = track;
    sel.keyframeIndex = index;
    m_selectedKeyframes.insert(sel);

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

void AnimationTimeline::DeselectKeyframe(IAnimationTrack* track, size_t index) {
    KeyframeSelection sel;
    sel.track = track;
    sel.keyframeIndex = index;
    m_selectedKeyframes.erase(sel);

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

bool AnimationTimeline::IsKeyframeSelected(IAnimationTrack* track, size_t index) const {
    KeyframeSelection sel;
    sel.track = track;
    sel.keyframeIndex = index;
    return m_selectedKeyframes.find(sel) != m_selectedKeyframes.end();
}

void AnimationTimeline::UpdateSelectionRect(const glm::vec2& start, const glm::vec2& end) {
    if (!m_clip) return;

    float minX = std::min(start.x, end.x);
    float maxX = std::max(start.x, end.x);
    float minY = std::min(start.y, end.y);
    float maxY = std::max(start.y, end.y);

    m_selectedKeyframes.clear();

    float yOffset = 0.0f;
    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track) continue;

        float trackTop = m_trackAreaPos.y + yOffset;
        float trackBottom = trackTop + m_trackHeight;

        // Check if track intersects selection rect
        if (trackBottom >= minY && trackTop <= maxY) {
            for (size_t j = 0; j < track->GetKeyframeCount(); ++j) {
                const TimelineKeyframe* kf = track->GetKeyframe(j);
                if (!kf) continue;

                float x = m_trackAreaPos.x + TimeToPixel(kf->time);
                float y = trackTop + m_trackHeight * 0.5f;

                // Check if keyframe is within selection rect
                if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                    KeyframeSelection sel;
                    sel.track = track;
                    sel.keyframeIndex = j;
                    m_selectedKeyframes.insert(sel);
                }
            }
        }

        yOffset += m_trackHeight;
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

float AnimationTimeline::TimeToPixel(float time) const {
    float visibleDuration = m_viewEndTime - m_viewStartTime;
    if (visibleDuration <= 0.0f) return 0.0f;
    return ((time - m_viewStartTime) / visibleDuration) * m_timelineAreaSize.x;
}

float AnimationTimeline::PixelToTime(float pixel) const {
    float visibleDuration = m_viewEndTime - m_viewStartTime;
    return m_viewStartTime + (pixel / m_timelineAreaSize.x) * visibleDuration;
}

float AnimationTimeline::ValueToPixel(float value, float minVal, float maxVal, float height) const {
    if (maxVal <= minVal) return height * 0.5f;
    float normalized = (value - minVal) / (maxVal - minVal);
    return height * (1.0f - normalized);
}

float AnimationTimeline::PixelToValue(float pixel, float minVal, float maxVal, float height) const {
    if (height <= 0.0f) return minVal;
    float normalized = 1.0f - (pixel / height);
    return minVal + normalized * (maxVal - minVal);
}

glm::vec2 AnimationTimeline::GetKeyframeScreenPos(IAnimationTrack* track, size_t index) const {
    if (!track || !m_clip) return glm::vec2(0.0f);

    const TimelineKeyframe* kf = track->GetKeyframe(index);
    if (!kf) return glm::vec2(0.0f);

    // Find track y position
    float yOffset = 0.0f;
    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        if (m_clip->GetTrack(i) == track) {
            break;
        }
        yOffset += m_trackHeight;
    }

    float x = m_trackAreaPos.x + TimeToPixel(kf->time);
    float y = m_trackAreaPos.y + yOffset + m_trackHeight * 0.5f;

    return glm::vec2(x, y);
}

IAnimationTrack* AnimationTimeline::GetTrackAtPosition(float y) const {
    if (!m_clip) return nullptr;

    float yOffset = m_trackAreaPos.y;
    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        if (y >= yOffset && y < yOffset + m_trackHeight) {
            return m_clip->GetTrack(i);
        }
        yOffset += m_trackHeight;
    }
    return nullptr;
}

std::optional<std::pair<IAnimationTrack*, size_t>> AnimationTimeline::GetKeyframeAtPosition(const glm::vec2& pos) const {
    if (!m_clip) return std::nullopt;

    float yOffset = 0.0f;
    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track) continue;

        float trackY = m_trackAreaPos.y + yOffset + m_trackHeight * 0.5f;

        for (size_t j = 0; j < track->GetKeyframeCount(); ++j) {
            const TimelineKeyframe* kf = track->GetKeyframe(j);
            if (!kf) continue;

            float kfX = m_trackAreaPos.x + TimeToPixel(kf->time);

            float dx = pos.x - kfX;
            float dy = pos.y - trackY;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= KEYFRAME_HIT_RADIUS) {
                return std::make_pair(track, j);
            }
        }

        yOffset += m_trackHeight;
    }

    return std::nullopt;
}

void AnimationTimeline::UpdatePlayback(float deltaTime) {
    if (!m_playback.isPlaying || !m_clip) return;

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - m_playback.lastUpdateTime).count();
    m_playback.lastUpdateTime = now;

    float prevTime = m_playback.currentTime;
    float newTime = m_playback.currentTime + elapsed * m_playback.playbackSpeed;

    if (newTime >= m_clip->GetDuration()) {
        if (m_playback.isLooping) {
            newTime = std::fmod(newTime, m_clip->GetDuration());
        } else {
            newTime = m_clip->GetDuration();
            m_playback.isPlaying = false;
            if (OnPlaybackStateChanged) {
                OnPlaybackStateChanged(false);
            }
        }
    }

    m_playback.currentTime = newTime;

    // Fire events
    FireEventsInRange(prevTime, newTime);

    if (OnTimeChanged) {
        OnTimeChanged(m_playback.currentTime);
    }
}

void AnimationTimeline::FireEventsInRange(float startTime, float endTime) {
    if (!m_clip || !OnAnimationEvent) return;

    for (size_t i = 0; i < m_clip->GetTrackCount(); ++i) {
        IAnimationTrack* track = m_clip->GetTrack(i);
        if (!track || track->GetType() != TrackType::Event || track->IsMuted()) continue;

        EventTrack* eventTrack = static_cast<EventTrack*>(track);
        auto events = eventTrack->GetEventsInRange(startTime, endTime);

        for (const auto& evt : events) {
            OnAnimationEvent(evt);
        }
    }
}

// =============================================================================
// Global Utility Functions
// =============================================================================

KeyframeValue InterpolateKeyframeValues(const KeyframeValue& a, const KeyframeValue& b,
                                         float t, KeyframeInterpolation mode) {
    if (mode == KeyframeInterpolation::Step) {
        return a;  // No interpolation, use first value
    }

    // Handle float interpolation
    if (std::holds_alternative<float>(a) && std::holds_alternative<float>(b)) {
        float va = std::get<float>(a);
        float vb = std::get<float>(b);
        return va + (vb - va) * t;
    }

    // Handle vec2 interpolation
    if (std::holds_alternative<glm::vec2>(a) && std::holds_alternative<glm::vec2>(b)) {
        glm::vec2 va = std::get<glm::vec2>(a);
        glm::vec2 vb = std::get<glm::vec2>(b);
        return va + (vb - va) * t;
    }

    // Handle vec3 interpolation
    if (std::holds_alternative<glm::vec3>(a) && std::holds_alternative<glm::vec3>(b)) {
        glm::vec3 va = std::get<glm::vec3>(a);
        glm::vec3 vb = std::get<glm::vec3>(b);
        return va + (vb - va) * t;
    }

    // Handle vec4 interpolation
    if (std::holds_alternative<glm::vec4>(a) && std::holds_alternative<glm::vec4>(b)) {
        glm::vec4 va = std::get<glm::vec4>(a);
        glm::vec4 vb = std::get<glm::vec4>(b);
        return va + (vb - va) * t;
    }

    // Handle quaternion interpolation (slerp)
    if (std::holds_alternative<glm::quat>(a) && std::holds_alternative<glm::quat>(b)) {
        glm::quat qa = std::get<glm::quat>(a);
        glm::quat qb = std::get<glm::quat>(b);
        return glm::slerp(qa, qb, t);
    }

    // For strings/bools, no interpolation
    return t < 0.5f ? a : b;
}

float EvaluateBezier(float p0, float p1, float p2, float p3, float t) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    return uuu * p0 + 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + ttt * p3;
}

glm::vec2 EvaluateBezier2D(const glm::vec2& p0, const glm::vec2& c0,
                            const glm::vec2& c1, const glm::vec2& p1, float t) {
    return glm::vec2(
        EvaluateBezier(p0.x, c0.x, c1.x, p1.x, t),
        EvaluateBezier(p0.y, c0.y, c1.y, p1.y, t)
    );
}

float FindBezierT(float x, float p0x, float c0x, float c1x, float p1x, float tolerance) {
    // Newton-Raphson iteration to find t where bezier x equals target x
    float t = 0.5f;
    for (int i = 0; i < 10; ++i) {
        float currentX = EvaluateBezier(p0x, c0x, c1x, p1x, t);
        float error = currentX - x;

        if (std::abs(error) < tolerance) {
            break;
        }

        // Derivative of cubic bezier
        float u = 1.0f - t;
        float derivative = 3.0f * u * u * (c0x - p0x) +
                          6.0f * u * t * (c1x - c0x) +
                          3.0f * t * t * (p1x - c1x);

        if (std::abs(derivative) < 0.0001f) break;

        t -= error / derivative;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    return t;
}

std::string KeyframeValueToString(const KeyframeValue& value) {
    std::ostringstream ss;

    std::visit([&ss](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, float>) {
            ss << std::fixed << std::setprecision(3) << arg;
        }
        else if constexpr (std::is_same_v<T, glm::vec2>) {
            ss << "(" << std::fixed << std::setprecision(2) << arg.x << ", " << arg.y << ")";
        }
        else if constexpr (std::is_same_v<T, glm::vec3>) {
            ss << "(" << std::fixed << std::setprecision(2) << arg.x << ", " << arg.y << ", " << arg.z << ")";
        }
        else if constexpr (std::is_same_v<T, glm::vec4>) {
            ss << "(" << std::fixed << std::setprecision(2) << arg.x << ", " << arg.y << ", " << arg.z << ", " << arg.w << ")";
        }
        else if constexpr (std::is_same_v<T, glm::quat>) {
            ss << "quat(" << std::fixed << std::setprecision(2) << arg.w << ", " << arg.x << ", " << arg.y << ", " << arg.z << ")";
        }
        else if constexpr (std::is_same_v<T, bool>) {
            ss << (arg ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            ss << "\"" << arg << "\"";
        }
    }, value);

    return ss.str();
}

glm::vec4 GetTrackTypeColor(TrackType type) {
    switch (type) {
        case TrackType::Transform:
            return glm::vec4(0.9f, 0.7f, 0.3f, 1.0f);  // Orange
        case TrackType::Property:
            return glm::vec4(0.5f, 0.8f, 0.5f, 1.0f);  // Green
        case TrackType::Event:
            return glm::vec4(0.95f, 0.95f, 0.4f, 1.0f);  // Yellow
        case TrackType::SDFMorph:
            return glm::vec4(0.7f, 0.5f, 0.9f, 1.0f);  // Purple
        default:
            return glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);  // Gray
    }
}

} // namespace Nova
