#include "animation/Animation.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <cmath>

namespace Nova {
namespace KeyframeUtils {

/**
 * @brief Create a keyframe from a transformation matrix
 */
Keyframe FromMatrix(const glm::mat4& matrix, float time) {
    Keyframe kf;
    kf.time = time;

    glm::vec3 skew;
    glm::vec4 perspective;

    glm::decompose(matrix, kf.scale, kf.rotation, kf.position, skew, perspective);

    return kf;
}

/**
 * @brief Convert keyframe to transformation matrix
 */
glm::mat4 ToMatrix(const Keyframe& kf) {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), kf.position);
    glm::mat4 rotation = glm::mat4_cast(kf.rotation);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), kf.scale);

    return translation * rotation * scale;
}

/**
 * @brief Create an identity keyframe at given time
 */
Keyframe Identity(float time) {
    Keyframe kf;
    kf.time = time;
    kf.position = glm::vec3(0.0f);
    kf.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    kf.scale = glm::vec3(1.0f);
    return kf;
}

/**
 * @brief Check if two keyframes are approximately equal
 */
bool ApproximatelyEqual(const Keyframe& a, const Keyframe& b, float epsilon) {
    auto vec3Equal = [epsilon](const glm::vec3& v1, const glm::vec3& v2) {
        return std::abs(v1.x - v2.x) < epsilon &&
               std::abs(v1.y - v2.y) < epsilon &&
               std::abs(v1.z - v2.z) < epsilon;
    };

    auto quatEqual = [epsilon](const glm::quat& q1, const glm::quat& q2) {
        // Quaternions q and -q represent the same rotation
        float dot = std::abs(glm::dot(q1, q2));
        return dot > (1.0f - epsilon);
    };

    return vec3Equal(a.position, b.position) &&
           quatEqual(a.rotation, b.rotation) &&
           vec3Equal(a.scale, b.scale);
}

/**
 * @brief Calculate the "distance" between two keyframes (for optimization)
 */
float Distance(const Keyframe& a, const Keyframe& b) {
    float posDist = glm::length(a.position - b.position);
    float rotDist = 1.0f - std::abs(glm::dot(a.rotation, b.rotation));
    float scaleDist = glm::length(a.scale - b.scale);

    // Weight rotation more heavily as small changes are very visible
    return posDist + rotDist * 10.0f + scaleDist;
}

/**
 * @brief Reduce keyframes by removing redundant ones
 * @param keyframes Input keyframes (must be sorted by time)
 * @param tolerance Distance threshold for removal
 * @return Optimized keyframes
 */
std::vector<Keyframe> Optimize(const std::vector<Keyframe>& keyframes, float tolerance) {
    if (keyframes.size() <= 2) {
        return keyframes;
    }

    std::vector<Keyframe> result;
    result.reserve(keyframes.size());
    result.push_back(keyframes.front());

    for (size_t i = 1; i < keyframes.size() - 1; ++i) {
        const Keyframe& prev = result.back();
        const Keyframe& curr = keyframes[i];
        const Keyframe& next = keyframes[i + 1];

        // Interpolate between prev and next at curr's time
        float t = (curr.time - prev.time) / (next.time - prev.time);
        Keyframe interpolated;
        interpolated.time = curr.time;
        interpolated.position = Interpolation::Lerp(prev.position, next.position, t);
        interpolated.rotation = Interpolation::Slerp(prev.rotation, next.rotation, t);
        interpolated.scale = Interpolation::Lerp(prev.scale, next.scale, t);

        // If interpolated differs significantly from actual, keep the keyframe
        if (Distance(curr, interpolated) > tolerance) {
            result.push_back(curr);
        }
    }

    result.push_back(keyframes.back());
    return result;
}

/**
 * @brief Resample keyframes at a new frame rate
 * @param keyframes Input keyframes
 * @param newFrameRate Target frame rate in frames per second
 * @return Resampled keyframes
 */
