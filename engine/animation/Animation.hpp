#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

/**
 * @brief Animation keyframe with position, rotation, and scale
 */
struct Keyframe {
    float time = 0.0f;
    glm::vec3 position{0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1};
};

/**
 * @brief Animation channel for a single bone/node
 */
struct AnimationChannel {
    std::string nodeName;
    std::vector<Keyframe> keyframes;

    glm::mat4 Evaluate(float time) const;
    Keyframe Interpolate(float time) const;
};

/**
 * @brief Animation clip containing multiple channels
 */
class Animation {
public:
    Animation() = default;
    explicit Animation(const std::string& name);

    void AddChannel(const AnimationChannel& channel);

    /**
     * @brief Evaluate all channels at a given time
     */
    std::unordered_map<std::string, glm::mat4> Evaluate(float time) const;

    // Properties
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    float GetDuration() const { return m_duration; }
    void SetDuration(float duration) { m_duration = duration; }

    float GetTicksPerSecond() const { return m_ticksPerSecond; }
    void SetTicksPerSecond(float tps) { m_ticksPerSecond = tps; }

    const std::vector<AnimationChannel>& GetChannels() const { return m_channels; }

private:
    std::string m_name;
    float m_duration = 0.0f;
    float m_ticksPerSecond = 25.0f;
    std::vector<AnimationChannel> m_channels;
};

/**
 * @brief Interpolation methods
 */
namespace Interpolation {
    glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t);
    glm::quat Slerp(const glm::quat& a, const glm::quat& b, float t);
    float SmoothStep(float t);
    float EaseInOut(float t);

    // Hermite spline interpolation
    glm::vec3 CatmullRom(const glm::vec3& p0, const glm::vec3& p1,
                          const glm::vec3& p2, const glm::vec3& p3, float t);
}

} // namespace Nova
