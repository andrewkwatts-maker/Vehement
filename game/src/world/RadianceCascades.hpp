#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

// Forward declaration for existing TileMap
namespace Vehement {
    class TileMap;
}

namespace Vehement2 {

/**
 * @brief Abstract interface for occlusion data
 *
 * This interface allows RadianceCascades to work with different occlusion
 * data sources. Implement this interface to provide custom occlusion data,
 * or use the built-in SetOcclusionFromVehementMap() for integration with
 * Vehement::TileMap.
 */
class IOcclusionProvider {
public:
    virtual ~IOcclusionProvider() = default;
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual bool IsBlocked(int x, int y) const = 0;  // Returns true if tile blocks light/visibility
    virtual float GetTileSize() const = 0;
};

/**
 * @brief Light source data for radiance cascade calculations
 */
struct RadianceLight {
    glm::vec2 position;     // World position
    glm::vec3 color;        // RGB color
    float intensity;        // Light intensity multiplier
    float radius;           // Maximum light radius

    RadianceLight() : position(0.0f), color(1.0f), intensity(1.0f), radius(100.0f) {}
    RadianceLight(glm::vec2 pos, glm::vec3 col, float intens, float rad)
        : position(pos), color(col), intensity(intens), radius(rad) {}
};

/**
 * @brief Radiance Cascades implementation for 2D global illumination
 *
 * This implements the Radiance Cascades algorithm popularized by Alexander Sannikov
 * for efficient 2D global illumination. The technique works by:
 *
 * 1. Creating multiple cascade levels at decreasing resolutions
 * 2. At each level, shooting rays in multiple directions to gather radiance
 * 3. Merging cascades from coarse to fine for smooth light propagation
 * 4. Producing a final radiance texture for lighting and visibility queries
 *
 * The algorithm is particularly suited for top-down 2D games where:
 * - Walls and objects block light realistically
 * - Multiple light sources need to be handled efficiently
 * - Soft shadows and light bleeding are desired
 * - Fog of war / visibility needs to be computed
 */
class RadianceCascades {
public:
    /**
     * @brief Configuration for radiance cascade computation
     */
    struct Config {
        int raysPerPixel = 64;          // Number of rays per pixel in first cascade
        float rayStepSize = 1.0f;       // Ray marching step size in pixels
        float maxRayDistance = 256.0f;  // Maximum ray distance
        float intervalLength = 4.0f;    // Base interval length for cascades
        float biasDistance = 0.5f;      // Bias to prevent self-occlusion
        bool enableSoftShadows = true;  // Enable penumbra calculation
        float shadowSoftness = 2.0f;    // Softness factor for shadows
    };

    RadianceCascades();
    ~RadianceCascades();

    // Disable copy
    RadianceCascades(const RadianceCascades&) = delete;
    RadianceCascades& operator=(const RadianceCascades&) = delete;

    // Enable move
    RadianceCascades(RadianceCascades&& other) noexcept;
    RadianceCascades& operator=(RadianceCascades&& other) noexcept;

    /**
     * @brief Initialize radiance cascades with screen dimensions
     * @param width Screen/texture width in pixels
     * @param height Screen/texture height in pixels
     * @param cascadeLevels Number of cascade levels (default 4)
     * @return true if initialization succeeded
     */
    bool Initialize(int width, int height, int cascadeLevels = 4);

    /**
     * @brief Cleanup all GPU resources
     */
    void Shutdown();

    /**
     * @brief Resize the cascade textures
     * @param width New width
     * @param height New height
     */
    void Resize(int width, int height);

    /**
     * @brief Set the occlusion map from an occlusion provider
     * @param provider Occlusion provider containing blocking information
     */
    void SetOcclusionMap(const IOcclusionProvider& provider);

    /**
     * @brief Set the occlusion map from the existing Vehement::TileMap
     * Uses the Tile::blocksSight property to determine occlusion.
     * @param map Vehement TileMap containing tiles with blocksSight data
     */
    void SetOcclusionFromVehementMap(const Vehement::TileMap& map);

    /**
     * @brief Set occlusion from raw texture data
     * @param data Occlusion data (1 byte per pixel, 255 = blocked, 0 = open)
     * @param width Texture width
     * @param height Texture height
     */
    void SetOcclusionData(const uint8_t* data, int width, int height);

    /**
     * @brief Add a light source
     * @param position World position of the light
     * @param color RGB color of the light (0-1 range)
     * @param intensity Light intensity multiplier
     * @param radius Maximum radius of the light
     */
    void AddLight(glm::vec2 position, glm::vec3 color, float intensity, float radius);

    /**
     * @brief Add a light using RadianceLight struct
     */
    void AddLight(const RadianceLight& light);

