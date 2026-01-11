#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <array>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Mesh;
class Material;
class Shader;
class Camera;

/**
 * @brief Render pass types
 */
enum class RenderPass : uint8_t {
    Shadow = 0,
    Depth,
    GBuffer,
    Opaque,
    Transparent,
    PostProcess,
    UI,
    Debug,
    Count
};

/**
 * @brief Blend mode for sorting
 */
enum class BlendMode : uint8_t {
    Opaque = 0,
    AlphaTest,
    AlphaBlend,
    Additive,
    Multiply
};

/**
 * @brief Render item representing a single draw call
 */
struct RenderItem {
    // Object data
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    glm::mat4 transform{1.0f};
    uint32_t objectID = 0;

    // Sorting keys
    uint64_t sortKey = 0;
    float depth = 0.0f;            // Distance from camera
    float screenSize = 0.0f;       // For LOD/culling

    // State hints
    RenderPass pass = RenderPass::Opaque;
    BlendMode blendMode = BlendMode::Opaque;
    uint32_t shaderID = 0;
    uint32_t materialID = 0;
    uint32_t textureID = 0;

    // Flags
    bool castsShadow = true;
    bool receivesShadow = true;
    bool visible = true;
    int lodLevel = 0;

    // Custom data
    void* userData = nullptr;
    glm::vec4 customData{0.0f};    // For shader params

    bool IsValid() const { return mesh != nullptr && material != nullptr; }
};

/**
 * @brief Configuration for render queue
 */
struct RenderQueueConfig {
    bool sortByState = true;          // Sort to minimize state changes
    bool sortByDepth = true;          // Sort by depth (front-to-back or back-to-front)
    bool enableInstancing = true;
    bool separateTransparent = true;
    int maxItemsPerBucket = 10000;

    // Sorting weights
    float shaderWeight = 1.0f;
    float materialWeight = 0.5f;
    float textureWeight = 0.25f;
    float depthWeight = 0.1f;
};

/**
 * @brief Statistics for render queue
 */
struct RenderQueueStats {
    uint32_t totalItems = 0;
    uint32_t visibleItems = 0;
    uint32_t opaqueItems = 0;
    uint32_t transparentItems = 0;
    uint32_t stateChanges = 0;
    uint32_t shaderChanges = 0;
    uint32_t materialChanges = 0;
    uint32_t textureChanges = 0;
    uint32_t drawCalls = 0;
    float sortTimeMs = 0.0f;

    void Reset() {
        totalItems = visibleItems = opaqueItems = transparentItems = 0;
        stateChanges = shaderChanges = materialChanges = textureChanges = 0;
        drawCalls = 0;
        sortTimeMs = 0.0f;
    }
};

/**
 * @brief Render bucket for grouping similar items
 */
struct RenderBucket {
    RenderPass pass;
    BlendMode blendMode;
    std::vector<RenderItem*> items;

    void Clear() { items.clear(); }
    bool IsEmpty() const { return items.empty(); }
    size_t Size() const { return items.size(); }
};

/**
 * @brief Sorted render queue for efficient draw call ordering
 *
 * Sorts render items to minimize GPU state changes and
 * optimize rendering throughput.
 */
class RenderQueue {
public:
    RenderQueue();
    ~RenderQueue();

    // Non-copyable
    RenderQueue(const RenderQueue&) = delete;
    RenderQueue& operator=(const RenderQueue&) = delete;

    /**
     * @brief Initialize the render queue
     */
    bool Initialize(const RenderQueueConfig& config = RenderQueueConfig{});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Clear all items for new frame
     */
    void Clear();

    /**
     * @brief Begin accepting items for a frame
     */
    void BeginFrame(const Camera& camera);

    /**
     * @brief End frame and finalize sorting
     */
    void EndFrame();

    /**
     * @brief Submit a render item
     */
    void Submit(const RenderItem& item);

    /**
     * @brief Submit with builder pattern
     */
    RenderItem& Submit(const std::shared_ptr<Mesh>& mesh,
                       const std::shared_ptr<Material>& material,
                       const glm::mat4& transform);

