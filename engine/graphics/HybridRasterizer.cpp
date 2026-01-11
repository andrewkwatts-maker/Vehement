#include "HybridRasterizer.hpp"
#include "core/Camera.hpp"
#include "scene/Scene.hpp"
#include "graphics/Texture.hpp"
#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace Nova {

HybridRasterizer::HybridRasterizer() = default;

HybridRasterizer::~HybridRasterizer() {
    Shutdown();
}

bool HybridRasterizer::Initialize(int width, int height) {
    if (m_initialized) {
        spdlog::warn("HybridRasterizer already initialized");
        return true;
    }

    spdlog::info("Initializing Hybrid Rasterizer ({}x{})", width, height);

    // Set default quality settings
    m_settings.renderWidth = width;
    m_settings.renderHeight = height;
    m_settings.renderOrder = QualitySettings::RenderOrder::SDFFirst;
    m_settings.enableDepthInterleaving = true;

    // Create sub-rasterizers
    m_sdfRasterizer = std::make_unique<SDFRasterizer>();
    if (!m_sdfRasterizer->Initialize(width, height)) {
        spdlog::error("Failed to initialize SDF rasterizer");
        return false;
    }

    m_polygonRasterizer = std::make_unique<PolygonRasterizer>();
    if (!m_polygonRasterizer->Initialize(width, height)) {
        spdlog::error("Failed to initialize polygon rasterizer");
        return false;
    }

    // Create depth merge system
    m_depthMerge = std::make_unique<HybridDepthMerge>();
    if (!m_depthMerge->Initialize(width, height)) {
        spdlog::error("Failed to initialize depth merge system");
        return false;
    }

    // Create output framebuffer
    m_outputFramebuffer = std::make_unique<Framebuffer>();
    if (!m_outputFramebuffer->Create(width, height, 1, true)) {
        spdlog::error("Failed to create output framebuffer");
        return false;
    }

    m_outputColor = m_outputFramebuffer->GetColorAttachment(0);
    m_outputDepth = m_outputFramebuffer->GetDepthAttachment();

    // Create intermediate framebuffers for separate passes
    m_sdfFramebuffer = std::make_unique<Framebuffer>();
    if (!m_sdfFramebuffer->Create(width, height, 1, true)) {
        spdlog::error("Failed to create SDF framebuffer");
        return false;
    }

    m_polygonFramebuffer = std::make_unique<Framebuffer>();
    if (!m_polygonFramebuffer->Create(width, height, 1, true)) {
        spdlog::error("Failed to create polygon framebuffer");
        return false;
    }

    m_initialized = true;
    spdlog::info("Hybrid Rasterizer initialized successfully");
    return true;
}

void HybridRasterizer::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down Hybrid Rasterizer");

    if (m_sdfRasterizer) m_sdfRasterizer->Shutdown();
    if (m_polygonRasterizer) m_polygonRasterizer->Shutdown();
    if (m_depthMerge) m_depthMerge->Shutdown();

    m_sdfRasterizer.reset();
    m_polygonRasterizer.reset();
    m_depthMerge.reset();
    m_outputFramebuffer.reset();
    m_sdfFramebuffer.reset();
    m_polygonFramebuffer.reset();
    m_outputColor.reset();
    m_outputDepth.reset();

    m_initialized = false;
}

void HybridRasterizer::Resize(int width, int height) {
    if (!m_initialized) return;

    spdlog::info("Resizing Hybrid Rasterizer to {}x{}", width, height);

    m_settings.renderWidth = width;
    m_settings.renderHeight = height;

    // Resize sub-rasterizers
    m_sdfRasterizer->Resize(width, height);
    m_polygonRasterizer->Resize(width, height);
    m_depthMerge->Resize(width, height);

    // Resize framebuffers
    m_outputFramebuffer->Resize(width, height);
    m_sdfFramebuffer->Resize(width, height);
    m_polygonFramebuffer->Resize(width, height);
}

