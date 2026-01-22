#include "RTSApplication.hpp"
#include "SettingsMenu.hpp"
#include "MenuSceneMeshes.hpp"

#include "core/Engine.hpp"
#include "core/Window.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Shader.hpp"
#include "graphics/debug/DebugDraw.hpp"
#include "input/InputManager.hpp"
#include "scene/FlyCamera.hpp"

// Game mode subsystems - conditionally included when RTS game library is built
// Enable by setting -DNOVA_ENABLE_RTS_GAME=ON in CMake
#ifdef NOVA_ENABLE_RTS_GAME
#include "game/src/rts/modes/SoloGameMode.hpp"
#include "game/src/rts/modes/GameMode.hpp"
#include "game/src/rts/modes/ModeRegistry.hpp"
#include "game/src/rts/RTSInputController.hpp"
#include "game/src/rts/ai/AIPlayer.hpp"
#include "game/src/rts/campaign/Campaign.hpp"
#include "game/src/rts/campaign/CampaignManager.hpp"
#include "game/src/editor/ingame/InGameEditor.hpp"
#endif

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

RTSApplication::RTSApplication() = default;

RTSApplication::~RTSApplication() = default;

bool RTSApplication::Initialize() {
    spdlog::info("Initializing RTS Application");

    // Create camera
    m_camera = std::make_unique<Nova::FlyCamera>();
    m_camera->SetPerspective(52.0f, Nova::Engine::Instance().GetWindow().GetAspectRatio(), 0.1f, 1000.0f);
    m_camera->LookAt(glm::vec3(-2.0f, 4.5f, 8.0f), glm::vec3(-4.0f, 2.0f, 3.0f)); // Cinematic hero focus

    // Create shader
    m_basicShader = std::make_shared<Nova::Shader>();
    if (!m_basicShader->LoadFromSource(BASIC_VERTEX_SHADER, BASIC_FRAGMENT_SHADER)) {
        spdlog::error("Failed to create basic shader");
        return false;
    }

    // Create primitive meshes
    m_cubeMesh = Nova::Mesh::CreateCube(1.0f);
    m_sphereMesh = Nova::Mesh::CreateSphere(0.5f, 32);
    m_planeMesh = Nova::Mesh::CreatePlane(20.0f, 20.0f, 10, 10);
    // Create menu scene meshes
    spdlog::info("Creating main menu scene meshes...");
    m_heroMesh = MenuScene::CreateHeroMesh();
    m_buildingMesh1 = MenuScene::CreateHouseMesh();
    m_buildingMesh2 = MenuScene::CreateTowerMesh();
    m_buildingMesh3 = MenuScene::CreateWallMesh();
    m_terrainMesh = MenuScene::CreateTerrainMesh(25, 2.0f);
    spdlog::info("Menu scene meshes created successfully");


    // Initialize settings menu
    m_settingsMenu = std::make_unique<SettingsMenu>();
    m_settingsMenu->Initialize(&Nova::Engine::Instance().GetInput(), &Nova::Engine::Instance().GetWindow());

    // Initialize RTS game systems if available
#ifdef NOVA_ENABLE_RTS_GAME
    spdlog::info("Initializing RTS game systems...");

    // Initialize RTS input controller
    m_rtsInputController = std::make_unique<Vehement::RTS::RTSInputController>();
    // Note: Full initialization requires camera and player setup, done when entering game modes

    // Initialize AI player for solo mode
    m_aiPlayer = std::make_unique<Vehement::RTS::AIPlayer>("AI Opponent");

    // Register standard game modes
    Vehement::ModeRegistry::Instance().Initialize();

    m_rtsSystemsInitialized = true;
    spdlog::info("RTS game systems initialized successfully");
#else
    // Placeholder mode: RTS game library not built
    // Solo/Campaign modes will use placeholder simulation
    spdlog::info("RTS game library not built - using placeholder mode");
    spdlog::info("To enable full RTS systems, build with -DNOVA_ENABLE_RTS_GAME=ON");
#endif

    spdlog::info("RTS Application initialized");
    return true;
}

