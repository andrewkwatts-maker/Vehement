#include "graphics/OptimizedRenderer.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Material.hpp"
#include "graphics/Shader.hpp"
#include "scene/SceneNode.hpp"
#include "scene/Camera.hpp"
#include "config/Config.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>

namespace Nova {

// ============================================================================
// PerformanceStats Implementation
// ============================================================================

void PerformanceStats::Aggregate(const Batching::Stats& batchStats,
                                  const Culler::Stats& cullStats,
                                  const LODStats& lodStats,
                                  const RenderQueueStats& queueStats) {
    // Batching stats
    batchedDrawCalls = batchStats.totalBatches;
    instancedDrawCalls = batchStats.instancedDrawCalls;
    drawCallsSaved = batchStats.drawCallsSaved;

    // Culling stats
    totalObjects = cullStats.totalObjects;
    visibleObjects = cullStats.visibleObjects;
    frustumCulled = cullStats.frustumCulled;
    occlusionCulled = cullStats.occlusionCulled;
    distanceCulled = cullStats.distanceCulled;
    cullingEfficiency = cullStats.cullingEfficiency;

    // LOD stats
    verticesAfterLOD = lodStats.trianglesRendered * 3;  // Approximate
    trianglesAfterLOD = lodStats.trianglesRendered;

    // Queue stats
    totalDrawCalls = queueStats.drawCalls;
    shaderChanges = queueStats.shaderChanges;
    materialChanges = queueStats.materialChanges;
    textureChanges = queueStats.textureChanges;
    stateChanges = queueStats.stateChanges;
}

// ============================================================================
// OptimizedRenderer Implementation
// ============================================================================

OptimizedRenderer::OptimizedRenderer()
    : m_batching(std::make_unique<Batching>())
    , m_culler(std::make_unique<Culler>())
    , m_lodManager(std::make_unique<LODManager>())
    , m_textureAtlas(std::make_unique<TextureAtlas>())
    , m_renderQueue(std::make_unique<RenderQueue>()) {
}

OptimizedRenderer::~OptimizedRenderer() {
    Shutdown();
}

bool OptimizedRenderer::Initialize(Renderer* renderer, const std::string& configPath) {
    if (m_initialized) {
        return true;
    }

    m_renderer = renderer;

    // Load settings from config if provided
    if (!configPath.empty()) {
        LoadSettings(configPath);
    }

    // Initialize subsystems
    if (!m_batching->Initialize(m_settings.batchConfig)) {
        spdlog::error("Failed to initialize batching system");
        return false;
    }

    if (!m_culler->Initialize(m_settings.cullingConfig)) {
        spdlog::error("Failed to initialize culling system");
        return false;
    }

    if (!m_lodManager->Initialize(m_settings.lodConfig)) {
        spdlog::error("Failed to initialize LOD system");
        return false;
    }

    if (!m_textureAtlas->Initialize(m_settings.atlasConfig)) {
        spdlog::error("Failed to initialize texture atlas");
        return false;
    }

    if (!m_renderQueue->Initialize(m_settings.queueConfig)) {
        spdlog::error("Failed to initialize render queue");
        return false;
    }

    // Create GPU timer query
    glGenQueries(1, &m_gpuTimerQuery);
    m_gpuTimerAvailable = true;

    m_initialized = true;
    spdlog::info("OptimizedRenderer initialized successfully");

    return true;
}

void OptimizedRenderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_gpuTimerQuery) {
        glDeleteQueries(1, &m_gpuTimerQuery);
        m_gpuTimerQuery = 0;
    }

    m_batching->Shutdown();
    m_culler->Shutdown();
    m_lodManager->Shutdown();
    m_textureAtlas->Shutdown();
    m_renderQueue->Shutdown();

    m_initialized = false;
}

