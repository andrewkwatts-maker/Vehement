#pragma once

#include "../world/Tile.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace Vehement {

// Forward declarations
class TileMap;

/**
 * @brief Road type enumeration
 */
enum class RoadType : uint8_t {
    Dirt,           ///< Unpaved dirt road
    Gravel,         ///< Gravel road
    Asphalt,        ///< Asphalt road
    Concrete,       ///< Concrete road
    Cobblestone,    ///< Cobblestone road
    Highway         ///< Multi-lane highway
};

/**
 * @brief Get display name for road type
 */
inline const char* GetRoadTypeName(RoadType type) {
    switch (type) {
        case RoadType::Dirt:        return "Dirt Road";
        case RoadType::Gravel:      return "Gravel Road";
        case RoadType::Asphalt:     return "Asphalt Road";
        case RoadType::Concrete:    return "Concrete Road";
        case RoadType::Cobblestone: return "Cobblestone Road";
        case RoadType::Highway:     return "Highway";
        default:                    return "Unknown";
    }
}

/**
 * @brief Get tile type for a road type
 */
inline TileType GetRoadTileType(RoadType type) {
    switch (type) {
        case RoadType::Dirt:        return TileType::GroundDirt;
        case RoadType::Gravel:      return TileType::GroundRocks;
        case RoadType::Asphalt:     return TileType::ConcreteAsphalt1;
        case RoadType::Concrete:    return TileType::ConcreteTiles1;
        case RoadType::Cobblestone: return TileType::StoneMarble1;
        case RoadType::Highway:     return TileType::ConcreteAsphalt2;
        default:                    return TileType::ConcreteAsphalt1;
    }
}

/**
 * @brief Road segment in a path
 */
struct RoadSegment {
    glm::vec2 start{0.0f};
    glm::vec2 end{0.0f};
    RoadType type = RoadType::Asphalt;
    int width = 2;
    bool isBridge = false;
    bool isTunnel = false;
    float elevation = 0.0f;
};

/**
 * @brief Road intersection data
 */
struct RoadIntersection {
    glm::ivec2 position{0, 0};
    std::vector<int> connectedSegments;     ///< Indices of connected segments
    bool hasTrafficLight = false;
    bool isRoundabout = false;
};

/**
 * @brief Complete road network
 */
struct RoadNetwork {
    std::vector<RoadSegment> segments;
    std::vector<RoadIntersection> intersections;
    std::string name;
};

/**
 * @brief Road editing mode
 */
enum class RoadEditMode : uint8_t {
    Draw,           ///< Draw new road segments
    Erase,          ///< Erase existing roads
    Modify,         ///< Modify road properties
    Connect         ///< Auto-connect intersections
};

/**
 * @brief Road network editor for world editing
 *
 * Features:
 * - Draw road paths point-by-point
 * - Road type selection
 * - Auto-connect intersections
 * - Bridge and tunnel modes
 * - Road width adjustment
 * - Edge smoothing
 *
 * Usage:
 * 1. Select road type and width
 * 2. Click to add road points
 * 3. System auto-connects intersections
 * 4. Apply to tile map
 */
class RoadEditor {
public:
    RoadEditor();
    ~RoadEditor() = default;

    // Allow copy and move
    RoadEditor(const RoadEditor&) = default;
    RoadEditor& operator=(const RoadEditor&) = default;
    RoadEditor(RoadEditor&&) noexcept = default;
    RoadEditor& operator=(RoadEditor&&) noexcept = default;

    // =========================================================================
    // Edit Mode
    // =========================================================================

    /** @brief Get current edit mode */
    [[nodiscard]] RoadEditMode GetMode() const noexcept { return m_mode; }

    /** @brief Set edit mode */
    void SetMode(RoadEditMode mode) noexcept { m_mode = mode; }

    // =========================================================================
    // Road Type Settings
    // =========================================================================

    /** @brief Get selected road type */
    [[nodiscard]] RoadType GetRoadType() const noexcept { return m_roadType; }

    /** @brief Set road type */
    void SetRoadType(RoadType type) noexcept { m_roadType = type; }

    /** @brief Get road width */
    [[nodiscard]] int GetWidth() const noexcept { return m_width; }

    /** @brief Set road width (1-6 tiles) */
    void SetWidth(int width) noexcept { m_width = std::clamp(width, 1, 6); }

    // =========================================================================
    // Bridge/Tunnel Mode
    // =========================================================================