void RTSApplication::Update(float deltaTime) {
    auto& engine = Nova::Engine::Instance();
    auto& input = engine.GetInput();

    // Only update camera if not in main menu
    if (m_currentMode != GameMode::MainMenu) {
        m_camera->Update(input, deltaTime);

        // Toggle cursor lock with Tab
        if (input.IsKeyPressed(Nova::Key::Tab)) {
            bool locked = !input.IsCursorLocked();
            input.SetCursorLocked(locked);
        }
    } else {
        // In main menu, ensure cursor is not locked
        if (input.IsCursorLocked()) {
            input.SetCursorLocked(false);
        }
    }

    // Escape to return to main menu or quit
    if (input.IsKeyPressed(Nova::Key::Escape)) {
        if (m_currentMode == GameMode::MainMenu) {
            engine.RequestShutdown();
        } else {
            ReturnToMainMenu();
        }
    }

    // Update rotation
    m_rotationAngle += deltaTime * 52.0f;

    // Update active subsystems based on current mode
    switch (m_currentMode) {
        case GameMode::Solo:
            // Placeholder: Update solo game simulation
            // Full integration with SoloGameMode will be added when game library is built
            UpdateSoloGame(deltaTime);
            break;

        case GameMode::MainMenu:
        case GameMode::Online:
        case GameMode::Campaign:
            // No subsystem updates needed
            break;
    }
}

void RTSApplication::Render() {
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
        m_basicShader->SetFloat("u_AmbientStrength", 0.42f); // Brighter ambience

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
}

void RTSApplication::OnImGui() {
    auto& engine = Nova::Engine::Instance();
    auto& window = engine.GetWindow();

    // Render different UI based on current mode
    switch (m_currentMode) {
        case GameMode::MainMenu:
            RenderMainMenu();
            break;
        case GameMode::Solo:
            RenderSoloGame();
            break;
        case GameMode::Online:
            RenderOnlineGame();
            break;
        case GameMode::Campaign:
            RenderCampaign();
            break;
    }

    // Render settings menu if open
    if (m_showSettings) {
        m_settingsMenu->Render(&m_showSettings);
    }
}

