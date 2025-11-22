#pragma once

#include "../world/Tile.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <functional>
#include <random>
#include <cstdint>

namespace Vehement {

// Forward declarations
class TileMap;

/**
 * @brief Brush shape for tile painting
 */
enum class BrushShape : uint8_t {
    Circle,     ///< Circular brush
    Square,     ///< Square brush
    Diamond     ///< Diamond (rotated square) brush
};

/**
 * @brief Get display name for brush shape
 */
inline const char* GetBrushShapeName(BrushShape shape) {
    switch (shape) {
        case BrushShape::Circle:  return "Circle";
        case BrushShape::Square:  return "Square";
        case BrushShape::Diamond: return "Diamond";
        default:                  return "Unknown";
    }
}

/**
 * @brief Brush mode for different operations
 */
enum class BrushMode : uint8_t {
    Paint,      ///< Paint tiles with selected type
    Erase,      ///< Erase tiles (set to empty)
    Sample,     ///< Sample tile type at cursor
    Smooth,     ///< Smooth elevation
    Raise,      ///< Raise elevation
    Lower,      ///< Lower elevation
    Flatten,    ///< Flatten to target elevation
    Noise       ///< Apply noise to tiles
};

/**
 * @brief Get display name for brush mode
 */
inline const char* GetBrushModeName(BrushMode mode) {
    switch (mode) {
        case BrushMode::Paint:   return "Paint";
        case BrushMode::Erase:   return "Erase";
        case BrushMode::Sample:  return "Sample";
        case BrushMode::Smooth:  return "Smooth";
        case BrushMode::Raise:   return "Raise";
        case BrushMode::Lower:   return "Lower";
        case BrushMode::Flatten: return "Flatten";
        case BrushMode::Noise:   return "Noise";
        default:                 return "Unknown";
    }
}

/**
 * @brief Configuration for noise-based variation
 */
struct NoiseConfig {
    float frequency = 0.1f;         ///< Noise frequency
    float amplitude = 1.0f;         ///< Noise amplitude
    int octaves = 3;                ///< Number of octaves
    float persistence = 0.5f;       ///< Persistence between octaves
    uint32_t seed = 12345;          ///< Random seed
    bool applyToVariant = true;     ///< Apply to tile variant
    bool applyToElevation = false;  ///< Apply to elevation
};

/**
 * @brief Represents a tile change made by the brush
 */
struct TileBrushChange {
    int x = 0;
    int y = 0;
    TileType oldType = TileType::None;
    TileType newType = TileType::None;
    uint8_t oldVariant = 0;
    uint8_t newVariant = 0;
    float oldElevation = 0.0f;
    float newElevation = 0.0f;
    bool wasWall = false;
    bool isWall = false;
    float oldWallHeight = 0.0f;
    float newWallHeight = 0.0f;
};

/**
 * @brief Tile painting brush for world editing
 *
 * Features:
 * - Multiple brush shapes (Circle, Square, Diamond)
 * - Adjustable brush size (1-50 tiles)
 * - Multiple modes (Paint, Erase, Sample, Smooth, etc.)
 * - Tile type selection
 * - Elevation adjustment
 * - Noise-based variation
 *
 * Usage:
 * 1. Set brush shape and size
 * 2. Set brush mode
 * 3. Set tile type (for Paint mode)
 * 4. Call Apply() at cursor position
 */
class TileBrush {
public:
    static constexpr int MIN_BRUSH_SIZE = 1;
    static constexpr int MAX_BRUSH_SIZE = 50;

    TileBrush();
    ~TileBrush() = default;

    // Allow copy and move
    TileBrush(const TileBrush&) = default;
    TileBrush& operator=(const TileBrush&) = default;
    TileBrush(TileBrush&&) noexcept = default;
    TileBrush& operator=(TileBrush&&) noexcept = default;

    // =========================================================================
    // Brush Settings
    // =========================================================================

    /** @brief Get brush shape */
    [[nodiscard]] BrushShape GetShape() const noexcept { return m_shape; }

