#include "Game.hpp"

#include <core/Engine.hpp>
#include <core/Logger.hpp>
#include <core/Time.hpp>
#include <input/InputManager.hpp>
#include <scene/Scene.hpp>

#include <fstream>
#include <filesystem>

namespace Vehement {

// =============================================================================
// Construction / Destruction
// =============================================================================

Game::Game(Nova::Engine& engine)
    : m_engine(engine)
{
    Nova::Logger::Info("[Vehement] Game instance created");
}

Game::~Game() {
    Shutdown();
}

// =============================================================================
// Initialization and Lifecycle
// =============================================================================

std::expected<void, GameError> Game::Initialize(const GameInitParams& params) {
    if (m_initialized) {
        Nova::Logger::Warn("[Vehement] Game already initialized");
        return {};
    }

    Nova::Logger::Info("[Vehement] Initializing game...");
    m_initParams = params;

    // Load game configuration
    if (std::filesystem::exists(params.configPath)) {
        try {
            std::ifstream configFile(params.configPath);
            if (configFile.is_open()) {
                configFile >> m_gameConfig;
                Nova::Logger::Info("[Vehement] Loaded game config from: {}", params.configPath);
            }
        } catch (const std::exception& e) {
            Nova::Logger::Warn("[Vehement] Failed to load config: {}", e.what());
            // Continue with defaults
        }
    }

    // Initialize subsystems
    // Note: These are placeholder initializations - actual implementations
    // will be added as the respective systems are developed

    // m_world = std::make_unique<World>();
    // m_entityManager = std::make_unique<EntityManager>();
    // m_combatSystem = std::make_unique<CombatSystem>();
    // m_uiManager = std::make_unique<UIManager>();
    // m_networkManager = std::make_unique<NetworkManager>();

    // Initialize GPS if requested
    if (params.enableGPS) {
        EnableGPS();
    }

    // Connect to Firebase if multiplayer enabled
    if (params.enableMultiplayer) {
        auto result = ConnectToFirebase(params.authToken);
        if (!result) {
            Nova::Logger::Warn("[Vehement] Failed to connect to Firebase, continuing offline");
        }
    }

    m_initialized = true;

    // Transition to appropriate starting state
    if (params.startInEditor) {
        TransitionTo(GameState::Editor);
    } else if (!params.levelPath.empty()) {
        auto loadResult = LoadLevel(params.levelPath);
        if (!loadResult) {
            return std::unexpected(loadResult.error());
        }
    } else {
        TransitionTo(GameState::MainMenu);
    }

    Nova::Logger::Info("[Vehement] Game initialized successfully");
    return {};
}

void Game::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Nova::Logger::Info("[Vehement] Shutting down game...");

    // Disconnect from network
    DisconnectFromFirebase();

    // Disable GPS
    DisableGPS();

    // Unload current level
    UnloadLevel();

    // Release subsystems in reverse order
    m_firebase.reset();
    m_networkManager.reset();
    m_uiManager.reset();
    m_combatSystem.reset();
    m_localPlayer.reset();
    m_entityManager.reset();
    m_world.reset();

    m_initialized = false;
    m_state = GameState::Initializing;

    Nova::Logger::Info("[Vehement] Game shutdown complete");
}

void Game::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Process input first
    ProcessInput(deltaTime);

    // State-specific update
    switch (m_state) {
        case GameState::Initializing:
            // Nothing to do - waiting for initialization
            break;

        case GameState::MainMenu:
            UpdateMainMenu(deltaTime);
            break;

        case GameState::Loading:
            UpdateLoading(deltaTime);
            break;

        case GameState::Playing:
            UpdatePlaying(deltaTime);
            break;

        case GameState::Paused:
            UpdatePaused(deltaTime);
            break;

        case GameState::GameOver:
            UpdateGameOver(deltaTime);
            break;

        case GameState::Editor:
            UpdateEditor(deltaTime);
            break;

        case GameState::Connecting:
        case GameState::Disconnected:
            // Network state handling
            ProcessNetworkMessages();
            break;
    }

    // Network sync (if connected)
    if (IsConnected()) {
        SyncWithFirebase(deltaTime);
    }
}

