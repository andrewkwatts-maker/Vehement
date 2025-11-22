#pragma once

#include "FirebaseCore.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <variant>

namespace Network {
namespace Firebase {

// JSON value types for realtime data
using RealtimeValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<std::shared_ptr<struct RealtimeNode>>,
    std::unordered_map<std::string, std::shared_ptr<struct RealtimeNode>>
>;

// Node in the realtime database
struct RealtimeNode {
    std::string key;
    RealtimeValue value;
    std::chrono::system_clock::time_point timestamp;

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(value); }
    bool isBool() const { return std::holds_alternative<bool>(value); }
    bool isInt() const { return std::holds_alternative<int64_t>(value); }
    bool isDouble() const { return std::holds_alternative<double>(value); }
    bool isString() const { return std::holds_alternative<std::string>(value); }
    bool isArray() const { return std::holds_alternative<std::vector<std::shared_ptr<RealtimeNode>>>(value); }
    bool isObject() const { return std::holds_alternative<std::unordered_map<std::string, std::shared_ptr<RealtimeNode>>>(value); }

    bool getBool() const { return std::get<bool>(value); }
    int64_t getInt() const { return std::get<int64_t>(value); }
    double getDouble() const { return std::get<double>(value); }
    const std::string& getString() const { return std::get<std::string>(value); }
};

// Player presence state
enum class PresenceState {
    Offline,
    Online,
    Away,
    Busy,
    InGame
};

// Player presence data
struct PlayerPresence {
    std::string oderId;
    std::string displayName;
    PresenceState state = PresenceState::Offline;
    std::chrono::system_clock::time_point lastSeen;
    std::string currentActivity;  // e.g., "In Match", "Lobby", "Menu"
    std::string matchId;          // If in a match
    std::string lobbyId;          // If in a lobby
    std::unordered_map<std::string, std::string> metadata;
};

// Session data for a player
struct SessionData {
    std::string sessionId;
    std::string oderId;
    std::string deviceId;
    std::string platform;
    std::string version;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastActivity;
    std::unordered_map<std::string, std::string> data;
};

// Game state for synchronization
struct GameState {
    std::string gameId;
    std::string matchId;
    int currentTurn = 0;
    int phase = 0;
    std::chrono::system_clock::time_point lastUpdate;
    std::unordered_map<std::string, RealtimeNode> playerStates;
    std::unordered_map<std::string, RealtimeNode> sharedState;
};

// Leaderboard entry
struct LeaderboardEntry {
    std::string oderId;
    std::string displayName;
    int64_t score = 0;
    int rank = 0;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

// Leaderboard configuration
struct LeaderboardConfig {
    std::string leaderboardId;
    std::string name;
    bool ascending = false;  // Higher scores = better by default
    int maxEntries = 100;
    std::chrono::hours resetInterval{0};  // 0 = never reset
    std::chrono::system_clock::time_point lastReset;
};

// Event types for listeners
enum class RealtimeEventType {
    ValueChanged,
    ChildAdded,
    ChildChanged,
    ChildRemoved,
    ChildMoved
};

// Callbacks
using RealtimeCallback = std::function<void(const RealtimeNode&, const FirebaseError&)>;
using PresenceCallback = std::function<void(const PlayerPresence&)>;
using GameStateCallback = std::function<void(const GameState&)>;
using LeaderboardCallback = std::function<void(const std::vector<LeaderboardEntry>&, const FirebaseError&)>;
using ListenerCallback = std::function<void(RealtimeEventType, const RealtimeNode&)>;

// Listener handle for unsubscribing
struct ListenerHandle {
    std::string path;
    uint64_t listenerId;
    bool isActive = true;
};

/**
 * FirebaseRealtime - Real-time database operations
 *
 * Features:
 * - Player presence tracking
 * - Game state synchronization
 * - Live leaderboards
 * - Session data management
 * - Real-time listeners
 */
class FirebaseRealtime {
public:
    static FirebaseRealtime& getInstance();

    // Initialization
    bool initialize();
    void shutdown();
    void update(float deltaTime);

    // Basic operations
    void get(const std::string& path, RealtimeCallback callback);
    void set(const std::string& path, const RealtimeNode& value, RealtimeCallback callback = nullptr);
    void update(const std::string& path, const std::unordered_map<std::string, RealtimeNode>& updates,
                RealtimeCallback callback = nullptr);
    void push(const std::string& path, const RealtimeNode& value, RealtimeCallback callback = nullptr);
    void remove(const std::string& path, std::function<void(const FirebaseError&)> callback = nullptr);

    // Transactions (atomic updates)
    void transaction(const std::string& path,
                     std::function<RealtimeNode(const RealtimeNode&)> updateFunc,
                     RealtimeCallback callback = nullptr);

    // Listeners
    ListenerHandle addValueListener(const std::string& path, ListenerCallback callback);
    ListenerHandle addChildListener(const std::string& path, ListenerCallback callback);
    void removeListener(const ListenerHandle& handle);
    void removeAllListeners(const std::string& path);

    // Player presence
    void setPresence(PresenceState state, const std::string& activity = "");
    void setPresenceOnDisconnect(PresenceState state);
    PlayerPresence getMyPresence() const { return m_myPresence; }
    void getPresence(const std::string& oderId, std::function<void(const PlayerPresence&, const FirebaseError&)> callback);
    void watchPresence(const std::string& oderId, PresenceCallback callback);
    void unwatchPresence(const std::string& oderId);
    void watchFriendsPresence(const std::vector<std::string>& friendIds, PresenceCallback callback);
    void getOnlineFriends(const std::vector<std::string>& friendIds,
                          std::function<void(const std::vector<PlayerPresence>&)> callback);