void HybridRasterizer::BeginFrame(const Camera& camera) {
    m_frameStartTime = std::chrono::high_resolution_clock::now();
    m_stats.Reset();

    // Cache camera data
    m_viewMatrix = camera.GetViewMatrix();
    m_projMatrix = camera.GetProjectionMatrix();
    m_cameraPosition = camera.GetPosition();

    // Begin frame for sub-rasterizers
    m_sdfRasterizer->BeginFrame(camera);
    m_polygonRasterizer->BeginFrame(camera);
}

void HybridRasterizer::EndFrame() {
    // End frame for sub-rasterizers
    m_sdfRasterizer->EndFrame();
    m_polygonRasterizer->EndFrame();

    // Update combined statistics
    UpdateStats();

    m_frameCount++;
}

void HybridRasterizer::Render(const Scene& scene, const Camera& camera) {
    if (!m_initialized) return;

    ScopedTimer timer(m_stats.frameTimeMs);

    // Extract SDF and polygon objects from scene
    ExtractSceneObjects(scene);

    // Setup shared lighting
    SetupSharedLighting(scene);

    // Determine render order
    auto renderOrder = m_settings.renderOrder;
    if (m_autoRenderOrder) {
        renderOrder = DetermineOptimalRenderOrder(scene);
    }

    // Render based on order
    switch (renderOrder) {
        case QualitySettings::RenderOrder::SDFFirst:
            RenderSDFFirst(scene, camera);
            break;

        case QualitySettings::RenderOrder::PolygonFirst:
            RenderPolygonFirst(scene, camera);
            break;

        case QualitySettings::RenderOrder::Auto:
            // Auto should have been resolved above
            RenderSDFFirst(scene, camera);
            break;
    }

    // Composite final results if needed
    CompositeResults();
}

void HybridRasterizer::SetQualitySettings(const QualitySettings& settings) {
    m_settings = settings;

    // Propagate settings to sub-rasterizers
    m_sdfRasterizer->SetQualitySettings(settings);
    m_polygonRasterizer->SetQualitySettings(settings);

    // Update depth merge mode
    DepthMergeMode depthMode;
    switch (settings.renderOrder) {
        case QualitySettings::RenderOrder::SDFFirst:
            depthMode = DepthMergeMode::SDFFirst;
            break;
        case QualitySettings::RenderOrder::PolygonFirst:
            depthMode = DepthMergeMode::PolygonFirst;
            break;
        default:
            depthMode = DepthMergeMode::Interleaved;
            break;
    }
    m_depthMerge->SetMode(depthMode);
}

bool HybridRasterizer::SupportsFeature(RenderFeature feature) const {
    switch (feature) {
        case RenderFeature::SDFRendering:
        case RenderFeature::PolygonRendering:
        case RenderFeature::HybridRendering:
        case RenderFeature::ComputeShaders:
        case RenderFeature::TileBasedCulling:
        case RenderFeature::PBRShading:
        case RenderFeature::ShadowMapping:
        case RenderFeature::DepthInterleaving:
        case RenderFeature::ClusteredLighting:
            return true;
        case RenderFeature::RTXRaytracing:
            return false;
        default:
            return false;
    }
}

std::shared_ptr<Texture> HybridRasterizer::GetOutputColor() const {
    return m_outputColor;
}

std::shared_ptr<Texture> HybridRasterizer::GetOutputDepth() const {
    return m_outputDepth;
}

void HybridRasterizer::SetDebugMode(bool enabled) {
    m_debugMode = enabled;
    m_sdfRasterizer->SetDebugMode(enabled);
    m_polygonRasterizer->SetDebugMode(enabled);
}

void HybridRasterizer::SetRenderOrder(QualitySettings::RenderOrder order) {
    m_settings.renderOrder = order;
}

QualitySettings::RenderOrder HybridRasterizer::GetRenderOrder() const {
    return m_settings.renderOrder;
}

