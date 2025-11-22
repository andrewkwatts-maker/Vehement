#pragma once

#include "SessionFogOfWar.hpp"
#include "VisionSource.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Vehement2 {
namespace RTS {

/**
 * @brief Minimap marker types
 */
enum class MinimapMarkerType : uint8_t {
    None = 0,
    Player,             // Local player position
    AllyUnit,           // Friendly units
    AllyBuilding,       // Friendly buildings
    EnemyUnit,          // Hostile units (only in vision)
    EnemyBuilding,      // Hostile buildings (only in vision)
    ResourceNode,       // Resource deposits
    Discovery,          // Points of interest
    Objective,          // Mission objectives
    Ping,               // Player ping/waypoint
    Alert               // Alert/warning
};

/**
 * @brief Minimap marker data
 */
struct MinimapMarker {
    MinimapMarkerType type = MinimapMarkerType::None;
    glm::vec2 worldPosition{0.0f};
    glm::vec2 minimapPosition{0.0f};        // Calculated minimap position
    glm::vec4 color{1.0f};                  // RGBA color
    float size = 4.0f;                      // Marker size in pixels
    float rotation = 0.0f;                  // Rotation angle (for directional markers)
    bool visible = true;                    // Currently visible on minimap
    bool pulsing = false;                   // Animated pulsing effect
    float pulseTimer = 0.0f;
    float lifetime = -1.0f;                 // Marker lifetime (-1 = permanent)
    uint32_t entityId = 0;                  // Associated entity ID
};

/**
 * @brief Configuration for minimap rendering
 */
struct MinimapConfig {
    // Size and position
    int width = 200;                        // Minimap width in pixels
    int height = 200;                       // Minimap height in pixels
    glm::vec2 screenPosition{20.0f, 20.0f}; // Screen position (bottom-left corner)

    // Appearance
    glm::vec4 backgroundColor{0.1f, 0.1f, 0.12f, 0.9f};
    glm::vec4 borderColor{0.3f, 0.3f, 0.35f, 1.0f};
    float borderWidth = 2.0f;
    float cornerRadius = 4.0f;

    // Fog colors
    glm::vec4 unknownColor{0.0f, 0.0f, 0.0f, 1.0f};         // Black for unknown
    glm::vec4 exploredColor{0.2f, 0.22f, 0.25f, 1.0f};      // Dark gray for explored
    glm::vec4 visibleColor{0.4f, 0.45f, 0.5f, 1.0f};        // Lighter for visible terrain

    // Terrain colors
    glm::vec4 terrainGround{0.3f, 0.35f, 0.25f, 1.0f};      // Ground/grass
    glm::vec4 terrainWater{0.2f, 0.3f, 0.5f, 1.0f};         // Water
    glm::vec4 terrainWall{0.15f, 0.15f, 0.18f, 1.0f};       // Walls/obstacles
    glm::vec4 terrainRoad{0.35f, 0.32f, 0.28f, 1.0f};       // Roads

    // Marker colors
    glm::vec4 playerColor{0.2f, 0.6f, 1.0f, 1.0f};          // Blue
    glm::vec4 allyUnitColor{0.2f, 0.8f, 0.3f, 1.0f};        // Green
    glm::vec4 allyBuildingColor{0.3f, 0.9f, 0.4f, 1.0f};    // Bright green
    glm::vec4 enemyUnitColor{1.0f, 0.3f, 0.2f, 1.0f};       // Red
    glm::vec4 enemyBuildingColor{0.9f, 0.2f, 0.15f, 1.0f};  // Dark red
    glm::vec4 resourceColor{1.0f, 0.85f, 0.2f, 1.0f};       // Gold
    glm::vec4 discoveryColor{0.9f, 0.5f, 1.0f, 1.0f};       // Purple
    glm::vec4 objectiveColor{1.0f, 1.0f, 0.3f, 1.0f};       // Yellow
    glm::vec4 pingColor{1.0f, 1.0f, 1.0f, 1.0f};            // White
    glm::vec4 alertColor{1.0f, 0.5f, 0.0f, 1.0f};           // Orange

    // Marker sizes
    float playerSize = 6.0f;
    float unitSize = 3.0f;
    float buildingSize = 5.0f;
    float resourceSize = 4.0f;
    float discoverySize = 3.0f;

    // Behavior
    bool showPlayerBuildings = true;        // Always show player's buildings
    bool showAlliedUnits = true;            // Show allied units
    bool showEnemiesInVision = true;        // Show enemies in visible areas
    bool showDiscoveries = true;            // Show discovered POIs
    bool showObjectives = true;             // Show mission objectives
    bool enablePinging = true;              // Allow player pings
    float pingDuration = 5.0f;              // How long pings last

