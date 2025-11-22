#include "BlendingSystem.hpp"
#include "../world/TileMap.hpp"
#include "RoadEditor.hpp"
#include <cmath>
#include <algorithm>
#include <random>

namespace Vehement {

// =============================================================================
// Constructor
// =============================================================================

BlendingSystem::BlendingSystem() = default;

// =============================================================================
// Edge Detection
// =============================================================================

std::vector<EdgeTile> BlendingSystem::FindEdgeTiles(
    const LocationDefinition& location,
    const TileMap& map) const {

    std::vector<EdgeTile> edges;

    const auto& bounds = location.GetWorldBounds();
    int minX = static_cast<int>(bounds.min.x);
    int maxX = static_cast<int>(bounds.max.x);
    int minZ = static_cast<int>(bounds.min.z);
    int maxZ = static_cast<int>(bounds.max.z);

    int radius = m_config.blendRadius;

    // Find tiles in blend zone (outside location but within radius)
    for (int z = minZ - radius; z <= maxZ + radius; ++z) {
        for (int x = minX - radius; x <= maxX + radius; ++x) {
            if (!map.IsValidPosition(x, z)) continue;

            // Check if inside location
            bool inside = (x >= minX && x <= maxX && z >= minZ && z <= maxZ);

            if (!inside) {
                // Calculate distance from location edge
                float distX = 0.0f;
                float distZ = 0.0f;

                if (x < minX) distX = static_cast<float>(minX - x);
                else if (x > maxX) distX = static_cast<float>(x - maxX);

                if (z < minZ) distZ = static_cast<float>(minZ - z);
                else if (z > maxZ) distZ = static_cast<float>(z - maxZ);

                float distance = std::sqrt(distX * distX + distZ * distZ);

                if (distance <= static_cast<float>(radius)) {
                    EdgeTile edge;
                    edge.position = glm::ivec2(x, z);
                    edge.distanceFromEdge = distance;
                    edge.blendFactor = CalculateBlendFactor(distance, static_cast<float>(radius));

                    const Tile& tile = map.GetTile(x, z);
                    edge.pcgType = tile.type;
                    edge.isRoad = (tile.type >= TileType::ConcreteAsphalt1 &&
                                  tile.type <= TileType::ConcreteTiles2);

                    edges.push_back(edge);
                }
            }
        }
    }

    return edges;
}

std::vector<EdgeTile> BlendingSystem::FindEdgeTiles(
    const glm::ivec2& min,
    const glm::ivec2& max,
    const TileMap& map) const {

    std::vector<EdgeTile> edges;
    int radius = m_config.blendRadius;

    for (int z = min.y - radius; z <= max.y + radius; ++z) {
        for (int x = min.x - radius; x <= max.x + radius; ++x) {
            if (!map.IsValidPosition(x, z)) continue;

            bool inside = (x >= min.x && x <= max.x && z >= min.y && z <= max.y);

            if (!inside) {
                float distX = 0.0f;
                float distZ = 0.0f;

                if (x < min.x) distX = static_cast<float>(min.x - x);
                else if (x > max.x) distX = static_cast<float>(x - max.x);

                if (z < min.y) distZ = static_cast<float>(min.y - z);
                else if (z > max.y) distZ = static_cast<float>(z - max.y);

                float distance = std::sqrt(distX * distX + distZ * distZ);

                if (distance <= static_cast<float>(radius)) {
                    EdgeTile edge;
                    edge.position = glm::ivec2(x, z);
                    edge.distanceFromEdge = distance;
                    edge.blendFactor = CalculateBlendFactor(distance, static_cast<float>(radius));

                    const Tile& tile = map.GetTile(x, z);
                    edge.pcgType = tile.type;

                    edges.push_back(edge);
                }
            }
        }
    }

    return edges;
}

bool BlendingSystem::IsEdgeTile(
    const glm::ivec2& pos,
    const LocationDefinition& location) const {

    const auto& bounds = location.GetWorldBounds();
    int minX = static_cast<int>(bounds.min.x);
    int maxX = static_cast<int>(bounds.max.x);
    int minZ = static_cast<int>(bounds.min.z);
    int maxZ = static_cast<int>(bounds.max.z);

    // Check if on boundary
    bool onBoundary = (pos.x == minX || pos.x == maxX ||
                       pos.y == minZ || pos.y == maxZ);

    return onBoundary && pos.x >= minX && pos.x <= maxX &&
           pos.y >= minZ && pos.y <= maxZ;
}

// =============================================================================
// Blending Operations
// =============================================================================

BlendResult BlendingSystem::BlendLocation(
    const LocationDefinition& location,
    const TileMap& manualMap,
    const TileMap& pcgMap,
    TileMap& outputMap) {

    BlendResult result;

    auto edges = FindEdgeTiles(location, pcgMap);

    for (const auto& edge : edges) {
        if (!outputMap.IsValidPosition(edge.position.x, edge.position.y)) {
            continue;
        }

        // Skip roads if configured
        if (m_config.preserveRoads && edge.isRoad) {
            continue;
        }

        // Skip buildings if configured
        if (m_config.preserveBuildings && edge.isBuilding) {
            continue;
        }

        TileType manualType = TileType::None;
        if (manualMap.IsValidPosition(edge.position.x, edge.position.y)) {
            manualType = manualMap.GetTile(edge.position.x, edge.position.y).type;
        }

        TileType blendedType = BlendTile(
            edge.position, edge.distanceFromEdge, manualType, edge.pcgType);

        Tile& outTile = outputMap.GetTile(edge.position.x, edge.position.y);

        // Store original for undo
        result.originalTiles.emplace_back(edge.position, outTile.type);

        // Apply blended tile
        outTile.type = blendedType;

        result.modifiedTiles.push_back(edge.position);
        result.tilesBlended++;
    }

    // Smooth elevation if configured
    if (m_config.smoothElevation) {
        SmoothElevation(location, outputMap);
    }

    result.success = true;
    return result;
}

BlendResult BlendingSystem::BlendRectangle(
    const glm::ivec2& min,
    const glm::ivec2& max,
    const TileMap& manualMap,
    const TileMap& pcgMap,
    TileMap& outputMap) {

    BlendResult result;

    auto edges = FindEdgeTiles(min, max, pcgMap);

    for (const auto& edge : edges) {
        if (!outputMap.IsValidPosition(edge.position.x, edge.position.y)) {
            continue;
        }

        TileType manualType = TileType::None;
        if (manualMap.IsValidPosition(edge.position.x, edge.position.y)) {
            manualType = manualMap.GetTile(edge.position.x, edge.position.y).type;
        }

        TileType blendedType = BlendTile(
            edge.position, edge.distanceFromEdge, manualType, edge.pcgType);

        Tile& outTile = outputMap.GetTile(edge.position.x, edge.position.y);
        result.originalTiles.emplace_back(edge.position, outTile.type);
        outTile.type = blendedType;

        result.modifiedTiles.push_back(edge.position);
        result.tilesBlended++;
    }

    result.success = true;
    return result;
}

TileType BlendingSystem::BlendTile(
    const glm::ivec2& pos,
    float distanceFromEdge,
    TileType manualType,
    TileType pcgType) const {

    // If custom resolver is set, use it
    if (m_customTileResolver) {
        float factor = CalculateBlendFactor(distanceFromEdge, static_cast<float>(m_config.blendRadius));
        return m_customTileResolver(manualType, pcgType, factor);
    }

    float factor = CalculateBlendFactor(distanceFromEdge, static_cast<float>(m_config.blendRadius));

    // Get transition tile
    return GetTransitionTile(manualType, pcgType, factor);
}

float BlendingSystem::CalculateBlendFactor(float distance, float maxDistance) const {
    if (maxDistance <= 0.0f) return 1.0f;
    if (distance <= 0.0f) return 0.0f;
    if (distance >= maxDistance) return 1.0f;

    // If custom function is set, use it
    if (m_customBlendFunction) {
        return m_customBlendFunction(distance, maxDistance);
    }

    switch (m_config.algorithm) {
        case BlendAlgorithm::Linear:
            return LinearBlend(distance, maxDistance);
        case BlendAlgorithm::Smooth:
            return SmoothBlend(distance, maxDistance);
        case BlendAlgorithm::Perlin:
            return PerlinBlend(distance, maxDistance, glm::ivec2(0)); // Position not available here
        case BlendAlgorithm::Cellular:
            return CellularBlend(distance, maxDistance, glm::ivec2(0));
        default:
            return SmoothBlend(distance, maxDistance);
    }
}

// =============================================================================
// Transition Tiles
// =============================================================================

TileType BlendingSystem::GetTransitionTile(
    TileType fromType,
    TileType toType,
    float blendFactor) const {

    // If factor is very low, use manual tile
    if (blendFactor < 0.3f) {
        return fromType != TileType::None ? fromType : toType;
    }

    // If factor is very high, use PCG tile
    if (blendFactor > 0.7f) {
        return toType;
    }

    // Middle range - try to find appropriate transition
    if (m_config.transitionType == TransitionType::Scatter) {
        // Probabilistic selection based on factor
        std::mt19937 rng(m_config.seed);
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < blendFactor) {
            return toType;
        }
        return fromType != TileType::None ? fromType : toType;
    }

