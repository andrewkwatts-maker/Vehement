#pragma once

#include "GBuffer.hpp"
#include "Shader.hpp"
#include "Framebuffer.hpp"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Camera;
class Mesh;
class Material;
class SDFRenderer;
class Texture;

// ============================================================================
// Light Types
// ============================================================================

/**
 * @brief Light type enumeration
 */
enum class LightType : uint8_t {
    Directional = 0,
    Point = 1,
    Spot = 2,
    Area = 3          // Future: Area lights
};

/**
 * @brief Light data structure
 *
 * Unified light representation for all light types.
 * Layout is GPU-friendly for SSBO usage.
 */
struct Light {
    // Core properties (all light types)
    glm::vec3 position{0.0f};           // World-space position (ignored for directional)
    float range = 10.0f;                // Attenuation range (0 for infinite/directional)

    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // Light direction (for directional/spot)
    float intensity = 1.0f;             // Light intensity multiplier

    glm::vec3 color{1.0f};              // RGB color
    LightType type = LightType::Point;

    // Spot light specific
    float innerConeAngle = 30.0f;       // Inner cone angle in degrees
    float outerConeAngle = 45.0f;       // Outer cone angle in degrees

    // Shadow mapping
    bool castsShadows = true;
    int shadowMapIndex = -1;            // Index into shadow map array (-1 = no shadow)

    // Flags
    bool enabled = true;
    uint8_t padding[3]{};               // Alignment padding

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /**
     * @brief Create directional light
     */
    static Light CreateDirectional(
        const glm::vec3& direction,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f
    ) {
        Light light;
        light.type = LightType::Directional;
        light.direction = glm::normalize(direction);
        light.color = color;
        light.intensity = intensity;
        light.range = 0.0f;  // Infinite range
        return light;
    }

    /**
     * @brief Create point light
     */
    static Light CreatePoint(
        const glm::vec3& position,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f,
        float range = 10.0f
    ) {
        Light light;
        light.type = LightType::Point;
        light.position = position;
        light.color = color;
        light.intensity = intensity;
        light.range = range;
        return light;
    }

    /**
     * @brief Create spot light
     */
    static Light CreateSpot(
        const glm::vec3& position,
        const glm::vec3& direction,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f,
        float range = 10.0f,
        float innerAngle = 30.0f,
        float outerAngle = 45.0f
    ) {
        Light light;
        light.type = LightType::Spot;
        light.position = position;
        light.direction = glm::normalize(direction);
        light.color = color;
        light.intensity = intensity;
        light.range = range;
        light.innerConeAngle = innerAngle;
        light.outerConeAngle = outerAngle;
        return light;
    }
};

/**
 * @brief GPU-aligned light data for SSBO
 */
struct alignas(16) GPULightData {
    glm::vec4 positionRange;            // xyz: position, w: range
    glm::vec4 directionIntensity;       // xyz: direction, w: intensity
    glm::vec4 colorType;                // xyz: color, w: type (as float)
    glm::vec4 spotParams;               // x: inner angle cos, y: outer angle cos, z: shadow index, w: enabled

    static GPULightData FromLight(const Light& light);
};

// ============================================================================
// Deferred Renderer Settings
// ============================================================================

/**
 * @brief Configuration for deferred renderer
 */
struct DeferredRendererConfig {
    // Resolution
    int width = 1920;
    int height = 1080;

    // G-Buffer settings
    GBufferConfig gbufferConfig = GBufferConfig::Default();

    // Lighting settings
    int maxLights = 1024;                   // Maximum number of lights
    bool enableClustering = true;           // Use clustered lighting for many lights
    glm::ivec3 clusterDimensions{16, 9, 24}; // Cluster grid size

    // Quality settings
    bool enableSSAO = true;                 // Screen-space ambient occlusion
    bool enableSSR = false;                 // Screen-space reflections
    bool enableBloom = true;                // HDR bloom
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.5f;

    // Tone mapping
    enum class ToneMapper {
        None,
        Reinhard,
        ACES,
        Uncharted2
    };
    ToneMapper toneMapper = ToneMapper::ACES;
    float exposure = 1.0f;
    float gamma = 2.2f;

    // Environment
    bool enableEnvironmentLighting = true;
    float ambientIntensity = 0.1f;
    glm::vec3 ambientColor{0.1f, 0.1f, 0.15f};

    // Shadow settings
    bool enableShadows = true;
    int shadowMapResolution = 2048;
    int maxShadowCastingLights = 4;

    static DeferredRendererConfig Default() {
        return DeferredRendererConfig{};
    }

