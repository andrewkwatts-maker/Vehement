#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Engine { namespace UI { class RuntimeUIManager; } }

namespace Game {
namespace UI {
namespace Menu {

/**
 * @brief Match type for multiplayer
 */
enum class MatchType {
    Ranked,
    Unranked
};

/**
 * @brief Game mode
 */
enum class GameMode {
    OneVsOne,
    TwoVsTwo,
    ThreeVsThree,
    FourVsFour,
    FreeForAll
};

/**
 * @brief Player ready state
 */
enum class PlayerReadyState {
    NotReady,
    Ready,
    Loading
};

/**
 * @brief Server region
 */
enum class ServerRegion {
    Auto,
    NorthAmerica,
    Europe,
    AsiaPacific,
    SouthAmerica,
    Oceania
};

/**
 * @brief Lobby player data
 */
struct LobbyPlayer {
    std::string id;
    std::string name;
    std::string avatarUrl;
    std::string raceId;
    std::string color;
    int team = 0;
    int slot = 0;
    PlayerReadyState readyState = PlayerReadyState::NotReady;
    bool isHost = false;
    bool isAI = false;
    int aiDifficulty = 1; // 1-4
    int ping = 0;
};

/**
 * @brief Lobby settings
 */
struct LobbySettings {
    std::string name;
    std::string password;
    std::string mapId;
    std::string mapName;
    GameMode gameMode = GameMode::OneVsOne;
    int maxPlayers = 2;
    int gameSpeed = 1; // 1=Normal, 2=Fast, 3=Faster
    bool allowSpectators = true;
    bool isPublic = true;
};

/**
 * @brief Lobby data
 */
struct Lobby {
    std::string id;
    std::string code;        // Join code
    LobbySettings settings;
    std::vector<LobbyPlayer> players;
    std::string hostId;
    bool gameStarting = false;
    int countdownSeconds = 0;
};

/**
 * @brief Player stats for multiplayer
 */
struct PlayerStats {
    std::string rank;
    int mmr = 1000;
    int wins = 0;
    int losses = 0;
    int winrate = 0;
    int gamesPlayed = 0;
};

/**
 * @brief Friend data
 */
struct Friend {
    std::string id;
    std::string name;
    std::string avatarUrl;
    std::string status;
    bool online = false;
    bool inGame = false;
};

/**
 * @brief Chat message
 */
struct ChatMessage {
    std::string sender;
    std::string text;
    std::string timestamp;
    std::string channel;
    bool isSystem = false;
};

/**
 * @brief Multiplayer Lobby Manager
 *
 * Manages multiplayer functionality including matchmaking queue,
 * private lobbies, player slots, team assignment, and chat.
 */
class MultiplayerLobby {
public:
    MultiplayerLobby();
    ~MultiplayerLobby();

    /**
     * @brief Initialize
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

    // Matchmaking

    /**
     * @brief Start matchmaking queue
     */
    void FindMatch(MatchType type, GameMode mode, ServerRegion region);

    /**
     * @brief Cancel matchmaking queue
     */
    void CancelQueue();

    /**
     * @brief Check if in queue
     */
    bool IsInQueue() const { return m_inQueue; }

    /**
     * @brief Get queue time in seconds
     */
    float GetQueueTime() const { return m_queueTime; }

    /**
     * @brief Accept found match
     */
    void AcceptMatch();

    /**
     * @brief Decline found match
     */
    void DeclineMatch();

    // Lobby Management

    /**
     * @brief Create a new lobby
     */
    void CreateLobby(const LobbySettings& settings);

    /**
     * @brief Join lobby with code
     */
    void JoinLobbyWithCode(const std::string& code);

    /**
     * @brief Join lobby by ID
     */
    void JoinLobby(const std::string& lobbyId);

    /**
     * @brief Leave current lobby
     */
    void LeaveLobby();

    /**
     * @brief Check if in lobby
     */
    bool IsInLobby() const { return m_inLobby; }

