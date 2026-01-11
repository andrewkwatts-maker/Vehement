#pragma once

#include "RenderBackend.hpp"
#include "Framebuffer.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Nova {

// Forward declarations
class Shader;
class Texture;
class Mesh;
class Material;

/**
 * @brief Render batch for polygon rendering
 */
struct PolygonBatch {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::vector<glm::mat4> transforms;
    uint32_t instanceCount = 0;
    bool isInstanced = false;
};

/**
 * @brief Traditional polygon rasterizer
 *
 * Uses OpenGL's standard rasterization pipeline for rendering
 * triangle meshes with materials. Supports:
 * - Instanced rendering for repeated geometry
 * - Shadow mapping with cascaded shadow maps
 * - PBR materials
 * - Forward+ or deferred rendering
 * - LOD system integration
 */
class PolygonRasterizer : public IRenderBackend {
public:
    PolygonRasterizer();
    ~PolygonRasterizer() override;

    // Non-copyable
    PolygonRasterizer(const PolygonRasterizer&) = delete;
    PolygonRasterizer& operator=(const PolygonRasterizer&) = delete;

    // IRenderBackend implementation
    bool Initialize(int width, int height) override;
    void Shutdown() override;
    void Resize(int width, int height) override;
    void BeginFrame(const Camera& camera) override;
    void EndFrame() override;
    void Render(const Scene& scene, const Camera& camera) override;
    void SetQualitySettings(const QualitySettings& settings) override;
    const QualitySettings& GetQualitySettings() const override { return m_settings; }
    const RenderStats& GetStats() const override { return m_stats; }
    bool SupportsFeature(RenderFeature feature) const override;
    const char* GetName() const override { return "Polygon Rasterizer (OpenGL)"; }
    std::shared_ptr<Texture> GetOutputColor() const override;
    std::shared_ptr<Texture> GetOutputDepth() const override;
    void SetDebugMode(bool enabled) override { m_debugMode = enabled; }

    /**
     * @brief Submit mesh for rendering
     */
    void SubmitMesh(const std::shared_ptr<Mesh>& mesh,
                    const std::shared_ptr<Material>& material,
                    const glm::mat4& transform);

    /**
     * @brief Submit instanced meshes
     */
    void SubmitInstanced(const std::shared_ptr<Mesh>& mesh,
                        const std::shared_ptr<Material>& material,
                        const std::vector<glm::mat4>& transforms);

    /**
     * @brief Clear all submitted geometry
     */
    void ClearSubmissions();

private:
    /**
     * @brief Build render batches from submissions
     */
    void BuildBatches();

    /**
     * @brief Render opaque geometry
     */
    void RenderOpaque();

    /**
     * @brief Render transparent geometry
     */
    void RenderTransparent();

    /**
     * @brief Render shadow maps
     */
    void RenderShadows();

    /**
     * @brief Setup shadow cascades
     */
    void SetupShadowCascades(const Camera& camera);

    /**
     * @brief Render a single batch
     */
    void RenderBatch(const PolygonBatch& batch);

    /**
     * @brief Setup PBR material uniforms
     */
    void SetupMaterial(const Material& material);

    /**
     * @brief Create GPU buffers for instancing
     */
    bool CreateInstanceBuffers();

    /**
     * @brief Create shaders
     */
    bool CreateShaders();

    /**
     * @brief Create shadow map framebuffers
     */
    bool CreateShadowMaps();

    /**
     * @brief Update performance statistics
     */
    void UpdateStats();

    // Settings and state
    QualitySettings m_settings;
    RenderStats m_stats;
    bool m_debugMode = false;
    bool m_initialized = false;

    // Render targets
    std::unique_ptr<Framebuffer> m_framebuffer;
    std::shared_ptr<Texture> m_colorTexture;
    std::shared_ptr<Texture> m_depthTexture;

    // Shadow maps
    std::vector<std::unique_ptr<Framebuffer>> m_shadowMapFramebuffers;
    std::vector<glm::mat4> m_shadowViewProj;
    std::vector<float> m_cascadeSplits;

    // Shaders
    std::shared_ptr<Shader> m_pbrShader;
    std::shared_ptr<Shader> m_shadowShader;
    std::shared_ptr<Shader> m_instancedShader;

    // Render batches
    std::vector<PolygonBatch> m_opaqueBatches;
    std::vector<PolygonBatch> m_transparentBatches;

    // Instancing buffers
    uint32_t m_instanceBuffer = 0;
    uint32_t m_instanceVAO = 0;
    size_t m_maxInstances = 1024;

    // Camera data
    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projMatrix{1.0f};
    glm::mat4 m_viewProjMatrix{1.0f};
    glm::vec3 m_cameraPosition{0.0f};

    // Timing
    uint32_t m_gpuQueryStart = 0;
    uint32_t m_gpuQueryEnd = 0;
    std::chrono::high_resolution_clock::time_point m_frameStartTime;

    // Frame counter for statistics
    uint32_t m_frameCount = 0;
    float m_accumulatedTime = 0.0f;
};

} // namespace Nova
