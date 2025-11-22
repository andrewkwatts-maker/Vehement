#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <queue>
#include <nlohmann/json.hpp>

namespace Vehement {

/**
 * @brief Firebase SDK wrapper for Vehement2 game
 *
 * Provides a unified interface for Firebase services:
 * - Anonymous authentication
 * - Realtime Database for game state
 * - Cloud Firestore for persistent town data
 * - Error handling and automatic reconnection
 *
 * This is a stub implementation that works offline.
 * When Firebase SDK is available, implement the actual calls.
 */
class FirebaseManager {
public:
    /**
     * @brief Error codes for Firebase operations
     */
    enum class ErrorCode {
        None = 0,
        NotInitialized,
        AuthenticationFailed,
        NetworkError,
        PermissionDenied,
        NotFound,
        InvalidData,
        Timeout,
        Unknown
    };

    /**
     * @brief Result structure for async operations
     */
    struct Result {
        bool success = false;
        ErrorCode error = ErrorCode::None;
        std::string errorMessage;
    };

    /**
     * @brief Firebase project configuration
     */
    struct FirebaseConfig {
        std::string projectId;
        std::string apiKey;
        std::string authDomain;
        std::string databaseUrl;
        std::string storageBucket;
        std::string messagingSenderId;
        std::string appId;
    };

    /**
     * @brief Connection state for monitoring
     */
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting
    };

    // Callback types
    using AuthCallback = std::function<void(bool success, const std::string& userId)>;
    using DataCallback = std::function<void(const nlohmann::json& data)>;
    using ResultCallback = std::function<void(const Result& result)>;
    using ConnectionCallback = std::function<void(ConnectionState state)>;

    /**
     * @brief Get the singleton instance
     * @return Reference to the FirebaseManager instance
     */
    [[nodiscard]] static FirebaseManager& Instance();

    // Delete copy/move - singleton pattern
    FirebaseManager(const FirebaseManager&) = delete;
    FirebaseManager& operator=(const FirebaseManager&) = delete;
    FirebaseManager(FirebaseManager&&) = delete;
    FirebaseManager& operator=(FirebaseManager&&) = delete;

    /**
     * @brief Initialize Firebase with configuration file
     * @param configPath Path to firebase config JSON file
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(const std::string& configPath);

    /**
     * @brief Initialize Firebase with configuration struct
     * @param config Firebase configuration
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(const FirebaseConfig& config);

    /**
     * @brief Shutdown Firebase and clean up resources
     */
    void Shutdown();

    /**
     * @brief Check if Firebase is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Get current connection state
     */
    [[nodiscard]] ConnectionState GetConnectionState() const noexcept { return m_connectionState; }

    /**
     * @brief Register callback for connection state changes
     * @param callback Function to call when connection state changes
     */
    void OnConnectionStateChanged(ConnectionCallback callback);

    // ==================== Authentication ====================

    /**
     * @brief Sign in anonymously (for guest users)
     * @param callback Function to call with result (success, userId)
     */
    void SignInAnonymously(AuthCallback callback);

    /**
     * @brief Sign out the current user
     */
    void SignOut();

    /**
     * @brief Check if a user is currently signed in
     */
    [[nodiscard]] bool IsSignedIn() const noexcept { return !m_userId.empty(); }

    /**
     * @brief Get the current user's ID
     * @return User ID or empty string if not signed in
     */
    [[nodiscard]] std::string GetUserId() const;

    // ==================== Realtime Database ====================

    /**
     * @brief Set a value at the specified path
     * @param path Database path (e.g., "towns/melbourne-au/players/abc123")
     * @param value JSON value to store
     * @param callback Optional callback for completion notification
     */
    void SetValue(const std::string& path, const nlohmann::json& value,
                  ResultCallback callback = nullptr);

    /**
     * @brief Get a value from the specified path (one-time read)
     * @param path Database path
     * @param callback Function to call with the data
     */
    void GetValue(const std::string& path, DataCallback callback);

    /**
     * @brief Update specific fields at the specified path (merge)
     * @param path Database path
     * @param updates JSON object with fields to update
     * @param callback Optional callback for completion notification
     */
    void UpdateValue(const std::string& path, const nlohmann::json& updates,
                     ResultCallback callback = nullptr);

