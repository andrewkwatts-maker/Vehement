/**
 * @file Generators.hpp
 * @brief Random data generators for property-based testing
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <random>
#include <vector>
#include <string>
#include <functional>
#include <limits>

namespace Nova {
namespace Test {

// =============================================================================
// Base Generator
// =============================================================================

/**
 * @brief Seeded random number generator for reproducible tests
 */
class RandomGenerator {
public:
    explicit RandomGenerator(uint64_t seed = std::random_device{}())
        : m_engine(seed), m_seed(seed) {}

    void Reset() { m_engine.seed(m_seed); }
    void SetSeed(uint64_t seed) { m_seed = seed; m_engine.seed(seed); }
    uint64_t GetSeed() const { return m_seed; }

    std::mt19937_64& Engine() { return m_engine; }

private:
    std::mt19937_64 m_engine;
    uint64_t m_seed;
};

// =============================================================================
// Primitive Generators
// =============================================================================

/**
 * @brief Generate random integers
 */
class IntGenerator {
public:
    explicit IntGenerator(int min = std::numeric_limits<int>::min(),
                         int max = std::numeric_limits<int>::max())
        : m_dist(min, max) {}

    int Generate(RandomGenerator& rng) {
        return m_dist(rng.Engine());
    }

    std::vector<int> GenerateMany(RandomGenerator& rng, size_t count) {
        std::vector<int> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.push_back(Generate(rng));
        }
        return result;
    }

private:
    std::uniform_int_distribution<int> m_dist;
};

/**
 * @brief Generate random floats
 */
class FloatGenerator {
public:
    explicit FloatGenerator(float min = 0.0f, float max = 1.0f)
        : m_dist(min, max) {}

    float Generate(RandomGenerator& rng) {
        return m_dist(rng.Engine());
    }

    std::vector<float> GenerateMany(RandomGenerator& rng, size_t count) {
        std::vector<float> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.push_back(Generate(rng));
        }
        return result;
    }

private:
    std::uniform_real_distribution<float> m_dist;
};

/**
 * @brief Generate random booleans
 */
class BoolGenerator {
public:
    explicit BoolGenerator(float trueProbability = 0.5f)
        : m_dist(0.0f, 1.0f), m_threshold(trueProbability) {}

    bool Generate(RandomGenerator& rng) {
        return m_dist(rng.Engine()) < m_threshold;
    }

private:
    std::uniform_real_distribution<float> m_dist;
    float m_threshold;
};

/**
 * @brief Generate random strings
 */
class StringGenerator {
public:
    explicit StringGenerator(size_t minLength = 1, size_t maxLength = 100,
                            const std::string& charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
        : m_lengthDist(minLength, maxLength)
        , m_charDist(0, charset.size() - 1)
        , m_charset(charset) {}

    std::string Generate(RandomGenerator& rng) {
        size_t length = m_lengthDist(rng.Engine());
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += m_charset[m_charDist(rng.Engine())];
        }
        return result;
    }

private:
    std::uniform_int_distribution<size_t> m_lengthDist;
    std::uniform_int_distribution<size_t> m_charDist;
    std::string m_charset;
};

// =============================================================================
// Vector Generators
// =============================================================================

/**
 * @brief Generate random vec2
 */
class Vec2Generator {
public:
    explicit Vec2Generator(float min = -100.0f, float max = 100.0f)
        : m_dist(min, max) {}

    glm::vec2 Generate(RandomGenerator& rng) {
        return glm::vec2(
            m_dist(rng.Engine()),
            m_dist(rng.Engine())
        );
    }

    glm::vec2 GenerateNormalized(RandomGenerator& rng) {
        return glm::normalize(Generate(rng));
    }

private:
    std::uniform_real_distribution<float> m_dist;
};

/**
 * @brief Generate random vec3
 */
class Vec3Generator {
public:
    explicit Vec3Generator(float min = -100.0f, float max = 100.0f)
        : m_dist(min, max) {}