    // Check if we have fade tiles
    if (m_config.transitionType == TransitionType::Fade) {
        // Return fade tile based on direction
        // This would need proper fade tile detection
        if (blendFactor < 0.5f) {
            return fromType != TileType::None ? fromType : toType;
        }
        return toType;
    }

    // Natural transition - mix based on terrain compatibility
    if (m_config.transitionType == TransitionType::Natural) {
        // Ground transitions naturally
        if (IsGroundTile(fromType) && IsGroundTile(toType)) {
            return blendFactor < 0.5f ? fromType : toType;
        }
    }

    // Default: use factor threshold
    return blendFactor < 0.5f ?
           (fromType != TileType::None ? fromType : toType) : toType;
}

bool BlendingSystem::CanTransition(TileType fromType, TileType toType) const {
    // Ground tiles can transition to each other
    if (IsGroundTile(fromType) && IsGroundTile(toType)) {
        return true;
    }

    // Concrete can transition to concrete
    if ((fromType >= TileType::ConcreteAsphalt1 && fromType <= TileType::ConcreteTiles2) &&
        (toType >= TileType::ConcreteAsphalt1 && toType <= TileType::ConcreteTiles2)) {
        return true;
    }

    return false;
}

// =============================================================================
// Road Connectivity
// =============================================================================