void Game::Render() {
    if (!m_initialized) {
        return;
    }

    // Render world and entities
    if (m_world && (m_state == GameState::Playing || m_state == GameState::Paused || m_state == GameState::Editor)) {
        // m_world->Render();
    }

    // Render entities
    if (m_entityManager && m_state == GameState::Playing) {
        // m_entityManager->Render();
    }

    // UI is rendered separately in RenderImGui
}

void Game::RenderImGui() {
    if (!m_initialized) {
        return;
    }

    // Render game UI based on state
    if (m_uiManager) {
        // m_uiManager->Render(m_state);
    }

    #ifdef VEHEMENT_DEBUG
    // Debug overlay
    // RenderDebugOverlay();
    #endif
}

// =============================================================================
// State Management
// =============================================================================

std::string_view Game::GetStateString() const noexcept {
    switch (m_state) {
        case GameState::Initializing: return "Initializing";
        case GameState::MainMenu:     return "MainMenu";
        case GameState::Loading:      return "Loading";
        case GameState::Playing:      return "Playing";
        case GameState::Paused:       return "Paused";
        case GameState::GameOver:     return "GameOver";
        case GameState::Editor:       return "Editor";
        case GameState::Connecting:   return "Connecting";
        case GameState::Disconnected: return "Disconnected";
        default:                      return "Unknown";
    }
}

bool Game::TransitionTo(GameState newState) {
    if (!IsValidTransition(m_state, newState)) {
        Nova::Logger::Warn("[Vehement] Invalid state transition: {} -> {}",
            GetStateString(),
            [newState]() -> std::string_view {
                switch (newState) {
                    case GameState::Initializing: return "Initializing";
                    case GameState::MainMenu:     return "MainMenu";
                    case GameState::Loading:      return "Loading";
                    case GameState::Playing:      return "Playing";
                    case GameState::Paused:       return "Paused";
                    case GameState::GameOver:     return "GameOver";
                    case GameState::Editor:       return "Editor";
                    case GameState::Connecting:   return "Connecting";
                    case GameState::Disconnected: return "Disconnected";
                    default:                      return "Unknown";
                }
            }());
        return false;
    }

    Nova::Logger::Info("[Vehement] State transition: {} -> {}", GetStateString(),
        [newState]() -> std::string_view {
            switch (newState) {
                case GameState::Initializing: return "Initializing";
                case GameState::MainMenu:     return "MainMenu";
                case GameState::Loading:      return "Loading";
                case GameState::Playing:      return "Playing";
                case GameState::Paused:       return "Paused";
                case GameState::GameOver:     return "GameOver";
                case GameState::Editor:       return "Editor";
                case GameState::Connecting:   return "Connecting";
                case GameState::Disconnected: return "Disconnected";
                default:                      return "Unknown";
            }
        }());

    OnExitState(m_state);
    m_previousState = m_state;
    m_state = newState;
    OnEnterState(newState);

    return true;
}

bool Game::IsValidTransition(GameState from, GameState to) const {
    // Define valid state transitions
    switch (from) {
        case GameState::Initializing:
            return to == GameState::MainMenu || to == GameState::Editor || to == GameState::Loading;

        case GameState::MainMenu:
            return to == GameState::Loading || to == GameState::Connecting ||
                   to == GameState::Editor || to == GameState::Playing;

        case GameState::Loading:
            return to == GameState::Playing || to == GameState::MainMenu || to == GameState::Editor;

        case GameState::Playing:
            return to == GameState::Paused || to == GameState::GameOver ||
                   to == GameState::MainMenu || to == GameState::Disconnected;

        case GameState::Paused:
            return to == GameState::Playing || to == GameState::MainMenu ||
                   to == GameState::Editor;

        case GameState::GameOver:
            return to == GameState::MainMenu || to == GameState::Loading;

        case GameState::Editor:
            return to == GameState::MainMenu || to == GameState::Playing ||
                   to == GameState::Loading || to == GameState::Paused;

        case GameState::Connecting:
            return to == GameState::Playing || to == GameState::MainMenu ||
                   to == GameState::Disconnected;

        case GameState::Disconnected:
            return to == GameState::MainMenu || to == GameState::Connecting;

        default:
            return false;
    }
}

