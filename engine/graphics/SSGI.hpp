#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Nova {

// Forward declarations
class Shader;
class Camera;
class Framebuffer;

/**
 * @brief SSGI technique selection
 */
enum class SSGITechnique {
    GTAO,              // Ground Truth Ambient Occlusion
    SSAO,              // Screen Space Ambient Occlusion
    SSR,               // Screen Space Reflections
    FullGI             // Combined AO + reflections + indirect lighting
};

/**
 * @brief SSGI quality preset
 */
enum class SSGIQuality {
    Low,               // 4 samples, no temporal, minimal denoising
    Medium,            // 8 samples, temporal, spatial denoise
    High,              // 16 samples, temporal, spatial + bilateral denoise
    Ultra              // 32 samples, temporal, full denoising pipeline
};

/**
 * @brief Configuration for SSGI
 */
struct SSGIConfig {
    SSGITechnique technique = SSGITechnique::FullGI;
    SSGIQuality quality = SSGIQuality::Medium;

    // GTAO/SSAO settings
    int aoSamples = 8;
    float aoRadius = 0.5f;
    float aoIntensity = 1.0f;
    float aoBias = 0.025f;
    bool aoMultiBounce = true;

    // SSR settings
    int ssrSteps = 32;
    int ssrBinarySearchSteps = 4;
    float ssrMaxDistance = 50.0f;
    float ssrThickness = 0.5f;
    bool ssrRoughness = true;

    // Indirect lighting settings
    int giSamples = 8;
    float giRadius = 2.0f;
    float giIntensity = 1.0f;

    // Denoising
    bool temporalFilter = true;
    bool spatialFilter = true;
    int spatialRadius = 2;
    float temporalAlpha = 0.1f;

    // Performance
    float renderScale = 1.0f;  // Resolution scale (0.5 = half res)
};

/**
 * @brief Screen-Space Global Illumination
 *
 * Implements real-time GI using screen-space techniques:
 * - GTAO for high-quality ambient occlusion
 * - SSR for reflections with roughness support
 * - Screen-space indirect lighting
 * - Temporal and spatial denoising
 *
 * Features:
 * - Real-time performance (2-5ms per frame)
 * - Temporal stability through reprojection
 * - Multi-bounce AO approximation
 * - Contact shadows for high-frequency detail
 * - Configurable quality/performance trade-offs
 */
class SSGI {
public:
    SSGI();
    ~SSGI();

    // Non-copyable
    SSGI(const SSGI&) = delete;
    SSGI& operator=(const SSGI&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize SSGI system
     */
    bool Initialize(int width, int height, const SSGIConfig& config);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Resize for new resolution
     */
    void Resize(int width, int height);

    /**
     * @brief Reconfigure system
     */
    bool Reconfigure(const SSGIConfig& config);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render SSGI effects
     * @param camera View camera
     * @param depthTexture Scene depth texture
     * @param normalTexture Scene normal texture (view space)
     * @param colorTexture Scene color texture
     * @return Output texture with GI applied
     */
    uint32_t Render(const Camera& camera, uint32_t depthTexture,
                   uint32_t normalTexture, uint32_t colorTexture);

    /**
     * @brief Get AO texture only
     */
    uint32_t GetAOTexture() const;

    /**
     * @brief Get SSR texture only
     */
    uint32_t GetSSRTexture() const;

    /**
     * @brief Get indirect lighting texture
     */
    uint32_t GetIndirectLightingTexture() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    const SSGIConfig& GetConfig() const { return m_config; }

    /**
     * @brief Apply quality preset
     */
    void ApplyQualityPreset(SSGIQuality quality);

    /**
     * @brief Enable/disable specific technique
     */
    void EnableTechnique(SSGITechnique technique, bool enabled);

    /**
     * @brief Check if technique is enabled
     */
    bool IsTechniqueEnabled(SSGITechnique technique) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        float totalTimeMs = 0.0f;
        float aoTimeMs = 0.0f;
        float ssrTimeMs = 0.0f;
        float giTimeMs = 0.0f;
        float denoiseTimeMs = 0.0f;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    // Render individual effects
    void RenderGTAO(const Camera& camera, uint32_t depthTexture, uint32_t normalTexture);
    void RenderSSR(const Camera& camera, uint32_t depthTexture, uint32_t normalTexture,
                  uint32_t colorTexture);
    void RenderIndirectLighting(const Camera& camera, uint32_t depthTexture,
                               uint32_t normalTexture, uint32_t colorTexture);

    // Denoising
    void ApplyTemporalFilter(uint32_t currentFrame, uint32_t motionVectors);
    void ApplySpatialFilter(uint32_t inputTexture, uint32_t outputTexture);

    // Create resources
    bool CreateResources();
    void DestroyResources();

    // Generate noise textures
    void GenerateNoiseTextures();

    bool m_initialized = false;
    SSGIConfig m_config;

    // Dimensions
    int m_width = 0;
    int m_height = 0;
    int m_renderWidth = 0;
    int m_renderHeight = 0;

    // Framebuffers
    std::unique_ptr<Framebuffer> m_aoFramebuffer;
    std::unique_ptr<Framebuffer> m_ssrFramebuffer;
    std::unique_ptr<Framebuffer> m_giFramebuffer;
    std::unique_ptr<Framebuffer> m_outputFramebuffer;
    std::unique_ptr<Framebuffer> m_temporalFramebuffer;
    std::unique_ptr<Framebuffer> m_historyFramebuffer;

    // Textures
    uint32_t m_noiseTexture = 0;
    uint32_t m_rotationTexture = 0;

    // Shaders
    std::unique_ptr<Shader> m_gtaoShader;
    std::unique_ptr<Shader> m_ssrShader;
    std::unique_ptr<Shader> m_giShader;
    std::unique_ptr<Shader> m_temporalShader;
    std::unique_ptr<Shader> m_spatialShader;
    std::unique_ptr<Shader> m_combineShader;

    // State
    int m_frameIndex = 0;
    glm::mat4 m_prevViewProj;
    glm::mat4 m_prevInvViewProj;

    // Enabled techniques
    bool m_enableAO = true;
    bool m_enableSSR = true;
    bool m_enableGI = true;

    // Statistics
    Stats m_stats;
};

} // namespace Nova
