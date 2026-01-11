#pragma once

#include <memory>
#include <cstdint>
#include "../math/Vector3.hpp"
#include "../math/Vector4.hpp"
#include "../math/Matrix4.hpp"
#include "GPUDrivenRenderer.hpp"
#include "ClusteredLightingExpanded.hpp"

namespace Engine {
namespace Graphics {

/**
 * @brief G-Buffer layout for deferred rendering
 */
struct GBuffer {
    uint32_t fbo;
    uint32_t albedoTexture;      // RGB: albedo, A: metallic
    uint32_t normalTexture;      // RGB: normal, A: roughness
    uint32_t materialTexture;    // R: IOR, G: scattering, B: emission, A: materialID
    uint32_t depthTexture;       // Depth buffer
    uint32_t width;
    uint32_t height;

    GBuffer() : fbo(0), albedoTexture(0), normalTexture(0),
                materialTexture(0), depthTexture(0), width(0), height(0) {}
};

/**
 * @brief Material properties for SDF rendering
 */
struct SDFMaterial {
    Vector3 albedo;
    float metallic;
    float roughness;
    float ior;               // Index of refraction
    float scattering;        // Subsurface scattering factor
    Vector3 emission;
    uint32_t materialID;

    SDFMaterial()
        : albedo(0.8f, 0.8f, 0.8f)
        , metallic(0.0f)
        , roughness(0.5f)
        , ior(1.45f)
        , scattering(0.0f)
        , emission(0, 0, 0)
        , materialID(0) {}
};

/**
 * @brief SDF raymarch settings
 */
struct RaymarchSettings {
    int maxSteps;
    float maxDistance;
    float hitThreshold;
    float normalEpsilon;
    bool enableAO;
    bool enableSoftShadows;
    int aoSamples;
    float aoRadius;

    RaymarchSettings()
        : maxSteps(128)
        , maxDistance(1000.0f)
        , hitThreshold(0.001f)
        , normalEpsilon(0.001f)
        , enableAO(true)
        , enableSoftShadows(true)
        , aoSamples(8)
        , aoRadius(0.5f) {}
};

/**
 * @brief Deferred lighting system for SDF objects
 * Two-pass rendering: raymarch to G-buffer, then screen-space lighting
 */
class SDFDeferredLighting {
public:
    explicit SDFDeferredLighting(uint32_t width, uint32_t height);
    ~SDFDeferredLighting();

    /**
     * @brief Initialize system
     */
    bool Initialize();

    /**
     * @brief Resize buffers
     */
    void Resize(uint32_t width, uint32_t height);

    /**
     * @brief Begin G-buffer pass (raymarch SDFs)
     */
    void BeginGBufferPass();

    /**
     * @brief End G-buffer pass
     */
    void EndGBufferPass();

    /**
     * @brief Render SDFs to G-buffer
     * @param sdfShader Custom SDF shader for scene
     * @param camera Camera for raymarching
     */
    void RenderSDFsToGBuffer(uint32_t sdfShader, const Matrix4& viewMatrix, const Matrix4& projMatrix);

    /**
     * @brief Execute lighting pass (deferred)
     * Reads G-buffer and applies clustered lighting
     */
    void ExecuteLightingPass(ClusteredLightingExpanded* lighting,
                            const Matrix4& viewMatrix,
                            const Matrix4& projMatrix,
                            const Vector3& cameraPos);

    /**
     * @brief Get final lit output texture
     */
    uint32_t GetOutputTexture() const { return m_outputTexture; }

    /**
     * @brief Get G-buffer
     */
    const GBuffer& GetGBuffer() const { return m_gbuffer; }

    /**
     * @brief Set raymarch settings
     */
    void SetRaymarchSettings(const RaymarchSettings& settings) { m_raymarchSettings = settings; }

    /**
     * @brief Get raymarch settings
     */
    RaymarchSettings GetRaymarchSettings() const { return m_raymarchSettings; }

    /**
     * @brief Performance statistics
     */
    struct Stats {
        float gbufferPassTimeMs;
        float lightingPassTimeMs;
        uint32_t pixelsShaded;
        uint32_t raymarchSteps;
        float avgStepsPerPixel;

        Stats()
            : gbufferPassTimeMs(0), lightingPassTimeMs(0)
            , pixelsShaded(0), raymarchSteps(0), avgStepsPerPixel(0) {}
    };

    Stats GetStats() const { return m_stats; }
    void ResetStats();

    /**
     * @brief Clear G-buffer
     */
    void ClearGBuffer();

    /**
     * @brief Blend with rasterized geometry
     * Allows mixing raymarched SDFs with traditional polygons
     */
    void BlendWithRasterized(uint32_t rasterDepthTexture);

private:
    /**
     * @brief Create G-buffer textures and framebuffer
     */
    bool CreateGBuffer();

    /**
     * @brief Create output framebuffer for lighting
     */
    bool CreateOutputBuffer();

    /**
     * @brief Load shaders
     */
    bool LoadShaders();

    /**
     * @brief Render fullscreen quad
     */
    void RenderFullscreenQuad();

    uint32_t m_width;
    uint32_t m_height;

    // G-Buffer
    GBuffer m_gbuffer;

    // Output buffer (final lit result)
    uint32_t m_outputFBO;
    uint32_t m_outputTexture;

    // Shaders
    uint32_t m_gbufferShader;      // Raymarch to G-buffer
    uint32_t m_lightingShader;     // Deferred lighting
    uint32_t m_blendShader;        // Blend with rasterized

    // Fullscreen quad VAO
    uint32_t m_quadVAO;
    uint32_t m_quadVBO;

    // Settings
    RaymarchSettings m_raymarchSettings;

    // Performance tracking
    Stats m_stats;
    uint32_t m_queryObjects[2];  // G-buffer and lighting queries
};

/**
 * @brief Hybrid renderer for both SDFs and polygons
 * Combines deferred lighting for SDFs with forward/deferred for polygons
 */
class HybridSDFRenderer {
public:
    HybridSDFRenderer(uint32_t width, uint32_t height);
    ~HybridSDFRenderer();

    /**
     * @brief Initialize
     */
    bool Initialize();

    /**
     * @brief Begin frame
     */
    void BeginFrame();

    /**
     * @brief Render rasterized geometry (forward pass)
     */
    void RenderRasterizedGeometry(const Matrix4& viewMatrix, const Matrix4& projMatrix);

    /**
     * @brief Render SDFs (deferred pass)
     */
    void RenderSDFs(uint32_t sdfShader, const Matrix4& viewMatrix, const Matrix4& projMatrix);

    /**
     * @brief Apply lighting to both rasterized and SDF geometry
     */
    void ApplyLighting(ClusteredLightingExpanded* lighting,
                      const Matrix4& viewMatrix,
                      const Matrix4& projMatrix,
                      const Vector3& cameraPos);

    /**
     * @brief End frame
     */
    void EndFrame();

    /**
     * @brief Get final output
     */
    uint32_t GetOutputTexture() const;

    /**
     * @brief Get SDF deferred system
     */
    SDFDeferredLighting* GetSDFDeferred() { return m_sdfDeferred.get(); }

private:
    uint32_t m_width;
    uint32_t m_height;

    std::unique_ptr<SDFDeferredLighting> m_sdfDeferred;

    // Rasterized geometry buffers
    uint32_t m_rasterFBO;
    uint32_t m_rasterColorTexture;
    uint32_t m_rasterDepthTexture;

    // Composite buffer
    uint32_t m_compositeFBO;
    uint32_t m_compositeTexture;

    // Shaders
    uint32_t m_compositeShader;
};

} // namespace Graphics
} // namespace Engine
