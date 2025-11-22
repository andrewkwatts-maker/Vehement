#pragma once

#include "RadianceCascades3D.hpp"

#include <vector>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Vehement2 {

/**
 * @brief 3D Fog of War state for each voxel
 */
enum class FogState3D : uint8_t {
    Unknown = 0,     // Never seen - completely black/hidden
    Explored = 1,    // Previously seen - dimmed
    Visible = 2      // Currently visible - full brightness
};

/**
 * @brief View mode for floor rendering
 */
enum class ViewMode3D {
    CurrentFloor,    // Only show current floor
    CutawayAbove,    // Show current floor + hide everything above
    XRay,            // See through floors within vision range
    AllFloors        // Debug: show everything (no hiding)
};

/**
 * @brief 3D Fog of War system with floor hiding
 *
 * This extends the 2D fog of war to support multi-floor buildings:
 *
 * Key features:
 * - Per-voxel fog state (Unknown/Explored/Visible)
 * - Floor-based visibility (hide floors above player)
 * - Vertical line of sight through stairs/holes
 * - Multiple view modes for different gameplay styles
 * - Smooth transitions when changing floors
 *
 * Floor visibility rules:
 * - Floors above current floor are hidden (not rendered)
 * - Current floor is fully visible within vision range
 * - Floors below can be seen if there are openings (stairs, holes)
 * - Previously explored areas remain visible in "explored" state
 *
 * Usage:
 * 1. Initialize with volume dimensions
 * 2. Call UpdateVisibility() each frame with player position
 * 3. Use GetFogTextureForFloor() for floor-by-floor rendering
 * 4. Use ShouldRenderFloor() to determine which floors to draw
 */
class FogOfWar3D {
public:
    /**
     * @brief Configuration for 3D fog of war
     */
    struct Config {
        float unexploredBrightness = 0.0f;    // Brightness of unexplored areas
        float exploredBrightness = 0.3f;      // Brightness of explored but not visible
        float visibleBrightness = 1.0f;       // Brightness of currently visible areas
        float transitionSpeed = 8.0f;         // Fog transition speed
        float visibilityThreshold = 0.1f;     // Minimum visibility to mark as explored
        bool revealOnExplore = true;          // Auto-mark visible areas as explored

        // Floor transition
        float floorTransitionSpeed = 4.0f;    // Speed of floor fade transitions
        float aboveFloorOpacity = 0.0f;       // Opacity of floors above player
        float belowFloorOpacity = 0.5f;       // Opacity of visible floors below

        // Colors
        glm::vec3 fogColor = glm::vec3(0.0f);
        glm::vec3 exploredTint = glm::vec3(0.6f, 0.65f, 0.8f);

        // Vertical vision
        int maxVerticalVisionUp = 1;          // How many floors up player can see
        int maxVerticalVisionDown = 2;        // How many floors down player can see
    };

    FogOfWar3D();
    ~FogOfWar3D();

    // Disable copy, enable move
    FogOfWar3D(const FogOfWar3D&) = delete;
    FogOfWar3D& operator=(const FogOfWar3D&) = delete;
    FogOfWar3D(FogOfWar3D&& other) noexcept;
    FogOfWar3D& operator=(FogOfWar3D&& other) noexcept;

    /**
     * @brief Initialize 3D fog of war system
     * @param width Volume width in voxels
     * @param height Volume height in voxels
     * @param depth Volume depth (number of floors)
     * @param tileSizeXY Tile size in X/Y plane
     * @param tileSizeZ Tile size in Z (height per floor)
     * @return true if initialization succeeded
     */
    bool Initialize(int width, int height, int depth, float tileSizeXY, float tileSizeZ);

    /**
     * @brief Cleanup all resources
     */
    void Shutdown();

    /**
     * @brief Resize the fog volume
     */
    void Resize(int width, int height, int depth);

    /**
     * @brief Set the 3D radiance cascade system for visibility
     */
    void SetRadianceCascades(RadianceCascades3D* cascades);

    // ========================================================================
    // Floor Management
    // ========================================================================

    /**
     * @brief Set the current floor the player is on
     * @param floor Floor index (0 = ground floor)
     */
    void SetCurrentFloor(int floor);

    /**
     * @brief Get the current player floor
     */
    int GetCurrentFloor() const { return m_currentFloor; }