    /**
     * @brief Set player position (emits visibility "light")
     * @param pos Player world position
     */
    void SetPlayerPosition(glm::vec2 pos);

    /**
     * @brief Set player visibility radius
     * @param radius Maximum visibility distance
     */
    void SetPlayerVisibilityRadius(float radius);

    /**
     * @brief Clear all dynamic lights (keeps player visibility)
     */
    void ClearLights();

    /**
     * @brief Update radiance cascades (runs compute shaders)
     * Call this once per frame after setting lights and before rendering
     */
    void Update();

    /**
     * @brief Get the final radiance texture for rendering
     * @return OpenGL texture ID containing RGBA radiance data
     */
    uint32_t GetRadianceTexture() const { return m_finalRadianceTexture; }

    /**
     * @brief Get a specific cascade texture (for debugging)
     * @param level Cascade level (0 = finest)
     * @return OpenGL texture ID
     */
    uint32_t GetCascadeTexture(int level) const;

    /**
     * @brief Get the occlusion texture (for debugging)
     * @return OpenGL texture ID
     */
    uint32_t GetOcclusionTexture() const { return m_occlusionTexture; }

    /**
     * @brief Check if a point is visible from another point
     * @param from Source position (e.g., viewer)
     * @param to Target position
     * @return true if line of sight is clear
     */
    bool IsVisible(glm::vec2 from, glm::vec2 to) const;

    /**
     * @brief Get visibility value at a point (from player's perspective)
     * @param position World position to query
     * @return Visibility value 0-1 (0 = not visible, 1 = fully visible)
     */
    float GetVisibility(glm::vec2 position) const;

    /**
     * @brief Get the radiance (light amount) at a specific position
     * @param position World position to query
     * @return RGB radiance value
     */
    glm::vec3 GetRadiance(glm::vec2 position) const;

    /**
     * @brief Set configuration parameters
     */
    void SetConfig(const Config& config);

    /**
     * @brief Get current configuration
     */
    const Config& GetConfig() const { return m_config; }

    /**
     * @brief Set world to screen transform
     * @param worldToScreen Transform matrix from world to screen coordinates
     */
    void SetWorldToScreenTransform(const glm::mat4& worldToScreen);

    /**
     * @brief Get texture dimensions
     */
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetCascadeLevels() const { return m_cascadeLevels; }

    /**
     * @brief Check if system is initialized
     */
    bool IsInitialized() const { return m_initialized; }

private:
    // Internal methods
    bool CreateShaders();
    bool CreateTextures();
    bool CreateBuffers();
    void DestroyResources();

    void UploadLightData();
    void DispatchRayMarch();
    void DispatchMerge();
    void DispatchFinal();

    // Utility for CPU-side visibility queries
    bool RayMarchOcclusion(glm::vec2 from, glm::vec2 to) const;
    float SampleOcclusion(int x, int y) const;

    // Dimensions
    int m_width = 0;
    int m_height = 0;
    int m_cascadeLevels = 4;

    // Configuration
    Config m_config;

    // State
    bool m_initialized = false;
    bool m_occlusionDirty = true;
    bool m_lightsDirty = true;

    // Cascade textures at different resolutions
    // Level 0 = full resolution, Level N = resolution / 2^N
    std::vector<uint32_t> m_cascadeTextures;

    // Temporary textures for ping-pong during merge
    std::vector<uint32_t> m_cascadeTempTextures;

    // Final output textures
    uint32_t m_finalRadianceTexture = 0;  // RGBA: RGB = radiance, A = visibility

    // Occlusion texture (R8: 255 = blocked, 0 = open)
    uint32_t m_occlusionTexture = 0;
    std::vector<uint8_t> m_occlusionData;  // CPU copy for queries
    int m_occlusionWidth = 0;
    int m_occlusionHeight = 0;

    // Light texture (for passing light data to GPU)
    uint32_t m_lightTexture = 0;

    // Light data
    std::vector<RadianceLight> m_lights;
    static const int MAX_LIGHTS = 256;

    // Player visibility
    glm::vec2 m_playerPosition = glm::vec2(0.0f);
    float m_playerVisibilityRadius = 300.0f;
    bool m_hasPlayer = false;

    // Transform
    glm::mat4 m_worldToScreen = glm::mat4(1.0f);
    glm::mat4 m_screenToWorld = glm::mat4(1.0f);

    // Compute shaders
    uint32_t m_rayMarchShader = 0;      // Initial ray marching pass
    uint32_t m_mergeShader = 0;         // Cascade merging pass
    uint32_t m_radianceShader = 0;      // Final radiance calculation

    // Shader storage buffer for lights
    uint32_t m_lightSSBO = 0;

    // Uniform buffer for cascade parameters
    uint32_t m_paramsUBO = 0;
};

} // namespace Vehement2
