#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>

namespace Engine { namespace UI { class RuntimeUIManager; class UIWindow; } }

namespace Game {
namespace UI {
namespace Menu {

/**
 * @brief Online multiplayer menu state
 */
enum class OnlineMenuState {
    Main,               // Main online menu with options
    HostLocal,          // Host local server configuration
    JoinLocal,          // Join local server (IP entry)
    ServerBrowser,      // Browse available servers (local + Firebase)
    FirebaseConnect,    // Connecting to Firebase
    Lobby,              // In lobby waiting for players
    Connecting,         // Connecting to server
    Error,              // Error state
    None
};

/**
 * @brief Connection type
 */
enum class ConnectionType {
    LocalLAN,           // Local LAN/IP connection
    FirebaseGlobal,     // Firebase global server
    DirectIP            // Direct IP connection
};

/**
 * @brief Server visibility
 */
enum class ServerVisibility {
    Public,             // Listed in server browser
    Private,            // Requires invite code
    FriendsOnly         // Only visible to friends
};

/**
 * @brief Server info for browser
 */
struct ServerInfo {
    std::string id;
    std::string name;
    std::string hostName;
    std::string mapName;
    std::string gameMode;
    ConnectionType connectionType;

    // Network info
    std::string ipAddress;
    uint16_t port = 0;
    std::string firebaseId;
    std::string region;

    // Status
    int currentPlayers = 0;
    int maxPlayers = 8;
    int ping = 0;
    bool hasPassword = false;
    bool isLAN = false;
    bool isFull = false;

    // Metadata
    std::string version;
    std::string createdAt;
    bool isFavorite = false;
};

/**
 * @brief Local server host configuration
 */
struct LocalServerConfig {
    std::string serverName;
    std::string password;
    std::string mapId;
    int maxPlayers = 8;
    ServerVisibility visibility = ServerVisibility::Public;
    bool enableFirebaseRelay = true;  // Allow Firebase players to join
    bool enableLAN = true;             // Broadcast on LAN
    uint16_t port = 7777;

    // Game settings
    std::string gameMode;
    int gameSpeed = 1;
    bool allowSpectators = true;
};

/**
 * @brief Player in lobby
 */
struct LobbyPlayerInfo {
    std::string id;
    std::string name;
    std::string avatarUrl;
    ConnectionType connectionType;
    int ping = 0;
    bool isHost = false;
    bool isReady = false;
    int team = 0;
    std::string raceId;
};

/**
 * @brief Current lobby state
 */
struct LobbyState {
    std::string lobbyId;
    std::string serverName;
    std::string hostId;
    std::string inviteCode;
    LocalServerConfig config;
    std::vector<LobbyPlayerInfo> players;
    bool gameStarting = false;
    int countdownSeconds = 0;
    std::string mapImageUrl;
};

/**
 * @brief Network statistics
 */
struct NetworkStats {
    int ping = 0;
    float packetLoss = 0.0f;
    float bandwidth = 0.0f;
    std::string connectionQuality;  // "Excellent", "Good", "Fair", "Poor"
    bool usingFirebaseRelay = false;
};

/**
 * @brief Quick join preferences
 */
struct QuickJoinPreferences {
    std::string preferredRegion;
    std::string preferredGameMode;
    int maxPing = 150;
    bool allowPassword = false;
    bool preferNotFull = true;
};

/**
 * @brief Online Multiplayer Menu Manager
 *
 * Provides comprehensive online multiplayer functionality:
 * - Host local servers (LAN + IP sharing)
 * - Join local servers (IP entry, LAN discovery)
 * - Firebase global server connection
 * - Server browser with filtering
 * - Lobby system with player management
 * - Network quality monitoring
 * - Friend invites and party system
 */
class OnlineMultiplayerMenu {
public:
    OnlineMultiplayerMenu();
    ~OnlineMultiplayerMenu();

    /**
     * @brief Initialize the online menu
     */
    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Update (call every frame)
     */
    void Update(float deltaTime);

    // State Management

    /**
     * @brief Get current menu state
     */
    OnlineMenuState GetCurrentState() const { return m_currentState; }

    /**
     * @brief Navigate to a menu state
     */
    void NavigateTo(OnlineMenuState state);

    /**
     * @brief Navigate back to previous state
     */
    void NavigateBack();

    /**
     * @brief Return to main online menu
     */
    void ReturnToMainMenu();

    // Visibility

