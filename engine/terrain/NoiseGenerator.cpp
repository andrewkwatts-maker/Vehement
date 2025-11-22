#include "terrain/NoiseGenerator.hpp"
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <mutex>

namespace Nova {

// Static member definitions
std::array<int, 512> NoiseGenerator::s_permutation{};
std::atomic<bool> NoiseGenerator::s_initialized{false};
std::atomic<int> NoiseGenerator::s_seed{12345};

// Mutex for thread-safe initialization
static std::mutex s_initMutex;

void NoiseGenerator::Initialize() {
    // Double-checked locking pattern
    if (s_initialized.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard<std::mutex> lock(s_initMutex);

    // Check again after acquiring lock
    if (s_initialized.load(std::memory_order_relaxed)) {
        return;
    }

    // Initialize with permutation table
    std::array<int, 256> p;
    std::iota(p.begin(), p.end(), 0);

    std::mt19937 rng(static_cast<unsigned int>(s_seed.load()));
    std::shuffle(p.begin(), p.end(), rng);

    // Double the permutation table for seamless wrapping
    for (int i = 0; i < 256; ++i) {
        s_permutation[i] = p[i];
        s_permutation[i + 256] = p[i];
    }

    s_initialized.store(true, std::memory_order_release);
}

void NoiseGenerator::SetSeed(int seed) {
    std::lock_guard<std::mutex> lock(s_initMutex);

    s_seed.store(seed);

    std::array<int, 256> p;
    std::iota(p.begin(), p.end(), 0);

    std::mt19937 rng(static_cast<unsigned int>(seed));
    std::shuffle(p.begin(), p.end(), rng);

    for (int i = 0; i < 256; ++i) {
        s_permutation[i] = p[i];
        s_permutation[i + 256] = p[i];
    }

    s_initialized.store(true, std::memory_order_release);
}

// ============================================================================
// 2D Perlin Noise
// ============================================================================

float NoiseGenerator::Perlin(float x, float y) noexcept {
    if (!s_initialized.load(std::memory_order_acquire)) {
        const_cast<void(*)()>(Initialize)();  // Force initialization
    }

    // Find unit grid cell
    const int xi = static_cast<int>(std::floor(x)) & 255;
    const int yi = static_cast<int>(std::floor(y)) & 255;

    // Relative position within cell
    const float xf = x - std::floor(x);
    const float yf = y - std::floor(y);

    // Compute fade curves
    const float u = Fade(xf);
    const float v = Fade(yf);

    // Hash coordinates of cube corners
    const int aa = s_permutation[s_permutation[xi] + yi];
    const int ab = s_permutation[s_permutation[xi] + yi + 1];
    const int ba = s_permutation[s_permutation[xi + 1] + yi];
    const int bb = s_permutation[s_permutation[xi + 1] + yi + 1];

    // Gradient dot products and interpolation
    const float x1 = Lerp(Grad(aa, xf, yf), Grad(ba, xf - 1.0f, yf), u);
    const float x2 = Lerp(Grad(ab, xf, yf - 1.0f), Grad(bb, xf - 1.0f, yf - 1.0f), u);

    // Normalize to [0, 1]
    return (Lerp(x1, x2, v) + 1.0f) * 0.5f;
}

// ============================================================================
// 3D Perlin Noise (proper implementation)
// ============================================================================

float NoiseGenerator::Perlin(float x, float y, float z) noexcept {
    if (!s_initialized.load(std::memory_order_acquire)) {
        const_cast<void(*)()>(Initialize)();
    }

    // Find unit cube
    const int xi = static_cast<int>(std::floor(x)) & 255;
    const int yi = static_cast<int>(std::floor(y)) & 255;
    const int zi = static_cast<int>(std::floor(z)) & 255;

    // Relative position within cube
    const float xf = x - std::floor(x);
    const float yf = y - std::floor(y);
    const float zf = z - std::floor(z);

    // Compute fade curves
    const float u = Fade(xf);
    const float v = Fade(yf);
    const float w = Fade(zf);

    // Hash coordinates of cube corners
    const int A  = s_permutation[xi] + yi;
    const int AA = s_permutation[A] + zi;
    const int AB = s_permutation[A + 1] + zi;
    const int B  = s_permutation[xi + 1] + yi;
    const int BA = s_permutation[B] + zi;
    const int BB = s_permutation[B + 1] + zi;

    // Gradient dot products
    const float g000 = Grad(s_permutation[AA], xf, yf, zf);
    const float g100 = Grad(s_permutation[BA], xf - 1.0f, yf, zf);
    const float g010 = Grad(s_permutation[AB], xf, yf - 1.0f, zf);
    const float g110 = Grad(s_permutation[BB], xf - 1.0f, yf - 1.0f, zf);
    const float g001 = Grad(s_permutation[AA + 1], xf, yf, zf - 1.0f);
    const float g101 = Grad(s_permutation[BA + 1], xf - 1.0f, yf, zf - 1.0f);
    const float g011 = Grad(s_permutation[AB + 1], xf, yf - 1.0f, zf - 1.0f);
    const float g111 = Grad(s_permutation[BB + 1], xf - 1.0f, yf - 1.0f, zf - 1.0f);

    // Trilinear interpolation
    const float x00 = Lerp(g000, g100, u);
    const float x10 = Lerp(g010, g110, u);
    const float x01 = Lerp(g001, g101, u);
    const float x11 = Lerp(g011, g111, u);

    const float y0 = Lerp(x00, x10, v);
    const float y1 = Lerp(x01, x11, v);

    // Normalize to [0, 1]
    return (Lerp(y0, y1, w) + 1.0f) * 0.5f;
}

// ============================================================================
// Simplex Noise (2D) - faster than Perlin
// ============================================================================

float NoiseGenerator::Simplex(float x, float y) noexcept {
    if (!s_initialized.load(std::memory_order_acquire)) {
        const_cast<void(*)()>(Initialize)();
    }

    // Skewing factors for 2D
    constexpr float F2 = 0.366025403784439f;  // (sqrt(3) - 1) / 2
    constexpr float G2 = 0.211324865405187f;  // (3 - sqrt(3)) / 6

    // Skew input space to determine simplex cell
    const float s = (x + y) * F2;
    const int i = static_cast<int>(std::floor(x + s));
    const int j = static_cast<int>(std::floor(y + s));

    // Unskew cell origin back to (x, y) space
    const float t = static_cast<float>(i + j) * G2;
    const float X0 = static_cast<float>(i) - t;
    const float Y0 = static_cast<float>(j) - t;

    // Position relative to cell origin
    const float x0 = x - X0;
    const float y0 = y - Y0;

    // Determine which simplex we're in
    int i1, j1;
    if (x0 > y0) {
        i1 = 1; j1 = 0;  // Lower triangle
    } else {
        i1 = 0; j1 = 1;  // Upper triangle
    }

    // Offsets for other corners
    const float x1 = x0 - static_cast<float>(i1) + G2;
    const float y1 = y0 - static_cast<float>(j1) + G2;
    const float x2 = x0 - 1.0f + 2.0f * G2;
    const float y2 = y0 - 1.0f + 2.0f * G2;

    // Hashed gradient indices
    const int ii = i & 255;
    const int jj = j & 255;
    const int gi0 = s_permutation[ii + s_permutation[jj]];
    const int gi1 = s_permutation[ii + i1 + s_permutation[jj + j1]];
    const int gi2 = s_permutation[ii + 1 + s_permutation[jj + 1]];

    // Calculate contributions from each corner
    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 >= 0.0f) {
        t0 *= t0;
        n0 = t0 * t0 * Grad(gi0, x0, y0);
    }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 >= 0.0f) {
        t1 *= t1;
        n1 = t1 * t1 * Grad(gi1, x1, y1);
    }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 >= 0.0f) {
        t2 *= t2;
        n2 = t2 * t2 * Grad(gi2, x2, y2);
    }

    // Scale to [0, 1]
    return (70.0f * (n0 + n1 + n2) + 1.0f) * 0.5f;
}

