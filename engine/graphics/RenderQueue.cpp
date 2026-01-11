#include "graphics/RenderQueue.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Material.hpp"
#include "graphics/Shader.hpp"
#include "scene/Camera.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <thread>

namespace Nova {

// ============================================================================
// RenderQueue Implementation
// ============================================================================

RenderQueue::RenderQueue() = default;

RenderQueue::~RenderQueue() {
    Shutdown();
}

bool RenderQueue::Initialize(const RenderQueueConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    // Pre-allocate storage
    m_items.reserve(m_config.maxItemsPerBucket);
    m_opaqueItems.reserve(m_config.maxItemsPerBucket);
    m_transparentItems.reserve(m_config.maxItemsPerBucket / 4);

    for (auto& bucket : m_passBuckets) {
        bucket.reserve(m_config.maxItemsPerBucket / static_cast<int>(RenderPass::Count));
    }

    m_initialized = true;
    spdlog::info("Render Queue initialized");

    return true;
}

void RenderQueue::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Clear();
    m_initialized = false;
}

void RenderQueue::Clear() {
    m_items.clear();
    m_opaqueItems.clear();
    m_transparentItems.clear();

    for (auto& bucket : m_passBuckets) {
        bucket.clear();
    }

    m_stats.Reset();
    m_sorted = false;
}

void RenderQueue::BeginFrame(const Camera& camera) {
    Clear();

    m_cameraPosition = camera.GetPosition();
    m_cameraForward = camera.GetForward();
    m_viewProjection = camera.GetProjectionView();
}

void RenderQueue::EndFrame() {
    if (!m_sorted) {
        Sort();
    }
}

void RenderQueue::Submit(const RenderItem& item) {
    if (!item.IsValid() || !item.visible) {
        return;
    }

    m_items.push_back(item);
    m_stats.totalItems++;
    m_sorted = false;
}

RenderItem& RenderQueue::Submit(const std::shared_ptr<Mesh>& mesh,
                                 const std::shared_ptr<Material>& material,
                                 const glm::mat4& transform) {
    RenderItem item;
    item.mesh = mesh;
    item.material = material;
    item.transform = transform;

    // Auto-detect blend mode
    if (material && material->IsTransparent()) {
        item.blendMode = BlendMode::AlphaBlend;
        item.pass = RenderPass::Transparent;
    } else {
        item.blendMode = BlendMode::Opaque;
        item.pass = RenderPass::Opaque;
    }

    // Extract IDs for sorting
    if (material) {
        if (auto shader = material->GetShaderPtr()) {
            item.shaderID = shader->GetID();
        }
    }

    m_items.push_back(item);
    m_stats.totalItems++;
    m_sorted = false;

    return m_items.back();
}

void RenderQueue::Sort() {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Compute sort keys for all items
    ComputeSortKeys();

    // Bucket items by pass
    BucketItems();

    // Sort opaque items (front-to-back, minimize state changes)
    SortOpaque();

    // Sort transparent items (back-to-front)
    SortTransparent();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.sortTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    m_sorted = true;
}

void RenderQueue::Execute(RenderPass pass,
                          const std::function<void(const RenderItem&)>& renderFunc) {
    if (!m_sorted) {
        Sort();
    }

    size_t passIndex = static_cast<size_t>(pass);
    if (passIndex >= m_passBuckets.size()) {
        return;
    }

    const auto& bucket = m_passBuckets[passIndex];

    uint32_t lastShaderID = 0;
    uint32_t lastMaterialID = 0;
    uint32_t lastTextureID = 0;

    for (const RenderItem* item : bucket) {
        if (!item || !item->visible) continue;

        // Track state changes
        if (item->shaderID != lastShaderID) {
            m_stats.shaderChanges++;
            lastShaderID = item->shaderID;
        }
        if (item->materialID != lastMaterialID) {
            m_stats.materialChanges++;
            lastMaterialID = item->materialID;
        }
        if (item->textureID != lastTextureID) {
            m_stats.textureChanges++;
            lastTextureID = item->textureID;
        }

        renderFunc(*item);
        m_stats.drawCalls++;
    }

    m_stats.stateChanges = m_stats.shaderChanges + m_stats.materialChanges + m_stats.textureChanges;
}

void RenderQueue::ExecuteAll(const std::function<void(RenderPass, const RenderItem&)>& renderFunc) {
    if (!m_sorted) {
        Sort();
    }

    // Execute passes in order
    const RenderPass passOrder[] = {
        RenderPass::Shadow,
        RenderPass::Depth,
        RenderPass::GBuffer,
        RenderPass::Opaque,
        RenderPass::Transparent,
        RenderPass::PostProcess,
        RenderPass::UI,
        RenderPass::Debug
    };

    for (RenderPass pass : passOrder) {
        Execute(pass, [&](const RenderItem& item) {
            renderFunc(pass, item);
        });
    }
}