    /**
     * @brief Show the online menu
     */
    void Show();

    /**
     * @brief Hide the online menu
     */
    void Hide();

    /**
     * @brief Check if visible
     */
    bool IsVisible() const { return m_visible; }

    // Local Server Hosting

    /**
     * @brief Show host local server dialog
     */
    void ShowHostLocalServer();

    /**
     * @brief Start hosting a local server
     */
    void HostLocalServer(const LocalServerConfig& config);

    /**
     * @brief Stop hosting local server
     */
    void StopHosting();

    /**
     * @brief Check if currently hosting
     */
    bool IsHosting() const { return m_isHosting; }

    /**
     * @brief Get local server IP address
     */
    std::string GetLocalIPAddress() const;

    /**
     * @brief Get server invite code
     */
    std::string GetServerInviteCode() const { return m_serverInviteCode; }

    /**
     * @brief Copy invite code to clipboard
     */
    void CopyInviteCodeToClipboard();

    // Local Server Joining

    /**
     * @brief Show join local server dialog
     */
    void ShowJoinLocalServer();

    /**
     * @brief Join server by IP address
     */
    void JoinByIPAddress(const std::string& ipAddress, uint16_t port = 7777, const std::string& password = "");

    /**
     * @brief Join server by invite code
     */
    void JoinByInviteCode(const std::string& inviteCode);

    /**
     * @brief Refresh LAN server list
     */
    void RefreshLANServers();

    /**
     * @brief Get discovered LAN servers
     */
    const std::vector<ServerInfo>& GetLANServers() const { return m_lanServers; }

    // Firebase Global Servers

    /**
     * @brief Connect to Firebase
     */
    void ConnectToFirebase();

    /**
     * @brief Disconnect from Firebase
     */
    void DisconnectFromFirebase();

    /**
     * @brief Check if connected to Firebase
     */
    bool IsConnectedToFirebase() const { return m_connectedToFirebase; }

    /**
     * @brief Join a Firebase global server
     */
    void JoinFirebaseServer(const std::string& serverId);

    /**
     * @brief Create Firebase global server
     */
    void CreateFirebaseServer(const LocalServerConfig& config);

    // Server Browser

    /**
     * @brief Show server browser
     */
    void ShowServerBrowser();

    /**
     * @brief Refresh server list
     */
    void RefreshServerList();

    /**
     * @brief Get all available servers (LAN + Firebase)
     */
    const std::vector<ServerInfo>& GetAvailableServers() const { return m_availableServers; }

    /**
     * @brief Filter servers
     */
    void FilterServers(const std::string& searchText, bool hideFullServers = false, bool hideLocked = false);

    /**
     * @brief Sort servers by criteria
     */
    void SortServers(const std::string& criteria); // "ping", "players", "name"

    /**
     * @brief Add server to favorites
     */
    void AddToFavorites(const std::string& serverId);

    /**
     * @brief Remove server from favorites
     */
    void RemoveFromFavorites(const std::string& serverId);

    /**
     * @brief Get favorite servers
     */
    std::vector<ServerInfo> GetFavoriteServers() const;

    /**
     * @brief Quick join - auto-find best server
     */
    void QuickJoin(const QuickJoinPreferences& prefs = {});

    // Lobby Management

    /**
     * @brief Check if in lobby
     */
    bool IsInLobby() const { return m_inLobby; }

    /**
     * @brief Get current lobby state
     */
    const LobbyState* GetCurrentLobby() const { return m_inLobby ? &m_currentLobby : nullptr; }

    /**
     * @brief Leave current lobby
     */
    void LeaveLobby();

    /**
     * @brief Set ready state
     */
    void SetReady(bool ready);

    /**
     * @brief Toggle ready
     */
    void ToggleReady();

    /**
     * @brief Check if ready
     */
    bool IsReady() const { return m_isReady; }

    /**
     * @brief Change team
     */
    void ChangeTeam(int team);

    /**
     * @brief Change race
     */
    void ChangeRace(const std::string& raceId);

    // Lobby Actions (Host Only)

    /**
     * @brief Update lobby settings
     */
    void UpdateLobbySettings(const LocalServerConfig& config);

    /**
     * @brief Kick player from lobby
     */
    void KickPlayer(const std::string& playerId);

    /**
     * @brief Start game
     */
    void StartGame();

    // Network Quality

    /**
     * @brief Get network statistics
     */
    const NetworkStats& GetNetworkStats() const { return m_networkStats; }