    /**
     * @brief Sort all submitted items
     */
    void Sort();

    /**
     * @brief Execute rendering for a specific pass
     * @param pass The render pass to execute
     * @param renderFunc Function to call for each item
     */
    void Execute(RenderPass pass,
                 const std::function<void(const RenderItem&)>& renderFunc);

    /**
     * @brief Execute all passes in order
     */
    void ExecuteAll(const std::function<void(RenderPass, const RenderItem&)>& renderFunc);

    /**
     * @brief Get items for a specific pass
     */
    const std::vector<RenderItem*>& GetPassItems(RenderPass pass) const;

    /**
     * @brief Get all opaque items (sorted)
     */
    const std::vector<RenderItem*>& GetOpaqueItems() const { return m_opaqueItems; }

    /**
     * @brief Get all transparent items (sorted back-to-front)
     */
    const std::vector<RenderItem*>& GetTransparentItems() const { return m_transparentItems; }

    /**
     * @brief Get statistics
     */
    const RenderQueueStats& GetStats() const { return m_stats; }

    /**
     * @brief Get configuration
     */
    const RenderQueueConfig& GetConfig() const { return m_config; }

    /**
     * @brief Update configuration
     */
    void SetConfig(const RenderQueueConfig& config);

    /**
     * @brief Reserve capacity for items
     */
    void Reserve(size_t capacity);

    /**
     * @brief Get total item count
     */
    size_t GetItemCount() const { return m_items.size(); }

    /**
     * @brief Check if queue has items for pass
     */
    bool HasItems(RenderPass pass) const;

    /**
     * @brief Set custom sort function
     */
    using SortFunction = std::function<bool(const RenderItem*, const RenderItem*)>;
    void SetCustomSort(RenderPass pass, SortFunction sortFunc);

    /**
     * @brief Filter items by predicate
     */
    std::vector<RenderItem*> Filter(const std::function<bool(const RenderItem&)>& predicate) const;

private:
    // Item storage
    std::vector<RenderItem> m_items;

    // Sorted views
    std::vector<RenderItem*> m_opaqueItems;
    std::vector<RenderItem*> m_transparentItems;
    std::array<std::vector<RenderItem*>, static_cast<size_t>(RenderPass::Count)> m_passBuckets;

    // Custom sort functions per pass
    std::array<SortFunction, static_cast<size_t>(RenderPass::Count)> m_customSortFuncs;

    // Camera data for depth sorting
    glm::vec3 m_cameraPosition{0.0f};
    glm::vec3 m_cameraForward{0.0f, 0.0f, -1.0f};
    glm::mat4 m_viewProjection{1.0f};

    RenderQueueConfig m_config;
    RenderQueueStats m_stats;
    bool m_initialized = false;
    bool m_sorted = false;

    // Sorting helpers
    void ComputeSortKeys();
    void SortOpaque();
    void SortTransparent();
    void BucketItems();
    uint64_t ComputeSortKey(const RenderItem& item) const;

    static constexpr uint64_t SORT_KEY_SHADER_SHIFT = 48;
    static constexpr uint64_t SORT_KEY_MATERIAL_SHIFT = 32;
    static constexpr uint64_t SORT_KEY_TEXTURE_SHIFT = 16;
    static constexpr uint64_t SORT_KEY_DEPTH_SHIFT = 0;
};

/**
 * @brief Builder for creating render items
 */
class RenderItemBuilder {
public:
    RenderItemBuilder() = default;

    RenderItemBuilder& SetMesh(const std::shared_ptr<Mesh>& mesh) {
        m_item.mesh = mesh;
        return *this;
    }

    RenderItemBuilder& SetMaterial(const std::shared_ptr<Material>& material) {
        m_item.material = material;
        return *this;
    }

    RenderItemBuilder& SetTransform(const glm::mat4& transform) {
        m_item.transform = transform;
        return *this;
    }