    /**
     * @brief Set the view mode for floor visibility
     */
    void SetViewMode(ViewMode3D mode);

    /**
     * @brief Get current view mode
     */
    ViewMode3D GetViewMode() const { return m_viewMode; }

    /**
     * @brief Check if a floor should be rendered
     * Based on current floor, view mode, and visibility settings
     * @param floor Floor index to check
     * @return true if floor should be rendered
     */
    bool ShouldRenderFloor(int floor) const;

    /**
     * @brief Get the opacity for rendering a specific floor
     * Used for smooth transitions and floor fading
     * @param floor Floor index
     * @return Opacity value 0-1
     */
    float GetFloorOpacity(int floor) const;

    /**
     * @brief Get the transition state for floor rendering
     * Returns 0-1 for smooth floor transitions
     */
    float GetFloorTransition() const { return m_floorTransition; }

    // ========================================================================
    // Visibility Updates
    // ========================================================================

    /**
     * @brief Update fog of war based on current visibility
     * @param playerPos Current player 3D position
     * @param deltaTime Time since last update
     */
    void UpdateVisibility(glm::vec3 playerPos, float deltaTime);

    /**
     * @brief Force update the entire fog volume
     */
    void ForceUpdate();

    // ========================================================================
    // Textures and Rendering
    // ========================================================================

    /**
     * @brief Get the fog texture for a specific floor
     * @param floor Floor index
     * @return OpenGL texture ID for that floor's fog
     */
    uint32_t GetFogTextureForFloor(int floor) const;

    /**
     * @brief Get the combined fog+lighting texture for a floor
     * @param floor Floor index
     * @return OpenGL texture ID
     */
    uint32_t GetCombinedTextureForFloor(int floor) const;

    /**
     * @brief Get the 3D fog volume texture
     * Useful for volumetric rendering
     */
    uint32_t GetFogVolume() const { return m_fogVolume; }

    // ========================================================================
    // Fog State Queries
    // ========================================================================

    /**
     * @brief Get fog state at a specific voxel
     */
    FogState3D GetFogState(int x, int y, int z) const;

    /**
     * @brief Get fog state at world position
     */
    FogState3D GetFogStateAtPosition(glm::vec3 worldPos) const;

    /**
     * @brief Get fog brightness at a position
     */
    float GetFogBrightness(glm::vec3 worldPos) const;

    /**
     * @brief Check if a voxel has been explored
     */
    bool IsExplored(int x, int y, int z) const;

    /**
     * @brief Check if a voxel is currently visible
     */
    bool IsVisible(int x, int y, int z) const;

    /**
     * @brief Check if there's vertical line of sight between positions
     */
    bool HasVerticalLineOfSight(glm::ivec3 from, glm::ivec3 to) const;

    // ========================================================================
    // Manual Reveal/Hide
    // ========================================================================

    /**
     * @brief Manually reveal a voxel
     */
    void RevealVoxel(int x, int y, int z);

    /**
     * @brief Reveal a 3D area
     */
    void RevealArea(glm::vec3 center, float radius);

    /**
     * @brief Reveal an entire floor
     */
    void RevealFloor(int floor);

    /**
     * @brief Reveal everything
     */
    void RevealAll();

    /**
     * @brief Reset all fog to unexplored
     */
    void ResetFog();

    /**
     * @brief Hide a specific voxel
     */
    void HideVoxel(int x, int y, int z);

    // ========================================================================
    // Persistence
    // ========================================================================

    /**
     * @brief Save explored state to file
     */
    bool SaveExploredState(const std::string& filepath) const;

    /**
     * @brief Load explored state from file
     */
    bool LoadExploredState(const std::string& filepath);

    /**
     * @brief Get explored state as raw data
     */
    const std::vector<uint8_t>& GetExploredData() const { return m_exploredState; }

    /**
     * @brief Set explored state from raw data
     */
    void SetExploredData(const std::vector<uint8_t>& data);

    /**
     * @brief Get exploration progress (0-100%)
     */
    float GetExplorationProgress() const;

    /**
     * @brief Get exploration progress for a specific floor
     */
    float GetFloorExplorationProgress(int floor) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    void SetConfig(const Config& config);
    const Config& GetConfig() const { return m_config; }

    // Dimensions
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetDepth() const { return m_depth; }
    float GetTileSizeXY() const { return m_tileSizeXY; }
    float GetTileSizeZ() const { return m_tileSizeZ; }

