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

    // Load level data from JSON file
    try {
        std::ifstream levelFile(std::string(levelPath));
        if (!levelFile.is_open()) {
            Nova::Logger::Error("[Vehement] Failed to open level file: {}", levelPath);
            TransitionTo(GameState::MainMenu);
            return std::unexpected(GameError::LoadFailed);
        }

        nlohmann::json levelData;
        levelFile >> levelData;
        m_loadProgress = 0.2f;

        // Load level metadata
        if (levelData.contains("name")) {
            Nova::Logger::Info("[Vehement] Level name: {}", levelData["name"].get<std::string>());
        }
        m_loadProgress = 0.4f;

        // Load world/map data
        if (levelData.contains("world") && m_world) {
            // m_world->LoadFromJson(levelData["world"]);
        }
        m_loadProgress = 0.6f;

        // Load entity spawn points and initial entities
        if (levelData.contains("entities") && m_entityManager) {
            // m_entityManager->LoadFromJson(levelData["entities"]);
        }
        m_loadProgress = 0.8f;

        // Load wave configuration if present
        if (levelData.contains("waves")) {
            m_currentWave = levelData.value("startWave", 1);
        }
        m_loadProgress = 1.0f;

    } catch (const std::exception& e) {
        Nova::Logger::Error("[Vehement] Failed to parse level file: {}", e.what());
        TransitionTo(GameState::MainMenu);
        return std::unexpected(GameError::LoadFailed);
    }

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

    // Save world state
    if (m_world) {
        // saveData["world"] = m_world->SerializeToJson();
        saveData["world"] = nlohmann::json::object();
    }

    // Save entity data
    if (m_entityManager) {
        // saveData["entities"] = m_entityManager->SerializeToJson();
        saveData["entities"] = nlohmann::json::array();
    }

    // Save player data
    if (m_localPlayer) {
        // saveData["player"] = m_localPlayer->SerializeToJson();
        saveData["player"] = nlohmann::json::object();
        saveData["player"]["health"] = Config::Player::MaxHealth;
        saveData["player"]["stamina"] = Config::Player::MaxStamina;
    }

    // Save GPS location if enabled
    if (m_gpsEnabled && m_currentLocation.has_value()) {
        saveData["gps"]["latitude"] = m_currentLocation->latitude;
        saveData["gps"]["longitude"] = m_currentLocation->longitude;
        saveData["gps"]["altitude"] = m_currentLocation->altitude;
    }

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

        // Load world state
        if (saveData.contains("world") && m_world) {
            // m_world->LoadFromJson(saveData["world"]);
        }

        // Load entity data
        if (saveData.contains("entities") && m_entityManager) {
            // m_entityManager->LoadFromJson(saveData["entities"]);
        }

        // Load player data
        if (saveData.contains("player") && m_localPlayer) {
            // m_localPlayer->LoadFromJson(saveData["player"]);
        }

        // Restore GPS location if saved
        if (saveData.contains("gps") && m_gpsEnabled) {
            GPSLocation location;
            location.latitude = saveData["gps"].value("latitude", 0.0);
            location.longitude = saveData["gps"].value("longitude", 0.0);
            location.altitude = saveData["gps"].value("altitude", 0.0);
            location.accuracy = Config::GPS::AccuracyThresholdMeters - 1.0f; // Mark as valid
            location.timestamp = std::chrono::system_clock::now();
            m_currentLocation = location;
        }

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

    // Initialize Firebase connection
    // Note: FirebaseConnection class would handle actual REST API calls
    // m_firebase = std::make_unique<FirebaseConnection>();

    // Validate configuration
    if (Config::Firebase::ApiKey == "YOUR_API_KEY_HERE") {
        Nova::Logger::Warn("[Vehement] Firebase API key not configured, running in offline mode");
        TransitionTo(GameState::MainMenu);
        return {};
    }

    // Attempt connection with optional auth token
    // if (authToken.has_value()) {
    //     auto result = m_firebase->ConnectWithAuth(
    //         Config::Firebase::ProjectId,
    //         Config::Firebase::ApiKey,
    //         authToken.value()
    //     );
    //     if (!result) {
    //         Nova::Logger::Error("[Vehement] Firebase authentication failed");
    //         TransitionTo(GameState::MainMenu);
    //         return std::unexpected(GameError::AuthenticationFailed);
    //     }
    // } else {
    //     auto result = m_firebase->ConnectAnonymous(
    //         Config::Firebase::ProjectId,
    //         Config::Firebase::ApiKey
    //     );
    //     if (!result) {
    //         Nova::Logger::Error("[Vehement] Firebase connection failed");
    //         TransitionTo(GameState::MainMenu);
    //         return std::unexpected(GameError::NetworkError);
    //     }
    // }

    // For now, Firebase integration is stubbed - running in offline mode
    Nova::Logger::Warn("[Vehement] Firebase not yet implemented, running in offline mode");
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

    // Validate parameters
    if (matchName.empty()) {
        Nova::Logger::Error("[Vehement] Match name cannot be empty");
        return std::unexpected(GameError::InvalidState);
    }

    if (maxPlayers < 1 || maxPlayers > Config::Firebase::MaxPlayersPerMatch) {
        Nova::Logger::Error("[Vehement] Invalid player count: {} (max: {})",
            maxPlayers, Config::Firebase::MaxPlayersPerMatch);
        return std::unexpected(GameError::InvalidState);
    }

    // Create match data structure
    // nlohmann::json matchData;
    // matchData["name"] = matchName;
    // matchData["maxPlayers"] = maxPlayers;
    // matchData["currentPlayers"] = 1;
    // matchData["hostId"] = m_firebase->GetUserId();
    // matchData["state"] = "lobby";
    // matchData["createdAt"] = std::chrono::system_clock::now().time_since_epoch().count();

    // Push to Firebase and get match ID
    // auto result = m_firebase->Push(Config::Firebase::MatchesPath, matchData);
    // if (!result) {
    //     return std::unexpected(GameError::NetworkError);
    // }
    //
    // std::string matchId = result.value();
    // Nova::Logger::Info("[Vehement] Match created with ID: {}", matchId);
    // return matchId;

    // Stub implementation - Firebase not yet integrated
    Nova::Logger::Warn("[Vehement] Match hosting not yet implemented");
    return std::unexpected(GameError::NetworkError);
}