    /**
     * @brief Check if host of lobby
     */
    bool IsHost() const { return m_isHost; }

    /**
     * @brief Get current lobby
     */
    const Lobby* GetCurrentLobby() const { return m_inLobby ? &m_currentLobby : nullptr; }

    // Lobby Actions (Host Only)

    /**
     * @brief Update lobby settings
     */
    void UpdateLobbySettings(const LobbySettings& settings);

    /**
     * @brief Kick player from lobby
     */
    void KickPlayer(const std::string& playerId);

    /**
     * @brief Add AI player
     */
    void AddAI(int team, int slot, int difficulty = 2);

    /**
     * @brief Remove AI player
     */
    void RemoveAI(int slot);

    /**
     * @brief Start game
     */
    void StartGame();

    // Player Actions

    /**
     * @brief Set ready state
     */
    void SetReady(bool ready);

    /**
     * @brief Toggle ready state
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

    /**
     * @brief Change color
     */
    void ChangeColor(const std::string& color);

    // Stats & Friends

    /**
     * @brief Get player stats
     */
    const PlayerStats& GetPlayerStats() const { return m_playerStats; }

    /**
     * @brief Get friends list
     */
    const std::vector<Friend>& GetFriends() const { return m_friends; }

    /**
     * @brief Invite friend to party
     */
    void InviteFriend(const std::string& friendId);

    /**
     * @brief Refresh friends list
     */
    void RefreshFriends();

    // Chat

    /**
     * @brief Send chat message
     */
    void SendChatMessage(const std::string& message, const std::string& channel = "global");

    /**
     * @brief Get chat history
     */
    const std::vector<ChatMessage>& GetChatHistory() const { return m_chatHistory; }

    // Network

    /**
     * @brief Set preferred region
     */
    void SetRegion(ServerRegion region);

    /**
     * @brief Get current ping
     */
    int GetPing() const { return m_currentPing; }

    /**
     * @brief Get online player count
     */
    int GetPlayersOnline() const { return m_playersOnline; }

    // Callbacks

    void SetOnMatchFound(std::function<void(const std::string&, const std::string&)> callback);
    void SetOnLobbyUpdate(std::function<void(const Lobby&)> callback);
    void SetOnGameStart(std::function<void(const Lobby&)> callback);
    void SetOnChatMessage(std::function<void(const ChatMessage&)> callback);
    void SetOnError(std::function<void(const std::string&)> callback);

private:
    void SetupEventHandlers();
    void UpdateQueueStatus();
    void UpdateLobbyUI();
    void ProcessServerMessage(const std::string& type, const nlohmann::json& data);
    std::string GameModeToString(GameMode mode) const;
    std::string RegionToString(ServerRegion region) const;

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;

    // Matchmaking state
    bool m_inQueue = false;
    float m_queueTime = 0.0f;
    MatchType m_queueMatchType = MatchType::Ranked;
    GameMode m_queueGameMode = GameMode::OneVsOne;
    ServerRegion m_selectedRegion = ServerRegion::Auto;
    int m_playersInQueue = 0;
    bool m_matchFound = false;

    // Lobby state
    bool m_inLobby = false;
    bool m_isHost = false;
    bool m_isReady = false;
    Lobby m_currentLobby;
    std::string m_localPlayerId;

    // Stats
    PlayerStats m_playerStats;
    std::vector<Friend> m_friends;

    // Chat
    std::vector<ChatMessage> m_chatHistory;
    int m_maxChatHistory = 100;

    // Network
    int m_currentPing = 0;
    int m_playersOnline = 0;

    // Callbacks
    std::function<void(const std::string&, const std::string&)> m_onMatchFound;
    std::function<void(const Lobby&)> m_onLobbyUpdate;
    std::function<void(const Lobby&)> m_onGameStart;
    std::function<void(const ChatMessage&)> m_onChatMessage;
    std::function<void(const std::string&)> m_onError;
};

} // namespace Menu
} // namespace UI
} // namespace Game
