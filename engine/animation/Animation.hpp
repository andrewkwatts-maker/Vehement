/**
 * @file Animation.hpp
 * @brief Core animation data structures and interpolation utilities
 *
 * This file provides the fundamental building blocks for the animation system:
 * - Keyframe: A single point in time with position, rotation, and scale
 * - AnimationChannel: A sequence of keyframes for one bone/node
 * - Animation: A collection of channels forming a complete animation clip
 *
 * @section animation_concepts Key Concepts
 *
 * **Keyframes** store transformation data at specific points in time.
 * The system interpolates between keyframes for smooth animation.
 *
 * **Channels** contain keyframes for a single bone or node. Each channel
 * is identified by name and maps to a skeleton bone or scene node.
 *
 * **Animations** group multiple channels together to form complete clips
 * like "walk", "run", or "attack".
 *
 * @section animation_usage Usage Example
 *
 * @code{.cpp}
 * // Create a simple animation
 * Nova::Animation walkAnim("Walk");
 * walkAnim.SetDuration(1.0f);
 * walkAnim.SetLooping(true);
 *
 * // Add a channel for the "Spine" bone
 * Nova::AnimationChannel spineChannel;
 * spineChannel.nodeName = "Spine";
 * spineChannel.interpolationMode = Nova::InterpolationMode::Linear;
 *
 * // Add keyframes
 * spineChannel.keyframes.push_back({0.0f, {0,0,0}, glm::quat(1,0,0,0), {1,1,1}});
 * spineChannel.keyframes.push_back({0.5f, {0,0.1f,0}, glm::quat(1,0,0,0), {1,1,1}});
 * spineChannel.keyframes.push_back({1.0f, {0,0,0}, glm::quat(1,0,0,0), {1,1,1}});
 *
 * walkAnim.AddChannel(std::move(spineChannel));
 *
 * // Evaluate at a specific time
 * auto transforms = walkAnim.Evaluate(0.25f);
 * glm::mat4 spineTransform = transforms["Spine"];
 * @endcode
 *
 * @section animation_interpolation Interpolation Modes
 *
 * - **Linear**: Simple linear interpolation (default, fastest)
 * - **Step**: No interpolation, snaps to keyframe values
 * - **CatmullRom**: Smooth spline interpolation through control points
 * - **Cubic**: Bezier curve interpolation for custom easing
 *
 * @section animation_blending Animation Blending
 *
 * Use AnimationLayer for blending multiple animations:
 *
 * @code{.cpp}
 * Nova::AnimationLayer baseLayer{&walkAnim, 0.0f, 1.0f, Nova::BlendMode::Override};
 * Nova::AnimationLayer additiveLayer{&waveAnim, 0.0f, 0.5f, Nova::BlendMode::Additive};
 * additiveLayer.boneMask = {"RightArm", "RightHand"}; // Only affect arm
 * @endcode
 *
 * @see AnimationController for playback management
 * @see AnimationStateMachine for state-based transitions
 * @see docs/api/Animation.md for complete API documentation
 *
 * @author Nova3D Team
 * @version 1.0.0
 */

#pragma once

#include <vector>
#include <string>
#include <span>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

/**
 * @brief Interpolation mode for keyframe animation
 */
enum class InterpolationMode : uint8_t {
    Linear,
    Step,
    CatmullRom,
    Cubic
};

/**
 * @brief Animation keyframe with position, rotation, and scale
 */
struct alignas(16) Keyframe {
    float time = 0.0f;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    bool operator<(const Keyframe& other) const noexcept {
        return time < other.time;
    }

    bool operator<(float t) const noexcept {
        return time < t;
    }
};

/**
 * @brief Animation channel for a single bone/node
 */
struct AnimationChannel {
    std::string nodeName;
    std::vector<Keyframe> keyframes;
    InterpolationMode interpolationMode = InterpolationMode::Linear;

    // Cached index for sequential playback optimization
    mutable size_t lastKeyframeIndex = 0;

