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

} // namespace Nova