bool OptimizedRenderer::LoadSettings(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            spdlog::warn("Could not open graphics config: {}", configPath);
            return false;
        }

        nlohmann::json json;
        file >> json;

        auto& graphics = json["graphics"];

        // Batching config
        if (graphics.contains("batching")) {
            auto& batch = graphics["batching"];
            m_settings.batchConfig.enabled = batch.value("enabled", true);
            m_settings.batchConfig.maxBatchSize = batch.value("max_batch_size", 1000);
            m_settings.batchConfig.minInstancesForBatching = batch.value("min_instances_for_batching", 2);
            m_settings.batchConfig.useInstancedRendering = batch.value("use_instanced_rendering", true);
            m_settings.batchConfig.usePersistentMapping = batch.value("use_persistent_mapping", true);
            m_settings.batchConfig.useIndirectRendering = batch.value("use_indirect_rendering", false);
        }

        // Culling config
        if (graphics.contains("culling")) {
            auto& cull = graphics["culling"];
            m_settings.cullingConfig.frustumCullingEnabled = cull.value("frustum", true);
            m_settings.cullingConfig.occlusionCullingEnabled = cull.value("occlusion", true);
            m_settings.cullingConfig.maxRenderDistance = cull.value("distance", 500.0f);
            m_settings.cullingConfig.smallObjectThreshold = cull.value("small_object_threshold", 0.01f);
            m_settings.cullingConfig.occlusionQueryDelay = cull.value("occlusion_query_delay", 0.1f);
        }

        // LOD config
        if (graphics.contains("lod")) {
            auto& lod = graphics["lod"];
            m_settings.lodConfig.enabled = lod.value("enabled", true);
            m_settings.lodConfig.lodBias = lod.value("lod_bias", 0.0f);
            m_settings.lodConfig.hysteresis = lod.value("hysteresis", 1.1f);
            m_settings.lodConfig.enableCrossfade = lod.value("enable_crossfade", false);

            if (lod.contains("distances") && lod["distances"].is_array()) {
                for (size_t i = 0; i < lod["distances"].size() && i < MAX_LOD_LEVELS; i++) {
                    m_settings.lodConfig.distances[i] = lod["distances"][i];
                }
            }
        }

        // Texture atlas config
        if (graphics.contains("texture_atlas")) {
            auto& atlas = graphics["texture_atlas"];
            m_settings.atlasConfig.maxSize = atlas.value("max_size", 4096);
            m_settings.atlasConfig.padding = atlas.value("padding", 1);
            m_settings.atlasConfig.generateMipmaps = atlas.value("generate_mipmaps", true);
            m_settings.atlasConfig.useCompression = atlas.value("use_compression", false);
        }

        // Render queue config
        if (graphics.contains("render_queue")) {
            auto& queue = graphics["render_queue"];
            m_settings.queueConfig.sortByState = queue.value("sort_by_state", true);
            m_settings.queueConfig.sortByDepth = queue.value("sort_by_depth", true);
            m_settings.queueConfig.enableInstancing = queue.value("enable_instancing", true);
            m_settings.queueConfig.separateTransparent = queue.value("separate_transparent", true);
        }

        // Instancing
        if (graphics.contains("instancing")) {
            auto& inst = graphics["instancing"];
            m_settings.instancingEnabled = inst.value("enabled", true);
            m_settings.minInstanceCount = inst.value("min_instances", 10);
        }

        // Debug
        if (graphics.contains("debug")) {
            auto& debug = graphics["debug"];
            m_settings.showStats = debug.value("show_culling_stats", false);
            m_settings.logPerformanceWarnings = debug.value("log_performance_warnings", true);
            m_settings.performanceWarningThresholdMs = debug.value("performance_warning_threshold_ms", 16.67f);
        }

        spdlog::info("Loaded graphics settings from: {}", configPath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error loading graphics config: {}", e.what());
        return false;
    }
}