std::vector<EdgeTile> BlendingSystem::PreserveRoadConnectivity(
    const std::vector<EdgeTile>& edges,
    const RoadEditor& roads) const {

    std::vector<EdgeTile> result = edges;

    auto affectedTiles = roads.GetAffectedTiles();

    for (auto& edge : result) {
        for (const auto& roadTile : affectedTiles) {
            if (edge.position == roadTile) {
                edge.isRoad = true;
                break;
            }
        }
    }

    return result;
}

std::vector<glm::ivec2> BlendingSystem::FindRoadConnectionPoints(
    const LocationDefinition& location,
    const RoadEditor& roads) const {

    std::vector<glm::ivec2> connections;

    const auto& bounds = location.GetWorldBounds();
    auto roadTiles = roads.GetAffectedTiles();

    for (const auto& tile : roadTiles) {
        // Check if tile is on location boundary
        float x = static_cast<float>(tile.x);
        float z = static_cast<float>(tile.y);

        bool onEdge = (std::abs(x - bounds.min.x) < 1.0f ||
                       std::abs(x - bounds.max.x) < 1.0f ||
                       std::abs(z - bounds.min.z) < 1.0f ||
                       std::abs(z - bounds.max.z) < 1.0f);

        if (onEdge) {
            connections.push_back(tile);
        }
    }

    return connections;
}

