#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

class Shader;
class Texture;
class Framebuffer;

// ============================================================================
// Post-Process Effect Types
// ============================================================================

/**
 * @brief Types of post-processing effects
 */
enum class PostProcessEffectType : uint8_t {
    None = 0,
    Bloom,
    ToneMapping,
    ColorGrading,
    AmbientOcclusion,
    MotionBlur,
    DepthOfField,
    FXAA,
    ChromaticAberration,
    Vignette,
    FilmGrain,
    Sharpen,
    Custom
};

/**
 * @brief Tone mapping operators
 */
enum class ToneMappingOperator : uint8_t {
    None,
    Reinhard,
    ReinhardExtended,
    ACES,
    Uncharted2,
    Exposure
};

// ============================================================================
// Effect Parameters
// ============================================================================

/**
 * @brief Bloom effect parameters
 */
struct BloomParams {
    float threshold = 1.0f;        ///< Brightness threshold for bloom
    float intensity = 1.0f;        ///< Bloom intensity
    float radius = 0.005f;         ///< Blur radius
    int iterations = 6;            ///< Number of blur iterations
    float softKnee = 0.5f;         ///< Soft threshold knee
    glm::vec3 tint{1.0f};          ///< Bloom color tint

    bool operator==(const BloomParams& other) const {
        return threshold == other.threshold && intensity == other.intensity &&
               radius == other.radius && iterations == other.iterations;
    }
};

/**
 * @brief Tone mapping parameters
 */
struct ToneMappingParams {
    ToneMappingOperator op = ToneMappingOperator::ACES;
    float exposure = 1.0f;         ///< Exposure multiplier
    float gamma = 2.2f;            ///< Gamma correction value
    float whitePoint = 11.2f;      ///< White point for some operators
    bool autoExposure = false;     ///< Enable auto-exposure
    float adaptationSpeed = 1.0f;  ///< Auto-exposure adaptation speed
    float minExposure = 0.1f;      ///< Minimum auto-exposure
    float maxExposure = 10.0f;     ///< Maximum auto-exposure
};

/**
 * @brief Color grading parameters (includes LUT support)
 */
struct ColorGradingParams {
    // Basic adjustments
    float contrast = 1.0f;         ///< Contrast (1 = no change)
    float saturation = 1.0f;       ///< Saturation (1 = no change)
    float brightness = 0.0f;       ///< Brightness offset
    float hueShift = 0.0f;         ///< Hue rotation in degrees

    // Lift/Gamma/Gain (color wheels)
    glm::vec3 lift{0.0f};          ///< Shadow color adjustment
    glm::vec3 gamma{1.0f};         ///< Midtone color adjustment
    glm::vec3 gain{1.0f};          ///< Highlight color adjustment

    // Color balance
    float temperature = 0.0f;      ///< Color temperature (-100 to 100)
    float tint = 0.0f;             ///< Tint (magenta-green) (-100 to 100)

    // LUT
    std::string lutPath;           ///< Path to LUT texture
    float lutIntensity = 1.0f;     ///< LUT blend intensity
};

/**
 * @brief Screen-space ambient occlusion parameters
 */
struct AmbientOcclusionParams {
    float radius = 0.5f;           ///< Sample radius in world units
    float intensity = 1.0f;        ///< AO intensity
    float bias = 0.025f;           ///< Depth bias to prevent self-occlusion
    int samples = 64;              ///< Number of samples
    float power = 2.0f;            ///< AO power curve
    bool halfResolution = true;    ///< Render at half resolution
    float falloffStart = 0.2f;     ///< Distance falloff start
    float falloffEnd = 100.0f;     ///< Distance falloff end

    enum class Quality : uint8_t {
        Low,      // 16 samples
        Medium,   // 32 samples
        High,     // 64 samples
        Ultra     // 128 samples
    };
    Quality quality = Quality::Medium;
};

/**
 * @brief Motion blur parameters
 */