void OptimizedRenderer::ApplyQualityPreset(const std::string& preset) {
    m_settings.qualityPreset = preset;

    if (preset == "low") {
        m_settings.cullingConfig.maxRenderDistance = 200.0f;
        m_settings.cullingConfig.occlusionCullingEnabled = false;
        m_settings.lodConfig.lodBias = 2.0f;
        m_settings.atlasConfig.maxSize = 2048;
    } else if (preset == "medium") {
        m_settings.cullingConfig.maxRenderDistance = 350.0f;
        m_settings.cullingConfig.occlusionCullingEnabled = true;
        m_settings.lodConfig.lodBias = 1.0f;
        m_settings.atlasConfig.maxSize = 4096;
    } else if (preset == "high") {
        m_settings.cullingConfig.maxRenderDistance = 500.0f;
        m_settings.cullingConfig.occlusionCullingEnabled = true;
        m_settings.lodConfig.lodBias = 0.0f;
        m_settings.atlasConfig.maxSize = 4096;
    } else if (preset == "ultra") {
        m_settings.cullingConfig.maxRenderDistance = 1000.0f;
        m_settings.cullingConfig.occlusionCullingEnabled = true;
        m_settings.lodConfig.lodBias = -0.5f;
        m_settings.atlasConfig.maxSize = 8192;
    }

    // Update subsystem configs
    m_culler->SetConfig(m_settings.cullingConfig);
    m_lodManager->SetConfig(m_settings.lodConfig);
    m_textureAtlas->SetConfig(m_settings.atlasConfig);

    spdlog::info("Applied quality preset: {}", preset);
}

void OptimizedRenderer::BeginFrame(const Camera& camera) {
    m_frameStartTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    m_frameNumber++;

    m_activeCamera = &camera;
    m_viewProjection = camera.GetProjectionView();

    m_stats.Reset();

    // Begin subsystems
    m_culler->BeginFrame(camera);
    m_lodManager->Update(camera);
    m_batching->BeginFrame();
    m_renderQueue->BeginFrame(camera);
}

void OptimizedRenderer::EndFrame() {
    // End subsystems
    m_culler->EndFrame();
    m_batching->EndFrame();
    m_renderQueue->EndFrame();

    // Gather statistics
    UpdatePerformanceStats();

    // Log warnings if needed
    if (m_settings.logPerformanceWarnings &&
        m_stats.frameTimeMs > m_settings.performanceWarningThresholdMs) {
        spdlog::warn("Frame time {:.2f}ms exceeds threshold {:.2f}ms",
                     m_stats.frameTimeMs, m_settings.performanceWarningThresholdMs);
    }
}

void OptimizedRenderer::Submit(SceneNode* node) {
    if (!node || !node->IsVisible()) {
        return;
    }

    CollectSceneNode(node, glm::mat4(1.0f));
}

void OptimizedRenderer::Submit(const std::shared_ptr<Mesh>& mesh,
                                const std::shared_ptr<Material>& material,
                                const glm::mat4& transform,
                                uint32_t objectID) {
    if (!mesh || !material) {
        return;
    }

    // Create AABB for culling
    AABB worldBounds = AABB(mesh->GetBoundsMin(), mesh->GetBoundsMax()).Transform(transform);

    // Test visibility
    if (!m_culler->IsVisible(worldBounds)) {
        return;
    }

    // Submit to render queue
    RenderItem& item = m_renderQueue->Submit(mesh, material, transform);
    item.objectID = objectID;

    // Also submit to batching system
    m_batching->Submit(mesh, material, transform, objectID);
}

void OptimizedRenderer::SubmitWithLOD(uint32_t lodGroupID,
                                       const std::shared_ptr<Material>& material,
                                       const glm::mat4& transform) {
    LODGroup* group = m_lodManager->GetLODGroup(lodGroupID);
    if (!group) {
        return;
    }

    // Get appropriate mesh for current LOD level
    const auto& mesh = group->GetCurrentMesh();
    if (!mesh) {
        return;
    }

    // Submit with the LOD mesh
    RenderItem& item = m_renderQueue->Submit(mesh, material, transform);
    item.lodLevel = group->currentLevel;
}

void OptimizedRenderer::Render() {
    if (!m_activeCamera || !m_renderer) {
        return;
    }

    // Sort render queue
    m_renderQueue->Sort();

    // Render opaque objects
    RenderOpaque();

    // Render transparent objects
    RenderTransparent();
}