std::vector<Keyframe> Resample(const std::vector<Keyframe>& keyframes, float newFrameRate) {
    if (keyframes.empty()) {
        return {};
    }

    if (keyframes.size() == 1) {
        return keyframes;
    }

    float startTime = keyframes.front().time;
    float endTime = keyframes.back().time;
    float frameTime = 1.0f / newFrameRate;

    std::vector<Keyframe> result;
    size_t estimatedFrames = static_cast<size_t>((endTime - startTime) * newFrameRate) + 2;
    result.reserve(estimatedFrames);

    // Create a temporary channel for interpolation
    AnimationChannel tempChannel;
    tempChannel.keyframes = keyframes;

    for (float time = startTime; time <= endTime; time += frameTime) {
        result.push_back(tempChannel.Interpolate(time));
    }

    // Ensure we have the last keyframe
    if (result.empty() || result.back().time < endTime - 0.0001f) {
        result.push_back(keyframes.back());
    }

    return result;
}

/**
 * @brief Sort keyframes by time
 */
void SortByTime(std::vector<Keyframe>& keyframes) {
    std::sort(keyframes.begin(), keyframes.end(),
        [](const Keyframe& a, const Keyframe& b) {
            return a.time < b.time;
        });
}

/**
 * @brief Remove duplicate keyframes at the same time
 */
void RemoveDuplicates(std::vector<Keyframe>& keyframes, float timeEpsilon) {
    if (keyframes.size() <= 1) {
        return;
    }

    SortByTime(keyframes);

    auto newEnd = std::unique(keyframes.begin(), keyframes.end(),
        [timeEpsilon](const Keyframe& a, const Keyframe& b) {
            return std::abs(a.time - b.time) < timeEpsilon;
        });

    keyframes.erase(newEnd, keyframes.end());
}

/**
 * @brief Scale all keyframe times by a factor
 */
void ScaleTime(std::vector<Keyframe>& keyframes, float factor) {
    for (auto& kf : keyframes) {
        kf.time *= factor;
    }
}

/**
 * @brief Offset all keyframe times by an amount
 */
void OffsetTime(std::vector<Keyframe>& keyframes, float offset) {
    for (auto& kf : keyframes) {
        kf.time += offset;
    }
}

/**
 * @brief Reverse the animation (flip time)
 */
void Reverse(std::vector<Keyframe>& keyframes) {
    if (keyframes.empty()) {
        return;
    }

    float maxTime = keyframes.back().time;

    for (auto& kf : keyframes) {
        kf.time = maxTime - kf.time;
    }

    std::reverse(keyframes.begin(), keyframes.end());
}

/**
 * @brief Create keyframes for a simple translation animation
 */
std::vector<Keyframe> CreateTranslationAnimation(
    const glm::vec3& start, const glm::vec3& end,
    float duration, size_t numKeyframes) {

    std::vector<Keyframe> result;
    result.reserve(numKeyframes);

    for (size_t i = 0; i < numKeyframes; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(numKeyframes - 1);
        Keyframe kf;
        kf.time = t * duration;
        kf.position = Interpolation::Lerp(start, end, t);
        kf.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        kf.scale = glm::vec3(1.0f);
        result.push_back(kf);
    }

    return result;
}

/**
 * @brief Create keyframes for a rotation animation
 */
std::vector<Keyframe> CreateRotationAnimation(
    const glm::quat& start, const glm::quat& end,
    float duration, size_t numKeyframes) {

    std::vector<Keyframe> result;
    result.reserve(numKeyframes);

    for (size_t i = 0; i < numKeyframes; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(numKeyframes - 1);
        Keyframe kf;
        kf.time = t * duration;
        kf.position = glm::vec3(0.0f);
        kf.rotation = Interpolation::Slerp(start, end, t);
        kf.scale = glm::vec3(1.0f);
        result.push_back(kf);
    }

    return result;
}

} // namespace KeyframeUtils
} // namespace Nova