struct MotionBlurParams {
    float intensity = 1.0f;        ///< Blur intensity
    int samples = 8;               ///< Number of samples
    float maxBlur = 0.05f;         ///< Maximum blur amount
    bool objectMotionBlur = true;  ///< Use per-object velocity
    float velocityScale = 1.0f;    ///< Velocity scaling factor
    float centerFalloff = 0.1f;    ///< Reduce blur near screen center
};

/**
 * @brief Depth of field parameters
 */
struct DepthOfFieldParams {
    float focusDistance = 10.0f;   ///< Focus distance
    float focusRange = 5.0f;       ///< Range of sharp focus
    float nearBlur = 3.0f;         ///< Near blur strength
    float farBlur = 5.0f;          ///< Far blur strength
    float aperture = 2.8f;         ///< Aperture for bokeh size
    int blurSamples = 8;           ///< Blur quality
    bool hexagonalBokeh = true;    ///< Use hexagonal bokeh shape

    enum class Quality : uint8_t {
        Low,      // Simple blur
        Medium,   // Gaussian blur
        High,     // Bokeh simulation
        Cinematic // Full bokeh with CoC
    };
    Quality quality = Quality::Medium;
};

/**
 * @brief FXAA anti-aliasing parameters
 */
struct FXAAParams {
    float edgeThreshold = 0.166f;        ///< Edge detection threshold
    float edgeThresholdMin = 0.0833f;    ///< Minimum edge threshold
    float subpixelQuality = 0.75f;       ///< Subpixel aliasing removal
};

/**
 * @brief Chromatic aberration parameters
 */
struct ChromaticAberrationParams {
    float intensity = 1.0f;        ///< Effect intensity
    glm::vec2 offset{0.002f, 0.002f}; ///< Red/Blue channel offset
    float edgeOnly = 0.5f;         ///< Only apply at screen edges
};

/**
 * @brief Vignette parameters
 */
struct VignetteParams {
    float intensity = 0.3f;        ///< Vignette darkness
    float smoothness = 0.5f;       ///< Falloff smoothness
    glm::vec2 center{0.5f, 0.5f}; ///< Vignette center
    bool rounded = true;           ///< Circular (true) or box (false)
    glm::vec3 color{0.0f};         ///< Vignette color (usually black)
};

/**
 * @brief Film grain parameters
 */
struct FilmGrainParams {
    float intensity = 0.1f;        ///< Grain intensity
    float size = 1.0f;             ///< Grain size
    float luminanceOnly = 0.0f;    ///< Only affect luminance
    bool colored = false;          ///< Colored grain
};

/**
 * @brief Sharpen parameters
 */
struct SharpenParams {
    float strength = 0.5f;         ///< Sharpening strength
    float clamp = 0.035f;          ///< Maximum sharpening
};

// ============================================================================
// Post-Process Effect Base
// ============================================================================

/**
 * @brief Base class for post-processing effects
 */
class PostProcessEffect {
public:
    PostProcessEffect(PostProcessEffectType type, const std::string& name)
        : m_type(type), m_name(name) {}
    virtual ~PostProcessEffect() = default;

    /**
     * @brief Initialize effect resources
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Cleanup resources
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Apply the effect
     * @param input Input texture
     * @param output Output framebuffer
     * @param depthTexture Optional depth texture
     */
    virtual void Apply(uint32_t inputTexture, uint32_t outputFBO,
                       uint32_t depthTexture = 0) = 0;

    /**
     * @brief Resize effect resources
     */
    virtual void Resize(int width, int height) {
        m_width = width;
        m_height = height;
    }

    // Properties
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    void SetOrder(int order) { m_order = order; }
    [[nodiscard]] int GetOrder() const { return m_order; }

    [[nodiscard]] PostProcessEffectType GetType() const { return m_type; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

protected:
    PostProcessEffectType m_type;
    std::string m_name;
    bool m_enabled = true;
    int m_order = 0;
    int m_width = 0;
    int m_height = 0;
};

// ============================================================================
// Bloom Effect
// ============================================================================

/**
 * @brief High-quality bloom effect with configurable parameters
 */
class BloomEffect : public PostProcessEffect {
public:
    BloomEffect() : PostProcessEffect(PostProcessEffectType::Bloom, "Bloom") {}