    /** @brief Set brush shape */
    void SetShape(BrushShape shape) noexcept { m_shape = shape; }

    /** @brief Get brush size (in tiles) */
    [[nodiscard]] int GetSize() const noexcept { return m_size; }

    /** @brief Set brush size (clamped to valid range) */
    void SetSize(int size) noexcept;

    /** @brief Get brush mode */
    [[nodiscard]] BrushMode GetMode() const noexcept { return m_mode; }

    /** @brief Set brush mode */
    void SetMode(BrushMode mode) noexcept { m_mode = mode; }

    /** @brief Get brush falloff (0 = hard edge, 1 = soft edge) */
    [[nodiscard]] float GetFalloff() const noexcept { return m_falloff; }

    /** @brief Set brush falloff */
    void SetFalloff(float falloff) noexcept { m_falloff = std::clamp(falloff, 0.0f, 1.0f); }

    /** @brief Get brush opacity (0-1) */
    [[nodiscard]] float GetOpacity() const noexcept { return m_opacity; }

    /** @brief Set brush opacity */
    void SetOpacity(float opacity) noexcept { m_opacity = std::clamp(opacity, 0.0f, 1.0f); }

    // =========================================================================
    // Tile Type Selection
    // =========================================================================

    /** @brief Get selected tile type */
    [[nodiscard]] TileType GetSelectedTile() const noexcept { return m_selectedTile; }

    /** @brief Set selected tile type */
    void SetSelectedTile(TileType type) noexcept { m_selectedTile = type; }

    /** @brief Get selected tile variant */
    [[nodiscard]] uint8_t GetSelectedVariant() const noexcept { return m_selectedVariant; }

    /** @brief Set selected tile variant */
    void SetSelectedVariant(uint8_t variant) noexcept { m_selectedVariant = variant; }

    /** @brief Check if painting walls */
    [[nodiscard]] bool IsWallMode() const noexcept { return m_wallMode; }

    /** @brief Set wall mode */
    void SetWallMode(bool wall) noexcept { m_wallMode = wall; }

    /** @brief Get wall height */
    [[nodiscard]] float GetWallHeight() const noexcept { return m_wallHeight; }

    /** @brief Set wall height */
    void SetWallHeight(float height) noexcept { m_wallHeight = height; }

    // =========================================================================
    // Elevation Settings
    // =========================================================================

    /** @brief Get elevation change amount */
    [[nodiscard]] float GetElevationDelta() const noexcept { return m_elevationDelta; }

    /** @brief Set elevation change amount */
    void SetElevationDelta(float delta) noexcept { m_elevationDelta = delta; }

    /** @brief Get target elevation (for Flatten mode) */
    [[nodiscard]] float GetTargetElevation() const noexcept { return m_targetElevation; }

    /** @brief Set target elevation */
    void SetTargetElevation(float elevation) noexcept { m_targetElevation = elevation; }

    /** @brief Check if using absolute elevation */
    [[nodiscard]] bool IsAbsoluteElevation() const noexcept { return m_absoluteElevation; }

    /** @brief Set absolute elevation mode */
    void SetAbsoluteElevation(bool absolute) noexcept { m_absoluteElevation = absolute; }

    // =========================================================================
    // Noise Settings
    // =========================================================================

    /** @brief Get noise configuration */
    [[nodiscard]] const NoiseConfig& GetNoiseConfig() const noexcept { return m_noiseConfig; }

    /** @brief Get mutable noise configuration */
    [[nodiscard]] NoiseConfig& GetNoiseConfig() noexcept { return m_noiseConfig; }

    /** @brief Set noise configuration */
    void SetNoiseConfig(const NoiseConfig& config) noexcept { m_noiseConfig = config; }

    /** @brief Enable/disable random variant selection */
    void SetRandomVariants(bool random) noexcept { m_randomVariants = random; }

    /** @brief Check if random variants enabled */
    [[nodiscard]] bool IsRandomVariants() const noexcept { return m_randomVariants; }