    /**
     * @brief Test connection to server
     */
    void TestServerConnection(const std::string& serverId);

    /**
     * @brief Get estimated ping to server
     */
    int GetPingToServer(const std::string& serverId) const;

    // Friend & Party System

    /**
     * @brief Invite friend to lobby
     */
    void InviteFriend(const std::string& friendId);

    /**
     * @brief Show invite friends dialog
     */
    void ShowInviteFriends();

    /**
     * @brief Accept friend invite
     */
    void AcceptInvite(const std::string& inviteId);

    /**
     * @brief Decline friend invite
     */
    void DeclineInvite(const std::string& inviteId);

    // Recent Servers

    /**
     * @brief Get recently joined servers
     */
    std::vector<ServerInfo> GetRecentServers() const;

    /**
     * @brief Clear recent servers
     */
    void ClearRecentServers();

    // Error Handling

    /**
     * @brief Show error message
     */
    void ShowError(const std::string& message);

    /**
     * @brief Clear error
     */
    void ClearError();

    /**
     * @brief Get last error message
     */
    std::string GetLastError() const { return m_lastError; }

    // Callbacks

    void SetOnServerJoined(std::function<void(const ServerInfo&)> callback);
    void SetOnLobbyUpdate(std::function<void(const LobbyState&)> callback);
    void SetOnGameStart(std::function<void()> callback);
    void SetOnConnectionError(std::function<void(const std::string&)> callback);
    void SetOnFirebaseConnected(std::function<void()> callback);
    void SetOnServerListUpdate(std::function<void(const std::vector<ServerInfo>&)> callback);

private:
    // Internal helpers
    void SetupEventHandlers();
    void UpdateLobbyUI();
    void UpdateServerBrowser();
    void UpdateNetworkStats();
    void DiscoverLANServers();
    void QueryFirebaseServers();
    void SyncWithFirebase();
    void HandleConnectionResult(bool success, const std::string& error = "");
    void AddToRecentServers(const ServerInfo& server);
    std::string GenerateInviteCode();
    std::string GetConnectionQualityString(int ping, float packetLoss);

    // UI state
    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;
    std::unordered_map<OnlineMenuState, Engine::UI::UIWindow*> m_stateWindows;
    OnlineMenuState m_currentState = OnlineMenuState::None;
    std::vector<OnlineMenuState> m_stateHistory;
    bool m_visible = false;

    // Hosting state
    bool m_isHosting = false;
    LocalServerConfig m_hostConfig;
    std::string m_serverInviteCode;
    std::string m_localIPAddress;

    // Connection state
    bool m_connectedToFirebase = false;
    std::string m_firebaseUserId;
    ConnectionType m_currentConnectionType = ConnectionType::LocalLAN;

    // Server lists
    std::vector<ServerInfo> m_lanServers;
    std::vector<ServerInfo> m_firebaseServers;
    std::vector<ServerInfo> m_availableServers;
    std::vector<ServerInfo> m_recentServers;
    std::vector<std::string> m_favoriteServerIds;
    float m_serverRefreshTimer = 0.0f;
    float m_lanDiscoveryTimer = 0.0f;

    // Lobby state
    bool m_inLobby = false;
    bool m_isReady = false;
    LobbyState m_currentLobby;
    std::string m_localPlayerId;

    // Network stats
    NetworkStats m_networkStats;
    float m_statsUpdateTimer = 0.0f;

    // Error state
    std::string m_lastError;
    std::chrono::steady_clock::time_point m_errorTime;

    // Constants
    static constexpr float SERVER_REFRESH_INTERVAL = 5.0f;  // Refresh every 5 seconds
    static constexpr float LAN_DISCOVERY_INTERVAL = 2.0f;   // LAN discovery every 2 seconds
    static constexpr float STATS_UPDATE_INTERVAL = 1.0f;    // Update stats every second
    static constexpr int MAX_RECENT_SERVERS = 10;

    // Callbacks
    std::function<void(const ServerInfo&)> m_onServerJoined;
    std::function<void(const LobbyState&)> m_onLobbyUpdate;
    std::function<void()> m_onGameStart;
    std::function<void(const std::string&)> m_onConnectionError;
    std::function<void()> m_onFirebaseConnected;
    std::function<void(const std::vector<ServerInfo>&)> m_onServerListUpdate;
};

} // namespace Menu
} // namespace UI
} // namespace Game
