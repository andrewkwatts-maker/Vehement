#pragma once

#include "../world/Tile.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>

namespace Vehement {

// Forward declarations
class TileMap;

/**
 * @brief Water body type
 */
enum class WaterBodyType : uint8_t {
    Lake,           ///< Still water lake
    River,          ///< Flowing river
    Ocean,          ///< Ocean/sea
    Pond,           ///< Small pond
    Stream,         ///< Small stream
    Swamp           ///< Swamp/marsh
};

/**
 * @brief Get display name for water body type
 */
inline const char* GetWaterBodyTypeName(WaterBodyType type) {
    switch (type) {
        case WaterBodyType::Lake:   return "Lake";
        case WaterBodyType::River:  return "River";
        case WaterBodyType::Ocean:  return "Ocean";
        case WaterBodyType::Pond:   return "Pond";
        case WaterBodyType::Stream: return "Stream";
        case WaterBodyType::Swamp:  return "Swamp";
        default:                    return "Unknown";
    }
}

/**
 * @brief Vertex in a water polygon
 */
struct WaterVertex {
    glm::vec2 position{0.0f};
    float depth = 1.0f;             ///< Water depth at this vertex
};

/**
 * @brief Water polygon definition
 */
struct WaterPolygon {
    std::string name;
    WaterBodyType type = WaterBodyType::Lake;
    std::vector<WaterVertex> vertices;
    float waterLevel = 0.0f;        ///< Y coordinate of water surface
    glm::vec2 flowDirection{0.0f};  ///< Flow direction for rivers
    float flowSpeed = 1.0f;         ///< Flow speed
    float waveAmplitude = 0.1f;     ///< Wave height
    float waveFrequency = 1.0f;     ///< Wave frequency
    glm::vec4 waterColor{0.2f, 0.4f, 0.8f, 0.7f};  ///< Water tint color
    bool isDeep = false;            ///< Deep water (dangerous)
    float damagePerSecond = 0.0f;   ///< Damage if deep/hazardous
};

/**
 * @brief Shore blend settings
 */
struct ShoreBlendSettings {
    int blendWidth = 2;             ///< Width of shore blend in tiles
    TileType shoreType = TileType::GroundDirt;  ///< Shore tile type
    bool useGradient = true;        ///< Gradual transition
    float wetnessDecay = 0.3f;      ///< How quickly wetness fades from shore
};

/**
 * @brief Water body editor mode
 */
enum class WaterEditMode : uint8_t {
    DrawPolygon,    ///< Draw water polygon outline
    AdjustLevel,    ///< Adjust water level
    SetFlow,        ///< Set river flow direction
    EditShore       ///< Edit shore blending
};

/**
 * @brief Water body editor for world editing
 *
 * Features:
 * - Define water polygons by clicking vertices
 * - Water level adjustment
 * - River flow direction setting
 * - Shore blending for natural transitions
 * - Multiple water body types
 *
 * Usage:
 * 1. Select water body type
 * 2. Click to define polygon vertices
 * 3. Adjust water level and properties
 * 4. Configure shore blending
 * 5. Apply to tile map
 */
class WaterEditor {
public:
    WaterEditor();
    ~WaterEditor() = default;

    // Allow copy and move
    WaterEditor(const WaterEditor&) = default;
    WaterEditor& operator=(const WaterEditor&) = default;
    WaterEditor(WaterEditor&&) noexcept = default;
    WaterEditor& operator=(WaterEditor&&) noexcept = default;

    // =========================================================================
    // Edit Mode
    // =========================================================================

    /** @brief Get current edit mode */
    [[nodiscard]] WaterEditMode GetMode() const noexcept { return m_mode; }

    /** @brief Set edit mode */
    void SetMode(WaterEditMode mode) noexcept { m_mode = mode; }

    // =========================================================================
    // Water Body Type
    // =========================================================================

    /** @brief Get selected water body type */
    [[nodiscard]] WaterBodyType GetWaterType() const noexcept { return m_waterType; }

    /** @brief Set water body type */
    void SetWaterType(WaterBodyType type) noexcept { m_waterType = type; }

    // =========================================================================
    // Polygon Drawing
    // =========================================================================

    /**
     * @brief Start new water polygon
     * @param name Name for the water body
     */
    void BeginPolygon(const std::string& name);

    /**
     * @brief Add vertex to current polygon
     * @param position World position
     * @param depth Water depth at vertex
     */
    void AddVertex(const glm::vec2& position, float depth = 1.0f);

    /**
     * @brief Remove last vertex
     */
    void RemoveLastVertex();

    /**
     * @brief Finish current polygon
     * @return true if polygon is valid (3+ vertices)
     */
    bool FinishPolygon();

    /**
     * @brief Cancel current polygon
     */
    void CancelPolygon();

    /**
     * @brief Check if currently drawing a polygon
     */
    [[nodiscard]] bool IsDrawing() const noexcept { return m_isDrawing; }

