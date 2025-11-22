#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include <variant>
#include <expected>
#include <span>
#include <chrono>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "GameConfig.hpp"

// Forward declarations for Nova3D engine
namespace Nova {
    class Engine;
    class Scene;
    class InputManager;
}

namespace Vehement {

// Forward declarations
class World;
class EntityManager;
class Player;
class FirebaseConnection;
class UIManager;
class CombatSystem;
class NetworkManager;

/**
 * @brief Game state enumeration
 */
enum class GameState : uint8_t {
    Initializing,   ///< Game is starting up
    MainMenu,       ///< Main menu screen
    Loading,        ///< Loading level/assets
    Playing,        ///< Active gameplay
    Paused,         ///< Game paused
    GameOver,       ///< Player died/game ended
    Editor,         ///< Level editor mode
    Connecting,     ///< Connecting to multiplayer
    Disconnected    ///< Lost connection
};

/**
 * @brief GPS location data structure
 */
struct GPSLocation {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    float accuracy = 0.0f;
    std::chrono::system_clock::time_point timestamp;

    [[nodiscard]] bool isValid() const noexcept {
        return accuracy > 0.0f && accuracy < Config::GPS::AccuracyThresholdMeters;
    }

    [[nodiscard]] glm::vec2 toWorldPosition() const noexcept {
        return {
            static_cast<float>(longitude / Config::GPS::DegreesLonPerTile * Config::World::TileSize),
            static_cast<float>(latitude / Config::GPS::DegreesLatPerTile * Config::World::TileSize)
        };
    }
};

/**
 * @brief Game initialization parameters
 */
struct GameInitParams {
    std::string configPath = "config/game.json";
    std::string levelPath = "";
    bool enableMultiplayer = false;
    bool enableGPS = false;
    bool startInEditor = false;
    std::optional<std::string> playerName;
    std::optional<std::string> authToken;
};

/**
 * @brief Error types for game operations
 */
enum class GameError {
    None,
    InitializationFailed,
    LoadFailed,
    NetworkError,
    AuthenticationFailed,
    InvalidState,
    AssetNotFound,
    SaveFailed
};

/**
 * @brief Core game class that manages all game systems
 *
 * This class is responsible for:
 * - Game state management (Menu, Playing, Paused, Editor)
 * - World and level management
 * - Entity management (players, zombies, NPCs)
 * - Firebase connection and multiplayer
 * - GPS location handling for AR features
 * - Coordinating all game subsystems
 */
class Game {
public:
    /**
     * @brief Construct a new Game instance
     * @param engine Reference to the Nova3D engine
     */
    explicit Game(Nova::Engine& engine);

    /**
     * @brief Destructor - cleans up all game resources
     */
    ~Game();

    // Non-copyable, non-movable (manages unique resources)
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    // =========================================================================
    // Initialization and Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the game with given parameters
     * @param params Initialization parameters
     * @return Expected success or error
     */
    [[nodiscard]] std::expected<void, GameError> Initialize(const GameInitParams& params = {});

    /**
     * @brief Shutdown the game and release all resources
     */
    void Shutdown();

    /**
     * @brief Main game update function called each frame
     * @param deltaTime Time since last frame in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Render the game
     */
    void Render();

    /**
     * @brief Render ImGui debug interface
     */
    void RenderImGui();

    // =========================================================================
    // State Management
    // =========================================================================

    /**
     * @brief Get current game state
     */
    [[nodiscard]] GameState GetState() const noexcept { return m_state; }

    /**
     * @brief Get state as string for debugging
     */
    [[nodiscard]] std::string_view GetStateString() const noexcept;

    /**
     * @brief Transition to a new game state
     * @param newState Target state
     * @return true if transition was valid and successful
     */
    bool TransitionTo(GameState newState);

    /**
     * @brief Check if game is in a playable state
     */
    [[nodiscard]] bool IsPlaying() const noexcept {
        return m_state == GameState::Playing;
    }

    /**
     * @brief Check if game is paused
     */
    [[nodiscard]] bool IsPaused() const noexcept {
        return m_state == GameState::Paused;
    }

    /**
     * @brief Pause the game (if playing)
     */
    void Pause();

    /**
     * @brief Resume the game (if paused)
     */
    void Resume();

    /**
     * @brief Toggle pause state
     */
    void TogglePause();

    // =========================================================================
    // Level Management
    // =========================================================================

    /**
     * @brief Load a level by name or path
     * @param levelPath Path to level data file
     * @return Expected success or error
     */
    [[nodiscard]] std::expected<void, GameError> LoadLevel(std::string_view levelPath);

    /**
     * @brief Unload current level
     */
    void UnloadLevel();

    /**
     * @brief Start a new game
     */
    void NewGame();

    /**
     * @brief Save current game state
     * @param savePath Path to save file
     * @return Expected success or error
     */
    [[nodiscard]] std::expected<void, GameError> SaveGame(std::string_view savePath);

    /**
     * @brief Load a saved game
     * @param savePath Path to save file
     * @return Expected success or error
     */
    [[nodiscard]] std::expected<void, GameError> LoadGame(std::string_view savePath);

    // =========================================================================
    // Multiplayer and Network
    // =========================================================================

    /**
     * @brief Connect to Firebase backend
     * @param authToken Optional authentication token
     * @return Expected success or error
     */
    [[nodiscard]] std::expected<void, GameError> ConnectToFirebase(
        std::optional<std::string_view> authToken = std::nullopt);

    /**
     * @brief Disconnect from Firebase
     */
    void DisconnectFromFirebase();

    /**
     * @brief Check if connected to Firebase
     */
    [[nodiscard]] bool IsConnected() const noexcept;

