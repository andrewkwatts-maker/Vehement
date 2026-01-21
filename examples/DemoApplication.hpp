#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {
    class FlyCamera;
    class ParticleSystem;
    class TerrainGenerator;
    class Mesh;
    class Shader;
    class Material;
    class Graph;
}

/**
 * @brief Demo application showcasing Nova3D engine features
 */
class DemoApplication {
public:
    DemoApplication();
    ~DemoApplication();

    bool Initialize();
    void Update(float deltaTime);
    void Render();
    void OnImGui();
    void Shutdown();

private:
    void SetupScene();
    void SetupLighting();
    void SetupParticles();
    void SetupTerrain();
    void SetupPathfinding();

    std::unique_ptr<Nova::FlyCamera> m_camera;
    std::unique_ptr<Nova::ParticleSystem> m_particles;
    std::unique_ptr<Nova::TerrainGenerator> m_terrain;
    std::unique_ptr<Nova::Graph> m_pathGraph;

    // Demo objects
    std::unique_ptr<Nova::Mesh> m_cubeMesh;
    std::unique_ptr<Nova::Mesh> m_sphereMesh;
    std::unique_ptr<Nova::Mesh> m_planeMesh;

    std::shared_ptr<Nova::Shader> m_basicShader;
    std::shared_ptr<Nova::Material> m_defaultMaterial;

    // State
    bool m_showGrid = true;
    bool m_showParticles = true;
    bool m_showTerrain = true;
    bool m_showPathfinding = false;

    float m_rotationAngle = 0.0f;

    // Lighting
    glm::vec3 m_lightDirection{-0.5f, -1.0f, -0.5f};
    glm::vec3 m_lightColor{1.0f, 0.95f, 0.9f};
    float m_ambientStrength = 0.2f;
};