    /**
     * @brief Get current polygon being edited
     */
    [[nodiscard]] const WaterPolygon& GetCurrentPolygon() const noexcept {
        return m_currentPolygon;
    }

    /** @brief Get mutable current polygon */
    [[nodiscard]] WaterPolygon& GetCurrentPolygon() noexcept { return m_currentPolygon; }

    // =========================================================================
    // Water Level
    // =========================================================================

    /** @brief Get default water level */
    [[nodiscard]] float GetWaterLevel() const noexcept { return m_waterLevel; }

    /** @brief Set default water level */
    void SetWaterLevel(float level) noexcept { m_waterLevel = level; }

    /**
     * @brief Adjust water level for a specific polygon
     * @param index Polygon index
     * @param level New water level
     */
    void SetPolygonWaterLevel(int index, float level);

    // =========================================================================
    // River Flow
    // =========================================================================

    /** @brief Get flow direction */
    [[nodiscard]] const glm::vec2& GetFlowDirection() const noexcept { return m_flowDirection; }

    /** @brief Set flow direction */
    void SetFlowDirection(const glm::vec2& direction);

    /** @brief Set flow direction from two points */
    void SetFlowFromPoints(const glm::vec2& from, const glm::vec2& to);

    /** @brief Get flow speed */
    [[nodiscard]] float GetFlowSpeed() const noexcept { return m_flowSpeed; }

    /** @brief Set flow speed */
    void SetFlowSpeed(float speed) noexcept { m_flowSpeed = std::max(0.0f, speed); }

    // =========================================================================
    // Shore Blending
    // =========================================================================

    /** @brief Get shore blend settings */
    [[nodiscard]] const ShoreBlendSettings& GetShoreSettings() const noexcept {
        return m_shoreSettings;
    }

    /** @brief Get mutable shore blend settings */
    [[nodiscard]] ShoreBlendSettings& GetShoreSettings() noexcept { return m_shoreSettings; }

    /** @brief Set shore blend settings */
    void SetShoreSettings(const ShoreBlendSettings& settings) noexcept {
        m_shoreSettings = settings;
    }

    // =========================================================================
    // Water Bodies Management
    // =========================================================================

    /** @brief Get all water bodies */
    [[nodiscard]] const std::vector<WaterPolygon>& GetWaterBodies() const noexcept {
        return m_waterBodies;
    }

    /** @brief Delete a water body by index */
    bool DeleteWaterBody(int index);

    /** @brief Clear all water bodies */
    void ClearAllWaterBodies();

    /** @brief Select a water body for editing */
    void SelectWaterBody(int index);

    /** @brief Get selected water body index */
    [[nodiscard]] int GetSelectedIndex() const noexcept { return m_selectedIndex; }

    // =========================================================================
    // Apply to Map
    // =========================================================================

    /**
     * @brief Apply all water bodies to tile map
     * @param map Tile map to modify
     * @return Vector of tile changes
     */
    std::vector<std::pair<glm::ivec2, TileType>> ApplyToMap(TileMap& map);

    /**
     * @brief Get tiles inside a water polygon
     */
    [[nodiscard]] std::vector<glm::ivec2> GetWaterTiles(const WaterPolygon& polygon) const;

    /**
     * @brief Get shore tiles for a water polygon
     */
    [[nodiscard]] std::vector<glm::ivec2> GetShoreTiles(const WaterPolygon& polygon) const;

    /**
     * @brief Check if a point is inside a water polygon
     */
    [[nodiscard]] bool IsPointInWater(const glm::vec2& point) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /** @brief Save water bodies to JSON */
    [[nodiscard]] std::string ToJson() const;

    /** @brief Load water bodies from JSON */
    bool FromJson(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using WaterCallback = std::function<void(const WaterPolygon&)>;

    void SetOnWaterBodyCreated(WaterCallback callback) {
        m_onWaterBodyCreated = std::move(callback);
    }
    void SetOnWaterBodyModified(WaterCallback callback) {
        m_onWaterBodyModified = std::move(callback);
    }

private:
    bool IsPointInPolygon(const glm::vec2& point, const std::vector<WaterVertex>& vertices) const;
    std::vector<glm::ivec2> RasterizePolygon(const std::vector<WaterVertex>& vertices) const;

    // Mode
    WaterEditMode m_mode = WaterEditMode::DrawPolygon;
    WaterBodyType m_waterType = WaterBodyType::Lake;

    // Current polygon
    WaterPolygon m_currentPolygon;
    bool m_isDrawing = false;

    // Water settings
    float m_waterLevel = 0.0f;
    glm::vec2 m_flowDirection{0.0f, 1.0f};
    float m_flowSpeed = 1.0f;

    // Shore settings
    ShoreBlendSettings m_shoreSettings;

    // All water bodies
    std::vector<WaterPolygon> m_waterBodies;
    int m_selectedIndex = -1;

    // Callbacks
    WaterCallback m_onWaterBodyCreated;
    WaterCallback m_onWaterBodyModified;
};

} // namespace Vehement
