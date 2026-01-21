#pragma once

#include "Shader.hpp"
#include "Framebuffer.hpp"
#include "../spatial/SDFBVH.hpp"
#include <memory>
#include <glm/glm.hpp>
#include <vector>

namespace Nova {

class SDFModel;
class Camera;
class Texture;
class RadianceCascade;
}


namespace Engine {
class SpectralRenderer;
}

namespace Nova {

/**
 * @brief Render parameters for SDF models
 */
struct SDFRenderSettings {
    // Raymarching settings
    int maxSteps = 128;
    float maxDistance = 100.0f;
    float hitThreshold = 0.001f;

    // Quality settings
    bool enableShadows = true;
    bool enableAO = true;
    bool enableReflections = false;

    // Shadow settings
    float shadowSoftness = 8.0f;
    int shadowSteps = 32;

    // Ambient occlusion settings
    int aoSteps = 5;
    float aoDistance = 0.5f;
    float aoIntensity = 0.5f;

    // Lighting
    glm::vec3 lightDirection{0.5f, -1.0f, 0.5f};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
    float lightIntensity = 1.0f;

    // Background
    glm::vec3 backgroundColor{0.1f, 0.1f, 0.15f};
    bool useEnvironmentMap = false;
};

/**
 * @brief GPU buffer data for SDF primitives
 */
struct SDFPrimitiveData {
    glm::mat4 transform;            // 64 bytes
    glm::mat4 inverseTransform;     // 64 bytes
    glm::vec4 parameters;           // radius, dimensions.x, dimensions.y, dimensions.z
    glm::vec4 parameters2;          // height, topRadius, bottomRadius, cornerRadius
    glm::vec4 parameters3;          // majorRadius, minorRadius, smoothness, sides (as float)
    glm::vec4 parameters4;          // onionThickness, shellMinY, shellMaxY, flags
    glm::vec4 material;             // metallic, roughness, emissive, unused
    glm::vec4 baseColor;
    glm::vec4 emissiveColor;        // rgb + padding
    glm::vec4 boundingSphere;       // xyz = world center, w = bounding radius (for early-out)
    int type;                       // SDFPrimitiveType
    int csgOperation;               // CSGOperation
    int visible;
    int parentIndex;                // -1 for root, >= 0 for child primitive index
};

/**
 * @brief Flags for SDF primitive features (stored in parameters4.w as float bits)
 */
enum SDFPrimitiveFlags : uint32_t {
    SDF_FLAG_NONE = 0,
    SDF_FLAG_ONION = 1 << 0,           // Enable onion shell
    SDF_FLAG_SHELL_BOUNDED = 1 << 1,   // Apply Y-axis bounds to shell
    SDF_FLAG_HOLLOW = 1 << 2,          // Hollow interior
    SDF_FLAG_FBM_DETAIL = 1 << 3,      // Add FBM surface detail
};

/**
 * @brief GPU data for BVH node (flat array layout for shader access)
 */
struct SDFBVHNodeGPU {
    glm::vec4 boundsMin;            // xyz = AABB min, w = leftFirst/firstPrimitive
    glm::vec4 boundsMax;            // xyz = AABB max, w = primitiveCount (0 = internal)
    int leftChild;                  // Left child index (internal) or first primitive (leaf)
    int rightChild;                 // Right child index (internal only)
    int primitiveCount;             // 0 = internal node, >0 = leaf with N primitives
    int padding;
};

/**
 * @brief SDF Renderer using GPU raymarching
 *
 * Renders SDF models using compute/fragment shader raymarching.
 * Supports all primitive types, CSG operations, and PBR materials.
 */
class SDFRenderer {
public:
    SDFRenderer();
    ~SDFRenderer();

