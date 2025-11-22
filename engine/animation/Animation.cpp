#include "animation/Animation.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Nova {

Animation::Animation(const std::string& name)
    : m_name(name)
{}

void Animation::AddChannel(const AnimationChannel& channel) {
    m_channels.push_back(channel);

    // Update duration
    for (const auto& kf : channel.keyframes) {
        m_duration = std::max(m_duration, kf.time);
    }
}

std::unordered_map<std::string, glm::mat4> Animation::Evaluate(float time) const {
    std::unordered_map<std::string, glm::mat4> result;

    for (const auto& channel : m_channels) {
        result[channel.nodeName] = channel.Evaluate(time);
    }

    return result;
}

glm::mat4 AnimationChannel::Evaluate(float time) const {
    Keyframe kf = Interpolate(time);

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

    // Find surrounding keyframes
    size_t nextIndex = 0;
    for (size_t i = 0; i < keyframes.size(); ++i) {
        if (keyframes[i].time > time) {
            nextIndex = i;
            break;
        }
        nextIndex = i;
    }

    size_t prevIndex = nextIndex > 0 ? nextIndex - 1 : 0;

    if (prevIndex == nextIndex) {
        return keyframes[prevIndex];
    }

    const Keyframe& prev = keyframes[prevIndex];
    const Keyframe& next = keyframes[nextIndex];

    float t = (time - prev.time) / (next.time - prev.time);
    t = std::clamp(t, 0.0f, 1.0f);

    Keyframe result;
    result.time = time;
    result.position = Interpolation::Lerp(prev.position, next.position, t);
    result.rotation = Interpolation::Slerp(prev.rotation, next.rotation, t);
    result.scale = Interpolation::Lerp(prev.scale, next.scale, t);

    return result;
}

namespace Interpolation {

glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) {
    return a + (b - a) * t;
}

glm::quat Slerp(const glm::quat& a, const glm::quat& b, float t) {
    return glm::slerp(a, b, t);
}

float SmoothStep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float EaseInOut(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

glm::vec3 CatmullRom(const glm::vec3& p0, const glm::vec3& p1,
                      const glm::vec3& p2, const glm::vec3& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

} // namespace Interpolation

} // namespace Nova