void OptimizedRenderer::RenderShadows(const glm::mat4& lightViewProjection) {
    m_renderQueue->Execute(RenderPass::Shadow,
        [this, &lightViewProjection](const RenderItem& item) {
            if (item.castsShadow) {
                RenderItem shadowItem = item;
                // Would use shadow shader here
                DrawItem(shadowItem);
            }
        });
}

void OptimizedRenderer::RenderOpaque() {
    m_renderQueue->Execute(RenderPass::Opaque,
        [this](const RenderItem& item) {
            DrawItem(item);
        });
}

void OptimizedRenderer::RenderTransparent() {
    m_renderer->SetBlending(true);

    m_renderQueue->Execute(RenderPass::Transparent,
        [this](const RenderItem& item) {
            DrawItem(item);
        });

    m_renderer->SetBlending(false);
}

void OptimizedRenderer::DrawItem(const RenderItem& item) {
    if (!item.mesh || !item.material) {
        return;
    }

    m_renderer->DrawMesh(*item.mesh, *item.material, item.transform);
}

void OptimizedRenderer::CollectSceneNode(SceneNode* node, const glm::mat4& parentTransform) {
    if (!node || !node->IsVisible()) {
        return;
    }

    glm::mat4 worldTransform = parentTransform * node->GetLocalTransform();

    // Submit this node if it has a mesh
    if (node->HasMesh() && node->HasMaterial()) {
        Submit(node->GetMesh(), node->GetMaterial(), worldTransform);
    }

    // Recursively collect children
    for (const auto& child : node->GetChildren()) {
        CollectSceneNode(child.get(), worldTransform);
    }
}

void OptimizedRenderer::UpdatePerformanceStats() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    m_stats.frameTimeMs = static_cast<float>(now - m_frameStartTime) / 1000000.0f;

    // Aggregate from subsystems
    m_stats.Aggregate(
        m_batching->GetStats(),
        m_culler->GetStats(),
        m_lodManager->GetStats(),
        m_renderQueue->GetStats()
    );
}

void OptimizedRenderer::SetSettings(const GraphicsSettings& settings) {
    m_settings = settings;

    // Update all subsystems
    m_batching->SetConfig(settings.batchConfig);
    m_culler->SetConfig(settings.cullingConfig);
    m_lodManager->SetConfig(settings.lodConfig);
    m_textureAtlas->SetConfig(settings.atlasConfig);
    m_renderQueue->SetConfig(settings.queueConfig);
}

int OptimizedRenderer::RegisterStaticBatch(
    const std::vector<std::shared_ptr<Mesh>>& meshes,
    const std::vector<std::shared_ptr<Material>>& materials,
    const std::vector<glm::mat4>& transforms) {
    return m_batching->CreateStaticBatch(meshes, materials, transforms);
}

uint32_t OptimizedRenderer::CreateLODGroup(const std::string& name) {
    return m_lodManager->CreateLODGroup(name);
}

bool OptimizedRenderer::AddLODLevel(uint32_t groupID, std::shared_ptr<Mesh> mesh, float maxDistance) {
    return m_lodManager->AddLODLevel(groupID, std::move(mesh), maxDistance);
}

bool OptimizedRenderer::AddTextureToAtlas(const std::string& name, const std::shared_ptr<Texture>& texture) {
    return m_textureAtlas->AddTexture(name, texture);
}

bool OptimizedRenderer::BuildTextureAtlas() {
    return m_textureAtlas->Build();
}

uint32_t OptimizedRenderer::RegisterCullable(const AABB& bounds, void* userData) {
    return m_culler->RegisterObject(bounds, userData);
}

void OptimizedRenderer::UpdateCullableBounds(uint32_t id, const AABB& newBounds) {
    m_culler->UpdateObjectBounds(id, newBounds);
}

// ============================================================================
// ScopedRenderPass Implementation
// ============================================================================