    bool IsInitialized() const { return m_initialized; }

private:
    // Internal methods
    bool CreateTextures();
    bool CreateShaders();
    void DestroyResources();

    void UpdateExploredState(glm::vec3 playerPos);
    void UpdateFogTextures(float deltaTime);
    void UpdateFloorTransition(float deltaTime);

    int VoxelIndex(int x, int y, int z) const;
    glm::ivec3 WorldToVoxel(glm::vec3 worldPos) const;
    glm::vec3 VoxelToWorld(int x, int y, int z) const;

    // Volume dimensions
    int m_width = 0;
    int m_height = 0;
    int m_depth = 0;
    float m_tileSizeXY = 32.0f;
    float m_tileSizeZ = 10.67f;

    // Configuration
    Config m_config;

    // State
    bool m_initialized = false;
    int m_currentFloor = 0;
    int m_previousFloor = 0;
    float m_floorTransition = 1.0f;  // 0 = transitioning, 1 = complete
    ViewMode3D m_viewMode = ViewMode3D::CutawayAbove;

    // Fog state per voxel
    std::vector<uint8_t> m_exploredState;    // 0 = unexplored, 1 = explored
    std::vector<uint8_t> m_visibilityState;  // 0 = not visible, 255 = visible
    std::vector<float> m_fogBrightness;      // Current brightness (for transitions)

    // 3D fog volume texture
    uint32_t m_fogVolume = 0;

    // Per-floor 2D textures
    std::vector<uint32_t> m_floorFogTextures;
    std::vector<uint32_t> m_floorCombinedTextures;
    std::vector<uint32_t> m_floorExploredTextures;

    // Compute shaders
    uint32_t m_fogUpdate3DShader = 0;
    uint32_t m_fogCombine3DShader = 0;
    uint32_t m_extractFloorFogShader = 0;

    // Reference to radiance cascades (not owned)
    RadianceCascades3D* m_radianceCascades = nullptr;

    // Player state
    glm::vec3 m_lastPlayerPos = glm::vec3(0.0f);
};

/**
 * @brief Helper class for applying 3D fog of war to floor-by-floor rendering
 *
 * Usage:
 *   FogOfWar3DRenderer renderer;
 *   renderer.Initialize();
 *
 *   // Render floors bottom-up
 *   for (int floor = 0; floor < numFloors; floor++) {
 *       if (!fogOfWar.ShouldRenderFloor(floor)) continue;
 *
 *       float opacity = fogOfWar.GetFloorOpacity(floor);
 *       renderer.BeginFloor(fogOfWar, floor, opacity);
 *       // ... render floor geometry ...
 *       renderer.EndFloor();
 *   }
 */
class FogOfWar3DRenderer {
public:
    FogOfWar3DRenderer();
    ~FogOfWar3DRenderer();

    bool Initialize();
    void Shutdown();

    /**
     * @brief Begin rendering a floor with fog applied
     * @param fogOfWar The 3D fog of war system
     * @param floor Floor index to render
     * @param opacity Floor opacity (for transitions)
     */
    void BeginFloor(const FogOfWar3D& fogOfWar, int floor, float opacity = 1.0f);

    /**
     * @brief End floor rendering
     */
    void EndFloor();

    /**
     * @brief Apply fog to a rendered floor
     * @param fogOfWar Fog of war system
     * @param cascades Radiance cascade system
     * @param floor Floor index
     * @param sceneTexture Rendered scene texture
     * @param outputFramebuffer Target framebuffer
     */
    void ApplyFogToFloor(const FogOfWar3D& fogOfWar, const RadianceCascades3D& cascades,
                         int floor, uint32_t sceneTexture, uint32_t outputFramebuffer = 0);

    /**
     * @brief Get recommended render order for floors
     * Returns floors sorted for proper rendering (bottom-up, skip hidden)
     * @param fogOfWar Fog of war system
     * @param outFloors Output vector of floor indices to render
     */
    void GetRenderOrder(const FogOfWar3D& fogOfWar, std::vector<int>& outFloors) const;

private:
    bool CreateShaders();
    void DestroyResources();

    uint32_t m_floorFogShader = 0;
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;
    bool m_initialized = false;

    // Current rendering state
    int m_currentRenderFloor = -1;
    float m_currentFloorOpacity = 1.0f;
};

} // namespace Vehement2
