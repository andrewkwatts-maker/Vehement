#pragma once

#include "VisionSource.hpp"
#include "../world/RadianceCascades.hpp"

#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <chrono>
#include <glm/glm.hpp>

namespace Vehement2 {
namespace RTS {

// Forward declarations
class Exploration;
class SessionManager;

/**
 * @brief Fog state for each tile - resets each session
 *
 * Unlike persistent fog of war, session fog resets when:
 * - Player disconnects
 * - Inactivity timeout (configurable)
 * - New game session starts
 *
 * This creates tension and encourages re-exploration each play session.
 */
enum class FogState : uint8_t {
    Unknown = 0,    // Never seen this session (completely black)
    Explored = 1,   // Seen but not currently visible (dark, shows terrain only)
    Visible = 2     // Currently in vision range (fully lit, shows everything)
};

/**
 * @brief Configuration for session-based fog of war
 */
struct SessionFogConfig {
    // Visual settings
    float unknownBrightness = 0.0f;         // Brightness of unknown areas (black)
    float exploredBrightness = 0.25f;       // Brightness of explored areas
    float visibleBrightness = 1.0f;         // Brightness of visible areas
    float transitionSpeed = 6.0f;           // Fog transition speed

    // Session settings
    float inactivityTimeout = 30.0f * 60.0f;    // 30 minutes before session reset
    bool resetOnDisconnect = true;              // Reset fog when player disconnects
    bool persistExploredOnDeath = false;        // Keep explored state on death

    // Vision settings
    float minimumVisionRadius = 2.0f;       // Minimum vision even in worst conditions
    bool enableLineOfSight = true;          // Use raycasting for vision blocking
    bool enableHeightAdvantage = true;      // Higher units see further

    // Rendering
    glm::vec3 fogColor = glm::vec3(0.0f);                   // Unknown area color
    glm::vec3 exploredTint = glm::vec3(0.4f, 0.45f, 0.6f);  // Explored area tint
    bool smoothEdges = true;                                 // Smooth fog transitions
};

/**
 * @brief Callback types for fog events
 */
using TileRevealedCallback = std::function<void(const glm::ivec2& tile, FogState previousState)>;
using AreaExploredCallback = std::function<void(float newExplorationPercent)>;
using SessionResetCallback = std::function<void()>;

/**
 * @brief Session-based fog of war system
 *
 * This fog of war system resets each game session, creating a fresh exploration
 * experience every time the player starts playing. Key features:
 *
 * - Three-state fog: Unknown, Explored, Visible
 * - Multiple vision sources (hero, workers, buildings, towers)
 * - Day/night and weather effects on vision
 * - Line-of-sight blocking by terrain
 * - Session reset on disconnect/timeout
 * - Integration with Radiance Cascades for rendering
 *
 * Usage:
 * 1. Initialize with map dimensions
 * 2. Add vision sources (hero, workers, buildings)
 * 3. Call Update() each frame
 * 4. Use GetFogTexture() for rendering
 * 5. Session automatically resets based on configuration
 */
class SessionFogOfWar {
public:
    SessionFogOfWar();
    ~SessionFogOfWar();

    // Disable copy, enable move
    SessionFogOfWar(const SessionFogOfWar&) = delete;
    SessionFogOfWar& operator=(const SessionFogOfWar&) = delete;
    SessionFogOfWar(SessionFogOfWar&& other) noexcept;
    SessionFogOfWar& operator=(SessionFogOfWar&& other) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the fog of war system
     * @param mapWidth Map width in tiles
     * @param mapHeight Map height in tiles
     * @param tileSize Size of each tile in world units
     * @param screenWidth Screen width for rendering
     * @param screenHeight Screen height for rendering
     * @return true if initialization succeeded
     */
    bool Initialize(int mapWidth, int mapHeight, float tileSize,
                    int screenWidth, int screenHeight);

    /**
     * @brief Shutdown and release resources
     */
    void Shutdown();

    /**
     * @brief Resize screen textures
     */
    void Resize(int screenWidth, int screenHeight);

    /**
     * @brief Check if system is initialized
     */
    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Session Management
    // =========================================================================

