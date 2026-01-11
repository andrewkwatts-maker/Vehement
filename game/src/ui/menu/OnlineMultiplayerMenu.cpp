#include "OnlineMultiplayerMenu.hpp"

#include "ui/runtime/RuntimeUIManager.hpp"
#include "ui/runtime/UIWindow.hpp"
#include "network/FirebaseManager.hpp"
#include "network/Matchmaking.hpp"
#include "network/MultiplayerSync.hpp"
#include "network/TownServer.hpp"
#include "network/replication/NetworkTransport.hpp"
#include "network/replication/ReplicationManager.hpp"

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace Game {
namespace UI {
namespace Menu {

OnlineMultiplayerMenu::OnlineMultiplayerMenu() = default;

OnlineMultiplayerMenu::~OnlineMultiplayerMenu() {
    Shutdown();
}

bool OnlineMultiplayerMenu::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) {
        spdlog::error("OnlineMultiplayerMenu: Invalid UI manager");
        return false;
    }

    m_uiManager = uiManager;

    // Generate local player ID
    m_localPlayerId = GenerateInviteCode();

    // Get local IP address
    m_localIPAddress = GetLocalIPAddress();

    spdlog::info("OnlineMultiplayerMenu initialized. Local IP: {}", m_localIPAddress);

    SetupEventHandlers();
    return true;
}

void OnlineMultiplayerMenu::Shutdown() {
    if (m_isHosting) {
        StopHosting();
    }

    if (m_inLobby) {
        LeaveLobby();
    }

    if (m_connectedToFirebase) {
        DisconnectFromFirebase();
    }

    m_uiManager = nullptr;
}

void OnlineMultiplayerMenu::Update(float deltaTime) {
    if (!m_visible) return;

    // Update server refresh timer
    m_serverRefreshTimer += deltaTime;
    if (m_serverRefreshTimer >= SERVER_REFRESH_INTERVAL) {
        m_serverRefreshTimer = 0.0f;
        if (m_currentState == OnlineMenuState::ServerBrowser) {
            RefreshServerList();
        }
    }

    // Update LAN discovery timer
    m_lanDiscoveryTimer += deltaTime;
    if (m_lanDiscoveryTimer >= LAN_DISCOVERY_INTERVAL) {
        m_lanDiscoveryTimer = 0.0f;
        DiscoverLANServers();
    }

    // Update network stats
    m_statsUpdateTimer += deltaTime;
    if (m_statsUpdateTimer >= STATS_UPDATE_INTERVAL) {
        m_statsUpdateTimer = 0.0f;
        UpdateNetworkStats();
    }

    // Update UI based on current state
    switch (m_currentState) {
        case OnlineMenuState::Lobby:
            UpdateLobbyUI();
            break;
        case OnlineMenuState::ServerBrowser:
            UpdateServerBrowser();
            break;
        case OnlineMenuState::FirebaseConnect:
            // Check connection status
            if (Vehement::FirebaseManager::Instance().IsSignedIn()) {
                m_connectedToFirebase = true;
                NavigateTo(OnlineMenuState::ServerBrowser);
                if (m_onFirebaseConnected) {
                    m_onFirebaseConnected();
                }
            }
            break;
        default:
            break;
    }
}

void OnlineMultiplayerMenu::SetupEventHandlers() {
    // Setup callbacks for networking systems
    auto& matchmaking = Vehement::Matchmaking::Instance();

    matchmaking.OnPlayerJoined([this](const Vehement::PlayerInfo& player) {
        spdlog::info("Player joined: {}", player.displayName);

        // Add to lobby
        LobbyPlayerInfo lobbyPlayer;
        lobbyPlayer.id = player.oderId;
        lobbyPlayer.name = player.displayName;
        lobbyPlayer.isHost = player.isHost;
        lobbyPlayer.ping = 0;
        lobbyPlayer.connectionType = m_connectedToFirebase ? ConnectionType::FirebaseGlobal : ConnectionType::LocalLAN;

        m_currentLobby.players.push_back(lobbyPlayer);

        if (m_onLobbyUpdate) {
            m_onLobbyUpdate(m_currentLobby);
        }
    });

    matchmaking.OnPlayerLeft([this](const std::string& oderId) {
        spdlog::info("Player left: {}", oderId);

        // Remove from lobby
        auto it = std::remove_if(m_currentLobby.players.begin(), m_currentLobby.players.end(),
            [&oderId](const LobbyPlayerInfo& player) { return player.id == oderId; });

        if (it != m_currentLobby.players.end()) {
            m_currentLobby.players.erase(it, m_currentLobby.players.end());

            if (m_onLobbyUpdate) {
                m_onLobbyUpdate(m_currentLobby);
            }
        }
    });
}

// State Management

void OnlineMultiplayerMenu::NavigateTo(OnlineMenuState state) {
    if (m_currentState == state) return;

    m_stateHistory.push_back(m_currentState);
    m_currentState = state;

    spdlog::debug("OnlineMultiplayerMenu: Navigating to state {}", static_cast<int>(state));
}

void OnlineMultiplayerMenu::NavigateBack() {
    if (m_stateHistory.empty()) {
        ReturnToMainMenu();
        return;
    }

    m_currentState = m_stateHistory.back();
    m_stateHistory.pop_back();
}

void OnlineMultiplayerMenu::ReturnToMainMenu() {
    m_stateHistory.clear();
    m_currentState = OnlineMenuState::Main;
}

void OnlineMultiplayerMenu::Show() {
    m_visible = true;
    if (m_currentState == OnlineMenuState::None) {
        m_currentState = OnlineMenuState::Main;
    }
}

void OnlineMultiplayerMenu::Hide() {
    m_visible = false;
}

// Local Server Hosting

void OnlineMultiplayerMenu::ShowHostLocalServer() {
    NavigateTo(OnlineMenuState::HostLocal);
}

void OnlineMultiplayerMenu::HostLocalServer(const LocalServerConfig& config) {
    spdlog::info("Hosting local server: {}", config.serverName);

    m_hostConfig = config;
    m_isHosting = true;

    // Generate invite code
    m_serverInviteCode = GenerateInviteCode();

    // Initialize network transport
    auto transport = Network::Replication::NetworkTransport::create();
    if (!transport->initialize()) {
        ShowError("Failed to initialize network transport");
        m_isHosting = false;
        return;
    }

    // Setup replication manager
    auto& replicationMgr = Network::Replication::ReplicationManager::getInstance();
    if (!replicationMgr.initialize(transport)) {
        ShowError("Failed to initialize replication manager");
        m_isHosting = false;
        return;
    }

    // Enable Firebase relay if configured
    if (config.enableFirebaseRelay && m_connectedToFirebase) {
        transport->enableFirebaseRelay(true);
        CreateFirebaseServer(config);
    }

    // Setup lobby
    m_inLobby = true;
    m_currentLobby = LobbyState();
    m_currentLobby.serverName = config.serverName;
    m_currentLobby.lobbyId = m_serverInviteCode;
    m_currentLobby.inviteCode = m_serverInviteCode;
    m_currentLobby.hostId = m_localPlayerId;
    m_currentLobby.config = config;

    // Add host as first player
    LobbyPlayerInfo hostPlayer;
    hostPlayer.id = m_localPlayerId;
    hostPlayer.name = "Host"; // Should get from profile
    hostPlayer.isHost = true;
    hostPlayer.isReady = true;
    hostPlayer.connectionType = ConnectionType::LocalLAN;
    m_currentLobby.players.push_back(hostPlayer);

    NavigateTo(OnlineMenuState::Lobby);

    spdlog::info("Local server hosted. IP: {} Port: {} Code: {}",
                 m_localIPAddress, config.port, m_serverInviteCode);
}

void OnlineMultiplayerMenu::StopHosting() {
    spdlog::info("Stopping local server hosting");

    m_isHosting = false;
    m_serverInviteCode.clear();

    // Cleanup networking
    auto& replicationMgr = Network::Replication::ReplicationManager::getInstance();
    replicationMgr.shutdown();

    if (m_inLobby) {
        LeaveLobby();
    }
}

std::string OnlineMultiplayerMenu::GetLocalIPAddress() const {
#ifdef _WIN32
    // Windows implementation
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return "127.0.0.1";
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        WSACleanup();
        return "127.0.0.1";
    }

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
        WSACleanup();
        return "127.0.0.1";
    }

    std::string ipAddress = "127.0.0.1";
    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)ptr->ai_addr;
        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipstr, sizeof(ipstr));

        // Skip loopback
        if (std::string(ipstr) != "127.0.0.1") {
            ipAddress = ipstr;
            break;
        }
    }

    freeaddrinfo(result);
    WSACleanup();

    return ipAddress;
