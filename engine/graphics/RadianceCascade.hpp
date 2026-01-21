#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {

class Shader;
class Texture;
class Framebuffer;
class Renderer;

/**
 * @brief Radiance Cascade Global Illumination System
 *
 * Implements radiance cascades for real-time global illumination:
 * - Multi-level 3D radiance cache
 * - Dynamic indirect lighting
 * - Works with meshes, SDFs, and terrain
 * - Supports emissive materials
 * - Fast light propagation with cascaded resolution
 *
 * Reference: "Radiance Cascades" by Alexander Sannikov
 */
class RadianceCascade {
public:
    struct Config {
        // Cascade configuration
        int numCascades = 4;                    // Number of cascade levels
        int baseResolution = 32;                // Resolution of finest cascade
        float cascadeScale = 2.0f;              // Scale factor between cascades

        // Spatial configuration
        glm::vec3 origin{0.0f};                 // World space origin
        float baseSpacing = 1.0f;               // Spacing of finest cascade
        float updateRadius = 100.0f;            // Max distance from camera to update

        // Quality settings
        int raysPerProbe = 64;                  // Rays per probe for propagation
        int bounces = 2;                        // Number of light bounces
        bool useInterpolation = true;           // Trilinear interpolation

        // Performance
        bool asyncUpdate = true;                // Update on separate thread
        int maxProbesPerFrame = 1024;           // Max probes to update per frame
        float temporalBlend = 0.95f;            // Temporal stability (0=no history, 1=all history)
    };

    RadianceCascade();
    ~RadianceCascade();

    /**
     * @brief Initialize the radiance cascade system
     */
    bool Initialize(const Config& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update radiance cascade from camera position
     */
    void Update(const glm::vec3& cameraPosition, float deltaTime);

    /**
     * @brief Inject direct lighting into cascade
     */
    void InjectDirectLighting(const std::vector<glm::vec3>& positions,
                             const std::vector<glm::vec3>& radiance);

    /**
     * @brief Inject emissive geometry into cascade
     */
    void InjectEmissive(const glm::vec3& position, const glm::vec3& radiance, float radius);

    /**
     * @brief Propagate light through cascades
     */
    void PropagateLighting();

    /**
     * @brief Sample radiance at world position
     */
    glm::vec3 SampleRadiance(const glm::vec3& worldPos, const glm::vec3& normal) const;

    /**
     * @brief Get cascade texture for binding to shaders
     */
    uint32_t GetCascadeTexture(int level) const;

    /**
     * @brief Get all cascade textures
     */
    const std::vector<uint32_t>& GetCascadeTextures() const { return m_cascadeTextures; }

    /**
     * @brief Get cascade origin for shaders
     */
    const glm::vec3& GetOrigin() const { return m_config.origin; }

    /**
     * @brief Get cascade spacing for shaders
     */
    float GetBaseSpacing() const { return m_config.baseSpacing; }

    /**
     * @brief Get configuration
     */
    const Config& GetConfig() const { return m_config; }
    void SetConfig(const Config& config);

    /**
     * @brief Enable/disable system
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Clear all radiance data
     */
    void Clear();

    /**
     * @brief Debug visualization
     */
    void DebugDraw(Renderer* renderer);

    /**
     * @brief Get statistics
     */
    struct Stats {
        int totalProbes = 0;
        int activeProbes = 0;
        int probesUpdatedThisFrame = 0;
        float updateTimeMs = 0.0f;
        float propagationTimeMs = 0.0f;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    /**
     * @brief Single cascade level
     */
    struct CascadeLevel {
        int resolution;                         // 3D texture resolution
        float spacing;                          // Probe spacing in world units
        glm::vec3 origin;                       // Origin offset

        // 3D texture storing radiance (RGB) + validity (A)
        uint32_t radianceTexture = 0;           // Current frame
        uint32_t radianceTextureHistory = 0;    // Previous frame

        // Probe data
        std::vector<glm::vec3> probePositions;
        std::vector<float> probeValidity;       // 0=invalid, 1=valid
        std::vector<bool> needsUpdate;
    };

    void InitializeCascade(CascadeLevel& cascade, int resolution, float spacing);
    void UpdateCascadeOrigin(const glm::vec3& cameraPosition);
    void UpdateProbes(CascadeLevel& cascade, int maxProbes);
    void PropagateLevel(int level);
    void PropagateLevelCPU(int level);
    bool LoadPropagationShader();
    glm::ivec3 WorldToGrid(const glm::vec3& worldPos, const CascadeLevel& cascade) const;
    glm::vec3 GridToWorld(const glm::ivec3& gridPos, const CascadeLevel& cascade) const;
    int GetProbeIndex(const glm::ivec3& gridPos, const CascadeLevel& cascade) const;

    // Rendering helpers
    void TraceProbeRays(const glm::vec3& probePos, std::vector<glm::vec3>& outRadiance);
    glm::vec3 SampleCascade(const glm::vec3& worldPos, int cascadeLevel) const;

    Config m_config;
    std::vector<CascadeLevel> m_cascades;
    std::vector<uint32_t> m_cascadeTextures;

    // Shaders
    std::shared_ptr<Shader> m_propagationShader;
    std::shared_ptr<Shader> m_injectionShader;
    std::shared_ptr<Shader> m_samplingShader;

    Stats m_stats;
    bool m_initialized = false;
    bool m_enabled = true;
    float m_time = 0.0f;
    int m_frameIndex = 0;
};

} // namespace Nova
