#include "scene/Scene.hpp"
#include "scene/SceneNode.hpp"
#include "scene/Camera.hpp"
#include "scene/FlyCamera.hpp"
#include "graphics/Renderer.hpp"

namespace Nova {

Scene::Scene() {
    m_root = std::make_unique<SceneNode>("Root");
    m_camera = std::make_unique<FlyCamera>();
}

Scene::~Scene() {
    Shutdown();
}

bool Scene::Initialize() {
    return true;
}

void Scene::Update(float deltaTime) {
    if (m_root) {
        m_root->Update(deltaTime);
    }
}

void Scene::Render(Renderer& renderer) {
    if (m_camera) {
        renderer.SetCamera(m_camera.get());
    }

    if (m_root) {
        m_root->Render(renderer);
    }
}

void Scene::Shutdown() {
    m_root.reset();
    m_camera.reset();
}

void Scene::SetCamera(std::unique_ptr<Camera> camera) {
    m_camera = std::move(camera);
}

SceneNode* Scene::FindNode(std::string_view name) const {
    if (!m_root) {
        return nullptr;
    }

    // Check root itself
    if (m_root->GetName() == name) {
        return m_root.get();
    }

    return m_root->FindChild(name, true);
}

std::vector<SceneNode*> Scene::FindNodes(
    const std::function<bool(const SceneNode&)>& predicate) const {
    std::vector<SceneNode*> results;

    if (!m_root) {
        return results;
    }

    // Check root
    if (predicate(*m_root)) {
        results.push_back(m_root.get());
    }

    m_root->FindAll(predicate, results);
    return results;
}

void Scene::ForEachNode(const std::function<void(SceneNode&)>& func) {
    if (m_root) {
        m_root->ForEach(func);
    }
}

void Scene::ForEachNode(const std::function<void(const SceneNode&)>& func) const {
    if (m_root) {
        m_root->ForEach(func);
    }
}

size_t Scene::GetNodeCount() const {
    if (!m_root) {
        return 0;
    }

    size_t count = 0;
    m_root->ForEach([&count](const SceneNode&) {
        ++count;
    });
    return count;
}

} // namespace Nova
