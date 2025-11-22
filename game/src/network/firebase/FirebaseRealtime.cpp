#include "FirebaseRealtime.hpp"
#include <sstream>
#include <random>
#include <algorithm>

namespace Network {
namespace Firebase {

FirebaseRealtime& FirebaseRealtime::getInstance() {
    static FirebaseRealtime instance;
    return instance;
}

FirebaseRealtime::FirebaseRealtime() {}

FirebaseRealtime::~FirebaseRealtime() {
    shutdown();
}

bool FirebaseRealtime::initialize() {
    if (m_initialized) return true;

    if (!FirebaseCore::getInstance().isInitialized()) {
        return false;
    }

    m_initialized = true;

    // Setup presence system
    setupPresenceSystem();

    // Sync server time
    getServerTime([this](std::chrono::system_clock::time_point serverTime) {
        auto localTime = std::chrono::system_clock::now();
        m_serverTimeOffset = std::chrono::duration_cast<std::chrono::milliseconds>(
            serverTime - localTime);
        m_serverTimeSynced = true;
    });

    return true;
}

void FirebaseRealtime::shutdown() {
    if (!m_initialized) return;

    // Set offline presence
    setPresence(PresenceState::Offline);

    // End session
    if (m_sessionActive) {
        endSession();
    }

    // Leave game state
    if (m_inGameState) {
        leaveGameState();
    }

    // Remove all listeners
    m_valueListeners.clear();
    m_childListeners.clear();

    m_initialized = false;
}

void FirebaseRealtime::update(float deltaTime) {
    if (!m_initialized) return;

    // Update presence timestamp periodically
    m_presenceUpdateTimer += deltaTime;
    if (m_presenceUpdateTimer >= PRESENCE_UPDATE_INTERVAL) {
        m_presenceUpdateTimer = 0.0f;
        updatePresenceTimestamp();
    }

    // Poll for listener updates (REST API fallback)
    m_pollTimer += deltaTime;
    if (m_pollTimer >= POLL_INTERVAL) {
        m_pollTimer = 0.0f;
        processListenerUpdates();
    }
}

void FirebaseRealtime::get(const std::string& path, RealtimeCallback callback) {
    executeGet(path, callback);
}

void FirebaseRealtime::set(const std::string& path, const RealtimeNode& value, RealtimeCallback callback) {
    std::string json = serializeNode(value);
    executeSet(path, json, callback);
}

void FirebaseRealtime::update(const std::string& path,
                               const std::unordered_map<std::string, RealtimeNode>& updates,
                               RealtimeCallback callback) {
    std::ostringstream json;
    json << "{";
    bool first = true;
    for (const auto& [key, value] : updates) {
        if (!first) json << ",";
        first = false;
        json << "\"" << key << "\":" << serializeNode(value);
    }
    json << "}";

    executeUpdate(path, json.str(), callback);
}

void FirebaseRealtime::push(const std::string& path, const RealtimeNode& value, RealtimeCallback callback) {
    std::string json = serializeNode(value);
    executePush(path, json, callback);
}

void FirebaseRealtime::remove(const std::string& path, std::function<void(const FirebaseError&)> callback) {
    executeDelete(path, callback);
}

void FirebaseRealtime::transaction(const std::string& path,
                                    std::function<RealtimeNode(const RealtimeNode&)> updateFunc,
                                    RealtimeCallback callback) {
    // REST API doesn't support true transactions
    // Implement optimistic locking pattern
    get(path, [this, path, updateFunc, callback](const RealtimeNode& current, const FirebaseError& error) {
        if (error) {
            if (callback) callback(RealtimeNode(), error);
            return;
        }

        RealtimeNode newValue = updateFunc(current);
        set(path, newValue, callback);
    });
}

ListenerHandle FirebaseRealtime::addValueListener(const std::string& path, ListenerCallback callback) {
    ListenerHandle handle;
    handle.path = path;
    handle.listenerId = m_nextListenerId++;
    handle.isActive = true;

    m_valueListeners[path].push_back({handle.listenerId, callback});

    // Start listening if this is the first listener for this path
    if (m_valueListeners[path].size() == 1) {
        startListening(path);
    }

    return handle;
}

ListenerHandle FirebaseRealtime::addChildListener(const std::string& path, ListenerCallback callback) {
    ListenerHandle handle;
    handle.path = path;
    handle.listenerId = m_nextListenerId++;
    handle.isActive = true;

    m_childListeners[path].push_back({handle.listenerId, callback});

    if (m_childListeners[path].size() == 1) {
        startListening(path);
    }

    return handle;
}

void FirebaseRealtime::removeListener(const ListenerHandle& handle) {
    // Remove from value listeners
    auto valueIt = m_valueListeners.find(handle.path);
    if (valueIt != m_valueListeners.end()) {
        auto& listeners = valueIt->second;
        listeners.erase(
            std::remove_if(listeners.begin(), listeners.end(),
                [&handle](const auto& pair) { return pair.first == handle.listenerId; }),
            listeners.end());

        if (listeners.empty()) {
            m_valueListeners.erase(valueIt);
            stopListening(handle.path);
        }
    }

    // Remove from child listeners
    auto childIt = m_childListeners.find(handle.path);
    if (childIt != m_childListeners.end()) {
        auto& listeners = childIt->second;
        listeners.erase(
            std::remove_if(listeners.begin(), listeners.end(),
                [&handle](const auto& pair) { return pair.first == handle.listenerId; }),
            listeners.end());

        if (listeners.empty()) {
            m_childListeners.erase(childIt);
            stopListening(handle.path);
        }
    }
}

void FirebaseRealtime::removeAllListeners(const std::string& path) {
    m_valueListeners.erase(path);
    m_childListeners.erase(path);
    stopListening(path);
}

void FirebaseRealtime::setPresence(PresenceState state, const std::string& activity) {
    auto& core = FirebaseCore::getInstance();
    if (!core.isSignedIn()) return;

    m_myPresence.oderId = core.getCurrentUser().uid;
    m_myPresence.displayName = core.getCurrentUser().displayName;
    m_myPresence.state = state;
    m_myPresence.lastSeen = std::chrono::system_clock::now();
    m_myPresence.currentActivity = activity;

    std::string path = "presence/" + m_myPresence.oderId;
    std::string json = serializePresence(m_myPresence);

    executeSet(path, json, nullptr);
}

void FirebaseRealtime::setPresenceOnDisconnect(PresenceState state) {
    auto& core = FirebaseCore::getInstance();
    if (!core.isSignedIn()) return;

    // REST API doesn't support onDisconnect directly
    // Would need to use server-side logic or Cloud Functions
    // For now, store the desired disconnect state
    PlayerPresence disconnectPresence = m_myPresence;
    disconnectPresence.state = state;
    disconnectPresence.lastSeen = std::chrono::system_clock::now();

    std::string path = "presence/" + m_myPresence.oderId + "/onDisconnect";
    std::string json = serializePresence(disconnectPresence);

    executeSet(path, json, nullptr);
}

void FirebaseRealtime::getPresence(const std::string& oderId,
    std::function<void(const PlayerPresence&, const FirebaseError&)> callback) {

    // Check cache first
    auto it = m_presenceCache.find(oderId);
    if (it != m_presenceCache.end()) {
        callback(it->second, FirebaseError());
        return;
    }

    std::string path = "presence/" + oderId;
    get(path, [this, oderId, callback](const RealtimeNode& node, const FirebaseError& error) {
        if (error) {
            callback(PlayerPresence(), error);
            return;
        }

        PlayerPresence presence = deserializePresence(serializeNode(node));
        m_presenceCache[oderId] = presence;
        callback(presence, FirebaseError());
    });
}

void FirebaseRealtime::watchPresence(const std::string& oderId, PresenceCallback callback) {
    m_presenceWatchers[oderId] = callback;

    std::string path = "presence/" + oderId;
    addValueListener(path, [this, oderId, callback](RealtimeEventType type, const RealtimeNode& node) {
        if (type == RealtimeEventType::ValueChanged) {
            PlayerPresence presence = deserializePresence(serializeNode(node));
            m_presenceCache[oderId] = presence;
            callback(presence);
        }
    });
}

void FirebaseRealtime::unwatchPresence(const std::string& oderId) {
    m_presenceWatchers.erase(oderId);
    removeAllListeners("presence/" + oderId);
}

void FirebaseRealtime::watchFriendsPresence(const std::vector<std::string>& friendIds,
                                             PresenceCallback callback) {
    for (const auto& friendId : friendIds) {
        watchPresence(friendId, callback);
    }
}

void FirebaseRealtime::getOnlineFriends(const std::vector<std::string>& friendIds,
    std::function<void(const std::vector<PlayerPresence>&)> callback) {

    auto results = std::make_shared<std::vector<PlayerPresence>>();
    auto remaining = std::make_shared<int>(static_cast<int>(friendIds.size()));

    if (friendIds.empty()) {
        callback({});
        return;
    }

    for (const auto& friendId : friendIds) {
        getPresence(friendId, [results, remaining, callback](const PlayerPresence& presence, const FirebaseError&) {
            if (presence.state != PresenceState::Offline) {
                results->push_back(presence);
            }

            (*remaining)--;
            if (*remaining == 0) {
                callback(*results);
            }
        });
    }
}

void FirebaseRealtime::createGameState(const std::string& matchId, GameStateCallback callback) {
    auto& core = FirebaseCore::getInstance();

    m_currentGameState = GameState();
    m_currentGameState.matchId = matchId;
    m_currentGameState.gameId = generateGameId();
    m_currentGameState.lastUpdate = std::chrono::system_clock::now();

    std::string path = "games/" + m_currentGameState.gameId;

    std::ostringstream json;
    json << "{"
         << "\"matchId\":\"" << matchId << "\","
         << "\"createdBy\":\"" << core.getCurrentUser().uid << "\","
         << "\"currentTurn\":0,"
         << "\"phase\":0,"
         << "\"timestamp\":{\".sv\":\"timestamp\"}"
         << "}";

    executeSet(path, json.str(), [this, callback](const RealtimeNode& node, const FirebaseError& error) {
        if (error) {
            callback(GameState());
            return;
        }

        m_inGameState = true;

        // Start listening for game state updates
        addValueListener("games/" + m_currentGameState.gameId,
            [this](RealtimeEventType type, const RealtimeNode& node) {
                // Update local game state
                for (auto& cb : m_gameStateCallbacks) {
                    cb(m_currentGameState);
                }
            });

        callback(m_currentGameState);
    });
}

void FirebaseRealtime::joinGameState(const std::string& gameId, GameStateCallback callback) {
    std::string path = "games/" + gameId;

    get(path, [this, gameId, callback](const RealtimeNode& node, const FirebaseError& error) {
        if (error) {
            callback(GameState());
            return;
        }

        m_currentGameState.gameId = gameId;
        m_inGameState = true;

        // Add player to game
        auto& core = FirebaseCore::getInstance();
        std::string playerPath = "games/" + gameId + "/players/" + core.getCurrentUser().uid;

        std::ostringstream json;
        json << "{"
             << "\"joined\":{\".sv\":\"timestamp\"},"
             << "\"displayName\":\"" << core.getCurrentUser().displayName << "\""
             << "}";

        executeSet(playerPath, json.str(), nullptr);

        // Start listening for game state updates
        addValueListener("games/" + gameId,
            [this](RealtimeEventType type, const RealtimeNode& node) {
                for (auto& cb : m_gameStateCallbacks) {
                    cb(m_currentGameState);
                }
            });

        callback(m_currentGameState);
    });
}

void FirebaseRealtime::leaveGameState() {
    if (!m_inGameState) return;

    auto& core = FirebaseCore::getInstance();
    std::string playerPath = "games/" + m_currentGameState.gameId + "/players/" + core.getCurrentUser().uid;

    remove(playerPath);
    removeAllListeners("games/" + m_currentGameState.gameId);

    m_inGameState = false;
    m_currentGameState = GameState();
}

void FirebaseRealtime::updateMyState(const RealtimeNode& state) {
    if (!m_inGameState) return;

    auto& core = FirebaseCore::getInstance();
    std::string path = "games/" + m_currentGameState.gameId + "/playerStates/" + core.getCurrentUser().uid;

    set(path, state);
}

void FirebaseRealtime::updateSharedState(const std::string& key, const RealtimeNode& value) {
    if (!m_inGameState) return;

    std::string path = "games/" + m_currentGameState.gameId + "/sharedState/" + key;
    set(path, value);
}

void FirebaseRealtime::broadcastAction(const std::string& action, const RealtimeNode& data) {
    if (!m_inGameState) return;

    auto& core = FirebaseCore::getInstance();
    std::string path = "games/" + m_currentGameState.gameId + "/actions";

    RealtimeNode actionNode;
    std::ostringstream json;
    json << "{"
         << "\"playerId\":\"" << core.getCurrentUser().uid << "\","
         << "\"action\":\"" << action << "\","
         << "\"data\":" << serializeNode(data) << ","
         << "\"timestamp\":{\".sv\":\"timestamp\"}"
         << "}";

    executePush(path, json.str(), nullptr);
}

void FirebaseRealtime::onGameStateUpdate(GameStateCallback callback) {
    m_gameStateCallbacks.push_back(callback);
}

void FirebaseRealtime::onPlayerAction(
    std::function<void(const std::string&, const std::string&, const RealtimeNode&)> callback) {
    m_actionCallbacks.push_back(callback);
}

void FirebaseRealtime::startSession() {
    auto& core = FirebaseCore::getInstance();
    if (!core.isSignedIn()) return;

    m_currentSession.sessionId = generateSessionId();
    m_currentSession.oderId = core.getCurrentUser().uid;
    m_currentSession.startTime = std::chrono::system_clock::now();
    m_currentSession.lastActivity = m_currentSession.startTime;

    #ifdef _WIN32
    m_currentSession.platform = "windows";
    #elif __APPLE__
    m_currentSession.platform = "macos";
    #elif __linux__
    m_currentSession.platform = "linux";
    #elif __ANDROID__
    m_currentSession.platform = "android";
    #elif __EMSCRIPTEN__
    m_currentSession.platform = "web";
    #else
    m_currentSession.platform = "unknown";
    #endif

    std::string path = "sessions/" + m_currentSession.sessionId;

    std::ostringstream json;
    json << "{"
         << "\"playerId\":\"" << m_currentSession.oderId << "\","
         << "\"platform\":\"" << m_currentSession.platform << "\","
         << "\"startTime\":{\".sv\":\"timestamp\"}"
         << "}";

    executeSet(path, json.str(), nullptr);
    m_sessionActive = true;

    // Update player's active session reference
    std::string playerPath = "players/" + m_currentSession.oderId + "/activeSession";
    RealtimeNode sessionRef;
    sessionRef.value = m_currentSession.sessionId;
    set(playerPath, sessionRef);
}

void FirebaseRealtime::endSession() {
    if (!m_sessionActive) return;

    std::string path = "sessions/" + m_currentSession.sessionId;

    // Update session end time
    std::ostringstream json;
    json << "{\"endTime\":{\".sv\":\"timestamp\"}}";
    executeUpdate(path, json.str(), nullptr);

    // Remove active session reference
    auto& core = FirebaseCore::getInstance();
    remove("players/" + core.getCurrentUser().uid + "/activeSession");

    m_sessionActive = false;
}

void FirebaseRealtime::updateSessionData(const std::string& key, const std::string& value) {
    if (!m_sessionActive) return;

    m_currentSession.data[key] = value;
    m_currentSession.lastActivity = std::chrono::system_clock::now();

    std::string path = "sessions/" + m_currentSession.sessionId + "/data/" + key;
    RealtimeNode node;
    node.value = value;
    set(path, node);
}

void FirebaseRealtime::getActiveSessionCount(std::function<void(int count)> callback) {
    // Query sessions with no endTime
    get("sessions", [callback](const RealtimeNode& node, const FirebaseError& error) {
        if (error || !node.isObject()) {
            callback(0);
            return;
        }

        int count = 0;
        const auto& sessions = std::get<std::unordered_map<std::string, std::shared_ptr<RealtimeNode>>>(node.value);
        for (const auto& [key, session] : sessions) {
            // Count sessions without endTime (active)
            // Simplified - would need proper JSON parsing
            count++;
        }
        callback(count);
    });
}

void FirebaseRealtime::submitScore(const std::string& leaderboardId, int64_t score,
    const std::unordered_map<std::string, std::string>& metadata,
    std::function<void(int rank, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();
    if (!core.isSignedIn()) {
        if (callback) {
            FirebaseError error;
            error.type = FirebaseErrorType::AuthError;
            callback(-1, error);
        }
        return;
    }

    std::string path = "leaderboards/" + leaderboardId + "/scores/" + core.getCurrentUser().uid;

    std::ostringstream json;
    json << "{"
         << "\"playerId\":\"" << core.getCurrentUser().uid << "\","
         << "\"displayName\":\"" << core.getCurrentUser().displayName << "\","
         << "\"score\":" << score << ","
         << "\"timestamp\":{\".sv\":\"timestamp\"}";

    if (!metadata.empty()) {
        json << ",\"metadata\":{";
        bool first = true;
        for (const auto& [key, value] : metadata) {
            if (!first) json << ",";
            first = false;
            json << "\"" << key << "\":\"" << value << "\"";
        }
        json << "}";
    }

    json << "}";

    executeSet(path, json.str(), [this, leaderboardId, callback](const RealtimeNode&, const FirebaseError& error) {
        if (error) {
            if (callback) callback(-1, error);
            return;
        }

        // Get rank
        if (callback) {
            getMyRank(leaderboardId, [callback](const LeaderboardEntry& entry, const FirebaseError& err) {
                callback(entry.rank, err);
            });
        }
    });
}

void FirebaseRealtime::getLeaderboard(const std::string& leaderboardId, int count, int offset,
                                       LeaderboardCallback callback) {
    // REST API query with orderBy and limitTo
    std::string path = "leaderboards/" + leaderboardId + "/scores";
    std::string url = buildUrl(path) +
        "&orderBy=\"score\"&limitToLast=" + std::to_string(count);

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "GET";
    request.url = url;

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode != 200) {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            callback({}, error);
            return;
        }

        std::vector<LeaderboardEntry> entries = parseLeaderboardEntries(response.body);

        // Assign ranks (descending order)
        for (size_t i = 0; i < entries.size(); i++) {
            entries[i].rank = static_cast<int>(i + 1);
        }

        callback(entries, FirebaseError());
    });
}