    // Camera/viewport indicator
    bool showCameraViewport = true;
    glm::vec4 viewportColor{1.0f, 1.0f, 1.0f, 0.3f};
    float viewportLineWidth = 1.0f;
};

/**
 * @brief Minimap renderer with fog of war integration
 *
 * Renders a minimap that respects the fog of war state:
 * - Unknown areas are completely black
 * - Explored areas show terrain but not units
 * - Visible areas show everything (terrain + units)
 *
 * The minimap also shows:
 * - Player's own buildings (always visible)
 * - Allied units and buildings
 * - Enemy units/buildings (only if in visible area)
 * - Discovered resources and POIs
 * - Mission objectives
 * - Player pings and alerts
 */
class MinimapReveal {
public:
    MinimapReveal();
    ~MinimapReveal();

    // Non-copyable
    MinimapReveal(const MinimapReveal&) = delete;
    MinimapReveal& operator=(const MinimapReveal&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the minimap renderer
     * @param fogOfWar Reference to fog of war system
     * @param mapWidth World map width in tiles
     * @param mapHeight World map height in tiles
     * @param tileSize Size of each tile in world units
     * @return true if initialization succeeded
     */
    bool Initialize(SessionFogOfWar* fogOfWar, int mapWidth, int mapHeight, float tileSize);

    /**
     * @brief Shutdown and release resources
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update minimap state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the minimap
     *
     * Should be called after main scene rendering, in the UI pass.
     */
    void Render();

    /**
     * @brief Get the minimap texture for external rendering
     * @return OpenGL texture ID
     */
    uint32_t GetMinimapTexture() const { return m_minimapTexture; }

    // =========================================================================
    // World Data
    // =========================================================================

    /**
     * @brief Set terrain data for minimap coloring
     * @param terrainTypes Array of terrain type per tile (0=ground, 1=water, 2=wall, 3=road)
     * @param width Terrain data width
     * @param height Terrain data height
     */
    void SetTerrainData(const uint8_t* terrainTypes, int width, int height);

    /**
     * @brief Set camera position for viewport indicator
     * @param position Camera center in world coordinates
     * @param viewSize Size of visible area
     */
    void SetCameraPosition(const glm::vec2& position, const glm::vec2& viewSize);

    // =========================================================================
    // Markers
    // =========================================================================

    /**
     * @brief Add a marker to the minimap
     * @param marker Marker data
     * @return Marker index
     */
    size_t AddMarker(const MinimapMarker& marker);

    /**
     * @brief Update a marker's position
     * @param entityId Entity ID of the marker
     * @param worldPosition New world position
     */
    void UpdateMarkerPosition(uint32_t entityId, const glm::vec2& worldPosition);

    /**
     * @brief Remove a marker by entity ID
     */
    void RemoveMarker(uint32_t entityId);

    /**
     * @brief Remove all markers of a type
     */
    void RemoveMarkersOfType(MinimapMarkerType type);

    /**
     * @brief Clear all markers
     */
    void ClearMarkers();

    /**
     * @brief Add a ping at a world position
     * @param worldPosition World position to ping
     */
    void AddPing(const glm::vec2& worldPosition);

    /**
     * @brief Add an alert marker
     * @param worldPosition World position
     * @param duration How long to show alert
     */
    void AddAlert(const glm::vec2& worldPosition, float duration = 3.0f);

    // =========================================================================
    // Visibility Helpers
    // =========================================================================

    /**
     * @brief Check if a world position should show on minimap
     * @param worldPosition Position to check
     * @param requireVisible If true, position must be in vision (not just explored)
     * @return true if position should be shown
     */
    bool ShouldShowOnMinimap(const glm::vec2& worldPosition, bool requireVisible = false) const;

    /**
     * @brief Convert world position to minimap position
     */
    glm::vec2 WorldToMinimap(const glm::vec2& worldPosition) const;

    /**
     * @brief Convert minimap position to world position
     */
    glm::vec2 MinimapToWorld(const glm::vec2& minimapPosition) const;

    /**
     * @brief Check if a screen position is over the minimap
     */
    bool IsPointOverMinimap(const glm::vec2& screenPosition) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set configuration
     */
    void SetConfig(const MinimapConfig& config);

    /**
     * @brief Get configuration
     */
    const MinimapConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set minimap position on screen
     */
    void SetPosition(const glm::vec2& position) { m_config.screenPosition = position; }

    /**
     * @brief Set minimap size
     */
    void SetSize(int width, int height);

    // =========================================================================
    // Player Interaction
    // =========================================================================

    /**
     * @brief Handle click on minimap
     * @param screenPosition Click position in screen coordinates
     * @return World position of click, or (-1, -1) if not on minimap
     */
    glm::vec2 HandleClick(const glm::vec2& screenPosition) const;

    /**
     * @brief Check if click was on minimap
     */
    bool WasClickOnMinimap(const glm::vec2& screenPosition) const;

private:
    // Internal methods
    bool CreateResources();
    void DestroyResources();
    void UpdateMinimapTexture();
    void RenderFogLayer();
    void RenderTerrainLayer();
    void RenderMarkers();
    void RenderViewport();
    void UpdateMarkers(float deltaTime);

    glm::vec4 GetMarkerColor(MinimapMarkerType type) const;
    float GetMarkerSize(MinimapMarkerType type) const;

    // References
    SessionFogOfWar* m_fogOfWar = nullptr;

    // Configuration
    MinimapConfig m_config;

    // State
    bool m_initialized = false;

    // Map dimensions
    int m_mapWidth = 0;
    int m_mapHeight = 0;
    float m_tileSize = 1.0f;

    // Terrain data
    std::vector<uint8_t> m_terrainData;

    // Camera state
    glm::vec2 m_cameraPosition{0.0f};
    glm::vec2 m_cameraViewSize{0.0f};

    // Markers
    std::vector<MinimapMarker> m_markers;

    // GPU resources
    uint32_t m_minimapTexture = 0;          // Final composited minimap
    uint32_t m_fogTexture = 0;              // Fog state at minimap resolution
    uint32_t m_terrainTexture = 0;          // Terrain colors at minimap resolution
    uint32_t m_minimapFBO = 0;              // Framebuffer for minimap rendering
    uint32_t m_minimapShader = 0;           // Shader for compositing
    uint32_t m_markerShader = 0;            // Shader for marker rendering
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;

    // Coordinate conversion cache
    float m_worldToMinimapScale = 1.0f;
};

/**
 * @brief Static helper functions for minimap integration
 */
class MinimapHelpers {
public:
    /**
     * @brief Create a standard minimap marker for a unit
     */
    static MinimapMarker CreateUnitMarker(uint32_t entityId, const glm::vec2& position,
                                           bool isAlly, bool isPlayer = false) {
        MinimapMarker marker;
        marker.entityId = entityId;
        marker.worldPosition = position;

        if (isPlayer) {
            marker.type = MinimapMarkerType::Player;
        } else if (isAlly) {
            marker.type = MinimapMarkerType::AllyUnit;
        } else {
            marker.type = MinimapMarkerType::EnemyUnit;
        }

        return marker;
    }