    glm::vec3 Generate(RandomGenerator& rng) {
        return glm::vec3(
            m_dist(rng.Engine()),
            m_dist(rng.Engine()),
            m_dist(rng.Engine())
        );
    }

    glm::vec3 GenerateNormalized(RandomGenerator& rng) {
        glm::vec3 v = Generate(rng);
        float len = glm::length(v);
        return len > 0.0001f ? v / len : glm::vec3(1, 0, 0);
    }

    glm::vec3 GeneratePositive(RandomGenerator& rng) {
        std::uniform_real_distribution<float> posDist(0.0f, std::abs(m_dist.max()));
        return glm::vec3(
            posDist(rng.Engine()),
            posDist(rng.Engine()),
            posDist(rng.Engine())
        );
    }

private:
    std::uniform_real_distribution<float> m_dist;
};

/**
 * @brief Generate random quaternions
 */
class QuatGenerator {
public:
    glm::quat Generate(RandomGenerator& rng) {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        glm::quat q(
            dist(rng.Engine()),
            dist(rng.Engine()),
            dist(rng.Engine()),
            dist(rng.Engine())
        );
        return glm::normalize(q);
    }

    // Generate quaternion from euler angles
    glm::quat GenerateFromEuler(RandomGenerator& rng) {
        std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
        return glm::quat(glm::vec3(
            angleDist(rng.Engine()),
            angleDist(rng.Engine()),
            angleDist(rng.Engine())
        ));
    }
};

// =============================================================================
// Spatial Data Generators
// =============================================================================

/**
 * @brief Generate random AABBs
 */
class AABBGenerator {
public:
    explicit AABBGenerator(float minExtent = 0.1f, float maxExtent = 10.0f,
                          float worldMin = -100.0f, float worldMax = 100.0f)
        : m_extentDist(minExtent, maxExtent)
        , m_posDist(worldMin, worldMax) {}

    struct AABBData {
        glm::vec3 min;
        glm::vec3 max;
    };

    AABBData Generate(RandomGenerator& rng) {
        glm::vec3 center(
            m_posDist(rng.Engine()),
            m_posDist(rng.Engine()),
            m_posDist(rng.Engine())
        );
        glm::vec3 extent(
            m_extentDist(rng.Engine()),
            m_extentDist(rng.Engine()),
            m_extentDist(rng.Engine())
        );
        return {center - extent, center + extent};
    }

    std::vector<AABBData> GenerateMany(RandomGenerator& rng, size_t count) {
        std::vector<AABBData> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.push_back(Generate(rng));
        }
        return result;
    }

    // Generate non-overlapping AABBs
    std::vector<AABBData> GenerateNonOverlapping(RandomGenerator& rng, size_t count, float spacing = 1.0f);

private:
    std::uniform_real_distribution<float> m_extentDist;
    std::uniform_real_distribution<float> m_posDist;
};

/**
 * @brief Generate random rays
 */
class RayGenerator {
public:
    explicit RayGenerator(float worldMin = -100.0f, float worldMax = 100.0f)
        : m_posDist(worldMin, worldMax)
        , m_dirDist(-1.0f, 1.0f) {}

    struct RayData {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    RayData Generate(RandomGenerator& rng) {
        glm::vec3 origin(
            m_posDist(rng.Engine()),
            m_posDist(rng.Engine()),
            m_posDist(rng.Engine())
        );
        glm::vec3 direction(
            m_dirDist(rng.Engine()),
            m_dirDist(rng.Engine()),
            m_dirDist(rng.Engine())
        );
        return {origin, glm::normalize(direction)};
    }

    // Generate ray that will hit a target AABB
    RayData GenerateHitting(RandomGenerator& rng, const glm::vec3& targetMin, const glm::vec3& targetMax);

private:
    std::uniform_real_distribution<float> m_posDist;
    std::uniform_real_distribution<float> m_dirDist;
};

// =============================================================================
// Animation Data Generators
// =============================================================================

/**
 * @brief Generate random animation keyframes
 */
class KeyframeGenerator {
public:
    struct KeyframeData {
        float time;
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    };