    static DeferredRendererConfig HighQuality() {
        DeferredRendererConfig config;
        config.gbufferConfig = GBufferConfig::HighQuality();
        config.maxLights = 4096;
        config.enableSSAO = true;
        config.enableSSR = true;
        config.enableBloom = true;
        config.shadowMapResolution = 4096;
        config.maxShadowCastingLights = 8;
        return config;
    }

    static DeferredRendererConfig Performance() {
        DeferredRendererConfig config;
        config.gbufferConfig = GBufferConfig::Performance();
        config.maxLights = 256;
        config.enableSSAO = false;
        config.enableSSR = false;
        config.enableBloom = false;
        config.enableShadows = false;
        return config;
    }
};

// ============================================================================
// Render Statistics
// ============================================================================

/**
 * @brief Performance statistics for deferred renderer
 */
struct DeferredRenderStats {
    // Timing (milliseconds)
    float geometryPassTime = 0.0f;
    float lightingPassTime = 0.0f;
    float compositePassTime = 0.0f;
    float postProcessTime = 0.0f;
    float totalFrameTime = 0.0f;

    // Counts
    uint32_t objectsRendered = 0;
    uint32_t trianglesRendered = 0;
    uint32_t activeLights = 0;
    uint32_t shadowCastingLights = 0;

    // Memory
    size_t gbufferMemory = 0;
    size_t lightBufferMemory = 0;

    void Reset() {
        geometryPassTime = 0.0f;
        lightingPassTime = 0.0f;
        compositePassTime = 0.0f;
        postProcessTime = 0.0f;
        totalFrameTime = 0.0f;
        objectsRendered = 0;
        trianglesRendered = 0;
        activeLights = 0;
        shadowCastingLights = 0;
    }
};

// ============================================================================
// Deferred Renderer
// ============================================================================

/**
 * @brief Deferred Rendering Pipeline
 *
 * Implements a full deferred shading pipeline with:
 * - G-Buffer geometry pass (Position, Normal, Albedo, Material)
 * - Lighting pass with support for hundreds of lights
 * - Optional clustered lighting for thousands of lights
 * - Screen-space effects (SSAO, SSR, Bloom)
 * - HDR rendering with tone mapping
 * - Integration with SDFRenderer for hybrid rendering
 *
 * Usage:
 * @code
 *     DeferredRenderer renderer;
 *     renderer.Initialize(config);
 *
 *     // Each frame:
 *     renderer.BeginFrame(camera);
 *
 *     // Geometry pass
 *     renderer.BeginGeometryPass();
 *     for (auto& object : objects) {
 *         renderer.SubmitMesh(object.mesh, object.material, object.transform);
 *     }
 *     renderer.EndGeometryPass();
 *
 *     // Add lights
 *     renderer.SetLights(lights);
 *
 *     // Lighting pass
 *     renderer.LightingPass();
 *
 *     // Composite and post-process
 *     renderer.Composite();
 *
 *     renderer.EndFrame();
 * @endcode
 */
class DeferredRenderer {
public:
    DeferredRenderer();
    ~DeferredRenderer();

    // Non-copyable
    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the deferred renderer
     * @param config Renderer configuration
     * @return true if initialization succeeded
     */
    bool Initialize(const DeferredRendererConfig& config = DeferredRendererConfig::Default());

    /**
     * @brief Initialize with dimensions only
     * @param width Viewport width
     * @param height Viewport height
     * @return true if initialization succeeded
     */
    bool Initialize(int width, int height);

    /**
     * @brief Shutdown and cleanup all resources
     */
    void Shutdown();

    /**
     * @brief Check if renderer is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Resize all buffers
     * @param width New width
     * @param height New height
     */
    void Resize(int width, int height);

    // =========================================================================
    // Frame Management
    // =========================================================================

    /**
     * @brief Begin a new frame
     * @param camera Active camera for this frame
     */
    void BeginFrame(const Camera& camera);

    /**
     * @brief End the current frame
     */
    void EndFrame();

    // =========================================================================
    // Geometry Pass
    // =========================================================================

    /**
     * @brief Begin geometry pass (fill G-Buffer)
     */
    void BeginGeometryPass();

