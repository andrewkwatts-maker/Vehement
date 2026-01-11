#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Nova {

// Forward declarations
class Shader;
class Camera;
class Framebuffer;

/**
 * @brief Configuration for volumetric lighting
 */
struct VolumetricConfig {
    // Volume texture resolution
    int volumeWidth = 160;      // Screen width / 8
    int volumeHeight = 90;      // Screen height / 8
    int volumeDepth = 128;      // Depth slices

    // Raymarch settings
    int numSteps = 64;
    float scattering = 0.5f;
    float absorption = 0.1f;
    float density = 0.5f;

    // Quality
    bool temporalFilter = true;
    float temporalAlpha = 0.1f;
    bool jitter = true;          // Temporal jitter for smoother results

    // Lighting
    float lightScattering = 0.8f;
    float ambientFog = 0.02f;

    // Performance
    float maxDistance = 100.0f;
    bool halfResolution = true;
};

/**
 * @brief Volumetric Lighting
 *
 * Implements volumetric fog and light shafts:
 * - Ray marching through 3D volume texture
 * - Scattering and absorption simulation
 * - Integration with shadow maps
 * - Temporal reprojection for stability
 * - SDF support for volumetric shadows
 *
 * Features:
 * - Real-time god rays/light shafts
 * - Atmospheric fog with depth
 * - Per-light volumetric contribution
 * - <1ms performance at quarter resolution
 * - Temporal stability through reprojection
 */
class VolumetricLighting {
public:
    VolumetricLighting();
    ~VolumetricLighting();

    // Non-copyable
    VolumetricLighting(const VolumetricLighting&) = delete;
    VolumetricLighting& operator=(const VolumetricLighting&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize volumetric lighting system
     */
    bool Initialize(int screenWidth, int screenHeight, const VolumetricConfig& config);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Resize for new screen resolution
     */
    void Resize(int screenWidth, int screenHeight);

    /**
     * @brief Reconfigure system
     */
    bool Reconfigure(const VolumetricConfig& config);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render volumetric lighting
     * @param camera View camera
     * @param depthTexture Scene depth texture
     * @param shadowMap Shadow map texture (optional)
     * @return 3D volume texture with lighting
     */
    uint32_t Render(const Camera& camera, uint32_t depthTexture,
                   uint32_t shadowMap = 0);

    /**
     * @brief Apply volumetrics to scene (composite step)
     * @param sceneColor Scene color texture
     * @param depthTexture Scene depth texture
     * @param volumeTexture Result from Render()
     * @return Final composited texture
     */
    uint32_t ApplyToScene(uint32_t sceneColor, uint32_t depthTexture, uint32_t volumeTexture);

    /**
     * @brief Get volumetric texture
     */
    uint32_t GetVolumeTexture() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    const VolumetricConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set fog density
     */
    void SetDensity(float density) { m_config.density = density; }

    /**
     * @brief Set scattering coefficient
     */
    void SetScattering(float scattering) { m_config.scattering = scattering; }

    /**
     * @brief Set light direction (for directional lights)
     */
    void SetLightDirection(const glm::vec3& direction);

    /**
     * @brief Set light color
     */
    void SetLightColor(const glm::vec3& color);

    // =========================================================================
    // Light Sources
    // =========================================================================

    /**
     * @brief Add point light for volumetric contribution
     */
    void AddPointLight(const glm::vec3& position, const glm::vec3& color,
                      float intensity, float range);

    /**
     * @brief Clear all volumetric lights
     */
    void ClearLights();

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        float renderTimeMs = 0.0f;
        float temporalFilterTimeMs = 0.0f;
        int volumeTexelsProcessed = 0;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    // Create 3D volume texture
    bool CreateVolumeTexture();

    // Destroy resources
    void DestroyResources();

    // Generate temporal jitter
    glm::vec3 GenerateJitter();

    bool m_initialized = false;
    VolumetricConfig m_config;

    // Dimensions
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    // Volume texture (3D)
    uint32_t m_volumeTexture = 0;
    uint32_t m_historyVolumeTexture = 0;

    // Framebuffers
    std::unique_ptr<Framebuffer> m_compositeFramebuffer;

    // Shaders
    std::unique_ptr<Shader> m_volumetricShader;      // Compute shader for volume
    std::unique_ptr<Shader> m_temporalShader;        // Temporal reprojection
    std::unique_ptr<Shader> m_compositeShader;       // Final composite

    // Lighting
    glm::vec3 m_lightDirection{0.5f, -1.0f, 0.5f};
    glm::vec3 m_lightColor{1.0f, 1.0f, 1.0f};

    struct VolumetricLight {
        glm::vec4 position;  // xyz = pos, w = range
        glm::vec4 color;     // rgb = color, a = intensity
    };
    std::vector<VolumetricLight> m_lights;

    // State
    int m_frameIndex = 0;
    glm::mat4 m_prevViewProj;

    // Statistics
    Stats m_stats;
};

} // namespace Nova