    [[nodiscard]] glm::mat4 Evaluate(float time) const;
    [[nodiscard]] Keyframe Interpolate(float time) const;

    /**
     * @brief Binary search for keyframe index
     * @return Index of the keyframe just before or at the given time
     */
    [[nodiscard]] size_t FindKeyframeIndex(float time) const noexcept;

    /**
     * @brief Clear cached lookup data
     */
    void ResetCache() const noexcept { lastKeyframeIndex = 0; }
};

/**
 * @brief Animation clip containing multiple channels
 */
class Animation {
public:
    Animation() = default;
    explicit Animation(const std::string& name);

    Animation(const Animation&) = default;
    Animation(Animation&&) noexcept = default;
    Animation& operator=(const Animation&) = default;
    Animation& operator=(Animation&&) noexcept = default;

    void AddChannel(const AnimationChannel& channel);
    void AddChannel(AnimationChannel&& channel);

    /**
     * @brief Evaluate all channels at a given time
     */
    [[nodiscard]] std::unordered_map<std::string, glm::mat4> Evaluate(float time) const;

    /**
     * @brief Evaluate into pre-allocated map (avoids allocations)
     */
    void EvaluateInto(float time, std::unordered_map<std::string, glm::mat4>& outTransforms) const;

    /**
     * @brief Get channel by node name
     */
    [[nodiscard]] const AnimationChannel* GetChannel(const std::string& nodeName) const;
    [[nodiscard]] AnimationChannel* GetChannel(const std::string& nodeName);

    // Properties
    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] float GetDuration() const noexcept { return m_duration; }
    void SetDuration(float duration) noexcept { m_duration = duration; }

    [[nodiscard]] float GetTicksPerSecond() const noexcept { return m_ticksPerSecond; }
    void SetTicksPerSecond(float tps) noexcept { m_ticksPerSecond = tps; }

    [[nodiscard]] bool IsLooping() const noexcept { return m_looping; }
    void SetLooping(bool looping) noexcept { m_looping = looping; }

    [[nodiscard]] const std::vector<AnimationChannel>& GetChannels() const noexcept { return m_channels; }
    [[nodiscard]] std::span<const AnimationChannel> GetChannelsSpan() const noexcept {
        return std::span<const AnimationChannel>(m_channels);
    }

    /**
     * @brief Reset all channel caches (call when seeking)
     */
    void ResetCaches() const;

private:
    std::string m_name;
    float m_duration = 0.0f;
    float m_ticksPerSecond = 25.0f;
    bool m_looping = true;
    std::vector<AnimationChannel> m_channels;
    std::unordered_map<std::string, size_t> m_channelLookup; // Name -> channel index
};

/**
 * @brief Blend mode for combining animations
 */
enum class BlendMode : uint8_t {
    Override,       // Replace previous animation
    Additive,       // Add to previous animation
    Multiply        // Multiply with previous animation
};

/**
 * @brief Layer for animation blending
 */
struct AnimationLayer {
    const Animation* animation = nullptr;
    float time = 0.0f;
    float weight = 1.0f;
    BlendMode blendMode = BlendMode::Override;

    // Bone mask - if empty, affects all bones
    std::vector<std::string> boneMask;
};

/**
 * @brief Blend two keyframes
 */
[[nodiscard]] Keyframe BlendKeyframes(const Keyframe& a, const Keyframe& b, float weight);

/**
 * @brief Blend two transforms
 */
[[nodiscard]] glm::mat4 BlendTransforms(const glm::mat4& a, const glm::mat4& b, float weight);

/**
 * @brief Interpolation methods with SIMD optimization hints
 */
