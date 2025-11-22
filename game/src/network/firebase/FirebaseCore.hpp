#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <atomic>
#include <queue>

namespace Network {
namespace Firebase {

// Forward declarations
class FirebaseCore;

// Authentication provider types
enum class AuthProvider {
    Anonymous,
    Email,
    Google,
    Apple,
    Facebook,
    GameCenter,
    PlayGames
};

// Connection states
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    AuthRequired,
    Error
};

// Error types for Firebase operations
enum class FirebaseErrorType {
    None,
    NetworkError,
    AuthError,
    PermissionDenied,
    NotFound,
    AlreadyExists,
    InvalidArgument,
    Timeout,
    RateLimited,
    ServerError,
    Unknown
};

// Firebase error with details
struct FirebaseError {
    FirebaseErrorType type = FirebaseErrorType::None;
    int code = 0;
    std::string message;
    std::string details;

    bool isRetryable() const {
        return type == FirebaseErrorType::NetworkError ||
               type == FirebaseErrorType::Timeout ||
               type == FirebaseErrorType::RateLimited ||
               type == FirebaseErrorType::ServerError;
    }

    operator bool() const { return type != FirebaseErrorType::None; }
};

// User information from authentication
struct FirebaseUser {
    std::string uid;
    std::string email;
    std::string displayName;
    std::string photoUrl;
    AuthProvider provider = AuthProvider::Anonymous;
    bool isAnonymous = true;
    bool emailVerified = false;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastSignIn;
    std::unordered_map<std::string, std::string> customClaims;
};

// Authentication tokens
struct AuthToken {
    std::string idToken;
    std::string refreshToken;
    std::string accessToken;
    std::chrono::system_clock::time_point expiresAt;

    bool isExpired() const {
        return std::chrono::system_clock::now() >= expiresAt;
    }

    bool needsRefresh() const {
        // Refresh if within 5 minutes of expiry
        auto threshold = expiresAt - std::chrono::minutes(5);
        return std::chrono::system_clock::now() >= threshold;
    }
};

// Configuration for Firebase
struct FirebaseConfig {
    std::string apiKey;
    std::string projectId;
    std::string authDomain;
    std::string databaseUrl;
    std::string storageBucket;
    std::string messagingSenderId;
    std::string appId;
    std::string measurementId;

    // REST API endpoints (constructed from config)
    std::string getAuthUrl() const {
        return "https://identitytoolkit.googleapis.com/v1";
    }

    std::string getFirestoreUrl() const {
        return "https://firestore.googleapis.com/v1/projects/" + projectId + "/databases/(default)/documents";
    }

    std::string getRealtimeDbUrl() const {
        return databaseUrl.empty() ?
            "https://" + projectId + "-default-rtdb.firebaseio.com" : databaseUrl;
    }

    std::string getTokenRefreshUrl() const {
        return "https://securetoken.googleapis.com/v1/token";
    }
};

// Retry configuration
struct RetryConfig {
    int maxRetries = 5;
    int baseDelayMs = 100;
    int maxDelayMs = 30000;
    float backoffMultiplier = 2.0f;
    bool useJitter = true;
};

// HTTP request/response for REST API
struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    int timeoutMs = 30000;
};

struct HttpResponse {
    int statusCode = 0;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    FirebaseError error;
};

// Callback types
using AuthCallback = std::function<void(const FirebaseUser&, const FirebaseError&)>;
using ConnectionCallback = std::function<void(ConnectionState)>;
using ErrorCallback = std::function<void(const FirebaseError&)>;
using HttpCallback = std::function<void(const HttpResponse&)>;
using TokenCallback = std::function<void(const std::string&, const FirebaseError&)>;

// Offline persistence cache entry
struct CacheEntry {
    std::string key;
    std::string data;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point expiresAt;
    bool isDirty = false;  // Needs sync when online
};

// Pending operation for offline support
struct PendingOperation {
    enum class Type { Create, Update, Delete };

    Type type;
    std::string path;
    std::string data;
    std::chrono::system_clock::time_point timestamp;
    int retryCount = 0;
    std::string id;  // Unique operation ID
};

/**
 * FirebaseCore - Core Firebase integration using REST API
 *
 * Provides authentication, connection management, token refresh,
 * offline persistence, and error handling with retry logic.
 */
class FirebaseCore {
public:
    static FirebaseCore& getInstance();