    // Non-copyable
    SDFRenderer(const SDFRenderer&) = delete;
    SDFRenderer& operator=(const SDFRenderer&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize renderer
     */
    bool Initialize();

    /**
     * @brief Shutdown renderer
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render SDF model
     */
    void Render(const SDFModel& model, const Camera& camera, const glm::mat4& modelTransform = glm::mat4(1.0f));

    /**
     * @brief Render SDF model to texture
     */
    void RenderToTexture(const SDFModel& model, const Camera& camera,
                         std::shared_ptr<Framebuffer> framebuffer,
                         const glm::mat4& modelTransform = glm::mat4(1.0f));

    /**
     * @brief Render multiple SDF models (instanced)
     */
    void RenderBatch(const std::vector<const SDFModel*>& models,
                     const std::vector<glm::mat4>& transforms,
                     const Camera& camera);

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Get render settings
     */
    [[nodiscard]] SDFRenderSettings& GetSettings() { return m_settings; }
    [[nodiscard]] const SDFRenderSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Set render settings
     */
    void SetSettings(const SDFRenderSettings& settings) { m_settings = settings; }

    /**
     * @brief Get shader
     */
    [[nodiscard]] Shader* GetShader() { return m_raymarchShader.get(); }

    // =========================================================================
    // Environment
    // =========================================================================

    /**
     * @brief Set environment map
     */
    void SetEnvironmentMap(std::shared_ptr<Texture> envMap);

    /**
     * @brief Get environment map
     */
    [[nodiscard]] std::shared_ptr<Texture> GetEnvironmentMap() const { return m_environmentMap; }

    // =========================================================================
    // Global Illumination
    // =========================================================================

    /**
     * @brief Set radiance cascade system
     */
    void SetRadianceCascade(std::shared_ptr<Nova::RadianceCascade> cascade);

    /**
     * @brief Enable/disable global illumination
     */
    void SetGlobalIlluminationEnabled(bool enabled) { m_enableGI = enabled; }
    bool IsGlobalIlluminationEnabled() const { return m_enableGI; }

    /**
     * @brief Set spectral rendering mode
     */
    void SetSpectralMode(int mode) { m_spectralMode = mode; }

    /**
     * @brief Enable advanced optics features
     */
    void SetDispersionEnabled(bool enabled) { m_enableDispersion = enabled; }
    void SetDiffractionEnabled(bool enabled) { m_enableDiffraction = enabled; }
    void SetBlackbodyEnabled(bool enabled) { m_enableBlackbody = enabled; }

    // =========================================================================
    // Compute Shader Rendering
    // =========================================================================

    /**
     * @brief Enable/disable compute shader rendering path
     * Compute path offers better GPU utilization for complex scenes
     */
    void SetComputeRenderingEnabled(bool enabled) { m_useComputeShader = enabled; }
    bool IsComputeRenderingEnabled() const { return m_useComputeShader; }

    /**
     * @brief Render using compute shader to output texture
     */
    void RenderCompute(const SDFModel& model, const Camera& camera,
                       unsigned int outputTexture, int width, int height,
                       const glm::mat4& modelTransform = glm::mat4(1.0f));

    // =========================================================================
    // BVH Acceleration
    // =========================================================================

    /**
     * @brief Enable/disable BVH acceleration for scene traversal
     * BVH provides early-out culling for complex scenes with many primitives
     */
    void SetBVHAccelerationEnabled(bool enabled) { m_useBVH = enabled; }
    bool IsBVHAccelerationEnabled() const { return m_useBVH; }

    // =========================================================================
    // SDF Cache (Brick-Map)
    // =========================================================================

    /**
     * @brief Set cached SDF 3D texture for fast evaluation
     * The texture should contain signed distance values in the R channel
     */
    void SetCacheTexture(unsigned int texture3D, const glm::vec3& boundsMin,
                         const glm::vec3& boundsMax, int resolution);

    /**
     * @brief Clear cached SDF texture
     */
    void ClearCacheTexture();

    /**
     * @brief Enable/disable cached SDF evaluation
     */
    void SetCachedSDFEnabled(bool enabled) { m_useCachedSDF = enabled; }
    bool IsCachedSDFEnabled() const { return m_useCachedSDF; }

    /**
     * @brief Build and upload cache texture from model
     * Creates a 3D texture with sampled SDF values for fast GPU evaluation
     * @param resolution Cache resolution (e.g., 64, 128, or 256)
     */
    void BuildCacheFromModel(const SDFModel& model, int resolution = 128);

private:
    // Upload model data to GPU
    void UploadModelData(const SDFModel& model, const glm::mat4& modelTransform);

    // Setup shader uniforms
    void SetupUniforms(const Camera& camera);

    // Create fullscreen quad for raymarching
    void CreateFullscreenQuad();

    bool m_initialized = false;

    // Shaders
    std::unique_ptr<Shader> m_raymarchShader;
    std::unique_ptr<Shader> m_computeShader;
    bool m_useComputeShader = false;

    // Render settings
    SDFRenderSettings m_settings;

    // GPU buffers
    unsigned int m_primitivesSSBO = 0;
    unsigned int m_fullscreenVAO = 0;
    unsigned int m_fullscreenVBO = 0;

    // Environment
    std::shared_ptr<Texture> m_environmentMap;

    // Stats
    int m_lastPrimitiveCount = 0;
    size_t m_maxPrimitives = 2560;  // 10x increase for high-detail models

    // Global illumination
    std::shared_ptr<Nova::RadianceCascade> m_radianceCascade;
    bool m_enableGI = true;

    // Spectral rendering
    std::unique_ptr<Engine::SpectralRenderer> m_spectralRenderer;
    int m_spectralMode = 2;  // HeroWavelength mode

    // Advanced optics
    bool m_enableDispersion = true;
    bool m_enableDiffraction = false;  // Expensive
    bool m_enableBlackbody = true;

    // BVH acceleration structure
    SDFBVH m_bvh;
    unsigned int m_bvhSSBO = 0;
    unsigned int m_bvhPrimitiveIndicesSSBO = 0;
    bool m_useBVH = true;  // Enabled by default for large primitive counts
    int m_bvhNodeCount = 0;

    // Build BVH from current primitives
    void BuildBVH(const std::vector<SDFPrimitiveData>& primitives);
    void UploadBVHToGPU();

    // SDF Cache (Brick-Map) for 100+ primitive characters
    unsigned int m_cacheTexture3D = 0;
    glm::vec3 m_cacheBoundsMin{0.0f};
    glm::vec3 m_cacheBoundsMax{0.0f};
    int m_cacheResolution = 0;
    bool m_useCachedSDF = false;
    bool m_ownsCacheTexture = false;  // Whether we created the texture ourselves

    // Black body radiation helpers
    glm::vec3 CalculateBlackbodyColor(float temperature);
    float CalculateBlackbodyIntensity(float temperature);
};

} // namespace Nova