namespace Interpolation {
    /**
     * @brief Linear interpolation for vec3
     */
    [[nodiscard]] glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) noexcept;

    /**
     * @brief Spherical linear interpolation for quaternions
     */
    [[nodiscard]] glm::quat Slerp(const glm::quat& a, const glm::quat& b, float t) noexcept;

    /**
     * @brief Normalized linear interpolation for quaternions (faster than slerp)
     */
    [[nodiscard]] glm::quat Nlerp(const glm::quat& a, const glm::quat& b, float t) noexcept;

    /**
     * @brief Smooth step interpolation factor
     */
    [[nodiscard]] constexpr float SmoothStep(float t) noexcept {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    }

    /**
     * @brief Smoother step (5th order) interpolation factor
     */
    [[nodiscard]] constexpr float SmootherStep(float t) noexcept {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    /**
     * @brief Ease in/out interpolation factor
     */
    [[nodiscard]] float EaseInOut(float t) noexcept;

    /**
     * @brief Hermite spline interpolation
     */
    [[nodiscard]] glm::vec3 CatmullRom(const glm::vec3& p0, const glm::vec3& p1,
                                        const glm::vec3& p2, const glm::vec3& p3, float t) noexcept;

    /**
     * @brief Cubic bezier interpolation
     */
    [[nodiscard]] glm::vec3 CubicBezier(const glm::vec3& p0, const glm::vec3& p1,
                                         const glm::vec3& p2, const glm::vec3& p3, float t) noexcept;

    /**
     * @brief Squad interpolation for quaternions (spherical cubic)
     */
    [[nodiscard]] glm::quat Squad(const glm::quat& q0, const glm::quat& q1,
                                   const glm::quat& s0, const glm::quat& s1, float t) noexcept;
}

/**
 * @brief Keyframe utility functions
 */
namespace KeyframeUtils {
    /**
     * @brief Create a keyframe from a transformation matrix
     */
    [[nodiscard]] Keyframe FromMatrix(const glm::mat4& matrix, float time = 0.0f);

    /**
     * @brief Convert keyframe to transformation matrix
     */
    [[nodiscard]] glm::mat4 ToMatrix(const Keyframe& kf);

    /**
     * @brief Create an identity keyframe at given time
     */
    [[nodiscard]] Keyframe Identity(float time = 0.0f);

    /**
     * @brief Check if two keyframes are approximately equal
     */
    [[nodiscard]] bool ApproximatelyEqual(const Keyframe& a, const Keyframe& b, float epsilon = 0.0001f);

    /**
     * @brief Calculate the "distance" between two keyframes
     */
    [[nodiscard]] float Distance(const Keyframe& a, const Keyframe& b);

    /**
     * @brief Reduce keyframes by removing redundant ones
     */
    [[nodiscard]] std::vector<Keyframe> Optimize(const std::vector<Keyframe>& keyframes, float tolerance = 0.001f);

    /**
     * @brief Resample keyframes at a new frame rate
     */
    [[nodiscard]] std::vector<Keyframe> Resample(const std::vector<Keyframe>& keyframes, float newFrameRate);

    /**
     * @brief Sort keyframes by time
     */
    void SortByTime(std::vector<Keyframe>& keyframes);

    /**
     * @brief Remove duplicate keyframes at the same time
     */
    void RemoveDuplicates(std::vector<Keyframe>& keyframes, float timeEpsilon = 0.0001f);

    /**
     * @brief Scale all keyframe times by a factor
     */
    void ScaleTime(std::vector<Keyframe>& keyframes, float factor);

    /**
     * @brief Offset all keyframe times by an amount
     */
    void OffsetTime(std::vector<Keyframe>& keyframes, float offset);

    /**
     * @brief Reverse the animation
     */
    void Reverse(std::vector<Keyframe>& keyframes);

    /**
     * @brief Create keyframes for a simple translation animation
     */
    [[nodiscard]] std::vector<Keyframe> CreateTranslationAnimation(
        const glm::vec3& start, const glm::vec3& end,
        float duration, size_t numKeyframes = 2);

    /**
     * @brief Create keyframes for a rotation animation
     */
    [[nodiscard]] std::vector<Keyframe> CreateRotationAnimation(
        const glm::quat& start, const glm::quat& end,
        float duration, size_t numKeyframes = 2);
}

} // namespace Nova