    /**
     * @brief Reset fog of war for a new session
     *
     * Clears all exploration progress and starts fresh.
     * Called automatically on:
     * - New game session
     * - Player disconnect (if configured)
     * - Inactivity timeout
     */
    void ResetFogOfWar();

    /**
     * @brief Record player activity (resets inactivity timer)
     */
    void RecordActivity();

    /**
     * @brief Check if session has expired due to inactivity
     */
    bool IsSessionExpired() const;

    /**
     * @brief Get time until session expires (seconds)
     */
    float GetTimeUntilExpiry() const;

    /**
     * @brief Handle player disconnect event
     */
    void OnPlayerDisconnect();

    /**
     * @brief Handle player reconnect event
     */
    void OnPlayerReconnect();

    // =========================================================================
    // Vision Updates
    // =========================================================================

    /**
     * @brief Update vision based on all vision sources
     * @param sources Vector of all vision sources
     * @param environment Current environmental conditions
     * @param deltaTime Time since last update
     */
    void UpdateVision(const std::vector<VisionSource>& sources,
                      const VisionEnvironment& environment,
                      float deltaTime);

    /**
     * @brief Update vision from a single source
     */
    void UpdateVisionFromSource(const VisionSource& source,
                                const VisionEnvironment& environment);

    /**
     * @brief Set the radiance cascades system for visibility calculation
     */
    void SetRadianceCascades(RadianceCascades* cascades);

    /**
     * @brief Set occlusion data for line-of-sight calculations
     * @param occlusionData Array of blocked tiles (1 = blocked, 0 = open)
     * @param width Occlusion map width
     * @param height Occlusion map height
     */
    void SetOcclusionData(const uint8_t* occlusionData, int width, int height);

    // =========================================================================
    // Visibility Queries
    // =========================================================================

    /**
     * @brief Check if a tile is currently visible
     * @param tile Tile coordinates
     * @return true if tile is in vision range
     */
    bool IsVisible(const glm::ivec2& tile) const;

    /**
     * @brief Check if a tile has been explored this session
     * @param tile Tile coordinates
     * @return true if tile has been seen at least once
     */
    bool IsExplored(const glm::ivec2& tile) const;

    /**
     * @brief Get fog state at a specific tile
     */
    FogState GetFogState(const glm::ivec2& tile) const;

    /**
     * @brief Get fog state at world position
     */
    FogState GetFogStateAtPosition(const glm::vec2& worldPos) const;

    /**
     * @brief Check if there's line of sight between two points
     * @param from Start position
     * @param to End position
     * @return true if line of sight is clear
     */
    bool HasLineOfSight(const glm::vec2& from, const glm::vec2& to) const;

    /**
     * @brief Check if a unit at a position can be seen
     * @param position Unit position
     * @param isHidden Whether the unit is using stealth
     * @param checkingSources Vision sources to check against
     * @return true if unit is visible
     */
    bool CanSeeUnit(const glm::vec2& position, bool isHidden,
                    const std::vector<VisionSource>& checkingSources) const;

    // =========================================================================
    // Manual Reveal
    // =========================================================================

    /**
     * @brief Manually reveal a single tile
     */
    void RevealTile(const glm::ivec2& tile);

    /**
     * @brief Reveal a circular area
     * @param center Center of reveal
     * @param radius Radius in tiles
     */
    void RevealArea(const glm::vec2& center, float radius);

    /**
     * @brief Reveal the entire map (debug/cheat)
     */
    void RevealAll();

    /**
     * @brief Hide a specific tile (mark as unknown)
     */
    void HideTile(const glm::ivec2& tile);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Get fog texture for rendering
     * @return OpenGL texture ID (R channel = fog factor)
     */
    uint32_t GetFogTexture() const { return m_fogTexture; }

    /**
     * @brief Get combined fog + lighting texture
     * @return OpenGL texture ID (RGBA)
     */
    uint32_t GetCombinedTexture() const { return m_combinedTexture; }

    /**
     * @brief Get the explored state texture
     * @return OpenGL texture ID (R channel = explored flag)
     */
    uint32_t GetExploredTexture() const { return m_exploredTexture; }