#else
    // Unix/Linux/Mac implementation
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;
    void* tmpAddrPtr = nullptr;
    std::string ipAddress = "127.0.0.1";

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

            // Skip loopback
            if (std::string(addressBuffer) != "127.0.0.1") {
                ipAddress = addressBuffer;
                break;
            }
        }
    }

    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }

    return ipAddress;
#endif
}

void OnlineMultiplayerMenu::CopyInviteCodeToClipboard() {
    // Platform-specific clipboard implementation would go here
    spdlog::info("Invite code copied to clipboard: {}", m_serverInviteCode);
}

// Local Server Joining

void OnlineMultiplayerMenu::ShowJoinLocalServer() {
    NavigateTo(OnlineMenuState::JoinLocal);
    RefreshLANServers();
}

void OnlineMultiplayerMenu::JoinByIPAddress(const std::string& ipAddress, uint16_t port, const std::string& password) {
    spdlog::info("Joining server at {}:{}", ipAddress, port);

    NavigateTo(OnlineMenuState::Connecting);

    // Initialize network transport
    auto transport = Network::Replication::NetworkTransport::create();
    if (!transport->initialize()) {
        HandleConnectionResult(false, "Failed to initialize network transport");
        return;
    }

    // Connect to server
    transport->connect(ipAddress, port);

    // Wait for connection (in real implementation, this would be async)
    // For now, simulate connection
    bool connected = true; // Would check actual connection status

    if (connected) {
        m_inLobby = true;
        m_currentConnectionType = ConnectionType::DirectIP;

        // Create temporary lobby state
        m_currentLobby = LobbyState();
        m_currentLobby.serverName = ipAddress + ":" + std::to_string(port);

        NavigateTo(OnlineMenuState::Lobby);
        HandleConnectionResult(true);
    } else {
        HandleConnectionResult(false, "Connection timed out");
    }
}

