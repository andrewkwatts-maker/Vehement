#pragma once

#include <random>
#include <mutex>
#include <concepts>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

/**
 * @brief Concept for numeric types usable in random range functions
 */
template<typename T>
concept RandomNumeric = std::integral<T> || std::floating_point<T>;

/**
 * @brief Thread-safe random number generation utilities
 *
 * All methods are thread-safe and use a high-quality PRNG (Mersenne Twister).
 * For deterministic sequences, use Seed() at the start of your program.
 */
class Random {
public:
    /**
     * @brief Seed the random number generator
     * @param seed The seed value (use same seed for reproducible sequences)
     */
    static void Seed(unsigned int seed);

    /**
     * @brief Get a random float in range [0, 1]
     */
    [[nodiscard]] static float Value();

    /**
     * @brief Get a random float in range [min, max]
     */
    [[nodiscard]] static float Range(float min, float max);

    /**
     * @brief Get a random integer in range [min, max] (inclusive)
     */
    [[nodiscard]] static int Range(int min, int max);

    /**
     * @brief Get a random value in range [min, max] for any numeric type
     */
    template<RandomNumeric T>
    [[nodiscard]] static T Range(T min, T max);

    /**
     * @brief Get a random boolean with given probability of true
     * @param probability Chance of returning true [0.0, 1.0]
     */
    [[nodiscard]] static bool Bool(float probability = 0.5f);

    /**
     * @brief Get a random point inside a unit sphere
     */
    [[nodiscard]] static glm::vec3 InUnitSphere();

    /**
     * @brief Get a random point on a unit sphere surface
     */
    [[nodiscard]] static glm::vec3 OnUnitSphere();

    /**
     * @brief Get a random point inside a unit circle (XY plane)
     */
    [[nodiscard]] static glm::vec2 InUnitCircle();

    /**
     * @brief Get a random point on a unit circle circumference
     */
    [[nodiscard]] static glm::vec2 OnUnitCircle();

    /**
     * @brief Get a random unit direction vector
     */
    [[nodiscard]] static glm::vec3 Direction();

    /**
     * @brief Get a random 2D unit direction vector
     */
    [[nodiscard]] static glm::vec2 Direction2D();

    /**
     * @brief Get a random RGB color
     */
    [[nodiscard]] static glm::vec3 Color();

    /**
     * @brief Get a random RGBA color with specified alpha
     */
    [[nodiscard]] static glm::vec4 Color(float alpha);

    /**
     * @brief Get a random rotation quaternion (uniformly distributed)
     */
    [[nodiscard]] static glm::quat Rotation();

    /**
     * @brief Get a random angle in radians [0, 2*PI]
     */
    [[nodiscard]] static float Angle();

    /**
     * @brief Get a random sign (-1 or 1)
     */
    [[nodiscard]] static int Sign();

    /**
     * @brief Pick a random element from a container
     */
    template<typename Container>
    [[nodiscard]] static auto& Pick(Container& container);

    template<typename Container>
    [[nodiscard]] static const auto& Pick(const Container& container);

private:
    static std::mt19937& GetEngine();
    static std::mutex& GetMutex();
};

// Template implementations
template<RandomNumeric T>
T Random::Range(T min, T max) {
    std::lock_guard lock(GetMutex());
    if constexpr (std::floating_point<T>) {
        std::uniform_real_distribution<T> dist(min, max);
        return dist(GetEngine());
    } else {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(GetEngine());
    }
}

template<typename Container>
auto& Random::Pick(Container& container) {
    auto size = std::size(container);
    auto index = Range(static_cast<size_t>(0), size - 1);
    auto it = std::begin(container);
    std::advance(it, index);
    return *it;
}

template<typename Container>
const auto& Random::Pick(const Container& container) {
    auto size = std::size(container);
    auto index = Range(static_cast<size_t>(0), size - 1);
    auto it = std::begin(container);
    std::advance(it, index);
    return *it;
}

} // namespace Nova
