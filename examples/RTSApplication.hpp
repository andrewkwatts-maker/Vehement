#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace Nova {
    class FlyCamera;
    class Shader;
    class Mesh;
    class Camera;
    class Renderer;
}

// Forward declarations for game systems
// These are available when NOVA_ENABLE_RTS_GAME is ON in CMake
namespace Vehement {
    class SoloGameMode;
    class InGameEditor;

    namespace RTS {
        class RTSInputController;
        class AIPlayer;

        namespace Campaign {
            class CampaignManager;
        }
    }
}

class SettingsMenu;

/**
 * @brief Main menu and game mode selection
 */
enum class GameMode {
    MainMenu,
    Solo,
    Online,
    Campaign
};

/**
 * @brief RTS Application with menu system
 */
class RTSApplication {
public:
    RTSApplication();
    ~RTSApplication();

    bool Initialize();
    void Update(float deltaTime);
    void Render();
    void OnImGui();
    void Shutdown();

private:
    void RenderMainMenu();
    void RenderSoloGame();
    void RenderOnlineGame();
    void RenderCampaign();

    void StartSoloGame();
    void StartOnlineGame();
    void StartCampaign(int raceIndex);
    void ReturnToMainMenu();
    void OpenSettings();
    void UpdateSoloGame(float deltaTime);

    // Core systems
    std::unique_ptr<Nova::FlyCamera> m_camera;
    std::shared_ptr<Nova::Shader> m_basicShader;

    // Demo meshes for 3D scene
    std::unique_ptr<Nova::Mesh> m_cubeMesh;
    std::unique_ptr<Nova::Mesh> m_sphereMesh;
    std::unique_ptr<Nova::Mesh> m_planeMesh;
    // Hero and building meshes for main menu
    std::unique_ptr<Nova::Mesh> m_heroMesh;
    std::unique_ptr<Nova::Mesh> m_buildingMesh1;
    std::unique_ptr<Nova::Mesh> m_buildingMesh2;
    std::unique_ptr<Nova::Mesh> m_buildingMesh3;
    std::unique_ptr<Nova::Mesh> m_terrainMesh;


    // State
    GameMode m_currentMode = GameMode::MainMenu;
    int m_selectedRace = 0;
    float m_rotationAngle = 0.0f;

    // Solo game state (placeholder until game library is built)
    float m_soloGameTime = 0.0f;
    float m_lastResourceTick = 0.0f;

    // Menu state
    bool m_showRaceSelection = false;
    bool m_showSettings = false;
    std::unique_ptr<SettingsMenu> m_settingsMenu;

    // Game mode subsystems
    // These are conditionally compiled when NOVA_ENABLE_RTS_GAME is ON
#ifdef NOVA_ENABLE_RTS_GAME
    // Solo game mode (1v1 vs AI with procedurally generated map)
    std::unique_ptr<Vehement::SoloGameMode> m_soloGameMode;

    // RTS input controller (selection, commands, camera)
    std::unique_ptr<Vehement::RTS::RTSInputController> m_rtsInputController;

    // AI player for computer opponent
    std::unique_ptr<Vehement::RTS::AIPlayer> m_aiPlayer;

    // In-game editor for map/campaign creation
    std::unique_ptr<Vehement::InGameEditor> m_inGameEditor;

    // Flag to track if full RTS systems are initialized
    bool m_rtsSystemsInitialized = false;
#endif

    // Lighting
    glm::vec3 m_lightDirection{-0.3f, -1.2f, -0.6f}; // Dramatic angle for hero
    glm::vec3 m_lightColor{1.1f, 1.05f, 0.95f}; // Warm golden light
    float m_ambientStrength = 0.2f;

    // Race definitions
    static constexpr const char* RACE_NAMES[] = {
        "Aliens",
        "Cryptids",
        "Fairies",
        "Naga",
        "Undead",
        "Vampires",
        "Humans"
    };

    static constexpr int NUM_RACES = 7;
};
