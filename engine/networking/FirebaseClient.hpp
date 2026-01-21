#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <future>
#include <nlohmann/json.hpp>

namespace Nova {

/**
 * @brief Firebase authentication state
 */
enum class FirebaseAuthState {
    NotAuthenticated,
    Authenticating,
    Authenticated,
    Error
};

/**
 * @brief Firebase operation result
 */
struct FirebaseResult {
    bool success = false;
    std::string errorMessage;
    nlohmann::json data;
    int httpCode = 0;
};

/**
 * @brief Firebase realtime database listener
 */
struct FirebaseListener {
    uint64_t id = 0;
    std::string path;
    std::function<void(const nlohmann::json&)> onValue;
    std::function<void(const std::string&, const nlohmann::json&)> onChildAdded;
    std::function<void(const std::string&, const nlohmann::json&)> onChildChanged;
    std::function<void(const std::string&)> onChildRemoved;
    std::function<void(const std::string&)> onError;
    bool active = true;
};

/**
 * @brief Pending Firebase operation
 */
struct FirebaseOperation {
    enum class Type {
        Get,
        Set,
        Update,
        Push,
        Delete,
        Transaction
    };

    Type type;
    std::string path;
    nlohmann::json data;
    std::function<void(const FirebaseResult&)> callback;
    std::function<nlohmann::json(const nlohmann::json&)> transactionFunc;
    int retryCount = 0;
    static constexpr int maxRetries = 3;
};

/**
 * @brief Firebase Realtime Database client
 *
 * Features:
 * - REST API for database operations
 * - Real-time listeners (polling-based simulation)
 * - Offline queue for operations
 * - Automatic retry on failure
 * - Terrain change persistence
 */
class FirebaseClient {
public:
    struct Config {
        std::string projectId;
        std::string apiKey;
        std::string databaseUrl;
        std::string authDomain;
        float pollInterval = 5.0f;      // Seconds between polls for listeners
        int maxQueueSize = 1000;
        bool offlineEnabled = true;
        std::string offlineCachePath = "firebase_cache.json";
    };

    FirebaseClient();
    ~FirebaseClient();

    /**
     * @brief Initialize Firebase client
     */
    bool Initialize(const Config& config);

    /**
     * @brief Shutdown client
     */
    void Shutdown();

    /**
     * @brief Update client (process queue, poll listeners)
     */
    void Update(float deltaTime);

    // =========================================================================
    // Authentication
    // =========================================================================

    /**
     * @brief Sign in anonymously
     */
    void SignInAnonymously(std::function<void(bool, const std::string&)> callback);

    /**
     * @brief Sign in with email/password
     */
    void SignInWithEmail(const std::string& email, const std::string& password,
                         std::function<void(bool, const std::string&)> callback);

    /**
     * @brief Sign in with custom token
     */
    void SignInWithCustomToken(const std::string& token,
                               std::function<void(bool, const std::string&)> callback);

    /**
     * @brief Sign out
     */
    void SignOut();

    /**
     * @brief Get authentication state
     */
    [[nodiscard]] FirebaseAuthState GetAuthState() const { return m_authState; }

    /**
     * @brief Get current user ID
     */
    [[nodiscard]] const std::string& GetUserId() const { return m_userId; }

    /**
     * @brief Get ID token for authenticated requests
     */
    [[nodiscard]] const std::string& GetIdToken() const { return m_idToken; }

    // =========================================================================
    // Database Operations
    // =========================================================================

    /**
     * @brief Get data at path
     */
    void Get(const std::string& path, std::function<void(const FirebaseResult&)> callback);

    /**
     * @brief Set data at path (overwrites)
     */
    void Set(const std::string& path, const nlohmann::json& data,
             std::function<void(const FirebaseResult&)> callback = nullptr);

    /**
     * @brief Update data at path (merges)
     */
    void Update(const std::string& path, const nlohmann::json& data,
                std::function<void(const FirebaseResult&)> callback = nullptr);

    /**
     * @brief Push new child to path
     */
    void Push(const std::string& path, const nlohmann::json& data,
              std::function<void(const FirebaseResult&)> callback = nullptr);

    /**
     * @brief Delete data at path
     */
    void Delete(const std::string& path, std::function<void(const FirebaseResult&)> callback = nullptr);

    /**
     * @brief Run transaction
     */
    void Transaction(const std::string& path,
                     std::function<nlohmann::json(const nlohmann::json&)> updateFunc,
                     std::function<void(const FirebaseResult&)> callback = nullptr);

    // =========================================================================
    // Listeners
    // =========================================================================

    /**
     * @brief Add listener for value changes
     * @return Listener ID
     */
    uint64_t AddValueListener(const std::string& path,
                              std::function<void(const nlohmann::json&)> callback);

    /**
     * @brief Add listener for child events
     * @return Listener ID
     */
    uint64_t AddChildListener(const std::string& path,
                              std::function<void(const std::string&, const nlohmann::json&)> onAdded,
                              std::function<void(const std::string&, const nlohmann::json&)> onChanged,
                              std::function<void(const std::string&)> onRemoved);