void FirebaseRealtime::getMyRank(const std::string& leaderboardId,
    std::function<void(const LeaderboardEntry&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();
    std::string oderId = core.getCurrentUser().uid;

    // Get my score first
    std::string path = "leaderboards/" + leaderboardId + "/scores/" + oderId;
    get(path, [this, leaderboardId, oderId, callback](const RealtimeNode& node, const FirebaseError& error) {
        if (error) {
            callback(LeaderboardEntry(), error);
            return;
        }

        // Count how many scores are higher
        std::string countPath = "leaderboards/" + leaderboardId + "/scores";
        std::string url = buildUrl(countPath);

        // Would need to count entries with higher scores
        // Simplified - return estimated rank
        LeaderboardEntry entry;
        entry.oderId = oderId;
        entry.rank = 1;  // Simplified
        callback(entry, FirebaseError());
    });
}

void FirebaseRealtime::getAroundMe(const std::string& leaderboardId, int count,
                                    LeaderboardCallback callback) {
    // Get full leaderboard and find player's position
    getLeaderboard(leaderboardId, 1000, 0, [this, count, callback](
        const std::vector<LeaderboardEntry>& entries, const FirebaseError& error) {

        if (error) {
            callback({}, error);
            return;
        }

        auto& core = FirebaseCore::getInstance();
        std::string myId = core.getCurrentUser().uid;

        // Find my position
        int myIndex = -1;
        for (size_t i = 0; i < entries.size(); i++) {
            if (entries[i].oderId == myId) {
                myIndex = static_cast<int>(i);
                break;
            }
        }

        if (myIndex < 0) {
            callback({}, FirebaseError());
            return;
        }

        // Get surrounding entries
        int halfCount = count / 2;
        int start = std::max(0, myIndex - halfCount);
        int end = std::min(static_cast<int>(entries.size()), myIndex + halfCount + 1);

        std::vector<LeaderboardEntry> result(entries.begin() + start, entries.begin() + end);
        callback(result, FirebaseError());
    });
}

void FirebaseRealtime::getFriendsLeaderboard(const std::string& leaderboardId,
    const std::vector<std::string>& friendIds, LeaderboardCallback callback) {

    auto results = std::make_shared<std::vector<LeaderboardEntry>>();
    auto remaining = std::make_shared<int>(static_cast<int>(friendIds.size()));

    if (friendIds.empty()) {
        callback({}, FirebaseError());
        return;
    }

    for (const auto& friendId : friendIds) {
        std::string path = "leaderboards/" + leaderboardId + "/scores/" + friendId;
        get(path, [results, remaining, friendId, callback](const RealtimeNode& node, const FirebaseError&) {
            // Parse entry if exists
            if (!node.isNull()) {
                LeaderboardEntry entry;
                entry.oderId = friendId;
                // Parse score from node
                results->push_back(entry);
            }

            (*remaining)--;
            if (*remaining == 0) {
                // Sort by score
                std::sort(results->begin(), results->end(),
                    [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
                        return a.score > b.score;
                    });

                // Assign ranks
                for (size_t i = 0; i < results->size(); i++) {
                    (*results)[i].rank = static_cast<int>(i + 1);
                }

                callback(*results, FirebaseError());
            }
        });
    }
}

void FirebaseRealtime::watchLeaderboard(const std::string& leaderboardId, int count,
                                         LeaderboardCallback callback) {
    m_leaderboardWatchers[leaderboardId] = callback;

    std::string path = "leaderboards/" + leaderboardId + "/scores";
    addValueListener(path, [this, leaderboardId, count, callback](RealtimeEventType, const RealtimeNode&) {
        getLeaderboard(leaderboardId, count, 0, callback);
    });
}

void FirebaseRealtime::unwatchLeaderboard(const std::string& leaderboardId) {
    m_leaderboardWatchers.erase(leaderboardId);
    removeAllListeners("leaderboards/" + leaderboardId + "/scores");
}

void FirebaseRealtime::getServerTime(std::function<void(std::chrono::system_clock::time_point)> callback) {
    // Write server timestamp and read it back
    std::string path = "serverTime/test";

    executeSet(path, "{\".sv\":\"timestamp\"}", [this, path, callback](const RealtimeNode&, const FirebaseError&) {
        get(path, [callback](const RealtimeNode& node, const FirebaseError&) {
            if (node.isInt()) {
                int64_t timestamp = node.getInt();
                auto serverTime = std::chrono::system_clock::from_time_t(timestamp / 1000);
                callback(serverTime);
            } else {
                callback(std::chrono::system_clock::now());
            }
        });
    });
}

void FirebaseRealtime::onConnectionStateChanged(std::function<void(bool)> callback) {
    m_connectionCallbacks.push_back(callback);
}

// Private methods

void FirebaseRealtime::executeGet(const std::string& path, RealtimeCallback callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "GET";
    request.url = buildUrl(path);

    core.makeAuthenticatedRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            RealtimeNode node = deserializeNode(response.body);
            if (callback) callback(node, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            if (callback) callback(RealtimeNode(), error);
        }
    });
}

