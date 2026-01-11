#include "animation/Animation.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <cmath>

namespace Nova {

Animation::Animation(const std::string& name)
    : m_name(name)
{}

void Animation::AddChannel(const AnimationChannel& channel) {
    m_channelLookup[channel.nodeName] = m_channels.size();
    m_channels.push_back(channel);

    // Update duration based on last keyframe
    if (!channel.keyframes.empty()) {
        m_duration = std::max(m_duration, channel.keyframes.back().time);
    }
}

void Animation::AddChannel(AnimationChannel&& channel) {
    // Update duration before moving
    if (!channel.keyframes.empty()) {
        m_duration = std::max(m_duration, channel.keyframes.back().time);
    }

    m_channelLookup[channel.nodeName] = m_channels.size();
    m_channels.push_back(std::move(channel));
}

std::unordered_map<std::string, glm::mat4> Animation::Evaluate(float time) const {
    std::unordered_map<std::string, glm::mat4> result;
    result.reserve(m_channels.size());

    for (const auto& channel : m_channels) {
        result.emplace(channel.nodeName, channel.Evaluate(time));
    }

    return result;
}

void Animation::EvaluateInto(float time, std::unordered_map<std::string, glm::mat4>& outTransforms) const {
    for (const auto& channel : m_channels) {
        outTransforms[channel.nodeName] = channel.Evaluate(time);
    }
}

const AnimationChannel* Animation::GetChannel(const std::string& nodeName) const {
    auto it = m_channelLookup.find(nodeName);
    if (it != m_channelLookup.end()) {
        return &m_channels[it->second];
    }
    return nullptr;
}

AnimationChannel* Animation::GetChannel(const std::string& nodeName) {
    auto it = m_channelLookup.find(nodeName);
    if (it != m_channelLookup.end()) {
        return &m_channels[it->second];
    }
    return nullptr;
}

void Animation::ResetCaches() const {
    for (const auto& channel : m_channels) {
        channel.ResetCache();
    }
}

// AnimationChannel implementation

size_t AnimationChannel::FindKeyframeIndex(float time) const noexcept {
    if (keyframes.empty()) {
        return 0;
    }

    // Check if cached index is still valid (sequential playback optimization)
    if (lastKeyframeIndex < keyframes.size() - 1) {
        if (keyframes[lastKeyframeIndex].time <= time &&
            keyframes[lastKeyframeIndex + 1].time > time) {
            return lastKeyframeIndex;
        }
    }

    // Binary search for the keyframe
    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), time,
        [](const Keyframe& kf, float t) { return kf.time < t; });

    size_t index = 0;
    if (it == keyframes.end()) {
        index = keyframes.size() - 1;
    } else if (it == keyframes.begin()) {
        index = 0;
    } else {
        index = static_cast<size_t>(std::distance(keyframes.begin(), it) - 1);
    }

    lastKeyframeIndex = index;
    return index;
}

glm::mat4 AnimationChannel::Evaluate(float time) const {
    Keyframe kf = Interpolate(time);

    // Compose TRS matrix (Translation * Rotation * Scale)
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), kf.position);
    glm::mat4 rotation = glm::mat4_cast(kf.rotation);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), kf.scale);

    return translation * rotation * scale;
}

Keyframe AnimationChannel::Interpolate(float time) const {
    if (keyframes.empty()) {
        return Keyframe{};
    }

    if (keyframes.size() == 1) {
        return keyframes[0];
    }

    // Handle time before first keyframe
    if (time <= keyframes.front().time) {
        return keyframes.front();
    }

    // Handle time after last keyframe
    if (time >= keyframes.back().time) {
        return keyframes.back();
    }

    // Find surrounding keyframes using binary search
    size_t prevIndex = FindKeyframeIndex(time);
    size_t nextIndex = prevIndex + 1;

    if (nextIndex >= keyframes.size()) {
        return keyframes[prevIndex];
    }

    const Keyframe& prev = keyframes[prevIndex];
    const Keyframe& next = keyframes[nextIndex];

    // Calculate interpolation factor
    float deltaTime = next.time - prev.time;
    float t = (deltaTime > 0.0f) ? (time - prev.time) / deltaTime : 0.0f;

    // Apply interpolation based on mode
    Keyframe result;
    result.time = time;

    switch (interpolationMode) {
        case InterpolationMode::Step:
            return prev;

        case InterpolationMode::CatmullRom: {
            // Get additional keyframes for Catmull-Rom
            size_t p0Idx = (prevIndex > 0) ? prevIndex - 1 : prevIndex;
            size_t p3Idx = (nextIndex + 1 < keyframes.size()) ? nextIndex + 1 : nextIndex;

            result.position = Interpolation::CatmullRom(
                keyframes[p0Idx].position, prev.position,
                next.position, keyframes[p3Idx].position, t);
            result.rotation = Interpolation::Slerp(prev.rotation, next.rotation, t);
            result.scale = Interpolation::Lerp(prev.scale, next.scale, t);
            break;
        }

        case InterpolationMode::Cubic: {
            // Smooth step for cubic feel
            float smoothT = Interpolation::SmoothStep(t);
            result.position = Interpolation::Lerp(prev.position, next.position, smoothT);
            result.rotation = Interpolation::Slerp(prev.rotation, next.rotation, smoothT);
            result.scale = Interpolation::Lerp(prev.scale, next.scale, smoothT);
            break;
        }

        case InterpolationMode::Linear:
        default:
            result.position = Interpolation::Lerp(prev.position, next.position, t);
            result.rotation = Interpolation::Slerp(prev.rotation, next.rotation, t);
            result.scale = Interpolation::Lerp(prev.scale, next.scale, t);
            break;
    }

    return result;
}

