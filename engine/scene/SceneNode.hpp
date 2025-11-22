#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

class Renderer;
class Mesh;
class Material;

/**
 * @brief Scene graph node with hierarchical transforms
 *
 * Supports parent-child relationships with transform inheritance.
 * Uses dirty flags to cache world transforms for optimal performance.
 */
class SceneNode {
public:
    explicit SceneNode(const std::string& name = "Node");
    virtual ~SceneNode();

    // Non-copyable but movable
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;
    SceneNode(SceneNode&&) noexcept = default;
    SceneNode& operator=(SceneNode&&) noexcept = default;

    // Transform setters
    void SetPosition(const glm::vec3& position);
    void SetRotation(const glm::quat& rotation);
    void SetRotation(const glm::vec3& eulerDegrees);
    void SetScale(const glm::vec3& scale);
    void SetScale(float uniformScale);

    /**
     * @brief Set local transform directly
     * @param transform Local transform matrix (will be decomposed to position/rotation/scale)
     */
    void SetLocalTransform(const glm::mat4& transform);

    // Transform getters
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
    [[nodiscard]] const glm::quat& GetRotation() const { return m_rotation; }
    [[nodiscard]] const glm::vec3& GetScale() const { return m_scale; }

    [[nodiscard]] glm::vec3 GetWorldPosition() const;
    [[nodiscard]] glm::quat GetWorldRotation() const;
    [[nodiscard]] glm::vec3 GetWorldScale() const;

    [[nodiscard]] const glm::mat4& GetLocalTransform() const;
    [[nodiscard]] const glm::mat4& GetWorldTransform() const;

    // Hierarchy management
    /**
     * @brief Add a child node (takes ownership)
     * @param child The child node to add
     */
    void AddChild(std::unique_ptr<SceneNode> child);

    /**
     * @brief Remove and return a child node
     * @param child Pointer to the child to remove
     * @return The removed child, or nullptr if not found
     */
    [[nodiscard]] std::unique_ptr<SceneNode> RemoveChild(SceneNode* child);

    /**
     * @brief Detach this node from its parent
     * @return This node as a unique_ptr (caller takes ownership)
     */
    [[nodiscard]] std::unique_ptr<SceneNode> DetachFromParent();

    /**
     * @brief Reparent this node to a new parent
     * @param newParent The new parent (nullptr for root-level)
     */
    void SetParent(SceneNode* newParent);

    [[nodiscard]] SceneNode* GetParent() const { return m_parent; }
    [[nodiscard]] const std::vector<std::unique_ptr<SceneNode>>& GetChildren() const { return m_children; }
    [[nodiscard]] size_t GetChildCount() const { return m_children.size(); }

    /**
     * @brief Find a child node by name
     * @param name Name to search for
     * @param recursive Search recursively through descendants
     * @return Pointer to found node, or nullptr
     */
    [[nodiscard]] SceneNode* FindChild(std::string_view name, bool recursive = true) const;

    /**
     * @brief Find all nodes matching a predicate
     * @param predicate Function returning true for matching nodes
     * @param results Vector to store matching nodes
     */
    void FindAll(const std::function<bool(const SceneNode&)>& predicate,
                 std::vector<SceneNode*>& results) const;

    /**
     * @brief Execute a function on this node and all descendants
     * @param func Function to call on each node
     */
    void ForEach(const std::function<void(SceneNode&)>& func);
    void ForEach(const std::function<void(const SceneNode&)>& func) const;

    // Visibility
    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    /**
     * @brief Check if this node and all ancestors are visible
     */
    [[nodiscard]] bool IsVisibleInHierarchy() const;

    // Name
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    // Rendering components
    void SetMesh(std::shared_ptr<Mesh> mesh) { m_mesh = std::move(mesh); }
    void SetMaterial(std::shared_ptr<Material> material) { m_material = std::move(material); }

    [[nodiscard]] const std::shared_ptr<Mesh>& GetMesh() const { return m_mesh; }
    [[nodiscard]] const std::shared_ptr<Material>& GetMaterial() const { return m_material; }
    [[nodiscard]] bool HasMesh() const { return m_mesh != nullptr; }
    [[nodiscard]] bool HasMaterial() const { return m_material != nullptr; }

    // Update and render
    virtual void Update(float deltaTime);
    virtual void Render(Renderer& renderer);

    /**
     * @brief Check if transform needs recalculation
     */
    [[nodiscard]] bool IsTransformDirty() const { return m_transformDirty; }

protected:
    void UpdateTransform() const;
    void MarkDirty();
    void MarkChildrenDirty();

    std::string m_name;
    SceneNode* m_parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_children;

    glm::vec3 m_position{0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_scale{1.0f};

    mutable glm::mat4 m_localTransform{1.0f};
    mutable glm::mat4 m_worldTransform{1.0f};
    mutable bool m_transformDirty = true;

    bool m_visible = true;

    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Material> m_material;
};

} // namespace Nova
