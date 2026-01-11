#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <array>

namespace Nova {

// Forward declarations
class Shader;
class Camera;
class Framebuffer;

/**
 * @brief Shadow map technique
 */
enum class ShadowTechnique {
    Standard,           // Traditional shadow mapping
    VSM,               // Variance Shadow Maps
    PCSS               // Percentage Closer Soft Shadows
};

/**
 * @brief Configuration for cascaded shadow maps
 */
struct CSMConfig {
    int numCascades = 4;
    int shadowMapResolution = 2048;
    ShadowTechnique technique = ShadowTechnique::VSM;
    float lambda = 0.5f;        // Cascade split lambda (0=linear, 1=logarithmic)
    float bias = 0.0005f;       // Shadow bias
    float normalOffset = 0.01f; // Normal offset bias
    bool stabilize = true;      // Stabilize cascades (snap to texels)
    float maxShadowDistance = 100.0f;
};

/**
 * @brief Single cascade data
 */
struct Cascade {
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 viewProjectionMatrix;
    float splitDepth;           // Far plane of this cascade in camera space
    float nearPlane;
    float farPlane;
    glm::vec4 sphere;           // Bounding sphere (xyz=center, w=radius)

    // OpenGL resources
    uint32_t framebuffer = 0;
    uint32_t shadowMap = 0;     // Depth texture (or RG32F for VSM)
    uint32_t blurredShadowMap = 0;  // For VSM blurring
};

/**
 * @brief Cascaded Shadow Maps
 *
 * Implements cascaded shadow mapping with multiple techniques:
 * - Standard shadow mapping with PCF
 * - Variance Shadow Maps (VSM) for soft shadows
 * - PCSS for high-quality soft shadows
 *
 * Features:
 * - Automatic cascade splitting (logarithmic/linear/hybrid)
 * - Cascade stabilization (eliminates shimmering)
 * - Efficient shadow map updates
 * - Support for large open worlds
 * - <2ms performance for 4 cascades at 2K resolution
 */
class CascadedShadowMaps {
public:
    CascadedShadowMaps();
    ~CascadedShadowMaps();

    // Non-copyable
    CascadedShadowMaps(const CascadedShadowMaps&) = delete;
    CascadedShadowMaps& operator=(const CascadedShadowMaps&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize cascaded shadow map system
     */
    bool Initialize(const CSMConfig& config);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Reconfigure system
     */
    bool Reconfigure(const CSMConfig& config);

    // =========================================================================
    // Shadow Map Updates
    // =========================================================================

    /**
     * @brief Update cascade matrices for current frame
     * @param camera View camera
     * @param lightDirection Directional light direction (normalized)
     */
    void UpdateCascades(const Camera& camera, const glm::vec3& lightDirection);

    /**
     * @brief Begin shadow map rendering for a cascade
     * @param cascadeIndex Index of cascade to render (0-based)
     */
    void BeginShadowPass(int cascadeIndex);

    /**
     * @brief End shadow map rendering for current cascade
     */
    void EndShadowPass();

    /**
     * @brief Bind shadow maps for rendering (call before main render pass)
     * @param startTextureUnit Starting texture unit for shadow maps
     */
    void BindForRendering(uint32_t startTextureUnit = 10);

    /**
     * @brief Set shader uniforms for shadow mapping
     */
    void SetShaderUniforms(Shader& shader);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    const CSMConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set light direction
     */
    void SetLightDirection(const glm::vec3& direction);

    /**
     * @brief Get light direction
     */
    const glm::vec3& GetLightDirection() const { return m_lightDirection; }

    /**
     * @brief Set shadow technique
     */
    void SetShadowTechnique(ShadowTechnique technique);

    /**
     * @brief Enable/disable cascade visualization
     */
    void SetDebugVisualization(bool enabled) { m_debugVisualization = enabled; }
    bool IsDebugVisualizationEnabled() const { return m_debugVisualization; }

    // =========================================================================
    // Access
    // =========================================================================

    /**
     * @brief Get number of cascades
     */
    int GetNumCascades() const { return m_config.numCascades; }

    /**
     * @brief Get cascade data
     */
    const Cascade& GetCascade(int index) const { return m_cascades[index]; }

    /**
     * @brief Get all cascades
     */
    const std::vector<Cascade>& GetCascades() const { return m_cascades; }

    /**
     * @brief Get shadow map for cascade
     */
    uint32_t GetShadowMap(int cascadeIndex) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        float updateTimeMs = 0.0f;
        float renderTimeMs = 0.0f;
        int trianglesRendered = 0;
        int drawCalls = 0;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    // Calculate cascade split depths
    void CalculateSplitDepths();

    // Calculate cascade matrices
    void CalculateCascadeMatrices(const Camera& camera);

    // Stabilize cascade (snap to texel grid)
    void StabilizeCascade(Cascade& cascade);

    // Create framebuffers and textures
    bool CreateCascadeResources();

    // Destroy cascade resources
    void DestroyCascadeResources();

    // Apply VSM blur
    void ApplyVSMBlur(int cascadeIndex);

    // Calculate frustum corners
    std::array<glm::vec3, 8> CalculateFrustumCorners(const glm::mat4& viewProj,
                                                      float nearPlane, float farPlane);

    // Calculate bounding sphere for frustum corners
    glm::vec4 CalculateBoundingSphere(const std::array<glm::vec3, 8>& corners);

    bool m_initialized = false;
    CSMConfig m_config;

    // Light
    glm::vec3 m_lightDirection{0.5f, -1.0f, 0.5f};

    // Cascades
    std::vector<Cascade> m_cascades;
    std::vector<float> m_splitDepths;  // Depth splits in camera space
    int m_currentCascade = -1;

    // Shaders
    std::unique_ptr<Shader> m_shadowShader;
    std::unique_ptr<Shader> m_vsmBlurShader;

    // Statistics
    Stats m_stats;

    // Debug
    bool m_debugVisualization = false;

    // Camera cache (for stabilization)
    glm::vec3 m_lastCameraPos{0.0f};
    glm::vec3 m_lastCameraDir{0.0f, 0.0f, -1.0f};
};

} // namespace Nova