    RenderItemBuilder& SetPass(RenderPass pass) {
        m_item.pass = pass;
        return *this;
    }

    RenderItemBuilder& SetBlendMode(BlendMode mode) {
        m_item.blendMode = mode;
        return *this;
    }

    RenderItemBuilder& SetObjectID(uint32_t id) {
        m_item.objectID = id;
        return *this;
    }

    RenderItemBuilder& SetShadowCaster(bool casts) {
        m_item.castsShadow = casts;
        return *this;
    }

    RenderItemBuilder& SetShadowReceiver(bool receives) {
        m_item.receivesShadow = receives;
        return *this;
    }

    RenderItemBuilder& SetLODLevel(int level) {
        m_item.lodLevel = level;
        return *this;
    }

    RenderItemBuilder& SetUserData(void* data) {
        m_item.userData = data;
        return *this;
    }

    RenderItemBuilder& SetCustomData(const glm::vec4& data) {
        m_item.customData = data;
        return *this;
    }

    RenderItem Build() const { return m_item; }

private:
    RenderItem m_item;
};

/**
 * @brief Render command for command buffer
 */
struct RenderCommand {
    enum class Type : uint8_t {
        BindShader,
        BindMaterial,
        BindTexture,
        SetUniform,
        DrawMesh,
        DrawInstanced,
        SetState,
        Clear,
        Custom
    };

    Type type;
    uint32_t param1 = 0;
    uint32_t param2 = 0;
    void* dataPtr = nullptr;
    glm::mat4 matrix{1.0f};
};

/**
 * @brief Command buffer for deferred rendering commands
 */
class RenderCommandBuffer {
public:
    RenderCommandBuffer();
    ~RenderCommandBuffer();

    /**
     * @brief Clear all commands
     */
    void Clear();

    /**
     * @brief Add bind shader command
     */
    void BindShader(uint32_t shaderID);

    /**
     * @brief Add draw mesh command
     */
    void DrawMesh(const Mesh* mesh, const glm::mat4& transform);

    /**
     * @brief Add instanced draw command
     */
    void DrawInstanced(const Mesh* mesh, uint32_t instanceCount, void* instanceData);

    /**
     * @brief Add set state command
     */
    void SetState(uint32_t stateFlags);

    /**
     * @brief Execute all commands
     */
    void Execute();

    /**
     * @brief Get command count
     */
    size_t GetCommandCount() const { return m_commands.size(); }

    /**
     * @brief Sort commands for optimal execution
     */
    void Sort();

private:
    std::vector<RenderCommand> m_commands;
};

/**
 * @brief Multi-threaded render queue for parallel submission
 */
class ParallelRenderQueue {
public:
    ParallelRenderQueue(int numThreads = 4);
    ~ParallelRenderQueue();

    /**
     * @brief Get queue for current thread
     */
    RenderQueue& GetThreadQueue();

    /**
     * @brief Merge all thread queues
     */
    void Merge(RenderQueue& mainQueue);

    /**
     * @brief Reset all thread queues
     */
    void Reset();

private:
    std::vector<std::unique_ptr<RenderQueue>> m_threadQueues;
    int m_numThreads;
};

/**
 * @brief Visibility set for tracking visible objects
 */
class VisibilitySet {
public:
    VisibilitySet(size_t capacity = 10000);

    /**
     * @brief Mark object as visible
     */
    void MarkVisible(uint32_t objectID);

    /**
     * @brief Check if object is visible
     */
    bool IsVisible(uint32_t objectID) const;

    /**
     * @brief Clear visibility data
     */
    void Clear();

    /**
     * @brief Get number of visible objects
     */
    size_t GetVisibleCount() const { return m_visibleCount; }

    /**
     * @brief Iterate visible objects
     */
    void ForEachVisible(const std::function<void(uint32_t)>& func) const;

private:
    std::vector<bool> m_visibility;
    std::vector<uint32_t> m_visibleList;
    size_t m_visibleCount = 0;
};

} // namespace Nova