// =============================================================================
// Elevation Blending
// =============================================================================

int BlendingSystem::SmoothElevation(
    const LocationDefinition& location,
    TileMap& map) {

    int tilesAffected = 0;

    auto edges = FindEdgeTiles(location, map);

    for (const auto& edge : edges) {
        if (!map.IsValidPosition(edge.position.x, edge.position.y)) {
            continue;
        }

        Tile& tile = map.GetTile(edge.position.x, edge.position.y);

        // Average elevation with neighbors
        float totalElev = tile.wallHeight;
        int count = 1;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;

                int nx = edge.position.x + dx;
                int ny = edge.position.y + dy;

                if (map.IsValidPosition(nx, ny)) {
                    totalElev += map.GetTile(nx, ny).wallHeight;
                    ++count;
                }
            }
        }

        float avgElev = totalElev / count;
        float blendedElev = BlendElevation(tile.wallHeight, avgElev, edge.blendFactor);

        if (std::abs(tile.wallHeight - blendedElev) > 0.01f) {
            tile.wallHeight = blendedElev;
            ++tilesAffected;
        }
    }

    return tilesAffected;
}

float BlendingSystem::BlendElevation(float manualElev, float pcgElev, float factor) const {
    return manualElev + (pcgElev - manualElev) * factor;
}

// =============================================================================
// Natural Borders
// =============================================================================

std::vector<glm::ivec2> BlendingSystem::GenerateNaturalBorder(
    const LocationDefinition& location,
    TileMap& map) {

    std::vector<glm::ivec2> modifiedTiles;

    auto edges = FindEdgeTiles(location, map);

    std::mt19937 rng(m_config.seed);

    for (const auto& edge : edges) {
        if (!map.IsValidPosition(edge.position.x, edge.position.y)) {
            continue;
        }

        // Add noise to the blend factor for natural look
        float noise = GenerateNoise(
            static_cast<float>(edge.position.x),
            static_cast<float>(edge.position.y),
            m_config.noiseScale
        );

        float adjustedFactor = edge.blendFactor + noise * m_config.noiseStrength;
        adjustedFactor = std::clamp(adjustedFactor, 0.0f, 1.0f);

        Tile& tile = map.GetTile(edge.position.x, edge.position.y);
        TileType blendedType = GetTransitionTile(TileType::None, tile.type, adjustedFactor);

        if (blendedType != tile.type) {
            tile.type = blendedType;
            modifiedTiles.push_back(edge.position);
        }
    }

    return modifiedTiles;
}

