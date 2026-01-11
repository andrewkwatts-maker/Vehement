#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Nova {

// Forward declarations
class Shader;
class Camera;
class Framebuffer;

/**
 * @brief TAA quality preset
 */
enum class TAAQuality {
    Low,            // 2-tap, minimal clamping
    Medium,         // 4-tap, moderate clamping
    High,           // 8-tap, full neighborhood clamping
    Ultra           // 16-tap, variance clipping
};

/**
 * @brief Configuration for TAA
 */
struct TAAConfig {
    TAAQuality quality = TAAQuality::High;

    // Temporal settings
    float temporalAlpha = 0.1f;         // Blend factor (lower = more temporal stability)
    float motionScale = 1.0f;            // Motion vector scale

    // Jitter
    bool enableJitter = true;
    float jitterScale = 1.0f;

    // Clamping
    bool neighborhoodClamping = true;
    float clampRadius = 1.0f;            // Neighborhood radius for clamping
    bool varianceClipping = true;        // Use variance-based clipping

    // Sharpening
    bool sharpen = true;
    float sharpenAmount = 0.25f;

    // Quality
    int samples = 8;                     // Historical samples to consider
    bool useYCoCg = true;                // Use YCoCg color space for better blending

    // Anti-ghosting
    float velocityThreshold = 0.001f;    // Threshold for velocity rejection
    float luminanceWeight = 1.0f;        // Weight for luminance-based rejection
};

/**
 * @brief Temporal Anti-Aliasing
 *
 * Implements high-quality TAA with advanced features:
 * - Temporal jitter for sub-pixel sampling
 * - Motion vector generation (camera + object motion)
 * - Neighborhood clamping for ghosting reduction
 * - Variance clipping for improved stability
 * - YCoCg color space for better blending
 * - Sharpening pass to recover detail
 *
 * Features:
 * - Works with raymarched SDFs (motion vectors)
 * - Sub-pixel accuracy
 * - Minimal ghosting
 * - ~1ms overhead at 1080p
 * - Integrates with all lighting passes
 */
class TAA {
public:
    TAA();
    ~TAA();

    // Non-copyable
    TAA(const TAA&) = delete;
    TAA& operator=(const TAA&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize TAA system
     */
    bool Initialize(int width, int height, const TAAConfig& config);

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
    bool Reconfigure(const TAAConfig& config);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Apply TAA to current frame
     * @param camera View camera
     * @param colorTexture Current frame color
     * @param depthTexture Current frame depth
     * @param velocityTexture Motion vectors (optional, will generate if null)
     * @return Anti-aliased output texture
     */
    uint32_t Apply(const Camera& camera, uint32_t colorTexture,
                  uint32_t depthTexture, uint32_t velocityTexture = 0);

    /**
     * @brief Generate motion vectors
     * @param camera Current camera
     * @param depthTexture Scene depth
     * @return Motion vector texture (RG = velocity)
     */
    uint32_t GenerateMotionVectors(const Camera& camera, uint32_t depthTexture);

    /**
     * @brief Get jitter offset for current frame
     * @return Jitter offset in NDC space
     */
    glm::vec2 GetJitterOffset() const { return m_currentJitter; }

    /**
     * @brief Get jittered projection matrix
     * @param camera Base camera
     * @return Jittered projection matrix
     */
    glm::mat4 GetJitteredProjection(const Camera& camera) const;

    // =========================================================================
    // Frame Management
    // =========================================================================

    /**
     * @brief Begin new frame (updates jitter)
     */
    void BeginFrame();

    /**
     * @brief End frame (swap history buffers)
     */
    void EndFrame();

    /**
     * @brief Reset temporal history (call when scene changes drastically)
     */
    void ResetHistory();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    const TAAConfig& GetConfig() const { return m_config; }

    /**
     * @brief Apply quality preset
     */
    void ApplyQualityPreset(TAAQuality quality);

    /**
     * @brief Enable/disable TAA
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Enable/disable debug visualization
     */
    void SetDebugVisualization(bool enabled) { m_debugVisualization = enabled; }

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        float totalTimeMs = 0.0f;
        float motionVectorTimeMs = 0.0f;
        float resolveTi meMs = 0.0f;
        float sharpenTimeMs = 0.0f;
        int frameIndex = 0;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    // Generate Halton sequence jitter
    glm::vec2 GenerateJitter();

    // Color space conversion
    glm::vec3 RGBToYCoCg(const glm::vec3& rgb);
    glm::vec3 YCoCgToRGB(const glm::vec3& ycocg);

    // Create resources
    bool CreateResources();
    void DestroyResources();

    bool m_initialized = false;
    TAAConfig m_config;
    bool m_enabled = true;

    // Dimensions
    int m_width = 0;
    int m_height = 0;

    // Framebuffers
    std::unique_ptr<Framebuffer> m_resolveFramebuffer;
    std::unique_ptr<Framebuffer> m_motionFramebuffer;
    std::unique_ptr<Framebuffer> m_sharpenFramebuffer;

    // History buffers (ping-pong)
    uint32_t m_historyTexture[2] = {0, 0};
    uint32_t m_currentHistoryIndex = 0;

    // Shaders
    std::unique_ptr<Shader> m_motionVectorShader;
    std::unique_ptr<Shader> m_resolveShader;
    std::unique_ptr<Shader> m_sharpenShader;

    // State
    int m_frameIndex = 0;
    glm::vec2 m_currentJitter{0.0f};
    glm::vec2 m_previousJitter{0.0f};
    glm::mat4 m_prevViewProj;
    glm::mat4 m_prevInvViewProj;
    glm::vec3 m_prevCameraPos;

    // Halton sequence for jitter
    std::vector<glm::vec2> m_jitterSequence;
    int m_jitterIndex = 0;

    // Debug
    bool m_debugVisualization = false;

    // Statistics
    Stats m_stats;
};

} // namespace Nova
