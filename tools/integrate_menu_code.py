#!/usr/bin/env python3
"""
Integrate menu scene enhancements into RTSApplication.cpp
"""

import re
from pathlib import Path

def integrate_menu_code():
    """Integrate the menu scene code into RTSApplication.cpp"""

    project_root = Path(__file__).parent.parent
    rts_app_cpp = project_root / "examples/RTSApplication.cpp"

    print(f"Integrating menu code into {rts_app_cpp}")

    # Read the file
    with open(rts_app_cpp, 'r') as f:
        content = f.read()

    # Check if already integrated
    if "MenuScene::CreateHeroMesh" in content:
        print("  OK Menu code already integrated")
        return True

    # Add mesh creation in Initialize()
    init_code = """
    // Create menu scene meshes
    spdlog::info("Creating main menu scene meshes...");
    m_heroMesh = MenuScene::CreateHeroMesh();
    m_buildingMesh1 = MenuScene::CreateHouseMesh();
    m_buildingMesh2 = MenuScene::CreateTowerMesh();
    m_buildingMesh3 = MenuScene::CreateWallMesh();
    m_terrainMesh = MenuScene::CreateTerrainMesh(25, 2.0f);
    spdlog::info("Menu scene meshes created successfully");
"""

    # Find Initialize() function and add after mesh creation
    init_pattern = r'(m_planeMesh = Nova::Mesh::CreatePlane[^;]+;)'
    if re.search(init_pattern, content):
        content = re.sub(init_pattern, r'\1' + init_code, content)
        print("  OK Added mesh creation to Initialize()")
    else:
        print("  ERROR Could not find mesh creation location")
        return False

    # Replace Render() function
    render_code = """void RTSApplication::Render() {
    auto& engine = Nova::Engine::Instance();
    auto& renderer = engine.GetRenderer();
    auto& debugDraw = renderer.GetDebugDraw();

    m_basicShader->Bind();
    m_basicShader->SetMat4("u_ProjectionView", m_camera->GetProjectionView());
    m_basicShader->SetVec3("u_LightDirection", m_lightDirection);
    m_basicShader->SetVec3("u_LightColor", m_lightColor);
    m_basicShader->SetVec3("u_ViewPos", m_camera->GetPosition());

    if (m_currentMode == GameMode::MainMenu) {
        // MAIN MENU: Fantasy movie scene
        m_basicShader->SetFloat("u_AmbientStrength", 0.35f);

        // Terrain
        if (m_terrainMesh) {
            glm::mat4 terrainTransform = glm::mat4(1.0f);
            m_basicShader->SetMat4("u_Model", terrainTransform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.3f, 0.65f, 0.3f));
            m_terrainMesh->Draw();
        }

        // Hero
        if (m_heroMesh) {
            glm::mat4 heroTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-4.0f, 0.0f, 3.0f));
            heroTransform = glm::rotate(heroTransform, glm::radians(25.0f), glm::vec3(0, 1, 0));
            heroTransform = glm::scale(heroTransform, glm::vec3(1.3f));
            m_basicShader->SetMat4("u_Model", heroTransform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.75f, 0.55f, 0.35f));
            m_heroMesh->Draw();
        }

        // Buildings
        if (m_buildingMesh1) {
            glm::mat4 building1Transform = glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 0.0f, -1.0f));
            building1Transform = glm::rotate(building1Transform, glm::radians(-15.0f), glm::vec3(0, 1, 0));
            m_basicShader->SetMat4("u_Model", building1Transform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.65f, 0.55f, 0.45f));
            m_buildingMesh1->Draw();
        }

        if (m_buildingMesh2) {
            glm::mat4 building2Transform = glm::translate(glm::mat4(1.0f), glm::vec3(9.0f, 0.0f, -6.0f));
            m_basicShader->SetMat4("u_Model", building2Transform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.55f, 0.55f, 0.60f));
            m_buildingMesh2->Draw();
        }

        if (m_buildingMesh3) {
            glm::mat4 building3Transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -15.0f));
            m_basicShader->SetMat4("u_Model", building3Transform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.45f, 0.45f, 0.50f));
            m_buildingMesh3->Draw();
        }

    } else {
        // OTHER MODES
        debugDraw.AddGrid(10, 1.0f, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
        debugDraw.AddTransform(glm::mat4(1.0f), 2.0f);
        m_basicShader->SetFloat("u_AmbientStrength", m_ambientStrength);

        if (m_cubeMesh) {
            glm::mat4 cubeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(3, 1, 0));
            cubeTransform = glm::rotate(cubeTransform, glm::radians(m_rotationAngle), glm::vec3(0, 1, 0));
            m_basicShader->SetMat4("u_Model", cubeTransform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.8f, 0.2f, 0.2f));
            m_cubeMesh->Draw();
        }

        if (m_sphereMesh) {
            glm::mat4 sphereTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-3, 0.5f, 0));
            m_basicShader->SetMat4("u_Model", sphereTransform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.2f, 0.8f, 0.2f));
            m_sphereMesh->Draw();
        }

        if (m_planeMesh) {
            glm::mat4 planeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
            m_basicShader->SetMat4("u_Model", planeTransform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.5f, 0.5f, 0.6f));
            m_planeMesh->Draw();
        }
    }
}"""

    # Find and replace Render() function
    render_pattern = r'void RTSApplication::Render\(\) \{[^}]+debugDraw\.AddGrid[^}]+\}'
    if re.search(render_pattern, content, re.DOTALL):
        content = re.sub(render_pattern, render_code, content, flags=re.DOTALL)
        print("  OK Replaced Render() function")
    else:
        print("  ERROR Could not find Render() function")
        return False

    # Write back
    with open(rts_app_cpp, 'w') as f:
        f.write(content)

    print(f"  OK Integration complete!")
    return True

if __name__ == "__main__":
    success = integrate_menu_code()
    exit(0 if success else 1)