    /**
     * @brief Create a marker for a building
     */
    static MinimapMarker CreateBuildingMarker(uint32_t entityId, const glm::vec2& position,
                                               bool isAlly) {
        MinimapMarker marker;
        marker.entityId = entityId;
        marker.worldPosition = position;
        marker.type = isAlly ? MinimapMarkerType::AllyBuilding : MinimapMarkerType::EnemyBuilding;
        return marker;
    }

    /**
     * @brief Create a marker for a resource
     */
    static MinimapMarker CreateResourceMarker(uint32_t resourceId, const glm::vec2& position) {
        MinimapMarker marker;
        marker.entityId = resourceId;
        marker.worldPosition = position;
        marker.type = MinimapMarkerType::ResourceNode;
        return marker;
    }

    /**
     * @brief Create a marker for a discovery/POI
     */
    static MinimapMarker CreateDiscoveryMarker(uint32_t discoveryId, const glm::vec2& position) {
        MinimapMarker marker;
        marker.entityId = discoveryId;
        marker.worldPosition = position;
        marker.type = MinimapMarkerType::Discovery;
        marker.pulsing = true;  // Discoveries pulse to attract attention
        return marker;
    }

    /**
     * @brief Create a marker for a mission objective
     */
    static MinimapMarker CreateObjectiveMarker(uint32_t objectiveId, const glm::vec2& position) {
        MinimapMarker marker;
        marker.entityId = objectiveId;
        marker.worldPosition = position;
        marker.type = MinimapMarkerType::Objective;
        marker.pulsing = true;
        marker.size = 8.0f;
        return marker;
    }
};

} // namespace RTS
} // namespace Vehement2
