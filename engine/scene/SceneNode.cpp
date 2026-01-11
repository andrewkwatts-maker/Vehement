#include "scene/SceneNode.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Material.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
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

void SceneNode::SetLocalTransform(const glm::mat4& transform) {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(transform, m_scale, m_rotation, m_position, skew, perspective);
    MarkDirty();
}

glm::vec3 SceneNode::GetWorldPosition() const {
    return glm::vec3(GetWorldTransform()[3]);
}

glm::quat SceneNode::GetWorldRotation() const {
    if (m_parent) {
        return m_parent->GetWorldRotation() * m_rotation;
    }
    return m_rotation;
}

glm::vec3 SceneNode::GetWorldScale() const {
    if (m_parent) {
        return m_parent->GetWorldScale() * m_scale;
    }
    return m_scale;
}

const glm::mat4& SceneNode::GetLocalTransform() const {
    if (m_transformDirty) {
        UpdateTransform();
    }
    return m_localTransform;
}

const glm::mat4& SceneNode::GetWorldTransform() const {
    if (m_transformDirty) {
        UpdateTransform();
    }
    return m_worldTransform;
}

void SceneNode::UpdateTransform() const {
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
    if (!m_transformDirty) {
        m_transformDirty = true;
        MarkChildrenDirty();
    }
}

void SceneNode::MarkChildrenDirty() {
    for (auto& child : m_children) {
        child->MarkDirty();
    }
}

void SceneNode::AddChild(std::unique_ptr<SceneNode> child) {
    if (!child) {
        return;
    }

    // Remove from previous parent if any
    if (child->m_parent) {
        child->m_parent->RemoveChild(child.get());
    }

    child->m_parent = this;
    child->MarkDirty();
    m_children.push_back(std::move(child));
}

std::unique_ptr<SceneNode> SceneNode::RemoveChild(SceneNode* child) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const auto& ptr) { return ptr.get() == child; });

    if (it != m_children.end()) {
        std::unique_ptr<SceneNode> removed = std::move(*it);
        removed->m_parent = nullptr;
        removed->MarkDirty();
        m_children.erase(it);
        return removed;
    }
    return nullptr;
}

std::unique_ptr<SceneNode> SceneNode::DetachFromParent() {
    if (m_parent) {
        return m_parent->RemoveChild(this);
    }
    return nullptr;
}

void SceneNode::SetParent(SceneNode* newParent) {
    if (m_parent == newParent) {
        return;
    }

    // Store world transform to preserve position
    glm::mat4 worldTransform = GetWorldTransform();

    // Detach from current parent
    std::unique_ptr<SceneNode> self;
    if (m_parent) {
        self = m_parent->RemoveChild(this);
    }

    // Attach to new parent
    if (newParent) {
        // Calculate new local transform to maintain world position
        glm::mat4 parentInverse = glm::inverse(newParent->GetWorldTransform());
        SetLocalTransform(parentInverse * worldTransform);
        newParent->AddChild(std::move(self));
    } else {
        // Becoming root-level, local = world
        SetLocalTransform(worldTransform);
        m_parent = nullptr;
    }
}

SceneNode* SceneNode::FindChild(std::string_view name, bool recursive) const {
    for (const auto& child : m_children) {
        if (child->m_name == name) {
            return child.get();
        }
        if (recursive) {
            SceneNode* found = child->FindChild(name, true);
            if (found) {
                return found;
            }
        }
    }
    return nullptr;
}

void SceneNode::FindAll(const std::function<bool(const SceneNode&)>& predicate,
                        std::vector<SceneNode*>& results) const {
    for (const auto& child : m_children) {
        if (predicate(*child)) {
            results.push_back(child.get());
        }
        child->FindAll(predicate, results);
    }
}

void SceneNode::ForEach(const std::function<void(SceneNode&)>& func) {
    func(*this);
    for (auto& child : m_children) {
        child->ForEach(func);
    }
}

void SceneNode::ForEach(const std::function<void(const SceneNode&)>& func) const {
    func(*this);
    for (const auto& child : m_children) {
        child->ForEach(func);
    }
}

bool SceneNode::IsVisibleInHierarchy() const {
    if (!m_visible) {
        return false;
    }
    if (m_parent) {
        return m_parent->IsVisibleInHierarchy();
    }
    return true;
}

void SceneNode::Update(float deltaTime) {
    for (auto& child : m_children) {
        child->Update(deltaTime);
    }
}

void SceneNode::Render(Renderer& renderer) {
    if (!m_visible) {
        return;
    }

    if (m_mesh && m_material) {
        renderer.DrawMesh(*m_mesh, *m_material, GetWorldTransform());
    }

    for (auto& child : m_children) {
        child->Render(renderer);
    }
}

} // namespace Nova