void OnlineMultiplayerMenu::JoinByInviteCode(const std::string& inviteCode) {
    spdlog::info("Joining server with invite code: {}", inviteCode);

    // Search for server with this invite code
    // First check LAN servers
    for (const auto& server : m_lanServers) {
        if (server.id == inviteCode) {
            JoinByIPAddress(server.ipAddress, server.port);
            return;
        }
    }

    // Then check Firebase servers
    if (m_connectedToFirebase) {
        for (const auto& server : m_firebaseServers) {
            if (server.id == inviteCode || server.firebaseId == inviteCode) {
                JoinFirebaseServer(server.id);
                return;
            }
        }
    }

    ShowError("Server not found with invite code: " + inviteCode);
}

void OnlineMultiplayerMenu::RefreshLANServers() {
    spdlog::debug("Refreshing LAN servers");
    DiscoverLANServers();
}

void OnlineMultiplayerMenu::DiscoverLANServers() {
    // In a real implementation, this would use UDP broadcast
    // to discover servers on the local network

    // For now, create a placeholder LAN server
    m_lanServers.clear();

    // Example LAN server discovery (would be replaced with actual UDP discovery)
    // This is just for demonstration

    spdlog::debug("LAN discovery completed. Found {} servers", m_lanServers.size());
}

// Firebase Global Servers

void OnlineMultiplayerMenu::ConnectToFirebase() {
    spdlog::info("Connecting to Firebase...");

    NavigateTo(OnlineMenuState::FirebaseConnect);

    auto& firebase = Vehement::FirebaseManager::Instance();

    // Initialize Firebase if not already initialized
    if (!firebase.IsInitialized()) {
        bool success = firebase.Initialize("config/firebase.json");
        if (!success) {
            HandleConnectionResult(false, "Failed to initialize Firebase");
            return;
        }
    }

    // Sign in anonymously
    firebase.SignInAnonymously([this](bool success, const std::string& userId) {
        if (success) {
            m_connectedToFirebase = true;
            m_firebaseUserId = userId;
            spdlog::info("Connected to Firebase. User ID: {}", userId);

            // Initialize matchmaking
            auto& matchmaking = Vehement::Matchmaking::Instance();
            matchmaking.Initialize();

            if (m_onFirebaseConnected) {
                m_onFirebaseConnected();
            }
        } else {
            HandleConnectionResult(false, "Firebase authentication failed");
        }
    });
}