    // Game state synchronization
    void createGameState(const std::string& matchId, GameStateCallback callback);
    void joinGameState(const std::string& gameId, GameStateCallback callback);
    void leaveGameState();
    void updateMyState(const RealtimeNode& state);
    void updateSharedState(const std::string& key, const RealtimeNode& value);
    void broadcastAction(const std::string& action, const RealtimeNode& data);
    void onGameStateUpdate(GameStateCallback callback);
    void onPlayerAction(std::function<void(const std::string& oderId, const std::string& action, const RealtimeNode&)> callback);
    const GameState& getCurrentGameState() const { return m_currentGameState; }

    // Session data
    void startSession();
    void endSession();
    void updateSessionData(const std::string& key, const std::string& value);
    SessionData getSession() const { return m_currentSession; }
    void getActiveSessionCount(std::function<void(int count)> callback);

    // Leaderboards
    void submitScore(const std::string& leaderboardId, int64_t score,
                     const std::unordered_map<std::string, std::string>& metadata = {},
                     std::function<void(int rank, const FirebaseError&)> callback = nullptr);
    void getLeaderboard(const std::string& leaderboardId, int count, int offset,
                        LeaderboardCallback callback);
    void getMyRank(const std::string& leaderboardId,
                   std::function<void(const LeaderboardEntry&, const FirebaseError&)> callback);
    void getAroundMe(const std::string& leaderboardId, int count,
                     LeaderboardCallback callback);
    void getFriendsLeaderboard(const std::string& leaderboardId,
                               const std::vector<std::string>& friendIds,
                               LeaderboardCallback callback);
    void watchLeaderboard(const std::string& leaderboardId, int count, LeaderboardCallback callback);
    void unwatchLeaderboard(const std::string& leaderboardId);

    // Server timestamp
    void getServerTime(std::function<void(std::chrono::system_clock::time_point)> callback);
    std::chrono::milliseconds getServerTimeOffset() const { return m_serverTimeOffset; }

    // Connection state
    bool isConnected() const { return m_isConnected; }
    void onConnectionStateChanged(std::function<void(bool connected)> callback);

private:
    FirebaseRealtime();
    ~FirebaseRealtime();
    FirebaseRealtime(const FirebaseRealtime&) = delete;
    FirebaseRealtime& operator=(const FirebaseRealtime&) = delete;

    // Internal operations
    void executeGet(const std::string& path, RealtimeCallback callback);
    void executeSet(const std::string& path, const std::string& json, RealtimeCallback callback);
    void executeUpdate(const std::string& path, const std::string& json, RealtimeCallback callback);
    void executePush(const std::string& path, const std::string& json, RealtimeCallback callback);
    void executeDelete(const std::string& path, std::function<void(const FirebaseError&)> callback);

    // Listener management
    void processListenerUpdates();
    void startListening(const std::string& path);
    void stopListening(const std::string& path);

    // Presence management
    void setupPresenceSystem();
    void updatePresenceTimestamp();
    void handleDisconnect();

    // Serialization
    std::string serializeNode(const RealtimeNode& node);
    RealtimeNode deserializeNode(const std::string& json);
    std::string serializePresence(const PlayerPresence& presence);
    PlayerPresence deserializePresence(const std::string& json);

    // URL building
    std::string buildUrl(const std::string& path, bool withAuth = true);

private:
    bool m_initialized = false;
    bool m_isConnected = false;

    // Presence
    PlayerPresence m_myPresence;
    std::unordered_map<std::string, PlayerPresence> m_presenceCache;
    std::unordered_map<std::string, PresenceCallback> m_presenceWatchers;
    float m_presenceUpdateTimer = 0.0f;
    static constexpr float PRESENCE_UPDATE_INTERVAL = 30.0f;

    // Game state
    GameState m_currentGameState;
    std::vector<GameStateCallback> m_gameStateCallbacks;
    std::vector<std::function<void(const std::string&, const std::string&, const RealtimeNode&)>> m_actionCallbacks;
    bool m_inGameState = false;

    // Session
    SessionData m_currentSession;
    bool m_sessionActive = false;

    // Listeners
    std::unordered_map<std::string, std::vector<std::pair<uint64_t, ListenerCallback>>> m_valueListeners;
    std::unordered_map<std::string, std::vector<std::pair<uint64_t, ListenerCallback>>> m_childListeners;
    uint64_t m_nextListenerId = 1;
    std::unordered_map<std::string, RealtimeNode> m_lastValues;  // For detecting changes

    // Leaderboard watchers
    std::unordered_map<std::string, LeaderboardCallback> m_leaderboardWatchers;

    // Connection callbacks
    std::vector<std::function<void(bool)>> m_connectionCallbacks;

    // Server time
    std::chrono::milliseconds m_serverTimeOffset{0};
    bool m_serverTimeSynced = false;

    // Polling for listeners (REST API doesn't support true realtime)
    float m_pollTimer = 0.0f;
    static constexpr float POLL_INTERVAL = 1.0f;
};

} // namespace Firebase
} // namespace Network