void Game::OnEnterState(GameState state) {
    switch (state) {
        case GameState::Playing:
            m_totalPlayTime = 0.0f;
            m_spawnTimer = 0.0f;
            break;

        case GameState::Paused:
            // Could pause audio, etc.
            break;

        case GameState::Loading:
            m_loadProgress = 0.0f;
            m_loadingMessage = "Loading...";
            break;

        case GameState::Editor:
            // Initialize editor systems
            break;

        default:
            break;
    }
}

void Game::OnExitState(GameState state) {
    switch (state) {
        case GameState::Playing:
            // Save stats, etc.
            break;

        case GameState::Editor:
            // Cleanup editor resources
            break;

        default:
            break;
    }
}

void Game::Pause() {
    if (m_state == GameState::Playing) {
        TransitionTo(GameState::Paused);
    }
}

void Game::Resume() {
    if (m_state == GameState::Paused) {
        TransitionTo(GameState::Playing);
    }
}

void Game::TogglePause() {
    if (m_state == GameState::Playing) {
        Pause();
    } else if (m_state == GameState::Paused) {
        Resume();
    }
}

// =============================================================================
// Level Management
// =============================================================================

std::expected<void, GameError> Game::LoadLevel(std::string_view levelPath) {
    Nova::Logger::Info("[Vehement] Loading level: {}", levelPath);

    TransitionTo(GameState::Loading);
    m_loadingMessage = "Loading level...";

    // Check if file exists
    if (!std::filesystem::exists(levelPath)) {
        Nova::Logger::Error("[Vehement] Level file not found: {}", levelPath);
        TransitionTo(GameState::MainMenu);
        return std::unexpected(GameError::AssetNotFound);
    }

    // TODO: Implement actual level loading
    // For now, simulate loading
    m_loadProgress = 1.0f;

    TransitionTo(GameState::Playing);
    Nova::Logger::Info("[Vehement] Level loaded successfully");

    return {};
}

void Game::UnloadLevel() {
    Nova::Logger::Info("[Vehement] Unloading current level");

    // Clear entities
    if (m_entityManager) {
        // m_entityManager->Clear();
    }

    // Clear world
    if (m_world) {
        // m_world->Clear();
    }

    // Reset game state
    m_currentWave = 0;
    m_zombiesKilled = 0;
    m_totalPlayTime = 0.0f;
}

void Game::NewGame() {
    Nova::Logger::Info("[Vehement] Starting new game");

    UnloadLevel();

    // Reset player stats
    m_currentWave = 1;
    m_zombiesKilled = 0;
    m_totalPlayTime = 0.0f;

    // Create local player
    // m_localPlayer = std::make_unique<Player>();

    TransitionTo(GameState::Playing);
}

std::expected<void, GameError> Game::SaveGame(std::string_view savePath) {
    Nova::Logger::Info("[Vehement] Saving game to: {}", savePath);

    nlohmann::json saveData;
    saveData["version"] = std::string(Config::GameVersion);
    saveData["playTime"] = m_totalPlayTime;
    saveData["wave"] = m_currentWave;
    saveData["zombiesKilled"] = m_zombiesKilled;

    // TODO: Save world state, entities, player data, etc.

    try {
        std::ofstream file(std::string(savePath));
        if (!file.is_open()) {
            return std::unexpected(GameError::SaveFailed);
        }
        file << saveData.dump(2);
    } catch (const std::exception& e) {
        Nova::Logger::Error("[Vehement] Failed to save game: {}", e.what());
        return std::unexpected(GameError::SaveFailed);
    }

    Nova::Logger::Info("[Vehement] Game saved successfully");
    return {};
}

