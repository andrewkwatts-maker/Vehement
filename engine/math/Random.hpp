#pragma once

#include <random>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Random number generation utilities
 */
class Random {
public:
    /**
     * @brief Seed the random number generator
     */
    static void Seed(unsigned int seed);

    /**
     * @brief Get a random float in range [0, 1]
     */
    static float Value();

    /**
     * @brief Get a random float in range [min, max]
     */
    static float Range(float min, float max);

    /**
     * @brief Get a random integer in range [min, max]
     */
    static int Range(int min, int max);

    /**
     * @brief Get a random point in a unit sphere
     */
    static glm::vec3 InUnitSphere();

    /**
     * @brief Get a random point on a unit sphere surface
     */
    static glm::vec3 OnUnitSphere();

    /**
     * @brief Get a random point in a unit circle (XY plane)
     */
    static glm::vec2 InUnitCircle();

    /**
     * @brief Get a random direction
     */
    static glm::vec3 Direction();

    /**
     * @brief Get a random color
     */
    static glm::vec3 Color();

    /**
     * @brief Get a random rotation quaternion
     */
    static glm::quat Rotation();

private:
    static std::mt19937& GetEngine();
};

} // namespace Nova
