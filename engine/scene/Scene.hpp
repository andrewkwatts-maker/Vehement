#pragma once

#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <functional>

namespace Nova {

class SceneNode;
class Renderer;
class Camera;

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

protected:
    std::string m_name = "Unnamed Scene";
    std::unique_ptr<SceneNode> m_root;
    std::unique_ptr<Camera> m_camera;
};

} // namespace Nova
