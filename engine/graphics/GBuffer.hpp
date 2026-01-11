#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Texture;
class Shader;

/**
 * @brief G-Buffer texture attachment types for deferred rendering
 */
enum class GBufferAttachment {
    Position = 0,       // RGB: World-space position, A: Depth
    Normal,             // RGB: World-space normal, A: unused
    Albedo,             // RGB: Albedo color, A: Alpha/opacity
    MaterialParams,     // R: Metallic, G: Roughness, B: AO, A: MaterialID
    Emission,           // RGB: Emissive color, A: Emissive intensity
    Velocity,           // RG: Screen-space velocity (for TAA/motion blur)
    Count               // Number of attachments
};

/**
 * @brief G-Buffer configuration options
 */
struct GBufferConfig {
    int width = 1920;
    int height = 1080;

    // Precision options
    bool highPrecisionPosition = true;      // Use RGBA32F for position (vs RGBA16F)
    bool highPrecisionNormal = false;       // Use RGBA16F for normal (vs RGB10A2)
    bool enableEmission = true;             // Enable emission buffer
    bool enableVelocity = false;            // Enable velocity buffer (for TAA)

    // Quality options
    int msaaSamples = 1;                    // 1 = no MSAA, 2/4/8 for multisampling

    static GBufferConfig Default() {
        return GBufferConfig{};
    }

    static GBufferConfig HighQuality() {
        GBufferConfig config;
        config.highPrecisionPosition = true;
        config.highPrecisionNormal = true;
        config.enableEmission = true;
        config.enableVelocity = true;
        return config;
    }

    static GBufferConfig Performance() {
        GBufferConfig config;
        config.highPrecisionPosition = false;
        config.highPrecisionNormal = false;
        config.enableEmission = false;
        config.enableVelocity = false;
        return config;
    }
};

/**
 * @brief G-Buffer for Deferred Rendering
 *
 * Manages multiple render targets for storing geometry data:
 * - Position buffer (world-space position + linear depth)
 * - Normal buffer (world-space normal)
 * - Albedo buffer (diffuse color + alpha)
 * - Material buffer (metallic, roughness, AO, material ID)
 * - Optional: Emission buffer, Velocity buffer
 *
 * Uses OpenGL 4.5+ features:
 * - Multiple Render Targets (MRT)
 * - Direct State Access (DSA)
 * - Immutable texture storage
 */
class GBuffer {
public:
    GBuffer();
    ~GBuffer();

    // Non-copyable, movable
    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    GBuffer(GBuffer&& other) noexcept;
    GBuffer& operator=(GBuffer&& other) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Create G-Buffer with specified configuration
     * @param config G-Buffer configuration
     * @return true if creation succeeded
     */
    bool Create(const GBufferConfig& config);

    /**
     * @brief Create G-Buffer with default configuration
     * @param width Viewport width
     * @param height Viewport height
     * @return true if creation succeeded
     */
    bool Create(int width, int height);

    /**
     * @brief Resize G-Buffer
     * @param width New width
     * @param height New height
     */
    void Resize(int width, int height);

    /**
     * @brief Cleanup all GPU resources
     */
    void Cleanup();

    /**
     * @brief Check if G-Buffer is valid and complete
     */
    [[nodiscard]] bool IsValid() const;

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Bind G-Buffer for geometry pass (writing)
     */
    void BindForGeometryPass();

    /**
     * @brief Bind G-Buffer textures for lighting pass (reading)
     * @param baseSlot Starting texture slot
     */
    void BindTexturesForLighting(uint32_t baseSlot = 0);

    /**
     * @brief Unbind G-Buffer (bind default framebuffer)
     */
    static void Unbind();

    /**
     * @brief Clear all G-Buffer attachments
     * @param clearColor Color to clear albedo buffer (default: black)
     */
    void Clear(const glm::vec4& clearColor = glm::vec4(0.0f));

