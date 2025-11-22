#pragma once

#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

class SceneNode;
class Renderer;
class Camera;
class JobSystem;

/**
 * @brief Batch render data for cache-efficient rendering
 */
struct RenderBatch {
    std::vector<SceneNode*> nodes;
    std::vector<glm::mat4> transforms;
    std::vector<uint32_t> materialIds;

    void Clear() {
        nodes.clear();
        transforms.clear();
        materialIds.clear();
    }

    void Reserve(size_t count) {
        nodes.reserve(count);
        transforms.reserve(count);
        materialIds.reserve(count);
    }
};

/**
 * @brief Scene container and manager
 *
 * Manages a hierarchical scene graph with a root node and camera.
 * Provides methods for updating, rendering, and querying scene contents.
 */
class Scene {
public:
    Scene();
    virtual ~Scene();

    // Non-copyable but movable
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

    /**
     * @brief Initialize the scene
     * @return true on success
     */
    virtual bool Initialize();

    /**
     * @brief Update the scene
     * @param deltaTime Time since last update in seconds
     */
    virtual void Update(float deltaTime);

    /**
     * @brief Render the scene
     * @param renderer The renderer to use
     */
    virtual void Render(Renderer& renderer);

    /**
     * @brief Shutdown and cleanup the scene
     */
    virtual void Shutdown();

    // Root node access
    [[nodiscard]] SceneNode* GetRoot() { return m_root.get(); }
    [[nodiscard]] const SceneNode* GetRoot() const { return m_root.get(); }

    // Camera management
    [[nodiscard]] Camera* GetCamera() { return m_camera.get(); }
    [[nodiscard]] const Camera* GetCamera() const { return m_camera.get(); }

    /**
     * @brief Set the main camera (takes ownership)
     * @param camera The camera to use
     */
    void SetCamera(std::unique_ptr<Camera> camera);

    /**
     * @brief Create and set a camera of the specified type
     * @tparam T Camera type to create
     * @tparam Args Constructor argument types
     * @param args Arguments to forward to camera constructor
     * @return Pointer to the created camera
     */
    template<typename T, typename... Args>
    T* CreateCamera(Args&&... args) {
        auto camera = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = camera.get();
        SetCamera(std::move(camera));
        return ptr;
    }

    // Scene metadata
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    // Node lookup utilities
    /**
     * @brief Find a node by name in the scene
     * @param name Name to search for
     * @return Pointer to found node, or nullptr
     */
    [[nodiscard]] SceneNode* FindNode(std::string_view name) const;

    /**
     * @brief Find all nodes matching a predicate
     * @param predicate Function returning true for matching nodes
     * @return Vector of matching node pointers
     */
    [[nodiscard]] std::vector<SceneNode*> FindNodes(
        const std::function<bool(const SceneNode&)>& predicate) const;

    /**
     * @brief Execute a function on all nodes in the scene
     * @param func Function to call on each node
     */
    void ForEachNode(const std::function<void(SceneNode&)>& func);
    void ForEachNode(const std::function<void(const SceneNode&)>& func) const;

    /**
     * @brief Get total node count in the scene
     */
    [[nodiscard]] size_t GetNodeCount() const;

    // =========================================================================
    // Performance Optimizations
    // =========================================================================

    /**
     * @brief Update scene with parallel transform computation
     * @param deltaTime Time since last update in seconds
     * @param useParallel Use job system for parallel updates
     */
    void UpdateParallel(float deltaTime, bool useParallel = true);

    /**
     * @brief Render with batching optimization
     * Collects renderable nodes and batches by material for cache efficiency
     * @param renderer The renderer to use
     */
    void RenderBatched(Renderer& renderer);

    /**
     * @brief Build a flat list of all visible nodes for cache-efficient iteration
     * @return Vector of pointers to visible nodes
     */
    [[nodiscard]] std::vector<SceneNode*> BuildFlatNodeList() const;

    /**
     * @brief Collect render batches grouped by material
     * @param batch Output batch to fill
     */
    void CollectRenderBatch(RenderBatch& batch) const;

    /**
     * @brief Pre-compute all world transforms (call before rendering)
     * Forces transform update on all dirty nodes
     */
    void PrecomputeTransforms();

    /**
     * @brief Enable/disable dirty flag propagation optimization
     * When enabled, only dirty subtrees are updated
     */
    void SetDirtyOptimizationEnabled(bool enabled) { m_dirtyOptEnabled = enabled; }

    /**
     * @brief Get render batch (cached between frames if scene unchanged)
     */
    [[nodiscard]] const RenderBatch& GetCachedRenderBatch() const { return m_renderBatch; }

    /**
     * @brief Mark render batch as needing rebuild
     */
    void InvalidateRenderBatch() { m_renderBatchDirty = true; }

protected:
    std::string m_name = "Unnamed Scene";
    std::unique_ptr<SceneNode> m_root;
    std::unique_ptr<Camera> m_camera;

    // Optimization state
    mutable RenderBatch m_renderBatch;
    mutable bool m_renderBatchDirty = true;
    bool m_dirtyOptEnabled = true;
};

} // namespace Nova