    /**
     * @brief Update fog rendering (call after UpdateVision)
     */
    void UpdateRendering(float deltaTime);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get percentage of map explored (0-100)
     */
    float GetExplorationPercent() const;

    /**
     * @brief Get number of tiles explored this session
     */
    int GetTilesExplored() const;

    /**
     * @brief Get number of tiles currently visible
     */
    int GetTilesVisible() const;

    /**
     * @brief Get total number of tiles
     */
    int GetTotalTiles() const { return m_mapWidth * m_mapHeight; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for when a tile is revealed
     */
    void SetTileRevealedCallback(TileRevealedCallback callback) {
        m_onTileRevealed = std::move(callback);
    }

    /**
     * @brief Set callback for exploration progress changes
     */
    void SetAreaExploredCallback(AreaExploredCallback callback) {
        m_onAreaExplored = std::move(callback);
    }

    /**
     * @brief Set callback for session reset
     */
    void SetSessionResetCallback(SessionResetCallback callback) {
        m_onSessionReset = std::move(callback);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set configuration
     */
    void SetConfig(const SessionFogConfig& config) { m_config = config; }

    /**
     * @brief Get current configuration
     */
    const SessionFogConfig& GetConfig() const { return m_config; }

    // =========================================================================
    // Map Information
    // =========================================================================

    int GetMapWidth() const { return m_mapWidth; }
    int GetMapHeight() const { return m_mapHeight; }
    float GetTileSize() const { return m_tileSize; }

private:
    // Internal methods
    bool CreateTextures();
    bool CreateShaders();
    void DestroyResources();

    void UpdateVisibilityState(const std::vector<VisionSource>& sources,
                               const VisionEnvironment& environment);
    void UpdateExploredState();
    void UpdateFogTexture(float deltaTime);
    void UpdateCombinedTexture();

    void CalculateVisionForSource(const VisionSource& source,
                                  const VisionEnvironment& environment,
                                  std::vector<uint8_t>& visibilityBuffer);

    bool RaycastOcclusion(const glm::vec2& from, const glm::vec2& to) const;

    // Coordinate conversion
    int TileIndex(int x, int y) const;
    glm::ivec2 WorldToTile(const glm::vec2& worldPos) const;
    glm::vec2 TileToWorld(int x, int y) const;

    // Map dimensions
    int m_mapWidth = 0;
    int m_mapHeight = 0;
    float m_tileSize = 1.0f;

    // Screen dimensions
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    // Configuration
    SessionFogConfig m_config;

    // State
    bool m_initialized = false;

    // Session timing
    std::chrono::steady_clock::time_point m_sessionStartTime;
    std::chrono::steady_clock::time_point m_lastActivityTime;
    bool m_sessionActive = false;

    // Fog state arrays (per tile)
    std::vector<FogState> m_fogState;           // Current state of each tile
    std::vector<uint8_t> m_visibilityState;     // Current visibility (0-255)
    std::vector<float> m_fogBrightness;         // Smooth transition brightness

    // Occlusion data for line of sight
    std::vector<uint8_t> m_occlusionData;
    int m_occlusionWidth = 0;
    int m_occlusionHeight = 0;

    // Statistics
    int m_tilesExploredCount = 0;
    int m_tilesVisibleCount = 0;
    float m_lastExplorationPercent = 0.0f;

    // GPU resources
    uint32_t m_fogTexture = 0;              // Fog factor texture
    uint32_t m_combinedTexture = 0;         // Combined fog + lighting
    uint32_t m_exploredTexture = 0;         // Explored state texture
    uint32_t m_visibilityTexture = 0;       // Visibility state texture
    uint32_t m_fogUpdateShader = 0;         // Fog update compute shader
    uint32_t m_fogCombineShader = 0;        // Fog combine compute shader

    // External references
    RadianceCascades* m_radianceCascades = nullptr;

    // Callbacks
    TileRevealedCallback m_onTileRevealed;
    AreaExploredCallback m_onAreaExplored;
    SessionResetCallback m_onSessionReset;
};

} // namespace RTS
} // namespace Vehement2