    /**
     * @brief Delete data at the specified path
     * @param path Database path
     * @param callback Optional callback for completion notification
     */
    void DeleteValue(const std::string& path, ResultCallback callback = nullptr);

    /**
     * @brief Push a new child with auto-generated key
     * @param path Parent path
     * @param value Value to push
     * @return The generated key for the new child
     */
    std::string PushValue(const std::string& path, const nlohmann::json& value);

    // ==================== Real-time Listeners ====================

    /**
     * @brief Start listening for changes at the specified path
     * @param path Database path to listen to
     * @param callback Function to call when data changes
     * @return Listener ID for later removal
     */
    std::string ListenToPath(const std::string& path, DataCallback callback);

    /**
     * @brief Stop listening to a specific path
     * @param path Database path to stop listening to
     */
    void StopListening(const std::string& path);

    /**
     * @brief Stop a listener by its ID
     * @param listenerId ID returned from ListenToPath
     */
    void StopListeningById(const std::string& listenerId);

    /**
     * @brief Stop all active listeners
     */
    void StopAllListeners();

    // ==================== Cloud Firestore ====================

    /**
     * @brief Get a document from Firestore
     * @param collection Collection name
     * @param documentId Document ID
     * @param callback Function to call with document data
     */
    void GetDocument(const std::string& collection, const std::string& documentId,
                     DataCallback callback);

    /**
     * @brief Set a document in Firestore
     * @param collection Collection name
     * @param documentId Document ID
     * @param data Document data
     * @param merge If true, merge with existing data
     * @param callback Optional callback for completion notification
     */
    void SetDocument(const std::string& collection, const std::string& documentId,
                     const nlohmann::json& data, bool merge = false,
                     ResultCallback callback = nullptr);

    /**
     * @brief Query documents from Firestore collection
     * @param collection Collection name
     * @param field Field to query on
     * @param op Comparison operator ("==", "<", ">", "<=", ">=", "!=")
     * @param value Value to compare
     * @param callback Function to call with query results
     */
    void QueryDocuments(const std::string& collection,
                        const std::string& field,
                        const std::string& op,
                        const nlohmann::json& value,
                        DataCallback callback);

    // ==================== Offline Support ====================

    /**
     * @brief Enable offline persistence
     */
    void EnableOfflineMode();

    /**
     * @brief Check if currently in offline mode
     */
    [[nodiscard]] bool IsOffline() const noexcept { return m_offlineMode; }

    /**
     * @brief Process pending operations (call from main loop)
     */
    void Update();

private:
    FirebaseManager() = default;
    ~FirebaseManager();

    // Configuration
    FirebaseConfig m_config;
    bool m_initialized = false;
    std::atomic<ConnectionState> m_connectionState{ConnectionState::Disconnected};
    bool m_offlineMode = false;

    // Authentication state
    std::string m_userId;
    mutable std::mutex m_authMutex;

    // Listeners
    struct Listener {
        std::string path;
        DataCallback callback;
        bool active = true;
    };
    std::unordered_map<std::string, Listener> m_listeners;
    std::mutex m_listenerMutex;
    uint64_t m_nextListenerId = 1;

    // Connection callbacks
    std::vector<ConnectionCallback> m_connectionCallbacks;
    std::mutex m_connectionCallbackMutex;

    // Offline data storage (stub implementation)
    std::unordered_map<std::string, nlohmann::json> m_localData;
    std::mutex m_dataMutex;

    // Pending operations queue for offline support
    struct PendingOperation {
        enum class Type { Set, Update, Delete, Push } type;
        std::string path;
        nlohmann::json data;
        ResultCallback callback;
    };
    std::queue<PendingOperation> m_pendingOperations;
    std::mutex m_pendingMutex;

    // Helper functions
    void NotifyConnectionStateChanged(ConnectionState state);
    void NotifyListeners(const std::string& path, const nlohmann::json& data);
    std::string GenerateUniqueId();
    bool LoadConfig(const std::string& configPath);
    void SimulateNetworkDelay(std::function<void()> operation);
};

} // namespace Vehement
