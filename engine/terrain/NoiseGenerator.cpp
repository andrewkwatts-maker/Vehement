#include "terrain/NoiseGenerator.hpp"
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>

namespace Nova {

int NoiseGenerator::s_permutation[512];
bool NoiseGenerator::s_initialized = false;

void NoiseGenerator::Initialize() {
    if (s_initialized) return;

    // Initialize with standard permutation
    std::vector<int> p(256);
    std::iota(p.begin(), p.end(), 0);

    std::mt19937 rng(12345);
    std::shuffle(p.begin(), p.end(), rng);

    for (int i = 0; i < 256; ++i) {
        s_permutation[i] = s_permutation[i + 256] = p[i];
    }

    s_initialized = true;
}

void NoiseGenerator::SetSeed(int seed) {
    std::vector<int> p(256);
    std::iota(p.begin(), p.end(), 0);

    std::mt19937 rng(seed);
    std::shuffle(p.begin(), p.end(), rng);

    for (int i = 0; i < 256; ++i) {
        s_permutation[i] = s_permutation[i + 256] = p[i];
    }

    s_initialized = true;
}

float NoiseGenerator::Fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float NoiseGenerator::Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float NoiseGenerator::Grad(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

float NoiseGenerator::Perlin(float x, float y) {
    Initialize();

    int xi = static_cast<int>(std::floor(x)) & 255;
    int yi = static_cast<int>(std::floor(y)) & 255;

    float xf = x - std::floor(x);
    float yf = y - std::floor(y);

    float u = Fade(xf);
    float v = Fade(yf);

    int aa = s_permutation[s_permutation[xi] + yi];
    int ab = s_permutation[s_permutation[xi] + yi + 1];
    int ba = s_permutation[s_permutation[xi + 1] + yi];
    int bb = s_permutation[s_permutation[xi + 1] + yi + 1];

    float x1 = Lerp(Grad(aa, xf, yf), Grad(ba, xf - 1, yf), u);
    float x2 = Lerp(Grad(ab, xf, yf - 1), Grad(bb, xf - 1, yf - 1), u);

    return (Lerp(x1, x2, v) + 1.0f) * 0.5f;
}

float NoiseGenerator::Perlin(float x, float y, float z) {
    // 3D Perlin noise (simplified)
    float xy = Perlin(x, y);
    float xz = Perlin(x, z);
    float yz = Perlin(y, z);

    return (xy + xz + yz) / 3.0f;
}

float NoiseGenerator::Simplex(float x, float y) {
    // Simplified simplex noise using Perlin as base
    return Perlin(x, y);
}

float NoiseGenerator::Simplex(float x, float y, float z) {
    return Perlin(x, y, z);
}

float NoiseGenerator::FractalNoise(float x, float y, int octaves,
                                    float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        total += Perlin(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

float NoiseGenerator::FractalNoise(float x, float y, float z, int octaves,
                                    float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        total += Perlin(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

float NoiseGenerator::RidgedNoise(float x, float y, int octaves,
                                   float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        float n = Perlin(x * frequency, y * frequency);
        n = 1.0f - std::abs(n * 2.0f - 1.0f);  // Ridge
        n = n * n;  // Sharpen

        total += n * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

float NoiseGenerator::Worley(float x, float y) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));

    float minDist = 1.0f;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = xi + dx;
            int cy = yi + dy;

            // Pseudo-random point in cell
            float px = cx + (Hash(cx, cy) & 255) / 255.0f;
            float py = cy + (Hash(cy, cx) & 255) / 255.0f;

            float dist = std::sqrt((x - px) * (x - px) + (y - py) * (y - py));
            minDist = std::min(minDist, dist);
        }
    }

    return minDist;
}

int NoiseGenerator::Hash(int x, int y) {
    Initialize();
    return s_permutation[(s_permutation[x & 255] + y) & 255];
}

} // namespace Nova
