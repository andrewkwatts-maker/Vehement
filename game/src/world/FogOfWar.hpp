#pragma once

#include "RadianceCascades.hpp"

#include <vector>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Vehement2 {

/**
 * @brief Fog of War state for each map cell
 */
enum class FogState : uint8_t {
    Unexplored = 0,     // Never seen - completely black
    Explored = 1,       // Previously seen - dimmed
    Visible = 2         // Currently visible - full brightness
};

/**
 * @brief Fog of War system using Radiance Cascades
 *
 * This system manages fog of war for top-down games:
 * - Unexplored areas are completely black
 * - Explored (but not currently visible) areas are dimmed
 * - Currently visible areas show full lighting
 *
 * The system integrates with RadianceCascades for visibility calculations
 * and maintains a persistent explored state that can be saved/loaded.
 *
 * Usage:
 * 1. Initialize with map dimensions
 * 2. Call UpdateVisibility() each frame with player position
 * 3. Use GetFogTexture() to multiply with scene rendering
 * 4. Save/Load explored state for persistence
 */
class FogOfWar {
public:
    /**
     * @brief Configuration for fog of war rendering
     */
    struct Config {
        float unexploredBrightness = 0.0f;    // Brightness of unexplored areas (0 = black)
        float exploredBrightness = 0.3f;      // Brightness of explored but not visible areas
        float visibleBrightness = 1.0f;       // Brightness of currently visible areas
        float transitionSpeed = 8.0f;         // How fast fog transitions (units per second)
        float visibilityThreshold = 0.1f;     // Minimum visibility to mark as explored
        bool revealOnExplore = true;          // Automatically mark visible areas as explored
        glm::vec3 fogColor = glm::vec3(0.0f); // Color of the fog (usually black)
        glm::vec3 exploredTint = glm::vec3(0.6f, 0.65f, 0.8f);  // Tint for explored areas
    };

    FogOfWar();
    ~FogOfWar();

    // Disable copy, enable move
    FogOfWar(const FogOfWar&) = delete;
    FogOfWar& operator=(const FogOfWar&) = delete;
    FogOfWar(FogOfWar&& other) noexcept;
    FogOfWar& operator=(FogOfWar&& other) noexcept;

    /**
     * @brief Initialize fog of war system
     * @param width Map width in tiles
     * @param height Map height in tiles
     * @param tileSize Size of each tile in pixels
     * @param screenWidth Screen width for textures
     * @param screenHeight Screen height for textures
     * @return true if initialization succeeded
     */
    bool Initialize(int width, int height, float tileSize, int screenWidth, int screenHeight);

    /**
     * @brief Cleanup all resources
     */
    void Shutdown();

    /**
     * @brief Resize screen textures
     */
    void Resize(int screenWidth, int screenHeight);

    /**
     * @brief Set the radiance cascade system to use for visibility
     * @param cascades Pointer to RadianceCascades (must outlive FogOfWar)
     */
    void SetRadianceCascades(RadianceCascades* cascades);

    /**
     * @brief Update fog of war based on current visibility
     * @param playerPos Current player position
     * @param deltaTime Time since last update (for smooth transitions)
     */
    void UpdateVisibility(glm::vec2 playerPos, float deltaTime);

    /**
     * @brief Force update the entire fog state (slower, but complete)
     */
    void ForceUpdate();

    /**
     * @brief Get the fog texture for rendering
     * @return OpenGL texture ID (R = fog factor, 0 = black, 1 = visible)
     */
    uint32_t GetFogTexture() const { return m_fogTexture; }

    /**
     * @brief Get the combined fog + lighting texture
     * This texture combines fog of war with radiance cascade lighting
     * @return OpenGL texture ID (RGBA)
     */
    uint32_t GetCombinedTexture() const { return m_combinedTexture; }

    /**
     * @brief Get fog state at a specific tile
     */
    FogState GetFogState(int tileX, int tileY) const;

    /**
     * @brief Get fog state at world position
     */
    FogState GetFogStateAtPosition(glm::vec2 worldPos) const;

    /**
     * @brief Get the current fog brightness at a position (0-1)
     */
    float GetFogBrightness(glm::vec2 worldPos) const;

    /**
     * @brief Check if a tile has been explored
     */
    bool IsExplored(int tileX, int tileY) const;

    /**
     * @brief Check if a tile is currently visible
     */
    bool IsVisible(int tileX, int tileY) const;

    /**
     * @brief Manually reveal a tile
     */
    void RevealTile(int tileX, int tileY);

    /**
     * @brief Manually reveal an area
     * @param center Center of area to reveal
     * @param radius Radius in tiles
     */
    void RevealArea(glm::vec2 center, float radius);

