#pragma once

#include <glm/glm.hpp>
#include <array>
#include <atomic>

namespace Nova {

/**
 * @brief High-performance noise generation utilities
 *
 * Provides various noise functions optimized for terrain generation.
 * All functions are thread-safe after initialization.
 */
class NoiseGenerator {
public:
    // ========================================================================
    // Basic Noise Functions
    // ========================================================================

    /**
     * @brief 2D Perlin noise
     * @param x X coordinate
     * @param y Y coordinate
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float Perlin(float x, float y) noexcept;

    /**
     * @brief 3D Perlin noise
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float Perlin(float x, float y, float z) noexcept;

    /**
     * @brief 2D Simplex noise (faster than Perlin)
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float Simplex(float x, float y) noexcept;

    /**
     * @brief 3D Simplex noise
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float Simplex(float x, float y, float z) noexcept;

    // ========================================================================
    // Fractal/Multi-octave Noise Functions
    // ========================================================================

    /**
     * @brief 2D Fractal Brownian Motion noise
     * @param x X coordinate
     * @param y Y coordinate
     * @param octaves Number of noise layers (default: 4)
     * @param persistence Amplitude multiplier per octave (default: 0.5)
     * @param lacunarity Frequency multiplier per octave (default: 2.0)
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float FractalNoise(float x, float y,
                                             int octaves = 4,
                                             float persistence = 0.5f,
                                             float lacunarity = 2.0f) noexcept;

    /**
     * @brief 3D Fractal Brownian Motion noise
     */
    [[nodiscard]] static float FractalNoise(float x, float y, float z,
                                             int octaves = 4,
                                             float persistence = 0.5f,
                                             float lacunarity = 2.0f) noexcept;

    /**
     * @brief Ridged multifractal noise (excellent for mountain ranges)
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float RidgedNoise(float x, float y,
                                            int octaves = 4,
                                            float persistence = 0.5f,
                                            float lacunarity = 2.0f) noexcept;

    /**
     * @brief Billowy noise (inverted ridged, good for clouds/hills)
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float BillowNoise(float x, float y,
                                            int octaves = 4,
                                            float persistence = 0.5f,
                                            float lacunarity = 2.0f) noexcept;

    // ========================================================================
    // Cellular/Voronoi Noise
    // ========================================================================

    /**
     * @brief Worley/Cellular noise (F1 - distance to nearest point)
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float Worley(float x, float y) noexcept;

    /**
     * @brief Worley noise with F2-F1 (cell edges)
     * @return Noise value in range [0, 1]
     */
    [[nodiscard]] static float WorleyF2F1(float x, float y) noexcept;

    // ========================================================================
    // Utility Functions
    // ========================================================================

    /**
     * @brief Set the seed for noise generation
     * @param seed Random seed value
     * @note Thread-safe, but will cause brief inconsistencies during regeneration
     */
    static void SetSeed(int seed);

    /**
     * @brief Get current seed
     */
    [[nodiscard]] static int GetSeed() noexcept { return s_seed.load(); }

    /**
     * @brief Force initialization (normally done lazily)
     */
    static void Initialize();

    /**
     * @brief Check if generator is initialized
     */
    [[nodiscard]] static bool IsInitialized() noexcept { return s_initialized.load(); }

private:
    // Internal helper functions
    [[nodiscard]] static int Hash(int x, int y) noexcept;
    [[nodiscard]] static int Hash(int x, int y, int z) noexcept;
    [[nodiscard]] static float Grad(int hash, float x, float y) noexcept;
    [[nodiscard]] static float Grad(int hash, float x, float y, float z) noexcept;

    // Interpolation functions
    [[nodiscard]] static constexpr float Fade(float t) noexcept {
        // Improved smoothstep: 6t^5 - 15t^4 + 10t^3
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    [[nodiscard]] static constexpr float Lerp(float a, float b, float t) noexcept {
        return a + t * (b - a);
    }

    // Permutation table (doubled for wrapping)
    static std::array<int, 512> s_permutation;
    static std::atomic<bool> s_initialized;
    static std::atomic<int> s_seed;

    // Gradient vectors for 3D noise
    static constexpr int GRAD3[][3] = {
        {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
        {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
        {0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1}
    };
};

// ============================================================================
// Inline implementations for performance-critical simple functions
// ============================================================================

inline float NoiseGenerator::Grad(int hash, float x, float y) noexcept {
    const int h = hash & 7;
    const float u = h < 4 ? x : y;
    const float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

inline float NoiseGenerator::Grad(int hash, float x, float y, float z) noexcept {
    const int h = hash & 11;
    return static_cast<float>(GRAD3[h][0]) * x +
           static_cast<float>(GRAD3[h][1]) * y +
           static_cast<float>(GRAD3[h][2]) * z;
}

inline int NoiseGenerator::Hash(int x, int y) noexcept {
    return s_permutation[(s_permutation[x & 255] + y) & 255];
}

inline int NoiseGenerator::Hash(int x, int y, int z) noexcept {
    return s_permutation[(s_permutation[(s_permutation[x & 255] + y) & 255] + z) & 255];
}

} // namespace Nova
