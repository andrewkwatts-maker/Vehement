#include "scene/SceneNode.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Material.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Nova {

SceneNode::SceneNode(const std::string& name)
    : m_name(name)
{}

SceneNode::~SceneNode() = default;

void SceneNode::SetPosition(const glm::vec3& position) {
    m_position = position;
    MarkDirty();
}

void SceneNode::SetRotation(const glm::quat& rotation) {
    m_rotation = rotation;
    MarkDirty();
}

void SceneNode::SetRotation(const glm::vec3& eulerDegrees) {
    m_rotation = glm::quat(glm::radians(eulerDegrees));
    MarkDirty();
}

void SceneNode::SetScale(const glm::vec3& scale) {
    m_scale = scale;
    MarkDirty();
}

void SceneNode::SetScale(float uniformScale) {
    m_scale = glm::vec3(uniformScale);
    MarkDirty();
}

glm::vec3 SceneNode::GetWorldPosition() const {
    return glm::vec3(GetWorldTransform()[3]);
}

glm::mat4 SceneNode::GetLocalTransform() const {
    if (m_transformDirty) {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), m_position);
        glm::mat4 rotation = glm::mat4_cast(m_rotation);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), m_scale);
        m_localTransform = translation * rotation * scale;
    }
    return m_localTransform;
}

glm::mat4 SceneNode::GetWorldTransform() const {
    if (m_transformDirty) {
        UpdateTransform();
    }
    return m_worldTransform;
}

void SceneNode::UpdateTransform() {
    m_localTransform = glm::translate(glm::mat4(1.0f), m_position) *
                       glm::mat4_cast(m_rotation) *
                       glm::scale(glm::mat4(1.0f), m_scale);

    if (m_parent) {
        m_worldTransform = m_parent->GetWorldTransform() * m_localTransform;
    } else {
        m_worldTransform = m_localTransform;
    }

    m_transformDirty = false;
}

void SceneNode::MarkDirty() {
    m_transformDirty = true;
    for (auto& child : m_children) {
        child->MarkDirty();
    }
}

void SceneNode::AddChild(std::unique_ptr<SceneNode> child) {
    child->m_parent = this;
    child->MarkDirty();
    m_children.push_back(std::move(child));
}

void SceneNode::RemoveChild(SceneNode* child) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const auto& ptr) { return ptr.get() == child; });

    if (it != m_children.end()) {
        (*it)->m_parent = nullptr;
        m_children.erase(it);
    }
}

SceneNode* SceneNode::FindChild(const std::string& name, bool recursive) {
    for (auto& child : m_children) {
        if (child->m_name == name) {
            return child.get();
        }
        if (recursive) {
            SceneNode* found = child->FindChild(name, true);
            if (found) return found;
        }
    }
    return nullptr;
}

void SceneNode::Update(float deltaTime) {
    for (auto& child : m_children) {
        child->Update(deltaTime);
    }
}

void SceneNode::Render(Renderer& renderer) {
    if (!m_visible) return;

    if (m_mesh && m_material) {
        renderer.DrawMesh(*m_mesh, *m_material, GetWorldTransform());
    }

    for (auto& child : m_children) {
        child->Render(renderer);
    }
}

} // namespace Nova