    /**
     * @brief Copy depth buffer to another framebuffer
     * @param targetFBO Target framebuffer ID (0 for default)
     */
    void CopyDepthTo(uint32_t targetFBO);

    // =========================================================================
    // Texture Access
    // =========================================================================

    /**
     * @brief Get position texture (RGB: position, A: linear depth)
     */
    [[nodiscard]] uint32_t GetPositionTexture() const { return m_positionTexture; }

    /**
     * @brief Get normal texture (RGB: normal, A: unused)
     */
    [[nodiscard]] uint32_t GetNormalTexture() const { return m_normalTexture; }

    /**
     * @brief Get albedo texture (RGB: albedo, A: alpha)
     */
    [[nodiscard]] uint32_t GetAlbedoTexture() const { return m_albedoTexture; }

    /**
     * @brief Get material parameters texture (R: metallic, G: roughness, B: AO, A: materialID)
     */
    [[nodiscard]] uint32_t GetMaterialTexture() const { return m_materialTexture; }

    /**
     * @brief Get emission texture (RGB: emission, A: intensity)
     */
    [[nodiscard]] uint32_t GetEmissionTexture() const { return m_emissionTexture; }

    /**
     * @brief Get velocity texture (RG: velocity)
     */
    [[nodiscard]] uint32_t GetVelocityTexture() const { return m_velocityTexture; }

    /**
     * @brief Get depth texture
     */
    [[nodiscard]] uint32_t GetDepthTexture() const { return m_depthTexture; }

    /**
     * @brief Get framebuffer object ID
     */
    [[nodiscard]] uint32_t GetFramebuffer() const { return m_fbo; }

    /**
     * @brief Get texture by attachment type
     */
    [[nodiscard]] uint32_t GetTexture(GBufferAttachment attachment) const;

    // =========================================================================
    // Properties
    // =========================================================================

    /**
     * @brief Get G-Buffer width
     */
    [[nodiscard]] int GetWidth() const { return m_config.width; }

    /**
     * @brief Get G-Buffer height
     */
    [[nodiscard]] int GetHeight() const { return m_config.height; }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const GBufferConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get number of active color attachments
     */
    [[nodiscard]] int GetAttachmentCount() const { return m_attachmentCount; }

    // =========================================================================
    // Debug
    // =========================================================================

    /**
     * @brief Visualize a specific G-Buffer attachment
     * @param attachment Which buffer to visualize
     * @param shader Visualization shader (optional, uses default if null)
     */
    void DebugVisualize(GBufferAttachment attachment, Shader* shader = nullptr);

    /**
     * @brief Get memory usage in bytes
     */
    [[nodiscard]] size_t GetMemoryUsage() const;

    /**
     * @brief Get attachment name for debugging
     */
    [[nodiscard]] static const char* GetAttachmentName(GBufferAttachment attachment);

private:
    // Create individual textures
    bool CreateTextures();
    bool CreateFramebuffer();

    // Texture format helpers
    uint32_t GetPositionFormat() const;
    uint32_t GetNormalFormat() const;
    uint32_t GetAlbedoFormat() const;
    uint32_t GetMaterialFormat() const;
    uint32_t GetEmissionFormat() const;
    uint32_t GetVelocityFormat() const;

    // Configuration
    GBufferConfig m_config;

    // OpenGL objects
    uint32_t m_fbo = 0;
    uint32_t m_positionTexture = 0;
    uint32_t m_normalTexture = 0;
    uint32_t m_albedoTexture = 0;
    uint32_t m_materialTexture = 0;
    uint32_t m_emissionTexture = 0;
    uint32_t m_velocityTexture = 0;
    uint32_t m_depthTexture = 0;

    // State
    int m_attachmentCount = 0;
    bool m_isValid = false;

    // Debug visualization
    std::unique_ptr<Shader> m_debugShader;
    uint32_t m_debugQuadVAO = 0;
    uint32_t m_debugQuadVBO = 0;
};

} // namespace Nova