// ============================================================================
// Simplex Noise (3D)
// ============================================================================

float NoiseGenerator::Simplex(float x, float y, float z) noexcept {
    // For simplicity, use optimized 3D Perlin
    // A proper 3D simplex would be more complex
    return Perlin(x, y, z);
}

// ============================================================================
// Fractal Brownian Motion (FBM) Noise
// ============================================================================

float NoiseGenerator::FractalNoise(float x, float y, int octaves,
                                    float persistence, float lacunarity) noexcept {
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
                                    float persistence, float lacunarity) noexcept {
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

// ============================================================================
// Ridged Multifractal Noise
// ============================================================================

float NoiseGenerator::RidgedNoise(float x, float y, int octaves,
                                   float persistence, float lacunarity) noexcept {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    float weight = 1.0f;

    for (int i = 0; i < octaves; ++i) {
        // Get noise value and convert to ridge
        float n = Perlin(x * frequency, y * frequency);
        n = 1.0f - std::abs(n * 2.0f - 1.0f);  // Create ridge
        n = n * n;  // Sharpen ridge

        // Weight successive contributions
        n *= weight;
        weight = std::clamp(n * 2.0f, 0.0f, 1.0f);

        total += n * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

// ============================================================================
// Billowy Noise (Inverted Ridged)
// ============================================================================

float NoiseGenerator::BillowNoise(float x, float y, int octaves,
                                   float persistence, float lacunarity) noexcept {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        // Get noise and convert to billow
        float n = Perlin(x * frequency, y * frequency);
        n = std::abs(n * 2.0f - 1.0f);  // Billow effect

        total += n * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

// ============================================================================
// Worley/Cellular Noise
// ============================================================================

float NoiseGenerator::Worley(float x, float y) noexcept {
    if (!s_initialized.load(std::memory_order_acquire)) {
        const_cast<void(*)()>(Initialize)();
    }

    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));

    float minDist = 2.0f;

    // Check 3x3 neighborhood
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int cx = xi + dx;
            const int cy = yi + dy;

            // Generate pseudo-random point in cell
            const int hash = Hash(cx, cy);
            const float px = static_cast<float>(cx) + (static_cast<float>(hash & 255) / 255.0f);
            const float py = static_cast<float>(cy) + (static_cast<float>((hash >> 8) & 255) / 255.0f);

            // Distance to point
            const float distX = x - px;
            const float distY = y - py;
            const float dist = std::sqrt(distX * distX + distY * distY);

            minDist = std::min(minDist, dist);
        }
    }

    return std::clamp(minDist, 0.0f, 1.0f);
}

float NoiseGenerator::WorleyF2F1(float x, float y) noexcept {
    if (!s_initialized.load(std::memory_order_acquire)) {
        const_cast<void(*)()>(Initialize)();
    }

    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));

    float minDist1 = 2.0f;  // F1 - closest
    float minDist2 = 2.0f;  // F2 - second closest

    // Check 3x3 neighborhood
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int cx = xi + dx;
            const int cy = yi + dy;

            const int hash = Hash(cx, cy);
            const float px = static_cast<float>(cx) + (static_cast<float>(hash & 255) / 255.0f);
            const float py = static_cast<float>(cy) + (static_cast<float>((hash >> 8) & 255) / 255.0f);

            const float distX = x - px;
            const float distY = y - py;
            const float dist = std::sqrt(distX * distX + distY * distY);

            if (dist < minDist1) {
                minDist2 = minDist1;
                minDist1 = dist;
            } else if (dist < minDist2) {
                minDist2 = dist;
            }
        }
    }

    return std::clamp(minDist2 - minDist1, 0.0f, 1.0f);
}

} // namespace Nova