std::expected<void, GameError> Game::JoinMatch(std::string_view matchId) {
    Nova::Logger::Info("[Vehement] Joining match: {}", matchId);

    if (!IsConnected()) {
        return std::unexpected(GameError::NetworkError);
    }

    // Validate match ID
    if (matchId.empty()) {
        Nova::Logger::Error("[Vehement] Match ID cannot be empty");
        return std::unexpected(GameError::InvalidState);
    }

    // Fetch match data from Firebase
    // std::string matchPath = std::string(Config::Firebase::MatchesPath) + "/" + std::string(matchId);
    // auto matchResult = m_firebase->Get(matchPath);
    // if (!matchResult) {
    //     Nova::Logger::Error("[Vehement] Match not found: {}", matchId);
    //     return std::unexpected(GameError::AssetNotFound);
    // }

    // Validate match state
    // nlohmann::json matchData = matchResult.value();
    // if (matchData["state"] != "lobby") {
    //     Nova::Logger::Error("[Vehement] Match already in progress");
    //     return std::unexpected(GameError::InvalidState);
    // }

    // Check player count
    // int currentPlayers = matchData.value("currentPlayers", 0);
    // int maxPlayers = matchData.value("maxPlayers", Config::Firebase::MaxPlayersPerMatch);
    // if (currentPlayers >= maxPlayers) {
    //     Nova::Logger::Error("[Vehement] Match is full");
    //     return std::unexpected(GameError::InvalidState);
    // }

    // Add player to match
    // matchData["currentPlayers"] = currentPlayers + 1;
    // m_firebase->Set(matchPath, matchData);

    // Subscribe to match updates
    // m_firebase->Subscribe(matchPath, [this](const nlohmann::json& update) {
    //     ProcessMatchUpdate(update);
    // });

    // Nova::Logger::Info("[Vehement] Successfully joined match: {}", matchId);
    // TransitionTo(GameState::Playing);
    // return {};

    // Stub implementation - Firebase not yet integrated
    Nova::Logger::Warn("[Vehement] Match joining not yet implemented");
    return std::unexpected(GameError::NetworkError);
}