    // =========================================================================
    // Brush Application
    // =========================================================================

    /**
     * @brief Apply brush at a world position
     * @param map Tile map to modify
     * @param centerX Center X coordinate (tile units)
     * @param centerY Center Y coordinate (tile units)
     * @return Vector of tile changes made
     */
    std::vector<TileBrushChange> Apply(TileMap& map, int centerX, int centerY);

    /**
     * @brief Apply brush along a path (for continuous strokes)
     * @param map Tile map to modify
     * @param startX Start X coordinate
     * @param startY Start Y coordinate
     * @param endX End X coordinate
     * @param endY End Y coordinate
     * @return Vector of tile changes made
     */
    std::vector<TileBrushChange> ApplyLine(
        TileMap& map, int startX, int startY, int endX, int endY);

    /**
     * @brief Sample a tile at position
     * @param map Tile map to sample from
     * @param x X coordinate
     * @param y Y coordinate
     * @return true if tile was sampled
     */
    bool Sample(const TileMap& map, int x, int y);

    /**
     * @brief Get tiles affected by brush at position
     * @param centerX Center X coordinate
     * @param centerY Center Y coordinate
     * @return Vector of affected tile positions
     */
    [[nodiscard]] std::vector<glm::ivec2> GetAffectedTiles(int centerX, int centerY) const;

    /**
     * @brief Get brush strength at a position (for falloff)
     * @param centerX Brush center X
     * @param centerY Brush center Y
     * @param tileX Tile X coordinate
     * @param tileY Tile Y coordinate
     * @return Strength value 0-1
     */
    [[nodiscard]] float GetStrengthAt(int centerX, int centerY, int tileX, int tileY) const;

    // =========================================================================
    // Preview
    // =========================================================================

    /**
     * @brief Get preview tiles for UI rendering
     * @param centerX Center X coordinate
     * @param centerY Center Y coordinate
     * @return Vector of tile positions with strength values
     */
    [[nodiscard]] std::vector<std::pair<glm::ivec2, float>> GetPreviewTiles(
        int centerX, int centerY) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using SampleCallback = std::function<void(TileType, uint8_t)>;

    /** @brief Set callback for tile sampling */
    void SetOnSample(SampleCallback callback) { m_onSample = std::move(callback); }

private:
    // Brush application helpers
    TileBrushChange ApplyPaint(TileMap& map, int x, int y, float strength);
    TileBrushChange ApplyErase(TileMap& map, int x, int y, float strength);
    TileBrushChange ApplySmooth(TileMap& map, int x, int y, float strength);
    TileBrushChange ApplyRaise(TileMap& map, int x, int y, float strength);
    TileBrushChange ApplyLower(TileMap& map, int x, int y, float strength);
    TileBrushChange ApplyFlatten(TileMap& map, int x, int y, float strength);
    TileBrushChange ApplyNoise(TileMap& map, int x, int y, float strength);

    // Noise generation
    float GenerateNoise(float x, float y) const;

    // Check if point is within brush shape
    bool IsInBrush(int centerX, int centerY, int tileX, int tileY) const;

    // Shape settings
    BrushShape m_shape = BrushShape::Circle;
    int m_size = 3;
    float m_falloff = 0.5f;
    float m_opacity = 1.0f;

    // Mode
    BrushMode m_mode = BrushMode::Paint;

    // Tile type
    TileType m_selectedTile = TileType::GroundGrass1;
    uint8_t m_selectedVariant = 0;
    bool m_wallMode = false;
    float m_wallHeight = 2.0f;

    // Elevation
    float m_elevationDelta = 0.5f;
    float m_targetElevation = 0.0f;
    bool m_absoluteElevation = false;

    // Noise
    NoiseConfig m_noiseConfig;
    bool m_randomVariants = false;

    // Random generator
    mutable std::mt19937 m_rng;

    // Callbacks
    SampleCallback m_onSample;
};

} // namespace Vehement
