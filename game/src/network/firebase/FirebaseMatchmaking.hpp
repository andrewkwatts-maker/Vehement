#pragma once

#include "FirebaseCore.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <optional>

namespace Network {
namespace Firebase {

// Match states
enum class MatchState {
    None,
    Searching,
    Found,
    InLobby,
    Ready,
    Starting,
    InProgress,
    Finished,
    Cancelled
};

// Matchmaking modes
enum class MatchmakingMode {
    QuickMatch,      // Fast skill-based matching
    Ranked,          // Competitive with MMR
    Custom,          // Custom game with filters
    Private,         // Invite-only lobby
    Tutorial         // AI/practice match
};

// Game modes for filtering
enum class GameMode {
    Deathmatch,
    TeamDeathmatch,
    Capture,
    Survival,
    Cooperative,
    Custom
};

// Region for matchmaking
enum class Region {
    Auto,
    NorthAmerica,
    Europe,
    Asia,
    SouthAmerica,
    Oceania,
    Africa,
    MiddleEast
};

// Player skill/rating data
struct PlayerRating {
    int mmr = 1000;              // Matchmaking Rating
    int elo = 1000;              // ELO rating
    int tier = 0;                // Ranked tier (0-10)
    int division = 0;            // Division within tier
    int wins = 0;
    int losses = 0;
    int draws = 0;
    float winRate = 0.5f;
    int streak = 0;              // Current win/loss streak
    int peakMmr = 1000;          // Highest MMR achieved

    float getWinRate() const {
        int total = wins + losses + draws;
        return total > 0 ? static_cast<float>(wins) / total : 0.5f;
    }
};

// Match search filters
struct MatchFilters {
    std::vector<GameMode> gameModes;
    std::vector<Region> regions;
    int minPlayers = 2;
    int maxPlayers = 10;
    int mmrRange = 200;          // +/- from player's MMR
    bool allowCrossPlatform = true;
    bool rankedOnly = false;
    std::unordered_map<std::string, std::string> customFilters;

    // Expand search over time
    float searchTimeMultiplier = 1.5f;  // MMR range expands by this factor over time
};

// Player in a lobby
struct LobbyPlayer {
    std::string odId;
    std::string displayName;
    std::string oderId;  // Added properly
    bool isHost = false;
    bool isReady = false;
    int team = 0;
    int slot = -1;
    PlayerRating rating;
    std::chrono::system_clock::time_point joinedAt;
    std::unordered_map<std::string, std::string> metadata;
};

// Match lobby
struct MatchLobby {
    std::string lobbyId;
    std::string hostId;
    std::string name;
    std::string password;        // Empty = public
    GameMode gameMode = GameMode::Deathmatch;
    Region region = Region::Auto;
    MatchmakingMode matchMode = MatchmakingMode::QuickMatch;
    MatchState state = MatchState::None;

    int minPlayers = 2;
    int maxPlayers = 10;
    int teamCount = 2;

    std::vector<LobbyPlayer> players;
    std::unordered_map<std::string, std::string> settings;

    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point expiresAt;

    bool isFull() const { return players.size() >= static_cast<size_t>(maxPlayers); }
    bool isEmpty() const { return players.empty(); }
    bool isPublic() const { return password.empty(); }
    int getPlayerCount() const { return static_cast<int>(players.size()); }
};

// Search ticket for matchmaking queue
struct MatchTicket {
    std::string ticketId;
    std::string oderId;
    MatchFilters filters;
    PlayerRating rating;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::milliseconds searchTime{0};
    int expandCount = 0;         // How many times search expanded
};

// Match result for rating updates
struct MatchResult {
    std::string matchId;
    std::string lobbyId;
    GameMode gameMode;
    bool ranked = false;

    struct PlayerResult {
        std::string oderId;
        int team;
        int placement;           // 1 = winner
        int score;
        int kills;
        int deaths;
        int assists;
        float performance;       // 0-1 performance score
    };