ScopedRenderPass::ScopedRenderPass(OptimizedRenderer& renderer, RenderPass pass,
                                     const std::string& name)
    : m_renderer(renderer)
    , m_pass(pass) {
    m_startTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    if (!name.empty()) {
        RenderProfiler::Instance().BeginSection(name);
    }
}

ScopedRenderPass::~ScopedRenderPass() {
    auto endTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    float durationMs = static_cast<float>(endTime - m_startTime) / 1000000.0f;
    (void)durationMs; // Would log or record this
}

// ============================================================================
// GPUTimer Implementation
// ============================================================================

GPUTimer::GPUTimer() {
    glGenQueries(1, &m_queryStart);
    glGenQueries(1, &m_queryEnd);
}

GPUTimer::~GPUTimer() {
    if (m_queryStart) glDeleteQueries(1, &m_queryStart);
    if (m_queryEnd) glDeleteQueries(1, &m_queryEnd);
}

void GPUTimer::Begin() {
    glQueryCounter(m_queryStart, GL_TIMESTAMP);
    m_resultCached = false;
}

void GPUTimer::End() {
    glQueryCounter(m_queryEnd, GL_TIMESTAMP);
}

float GPUTimer::GetElapsedMs() const {
    if (m_resultCached) {
        return m_cachedResult;
    }

    GLuint64 startTime, endTime;
    glGetQueryObjectui64v(m_queryStart, GL_QUERY_RESULT, &startTime);
    glGetQueryObjectui64v(m_queryEnd, GL_QUERY_RESULT, &endTime);

    m_cachedResult = static_cast<float>(endTime - startTime) / 1000000.0f;
    m_resultCached = true;

    return m_cachedResult;
}

bool GPUTimer::IsResultAvailable() const {
    GLuint available = GL_FALSE;
    glGetQueryObjectuiv(m_queryEnd, GL_QUERY_RESULT_AVAILABLE, &available);
    return available == GL_TRUE;
}

// ============================================================================
// RenderProfiler Implementation
// ============================================================================

RenderProfiler& RenderProfiler::Instance() {
    static RenderProfiler instance;
    return instance;
}

void RenderProfiler::BeginFrame() {
    m_drawCalls = 0;
    m_stateChanges = 0;
    m_triangles = 0;
}

void RenderProfiler::EndFrame() {
    // Finalize any open sections
}

void RenderProfiler::BeginSection(const std::string& name) {
    auto it = m_sectionIndices.find(name);
    int index;

    if (it == m_sectionIndices.end()) {
        SectionStats stats;
        stats.name = name;
        index = static_cast<int>(m_sections.size());
        m_sections.push_back(stats);
        m_sectionIndices[name] = index;
    } else {
        index = it->second;
    }

    // Ensure we have space for start times
    if (m_sectionStartTimes.size() <= static_cast<size_t>(index)) {
        m_sectionStartTimes.resize(index + 1);
    }

    m_sectionStartTimes[index] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

void RenderProfiler::EndSection(const std::string& name) {
    auto it = m_sectionIndices.find(name);
    if (it == m_sectionIndices.end()) {
        return;
    }

    int index = it->second;
    auto endTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    float durationMs = static_cast<float>(endTime - m_sectionStartTimes[index]) / 1000000.0f;

    m_sections[index].totalTimeMs += durationMs;
    m_sections[index].callCount++;
    m_sections[index].avgTimeMs = m_sections[index].totalTimeMs / m_sections[index].callCount;
}

void RenderProfiler::RecordDrawCall(uint32_t triangles) {
    m_drawCalls++;
    m_triangles += triangles;
}

void RenderProfiler::RecordStateChange() {
    m_stateChanges++;
}

void RenderProfiler::RecordTextureChange() {
    m_stateChanges++;
}

void RenderProfiler::Reset() {
    m_sections.clear();
    m_sectionIndices.clear();
    m_sectionStartTimes.clear();
    m_drawCalls = m_stateChanges = m_triangles = 0;
}

} // namespace Nova
