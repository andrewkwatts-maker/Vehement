#pragma once

#include "RenderBackend.hpp"
#include "SDFRasterizer.hpp"
#include "PolygonRasterizer.hpp"
#include "HybridDepthMerge.hpp"
#include "Framebuffer.hpp"
#include <memory>

namespace Nova {

/**
 * @brief Hybrid rasterizer combining SDF and polygon rendering
 *
 * Integrates SDFRasterizer and PolygonRasterizer with proper depth
 * buffer interleaving. Supports multiple rendering orders:
 * - SDF-first: Render SDFs, then polygons with SDF depth test
 * - Polygon-first: Render polygons, then SDFs with polygon depth
 * - Auto: Dynamically choose based on scene composition
 *
 * Key features:
 * - Seamless Z-buffer integration between SDF and polygon passes
 * - Shared lighting and shadow systems
 * - Unified material system (PBR for both)
 * - Automatic render order optimization
 * - Per-pass performance profiling
 */
class HybridRasterizer : public IRenderBackend {
public:
    HybridRasterizer();
    ~HybridRasterizer() override;

    // Non-copyable
    HybridRasterizer(const HybridRasterizer&) = delete;
    HybridRasterizer& operator=(const HybridRasterizer&) = delete;

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
    const char* GetName() const override { return "Hybrid Rasterizer (SDF + Polygon)"; }
    std::shared_ptr<Texture> GetOutputColor() const override;
    std::shared_ptr<Texture> GetOutputDepth() const override;
    void SetDebugMode(bool enabled) override;

    /**
     * @brief Get SDF rasterizer for direct access
     */
    SDFRasterizer& GetSDFRasterizer() { return *m_sdfRasterizer; }

    /**
     * @brief Get polygon rasterizer for direct access
     */
    PolygonRasterizer& GetPolygonRasterizer() { return *m_polygonRasterizer; }

    /**
     * @brief Get depth merge system
     */
    HybridDepthMerge& GetDepthMerge() { return *m_depthMerge; }

    /**
     * @brief Set render order mode
     */
    void SetRenderOrder(QualitySettings::RenderOrder order);

    /**
     * @brief Get current render order
     */
    QualitySettings::RenderOrder GetRenderOrder() const;

    /**
     * @brief Enable/disable automatic render order selection
     */
    void SetAutoRenderOrder(bool enabled) { m_autoRenderOrder = enabled; }

    /**
     * @brief Check if auto render order is enabled
     */
    bool IsAutoRenderOrder() const { return m_autoRenderOrder; }

    /**
     * @brief Get percentage of scene that is SDF vs polygon
     * @return SDF percentage (0.0 = all polygon, 1.0 = all SDF)
     */
    float GetSDFPercentage() const;

private:
    /**
     * @brief Decide optimal render order based on scene
     */
    QualitySettings::RenderOrder DetermineOptimalRenderOrder(const Scene& scene);

    /**
     * @brief Render in SDF-first order
     */
    void RenderSDFFirst(const Scene& scene, const Camera& camera);

    /**
     * @brief Render in polygon-first order
     */
    void RenderPolygonFirst(const Scene& scene, const Camera& camera);

    /**
     * @brief Composite SDF and polygon results
     */
    void CompositeResults();

    /**
     * @brief Merge depth buffers from both passes
     */
    void MergeDepthBuffers();

    /**
     * @brief Extract SDF and polygon objects from scene
     */
    void ExtractSceneObjects(const Scene& scene);

    /**
     * @brief Setup shared lighting for both passes
     */
    void SetupSharedLighting(const Scene& scene);

    /**
     * @brief Update combined statistics
     */
    void UpdateStats();

    // Settings and state
    QualitySettings m_settings;
    RenderStats m_stats;
    bool m_debugMode = false;
    bool m_initialized = false;
    bool m_autoRenderOrder = false;

    // Sub-rasterizers
    std::unique_ptr<SDFRasterizer> m_sdfRasterizer;
    std::unique_ptr<PolygonRasterizer> m_polygonRasterizer;
    std::unique_ptr<HybridDepthMerge> m_depthMerge;

    // Output framebuffer
    std::unique_ptr<Framebuffer> m_outputFramebuffer;
    std::shared_ptr<Texture> m_outputColor;
    std::shared_ptr<Texture> m_outputDepth;

    // Intermediate framebuffers
    std::unique_ptr<Framebuffer> m_sdfFramebuffer;
    std::unique_ptr<Framebuffer> m_polygonFramebuffer;

    // Scene composition tracking
    uint32_t m_sdfObjectCount = 0;
    uint32_t m_polygonObjectCount = 0;

    // Camera data (cached for both passes)
    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projMatrix{1.0f};
    glm::vec3 m_cameraPosition{0.0f};

    // Timing
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    uint32_t m_frameCount = 0;
    float m_accumulatedTime = 0.0f;
};

} // namespace Nova