void OnlineMultiplayerMenu::DisconnectFromFirebase() {
    if (!m_connectedToFirebase) return;

    spdlog::info("Disconnecting from Firebase");

    auto& firebase = Vehement::FirebaseManager::Instance();
    firebase.SignOut();

    auto& matchmaking = Vehement::Matchmaking::Instance();
    matchmaking.Shutdown();

    m_connectedToFirebase = false;
    m_firebaseUserId.clear();
    m_firebaseServers.clear();
}

void OnlineMultiplayerMenu::JoinFirebaseServer(const std::string& serverId) {
    if (!m_connectedToFirebase) {
        ShowError("Not connected to Firebase");
        return;
    }

    spdlog::info("Joining Firebase server: {}", serverId);

    NavigateTo(OnlineMenuState::Connecting);

    // Use Firebase matchmaking to join server
    // Implementation would involve Firebase Realtime Database

    m_inLobby = true;
    m_currentConnectionType = ConnectionType::FirebaseGlobal;
    NavigateTo(OnlineMenuState::Lobby);
}

void OnlineMultiplayerMenu::CreateFirebaseServer(const LocalServerConfig& config) {
    if (!m_connectedToFirebase) {
        spdlog::warn("Cannot create Firebase server - not connected");
        return;
    }

    spdlog::info("Creating Firebase server: {}", config.serverName);

    // Create server entry in Firebase
    nlohmann::json serverData;
    serverData["name"] = config.serverName;
    serverData["hostId"] = m_firebaseUserId;
    serverData["maxPlayers"] = config.maxPlayers;
    serverData["currentPlayers"] = 1;
    serverData["gameMode"] = config.gameMode;
    serverData["mapId"] = config.mapId;
    serverData["hasPassword"] = !config.password.empty();
    serverData["inviteCode"] = m_serverInviteCode;
    serverData["visibility"] = static_cast<int>(config.visibility);

    auto& firebase = Vehement::FirebaseManager::Instance();
    std::string serverPath = "servers/" + m_serverInviteCode;
    firebase.SetValue(serverPath, serverData);

    spdlog::info("Firebase server created at path: {}", serverPath);
}

// Server Browser

void OnlineMultiplayerMenu::ShowServerBrowser() {
    NavigateTo(OnlineMenuState::ServerBrowser);
    RefreshServerList();
}

void OnlineMultiplayerMenu::RefreshServerList() {
    spdlog::debug("Refreshing server list");

    m_availableServers.clear();

    // Add LAN servers
    m_availableServers.insert(m_availableServers.end(), m_lanServers.begin(), m_lanServers.end());

    // Add Firebase servers if connected
    if (m_connectedToFirebase) {
        QueryFirebaseServers();
        m_availableServers.insert(m_availableServers.end(), m_firebaseServers.begin(), m_firebaseServers.end());
    }

    // Sort by ping
    SortServers("ping");

    if (m_onServerListUpdate) {
        m_onServerListUpdate(m_availableServers);
    }

    spdlog::debug("Server list refreshed. {} total servers", m_availableServers.size());
}

void OnlineMultiplayerMenu::QueryFirebaseServers() {
    if (!m_connectedToFirebase) return;

    auto& firebase = Vehement::FirebaseManager::Instance();

    firebase.GetValue("servers", [this](const nlohmann::json& data) {
        m_firebaseServers.clear();

        if (data.is_object()) {
            for (auto& [key, value] : data.items()) {
                ServerInfo server;
                server.id = key;
                server.firebaseId = key;
                server.name = value.value("name", "Unnamed Server");
                server.hostName = value.value("hostName", "Unknown");
                server.gameMode = value.value("gameMode", "Standard");
                server.mapName = value.value("mapId", "");
                server.currentPlayers = value.value("currentPlayers", 0);
                server.maxPlayers = value.value("maxPlayers", 8);
                server.hasPassword = value.value("hasPassword", false);
                server.connectionType = ConnectionType::FirebaseGlobal;
                server.isLAN = false;

                // Filter out full servers in certain cases
                server.isFull = server.currentPlayers >= server.maxPlayers;

                m_firebaseServers.push_back(server);
            }
        }

        spdlog::debug("Queried {} Firebase servers", m_firebaseServers.size());
    });
}