    /**
     * @brief Host a new multiplayer match
     * @param matchName Name of the match
     * @param maxPlayers Maximum players allowed
     * @return Match ID if successful
     */
    [[nodiscard]] std::expected<std::string, GameError> HostMatch(
        std::string_view matchName,
        int32_t maxPlayers = Config::Firebase::MaxPlayersPerMatch);

    /**
     * @brief Join an existing multiplayer match
     * @param matchId ID of match to join
     * @return Expected success or error
     */
    [[nodiscard]] std::expected<void, GameError> JoinMatch(std::string_view matchId);

    /**
     * @brief Leave current multiplayer match
     */
    void LeaveMatch();

    // =========================================================================
    // GPS and Location
    // =========================================================================

    /**
     * @brief Enable GPS location tracking
     */
    void EnableGPS();

    /**
     * @brief Disable GPS location tracking
     */
    void DisableGPS();

    /**
     * @brief Check if GPS is enabled and active
     */
    [[nodiscard]] bool IsGPSEnabled() const noexcept { return m_gpsEnabled; }

    /**
     * @brief Get current GPS location
     * @return Current location or nullopt if unavailable
     */
    [[nodiscard]] std::optional<GPSLocation> GetCurrentLocation() const;

    /**
     * @brief Update GPS location (called by platform-specific code)
     * @param location New location data
     */
    void UpdateGPSLocation(const GPSLocation& location);

    // =========================================================================
    // Editor Mode
    // =========================================================================

    /**
     * @brief Enter level editor mode
     */
    void EnterEditor();

    /**
     * @brief Exit level editor mode
     */
    void ExitEditor();

    /**
     * @brief Check if in editor mode
     */
    [[nodiscard]] bool IsInEditor() const noexcept {
        return m_state == GameState::Editor;
    }

    // =========================================================================
    // Subsystem Access
    // =========================================================================

    [[nodiscard]] Nova::Engine& GetEngine() noexcept { return m_engine; }
    [[nodiscard]] const Nova::Engine& GetEngine() const noexcept { return m_engine; }

    [[nodiscard]] World* GetWorld() noexcept { return m_world.get(); }
    [[nodiscard]] const World* GetWorld() const noexcept { return m_world.get(); }

    [[nodiscard]] EntityManager* GetEntityManager() noexcept { return m_entityManager.get(); }
    [[nodiscard]] const EntityManager* GetEntityManager() const noexcept { return m_entityManager.get(); }

    [[nodiscard]] Player* GetLocalPlayer() noexcept { return m_localPlayer.get(); }
    [[nodiscard]] const Player* GetLocalPlayer() const noexcept { return m_localPlayer.get(); }

    [[nodiscard]] CombatSystem* GetCombatSystem() noexcept { return m_combatSystem.get(); }
    [[nodiscard]] const CombatSystem* GetCombatSystem() const noexcept { return m_combatSystem.get(); }

    [[nodiscard]] UIManager* GetUIManager() noexcept { return m_uiManager.get(); }
    [[nodiscard]] const UIManager* GetUIManager() const noexcept { return m_uiManager.get(); }

    // =========================================================================
    // Statistics and Debug
    // =========================================================================

    /**
     * @brief Get total play time in seconds
     */
    [[nodiscard]] float GetPlayTime() const noexcept { return m_totalPlayTime; }

    /**
     * @brief Get current wave number
     */
    [[nodiscard]] int32_t GetCurrentWave() const noexcept { return m_currentWave; }

    /**
     * @brief Get total zombies killed this session
     */
    [[nodiscard]] int32_t GetZombiesKilled() const noexcept { return m_zombiesKilled; }

private:
    // State management
    void OnEnterState(GameState state);
    void OnExitState(GameState state);
    bool IsValidTransition(GameState from, GameState to) const;

    // Update functions for each state
    void UpdateMainMenu(float deltaTime);
    void UpdateLoading(float deltaTime);
    void UpdatePlaying(float deltaTime);
    void UpdatePaused(float deltaTime);
    void UpdateGameOver(float deltaTime);
    void UpdateEditor(float deltaTime);

    // Input handling
    void ProcessInput(float deltaTime);
    void ProcessPlayingInput(float deltaTime);
    void ProcessMenuInput(float deltaTime);
    void ProcessEditorInput(float deltaTime);

    // Network synchronization
    void SyncWithFirebase(float deltaTime);
    void ProcessNetworkMessages();

    // Spawn system
    void UpdateSpawning(float deltaTime);
    void SpawnZombieWave();

    // Core references
    Nova::Engine& m_engine;

    // Game state
    GameState m_state = GameState::Initializing;
    GameState m_previousState = GameState::Initializing;
    bool m_initialized = false;

    // Subsystems (owned)
    std::unique_ptr<World> m_world;
    std::unique_ptr<EntityManager> m_entityManager;
    std::unique_ptr<Player> m_localPlayer;
    std::unique_ptr<CombatSystem> m_combatSystem;
    std::unique_ptr<UIManager> m_uiManager;
    std::unique_ptr<NetworkManager> m_networkManager;
    std::unique_ptr<FirebaseConnection> m_firebase;

    // GPS tracking
    bool m_gpsEnabled = false;
    std::optional<GPSLocation> m_currentLocation;
    float m_gpsSyncTimer = 0.0f;

    // Game session data
    float m_totalPlayTime = 0.0f;
    int32_t m_currentWave = 0;
    int32_t m_zombiesKilled = 0;
    float m_spawnTimer = 0.0f;
    float m_networkSyncTimer = 0.0f;

    // Loading state
    float m_loadProgress = 0.0f;
    std::string m_loadingMessage;

    // Configuration
    GameInitParams m_initParams;
    nlohmann::json m_gameConfig;
};

} // namespace Vehement