void FirebaseRealtime::executeSet(const std::string& path, const std::string& json,
                                   RealtimeCallback callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "PUT";
    request.url = buildUrl(path);
    request.headers["Content-Type"] = "application/json";
    request.body = json;

    core.makeAuthenticatedRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            RealtimeNode node = deserializeNode(response.body);
            if (callback) callback(node, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            if (callback) callback(RealtimeNode(), error);
        }
    });
}

void FirebaseRealtime::executeUpdate(const std::string& path, const std::string& json,
                                      RealtimeCallback callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "PATCH";
    request.url = buildUrl(path);
    request.headers["Content-Type"] = "application/json";
    request.body = json;

    core.makeAuthenticatedRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            RealtimeNode node = deserializeNode(response.body);
            if (callback) callback(node, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            if (callback) callback(RealtimeNode(), error);
        }
    });
}

void FirebaseRealtime::executePush(const std::string& path, const std::string& json,
                                    RealtimeCallback callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = buildUrl(path);
    request.headers["Content-Type"] = "application/json";
    request.body = json;

    core.makeAuthenticatedRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            RealtimeNode node = deserializeNode(response.body);
            if (callback) callback(node, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            if (callback) callback(RealtimeNode(), error);
        }
    });
}

void FirebaseRealtime::executeDelete(const std::string& path,
                                      std::function<void(const FirebaseError&)> callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "DELETE";
    request.url = buildUrl(path);

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200 || response.statusCode == 204) {
            if (callback) callback(FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            if (callback) callback(error);
        }
    });
}

