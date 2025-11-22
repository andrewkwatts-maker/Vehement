#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Editor {
namespace Network {

// UI state
enum class MatchmakingUIState {
    Idle,
    Searching,
    MatchFound,
    InLobby,
    Starting,
    Error
};

// Player display info
struct PlayerDisplayInfo {
    std::string oderId;
    std::string displayName;
    int rating;
    int tier;
    bool isReady;
    bool isHost;
    int team;
    std::string avatarUrl;
};

// Lobby display info
struct LobbyDisplayInfo {
    std::string lobbyId;
    std::string name;
    std::string hostName;
    std::string gameMode;
    int playerCount;
    int maxPlayers;
    bool hasPassword;
    int avgRating;
};

// Search progress info
struct SearchProgressInfo {
    float searchTime;
    int expandCount;
    int currentMmrRange;
    int playersInQueue;
    std::string estimatedWaitTime;
};

// Callbacks
using StateChangeCallback = std::function<void(MatchmakingUIState)>;
using LobbyJoinCallback = std::function<void(const std::string& lobbyId)>;
using ReadyCallback = std::function<void(bool ready)>;

/**
 * MatchmakingUI - Matchmaking interface
 *
 * Features:
 * - Queue status display
 * - Match found dialog
 * - Lobby browser
 * - Player list in lobby
 */
class MatchmakingUI {
public:
    static MatchmakingUI& getInstance();

    // Panel control
    void show(bool visible = true) { m_visible = visible; }
    void hide() { m_visible = false; }
    void toggle() { m_visible = !m_visible; }
    bool isVisible() const { return m_visible; }

    // Update and render
    void update(float deltaTime);
    void render();
    std::string renderHTML();

    // State management
    MatchmakingUIState getState() const { return m_state; }
    void setState(MatchmakingUIState state);
    void onStateChange(StateChangeCallback callback);

    // Search UI
    void showSearchUI();
    void updateSearchProgress(const SearchProgressInfo& info);
    void hideSearchUI();

    // Match found dialog
    void showMatchFound(float acceptTimeout);
    void hideMatchFound();
    bool isMatchFoundVisible() const { return m_showMatchFound; }
    void acceptMatch();
    void declineMatch();

    // Lobby browser
    void showLobbyBrowser();
    void hideLobbyBrowser();
    void updateLobbyList(const std::vector<LobbyDisplayInfo>& lobbies);
    void setLobbyFilter(const std::string& filter);
    void refreshLobbyList();
    void onLobbyJoin(LobbyJoinCallback callback);

    // Lobby UI
    void showLobbyUI(const std::string& lobbyId, const std::string& name);
    void hideLobbyUI();
    void updatePlayerList(const std::vector<PlayerDisplayInfo>& players);
    void updateLobbySettings(const std::unordered_map<std::string, std::string>& settings);
    void setLocalPlayerId(const std::string& id) { m_localPlayerId = id; }
    bool isHost() const;

    // Ready system
    void setReady(bool ready);
    bool isReady() const { return m_isReady; }
    void onReadyChange(ReadyCallback callback);
    void showReadyCheck(float timeout);
    void hideReadyCheck();
    bool isAllReady() const;

    // Invite system
    void showInviteCode(const std::string& code);
    void copyInviteCode();
    void joinByCode(const std::string& code);

    // Error display
    void showError(const std::string& message);
    void hideError();

private:
    MatchmakingUI();
    ~MatchmakingUI();
    MatchmakingUI(const MatchmakingUI&) = delete;
    MatchmakingUI& operator=(const MatchmakingUI&) = delete;

    // Rendering helpers
    std::string renderSearchingHTML();
    std::string renderMatchFoundHTML();
    std::string renderLobbyBrowserHTML();
    std::string renderLobbyHTML();
    std::string renderReadyCheckHTML();
    std::string formatTime(float seconds);

private:
    bool m_visible = false;
    MatchmakingUIState m_state = MatchmakingUIState::Idle;

    // Search state
    bool m_isSearching = false;
    SearchProgressInfo m_searchProgress;

    // Match found
    bool m_showMatchFound = false;
    float m_matchFoundTimeout = 0.0f;
    float m_matchFoundTimer = 0.0f;

    // Lobby browser
    bool m_showLobbyBrowser = false;
    std::vector<LobbyDisplayInfo> m_lobbies;
    std::string m_lobbyFilter;
    int m_selectedLobbyIndex = -1;

    // Current lobby
    bool m_inLobby = false;
    std::string m_currentLobbyId;
    std::string m_currentLobbyName;
    std::vector<PlayerDisplayInfo> m_players;
    std::unordered_map<std::string, std::string> m_lobbySettings;
    std::string m_localPlayerId;
    bool m_isReady = false;
    std::string m_inviteCode;

    // Ready check
    bool m_showReadyCheck = false;
    float m_readyCheckTimeout = 0.0f;
    float m_readyCheckTimer = 0.0f;

    // Error
    bool m_showErrorDialog = false;
    std::string m_errorMessage;

    // Callbacks
    std::vector<StateChangeCallback> m_stateCallbacks;
    std::vector<LobbyJoinCallback> m_lobbyJoinCallbacks;
    std::vector<ReadyCallback> m_readyCallbacks;
};

} // namespace Network
} // namespace Editor