std::expected<void, GameError> Game::LoadGame(std::string_view savePath) {
    Nova::Logger::Info("[Vehement] Loading game from: {}", savePath);

    if (!std::filesystem::exists(savePath)) {
        return std::unexpected(GameError::AssetNotFound);
    }

    try {
        std::ifstream file(std::string(savePath));
        if (!file.is_open()) {
            return std::unexpected(GameError::LoadFailed);
        }

        nlohmann::json saveData;
        file >> saveData;

        // Validate version
        if (saveData.contains("version")) {
            // Version check could go here
        }

        m_totalPlayTime = saveData.value("playTime", 0.0f);
        m_currentWave = saveData.value("wave", 1);
        m_zombiesKilled = saveData.value("zombiesKilled", 0);

        // TODO: Load world state, entities, player data, etc.

    } catch (const std::exception& e) {
        Nova::Logger::Error("[Vehement] Failed to load game: {}", e.what());
        return std::unexpected(GameError::LoadFailed);
    }

    TransitionTo(GameState::Playing);
    Nova::Logger::Info("[Vehement] Game loaded successfully");
    return {};
}

// =============================================================================
// Multiplayer and Network
// =============================================================================

std::expected<void, GameError> Game::ConnectToFirebase(std::optional<std::string_view> authToken) {
    Nova::Logger::Info("[Vehement] Connecting to Firebase...");

    TransitionTo(GameState::Connecting);

    // TODO: Implement actual Firebase connection
    // m_firebase = std::make_unique<FirebaseConnection>();
    // auto result = m_firebase->Connect(Config::Firebase::ProjectId, authToken);

    // For now, simulate connection failure (offline mode)
    Nova::Logger::Warn("[Vehement] Firebase not implemented, running in offline mode");

    TransitionTo(GameState::MainMenu);
    return {};
}

void Game::DisconnectFromFirebase() {
    if (m_firebase) {
        Nova::Logger::Info("[Vehement] Disconnecting from Firebase...");
        // m_firebase->Disconnect();
        m_firebase.reset();
    }
}

bool Game::IsConnected() const noexcept {
    return m_firebase != nullptr; // && m_firebase->IsConnected();
}

std::expected<std::string, GameError> Game::HostMatch(std::string_view matchName, int32_t maxPlayers) {
    Nova::Logger::Info("[Vehement] Hosting match: {} (max {} players)", matchName, maxPlayers);

    if (!IsConnected()) {
        return std::unexpected(GameError::NetworkError);
    }

    // TODO: Implement match hosting via Firebase

    return std::unexpected(GameError::NetworkError);
}

std::expected<void, GameError> Game::JoinMatch(std::string_view matchId) {
    Nova::Logger::Info("[Vehement] Joining match: {}", matchId);

    if (!IsConnected()) {
        return std::unexpected(GameError::NetworkError);
    }

    // TODO: Implement match joining via Firebase

    return std::unexpected(GameError::NetworkError);
}

void Game::LeaveMatch() {
    Nova::Logger::Info("[Vehement] Leaving current match");

    // TODO: Implement match leaving
}

// =============================================================================
// GPS and Location
// =============================================================================

void Game::EnableGPS() {
    Nova::Logger::Info("[Vehement] Enabling GPS tracking");
    m_gpsEnabled = true;
    m_gpsSyncTimer = 0.0f;
}

void Game::DisableGPS() {
    Nova::Logger::Info("[Vehement] Disabling GPS tracking");
    m_gpsEnabled = false;
    m_currentLocation = std::nullopt;
}

std::optional<GPSLocation> Game::GetCurrentLocation() const {
    return m_currentLocation;
}

void Game::UpdateGPSLocation(const GPSLocation& location) {
    if (!m_gpsEnabled) {
        return;
    }

    if (location.isValid()) {
        m_currentLocation = location;

        // Convert GPS to world position and update player
        if (m_localPlayer) {
            [[maybe_unused]] auto worldPos = location.toWorldPosition();
            // m_localPlayer->SetPosition(glm::vec3(worldPos.x, 0.0f, worldPos.y));
        }
    }
}

// =============================================================================
// Editor Mode
// =============================================================================

void Game::EnterEditor() {
    Nova::Logger::Info("[Vehement] Entering editor mode");
    TransitionTo(GameState::Editor);
}

void Game::ExitEditor() {
    Nova::Logger::Info("[Vehement] Exiting editor mode");
    TransitionTo(GameState::MainMenu);
}

// =============================================================================
// Private Update Functions
// =============================================================================

void Game::UpdateMainMenu([[maybe_unused]] float deltaTime) {
    // Main menu is primarily UI-driven
    // Update any animated elements
}