const std::vector<RenderItem*>& RenderQueue::GetPassItems(RenderPass pass) const {
    size_t passIndex = static_cast<size_t>(pass);
    if (passIndex < m_passBuckets.size()) {
        return m_passBuckets[passIndex];
    }

    static std::vector<RenderItem*> empty;
    return empty;
}

void RenderQueue::SetConfig(const RenderQueueConfig& config) {
    m_config = config;
}

void RenderQueue::Reserve(size_t capacity) {
    m_items.reserve(capacity);
    m_opaqueItems.reserve(capacity);
    m_transparentItems.reserve(capacity / 4);
}

bool RenderQueue::HasItems(RenderPass pass) const {
    size_t passIndex = static_cast<size_t>(pass);
    return passIndex < m_passBuckets.size() && !m_passBuckets[passIndex].empty();
}

void RenderQueue::SetCustomSort(RenderPass pass, SortFunction sortFunc) {
    size_t passIndex = static_cast<size_t>(pass);
    if (passIndex < m_customSortFuncs.size()) {
        m_customSortFuncs[passIndex] = std::move(sortFunc);
    }
}

std::vector<RenderItem*> RenderQueue::Filter(const std::function<bool(const RenderItem&)>& predicate) const {
    std::vector<RenderItem*> result;
    result.reserve(m_items.size());

    for (const auto& item : m_items) {
        if (predicate(item)) {
            result.push_back(const_cast<RenderItem*>(&item));
        }
    }

    return result;
}

void RenderQueue::ComputeSortKeys() {
    for (auto& item : m_items) {
        item.sortKey = ComputeSortKey(item);

        // Calculate depth from camera
        glm::vec3 itemPos = glm::vec3(item.transform[3]);
        item.depth = glm::dot(itemPos - m_cameraPosition, m_cameraForward);
    }
}

void RenderQueue::SortOpaque() {
    if (!m_config.sortByState) {
        return;
    }

    // Sort opaque: front-to-back for early-Z, but prioritize state coherence
    std::sort(m_opaqueItems.begin(), m_opaqueItems.end(),
        [this](const RenderItem* a, const RenderItem* b) {
            // Primary: sort by state (shader, material, texture)
            if (a->sortKey != b->sortKey) {
                return a->sortKey < b->sortKey;
            }
            // Secondary: front-to-back for depth
            return a->depth < b->depth;
        });
}

void RenderQueue::SortTransparent() {
    // Sort transparent: strictly back-to-front for correct blending
    std::sort(m_transparentItems.begin(), m_transparentItems.end(),
        [](const RenderItem* a, const RenderItem* b) {
            return a->depth > b->depth;  // Back-to-front
        });
}

void RenderQueue::BucketItems() {
    m_opaqueItems.clear();
    m_transparentItems.clear();

    for (auto& bucket : m_passBuckets) {
        bucket.clear();
    }

    for (auto& item : m_items) {
        if (!item.visible) continue;

        // Add to pass bucket
        size_t passIndex = static_cast<size_t>(item.pass);
        if (passIndex < m_passBuckets.size()) {
            m_passBuckets[passIndex].push_back(&item);
        }

        // Add to opaque/transparent lists
        if (item.blendMode == BlendMode::Opaque || item.blendMode == BlendMode::AlphaTest) {
            m_opaqueItems.push_back(&item);
            m_stats.opaqueItems++;
        } else {
            m_transparentItems.push_back(&item);
            m_stats.transparentItems++;
        }

        m_stats.visibleItems++;
    }
}

uint64_t RenderQueue::ComputeSortKey(const RenderItem& item) const {
    uint64_t key = 0;

    // Pack: [pass:4][blend:4][shader:16][material:16][texture:16][depth:8]
    key |= (static_cast<uint64_t>(item.pass) & 0xF) << 60;
    key |= (static_cast<uint64_t>(item.blendMode) & 0xF) << 56;
    key |= (static_cast<uint64_t>(item.shaderID) & 0xFFFF) << 40;
    key |= (static_cast<uint64_t>(item.materialID) & 0xFFFF) << 24;
    key |= (static_cast<uint64_t>(item.textureID) & 0xFFFF) << 8;

    // Pack depth into 8 bits (quantized)
    float normalizedDepth = std::clamp(item.depth / 1000.0f, 0.0f, 1.0f);
    key |= static_cast<uint64_t>(normalizedDepth * 255.0f) & 0xFF;

    return key;
}

// ============================================================================
// RenderCommandBuffer Implementation
// ============================================================================

RenderCommandBuffer::RenderCommandBuffer() = default;

RenderCommandBuffer::~RenderCommandBuffer() = default;

void RenderCommandBuffer::Clear() {
    m_commands.clear();
}

void RenderCommandBuffer::BindShader(uint32_t shaderID) {
    RenderCommand cmd;
    cmd.type = RenderCommand::Type::BindShader;
    cmd.param1 = shaderID;
    m_commands.push_back(cmd);
}