    bool Initialize() override;
    void Shutdown() override;
    void Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t depthTexture) override;
    void Resize(int width, int height) override;

    void SetParams(const BloomParams& params) { m_params = params; }
    BloomParams& GetParams() { return m_params; }
    const BloomParams& GetParams() const { return m_params; }

private:
    void CreateMipChain();
    void DownsamplePass(uint32_t srcTex, int srcLevel, int dstLevel);
    void UpsamplePass(int srcLevel, int dstLevel);

    BloomParams m_params;
    std::unique_ptr<Shader> m_thresholdShader;
    std::unique_ptr<Shader> m_downsampleShader;
    std::unique_ptr<Shader> m_upsampleShader;
    std::unique_ptr<Shader> m_compositeShader;

    std::vector<uint32_t> m_mipFBOs;
    std::vector<uint32_t> m_mipTextures;
    std::vector<glm::ivec2> m_mipSizes;
    static constexpr int kMaxMipLevels = 8;
};

// ============================================================================
// Tone Mapping Effect
// ============================================================================

/**
 * @brief HDR to LDR tone mapping with various operators
 */
class ToneMappingEffect : public PostProcessEffect {
public:
    ToneMappingEffect() : PostProcessEffect(PostProcessEffectType::ToneMapping, "ToneMapping") {}

    bool Initialize() override;
    void Shutdown() override;
    void Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t depthTexture) override;

    void SetParams(const ToneMappingParams& params) { m_params = params; }
    ToneMappingParams& GetParams() { return m_params; }
    const ToneMappingParams& GetParams() const { return m_params; }

    /**
     * @brief Get current auto-exposure value
     */
    [[nodiscard]] float GetCurrentExposure() const { return m_currentExposure; }

private:
    void UpdateAutoExposure(uint32_t inputTexture, float deltaTime);

    ToneMappingParams m_params;
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Shader> m_luminanceShader;
    uint32_t m_luminanceFBO = 0;
    uint32_t m_luminanceTexture = 0;
    float m_currentExposure = 1.0f;
};

// ============================================================================
// Color Grading Effect
// ============================================================================

/**
 * @brief Color grading with LUT support
 */
class ColorGradingEffect : public PostProcessEffect {
public:
    ColorGradingEffect() : PostProcessEffect(PostProcessEffectType::ColorGrading, "ColorGrading") {}

    bool Initialize() override;
    void Shutdown() override;
    void Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t depthTexture) override;

    void SetParams(const ColorGradingParams& params);
    ColorGradingParams& GetParams() { return m_params; }
    const ColorGradingParams& GetParams() const { return m_params; }

    /**
     * @brief Load a LUT texture
     */
    bool LoadLUT(const std::string& path);

private:
    ColorGradingParams m_params;
    std::unique_ptr<Shader> m_shader;
    std::shared_ptr<Texture> m_lutTexture;
};

// ============================================================================
// SSAO Effect
// ============================================================================

/**
 * @brief Screen-space ambient occlusion
 */
class SSAOEffect : public PostProcessEffect {
public:
    SSAOEffect() : PostProcessEffect(PostProcessEffectType::AmbientOcclusion, "SSAO") {}

    bool Initialize() override;
    void Shutdown() override;
    void Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t depthTexture) override;
    void Resize(int width, int height) override;

    void SetParams(const AmbientOcclusionParams& params);
    AmbientOcclusionParams& GetParams() { return m_params; }
    const AmbientOcclusionParams& GetParams() const { return m_params; }

    /**
     * @brief Set view and projection matrices for AO calculation
     */
    void SetMatrices(const glm::mat4& view, const glm::mat4& projection);