void RTSApplication::RenderMainMenu() {
    auto& window = Nova::Engine::Instance().GetWindow();
    ImVec2 windowSize = ImVec2(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));

    // Full screen overlay for main menu
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(windowSize);
    ImGui::Begin("MainMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse);

    // Center the menu
    ImGui::SetCursorPosY(windowSize.y * 0.25f);

    // Title
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font, but larger
    float titleSize = 48.0f;
    const char* title = "RTS GAME - NOVA3D ENGINE";
    float titleWidth = ImGui::CalcTextSize(title).x * 2.0f; // Approximate larger size
    ImGui::SetCursorPosX((windowSize.x - titleWidth) * 0.5f);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::Text("%s", title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // Menu buttons
    float buttonWidth = 300.0f;
    float buttonHeight = 50.0f;
    float centerX = (windowSize.x - buttonWidth) * 0.5f;

    ImGui::SetCursorPosX(centerX);
    if (ImGui::Button("Solo Play", ImVec2(buttonWidth, buttonHeight))) {
        StartSoloGame();
    }

    ImGui::SetCursorPosX(centerX);
    if (ImGui::Button("Online Multiplayer", ImVec2(buttonWidth, buttonHeight))) {
        StartOnlineGame();
    }

    ImGui::SetCursorPosX(centerX);
    if (ImGui::Button("Campaign", ImVec2(buttonWidth, buttonHeight))) {
        m_showRaceSelection = true;
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetCursorPosX(centerX);
    if (ImGui::Button("Settings", ImVec2(buttonWidth, buttonHeight))) {
        OpenSettings();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetCursorPosX(centerX);
    if (ImGui::Button("Exit", ImVec2(buttonWidth, buttonHeight))) {
        Nova::Engine::Instance().RequestShutdown();
    }

    ImGui::End();

    // Race selection popup
    if (m_showRaceSelection) {
        ImGui::OpenPopup("Select Race");
        ImVec2 popupSize(500, 400);
        ImGui::SetNextWindowPos(ImVec2((windowSize.x - popupSize.x) * 0.5f, (windowSize.y - popupSize.y) * 0.5f));
        ImGui::SetNextWindowSize(popupSize);

        if (ImGui::BeginPopupModal("Select Race", &m_showRaceSelection, ImGuiWindowFlags_NoResize)) {
            ImGui::Text("Choose your race for the campaign:");
            ImGui::Separator();
            ImGui::Spacing();

            for (int i = 0; i < NUM_RACES; i++) {
                if (ImGui::Selectable(RACE_NAMES[i], m_selectedRace == i, 0, ImVec2(0, 30))) {
                    m_selectedRace = i;
                }

                // Display race description
                if (m_selectedRace == i) {
                    ImGui::Indent();
                    ImGui::TextWrapped("Campaign missions for the %s faction.", RACE_NAMES[i]);
                    ImGui::Unindent();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float buttonW = 120.0f;
            ImGui::SetCursorPosX((popupSize.x - buttonW * 2 - 20) * 0.5f);
            if (ImGui::Button("Start Campaign", ImVec2(buttonW, 30))) {
                StartCampaign(m_selectedRace);
                m_showRaceSelection = false;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(buttonW, 30))) {
                m_showRaceSelection = false;
            }

            ImGui::EndPopup();
        }
    }
}

void RTSApplication::RenderSoloGame() {
    ImGui::Begin("Solo Game");

    ImGui::Text("Solo Play Mode - 1v1 Match");
    ImGui::Separator();

#ifdef NOVA_ENABLE_RTS_GAME
    if (m_soloGameMode && m_soloGameMode->IsMapGenerated()) {
        // Full RTS game mode UI
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Full RTS Systems Active");
        ImGui::Spacing();

        const auto& config = m_soloGameMode->GetConfig();
        ImGui::Text("Map: %dx%d  |  Seed: %llu", config.mapWidth, config.mapHeight, config.seed);

        const auto& spawns = m_soloGameMode->GetPlayerSpawns();
        ImGui::Text("Players: %zu spawned", spawns.size());

        const auto& resources = m_soloGameMode->GetResourceNodes();
        ImGui::Text("Resource Nodes: %zu placed", resources.size());
        ImGui::Spacing();

        // Show player spawn positions
        if (!spawns.empty()) {
            ImGui::Text("Player Spawns:");
            for (const auto& spawn : spawns) {
                ImGui::BulletText("Player %d: (%.1f, %.1f)", spawn.playerId, spawn.position.x, spawn.position.z);
            }
        }
        ImGui::Spacing();

        // Show AI player state
        if (m_aiPlayer) {
            const auto& aiState = m_aiPlayer->GetState();
            ImGui::Text("AI Opponent (%s):", m_aiPlayer->GetName().c_str());
            ImGui::Text("  Phase: %s", Vehement::RTS::StrategyPhaseToString(aiState.phase));
            ImGui::Text("  Behavior: %s", Vehement::RTS::AIBehaviorToString(aiState.behavior));
            ImGui::Text("  Workers: %d  |  Military: %d", aiState.workerCount, aiState.militaryUnits);
        }
        ImGui::Spacing();

        ImGui::Text("Starting Resources:");
        ImGui::Text("  Food: %d  Wood: %d  Stone: %d  Metal: %d",
                    config.startingFood, config.startingWood, config.startingStone, config.startingMetal);
    } else {
        // Fallback to placeholder display
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Solo Game Active (Initialization pending)");
    }
#else
    // Placeholder mode display (RTS game library not built)
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Placeholder Mode");
    ImGui::Text("RTS game library not built");
    ImGui::Text("Build with -DNOVA_ENABLE_RTS_GAME=ON for full features");
    ImGui::Spacing();

    // Simulated game state display
    ImGui::Text("Map: 64x64  |  Seed: %u", static_cast<unsigned int>(std::hash<float>{}(m_rotationAngle) % 100000));
    ImGui::Text("Players: 2 spawned (You vs AI)");
    ImGui::Text("Resources: 24 nodes placed");
    ImGui::Spacing();

    ImGui::Text("Starting Resources:");
    ImGui::Text("  Food: 500  Wood: 500  Stone: 200  Metal: 100");
    ImGui::Spacing();

    // Progress indicator for features available with full build
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Available with NOVA_ENABLE_RTS_GAME=ON:");
    ImGui::BulletText("Procedurally generated 1v1 maps");
    ImGui::BulletText("Resource placement (trees, rocks, gold)");
    ImGui::BulletText("AI opponent with decision tree logic");
    ImGui::BulletText("Full RTS input controls (selection, commands)");
    ImGui::BulletText("8 standard game modes (Melee, CTF, KotH, etc.)");
#endif

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Game Time: %.1f seconds", m_soloGameTime);
    ImGui::Text("Camera Position: %.1f, %.1f, %.1f",
                m_camera->GetPosition().x, m_camera->GetPosition().y, m_camera->GetPosition().z);

    ImGui::Spacing();
    ImGui::Text("Controls:");
    ImGui::BulletText("WASD - Move camera");
    ImGui::BulletText("Right Mouse + Drag - Look around");
    ImGui::BulletText("Tab - Toggle cursor lock");
    ImGui::BulletText("Shift - Sprint");
    ImGui::BulletText("Escape - Return to Main Menu");

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Return to Main Menu")) {
        ReturnToMainMenu();
    }

    ImGui::End();
}

void RTSApplication::RenderOnlineGame() {
    ImGui::Begin("Online Multiplayer");

    ImGui::Text("Online Multiplayer Mode");
    ImGui::Separator();

    ImGui::TextWrapped("Online multiplayer features will be implemented here, including:");
    ImGui::BulletText("Matchmaking");
    ImGui::BulletText("Lobby system");
    ImGui::BulletText("Player synchronization");
    ImGui::BulletText("Leaderboards");

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Return to Main Menu")) {
        ReturnToMainMenu();
    }

    ImGui::End();
}

void RTSApplication::RenderCampaign() {
    ImGui::Begin("Campaign");

    ImGui::Text("Campaign Mode - %s", RACE_NAMES[m_selectedRace]);
    ImGui::Separator();

#ifdef NOVA_ENABLE_RTS_GAME
    // Full campaign system when RTS game library is built
    auto& campaignMgr = Vehement::RTS::Campaign::CampaignManager::Instance();

    if (campaignMgr.IsInitialized()) {
        auto* currentCampaign = campaignMgr.GetCurrentCampaign();

        if (currentCampaign) {
            // Use public member variables (Campaign class uses direct access)
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Campaign: %s", currentCampaign->title.c_str());
            ImGui::TextWrapped("%s", currentCampaign->description.c_str());
            ImGui::Spacing();

            // Show chapters
            const auto& chapters = currentCampaign->chapters;
            ImGui::Text("Chapters: %zu", chapters.size());

            for (size_t i = 0; i < chapters.size() && i < 7; ++i) {
                ImGui::PushID(static_cast<int>(i));
                const auto& chapter = chapters[i];
                // Chapter class would have similar public members
                std::string label = "Chapter " + std::to_string(i + 1);
                if (ImGui::Button(label.c_str(), ImVec2(300, 30))) {
                    // Start chapter via campaign manager
                    spdlog::info("Starting chapter {} for {}", i + 1, RACE_NAMES[m_selectedRace]);
                }
                ImGui::PopID();
            }
        } else {
            ImGui::TextWrapped("Campaign missions for the %s faction.", RACE_NAMES[m_selectedRace]);
            ImGui::Spacing();
            ImGui::Text("Select a campaign to begin...");
        }
    } else {
        // Campaign manager not initialized - show placeholder
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Campaign system initializing...");
    }
#else
    // Placeholder mode when RTS game library is not built
    ImGui::TextWrapped("Campaign missions for the %s faction.", RACE_NAMES[m_selectedRace]);
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Placeholder Mode");
    ImGui::Text("Build with -DNOVA_ENABLE_RTS_GAME=ON for full campaign");
    ImGui::Spacing();

    ImGui::Text("Available Missions:");

    // Display sample missions (placeholder)
    for (int i = 1; i <= 7; i++) {
        ImGui::PushID(i);
        if (ImGui::Button(("Mission " + std::to_string(i)).c_str(), ImVec2(200, 30))) {
            spdlog::info("Starting mission {} for {}", i, RACE_NAMES[m_selectedRace]);
        }
        ImGui::PopID();
    }
#endif

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Return to Main Menu")) {
        ReturnToMainMenu();
    }

    ImGui::End();
}

void RTSApplication::StartSoloGame() {
    spdlog::info("Starting Solo Game");

    // Reset solo game state
    m_soloGameTime = 0.0f;
    m_lastResourceTick = 0.0f;

#ifdef NOVA_ENABLE_RTS_GAME
    if (m_rtsSystemsInitialized) {
        // Full RTS game mode with procedurally generated map
        spdlog::info("Initializing full Solo Game Mode...");

        // Create solo game mode with default config
        m_soloGameMode = std::make_unique<Vehement::SoloGameMode>();

        Vehement::SoloGameConfig config;
        config.mapWidth = 64;
        config.mapHeight = 64;
        config.tileSize = 1.0f;
        config.seed = 0;  // Random seed
        config.aiDifficulty = "medium";
        config.startingFood = 500;
        config.startingWood = 500;
        config.startingStone = 200;
        config.startingMetal = 100;

        if (m_soloGameMode->Initialize(config)) {
            // Generate the map
            auto& renderer = Nova::Engine::Instance().GetRenderer();
            if (m_soloGameMode->GenerateMap(renderer)) {
                spdlog::info("Solo game map generated successfully");

                // Setup AI player
                if (m_aiPlayer) {
                    m_aiPlayer->SetBehavior(Vehement::RTS::AIBehavior::Balanced);
                    m_aiPlayer->SetDifficulty(1.0f);  // Normal difficulty
                    glm::vec3 aiSpawn = m_soloGameMode->GetPlayerSpawnPosition(1);
                    m_aiPlayer->SetBaseLocation(glm::vec2(aiSpawn.x, aiSpawn.z));
                    spdlog::info("AI player initialized at ({}, {})", aiSpawn.x, aiSpawn.z);
                }

                // Initialize RTS input controller with camera
                if (m_rtsInputController && m_camera) {
                    // RTS input controller would be fully initialized here
                    // m_rtsInputController->Initialize(m_camera.get(), humanPlayer);
                    spdlog::info("RTS input controller ready");
                }
            } else {
                spdlog::warn("Failed to generate solo game map, using placeholder");
                m_soloGameMode.reset();
            }
        } else {
            spdlog::warn("Failed to initialize solo game mode, using placeholder");
            m_soloGameMode.reset();
        }
    }

    if (m_soloGameMode) {
        spdlog::info("Solo game initialized with full RTS systems");
    } else {
        spdlog::info("Solo game initialized (placeholder mode)");
    }
#else
    // Placeholder mode when RTS game library is not built
    spdlog::info("Solo game initialized (placeholder mode)");
#endif

    m_currentMode = GameMode::Solo;
}

void RTSApplication::StartOnlineGame() {
    spdlog::info("Starting Online Multiplayer");
    m_currentMode = GameMode::Online;
}

void RTSApplication::StartCampaign(int raceIndex) {
    spdlog::info("Starting Campaign for race: {}", RACE_NAMES[raceIndex]);
    m_selectedRace = raceIndex;

#ifdef NOVA_ENABLE_RTS_GAME
    if (m_rtsSystemsInitialized) {
        auto& campaignMgr = Vehement::RTS::Campaign::CampaignManager::Instance();

        if (!campaignMgr.IsInitialized()) {
            if (campaignMgr.Initialize()) {
                // Load campaigns from the game assets directory
                campaignMgr.LoadAllCampaigns("game_assets/campaigns/");
                spdlog::info("Campaign system initialized");
            } else {
                spdlog::warn("Failed to initialize campaign system");
            }
        }

        // Map race index to campaign ID
        static const char* RACE_CAMPAIGN_IDS[] = {
            "campaign_aliens",
            "campaign_cryptids",
            "campaign_fairies",
            "campaign_naga",
            "campaign_undead",
            "campaign_vampires",
            "campaign_humans"
        };

        if (raceIndex >= 0 && raceIndex < NUM_RACES) {
            auto* campaign = campaignMgr.GetCampaign(RACE_CAMPAIGN_IDS[raceIndex]);
            if (campaign) {
                campaignMgr.SetCurrentCampaign(RACE_CAMPAIGN_IDS[raceIndex]);
                campaignMgr.StartCampaign(RACE_CAMPAIGN_IDS[raceIndex],
                                          Vehement::RTS::Campaign::CampaignDifficulty::Normal);
                // Use public member variable 'title' instead of GetTitle()
                spdlog::info("Campaign started: {}", campaign->title);
            } else {
                spdlog::warn("Campaign not found for race: {}", RACE_NAMES[raceIndex]);
            }
        }
    }
#endif

    m_currentMode = GameMode::Campaign;
}

void RTSApplication::OpenSettings() {
    spdlog::info("Opening Settings");
    m_showSettings = true;
}

void RTSApplication::ReturnToMainMenu() {
    spdlog::info("Returning to Main Menu");

#ifdef NOVA_ENABLE_RTS_GAME
    // Clean up active game modes
    if (m_soloGameMode) {
        m_soloGameMode->Shutdown();
        m_soloGameMode.reset();
        spdlog::info("Solo game mode shut down");
    }
#endif

    // Reset game state
    m_soloGameTime = 0.0f;
    m_lastResourceTick = 0.0f;

    m_currentMode = GameMode::MainMenu;
}

void RTSApplication::UpdateSoloGame(float deltaTime) {
#ifdef NOVA_ENABLE_RTS_GAME
    // Full RTS game update when game library is built
    if (m_soloGameMode) {
        // Update game world simulation
        m_soloGameMode->Update(deltaTime);

        // Update AI player decision making
        if (m_aiPlayer) {
            // AI update would be called here with game systems
            // m_aiPlayer->Update(deltaTime, population, entityManager, ...);
        }

        // Update RTS input handling
        if (m_rtsInputController) {
            auto& input = Nova::Engine::Instance().GetInput();
            m_rtsInputController->Update(input, deltaTime);
        }

        m_soloGameTime += deltaTime;
        return;
    }
#endif

    // Placeholder solo game update - simulates game logic
    // Used when RTS game library is not built or initialization failed
    m_soloGameTime += deltaTime;

    // Simulate resource accumulation every second
    if (m_soloGameTime - m_lastResourceTick >= 1.0f) {
        m_lastResourceTick = m_soloGameTime;
        // Resources would accumulate here in the real implementation
    }
}

void RTSApplication::Shutdown() {
    spdlog::info("Shutting down RTS Application");

    // Shutdown subsystems
    if (m_settingsMenu) {
        m_settingsMenu->Shutdown();
        m_settingsMenu.reset();
    }

    // Reset solo game state
    m_soloGameTime = 0.0f;
    m_lastResourceTick = 0.0f;

#ifdef NOVA_ENABLE_RTS_GAME
    // Shutdown RTS game systems
    if (m_inGameEditor) {
        m_inGameEditor->Shutdown();
        m_inGameEditor.reset();
    }

    if (m_soloGameMode) {
        m_soloGameMode->Shutdown();
        m_soloGameMode.reset();
    }

    m_rtsInputController.reset();
    m_aiPlayer.reset();

    // Shutdown mode registry
    Vehement::ModeRegistry::Instance().Shutdown();

    m_rtsSystemsInitialized = false;
    spdlog::info("RTS game systems shut down");
#endif

    // Cleanup graphics resources
    m_camera.reset();
    m_cubeMesh.reset();
    m_sphereMesh.reset();
    m_planeMesh.reset();
    m_heroMesh.reset();
    m_buildingMesh1.reset();
    m_buildingMesh2.reset();
    m_buildingMesh3.reset();
    m_terrainMesh.reset();
    m_basicShader.reset();
}