    // Initialization
    bool initialize(const FirebaseConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Configuration
    const FirebaseConfig& getConfig() const { return m_config; }
    void setRetryConfig(const RetryConfig& config) { m_retryConfig = config; }

    // Authentication
    void signInAnonymously(AuthCallback callback);
    void signInWithEmail(const std::string& email, const std::string& password, AuthCallback callback);
    void signUpWithEmail(const std::string& email, const std::string& password, AuthCallback callback);
    void signInWithGoogle(const std::string& idToken, AuthCallback callback);
    void signInWithApple(const std::string& idToken, const std::string& nonce, AuthCallback callback);
    void signInWithCustomToken(const std::string& customToken, AuthCallback callback);
    void signOut();
    void deleteAccount(AuthCallback callback);

    // Password management
    void sendPasswordReset(const std::string& email, std::function<void(const FirebaseError&)> callback);
    void updatePassword(const std::string& newPassword, AuthCallback callback);
    void updateEmail(const std::string& newEmail, AuthCallback callback);
    void updateProfile(const std::string& displayName, const std::string& photoUrl, AuthCallback callback);

    // Token management
    void refreshToken(TokenCallback callback = nullptr);
    std::string getIdToken() const;
    bool isTokenValid() const;
    void setAutoRefresh(bool enabled) { m_autoRefreshEnabled = enabled; }

    // User state
    bool isSignedIn() const { return m_isSignedIn; }
    const FirebaseUser& getCurrentUser() const { return m_currentUser; }
    void onAuthStateChanged(AuthCallback callback);

    // Connection management
    ConnectionState getConnectionState() const { return m_connectionState; }
    void onConnectionStateChanged(ConnectionCallback callback);
    void reconnect();
    void goOffline();
    void goOnline();
    bool isOnline() const { return m_isOnline; }

    // HTTP helpers for other Firebase classes
    void makeRequest(const HttpRequest& request, HttpCallback callback);
    void makeAuthenticatedRequest(const HttpRequest& request, HttpCallback callback);

    // Offline persistence
    void enablePersistence(bool enabled);
    void clearCache();
    void setCache(const std::string& key, const std::string& data,
                  std::chrono::seconds ttl = std::chrono::seconds(3600));
    std::string getCache(const std::string& key);
    bool hasCache(const std::string& key);

    // Pending operations (offline writes)
    void addPendingOperation(const PendingOperation& op);
    void processPendingOperations();
    size_t getPendingOperationCount() const;

    // Error handling
    void onError(ErrorCallback callback);
    FirebaseError getLastError() const { return m_lastError; }

    // Update (call from main loop)
    void update(float deltaTime);

private:
    FirebaseCore();
    ~FirebaseCore();
    FirebaseCore(const FirebaseCore&) = delete;
    FirebaseCore& operator=(const FirebaseCore&) = delete;

    // Internal auth helpers
    void handleAuthResponse(const HttpResponse& response, AuthCallback callback);
    void parseUserFromResponse(const std::string& json, FirebaseUser& user);
    void scheduleTokenRefresh();

    // HTTP implementation
    void executeHttpRequest(const HttpRequest& request, HttpCallback callback);
    int calculateRetryDelay(int retryCount);

    // Connection management
    void updateConnectionState(ConnectionState newState);
    void startHeartbeat();
    void stopHeartbeat();

    // Persistence helpers
    void loadCacheFromDisk();
    void saveCacheToDisk();
    void loadPendingOperations();
    void savePendingOperations();
    void cleanExpiredCache();

    // Error helpers
    FirebaseError parseError(const HttpResponse& response);
    void reportError(const FirebaseError& error);

private:
    bool m_initialized = false;
    FirebaseConfig m_config;
    RetryConfig m_retryConfig;

    // Authentication state
    std::atomic<bool> m_isSignedIn{false};
    FirebaseUser m_currentUser;
    AuthToken m_authToken;
    mutable std::mutex m_authMutex;

    // Connection state
    std::atomic<ConnectionState> m_connectionState{ConnectionState::Disconnected};
    std::atomic<bool> m_isOnline{true};
    float m_heartbeatTimer = 0.0f;
    float m_tokenRefreshTimer = 0.0f;
    int m_reconnectAttempts = 0;

    // Callbacks
    std::vector<AuthCallback> m_authCallbacks;
    std::vector<ConnectionCallback> m_connectionCallbacks;
    std::vector<ErrorCallback> m_errorCallbacks;
    std::mutex m_callbackMutex;

    // Offline persistence
    bool m_persistenceEnabled = false;
    std::unordered_map<std::string, CacheEntry> m_cache;
    std::queue<PendingOperation> m_pendingOperations;
    mutable std::mutex m_cacheMutex;
    mutable std::mutex m_pendingOpsMutex;
    std::string m_cacheFilePath;
    std::string m_pendingOpsFilePath;

    // Error tracking
    FirebaseError m_lastError;

    // Timing
    std::chrono::steady_clock::time_point m_lastHeartbeat;
};

// Helper macros for Firebase operations
#define FIREBASE_CHECK_INITIALIZED() \
    if (!FirebaseCore::getInstance().isInitialized()) { \
        FirebaseError err; \
        err.type = FirebaseErrorType::InvalidArgument; \
        err.message = "Firebase not initialized"; \
        return; \
    }

#define FIREBASE_CHECK_AUTH() \
    if (!FirebaseCore::getInstance().isSignedIn()) { \
        FirebaseError err; \
        err.type = FirebaseErrorType::AuthError; \
        err.message = "User not authenticated"; \
        return; \
    }

} // namespace Firebase
} // namespace Network