    std::vector<PlayerResult> results;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point endedAt;
    std::chrono::milliseconds duration;
};

// Rating change after a match
struct RatingChange {
    int mmrChange = 0;
    int eloChange = 0;
    int newMmr = 0;
    int newElo = 0;
    bool tierChanged = false;
    int newTier = 0;
    int newDivision = 0;
    bool promoted = false;
    bool demoted = false;
};

// Lobby browser entry
struct LobbyBrowserEntry {
    std::string lobbyId;
    std::string name;
    std::string hostName;
    GameMode gameMode;
    Region region;
    int playerCount;
    int maxPlayers;
    bool hasPassword;
    int avgMmr;
    std::chrono::system_clock::time_point createdAt;
};

// Callbacks
using MatchFoundCallback = std::function<void(const MatchLobby&, const FirebaseError&)>;
using LobbyUpdateCallback = std::function<void(const MatchLobby&)>;
using PlayerJoinCallback = std::function<void(const LobbyPlayer&)>;
using PlayerLeaveCallback = std::function<void(const std::string& oderId)>;
using ReadyCheckCallback = std::function<void(bool allReady)>;
using MatchStartCallback = std::function<void(const std::string& matchId, const std::string& serverInfo)>;
using LobbyBrowserCallback = std::function<void(const std::vector<LobbyBrowserEntry>&, const FirebaseError&)>;
using RatingCallback = std::function<void(const PlayerRating&, const FirebaseError&)>;
using RatingChangeCallback = std::function<void(const RatingChange&, const FirebaseError&)>;

/**
 * FirebaseMatchmaking - Complete matchmaking system
 *
 * Features:
 * - Quick match with skill-based matching
 * - Custom match with filters
 * - Lobby browser
 * - Ready check system
 * - MMR/ELO rating updates
 * - Rematch handling
 */
class FirebaseMatchmaking {
public:
    static FirebaseMatchmaking& getInstance();

    // Initialization
    bool initialize();
    void shutdown();
    void update(float deltaTime);

    // Quick match
    void startQuickMatch(const MatchFilters& filters, MatchFoundCallback callback);
    void cancelSearch();
    bool isSearching() const { return m_isSearching; }
    float getSearchTime() const;
    const MatchTicket& getCurrentTicket() const { return m_currentTicket; }

    // Custom match / Lobby creation
    void createLobby(const std::string& name, GameMode mode, int maxPlayers,
                     const std::string& password, MatchFoundCallback callback);
    void joinLobby(const std::string& lobbyId, const std::string& password,
                   MatchFoundCallback callback);
    void joinLobbyByCode(const std::string& code, MatchFoundCallback callback);
    void leaveLobby();
    bool isInLobby() const { return m_currentLobby.has_value(); }
    const MatchLobby& getCurrentLobby() const { return *m_currentLobby; }

    // Lobby management (host only)
    void setLobbySettings(const std::unordered_map<std::string, std::string>& settings);
    void kickPlayer(const std::string& oderId);
    void transferHost(const std::string& newHostId);
    void setTeam(const std::string& oderId, int team);
    void setMaxPlayers(int maxPlayers);
    void setPassword(const std::string& password);

    // Ready system
    void setReady(bool ready);
    void startReadyCheck();
    void respondToReadyCheck(bool accept);
    void forceStart();  // Host only, start even if not all ready

    // Match start
    void startMatch();
    void requestRematch();
    void acceptRematch(bool accept);

    // Lobby browser
    void browsersLobbies(const MatchFilters& filters, LobbyBrowserCallback callback);
    void refreshLobbyBrowser(LobbyBrowserCallback callback);

    // Rating system
    void getPlayerRating(const std::string& oderId, RatingCallback callback);
    void getMyRating(RatingCallback callback);
    void submitMatchResult(const MatchResult& result, RatingChangeCallback callback);
    void getLeaderboard(int count, int offset,
                       std::function<void(const std::vector<PlayerRating>&, const FirebaseError&)> callback);