void RenderCommandBuffer::DrawMesh(const Mesh* mesh, const glm::mat4& transform) {
    RenderCommand cmd;
    cmd.type = RenderCommand::Type::DrawMesh;
    cmd.dataPtr = const_cast<Mesh*>(mesh);
    cmd.matrix = transform;
    m_commands.push_back(cmd);
}

void RenderCommandBuffer::DrawInstanced(const Mesh* mesh, uint32_t instanceCount, void* instanceData) {
    RenderCommand cmd;
    cmd.type = RenderCommand::Type::DrawInstanced;
    cmd.dataPtr = const_cast<Mesh*>(mesh);
    cmd.param1 = instanceCount;
    m_commands.push_back(cmd);
}

void RenderCommandBuffer::SetState(uint32_t stateFlags) {
    RenderCommand cmd;
    cmd.type = RenderCommand::Type::SetState;
    cmd.param1 = stateFlags;
    m_commands.push_back(cmd);
}

void RenderCommandBuffer::Execute() {
    uint32_t currentShader = 0;

    for (const auto& cmd : m_commands) {
        switch (cmd.type) {
            case RenderCommand::Type::BindShader:
                if (cmd.param1 != currentShader) {
                    glUseProgram(cmd.param1);
                    currentShader = cmd.param1;
                }
                break;

            case RenderCommand::Type::DrawMesh:
                if (auto* mesh = static_cast<Mesh*>(cmd.dataPtr)) {
                    mesh->Draw();
                }
                break;

            case RenderCommand::Type::DrawInstanced:
                if (auto* mesh = static_cast<Mesh*>(cmd.dataPtr)) {
                    mesh->DrawInstanced(cmd.param1);
                }
                break;

            case RenderCommand::Type::SetState:
                // Apply state flags
                if (cmd.param1 & 0x01) glEnable(GL_DEPTH_TEST);
                else glDisable(GL_DEPTH_TEST);
                if (cmd.param1 & 0x02) glEnable(GL_BLEND);
                else glDisable(GL_BLEND);
                if (cmd.param1 & 0x04) glEnable(GL_CULL_FACE);
                else glDisable(GL_CULL_FACE);
                break;

            default:
                break;
        }
    }
}

void RenderCommandBuffer::Sort() {
    // Sort commands by type to minimize state changes
    std::stable_sort(m_commands.begin(), m_commands.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            if (a.type != b.type) {
                return static_cast<int>(a.type) < static_cast<int>(b.type);
            }
            // For shader binds, sort by ID
            if (a.type == RenderCommand::Type::BindShader) {
                return a.param1 < b.param1;
            }
            return false;
        });
}

// ============================================================================
// ParallelRenderQueue Implementation
// ============================================================================

ParallelRenderQueue::ParallelRenderQueue(int numThreads)
    : m_numThreads(numThreads) {

    m_threadQueues.reserve(numThreads);
    for (int i = 0; i < numThreads; i++) {
        m_threadQueues.push_back(std::make_unique<RenderQueue>());
        m_threadQueues.back()->Initialize();
    }
}

ParallelRenderQueue::~ParallelRenderQueue() = default;

RenderQueue& ParallelRenderQueue::GetThreadQueue() {
    // Simple thread ID to queue mapping
    // In production, use thread-local storage or proper thread ID
    static thread_local int threadIndex = 0;
    static std::atomic<int> nextIndex{0};

    if (threadIndex == 0) {
        threadIndex = nextIndex.fetch_add(1) % m_numThreads;
        if (threadIndex == 0) threadIndex = 1;  // Reserve 0 for main thread
    }

    return *m_threadQueues[threadIndex % m_numThreads];
}

void ParallelRenderQueue::Merge(RenderQueue& mainQueue) {
    // Merge all thread queues into main queue
    for (auto& threadQueue : m_threadQueues) {
        // Copy items from thread queue to main queue
        // In a real implementation, this would transfer the items more efficiently
    }
}

void ParallelRenderQueue::Reset() {
    for (auto& queue : m_threadQueues) {
        queue->Clear();
    }
}

// ============================================================================
// VisibilitySet Implementation
// ============================================================================

VisibilitySet::VisibilitySet(size_t capacity) {
    m_visibility.resize(capacity, false);
    m_visibleList.reserve(capacity);
}

void VisibilitySet::MarkVisible(uint32_t objectID) {
    if (objectID >= m_visibility.size()) {
        m_visibility.resize(objectID + 1, false);
    }

    if (!m_visibility[objectID]) {
        m_visibility[objectID] = true;
        m_visibleList.push_back(objectID);
        m_visibleCount++;
    }
}

bool VisibilitySet::IsVisible(uint32_t objectID) const {
    return objectID < m_visibility.size() && m_visibility[objectID];
}

void VisibilitySet::Clear() {
    for (uint32_t id : m_visibleList) {
        m_visibility[id] = false;
    }
    m_visibleList.clear();
    m_visibleCount = 0;
}

void VisibilitySet::ForEachVisible(const std::function<void(uint32_t)>& func) const {
    for (uint32_t id : m_visibleList) {
        func(id);
    }
}

} // namespace Nova
