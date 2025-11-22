#pragma once

#include <memory>
#include <vector>
#include <string>

namespace Nova {

class SceneNode;
class Renderer;
class Camera;

/**
 * @brief Scene container and manager
 */
class Scene {
public:
    Scene();
    virtual ~Scene();

    /**
     * @brief Initialize the scene
     */
    virtual bool Initialize();

    /**
     * @brief Update the scene
     */
    virtual void Update(float deltaTime);

    /**
     * @brief Render the scene
     */
    virtual void Render(Renderer& renderer);

    /**
     * @brief Shutdown the scene
     */
    virtual void Shutdown();

    /**
     * @brief Get the root node
     */
    SceneNode* GetRoot() { return m_root.get(); }

    /**
     * @brief Get the main camera
     */
    Camera* GetCamera() { return m_camera.get(); }

    /**
     * @brief Set the main camera
     */
    void SetCamera(std::unique_ptr<Camera> camera);

    /**
     * @brief Get scene name
     */
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

protected:
    std::string m_name = "Unnamed Scene";
    std::unique_ptr<SceneNode> m_root;
    std::unique_ptr<Camera> m_camera;
};

} // namespace Nova