private:
    void GenerateNoiseTexture();
    void GenerateKernel();

    AmbientOcclusionParams m_params;
    std::unique_ptr<Shader> m_ssaoShader;
    std::unique_ptr<Shader> m_blurShader;

    uint32_t m_ssaoFBO = 0;
    uint32_t m_ssaoTexture = 0;
    uint32_t m_blurFBO = 0;
    uint32_t m_blurTexture = 0;
    uint32_t m_noiseTexture = 0;

    std::vector<glm::vec3> m_kernel;
    glm::mat4 m_view;
    glm::mat4 m_projection;
};

// ============================================================================
// Motion Blur Effect
// ============================================================================

/**
 * @brief Camera and object motion blur
 */
class MotionBlurEffect : public PostProcessEffect {
public:
    MotionBlurEffect() : PostProcessEffect(PostProcessEffectType::MotionBlur, "MotionBlur") {}

    bool Initialize() override;
    void Shutdown() override;
    void Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t depthTexture) override;

    void SetParams(const MotionBlurParams& params) { m_params = params; }
    MotionBlurParams& GetParams() { return m_params; }
    const MotionBlurParams& GetParams() const { return m_params; }

    /**
     * @brief Set velocity texture for per-pixel motion vectors
     */
    void SetVelocityTexture(uint32_t velocityTex);

    /**
     * @brief Set view-projection matrices for camera motion blur
     */
    void SetViewProjection(const glm::mat4& current, const glm::mat4& previous);

private:
    MotionBlurParams m_params;
    std::unique_ptr<Shader> m_shader;
    uint32_t m_velocityTexture = 0;
    glm::mat4 m_currentVP;
    glm::mat4 m_previousVP;
};

// ============================================================================
// Depth of Field Effect
// ============================================================================

/**
 * @brief Cinematic depth of field with bokeh
 */
class DepthOfFieldEffect : public PostProcessEffect {
public:
    DepthOfFieldEffect() : PostProcessEffect(PostProcessEffectType::DepthOfField, "DepthOfField") {}

    bool Initialize() override;
    void Shutdown() override;
    void Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t depthTexture) override;
    void Resize(int width, int height) override;

    void SetParams(const DepthOfFieldParams& params) { m_params = params; }
    DepthOfFieldParams& GetParams() { return m_params; }
    const DepthOfFieldParams& GetParams() const { return m_params; }

    /**
     * @brief Set camera near/far planes for depth linearization
     */
    void SetCameraPlanes(float near, float far);

private:
    void CalculateCoC(uint32_t depthTexture);

    DepthOfFieldParams m_params;
    std::unique_ptr<Shader> m_cocShader;
    std::unique_ptr<Shader> m_blurShader;
    std::unique_ptr<Shader> m_compositeShader;

    uint32_t m_cocFBO = 0;
    uint32_t m_cocTexture = 0;
    uint32_t m_blurFBO = 0;
    uint32_t m_blurTexture = 0;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;
};

// ============================================================================
// Post-Process Pipeline
// ============================================================================

/**
 * @brief Complete post-processing pipeline
 *
 * Manages a chain of post-processing effects that are applied in order.
 * Handles framebuffer ping-ponging for efficient multi-pass rendering.
 *
 * Usage:
 * @code
 * PostProcessPipeline pipeline;
 * pipeline.Initialize(1920, 1080);
 *
 * pipeline.AddEffect<BloomEffect>("bloom");
 * pipeline.AddEffect<ToneMappingEffect>("tonemapping");
 *
 * pipeline.GetEffect<BloomEffect>("bloom")->GetParams().intensity = 1.5f;
 *
 * // In render loop
 * pipeline.Begin();  // Bind pipeline FBO
 * // ... render scene ...
 * pipeline.End();    // Apply effects and render to screen
 * @endcode
 */
class PostProcessPipeline {
public:
    PostProcessPipeline() = default;
    ~PostProcessPipeline();

    /**
     * @brief Initialize the pipeline
     * @param width Render width
     * @param height Render height
     * @param hdr Use HDR format (RGBA16F)
     */
    bool Initialize(int width, int height, bool hdr = true);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Resize all buffers
     */
    void Resize(int width, int height);

