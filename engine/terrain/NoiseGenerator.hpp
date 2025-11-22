#pragma once

#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Noise generation utilities
 */
class NoiseGenerator {
public:
    /**
     * @brief Perlin noise
     */
    static float Perlin(float x, float y);
    static float Perlin(float x, float y, float z);

    /**
     * @brief Simplex noise
     */
    static float Simplex(float x, float y);
    static float Simplex(float x, float y, float z);

    /**
     * @brief Fractal/FBM noise (multiple octaves)
     */
    static float FractalNoise(float x, float y, int octaves = 4,
                               float persistence = 0.5f, float lacunarity = 2.0f);

    static float FractalNoise(float x, float y, float z, int octaves = 4,
                               float persistence = 0.5f, float lacunarity = 2.0f);

    /**
     * @brief Ridged multifractal noise (good for mountains)
     */
    static float RidgedNoise(float x, float y, int octaves = 4,
                              float persistence = 0.5f, float lacunarity = 2.0f);

    /**
     * @brief Worley/Cellular noise
     */
    static float Worley(float x, float y);

    /**
     * @brief Set the seed for noise generation
     */
    static void SetSeed(int seed);

private:
    static int Hash(int x, int y);
    static float Grad(int hash, float x, float y);
    static float Fade(float t);
    static float Lerp(float a, float b, float t);

    static int s_permutation[512];
    static bool s_initialized;
    static void Initialize();
};

} // namespace Nova