void Game::LeaveMatch() {
    Nova::Logger::Info("[Vehement] Leaving current match");

    if (!IsConnected()) {
        Nova::Logger::Warn("[Vehement] Not connected to any match");
        return;
    }

    // Notify other players and update match state
    // if (m_firebase) {
    //     // Get current match path from stored match ID
    //     // std::string matchPath = std::string(Config::Firebase::MatchesPath) + "/" + m_currentMatchId;
    //
    //     // Decrement player count
    //     // auto matchResult = m_firebase->Get(matchPath);
    //     // if (matchResult) {
    //     //     nlohmann::json matchData = matchResult.value();
    //     //     int currentPlayers = matchData.value("currentPlayers", 1);
    //     //     matchData["currentPlayers"] = std::max(0, currentPlayers - 1);
    //     //
    //     //     // If host is leaving and players remain, transfer host
    //     //     // If no players remain, delete match
    //     //     if (matchData["currentPlayers"] == 0) {
    //     //         m_firebase->Delete(matchPath);
    //     //     } else {
    //     //         m_firebase->Set(matchPath, matchData);
    //     //     }
    //     // }
    //
    //     // Unsubscribe from match updates
    //     // m_firebase->Unsubscribe(matchPath);
    //
    //     // Remove player data from match
    //     // std::string playerPath = matchPath + "/players/" + m_firebase->GetUserId();
    //     // m_firebase->Delete(playerPath);
    // }

    // Transition back to main menu
    if (m_state == GameState::Playing || m_state == GameState::Paused) {
        TransitionTo(GameState::MainMenu);
    }

    Nova::Logger::Info("[Vehement] Left match successfully");
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

    // Process incoming messages from Firebase
    // This would be called by the Firebase SDK's callback system
    // or polled from a message queue

    // Process other player position updates
    // if (m_firebase->HasPendingMessages()) {
    //     auto messages = m_firebase->GetPendingMessages();
    //     for (const auto& msg : messages) {
    //         switch (msg.type) {
    //             case NetworkMessageType::PlayerPosition: {
    //                 // Update remote player positions
    //                 // std::string playerId = msg.data["playerId"];
    //                 // glm::vec3 position = {
    //                 //     msg.data["x"].get<float>(),
    //                 //     msg.data["y"].get<float>(),
    //                 //     msg.data["z"].get<float>()
    //                 // };
    //                 // if (m_entityManager) {
    //                 //     m_entityManager->UpdateRemotePlayer(playerId, position);
    //                 // }
    //                 break;
    //             }
    //             case NetworkMessageType::WorldStateChange: {
    //                 // Handle world state changes (e.g., door opened, item picked up)
    //                 // if (m_world) {
    //                 //     m_world->ApplyStateChange(msg.data);
    //                 // }
    //                 break;
    //             }
    //             case NetworkMessageType::EntitySpawn: {
    //                 // Spawn networked entity
    //                 // if (m_entityManager) {
    //                 //     m_entityManager->SpawnNetworkedEntity(msg.data);
    //                 // }
    //                 break;
    //             }
    //             case NetworkMessageType::EntityDeath: {
    //                 // Remove networked entity
    //                 // if (m_entityManager) {
    //                 //     m_entityManager->RemoveEntity(msg.data["entityId"]);
    //                 // }
    //                 break;
    //             }
    //             case NetworkMessageType::MatchEvent: {
    //                 // Handle match events (wave start, game over, etc.)
    //                 // ProcessMatchEvent(msg.data);
    //                 break;
    //             }
    //             case NetworkMessageType::PlayerJoined: {
    //                 Nova::Logger::Info("[Vehement] Player joined: {}",
    //                     msg.data.value("playerName", "Unknown"));
    //                 break;
    //             }
    //             case NetworkMessageType::PlayerLeft: {
    //                 Nova::Logger::Info("[Vehement] Player left: {}",
    //                     msg.data.value("playerName", "Unknown"));
    //                 // Remove player entity
    //                 // if (m_entityManager) {
    //                 //     m_entityManager->RemoveRemotePlayer(msg.data["playerId"]);
    //                 // }
    //                 break;
    //             }
    //             default:
    //                 Nova::Logger::Warn("[Vehement] Unknown network message type");
    //                 break;
    //         }
    //     }
    // }
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
    // Check if entity manager is available
    if (!m_entityManager) {
        return;
    }

    // Check total zombie count limit
    // int currentZombieCount = m_entityManager ? m_entityManager->GetZombieCount() : 0;
    int currentZombieCount = 0; // Placeholder until EntityManager is implemented
    if (currentZombieCount >= Config::Zombie::MaxTotalZombies) {
        return;
    }

    // Calculate zombies to spawn based on current wave
    int zombiesToSpawn = std::min(
        static_cast<int>(m_currentWave + 2), // Base + wave number
        Config::Zombie::MaxTotalZombies - currentZombieCount
    );

    // Get player position for spawn distance calculation
    glm::vec3 playerPos = glm::vec3(0.0f);
    if (m_localPlayer) {
        // playerPos = m_localPlayer->GetPosition();
    } else if (m_currentLocation.has_value()) {
        auto worldPos = m_currentLocation->toWorldPosition();
        playerPos = glm::vec3(worldPos.x, 0.0f, worldPos.y);
    }

    // Spawn zombies at valid locations
    for (int i = 0; i < zombiesToSpawn; ++i) {
        // Find valid spawn location (outside player view but within chase range)
        // glm::vec3 spawnPos = FindValidSpawnLocation(playerPos);

        // Determine zombie type based on wave
        // ZombieType type = DetermineZombieType(m_currentWave);

        // Calculate health based on type and wave scaling
        // float health = GetZombieBaseHealth(type) * (1.0f + m_currentWave * 0.1f);

        // Create zombie entity
        // if (m_entityManager) {
        //     auto zombie = m_entityManager->CreateZombie(type, spawnPos, health);
        //     if (zombie) {
        //         // Set zombie target to player
        //         zombie->SetTarget(m_localPlayer.get());
        //     }
        // }
    }

    // Log spawn event
    if (zombiesToSpawn > 0) {
        Nova::Logger::Debug("[Vehement] Spawned {} zombies for wave {}", zombiesToSpawn, m_currentWave);
    }
}

} // namespace Vehement