float HybridRasterizer::GetSDFPercentage() const {
    uint32_t total = m_sdfObjectCount + m_polygonObjectCount;
    if (total == 0) return 0.0f;
    return static_cast<float>(m_sdfObjectCount) / static_cast<float>(total);
}

QualitySettings::RenderOrder HybridRasterizer::DetermineOptimalRenderOrder(const Scene& scene) {
    // Heuristic: If scene is mostly SDF, render SDF first
    // If scene is mostly polygons, render polygons first
    float sdfPercentage = GetSDFPercentage();

    if (sdfPercentage > 0.6f) {
        return QualitySettings::RenderOrder::SDFFirst;
    } else if (sdfPercentage < 0.4f) {
        return QualitySettings::RenderOrder::PolygonFirst;
    } else {
        // Mixed scene - SDF first tends to be faster for early-Z rejection
        return QualitySettings::RenderOrder::SDFFirst;
    }
}

void HybridRasterizer::RenderSDFFirst(const Scene& scene, const Camera& camera) {
    // Phase 1: Render SDFs to intermediate framebuffer
    {
        ScopedTimer timer(m_stats.sdfPassMs);

        // Clear SDF framebuffer
        m_sdfFramebuffer->Bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Prepare depth merge for SDF pass
        if (m_settings.enableDepthInterleaving) {
            m_depthMerge->PrepareSDFPass(DepthMergeMode::SDFFirst);
        }

        // Render SDFs
        m_sdfRasterizer->Render(scene, camera);

        Framebuffer::Unbind();
    }

    // Phase 2: Copy SDF depth to output framebuffer
    if (m_settings.enableDepthInterleaving) {
        auto sdfDepth = m_sdfRasterizer->GetOutputDepth();
        m_depthMerge->CopyDepth(sdfDepth, m_outputDepth, false);
    }

    // Phase 3: Render polygons to output framebuffer with SDF depth test
    {
        ScopedTimer timer(m_stats.polygonPassMs);

        // Bind output framebuffer
        m_outputFramebuffer->Bind();

        // Copy SDF color to output
        glBlitNamedFramebuffer(
            m_sdfFramebuffer->GetID(), m_outputFramebuffer->GetID(),
            0, 0, m_settings.renderWidth, m_settings.renderHeight,
            0, 0, m_settings.renderWidth, m_settings.renderHeight,
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );

        // Prepare depth merge for polygon pass (will use existing SDF depth)
        if (m_settings.enableDepthInterleaving) {
            m_depthMerge->PreparePolygonPass(DepthMergeMode::SDFFirst);
        }

        // Render polygons (they will depth test against SDF depth)
        m_polygonRasterizer->Render(scene, camera);

        Framebuffer::Unbind();
    }

    // Phase 4: Merge depth buffers if interleaving enabled
    if (m_settings.enableDepthInterleaving) {
        ScopedTimer timer(m_stats.depthMergeMs);
        MergeDepthBuffers();
    }
}

void HybridRasterizer::RenderPolygonFirst(const Scene& scene, const Camera& camera) {
    // Phase 1: Render polygons to intermediate framebuffer
    {
        ScopedTimer timer(m_stats.polygonPassMs);

        // Clear polygon framebuffer
        m_polygonFramebuffer->Bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Prepare depth merge for polygon pass
        if (m_settings.enableDepthInterleaving) {
            m_depthMerge->PreparePolygonPass(DepthMergeMode::PolygonFirst);
        }

        // Render polygons
        m_polygonRasterizer->Render(scene, camera);

        Framebuffer::Unbind();
    }

    // Phase 2: Copy polygon depth to output framebuffer
    if (m_settings.enableDepthInterleaving) {
        auto polygonDepth = m_polygonRasterizer->GetOutputDepth();
        m_depthMerge->CopyDepth(polygonDepth, m_outputDepth, false);
    }

    // Phase 3: Render SDFs to output framebuffer with polygon depth test
    {
        ScopedTimer timer(m_stats.sdfPassMs);

        // Bind output framebuffer
        m_outputFramebuffer->Bind();

        // Copy polygon color to output
        glBlitNamedFramebuffer(
            m_polygonFramebuffer->GetID(), m_outputFramebuffer->GetID(),
            0, 0, m_settings.renderWidth, m_settings.renderHeight,
            0, 0, m_settings.renderWidth, m_settings.renderHeight,
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );

        // Prepare depth merge for SDF pass (will use existing polygon depth)
        if (m_settings.enableDepthInterleaving) {
            m_depthMerge->PrepareSDFPass(DepthMergeMode::PolygonFirst);
        }

        // Render SDFs (they will depth test against polygon depth)
        m_sdfRasterizer->Render(scene, camera);

        Framebuffer::Unbind();
    }

    // Phase 4: Merge depth buffers if interleaving enabled
    if (m_settings.enableDepthInterleaving) {
        ScopedTimer timer(m_stats.depthMergeMs);
        MergeDepthBuffers();
    }
}

