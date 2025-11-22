#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace Vehement2 {

/**
 * @brief Abstract interface for 3D voxel occlusion data
 *
 * Implement this interface to provide custom voxel occlusion data
 * for the 3D radiance cascade system.
 */
class IVoxelOcclusionProvider {
public:
    virtual ~IVoxelOcclusionProvider() = default;
    virtual int GetWidth() const = 0;   // X dimension
    virtual int GetHeight() const = 0;  // Y dimension
    virtual int GetDepth() const = 0;   // Z dimension (number of floors)
    virtual bool IsBlocked(int x, int y, int z) const = 0;  // Returns true if voxel blocks light/visibility
    virtual float GetTileSizeXY() const = 0;  // Tile size in X/Y plane
    virtual float GetTileSizeZ() const = 0;   // Tile size in Z (height per floor)
};

/**
 * @brief 3D Voxel map structure for occlusion data
 *
 * Simple voxel map implementation that can be used directly
 * or as a reference for custom implementations.
 */
class Voxel3DMap {
public:
    Voxel3DMap() = default;
    Voxel3DMap(int width, int height, int depth, float tileSizeXY, float tileSizeZ);

    void Resize(int width, int height, int depth);

    // Accessors
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetDepth() const { return m_depth; }
    float GetTileSizeXY() const { return m_tileSizeXY; }
    float GetTileSizeZ() const { return m_tileSizeZ; }

    // Voxel access
    bool IsBlocked(int x, int y, int z) const;
    void SetBlocked(int x, int y, int z, bool blocked);

    // Bulk access for GPU upload
    const std::vector<uint8_t>& GetData() const { return m_data; }
    std::vector<uint8_t>& GetData() { return m_data; }

    // Convert world position to voxel coordinates
    glm::ivec3 WorldToVoxel(glm::vec3 worldPos) const;
    glm::vec3 VoxelToWorld(int x, int y, int z) const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_depth = 0;
    float m_tileSizeXY = 32.0f;
    float m_tileSizeZ = 10.67f;  // Default: 1/3 of X/Y tile size
    std::vector<uint8_t> m_data;

    int VoxelIndex(int x, int y, int z) const;
};

/**
 * @brief 3D Light source data for radiance cascade calculations
 */
struct RadianceLight3D {
    glm::vec3 position;     // World position (x, y, z)
    glm::vec3 color;        // RGB color
    float intensity;        // Light intensity multiplier
    float radius;           // Maximum light radius
    int floorLevel;         // Which floor the light is on (-1 = auto-detect)

    RadianceLight3D()
        : position(0.0f), color(1.0f), intensity(1.0f), radius(100.0f), floorLevel(-1) {}
    RadianceLight3D(glm::vec3 pos, glm::vec3 col, float intens, float rad, int floor = -1)
        : position(pos), color(col), intensity(intens), radius(rad), floorLevel(floor) {}
};

/**
 * @brief Full 3D Radiance Cascades implementation for voxel-based global illumination
 *
 * This extends the 2D Radiance Cascades algorithm to work in full 3D space:
 *
 * 1. Uses 3D textures (volumes) instead of 2D textures for cascade data
 * 2. Voxel-based occlusion where walls, floors, and ceilings block light
 * 3. Ray marching in 3D space through the voxel grid
 * 4. Floor-by-floor radiance extraction for rendering
 *
 * Key features:
 * - Full 3D light propagation through multi-floor buildings
 * - Vertical light blocking (floors/ceilings)
 * - Configurable Z dimension (default: Z tile = 1/3 of X/Y tile)
 * - Per-floor radiance texture extraction for layered rendering
 * - Efficient GPU-based compute shader implementation
 */
class RadianceCascades3D {
public:
    /**
     * @brief Configuration for 3D radiance cascade computation
     */
    struct Config {
        int raysPerVoxel = 64;          // Number of rays per voxel in first cascade
        float rayStepSize = 1.0f;       // Ray marching step size
        float maxRayDistance = 256.0f;  // Maximum ray distance
        float intervalLength = 4.0f;    // Base interval length for cascades
        float biasDistance = 0.5f;      // Bias to prevent self-occlusion
        bool enableSoftShadows = true;  // Enable penumbra calculation
        float shadowSoftness = 2.0f;    // Softness factor for shadows
        float zScale = 0.333f;          // Z tile size relative to X/Y (default 1/3)
        bool enableVerticalLight = true; // Allow light to travel between floors
        float verticalFalloff = 0.5f;   // How much light diminishes between floors
    };