    /**
     * @brief Remove listener
     */
    void RemoveListener(uint64_t listenerId);

    // =========================================================================
    // Terrain Persistence
    // =========================================================================

    /**
     * @brief Save terrain chunk
     */
    void SaveTerrainChunk(const std::string& worldId, int chunkX, int chunkY, int chunkZ,
                          const nlohmann::json& chunkData,
                          std::function<void(bool)> callback = nullptr);

    /**
     * @brief Load terrain chunk
     */
    void LoadTerrainChunk(const std::string& worldId, int chunkX, int chunkY, int chunkZ,
                          std::function<void(bool, const nlohmann::json&)> callback);

    /**
     * @brief Save terrain modification event
     */
    void SaveTerrainModification(const std::string& worldId,
                                  const nlohmann::json& modification,
                                  std::function<void(bool)> callback = nullptr);

    /**
     * @brief Load terrain modifications since timestamp
     */
    void LoadTerrainModifications(const std::string& worldId, uint64_t sinceTimestamp,
                                   std::function<void(const std::vector<nlohmann::json>&)> callback);

    /**
     * @brief Subscribe to terrain modifications
     * @return Subscription ID
     */
    uint64_t SubscribeToTerrainModifications(const std::string& worldId,
                                              std::function<void(const nlohmann::json&)> callback);

    // =========================================================================
    // Offline Support
    // =========================================================================

    /**
     * @brief Check if online
     */
    [[nodiscard]] bool IsOnline() const { return m_online; }

    /**
     * @brief Get pending operation count
     */
    [[nodiscard]] size_t GetPendingOperationCount() const;

    /**
     * @brief Force sync offline operations
     */
    void SyncOfflineOperations();

    /**
     * @brief Clear offline cache
     */
    void ClearOfflineCache();

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get server timestamp
     */
    static nlohmann::json ServerTimestamp();

    /**
     * @brief Generate push ID
     */
    std::string GeneratePushId();

private:
    // HTTP operations (would use actual HTTP library in production)
    void HttpGet(const std::string& url, std::function<void(int, const std::string&)> callback);
    void HttpPost(const std::string& url, const std::string& body,
                  std::function<void(int, const std::string&)> callback);
    void HttpPut(const std::string& url, const std::string& body,
                 std::function<void(int, const std::string&)> callback);
    void HttpPatch(const std::string& url, const std::string& body,
                   std::function<void(int, const std::string&)> callback);
    void HttpDelete(const std::string& url, std::function<void(int, const std::string&)> callback);

    // URL building
    std::string BuildDatabaseUrl(const std::string& path) const;
    std::string BuildAuthUrl(const std::string& endpoint) const;

    // Queue processing
    void ProcessOperationQueue();
    void QueueOperation(FirebaseOperation op);
    void RetryOperation(FirebaseOperation& op);

    // Listener polling
    void PollListeners();

    // Token refresh
    void RefreshToken();

    // Offline cache
    void SaveToOfflineCache();
    void LoadFromOfflineCache();

    Config m_config;
    bool m_initialized = false;
    bool m_online = false;

    // Authentication
    FirebaseAuthState m_authState = FirebaseAuthState::NotAuthenticated;
    std::string m_userId;
    std::string m_idToken;
    std::string m_refreshToken;
    uint64_t m_tokenExpiry = 0;

    // Operation queue
    std::queue<FirebaseOperation> m_operationQueue;
    mutable std::mutex m_queueMutex;

    // Listeners
    std::vector<FirebaseListener> m_listeners;
    std::unordered_map<std::string, nlohmann::json> m_listenerCache;  // Last known values
    uint64_t m_nextListenerId = 1;
    float m_pollTimer = 0.0f;

    // Offline cache
    std::unordered_map<std::string, nlohmann::json> m_offlineCache;
    std::queue<FirebaseOperation> m_offlineQueue;

    // Push ID generation
    std::string m_lastPushId;
    int m_pushIdCounter = 0;
};

/**
 * @brief Helper class for building Firebase queries
 */
class FirebaseQuery {
public:
    FirebaseQuery(FirebaseClient* client, const std::string& path);

    FirebaseQuery& OrderByChild(const std::string& child);
    FirebaseQuery& OrderByKey();
    FirebaseQuery& OrderByValue();
    FirebaseQuery& StartAt(const nlohmann::json& value);
    FirebaseQuery& EndAt(const nlohmann::json& value);
    FirebaseQuery& EqualTo(const nlohmann::json& value);
    FirebaseQuery& LimitToFirst(int limit);
    FirebaseQuery& LimitToLast(int limit);

    void Get(std::function<void(const FirebaseResult&)> callback);
    uint64_t Listen(std::function<void(const nlohmann::json&)> callback);

private:
    std::string BuildQueryString() const;

    FirebaseClient* m_client;
    std::string m_path;

    std::string m_orderBy;
    std::optional<nlohmann::json> m_startAt;
    std::optional<nlohmann::json> m_endAt;
    std::optional<nlohmann::json> m_equalTo;
    int m_limitToFirst = 0;
    int m_limitToLast = 0;
};

} // namespace Nova