void HybridRasterizer::CompositeResults() {
    // Results are already composited during rendering
    // This function is for future extensions (e.g., post-processing)
}

void HybridRasterizer::MergeDepthBuffers() {
    auto sdfDepth = m_sdfRasterizer->GetOutputDepth();
    auto polygonDepth = m_polygonRasterizer->GetOutputDepth();

    if (sdfDepth && polygonDepth && m_outputDepth) {
        m_depthMerge->MergeDepthBuffers(sdfDepth, polygonDepth, m_outputDepth);
    }
}

void HybridRasterizer::ExtractSceneObjects(const Scene& scene) {
    // TODO: Extract SDF and polygon objects from scene
    // This is a placeholder - actual implementation depends on Scene class

    // For now, assume we can query object counts
    m_sdfObjectCount = 0;
    m_polygonObjectCount = 0;

    // In a real implementation:
    // - Iterate through scene nodes
    // - Classify each as SDF or polygon
    // - Submit to appropriate rasterizer
}

void HybridRasterizer::SetupSharedLighting(const Scene& scene) {
    // TODO: Setup lighting that works for both SDF and polygon passes
    // This includes:
    // - Directional lights (sun)
    // - Point lights
    // - Spot lights
    // - Environment maps
    // - Shadow maps (shared between passes)
}

void HybridRasterizer::UpdateStats() {
    // Calculate frame time
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> frameDuration = frameEndTime - m_frameStartTime;
    m_stats.frameTimeMs = frameDuration.count();

    // Aggregate stats from sub-rasterizers
    const auto& sdfStats = m_sdfRasterizer->GetStats();
    const auto& polygonStats = m_polygonRasterizer->GetStats();

    m_stats.cpuTimeMs = sdfStats.cpuTimeMs + polygonStats.cpuTimeMs;
    m_stats.gpuTimeMs = sdfStats.gpuTimeMs + polygonStats.gpuTimeMs;

    m_stats.drawCalls = sdfStats.drawCalls + polygonStats.drawCalls;
    m_stats.computeDispatches = sdfStats.computeDispatches;
    m_stats.trianglesRendered = polygonStats.trianglesRendered;
    m_stats.sdfObjectsRendered = sdfStats.sdfObjectsRendered;
    m_stats.polygonObjectsRendered = polygonStats.polygonObjectsRendered;

    m_stats.tilesProcessed = sdfStats.tilesProcessed;
    m_stats.tilesCulled = sdfStats.tilesCulled;
    m_stats.objectsCulled = sdfStats.objectsCulled + polygonStats.objectsCulled;

    // Calculate FPS
    m_accumulatedTime += m_stats.frameTimeMs;
    if (m_accumulatedTime >= 1000.0f) {
        m_stats.fps = static_cast<int>(m_frameCount * 1000.0f / m_accumulatedTime);
        m_frameCount = 0;
        m_accumulatedTime = 0.0f;
    }
}

} // namespace Nova