void FirebaseRealtime::processListenerUpdates() {
    // Poll all listened paths for changes
    for (const auto& [path, listeners] : m_valueListeners) {
        get(path, [this, path](const RealtimeNode& node, const FirebaseError& error) {
            if (error) return;

            // Check if value changed
            auto lastIt = m_lastValues.find(path);
            bool changed = lastIt == m_lastValues.end() ||
                           serializeNode(lastIt->second) != serializeNode(node);

            if (changed) {
                m_lastValues[path] = node;

                auto it = m_valueListeners.find(path);
                if (it != m_valueListeners.end()) {
                    for (const auto& [id, callback] : it->second) {
                        callback(RealtimeEventType::ValueChanged, node);
                    }
                }
            }
        });
    }
}

void FirebaseRealtime::startListening(const std::string& path) {
    // With REST API, we poll - nothing special to start
}

void FirebaseRealtime::stopListening(const std::string& path) {
    m_lastValues.erase(path);
}

void FirebaseRealtime::setupPresenceSystem() {
    auto& core = FirebaseCore::getInstance();
    if (!core.isSignedIn()) return;

    // Set initial presence
    setPresence(PresenceState::Online);

    // Set offline presence on disconnect
    setPresenceOnDisconnect(PresenceState::Offline);
}

void FirebaseRealtime::updatePresenceTimestamp() {
    if (m_myPresence.state == PresenceState::Offline) return;

    m_myPresence.lastSeen = std::chrono::system_clock::now();

    std::string path = "presence/" + m_myPresence.oderId + "/lastSeen";
    RealtimeNode node;
    // Use server timestamp
    executeSet(path, "{\".sv\":\"timestamp\"}", nullptr);
}