    /**
     * @brief Submit mesh for rendering in geometry pass
     * @param mesh Mesh to render
     * @param material Material for shading
     * @param transform World transform matrix
     */
    void SubmitMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform);

    /**
     * @brief Submit mesh with custom shader
     * @param mesh Mesh to render
     * @param shader Custom G-Buffer shader
     * @param transform World transform matrix
     */
    void SubmitMeshCustomShader(const Mesh& mesh, Shader& shader, const glm::mat4& transform);

    /**
     * @brief End geometry pass
     */
    void EndGeometryPass();

    // =========================================================================
    // Lighting Pass
    // =========================================================================

    /**
     * @brief Execute lighting pass
     *
     * Reads from G-Buffer and computes lighting for all pixels.
     * Uses screen-space quad rendering with per-pixel lighting.
     */
    void LightingPass();

    /**
     * @brief Execute lighting pass with custom shader
     * @param shader Custom lighting shader
     */
    void LightingPass(Shader& shader);

    // =========================================================================
    // Composite Pass
    // =========================================================================

    /**
     * @brief Composite final image with post-processing
     *
     * Applies:
     * - SSAO (if enabled)
     * - SSR (if enabled)
     * - Bloom (if enabled)
     * - Tone mapping
     * - Gamma correction
     */
    void Composite();

    /**
     * @brief Composite to custom framebuffer
     * @param targetFBO Target framebuffer (0 for default)
     */
    void CompositeToFramebuffer(uint32_t targetFBO);

    // =========================================================================
    // Light Management
    // =========================================================================

    /**
     * @brief Set all lights for this frame
     * @param lights Vector of lights
     */
    void SetLights(const std::vector<Light>& lights);

    /**
     * @brief Add a single light
     * @param light Light to add
     * @return Index of added light
     */
    uint32_t AddLight(const Light& light);

    /**
     * @brief Update existing light
     * @param index Light index
     * @param light New light data
     */
    void UpdateLight(uint32_t index, const Light& light);

    /**
     * @brief Remove light
     * @param index Light index to remove
     */
    void RemoveLight(uint32_t index);

    /**
     * @brief Clear all lights
     */
    void ClearLights();

    /**
     * @brief Get current light count
     */
    [[nodiscard]] uint32_t GetLightCount() const { return static_cast<uint32_t>(m_lights.size()); }

    /**
     * @brief Get light at index
     */
    [[nodiscard]] const Light& GetLight(uint32_t index) const { return m_lights[index]; }

    // =========================================================================
    // Environment & Ambient
    // =========================================================================

    /**
     * @brief Set environment map for IBL
     * @param envMap Environment cubemap
     * @param irradianceMap Irradiance map (diffuse IBL)
     * @param prefilteredMap Prefiltered map (specular IBL)
     * @param brdfLUT BRDF lookup texture
     */
    void SetEnvironmentMaps(
        std::shared_ptr<Texture> envMap,
        std::shared_ptr<Texture> irradianceMap = nullptr,
        std::shared_ptr<Texture> prefilteredMap = nullptr,
        std::shared_ptr<Texture> brdfLUT = nullptr
    );

    /**
     * @brief Set ambient color
     */
    void SetAmbientColor(const glm::vec3& color) { m_config.ambientColor = color; }

    /**
     * @brief Set ambient intensity
     */
    void SetAmbientIntensity(float intensity) { m_config.ambientIntensity = intensity; }

    // =========================================================================
    // SDF Integration
    // =========================================================================

    /**
     * @brief Set SDF renderer for hybrid rendering
     * @param sdfRenderer Pointer to SDFRenderer (can be null)
     */
    void SetSDFRenderer(SDFRenderer* sdfRenderer) { m_sdfRenderer = sdfRenderer; }

    /**
     * @brief Render SDF objects into G-Buffer
     *
     * This allows mixing traditional mesh rendering with SDF raymarching.
     */
    void RenderSDFToGBuffer();

    /**
     * @brief Merge SDF depth with G-Buffer depth
     * @param sdfDepthTexture Depth texture from SDF rendering
     */
    void MergeSDFDepth(uint32_t sdfDepthTexture);

    // =========================================================================
    // Access
    // =========================================================================

    /**
     * @brief Get G-Buffer
     */
    [[nodiscard]] GBuffer& GetGBuffer() { return m_gbuffer; }
    [[nodiscard]] const GBuffer& GetGBuffer() const { return m_gbuffer; }

    /**
     * @brief Get final output texture
     */
    [[nodiscard]] uint32_t GetOutputTexture() const { return m_outputTexture; }

    /**
     * @brief Get lighting result texture (before post-processing)
     */
    [[nodiscard]] uint32_t GetLightingTexture() const { return m_lightingTexture; }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const DeferredRendererConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get render statistics
     */
    [[nodiscard]] const DeferredRenderStats& GetStats() const { return m_stats; }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set exposure for tone mapping
     */
    void SetExposure(float exposure) { m_config.exposure = exposure; }

    /**
     * @brief Set gamma for gamma correction
     */
    void SetGamma(float gamma) { m_config.gamma = gamma; }

    /**
     * @brief Set tone mapper
     */
    void SetToneMapper(DeferredRendererConfig::ToneMapper toneMapper) {
        m_config.toneMapper = toneMapper;
    }

    /**
     * @brief Enable/disable SSAO
     */
    void SetSSAOEnabled(bool enabled) { m_config.enableSSAO = enabled; }

    /**
     * @brief Enable/disable bloom
     */
    void SetBloomEnabled(bool enabled) { m_config.enableBloom = enabled; }

    /**
     * @brief Set bloom parameters
     */
    void SetBloomParams(float threshold, float intensity) {
        m_config.bloomThreshold = threshold;
        m_config.bloomIntensity = intensity;
    }

    // =========================================================================
    // Debug
    // =========================================================================

    /**
     * @brief Visualize G-Buffer attachment
     * @param attachment Which buffer to visualize
     */
    void DebugVisualizeGBuffer(GBufferAttachment attachment);

    /**
     * @brief Visualize light volumes (debug)
     */
    void DebugVisualizeLights();

    /**
     * @brief Get geometry shader for G-Buffer pass
     */
    [[nodiscard]] Shader* GetGeometryShader() { return m_geometryShader.get(); }

    /**
     * @brief Get lighting shader
     */
    [[nodiscard]] Shader* GetLightingShader() { return m_lightingShader.get(); }

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    bool CreateShaders();
    bool CreateFramebuffers();
    bool CreateLightBuffers();
    void CreateFullscreenQuad();

    void UploadLightsToGPU();
    void UpdateClusterData();

    void ApplySSAO();
    void ApplySSR();
    void ApplyBloom();
    void ApplyToneMapping();

    void SetupGeometryShaderUniforms(const Material& material, const glm::mat4& transform);
    void SetupLightingShaderUniforms();

    // =========================================================================
    // State
    // =========================================================================

    bool m_initialized = false;
    DeferredRendererConfig m_config;
    DeferredRenderStats m_stats;

    // G-Buffer
    GBuffer m_gbuffer;

    // Output buffers
    uint32_t m_lightingFBO = 0;
    uint32_t m_lightingTexture = 0;
    uint32_t m_compositeFBO = 0;
    uint32_t m_outputTexture = 0;

    // Bloom buffers
    std::vector<uint32_t> m_bloomFBOs;
    std::vector<uint32_t> m_bloomTextures;

    // SSAO buffers
    uint32_t m_ssaoFBO = 0;
    uint32_t m_ssaoTexture = 0;
    uint32_t m_ssaoNoiseTexture = 0;
    std::vector<glm::vec3> m_ssaoKernel;

    // Shaders
    std::unique_ptr<Shader> m_geometryShader;
    std::unique_ptr<Shader> m_lightingShader;
    std::unique_ptr<Shader> m_compositeShader;
    std::unique_ptr<Shader> m_ssaoShader;
    std::unique_ptr<Shader> m_bloomShader;
    std::unique_ptr<Shader> m_tonemapShader;
    std::unique_ptr<Shader> m_debugShader;

    // Lights
    std::vector<Light> m_lights;
    uint32_t m_lightSSBO = 0;
    bool m_lightsDirty = true;

    // Clustered lighting
    uint32_t m_clusterSSBO = 0;
    uint32_t m_lightIndexSSBO = 0;
    std::unique_ptr<Shader> m_clusterBuildShader;
    std::unique_ptr<Shader> m_clusterCullShader;

    // Environment
    std::shared_ptr<Texture> m_envMap;
    std::shared_ptr<Texture> m_irradianceMap;
    std::shared_ptr<Texture> m_prefilteredMap;
    std::shared_ptr<Texture> m_brdfLUT;

    // SDF integration
    SDFRenderer* m_sdfRenderer = nullptr;

    // Camera data (cached for current frame)
    const Camera* m_currentCamera = nullptr;
    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projMatrix{1.0f};
    glm::mat4 m_viewProjMatrix{1.0f};
    glm::mat4 m_invViewProjMatrix{1.0f};
    glm::vec3 m_cameraPosition{0.0f};

    // Fullscreen quad
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;

    // Timing queries
    uint32_t m_queryObjects[4] = {0, 0, 0, 0};  // geometry, lighting, composite, post
};

} // namespace Nova