void OnlineMultiplayerMenu::FilterServers(const std::string& searchText, bool hideFullServers, bool hideLocked) {
    std::vector<ServerInfo> filtered;

    for (const auto& server : m_availableServers) {
        // Apply search filter
        if (!searchText.empty()) {
            std::string lowerSearch = searchText;
            std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);

            std::string lowerName = server.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (lowerName.find(lowerSearch) == std::string::npos) {
                continue;
            }
        }

        // Filter full servers
        if (hideFullServers && server.isFull) {
            continue;
        }

        // Filter locked servers
        if (hideLocked && server.hasPassword) {
            continue;
        }

        filtered.push_back(server);
    }

    // Update available servers with filtered list (temporarily)
    // In real implementation, would maintain separate filtered list
    if (m_onServerListUpdate) {
        m_onServerListUpdate(filtered);
    }
}

void OnlineMultiplayerMenu::SortServers(const std::string& criteria) {
    if (criteria == "ping") {
        std::sort(m_availableServers.begin(), m_availableServers.end(),
            [](const ServerInfo& a, const ServerInfo& b) { return a.ping < b.ping; });
    }
    else if (criteria == "players") {
        std::sort(m_availableServers.begin(), m_availableServers.end(),
            [](const ServerInfo& a, const ServerInfo& b) { return a.currentPlayers > b.currentPlayers; });
    }
    else if (criteria == "name") {
        std::sort(m_availableServers.begin(), m_availableServers.end(),
            [](const ServerInfo& a, const ServerInfo& b) { return a.name < b.name; });
    }
}

void OnlineMultiplayerMenu::AddToFavorites(const std::string& serverId) {
    if (std::find(m_favoriteServerIds.begin(), m_favoriteServerIds.end(), serverId) == m_favoriteServerIds.end()) {
        m_favoriteServerIds.push_back(serverId);
        spdlog::info("Added server {} to favorites", serverId);
    }
}

void OnlineMultiplayerMenu::RemoveFromFavorites(const std::string& serverId) {
    auto it = std::find(m_favoriteServerIds.begin(), m_favoriteServerIds.end(), serverId);
    if (it != m_favoriteServerIds.end()) {
        m_favoriteServerIds.erase(it);
        spdlog::info("Removed server {} from favorites", serverId);
    }
}

std::vector<ServerInfo> OnlineMultiplayerMenu::GetFavoriteServers() const {
    std::vector<ServerInfo> favorites;

    for (const auto& server : m_availableServers) {
        if (std::find(m_favoriteServerIds.begin(), m_favoriteServerIds.end(), server.id) != m_favoriteServerIds.end()) {
            favorites.push_back(server);
        }
    }

    return favorites;
}

void OnlineMultiplayerMenu::QuickJoin(const QuickJoinPreferences& prefs) {
    RefreshServerList();

    // Find best server based on preferences
    ServerInfo* bestServer = nullptr;
    int bestScore = -1;

    for (auto& server : m_availableServers) {
        // Skip full servers if preferred
        if (prefs.preferNotFull && server.isFull) continue;

        // Skip password servers if not allowed
        if (!prefs.allowPassword && server.hasPassword) continue;

        // Skip high ping servers
        if (server.ping > prefs.maxPing) continue;

        // Calculate score
        int score = 0;

        // Prefer servers with more players
        score += server.currentPlayers * 10;

        // Prefer lower ping
        score += (prefs.maxPing - server.ping) / 10;

        // Prefer matching game mode
        if (!prefs.preferredGameMode.empty() && server.gameMode == prefs.preferredGameMode) {
            score += 50;
        }

        if (score > bestScore) {
            bestScore = score;
            bestServer = &server;
        }
    }

    if (bestServer) {
        spdlog::info("Quick join selected server: {}", bestServer->name);

        if (bestServer->connectionType == ConnectionType::FirebaseGlobal) {
            JoinFirebaseServer(bestServer->id);
        } else {
            JoinByIPAddress(bestServer->ipAddress, bestServer->port);
        }
    } else {
        ShowError("No suitable servers found for quick join");
    }
}

// Lobby Management

void OnlineMultiplayerMenu::LeaveLobby() {
    if (!m_inLobby) return;

    spdlog::info("Leaving lobby");

    m_inLobby = false;
    m_isReady = false;
    m_currentLobby = LobbyState();

    if (m_isHosting) {
        StopHosting();
    }

    ReturnToMainMenu();
}