void FirebaseRealtime::handleDisconnect() {
    m_isConnected = false;

    for (auto& callback : m_connectionCallbacks) {
        callback(false);
    }
}

std::string FirebaseRealtime::serializeNode(const RealtimeNode& node) {
    if (node.isNull()) return "null";
    if (node.isBool()) return node.getBool() ? "true" : "false";
    if (node.isInt()) return std::to_string(node.getInt());
    if (node.isDouble()) return std::to_string(node.getDouble());
    if (node.isString()) return "\"" + node.getString() + "\"";

    if (node.isArray()) {
        std::ostringstream json;
        json << "[";
        const auto& arr = std::get<std::vector<std::shared_ptr<RealtimeNode>>>(node.value);
        bool first = true;
        for (const auto& item : arr) {
            if (!first) json << ",";
            first = false;
            json << serializeNode(*item);
        }
        json << "]";
        return json.str();
    }

    if (node.isObject()) {
        std::ostringstream json;
        json << "{";
        const auto& obj = std::get<std::unordered_map<std::string, std::shared_ptr<RealtimeNode>>>(node.value);
        bool first = true;
        for (const auto& [key, value] : obj) {
            if (!first) json << ",";
            first = false;
            json << "\"" << key << "\":" << serializeNode(*value);
        }
        json << "}";
        return json.str();
    }

    return "null";
}

