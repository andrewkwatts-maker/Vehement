#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

class Renderer;
class Mesh;
class Material;

/**
 * @brief Scene graph node with hierarchical transforms
 */
class SceneNode {
public:
    explicit SceneNode(const std::string& name = "Node");
    virtual ~SceneNode();

    // Transform
    void SetPosition(const glm::vec3& position);
    void SetRotation(const glm::quat& rotation);
    void SetRotation(const glm::vec3& eulerDegrees);
    void SetScale(const glm::vec3& scale);
    void SetScale(float uniformScale);

    const glm::vec3& GetPosition() const { return m_position; }
    const glm::quat& GetRotation() const { return m_rotation; }
    const glm::vec3& GetScale() const { return m_scale; }

    glm::vec3 GetWorldPosition() const;
    glm::mat4 GetLocalTransform() const;
    glm::mat4 GetWorldTransform() const;

    // Hierarchy
    void AddChild(std::unique_ptr<SceneNode> child);
    void RemoveChild(SceneNode* child);
    SceneNode* GetParent() const { return m_parent; }
    const std::vector<std::unique_ptr<SceneNode>>& GetChildren() const { return m_children; }

    SceneNode* FindChild(const std::string& name, bool recursive = true);

    // Visibility
    void SetVisible(bool visible) { m_visible = visible; }
    bool IsVisible() const { return m_visible; }

    // Name
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    // Rendering
    void SetMesh(std::shared_ptr<Mesh> mesh) { m_mesh = std::move(mesh); }
    void SetMaterial(std::shared_ptr<Material> material) { m_material = std::move(material); }

    std::shared_ptr<Mesh> GetMesh() const { return m_mesh; }
    std::shared_ptr<Material> GetMaterial() const { return m_material; }

    // Update and render
    virtual void Update(float deltaTime);
    virtual void Render(Renderer& renderer);

protected:
    void UpdateTransform();
    void MarkDirty();

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