void OnlineMultiplayerMenu::SetReady(bool ready) {
    m_isReady = ready;

    // Update ready state in lobby
    for (auto& player : m_currentLobby.players) {
        if (player.id == m_localPlayerId) {
            player.isReady = ready;
            break;
        }
    }

    // Sync ready state over network
    // Implementation would depend on networking backend

    if (m_onLobbyUpdate) {
        m_onLobbyUpdate(m_currentLobby);
    }
}

void OnlineMultiplayerMenu::ToggleReady() {
    SetReady(!m_isReady);
}

void OnlineMultiplayerMenu::ChangeTeam(int team) {
    for (auto& player : m_currentLobby.players) {
        if (player.id == m_localPlayerId) {
            player.team = team;
            break;
        }
    }

    if (m_onLobbyUpdate) {
        m_onLobbyUpdate(m_currentLobby);
    }
}

void OnlineMultiplayerMenu::ChangeRace(const std::string& raceId) {
    for (auto& player : m_currentLobby.players) {
        if (player.id == m_localPlayerId) {
            player.raceId = raceId;
            break;
        }
    }

    if (m_onLobbyUpdate) {
        m_onLobbyUpdate(m_currentLobby);
    }
}

// Lobby Actions (Host Only)

void OnlineMultiplayerMenu::UpdateLobbySettings(const LocalServerConfig& config) {
    if (!m_isHosting) {
        spdlog::warn("Cannot update lobby settings - not hosting");
        return;
    }

    m_currentLobby.config = config;
    m_hostConfig = config;

    if (m_onLobbyUpdate) {
        m_onLobbyUpdate(m_currentLobby);
    }
}

void OnlineMultiplayerMenu::KickPlayer(const std::string& playerId) {
    if (!m_isHosting) {
        spdlog::warn("Cannot kick player - not hosting");
        return;
    }

    auto it = std::remove_if(m_currentLobby.players.begin(), m_currentLobby.players.end(),
        [&playerId](const LobbyPlayerInfo& player) { return player.id == playerId; });

    if (it != m_currentLobby.players.end()) {
        m_currentLobby.players.erase(it, m_currentLobby.players.end());
        spdlog::info("Kicked player: {}", playerId);

        if (m_onLobbyUpdate) {
            m_onLobbyUpdate(m_currentLobby);
        }
    }
}

void OnlineMultiplayerMenu::StartGame() {
    if (!m_isHosting) {
        spdlog::warn("Cannot start game - not hosting");
        return;
    }

    // Check if all players are ready
    bool allReady = true;
    for (const auto& player : m_currentLobby.players) {
        if (!player.isReady && !player.isHost) {
            allReady = false;
            break;
        }
    }

    if (!allReady) {
        ShowError("Not all players are ready");
        return;
    }

    spdlog::info("Starting game...");
    m_currentLobby.gameStarting = true;
    m_currentLobby.countdownSeconds = 5;

    if (m_onGameStart) {
        m_onGameStart();
    }
}

// Network Quality

void OnlineMultiplayerMenu::UpdateNetworkStats() {
    if (!m_inLobby) return;

    // Get stats from transport layer
    auto transport = Network::Replication::NetworkTransport::create();
    if (transport && transport->isConnected()) {
        auto quality = transport->getConnectionQuality();

        m_networkStats.ping = quality.latency;
        m_networkStats.packetLoss = static_cast<float>(quality.packetLoss);
        m_networkStats.bandwidth = quality.bandwidth;
        m_networkStats.connectionQuality = GetConnectionQualityString(quality.latency, static_cast<float>(quality.packetLoss));
        m_networkStats.usingFirebaseRelay = transport->isUsingFirebaseRelay();
    }
}

std::string OnlineMultiplayerMenu::GetConnectionQualityString(int ping, float packetLoss) {
    if (ping < 50 && packetLoss < 1.0f) return "Excellent";
    if (ping < 100 && packetLoss < 3.0f) return "Good";
    if (ping < 150 && packetLoss < 5.0f) return "Fair";
    return "Poor";
}

void OnlineMultiplayerMenu::TestServerConnection(const std::string& serverId) {
    // Send ping packets to test connection quality
    spdlog::debug("Testing connection to server: {}", serverId);
}

int OnlineMultiplayerMenu::GetPingToServer(const std::string& serverId) const {
    for (const auto& server : m_availableServers) {
        if (server.id == serverId) {
            return server.ping;
        }
    }
    return 999; // Unknown
}

// Helper Functions