RealtimeNode FirebaseRealtime::deserializeNode(const std::string& json) {
    RealtimeNode node;
    // Simplified JSON parsing - in production use proper JSON library
    if (json.empty() || json == "null") {
        node.value = nullptr;
    } else if (json == "true") {
        node.value = true;
    } else if (json == "false") {
        node.value = false;
    } else if (json[0] == '"') {
        node.value = json.substr(1, json.length() - 2);
    } else if (json[0] == '{' || json[0] == '[') {
        // Object or array - simplified
        node.value = json;  // Store as string for now
    } else {
        // Number
        if (json.find('.') != std::string::npos) {
            node.value = std::stod(json);
        } else {
            node.value = static_cast<int64_t>(std::stoll(json));
        }
    }
    return node;
}

std::string FirebaseRealtime::serializePresence(const PlayerPresence& presence) {
    std::ostringstream json;
    json << "{"
         << "\"playerId\":\"" << presence.oderId << "\","
         << "\"displayName\":\"" << presence.displayName << "\","
         << "\"state\":" << static_cast<int>(presence.state) << ","
         << "\"activity\":\"" << presence.currentActivity << "\"";

    if (!presence.matchId.empty()) {
        json << ",\"matchId\":\"" << presence.matchId << "\"";
    }
    if (!presence.lobbyId.empty()) {
        json << ",\"lobbyId\":\"" << presence.lobbyId << "\"";
    }

    json << ",\"lastSeen\":{\".sv\":\"timestamp\"}"
         << "}";

    return json.str();
}

PlayerPresence FirebaseRealtime::deserializePresence(const std::string& json) {
    PlayerPresence presence;
    // Parse JSON - simplified
    return presence;
}

std::string FirebaseRealtime::buildUrl(const std::string& path, bool withAuth) {
    auto& core = FirebaseCore::getInstance();
    std::string url = core.getConfig().getRealtimeDbUrl() + "/" + path + ".json";

    if (withAuth && !core.getIdToken().empty()) {
        url += "?auth=" + core.getIdToken();
    }

    return url;
}

// Helper functions
namespace {

std::string generateGameId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id = "game_";
    for (int i = 0; i < 16; i++) {
        id += hex[dis(gen)];
    }
    return id;
}

std::string generateSessionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id = "session_";
    for (int i = 0; i < 20; i++) {
        id += hex[dis(gen)];
    }
    return id;
}

std::vector<LeaderboardEntry> parseLeaderboardEntries(const std::string& json) {
    std::vector<LeaderboardEntry> entries;
    // Parse JSON array - simplified
    return entries;
}

} // anonymous namespace

} // namespace Firebase
} // namespace Network