    RadianceCascades3D();
    ~RadianceCascades3D();

    // Disable copy
    RadianceCascades3D(const RadianceCascades3D&) = delete;
    RadianceCascades3D& operator=(const RadianceCascades3D&) = delete;

    // Enable move
    RadianceCascades3D(RadianceCascades3D&& other) noexcept;
    RadianceCascades3D& operator=(RadianceCascades3D&& other) noexcept;

    /**
     * @brief Initialize 3D radiance cascades
     * @param width Volume width in voxels
     * @param height Volume height in voxels
     * @param depth Volume depth (number of Z levels/floors)
     * @param cascadeLevels Number of cascade levels (default 4)
     * @return true if initialization succeeded
     */
    bool Initialize(int width, int height, int depth, int cascadeLevels = 4);

    /**
     * @brief Cleanup all GPU resources
     */
    void Shutdown();

    /**
     * @brief Resize the cascade volumes
     */
    void Resize(int width, int height, int depth);

    // ========================================================================
    // Occlusion Volume Management
    // ========================================================================

    /**
     * @brief Update occlusion volume from a Voxel3DMap
     * @param map 3D voxel map containing blocking information
     */
    void UpdateOcclusionVolume(const Voxel3DMap& map);

    /**
     * @brief Update occlusion volume from an occlusion provider
     * @param provider Voxel occlusion provider
     */
    void UpdateOcclusionVolume(const IVoxelOcclusionProvider& provider);

    /**
     * @brief Set occlusion from raw volume data
     * @param data Occlusion data (1 byte per voxel, 255 = blocked, 0 = open)
     * @param width Volume width
     * @param height Volume height
     * @param depth Volume depth
     */
    void SetOcclusionData(const uint8_t* data, int width, int height, int depth);

    /**
     * @brief Get the occlusion volume texture
     */
    uint32_t GetOcclusionVolume() const { return m_occlusionVolume; }

    // ========================================================================
    // Light Management
    // ========================================================================

    /**
     * @brief Add a light source with 3D position
     * @param position World position of the light (x, y, z)
     * @param color RGB color of the light (0-1 range)
     * @param intensity Light intensity multiplier
     * @param radius Maximum radius of the light
     */
    void AddLight(glm::vec3 position, glm::vec3 color, float intensity, float radius);

    /**
     * @brief Add a light using RadianceLight3D struct
     */
    void AddLight(const RadianceLight3D& light);

    /**
     * @brief Clear all dynamic lights
     */
    void ClearLights();

    // ========================================================================
    // Player Position and Floor Management
    // ========================================================================

    /**
     * @brief Set player position in 3D space
     * @param pos Player world position (x, y, z)
     */
    void SetPlayerPosition(glm::vec3 pos);

    /**
     * @brief Get current player position
     */
    glm::vec3 GetPlayerPosition() const { return m_playerPosition; }

    /**
     * @brief Set the current floor the player is on
     * @param zLevel Floor index (0 = ground floor)
     */
    void SetPlayerFloor(int zLevel);

    /**
     * @brief Get the current player floor
     */
    int GetPlayerFloor() const { return m_playerFloor; }

    /**
     * @brief Set player visibility radius
     * @param radius Maximum visibility distance
     */
    void SetPlayerVisibilityRadius(float radius);

    // ========================================================================
    // Update and Rendering
    // ========================================================================

    /**
     * @brief Update radiance cascades (runs compute shaders)
     * Call this once per frame after setting lights and before rendering
     */
    void Update();

    /**
     * @brief Get the final 3D radiance volume texture
     * @return OpenGL 3D texture ID containing RGBA radiance data
     */
    uint32_t GetRadianceVolume() const { return m_finalRadianceVolume; }

    /**
     * @brief Get radiance texture for a specific Z level (floor)
     * Extracts a 2D slice from the 3D volume for floor-by-floor rendering
     * @param zLevel Floor index to extract
     * @return OpenGL 2D texture ID for that floor's radiance
     */
    uint32_t GetRadianceTextureForLevel(int zLevel) const;

    /**
     * @brief Get a specific cascade volume (for debugging)
     * @param level Cascade level (0 = finest)
     * @return OpenGL 3D texture ID
     */
    uint32_t GetCascadeVolume(int level) const;

    // ========================================================================
    // Visibility Queries
    // ========================================================================