void OnlineMultiplayerMenu::UpdateLobbyUI() {
    // Update lobby UI with current state
    // This would interact with the UI manager to update lobby display
}

void OnlineMultiplayerMenu::UpdateServerBrowser() {
    // Update server browser UI
}

void OnlineMultiplayerMenu::HandleConnectionResult(bool success, const std::string& error) {
    if (success) {
        if (m_onServerJoined) {
            ServerInfo dummyServer; // Would be actual server info
            m_onServerJoined(dummyServer);
        }
    } else {
        NavigateBack();
        ShowError(error);

        if (m_onConnectionError) {
            m_onConnectionError(error);
        }
    }
}

void OnlineMultiplayerMenu::AddToRecentServers(const ServerInfo& server) {
    // Remove if already in list
    auto it = std::find_if(m_recentServers.begin(), m_recentServers.end(),
        [&server](const ServerInfo& s) { return s.id == server.id; });

    if (it != m_recentServers.end()) {
        m_recentServers.erase(it);
    }

    // Add to front
    m_recentServers.insert(m_recentServers.begin(), server);

    // Trim to max size
    if (m_recentServers.size() > MAX_RECENT_SERVERS) {
        m_recentServers.resize(MAX_RECENT_SERVERS);
    }
}

std::string OnlineMultiplayerMenu::GenerateInviteCode() {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 35);

    std::string code;
    code.reserve(8);

    for (int i = 0; i < 8; ++i) {
        code += chars[dis(gen)];
    }

    return code;
}

// Recent Servers

std::vector<ServerInfo> OnlineMultiplayerMenu::GetRecentServers() const {
    return m_recentServers;
}

void OnlineMultiplayerMenu::ClearRecentServers() {
    m_recentServers.clear();
}

// Error Handling

void OnlineMultiplayerMenu::ShowError(const std::string& message) {
    m_lastError = message;
    m_errorTime = std::chrono::steady_clock::now();
    spdlog::error("OnlineMultiplayerMenu: {}", message);
}

void OnlineMultiplayerMenu::ClearError() {
    m_lastError.clear();
}

// Friend & Party System

void OnlineMultiplayerMenu::InviteFriend(const std::string& friendId) {
    if (!m_inLobby) {
        ShowError("Not in a lobby");
        return;
    }

    spdlog::info("Inviting friend {} to lobby", friendId);

    // Send invite through Firebase or network system
    if (m_connectedToFirebase) {
        auto& firebase = Vehement::FirebaseManager::Instance();

        nlohmann::json inviteData;
        inviteData["lobbyId"] = m_currentLobby.lobbyId;
        inviteData["inviteCode"] = m_currentLobby.inviteCode;
        inviteData["serverName"] = m_currentLobby.serverName;
        inviteData["from"] = m_localPlayerId;

        std::string invitePath = "invites/" + friendId + "/" + m_currentLobby.lobbyId;
        firebase.SetValue(invitePath, inviteData);
    }
}

void OnlineMultiplayerMenu::ShowInviteFriends() {
    // Show friend list dialog for invites
}

void OnlineMultiplayerMenu::AcceptInvite(const std::string& inviteId) {
    // Join lobby from invite
    spdlog::info("Accepting invite: {}", inviteId);
}

void OnlineMultiplayerMenu::DeclineInvite(const std::string& inviteId) {
    spdlog::info("Declining invite: {}", inviteId);
}

// Callbacks

void OnlineMultiplayerMenu::SetOnServerJoined(std::function<void(const ServerInfo&)> callback) {
    m_onServerJoined = std::move(callback);
}

void OnlineMultiplayerMenu::SetOnLobbyUpdate(std::function<void(const LobbyState&)> callback) {
    m_onLobbyUpdate = std::move(callback);
}

void OnlineMultiplayerMenu::SetOnGameStart(std::function<void()> callback) {
    m_onGameStart = std::move(callback);
}

void OnlineMultiplayerMenu::SetOnConnectionError(std::function<void(const std::string&)> callback) {
    m_onConnectionError = std::move(callback);
}

void OnlineMultiplayerMenu::SetOnFirebaseConnected(std::function<void()> callback) {
    m_onFirebaseConnected = std::move(callback);
}

void OnlineMultiplayerMenu::SetOnServerListUpdate(std::function<void(const std::vector<ServerInfo>&)> callback) {
    m_onServerListUpdate = std::move(callback);
}

} // namespace Menu
} // namespace UI
} // namespace Game