    /**
     * @brief Reveal the entire map
     */
    void RevealAll();

    /**
     * @brief Reset fog to unexplored
     */
    void ResetFog();

    /**
     * @brief Hide a specific tile (mark as unexplored)
     */
    void HideTile(int tileX, int tileY);

    /**
     * @brief Save explored state to file
     * @param filepath Path to save file
     * @return true if saved successfully
     */
    bool SaveExploredState(const std::string& filepath) const;

    /**
     * @brief Load explored state from file
     * @param filepath Path to load file
     * @return true if loaded successfully
     */
    bool LoadExploredState(const std::string& filepath);

    /**
     * @brief Get explored state as raw data (for custom serialization)
     */
    const std::vector<uint8_t>& GetExploredData() const { return m_exploredState; }

    /**
     * @brief Set explored state from raw data
     */
    void SetExploredData(const std::vector<uint8_t>& data);

    /**
     * @brief Get percentage of map explored (0-100)
     */
    float GetExplorationProgress() const;

    /**
     * @brief Set configuration
     */
    void SetConfig(const Config& config);

    /**
     * @brief Get current configuration
     */
    const Config& GetConfig() const { return m_config; }

    /**
     * @brief Get map dimensions
     */
    int GetMapWidth() const { return m_mapWidth; }
    int GetMapHeight() const { return m_mapHeight; }
    float GetTileSize() const { return m_tileSize; }

    /**
     * @brief Check if system is initialized
     */
    bool IsInitialized() const { return m_initialized; }

private:
    // Internal methods
    bool CreateTextures();
    bool CreateShaders();
    void DestroyResources();

    void UpdateExploredState(glm::vec2 playerPos);
    void UpdateFogTexture(float deltaTime);
    void UpdateCombinedTexture();

    int TileIndex(int x, int y) const;
    glm::ivec2 WorldToTile(glm::vec2 worldPos) const;
    glm::vec2 TileToWorld(int x, int y) const;

    // Map dimensions
    int m_mapWidth = 0;
    int m_mapHeight = 0;
    float m_tileSize = 0.0f;

    // Screen dimensions
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    // Configuration
    Config m_config;

    // State
    bool m_initialized = false;

    // Persistent explored state (1 byte per tile: 0 = unexplored, 1 = explored)
    std::vector<uint8_t> m_exploredState;

    // Current visibility state (1 byte per tile: 0 = not visible, 255 = visible)
    std::vector<uint8_t> m_visibilityState;

    // Current fog brightness (smooth transition)
    std::vector<float> m_fogBrightness;

    // Textures
    uint32_t m_fogTexture = 0;          // Single channel fog factor
    uint32_t m_combinedTexture = 0;     // Combined fog + lighting
    uint32_t m_exploredTexture = 0;     // Explored state texture

    // Compute shaders
    uint32_t m_fogUpdateShader = 0;     // Updates fog brightness
    uint32_t m_fogCombineShader = 0;    // Combines fog with lighting

    // Reference to radiance cascades (not owned)
    RadianceCascades* m_radianceCascades = nullptr;

    // Player state
    glm::vec2 m_lastPlayerPos = glm::vec2(0.0f);
};

/**
 * @brief Helper class for applying fog of war to rendering
 *
 * Use this to easily integrate fog of war with your renderer:
 *
 * Example usage:
 *   FogOfWarRenderer fogRenderer;
 *   fogRenderer.Begin(fogOfWar);
 *   // ... render your scene ...
 *   fogRenderer.End();
 */
class FogOfWarRenderer {
public:
    FogOfWarRenderer();
    ~FogOfWarRenderer();

    /**
     * @brief Initialize the fog renderer
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Apply fog of war to a rendered scene
     * @param fogOfWar The fog of war system
     * @param sceneTexture The rendered scene texture
     * @param outputFramebuffer Target framebuffer (0 for default)
     */
    void ApplyFog(const FogOfWar& fogOfWar, uint32_t sceneTexture, uint32_t outputFramebuffer = 0);

    /**
     * @brief Apply fog using radiance cascade lighting directly
     * @param fogOfWar The fog of war system
     * @param cascades The radiance cascade system
     * @param sceneTexture The rendered scene texture
     * @param outputFramebuffer Target framebuffer
     */
    void ApplyFogWithLighting(const FogOfWar& fogOfWar, const RadianceCascades& cascades,
                              uint32_t sceneTexture, uint32_t outputFramebuffer = 0);

private:
    bool CreateShaders();
    void DestroyResources();

    uint32_t m_fogShader = 0;
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;
    bool m_initialized = false;
};

} // namespace Vehement2
