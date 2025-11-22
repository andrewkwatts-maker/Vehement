#include "DemoApplication.hpp"

#include "core/Engine.hpp"
#include "core/Window.hpp"
#include "core/Time.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Material.hpp"
#include "graphics/debug/DebugDraw.hpp"
#include "input/InputManager.hpp"
#include "scene/FlyCamera.hpp"
#include "particles/ParticleSystem.hpp"
#include "terrain/TerrainGenerator.hpp"
#include "pathfinding/Graph.hpp"
#include "pathfinding/Pathfinder.hpp"
#include "config/Config.hpp"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

static const char* BASIC_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ProjectionView;
uniform mat4 u_Model;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_TexCoord = a_TexCoord;
    gl_Position = u_ProjectionView * worldPos;
}
)";

static const char* BASIC_FRAGMENT_SHADER = R"(
#version 460 core

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_AmbientStrength;
uniform vec3 u_ObjectColor;
uniform vec3 u_ViewPos;

out vec4 FragColor;

void main() {
    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(-u_LightDirection);

    // Ambient
    vec3 ambient = u_AmbientStrength * u_LightColor;

    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;

    // Specular
    vec3 viewDir = normalize(u_ViewPos - v_WorldPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * u_LightColor;

    vec3 result = (ambient + diffuse + specular) * u_ObjectColor;
    FragColor = vec4(result, 1.0);
}
)";

DemoApplication::DemoApplication() = default;

DemoApplication::~DemoApplication() = default;

bool DemoApplication::Initialize() {
    spdlog::info("Initializing Demo Application");

    // Create camera
    m_camera = std::make_unique<Nova::FlyCamera>();
    m_camera->SetPerspective(45.0f, Nova::Engine::Instance().GetWindow().GetAspectRatio(), 0.1f, 1000.0f);
    m_camera->LookAt(glm::vec3(10, 10, 10), glm::vec3(0, 0, 0));

    // Create shader
    m_basicShader = std::make_shared<Nova::Shader>();
    if (!m_basicShader->LoadFromSource(BASIC_VERTEX_SHADER, BASIC_FRAGMENT_SHADER)) {
        spdlog::error("Failed to create basic shader");
        return false;
    }

    SetupScene();
    SetupParticles();
    SetupTerrain();
    SetupPathfinding();

    spdlog::info("Demo Application initialized");
    return true;
}

void DemoApplication::SetupScene() {
    // Create primitive meshes
    m_cubeMesh = Nova::Mesh::CreateCube(1.0f);
    m_sphereMesh = Nova::Mesh::CreateSphere(0.5f, 32);
    m_planeMesh = Nova::Mesh::CreatePlane(20.0f, 20.0f, 10, 10);
}

void DemoApplication::SetupParticles() {
    m_particles = std::make_unique<Nova::ParticleSystem>();
    if (!m_particles->Initialize(5000)) {
        spdlog::warn("Failed to initialize particle system");
        return;
    }

    auto& config = m_particles->GetConfig();
    config.emissionRate = 50.0f;
    config.lifetimeMin = 1.0f;
    config.lifetimeMax = 3.0f;
    config.velocityMin = glm::vec3(-1, 2, -1);
    config.velocityMax = glm::vec3(1, 5, 1);
    config.startSizeMin = 0.1f;
    config.startSizeMax = 0.2f;
    config.endSizeMin = 0.0f;
    config.endSizeMax = 0.05f;
    config.startColor = glm::vec4(1.0f, 0.5f, 0.2f, 1.0f);
    config.endColor = glm::vec4(1.0f, 0.2f, 0.1f, 0.0f);
    config.gravity = glm::vec3(0, -5.0f, 0);
}

void DemoApplication::SetupTerrain() {
    m_terrain = std::make_unique<Nova::TerrainGenerator>();
    m_terrain->Initialize();
}

void DemoApplication::SetupPathfinding() {
    m_pathGraph = std::make_unique<Nova::Graph>();
    m_pathGraph->BuildGrid(10, 10, 2.0f);
}

void DemoApplication::Update(float deltaTime) {
    auto& engine = Nova::Engine::Instance();
    auto& input = engine.GetInput();

    // Update camera
    m_camera->Update(input, deltaTime);

    // Toggle cursor lock with Tab
    if (input.IsKeyPressed(Nova::Key::Tab)) {
        bool locked = !input.IsCursorLocked();
        input.SetCursorLocked(locked);
    }

    // Escape to quit
    if (input.IsKeyPressed(Nova::Key::Escape)) {
        engine.RequestShutdown();
    }

    // Update rotation
    m_rotationAngle += deltaTime * 45.0f;

    // Update particles
    if (m_showParticles && m_particles) {
        m_particles->Emit(glm::vec3(0, 0, 0), 1);
        m_particles->Update(deltaTime, m_camera->GetView());
    }

    // Update terrain
    if (m_showTerrain && m_terrain) {
        m_terrain->Update(m_camera->GetPosition());
    }
}

