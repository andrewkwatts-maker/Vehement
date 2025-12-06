#pragma once

#include "Shader.hpp"
#include "Framebuffer.hpp"
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
    glm::vec4 material;             // metallic, roughness, emissive, unused
    glm::vec4 baseColor;
    glm::vec4 emissiveColor;        // rgb + padding
    int type;                       // SDFPrimitiveType
    int csgOperation;               // CSGOperation
    int visible;
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
    size_t m_maxPrimitives = 256;

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

    // Black body radiation helpers
    glm::vec3 CalculateBlackbodyColor(float temperature);
    float CalculateBlackbodyIntensity(float temperature);
};

} // namespace Nova