    /** @brief Check if bridge mode is enabled */
    [[nodiscard]] bool IsBridgeMode() const noexcept { return m_bridgeMode; }

    /** @brief Set bridge mode */
    void SetBridgeMode(bool bridge) noexcept { m_bridgeMode = bridge; m_tunnelMode = false; }

    /** @brief Check if tunnel mode is enabled */
    [[nodiscard]] bool IsTunnelMode() const noexcept { return m_tunnelMode; }

    /** @brief Set tunnel mode */
    void SetTunnelMode(bool tunnel) noexcept { m_tunnelMode = tunnel; m_bridgeMode = false; }

    /** @brief Get bridge/tunnel elevation */
    [[nodiscard]] float GetElevation() const noexcept { return m_elevation; }

    /** @brief Set bridge/tunnel elevation */
    void SetElevation(float elevation) noexcept { m_elevation = elevation; }

    // =========================================================================
    // Drawing
    // =========================================================================

    /**
     * @brief Start drawing a new road path
     * @param startPos Starting tile position
     */
    void BeginPath(const glm::ivec2& startPos);

    /**
     * @brief Add point to current road path
     * @param pos Tile position
     */
    void AddPoint(const glm::ivec2& pos);

    /**
     * @brief Finish current road path
     */
    void EndPath();

    /**
     * @brief Cancel current road path
     */
    void CancelPath();

    /**
     * @brief Check if currently drawing a path
     */
    [[nodiscard]] bool IsDrawing() const noexcept { return m_isDrawing; }

    /**
     * @brief Get current path points
     */
    [[nodiscard]] const std::vector<glm::ivec2>& GetCurrentPath() const noexcept {
        return m_currentPath;
    }

    // =========================================================================
    // Road Network
    // =========================================================================

    /**
     * @brief Get current road network
     */
    [[nodiscard]] const RoadNetwork& GetRoadNetwork() const noexcept { return m_network; }

    /**
     * @brief Clear road network
     */
    void ClearNetwork();

    /**
     * @brief Auto-connect nearby road endpoints
     * @param maxDistance Maximum distance to auto-connect
     * @return Number of connections made
     */
    int AutoConnectIntersections(float maxDistance = 3.0f);

    /**
     * @brief Find intersections in the road network
     * @return Vector of intersection positions
     */
    [[nodiscard]] std::vector<glm::ivec2> FindIntersections() const;

    // =========================================================================
    // Apply to Map
    // =========================================================================

    /**
     * @brief Apply road network to tile map
     * @param map Tile map to modify
     * @return Vector of tile changes
     */
    std::vector<std::pair<glm::ivec2, TileType>> ApplyToMap(TileMap& map);

    /**
     * @brief Get tiles that would be modified by current road
     */
    [[nodiscard]] std::vector<glm::ivec2> GetAffectedTiles() const;

    /**
     * @brief Get tiles for a road segment
     */
    [[nodiscard]] std::vector<glm::ivec2> GetSegmentTiles(const RoadSegment& segment) const;

    // =========================================================================
    // Erase
    // =========================================================================

    /**
     * @brief Erase road at position
     * @param pos Tile position
     * @return true if road was erased
     */
    bool EraseRoadAt(const glm::ivec2& pos);

    /**
     * @brief Erase road segment by index
     * @param index Segment index
     * @return true if segment was erased
     */
    bool EraseSegment(int index);

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Save road network to JSON
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Load road network from JSON
     */
    bool FromJson(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using RoadCallback = std::function<void(const RoadSegment&)>;

    void SetOnRoadCreated(RoadCallback callback) { m_onRoadCreated = std::move(callback); }
    void SetOnRoadErased(RoadCallback callback) { m_onRoadErased = std::move(callback); }

private:
    void CreateSegmentFromPath();
    std::vector<glm::ivec2> GetLineTiles(const glm::ivec2& start, const glm::ivec2& end, int width) const;
    bool IsNearRoad(const glm::ivec2& pos, float distance) const;

    // Mode
    RoadEditMode m_mode = RoadEditMode::Draw;

    // Road settings
    RoadType m_roadType = RoadType::Asphalt;
    int m_width = 2;
    bool m_bridgeMode = false;
    bool m_tunnelMode = false;
    float m_elevation = 2.0f;

    // Drawing state
    bool m_isDrawing = false;
    std::vector<glm::ivec2> m_currentPath;

    // Road network
    RoadNetwork m_network;

    // Callbacks
    RoadCallback m_onRoadCreated;
    RoadCallback m_onRoadErased;
};

} // namespace Vehement