// Blending functions

Keyframe BlendKeyframes(const Keyframe& a, const Keyframe& b, float weight) {
    Keyframe result;
    result.time = a.time; // Time from first keyframe
    result.position = Interpolation::Lerp(a.position, b.position, weight);
    result.rotation = Interpolation::Slerp(a.rotation, b.rotation, weight);
    result.scale = Interpolation::Lerp(a.scale, b.scale, weight);
    return result;
}

glm::mat4 BlendTransforms(const glm::mat4& a, const glm::mat4& b, float weight) {
    // Decompose matrices
    glm::vec3 scaleA, scaleB;
    glm::quat rotA, rotB;
    glm::vec3 transA, transB;
    glm::vec3 skewA, skewB;
    glm::vec4 perspA, perspB;

    glm::decompose(a, scaleA, rotA, transA, skewA, perspA);
    glm::decompose(b, scaleB, rotB, transB, skewB, perspB);

    // Blend components
    glm::vec3 blendedTrans = Interpolation::Lerp(transA, transB, weight);
    glm::quat blendedRot = Interpolation::Slerp(rotA, rotB, weight);
    glm::vec3 blendedScale = Interpolation::Lerp(scaleA, scaleB, weight);

    // Recompose matrix
    glm::mat4 result = glm::translate(glm::mat4(1.0f), blendedTrans);
    result *= glm::mat4_cast(blendedRot);
    result = glm::scale(result, blendedScale);

    return result;
}

// Interpolation namespace implementations

namespace Interpolation {

glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) noexcept {
    // GLM's mix is optimized and may use SIMD
    return glm::mix(a, b, t);
}

glm::quat Slerp(const glm::quat& a, const glm::quat& b, float t) noexcept {
    return glm::slerp(a, b, t);
}

glm::quat Nlerp(const glm::quat& a, const glm::quat& b, float t) noexcept {
    // Normalized linear interpolation - faster than slerp for small angles
    glm::quat result = glm::mix(a, b, t);
    return glm::normalize(result);
}

float EaseInOut(float t) noexcept {
    if (t < 0.5f) {
        return 2.0f * t * t;
    }
    float adjusted = -2.0f * t + 2.0f;
    return 1.0f - (adjusted * adjusted) * 0.5f;
}

glm::vec3 CatmullRom(const glm::vec3& p0, const glm::vec3& p1,
                      const glm::vec3& p2, const glm::vec3& p3, float t) noexcept {
    float t2 = t * t;
    float t3 = t2 * t;

    // Catmull-Rom spline coefficients
    glm::vec3 result = 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );

    return result;
}

glm::vec3 CubicBezier(const glm::vec3& p0, const glm::vec3& p1,
                       const glm::vec3& p2, const glm::vec3& p3, float t) noexcept {
    float u = 1.0f - t;
    float u2 = u * u;
    float u3 = u2 * u;
    float t2 = t * t;
    float t3 = t2 * t;

    return u3 * p0 + 3.0f * u2 * t * p1 + 3.0f * u * t2 * p2 + t3 * p3;
}

glm::quat Squad(const glm::quat& q0, const glm::quat& q1,
                 const glm::quat& s0, const glm::quat& s1, float t) noexcept {
    // Spherical cubic interpolation
    glm::quat slerpQ = glm::slerp(q0, q1, t);
    glm::quat slerpS = glm::slerp(s0, s1, t);
    return glm::slerp(slerpQ, slerpS, 2.0f * t * (1.0f - t));
}

} // namespace Interpolation

} // namespace Nova