    // Callbacks
    void onLobbyUpdate(LobbyUpdateCallback callback);
    void onPlayerJoin(PlayerJoinCallback callback);
    void onPlayerLeave(PlayerLeaveCallback callback);
    void onReadyCheck(ReadyCheckCallback callback);
    void onMatchStart(MatchStartCallback callback);
    void onSearchProgress(std::function<void(float progress, int expandCount)> callback);

    // State queries
    MatchState getMatchState() const { return m_matchState; }
    bool isHost() const;
    std::string getInviteCode() const;

private:
    FirebaseMatchmaking();
    ~FirebaseMatchmaking();
    FirebaseMatchmaking(const FirebaseMatchmaking&) = delete;
    FirebaseMatchmaking& operator=(const FirebaseMatchmaking&) = delete;

    // Internal matchmaking
    void processMatchmakingQueue();
    void expandSearch();
    bool checkMatchCompatibility(const MatchTicket& ticket1, const MatchTicket& ticket2);
    void createMatchFromTickets(const std::vector<MatchTicket>& tickets);
    void onMatchFound(const MatchLobby& lobby);

    // Rating calculations
    RatingChange calculateRatingChange(const PlayerRating& rating, const MatchResult::PlayerResult& result,
                                        const MatchResult& match);
    int calculateEloChange(int playerElo, int opponentElo, float score);
    int calculateMmrChange(const PlayerRating& rating, float performance, bool won);

    // Firestore operations
    void createLobbyDocument(const MatchLobby& lobby, MatchFoundCallback callback);
    void updateLobbyDocument(const MatchLobby& lobby);
    void deleteLobbyDocument(const std::string& lobbyId);
    void subscribeToLobby(const std::string& lobbyId);
    void unsubscribeFromLobby();

    // Realtime Database operations (for matchmaking queue)
    void addToMatchmakingQueue(const MatchTicket& ticket);
    void removeFromMatchmakingQueue(const std::string& ticketId);
    void subscribeToMatchmakingQueue();

    // Helper methods
    std::string generateLobbyCode();
    std::string generateTicketId();
    void notifyLobbyUpdate();

private:
    bool m_initialized = false;

    // Search state
    std::atomic<bool> m_isSearching{false};
    MatchTicket m_currentTicket;
    std::chrono::steady_clock::time_point m_searchStartTime;
    float m_searchExpandTimer = 0.0f;
    MatchFoundCallback m_matchFoundCallback;

    // Lobby state
    std::optional<MatchLobby> m_currentLobby;
    MatchState m_matchState = MatchState::None;

    // Ready check
    bool m_readyCheckActive = false;
    std::chrono::steady_clock::time_point m_readyCheckStartTime;
    std::unordered_map<std::string, bool> m_readyResponses;

    // Rematch
    bool m_rematchRequested = false;
    std::unordered_map<std::string, bool> m_rematchResponses;

    // Callbacks
    std::vector<LobbyUpdateCallback> m_lobbyUpdateCallbacks;
    std::vector<PlayerJoinCallback> m_playerJoinCallbacks;
    std::vector<PlayerLeaveCallback> m_playerLeaveCallbacks;
    std::vector<ReadyCheckCallback> m_readyCheckCallbacks;
    std::vector<MatchStartCallback> m_matchStartCallbacks;
    std::function<void(float, int)> m_searchProgressCallback;

    // Lobby browser cache
    std::vector<LobbyBrowserEntry> m_cachedLobbies;
    MatchFilters m_lastBrowserFilters;
    std::chrono::steady_clock::time_point m_lastBrowserRefresh;

    // Rating cache
    std::unordered_map<std::string, PlayerRating> m_ratingCache;

    // Constants
    static constexpr float SEARCH_EXPAND_INTERVAL = 10.0f;  // Expand search every 10 seconds
    static constexpr float READY_CHECK_TIMEOUT = 30.0f;     // 30 seconds to accept ready check
    static constexpr int MAX_SEARCH_EXPANDS = 5;            // Maximum search expansions
    static constexpr int BASE_K_FACTOR = 32;                // ELO K-factor
};

} // namespace Firebase
} // namespace Network
