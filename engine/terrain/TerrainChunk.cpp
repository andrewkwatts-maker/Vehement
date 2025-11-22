/**
 * @file TerrainChunk.cpp
 * @brief Terrain chunk utilities and helper functions
 *
 * Main TerrainChunk implementation is in TerrainGenerator.cpp for cohesion.
 * This file contains additional utilities for terrain manipulation.
 */

#include "terrain/TerrainGenerator.hpp"
#include "terrain/NoiseGenerator.hpp"
#include <cmath>
#include <algorithm>

namespace Nova {

// ============================================================================
// Terrain Utility Functions
// ============================================================================

namespace TerrainUtils {

/**
 * @brief Calculate slope at a given point using height differences
 * @param terrain Reference to terrain generator
 * @param x World X coordinate
 * @param z World Z coordinate
 * @param delta Sample distance for derivative calculation
 * @return Slope angle in radians
 */
float CalculateSlope(const TerrainGenerator& terrain, float x, float z, float delta = 1.0f) {
    const float hC = terrain.GetHeightAt(x, z);
    const float hL = terrain.GetHeightAt(x - delta, z);
    const float hR = terrain.GetHeightAt(x + delta, z);
    const float hD = terrain.GetHeightAt(x, z - delta);
    const float hU = terrain.GetHeightAt(x, z + delta);

    // Calculate partial derivatives
    const float dhdx = (hR - hL) / (2.0f * delta);
    const float dhdz = (hU - hD) / (2.0f * delta);

    // Magnitude of gradient gives slope
    const float gradient = std::sqrt(dhdx * dhdx + dhdz * dhdz);

    return std::atan(gradient);
}

/**
 * @brief Calculate curvature (convexity/concavity) at a point
 * @return Positive for convex (hilltop), negative for concave (valley)
 */
float CalculateCurvature(const TerrainGenerator& terrain, float x, float z, float delta = 1.0f) {
    const float hC = terrain.GetHeightAt(x, z);
    const float hL = terrain.GetHeightAt(x - delta, z);
    const float hR = terrain.GetHeightAt(x + delta, z);
    const float hD = terrain.GetHeightAt(x, z - delta);
    const float hU = terrain.GetHeightAt(x, z + delta);

    // Laplacian approximation
    return (hL + hR + hD + hU - 4.0f * hC) / (delta * delta);
}

/**
 * @brief Get terrain type based on height and slope
 */
enum class TerrainType {
    DeepWater,
    ShallowWater,
    Beach,
    Grassland,
    Forest,
    Mountain,
    Snow
};

TerrainType ClassifyTerrain(float height, float slope, float maxHeight) {
    const float normalizedHeight = height / maxHeight;
    const float slopeDegrees = slope * 180.0f / 3.14159265f;

    if (normalizedHeight < 0.1f) {
        return TerrainType::DeepWater;
    } else if (normalizedHeight < 0.2f) {
        return TerrainType::ShallowWater;
    } else if (normalizedHeight < 0.25f) {
        return TerrainType::Beach;
    } else if (normalizedHeight < 0.6f) {
        if (slopeDegrees > 30.0f) {
            return TerrainType::Mountain;
        }
        return normalizedHeight < 0.4f ? TerrainType::Grassland : TerrainType::Forest;
    } else if (normalizedHeight < 0.8f) {
        return TerrainType::Mountain;
    } else {
        return TerrainType::Snow;
    }
}

/**
 * @brief Blend factor for texture splatting based on terrain properties
 * @return Vector of blend weights for different terrain textures
 */
glm::vec4 CalculateTextureWeights(float height, float slope, float maxHeight) {
    glm::vec4 weights(0.0f);

    const float normalizedHeight = std::clamp(height / maxHeight, 0.0f, 1.0f);
    const float slopeFactor = std::clamp(slope / 1.0f, 0.0f, 1.0f);  // Normalize slope

    // Weight 0: Sand/Beach (low elevation)
    weights.x = std::max(0.0f, 1.0f - normalizedHeight * 4.0f);

    // Weight 1: Grass (medium elevation, low slope)
    weights.y = std::max(0.0f, (1.0f - std::abs(normalizedHeight - 0.3f) * 3.0f)) * (1.0f - slopeFactor);

    // Weight 2: Rock (high slope or high elevation)
    weights.z = std::max(slopeFactor, normalizedHeight > 0.7f ? normalizedHeight : 0.0f);

    // Weight 3: Snow (very high elevation)
    weights.w = std::max(0.0f, (normalizedHeight - 0.8f) * 5.0f);

    // Normalize weights
    const float sum = weights.x + weights.y + weights.z + weights.w;
    if (sum > 0.001f) {
        weights /= sum;
    } else {
        weights = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);  // Default to grass
    }

    return weights;
}

/**
 * @brief Ray-terrain intersection test
 * @param terrain Terrain generator
 * @param rayOrigin Ray starting point
 * @param rayDir Normalized ray direction
 * @param maxDistance Maximum distance to check
 * @param hitPoint Output: point of intersection
 * @return True if ray hits terrain
 */
bool RaycastTerrain(const TerrainGenerator& terrain,
                    const glm::vec3& rayOrigin,
                    const glm::vec3& rayDir,
                    float maxDistance,
                    glm::vec3& hitPoint) {
    constexpr float stepSize = 1.0f;
    constexpr float refinementSteps = 8;

    glm::vec3 currentPos = rayOrigin;
    float travelled = 0.0f;

    // Coarse stepping
    while (travelled < maxDistance) {
        const float terrainHeight = terrain.GetHeightAt(currentPos.x, currentPos.z);

        if (currentPos.y < terrainHeight) {
            // We've gone below terrain - binary search to refine
            glm::vec3 low = currentPos - rayDir * stepSize;
            glm::vec3 high = currentPos;

            for (int i = 0; i < refinementSteps; ++i) {
                const glm::vec3 mid = (low + high) * 0.5f;
                const float midHeight = terrain.GetHeightAt(mid.x, mid.z);

                if (mid.y < midHeight) {
                    high = mid;
                } else {
                    low = mid;
                }
            }

            hitPoint = (low + high) * 0.5f;
            hitPoint.y = terrain.GetHeightAt(hitPoint.x, hitPoint.z);
            return true;
        }

        currentPos += rayDir * stepSize;
        travelled += stepSize;
    }

    return false;
}

/**
 * @brief Calculate ambient occlusion factor for a point on terrain
 * @return AO factor in range [0, 1] where 1 is fully lit
 */
float CalculateAmbientOcclusion(const TerrainGenerator& terrain, float x, float z,
                                 float sampleRadius = 5.0f, int numSamples = 8) {
    const float centerHeight = terrain.GetHeightAt(x, z);
    float occlusion = 0.0f;

    const float angleStep = 2.0f * 3.14159265f / static_cast<float>(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        const float angle = static_cast<float>(i) * angleStep;
        const float sampleX = x + std::cos(angle) * sampleRadius;
        const float sampleZ = z + std::sin(angle) * sampleRadius;
        const float sampleHeight = terrain.GetHeightAt(sampleX, sampleZ);

        // Height difference contributes to occlusion
        const float heightDiff = sampleHeight - centerHeight;
        if (heightDiff > 0.0f) {
            occlusion += std::min(heightDiff / sampleRadius, 1.0f);
        }
    }

    occlusion /= static_cast<float>(numSamples);
    return 1.0f - std::clamp(occlusion, 0.0f, 1.0f);
}

} // namespace TerrainUtils

} // namespace Nova