void BlendingSystem::AddBorderScatter(
    const std::vector<EdgeTile>& edges,
    TileMap& map,
    TileType scatterType,
    float density) {

    std::mt19937 rng(m_config.seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (const auto& edge : edges) {
        if (!map.IsValidPosition(edge.position.x, edge.position.y)) {
            continue;
        }

        // Scatter probability based on distance from edge and density
        float probability = (1.0f - edge.blendFactor) * density;

        if (dist(rng) < probability) {
            Tile& tile = map.GetTile(edge.position.x, edge.position.y);
            tile.type = scatterType;
        }
    }
}

// =============================================================================
// Preview
// =============================================================================

std::vector<std::pair<glm::ivec2, TileType>> BlendingSystem::PreviewBlend(
    const LocationDefinition& location,
    const TileMap& manualMap,
    const TileMap& pcgMap) const {

    std::vector<std::pair<glm::ivec2, TileType>> preview;

    auto edges = FindEdgeTiles(location, pcgMap);

    for (const auto& edge : edges) {
        TileType manualType = TileType::None;
        if (manualMap.IsValidPosition(edge.position.x, edge.position.y)) {
            manualType = manualMap.GetTile(edge.position.x, edge.position.y).type;
        }

        TileType blendedType = BlendTile(
            edge.position, edge.distanceFromEdge, manualType, edge.pcgType);

        preview.emplace_back(edge.position, blendedType);
    }

    return preview;
}

std::vector<std::pair<glm::ivec2, float>> BlendingSystem::GetBlendFactorMap(
    const LocationDefinition& location) const {

    std::vector<std::pair<glm::ivec2, float>> factorMap;

    const auto& bounds = location.GetWorldBounds();
    int minX = static_cast<int>(bounds.min.x);
    int maxX = static_cast<int>(bounds.max.x);
    int minZ = static_cast<int>(bounds.min.z);
    int maxZ = static_cast<int>(bounds.max.z);
    int radius = m_config.blendRadius;

    for (int z = minZ - radius; z <= maxZ + radius; ++z) {
        for (int x = minX - radius; x <= maxX + radius; ++x) {
            float distX = 0.0f;
            float distZ = 0.0f;

            if (x < minX) distX = static_cast<float>(minX - x);
            else if (x > maxX) distX = static_cast<float>(x - maxX);

            if (z < minZ) distZ = static_cast<float>(minZ - z);
            else if (z > maxZ) distZ = static_cast<float>(z - maxZ);

            float distance = std::sqrt(distX * distX + distZ * distZ);
            float factor = CalculateBlendFactor(distance, static_cast<float>(radius));

            factorMap.emplace_back(glm::ivec2(x, z), factor);
        }
    }

    return factorMap;
}

// =============================================================================
// Private Helpers - Blend Algorithms
// =============================================================================

float BlendingSystem::LinearBlend(float distance, float maxDistance) const {
    return distance / maxDistance;
}

float BlendingSystem::SmoothBlend(float distance, float maxDistance) const {
    float t = distance / maxDistance;
    return t * t * (3.0f - 2.0f * t);  // Smoothstep
}

float BlendingSystem::PerlinBlend(float distance, float maxDistance, const glm::ivec2& pos) const {
    float baseBlend = SmoothBlend(distance, maxDistance);
    float noise = GenerateNoise(static_cast<float>(pos.x), static_cast<float>(pos.y), m_config.noiseScale);
    return std::clamp(baseBlend + noise * m_config.noiseStrength, 0.0f, 1.0f);
}

float BlendingSystem::CellularBlend(float distance, float maxDistance, const glm::ivec2& pos) const {
    float baseBlend = SmoothBlend(distance, maxDistance);

    // Simple cellular noise approximation
    float cellNoise = GenerateNoise(
        std::floor(static_cast<float>(pos.x) * m_config.noiseScale),
        std::floor(static_cast<float>(pos.y) * m_config.noiseScale),
        1.0f
    );

    return std::clamp(baseBlend + cellNoise * m_config.noiseStrength, 0.0f, 1.0f);
}

float BlendingSystem::GenerateNoise(float x, float y, float scale) const {
    // Simple hash-based noise
    float fx = x * scale;
    float fy = y * scale;

    int xi = static_cast<int>(std::floor(fx));
    int yi = static_cast<int>(std::floor(fy));

    auto hash = [seed = m_config.seed](int x, int y) -> float {
        uint32_t h = seed;
        h ^= static_cast<uint32_t>(x) * 374761393u;
        h ^= static_cast<uint32_t>(y) * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF) * 2.0f - 1.0f;
    };

    float xf = fx - xi;
    float yf = fy - yi;

    float u = xf * xf * (3.0f - 2.0f * xf);
    float v = yf * yf * (3.0f - 2.0f * yf);

    float n00 = hash(xi, yi);
    float n10 = hash(xi + 1, yi);
    float n01 = hash(xi, yi + 1);
    float n11 = hash(xi + 1, yi + 1);

    float nx0 = n00 + u * (n10 - n00);
    float nx1 = n01 + u * (n11 - n01);

    return nx0 + v * (nx1 - nx0);
}

} // namespace Vehement