    /**
     * @brief Add an effect to the pipeline
     */
    template<typename T>
    T* AddEffect(const std::string& name);

    /**
     * @brief Get an effect by name
     */
    template<typename T>
    T* GetEffect(const std::string& name);

    /**
     * @brief Get an effect by type
     */
    PostProcessEffect* GetEffectByType(PostProcessEffectType type);

    /**
     * @brief Remove an effect
     */
    void RemoveEffect(const std::string& name);

    /**
     * @brief Enable/disable an effect by name
     */
    void SetEffectEnabled(const std::string& name, bool enabled);

    /**
     * @brief Begin rendering to pipeline
     */
    void Begin();

    /**
     * @brief End rendering and apply all effects
     * @param depthTexture Optional depth texture for depth-aware effects
     */
    void End(uint32_t depthTexture = 0);

    /**
     * @brief Apply effects to external texture
     */
    void Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t depthTexture = 0);

    /**
     * @brief Set view-projection for motion blur/DoF
     */
    void SetViewProjection(const glm::mat4& view, const glm::mat4& projection);

    /**
     * @brief Set camera planes for DoF
     */
    void SetCameraPlanes(float near, float far);

    /**
     * @brief Get scene framebuffer (for rendering scene to)
     */
    [[nodiscard]] uint32_t GetSceneFBO() const { return m_sceneFBO; }

    /**
     * @brief Get scene color texture
     */
    [[nodiscard]] uint32_t GetSceneTexture() const { return m_sceneTexture; }

    /**
     * @brief Get pipeline dimensions
     */
    [[nodiscard]] int GetWidth() const { return m_width; }
    [[nodiscard]] int GetHeight() const { return m_height; }

    /**
     * @brief Check if HDR mode
     */
    [[nodiscard]] bool IsHDR() const { return m_hdr; }

    /**
     * @brief Get all effect names
     */
    [[nodiscard]] std::vector<std::string> GetEffectNames() const;

private:
    void CreateFramebuffers();
    void RenderFullscreenQuad();

    int m_width = 0;
    int m_height = 0;
    bool m_hdr = true;

    // Scene framebuffer
    uint32_t m_sceneFBO = 0;
    uint32_t m_sceneTexture = 0;
    uint32_t m_sceneDepthRBO = 0;

    // Ping-pong buffers for effects
    uint32_t m_pingFBO = 0;
    uint32_t m_pingTexture = 0;
    uint32_t m_pongFBO = 0;
    uint32_t m_pongTexture = 0;
    bool m_usePing = true;

    // Fullscreen quad
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;
    std::unique_ptr<Shader> m_copyShader;

    // Effects
    std::vector<std::unique_ptr<PostProcessEffect>> m_effects;
    std::unordered_map<std::string, PostProcessEffect*> m_effectsByName;

    // Matrices for depth-aware effects
    glm::mat4 m_view;
    glm::mat4 m_projection;
    glm::mat4 m_previousVP;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;
};

// Template implementations
template<typename T>
T* PostProcessPipeline::AddEffect(const std::string& name) {
    static_assert(std::is_base_of<PostProcessEffect, T>::value,
                  "T must derive from PostProcessEffect");

    auto effect = std::make_unique<T>();
    if (!effect->Initialize()) {
        return nullptr;
    }

    effect->Resize(m_width, m_height);
    effect->SetOrder(static_cast<int>(m_effects.size()));

    T* ptr = effect.get();
    m_effectsByName[name] = ptr;
    m_effects.push_back(std::move(effect));

    // Sort by order
    std::sort(m_effects.begin(), m_effects.end(),
              [](const auto& a, const auto& b) { return a->GetOrder() < b->GetOrder(); });

    return ptr;
}

template<typename T>
T* PostProcessPipeline::GetEffect(const std::string& name) {
    auto it = m_effectsByName.find(name);
    if (it != m_effectsByName.end()) {
        return dynamic_cast<T*>(it->second);
    }
    return nullptr;
}

} // namespace Nova