    /**
     * @brief Check if a point is visible from another point in 3D space
     * @param from Source position (e.g., viewer)
     * @param to Target position
     * @return true if line of sight is clear
     */
    bool IsVisible(glm::vec3 from, glm::vec3 to) const;

    /**
     * @brief Get visibility value at a 3D position (from player's perspective)
     * @param position World position to query
     * @return Visibility value 0-1 (0 = not visible, 1 = fully visible)
     */
    float GetVisibility(glm::vec3 position) const;

    /**
     * @brief Check if there's vertical line of sight between two points
     * Useful for checking visibility through stairs, holes, etc.
     * @param from Source position
     * @param to Target position
     * @return true if vertical line of sight exists
     */
    bool HasVerticalLineOfSight(glm::vec3 from, glm::vec3 to) const;

    /**
     * @brief Get the radiance (light amount) at a specific 3D position
     * @param position World position to query
     * @return RGB radiance value
     */
    glm::vec3 GetRadiance(glm::vec3 position) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set configuration parameters
     */
    void SetConfig(const Config& config);

    /**
     * @brief Get current configuration
     */
    const Config& GetConfig() const { return m_config; }

    /**
     * @brief Set world to voxel transform parameters
     * @param tileSizeXY Size of tiles in X/Y plane
     * @param tileSizeZ Size of tiles in Z (height per floor)
     */
    void SetTileSizes(float tileSizeXY, float tileSizeZ);

    /**
     * @brief Get volume dimensions
     */
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetDepth() const { return m_depth; }
    int GetCascadeLevels() const { return m_cascadeLevels; }

    /**
     * @brief Check if system is initialized
     */
    bool IsInitialized() const { return m_initialized; }

private:
    // Internal methods
    bool CreateShaders();
    bool CreateVolumes();
    bool CreateBuffers();
    bool CreateFloorTextures();
    void DestroyResources();

    void UploadLightData();
    void DispatchRayMarch3D();
    void DispatchMerge3D();
    void DispatchFinal3D();
    void ExtractFloorTextures();

    // CPU-side visibility queries
    bool RayMarch3DOcclusion(glm::vec3 from, glm::vec3 to) const;
    float SampleOcclusion3D(int x, int y, int z) const;
    glm::ivec3 WorldToVoxel(glm::vec3 worldPos) const;

    // Dimensions
    int m_width = 0;
    int m_height = 0;
    int m_depth = 0;
    int m_cascadeLevels = 4;

    // Tile sizes
    float m_tileSizeXY = 32.0f;
    float m_tileSizeZ = 10.67f;  // 32 / 3 = ~10.67

    // Configuration
    Config m_config;

    // State
    bool m_initialized = false;
    bool m_occlusionDirty = true;
    bool m_lightsDirty = true;

    // 3D Cascade volumes at different resolutions
    // Level 0 = full resolution, Level N = resolution / 2^N
    std::vector<uint32_t> m_cascadeVolumes;

    // Temporary volumes for ping-pong during merge
    std::vector<uint32_t> m_cascadeTempVolumes;

    // Final output volume
    uint32_t m_finalRadianceVolume = 0;  // RGBA: RGB = radiance, A = visibility

    // Occlusion volume (R8: 255 = blocked, 0 = open)
    uint32_t m_occlusionVolume = 0;
    std::vector<uint8_t> m_occlusionData;  // CPU copy for queries
    int m_occlusionWidth = 0;
    int m_occlusionHeight = 0;
    int m_occlusionDepth = 0;

    // Per-floor 2D textures (extracted from 3D volume for rendering)
    std::vector<uint32_t> m_floorRadianceTextures;

    // Light data
    std::vector<RadianceLight3D> m_lights;
    static const int MAX_LIGHTS = 256;

    // Player state
    glm::vec3 m_playerPosition = glm::vec3(0.0f);
    int m_playerFloor = 0;
    float m_playerVisibilityRadius = 300.0f;
    bool m_hasPlayer = false;

    // Compute shaders
    uint32_t m_rayMarch3DShader = 0;     // 3D ray marching pass
    uint32_t m_merge3DShader = 0;        // 3D cascade merging pass
    uint32_t m_radiance3DShader = 0;     // Final 3D radiance calculation
    uint32_t m_extractFloorShader = 0;   // Extract 2D slices from 3D volume

    // Shader storage buffer for lights
    uint32_t m_lightSSBO = 0;

    // Uniform buffer for cascade parameters
    uint32_t m_paramsUBO = 0;
};

} // namespace Vehement2
