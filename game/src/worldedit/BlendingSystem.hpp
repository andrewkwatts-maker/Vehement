#pragma once

#include "../world/Tile.hpp"
#include "LocationDefinition.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <functional>

namespace Vehement {

// Forward declarations
class TileMap;
class RoadEditor;

/**
 * @brief Blending algorithm type
 */
enum class BlendAlgorithm : uint8_t {
    Linear,         ///< Simple linear interpolation
    Smooth,         ///< Smoothstep interpolation
    Perlin,         ///< Perlin noise-based blending
    Cellular,       ///< Cellular/Voronoi blending
    Custom          ///< Custom blend function
};

/**
 * @brief Get display name for blend algorithm
 */
inline const char* GetBlendAlgorithmName(BlendAlgorithm algo) {
    switch (algo) {
        case BlendAlgorithm::Linear:   return "Linear";
        case BlendAlgorithm::Smooth:   return "Smooth";
        case BlendAlgorithm::Perlin:   return "Perlin Noise";
        case BlendAlgorithm::Cellular: return "Cellular";
        case BlendAlgorithm::Custom:   return "Custom";
        default:                       return "Unknown";
    }
}

/**
 * @brief Transition tile type for edge blending
 */
enum class TransitionType : uint8_t {
    None,           ///< No transition
    Fade,           ///< Fade tiles
    Scatter,        ///< Scattered transition
    Gradient,       ///< Gradient transition
    Natural         ///< Natural-looking organic transition
};

/**
 * @brief Configuration for edge blending
 */
struct BlendConfig {
    BlendAlgorithm algorithm = BlendAlgorithm::Smooth;
    TransitionType transitionType = TransitionType::Natural;
    int blendRadius = 5;            ///< Radius of blend zone in tiles
    float noiseScale = 0.1f;        ///< Scale of noise for Perlin/Cellular
    float noiseStrength = 0.5f;     ///< Strength of noise influence
    bool preserveRoads = true;      ///< Don't blend over roads
    bool preserveBuildings = true;  ///< Don't blend over building footprints
    bool smoothElevation = true;    ///< Smooth elevation at boundaries
    uint32_t seed = 12345;          ///< Random seed for noise
};

/**
 * @brief Result of a blend operation
 */
struct BlendResult {
    std::vector<glm::ivec2> modifiedTiles;
    std::vector<std::pair<glm::ivec2, TileType>> originalTiles;  ///< For undo
    int tilesBlended = 0;
    bool success = true;
    std::string errorMessage;
};

/**
 * @brief Edge tile information for blending
 */
struct EdgeTile {
    glm::ivec2 position{0, 0};
    float distanceFromEdge = 0.0f;
    float blendFactor = 0.0f;       ///< 0 = manual, 1 = PCG
    TileType manualType = TileType::None;
    TileType pcgType = TileType::None;
    bool isRoad = false;
    bool isBuilding = false;
};

/**
 * @brief System for blending manual edits with procedural content generation
 *
 * Features:
 * - Multiple blending algorithms
 * - Smooth edge transitions
 * - Transition tiles at boundaries
 * - Road connectivity preservation
 * - Natural-looking borders
 *
 * Usage:
 * 1. Configure blend settings
 * 2. Identify manual edit boundaries
 * 3. Apply blending to create smooth transitions
 * 4. Optionally preserve roads and buildings
 */
class BlendingSystem {
public:
    using BlendFunction = std::function<float(float distance, float maxDistance)>;
    using TileResolver = std::function<TileType(TileType manual, TileType pcg, float factor)>;

    BlendingSystem();
    ~BlendingSystem() = default;

    // Allow copy and move
    BlendingSystem(const BlendingSystem&) = default;
    BlendingSystem& operator=(const BlendingSystem&) = default;
    BlendingSystem(BlendingSystem&&) noexcept = default;
    BlendingSystem& operator=(BlendingSystem&&) noexcept = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /** @brief Get blend configuration */
    [[nodiscard]] const BlendConfig& GetConfig() const noexcept { return m_config; }

    /** @brief Get mutable blend configuration */
    [[nodiscard]] BlendConfig& GetConfig() noexcept { return m_config; }

    /** @brief Set blend configuration */
    void SetConfig(const BlendConfig& config) noexcept { m_config = config; }

    /** @brief Set blend algorithm */
    void SetAlgorithm(BlendAlgorithm algo) noexcept { m_config.algorithm = algo; }

    /** @brief Set blend radius */
    void SetBlendRadius(int radius) noexcept { m_config.blendRadius = std::max(1, radius); }

    /** @brief Set custom blend function */
    void SetCustomBlendFunction(BlendFunction func) { m_customBlendFunction = std::move(func); }

    /** @brief Set custom tile resolver */
    void SetCustomTileResolver(TileResolver func) { m_customTileResolver = std::move(func); }

    // =========================================================================
    // Edge Detection
    // =========================================================================

    /**
     * @brief Find edge tiles of a location
     * @param location Location to find edges of
     * @param map Tile map
     * @return Vector of edge tiles with blend information
     */
    [[nodiscard]] std::vector<EdgeTile> FindEdgeTiles(
        const LocationDefinition& location,
        const TileMap& map) const;