    // Generate keyframes for a smooth animation
    std::vector<KeyframeData> GenerateSmooth(
        RandomGenerator& rng,
        size_t count,
        float duration,
        float maxPositionDelta = 10.0f,
        float maxAngleDelta = 0.5f)
    {
        std::vector<KeyframeData> keyframes;
        keyframes.reserve(count);

        Vec3Generator posGen(-maxPositionDelta, maxPositionDelta);
        std::uniform_real_distribution<float> scaleDist(0.5f, 2.0f);

        glm::vec3 currentPos(0.0f);
        glm::quat currentRot(1.0f, 0.0f, 0.0f, 0.0f);

        for (size_t i = 0; i < count; ++i) {
            float t = (count > 1) ? (duration * i / (count - 1)) : 0.0f;

            // Smooth position changes
            glm::vec3 delta = posGen.Generate(rng) * 0.1f;
            currentPos += delta;

            // Smooth rotation changes
            std::uniform_real_distribution<float> angleDist(-maxAngleDelta, maxAngleDelta);
            glm::quat rotDelta = glm::quat(glm::vec3(
                angleDist(rng.Engine()),
                angleDist(rng.Engine()),
                angleDist(rng.Engine())
            ));
            currentRot = glm::normalize(currentRot * rotDelta);

            keyframes.push_back({
                t,
                currentPos,
                currentRot,
                glm::vec3(scaleDist(rng.Engine()))
            });
        }

        return keyframes;
    }
};

// =============================================================================
// Entity/Game Data Generators
// =============================================================================

/**
 * @brief Generate random entity IDs
 */
class EntityIdGenerator {
public:
    explicit EntityIdGenerator(uint64_t maxId = 1000000)
        : m_dist(1, maxId) {}

    uint64_t Generate(RandomGenerator& rng) {
        return m_dist(rng.Engine());
    }

    std::vector<uint64_t> GenerateUnique(RandomGenerator& rng, size_t count) {
        std::set<uint64_t> seen;
        std::vector<uint64_t> result;
        result.reserve(count);

        while (result.size() < count) {
            uint64_t id = Generate(rng);
            if (seen.insert(id).second) {
                result.push_back(id);
            }
        }
        return result;
    }

private:
    std::uniform_int_distribution<uint64_t> m_dist;
};

/**
 * @brief Generate random damage events
 */
class DamageEventGenerator {
public:
    struct DamageEventData {
        uint32_t targetId;
        uint32_t sourceId;
        float damage;
        glm::vec3 hitPosition;
        bool isHeadshot;
        bool isExplosion;
    };

    DamageEventData Generate(RandomGenerator& rng) {
        std::uniform_int_distribution<uint32_t> idDist(1, 1000);
        std::uniform_real_distribution<float> damageDist(1.0f, 100.0f);
        Vec3Generator posGen(-50.0f, 50.0f);
        BoolGenerator boolGen(0.2f);

        return {
            idDist(rng.Engine()),
            idDist(rng.Engine()),
            damageDist(rng.Engine()),
            posGen.Generate(rng),
            boolGen.Generate(rng),
            boolGen.Generate(rng)
        };
    }
};

// =============================================================================
// Property-Based Testing Helper
// =============================================================================

/**
 * @brief Run a property test with generated data
 */
template<typename Generator, typename Property>
void PropertyTest(Generator& gen, Property&& prop, size_t iterations = 100, uint64_t seed = 42) {
    RandomGenerator rng(seed);

    for (size_t i = 0; i < iterations; ++i) {
        auto data = gen.Generate(rng);
        if (!prop(data)) {
            // Re-run with same seed for debugging
            FAIL() << "Property failed at iteration " << i << " with seed " << seed;
        }
    }
}

} // namespace Test
} // namespace Nova