void DemoApplication::Render() {
    auto& engine = Nova::Engine::Instance();
    auto& renderer = engine.GetRenderer();
    auto& debugDraw = renderer.GetDebugDraw();

    // Set camera
    renderer.SetCamera(m_camera.get());

    // Draw grid
    if (m_showGrid) {
        debugDraw.AddGrid(10, 1.0f, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
        debugDraw.AddTransform(glm::mat4(1.0f), 2.0f);
    }

    // Draw demo objects
    m_basicShader->Bind();
    m_basicShader->SetMat4("u_ProjectionView", m_camera->GetProjectionView());
    m_basicShader->SetVec3("u_LightDirection", m_lightDirection);
    m_basicShader->SetVec3("u_LightColor", m_lightColor);
    m_basicShader->SetFloat("u_AmbientStrength", m_ambientStrength);
    m_basicShader->SetVec3("u_ViewPos", m_camera->GetPosition());

    // Draw rotating cube
    glm::mat4 cubeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(3, 1, 0));
    cubeTransform = glm::rotate(cubeTransform, glm::radians(m_rotationAngle), glm::vec3(0, 1, 0));
    m_basicShader->SetMat4("u_Model", cubeTransform);
    m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.8f, 0.2f, 0.2f));
    m_cubeMesh->Draw();

    // Draw sphere
    glm::mat4 sphereTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-3, 0.5f, 0));
    m_basicShader->SetMat4("u_Model", sphereTransform);
    m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.2f, 0.8f, 0.2f));
    m_sphereMesh->Draw();

    // Draw ground plane
    glm::mat4 planeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
    m_basicShader->SetMat4("u_Model", planeTransform);
    m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.5f, 0.5f, 0.6f));
    m_planeMesh->Draw();

    // Draw terrain
    if (m_showTerrain && m_terrain) {
        m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.4f, 0.6f, 0.3f));
        m_terrain->Render(*m_basicShader);
    }

    // Draw particles
    if (m_showParticles && m_particles) {
        m_particles->Render(m_camera->GetProjectionView());
    }

    // Draw pathfinding graph
    if (m_showPathfinding && m_pathGraph) {
        for (const auto& [id, node] : m_pathGraph->GetNodes()) {
            debugDraw.AddPoint(node.position + glm::vec3(0, 0.1f, 0), 0.2f, glm::vec4(0, 1, 1, 1));
            for (int neighborId : node.neighbors) {
                const auto* neighbor = m_pathGraph->GetNode(neighborId);
                if (neighbor && neighborId > id) {
                    debugDraw.AddLine(node.position + glm::vec3(0, 0.1f, 0),
                                     neighbor->position + glm::vec3(0, 0.1f, 0),
                                     glm::vec4(0, 0.5f, 0.5f, 0.5f));
                }
            }
        }
    }
}

void DemoApplication::OnImGui() {
    ImGui::Begin("Demo Controls");

    ImGui::Text("Camera Position: %.1f, %.1f, %.1f",
                m_camera->GetPosition().x, m_camera->GetPosition().y, m_camera->GetPosition().z);

    ImGui::Separator();
    ImGui::Text("Render Options");
    ImGui::Checkbox("Show Grid", &m_showGrid);
    ImGui::Checkbox("Show Particles", &m_showParticles);
    ImGui::Checkbox("Show Terrain", &m_showTerrain);
    ImGui::Checkbox("Show Pathfinding", &m_showPathfinding);

    ImGui::Separator();
    ImGui::Text("Lighting");
    ImGui::SliderFloat3("Light Direction", &m_lightDirection.x, -1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", &m_lightColor.x);
    ImGui::SliderFloat("Ambient", &m_ambientStrength, 0.0f, 1.0f);

    if (m_showParticles && m_particles) {
        ImGui::Separator();
        ImGui::Text("Particles: %zu / %zu",
                    m_particles->GetActiveParticleCount(),
                    m_particles->GetMaxParticles());

        auto& config = m_particles->GetConfig();
        ImGui::SliderFloat("Emission Rate", &config.emissionRate, 0.0f, 500.0f);
        ImGui::ColorEdit4("Start Color", &config.startColor.x);
        ImGui::ColorEdit4("End Color", &config.endColor.x);
    }

    ImGui::Separator();
    ImGui::Text("Controls:");
    ImGui::BulletText("WASD - Move camera");
    ImGui::BulletText("Right Mouse + Drag - Look around");
    ImGui::BulletText("Tab - Toggle cursor lock");
    ImGui::BulletText("Shift - Sprint");
    ImGui::BulletText("Escape - Quit");

    ImGui::End();
}

void DemoApplication::Shutdown() {
    spdlog::info("Shutting down Demo Application");

    m_particles.reset();
    m_terrain.reset();
    m_pathGraph.reset();
    m_camera.reset();

    m_cubeMesh.reset();
    m_sphereMesh.reset();
    m_planeMesh.reset();
    m_basicShader.reset();
}