    /**
     * @brief Find edge tiles of a selection rectangle
     */
    [[nodiscard]] std::vector<EdgeTile> FindEdgeTiles(
        const glm::ivec2& min,
        const glm::ivec2& max,
        const TileMap& map) const;

    /**
     * @brief Check if a tile is on the edge of manual edits
     */
    [[nodiscard]] bool IsEdgeTile(
        const glm::ivec2& pos,
        const LocationDefinition& location) const;

    // =========================================================================
    // Blending Operations
    // =========================================================================

    /**
     * @brief Blend a location with surrounding PCG content
     * @param location Location to blend
     * @param manualMap Map with manual edits
     * @param pcgMap Map with PCG content
     * @param outputMap Output map (can be same as manualMap)
     * @return Blend result with affected tiles
     */
    BlendResult BlendLocation(
        const LocationDefinition& location,
        const TileMap& manualMap,
        const TileMap& pcgMap,
        TileMap& outputMap);

    /**
     * @brief Blend edges of a rectangular region
     */
    BlendResult BlendRectangle(
        const glm::ivec2& min,
        const glm::ivec2& max,
        const TileMap& manualMap,
        const TileMap& pcgMap,
        TileMap& outputMap);

    /**
     * @brief Blend a single tile
     * @param pos Tile position
     * @param distanceFromEdge Distance from edge (0 = on edge)
     * @param manualType Manual edit tile type
     * @param pcgType PCG tile type
     * @return Blended tile type
     */
    [[nodiscard]] TileType BlendTile(
        const glm::ivec2& pos,
        float distanceFromEdge,
        TileType manualType,
        TileType pcgType) const;

    /**
     * @brief Calculate blend factor at a position
     * @param distance Distance from edge
     * @param maxDistance Maximum blend distance
     * @return Blend factor 0-1 (0 = manual, 1 = PCG)
     */
    [[nodiscard]] float CalculateBlendFactor(float distance, float maxDistance) const;

    // =========================================================================
    // Transition Tiles
    // =========================================================================

    /**
     * @brief Get appropriate transition tile between two types
     * @param fromType Source tile type
     * @param toType Destination tile type
     * @param blendFactor How far along the transition (0-1)
     * @return Transition tile type
     */
    [[nodiscard]] TileType GetTransitionTile(
        TileType fromType,
        TileType toType,
        float blendFactor) const;

    /**
     * @brief Check if two tile types can have a natural transition
     */
    [[nodiscard]] bool CanTransition(TileType fromType, TileType toType) const;

    // =========================================================================
    // Road Connectivity
    // =========================================================================

    /**
     * @brief Preserve road connectivity during blending
     * @param edges Edge tiles being blended
     * @param roads Road editor with road network
     * @return Modified edge tiles with road preservation
     */
    std::vector<EdgeTile> PreserveRoadConnectivity(
        const std::vector<EdgeTile>& edges,
        const RoadEditor& roads) const;

    /**
     * @brief Find road connection points at location boundary
     */
    [[nodiscard]] std::vector<glm::ivec2> FindRoadConnectionPoints(
        const LocationDefinition& location,
        const RoadEditor& roads) const;

    // =========================================================================
    // Elevation Blending
    // =========================================================================

    /**
     * @brief Smooth elevation at location boundaries
     * @param location Location to smooth
     * @param map Tile map to modify
     * @return Number of tiles affected
     */
    int SmoothElevation(
        const LocationDefinition& location,
        TileMap& map);

    /**
     * @brief Blend elevation between two values
     */
    [[nodiscard]] float BlendElevation(float manualElev, float pcgElev, float factor) const;

    // =========================================================================
    // Natural Borders
    // =========================================================================

    /**
     * @brief Generate natural-looking border
     * @param location Location boundary
     * @param map Tile map to modify
     * @return Tiles modified for natural border
     */
    std::vector<glm::ivec2> GenerateNaturalBorder(
        const LocationDefinition& location,
        TileMap& map);

    /**
     * @brief Add scatter tiles at border
     */
    void AddBorderScatter(
        const std::vector<EdgeTile>& edges,
        TileMap& map,
        TileType scatterType,
        float density);

    // =========================================================================
    // Preview
    // =========================================================================

    /**
     * @brief Get preview of blend result without applying
     */
    [[nodiscard]] std::vector<std::pair<glm::ivec2, TileType>> PreviewBlend(
        const LocationDefinition& location,
        const TileMap& manualMap,
        const TileMap& pcgMap) const;

    /**
     * @brief Get blend factor visualization (for editor display)
     */
    [[nodiscard]] std::vector<std::pair<glm::ivec2, float>> GetBlendFactorMap(
        const LocationDefinition& location) const;

private:
    float LinearBlend(float distance, float maxDistance) const;
    float SmoothBlend(float distance, float maxDistance) const;
    float PerlinBlend(float distance, float maxDistance, const glm::ivec2& pos) const;
    float CellularBlend(float distance, float maxDistance, const glm::ivec2& pos) const;

    float GenerateNoise(float x, float y, float scale) const;

    BlendConfig m_config;
    BlendFunction m_customBlendFunction;
    TileResolver m_customTileResolver;
};

} // namespace Vehement