void Game::UpdateLoading([[maybe_unused]] float deltaTime) {
    // Loading progress is updated by actual loading operations
    // Check if loading is complete
    if (m_loadProgress >= 1.0f) {
        TransitionTo(GameState::Playing);
    }
}

void Game::UpdatePlaying(float deltaTime) {
    // Update play time
    m_totalPlayTime += deltaTime;

    // Update world
    if (m_world) {
        // m_world->Update(deltaTime);
    }

    // Update entities
    if (m_entityManager) {
        // m_entityManager->Update(deltaTime);
    }

    // Update combat
    if (m_combatSystem) {
        // m_combatSystem->Update(deltaTime);
    }

    // Update spawning system
    UpdateSpawning(deltaTime);
}

void Game::UpdatePaused([[maybe_unused]] float deltaTime) {
    // Game logic paused, but UI still updates
}

void Game::UpdateGameOver([[maybe_unused]] float deltaTime) {
    // Show game over UI, wait for player input
}

void Game::UpdateEditor(float deltaTime) {
    // Update editor tools and preview
    if (m_world) {
        // m_world->Update(deltaTime);
    }
}

// =============================================================================
// Input Handling
// =============================================================================

void Game::ProcessInput([[maybe_unused]] float deltaTime) {
    auto& input = m_engine.GetInput();

    // Global inputs (work in any state)
    if (input.IsKeyPressed(Nova::Key::Escape)) {
        if (m_state == GameState::Playing) {
            Pause();
        } else if (m_state == GameState::Paused) {
            Resume();
        }
    }

    // State-specific input
    switch (m_state) {
        case GameState::Playing:
            ProcessPlayingInput(deltaTime);
            break;

        case GameState::MainMenu:
        case GameState::Paused:
        case GameState::GameOver:
            ProcessMenuInput(deltaTime);
            break;

        case GameState::Editor:
            ProcessEditorInput(deltaTime);
            break;

        default:
            break;
    }
}

void Game::ProcessPlayingInput([[maybe_unused]] float deltaTime) {
    [[maybe_unused]] auto& input = m_engine.GetInput();

    // Movement, shooting, etc. handled by player entity
    // This is for game-level inputs only

    #ifdef VEHEMENT_EDITOR
    // Quick editor toggle in debug builds
    // if (input.IsKeyPressed(Nova::Key::F1)) {
    //     EnterEditor();
    // }
    #endif
}

void Game::ProcessMenuInput([[maybe_unused]] float deltaTime) {
    // Menu navigation - primarily handled by UI system
}

void Game::ProcessEditorInput([[maybe_unused]] float deltaTime) {
    // Editor-specific controls
}

// =============================================================================
// Network Synchronization
// =============================================================================

void Game::SyncWithFirebase(float deltaTime) {
    m_networkSyncTimer += deltaTime;

    if (m_networkSyncTimer >= Config::Firebase::SyncIntervalSeconds) {
        m_networkSyncTimer = 0.0f;

        // Sync player position and state
        if (m_localPlayer && m_firebase) {
            // auto playerData = m_localPlayer->Serialize();
            // m_firebase->SyncPlayerState(playerData);
        }

        // Process incoming network messages
        ProcessNetworkMessages();
    }
}

void Game::ProcessNetworkMessages() {
    if (!m_firebase) {
        return;
    }

    // TODO: Process incoming messages from Firebase
    // - Other player positions
    // - World state changes
    // - Match events
}

// =============================================================================
// Spawn System
// =============================================================================

void Game::UpdateSpawning(float deltaTime) {
    m_spawnTimer += deltaTime;

    // Calculate spawn rate based on wave
    float spawnRate = std::max(
        Config::Zombie::MinSpawnRate,
        Config::Zombie::BaseSpawnRate - (m_currentWave - 1) * Config::Zombie::SpawnRateDecreasePerWave
    );

    if (m_spawnTimer >= spawnRate) {
        m_spawnTimer = 0.0f;
        SpawnZombieWave();
    }
}

void Game::SpawnZombieWave() {
    // TODO: Implement zombie spawning
    // - Check total zombie count
    // - Find valid spawn locations
    // - Create zombie entities
    // - Register with entity manager
}

} // namespace Vehement
