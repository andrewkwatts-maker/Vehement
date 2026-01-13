/**
 * @file ICloudProvider.hpp
 * @brief Abstract cloud provider interface for the Vehement SDF Engine
 *
 * This file defines the abstract interface for cloud storage providers,
 * enabling support for Firebase, AWS, Azure, and custom backends.
 * All operations are asynchronous with callback-based completion.
 *
 * @section cloud_usage Basic Usage
 *
 * @code{.cpp}
 * #include <engine/networking/ICloudProvider.hpp>
 * #include <engine/networking/CloudProviderRegistry.hpp>
 *
 * // Get a provider from the registry
 * auto& registry = Nova::CloudProviderRegistry::Instance();
 * auto provider = registry.GetProvider(Nova::CloudProviderType::Firebase);
 *
 * if (provider) {
 *     Nova::CloudCredentials creds;
 *     creds.apiKey = "your-api-key";
 *     creds.projectId = "your-project";
 *
 *     if (provider->Initialize(creds)) {
 *         provider->Authenticate("user@example.com", "password",
 *             [](bool success, const Nova::CloudError& error) {
 *                 if (success) {
 *                     // Ready to use storage operations
 *                 }
 *             });
 *     }
 * }
 * @endcode
 *
 * @see CloudProviderRegistry
 * @see FirebaseProvider (concrete implementation)
 *
 * @author Nova3D Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace Nova {

// ============================================================================
// Cloud Provider Types
// ============================================================================

/**
 * @brief Supported cloud provider types
 */
enum class CloudProviderType : uint8_t {
    Firebase = 0,   ///< Google Firebase (Realtime Database + Storage)
    AWS,            ///< Amazon Web Services (S3 + DynamoDB)
    Azure,          ///< Microsoft Azure (Blob Storage + Cosmos DB)
    Custom          ///< Custom/third-party provider
};

/**
 * @brief Convert CloudProviderType to string representation
 */
[[nodiscard]] constexpr std::string_view CloudProviderTypeToString(CloudProviderType type) noexcept {
    switch (type) {
        case CloudProviderType::Firebase: return "Firebase";
        case CloudProviderType::AWS:      return "AWS";
        case CloudProviderType::Azure:    return "Azure";
        case CloudProviderType::Custom:   return "Custom";
        default:                          return "Unknown";
    }
}

// ============================================================================
// Cloud Credentials
// ============================================================================

/**
 * @brief Authentication credentials for cloud providers
 *
 * Different providers may use different subsets of these fields.
 * Consult provider documentation for required fields.
 */
struct CloudCredentials {
    std::string apiKey;                                 ///< API key or access key ID
    std::string projectId;                              ///< Project/account identifier
    std::string userId;                                 ///< User identifier (if pre-authenticated)
    std::string authToken;                              ///< OAuth token or session token
    std::chrono::system_clock::time_point tokenExpiry;  ///< Token expiration time

    // AWS-specific
    std::string secretAccessKey;                        ///< AWS secret key
    std::string region;                                 ///< AWS region (e.g., "us-east-1")
    std::string sessionToken;                           ///< AWS session token (for STS)

    // Azure-specific
    std::string connectionString;                       ///< Azure connection string
    std::string tenantId;                               ///< Azure AD tenant ID
    std::string clientId;                               ///< Azure AD client/app ID
    std::string clientSecret;                           ///< Azure AD client secret

    // Firebase-specific
    std::string databaseUrl;                            ///< Firebase Realtime Database URL
    std::string storageBucket;                          ///< Firebase Storage bucket

    /**
     * @brief Check if the auth token has expired
     */
    [[nodiscard]] bool IsTokenExpired() const noexcept {
        return std::chrono::system_clock::now() >= tokenExpiry;
    }

    /**
     * @brief Check if credentials have minimal required data
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return !apiKey.empty() || !connectionString.empty() || !authToken.empty();
    }

    /**
     * @brief Clear all credential data (secure wipe)
     */
    void Clear() noexcept {
        apiKey.clear();
        projectId.clear();
        userId.clear();
        authToken.clear();
        tokenExpiry = {};
        secretAccessKey.clear();
        region.clear();
        sessionToken.clear();
        connectionString.clear();
        tenantId.clear();
        clientId.clear();
        clientSecret.clear();
        databaseUrl.clear();
        storageBucket.clear();
    }
};

// ============================================================================
// Cloud Error Handling
// ============================================================================

/**
 * @brief Error codes for cloud operations
 */
enum class CloudErrorCode : int {
    None = 0,                   ///< No error
    Unknown = -1,               ///< Unknown error

    // Authentication errors (100-199)
    AuthenticationFailed = 100,
    InvalidCredentials = 101,
    TokenExpired = 102,
    UserNotFound = 103,
    UserDisabled = 104,
    TooManyRequests = 105,
    InvalidEmail = 106,
    WeakPassword = 107,
    EmailAlreadyInUse = 108,

    // Network errors (200-299)
    NetworkUnavailable = 200,
    Timeout = 201,
    ConnectionRefused = 202,
    SSLError = 203,
    DNSError = 204,

    // Storage errors (300-399)
    ObjectNotFound = 300,
    BucketNotFound = 301,
    PermissionDenied = 302,
    QuotaExceeded = 303,
    InvalidPath = 304,
    ObjectTooLarge = 305,
    InvalidData = 306,
    ConcurrentModification = 307,

    // Database errors (400-499)
    DatabaseError = 400,
    TransactionFailed = 401,
    IndexNotFound = 402,
    ValidationFailed = 403,

    // Provider errors (500-599)
    ProviderNotInitialized = 500,
    ProviderNotSupported = 501,
    ConfigurationError = 502,
    InternalError = 503
};

/**
 * @brief Error information from cloud operations
 */
struct CloudError {
    CloudErrorCode code = CloudErrorCode::None;  ///< Error code
    std::string message;                          ///< Human-readable error message
    std::string details;                          ///< Additional details (e.g., stack trace)
    int httpCode = 0;                             ///< HTTP status code if applicable
    std::string requestId;                        ///< Provider request ID for debugging

    /**
     * @brief Create a success result (no error)
     */
    [[nodiscard]] static CloudError Success() noexcept {
        return CloudError{};
    }

    /**
     * @brief Create an error with code and message
     */
    [[nodiscard]] static CloudError Make(CloudErrorCode errorCode, std::string msg,
                                          std::string det = "") {
        return CloudError{errorCode, std::move(msg), std::move(det)};
    }

    /**
     * @brief Check if this represents an error
     */
    [[nodiscard]] bool HasError() const noexcept {
        return code != CloudErrorCode::None;
    }

    /**
     * @brief Check if this represents success
     */
    [[nodiscard]] bool IsSuccess() const noexcept {
        return code == CloudErrorCode::None;
    }

    /**
     * @brief Check if error is recoverable (can retry)
     */
    [[nodiscard]] bool IsRetryable() const noexcept {
        switch (code) {
            case CloudErrorCode::NetworkUnavailable:
            case CloudErrorCode::Timeout:
            case CloudErrorCode::ConnectionRefused:
            case CloudErrorCode::TooManyRequests:
            case CloudErrorCode::ConcurrentModification:
            case CloudErrorCode::InternalError:
                return true;
            default:
                return false;
        }
    }

    /**
     * @brief Format error as string for logging
     */
    [[nodiscard]] std::string ToString() const {
        if (IsSuccess()) {
            return "Success";
        }
        std::string result = "CloudError[" + std::to_string(static_cast<int>(code)) + "]: " + message;
        if (!details.empty()) {
            result += " (" + details + ")";
        }
        if (httpCode > 0) {
            result += " [HTTP " + std::to_string(httpCode) + "]";
        }
        return result;
    }
};

// ============================================================================
// Cloud Metadata
// ============================================================================

/**
 * @brief Metadata for cloud storage objects
 */
struct CloudObjectMetadata {
    std::string path;                                    ///< Full path to object
    std::string name;                                    ///< Object name (filename)
    std::string contentType;                             ///< MIME type
    size_t size = 0;                                     ///< Size in bytes
    std::chrono::system_clock::time_point created;       ///< Creation timestamp
    std::chrono::system_clock::time_point modified;      ///< Last modification timestamp
    std::string etag;                                    ///< Entity tag for versioning
    std::string md5Hash;                                 ///< MD5 hash of content
    std::unordered_map<std::string, std::string> customMetadata;  ///< User-defined metadata
};

// ============================================================================
// Callback Types
// ============================================================================

/**
 * @brief Basic callback for operations with success/error result
 */
using CloudCallback = std::function<void(bool success, const CloudError& error)>;

/**
 * @brief Callback for operations returning binary data
 */
using CloudDataCallback = std::function<void(bool success,
                                              const std::vector<uint8_t>& data,
                                              const CloudError& error)>;

/**
 * @brief Callback for operations returning JSON data
 */
using CloudJsonCallback = std::function<void(bool success,
                                              const nlohmann::json& data,
                                              const CloudError& error)>;

/**
 * @brief Callback for list operations
 */
using CloudListCallback = std::function<void(bool success,
                                              const std::vector<std::string>& items,
                                              const CloudError& error)>;

/**
 * @brief Callback for metadata operations
 */
using CloudMetadataCallback = std::function<void(bool success,
                                                  const CloudObjectMetadata& metadata,
                                                  const CloudError& error)>;

/**
 * @brief Callback for real-time data subscriptions
 */
using CloudSubscriptionCallback = std::function<void(const nlohmann::json& data)>;

/**
 * @brief Callback for upload/download progress
 * @param bytesTransferred Bytes transferred so far
 * @param totalBytes Total bytes to transfer (0 if unknown)
 */
using CloudProgressCallback = std::function<void(size_t bytesTransferred, size_t totalBytes)>;

// ============================================================================
// Upload Options
// ============================================================================

/**
 * @brief Options for upload operations
 */
struct CloudUploadOptions {
    std::string contentType;                              ///< MIME type (auto-detected if empty)
    std::unordered_map<std::string, std::string> metadata; ///< Custom metadata
    CloudProgressCallback progressCallback;               ///< Progress callback
    bool resumable = false;                               ///< Use resumable upload
    bool overwrite = true;                                ///< Overwrite existing object

    /**
     * @brief Default options
     */
    [[nodiscard]] static CloudUploadOptions Default() {
        return CloudUploadOptions{};
    }
};

/**
 * @brief Options for download operations
 */
struct CloudDownloadOptions {
    CloudProgressCallback progressCallback;  ///< Progress callback
    size_t rangeStart = 0;                   ///< Range start for partial download
    size_t rangeEnd = 0;                     ///< Range end (0 = to end)

    /**
     * @brief Default options
     */
    [[nodiscard]] static CloudDownloadOptions Default() {
        return CloudDownloadOptions{};
    }
};

// ============================================================================
// ICloudProvider Interface
// ============================================================================

/**
 * @brief Abstract interface for cloud storage providers
 *
 * This interface defines the contract for cloud storage operations including
 * authentication, file storage, and real-time database functionality.
 *
 * All operations are asynchronous and use callbacks for completion notification.
 * Implementations should be thread-safe for concurrent operations.
 *
 * @note Implementations should handle automatic token refresh and reconnection.
 */
class ICloudProvider {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~ICloudProvider() = default;

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    /**
     * @brief Initialize the provider with credentials
     *
     * Must be called before any other operations. Provider-specific
     * validation of credentials is performed.
     *
     * @param credentials Authentication credentials
     * @return true if initialization succeeded, false otherwise
     */
    virtual bool Initialize(const CloudCredentials& credentials) = 0;

    /**
     * @brief Shutdown the provider and release resources
     *
     * Cancels pending operations and disconnects from the service.
     * Safe to call multiple times.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Check if the provider is connected to the service
     * @return true if connected and ready for operations
     */
    [[nodiscard]] virtual bool IsConnected() const = 0;

    /**
     * @brief Check if the provider has been initialized
     * @return true if Initialize() was called successfully
     */
    [[nodiscard]] virtual bool IsInitialized() const = 0;

    /**
     * @brief Update provider state (call periodically)
     *
     * Processes pending callbacks, handles token refresh, etc.
     * Should be called from the main thread.
     *
     * @param deltaTime Time since last update in seconds
     */
    virtual void Update(float deltaTime) = 0;

    // ========================================================================
    // Authentication
    // ========================================================================

    /**
     * @brief Authenticate with email and password
     *
     * @param email User email address
     * @param password User password
     * @param callback Completion callback
     */
    virtual void Authenticate(const std::string& email,
                               const std::string& password,
                               CloudCallback callback) = 0;

    /**
     * @brief Authenticate anonymously (guest mode)
     *
     * Creates a temporary anonymous account. Data persists until
     * the account is converted or deleted.
     *
     * @param callback Completion callback
     */
    virtual void AuthenticateAnonymous(CloudCallback callback) = 0;

    /**
     * @brief Authenticate with custom token
     *
     * Used for server-generated tokens or OAuth providers.
     *
     * @param token Authentication token
     * @param callback Completion callback
     */
    virtual void AuthenticateWithToken(const std::string& token,
                                        CloudCallback callback) = 0;

    /**
     * @brief Sign out the current user
     *
     * Clears authentication state and cancels subscriptions.
     */
    virtual void SignOut() = 0;

    /**
     * @brief Check if a user is currently authenticated
     * @return true if authenticated
     */
    [[nodiscard]] virtual bool IsAuthenticated() const = 0;

    /**
     * @brief Get the current user's ID
     * @return User ID or empty string if not authenticated
     */
    [[nodiscard]] virtual std::string GetUserId() const = 0;

    /**
     * @brief Get the current authentication token
     * @return Auth token or empty string if not authenticated
     */
    [[nodiscard]] virtual std::string GetAuthToken() const = 0;

    /**
     * @brief Refresh the authentication token
     *
     * Called automatically when token expires, but can be called
     * manually to ensure fresh token.
     *
     * @param callback Completion callback
     */
    virtual void RefreshToken(CloudCallback callback) = 0;

    // ========================================================================
    // Storage Operations
    // ========================================================================

    /**
     * @brief Upload binary data to cloud storage
     *
     * @param path Destination path in storage
     * @param data Binary data to upload
     * @param callback Completion callback
     */
    virtual void Upload(const std::string& path,
                        const std::vector<uint8_t>& data,
                        CloudCallback callback) = 0;

    /**
     * @brief Upload binary data with options
     *
     * @param path Destination path in storage
     * @param data Binary data to upload
     * @param options Upload options (content type, metadata, etc.)
     * @param callback Completion callback
     */
    virtual void Upload(const std::string& path,
                        const std::vector<uint8_t>& data,
                        const CloudUploadOptions& options,
                        CloudCallback callback) = 0;

    /**
     * @brief Download binary data from cloud storage
     *
     * @param path Source path in storage
     * @param callback Callback receiving the data
     */
    virtual void Download(const std::string& path,
                          CloudDataCallback callback) = 0;

    /**
     * @brief Download binary data with options
     *
     * @param path Source path in storage
     * @param options Download options (progress callback, range, etc.)
     * @param callback Callback receiving the data
     */
    virtual void Download(const std::string& path,
                          const CloudDownloadOptions& options,
                          CloudDataCallback callback) = 0;

    /**
     * @brief Delete an object from cloud storage
     *
     * @param path Path to delete
     * @param callback Completion callback
     */
    virtual void Delete(const std::string& path,
                        CloudCallback callback) = 0;

    /**
     * @brief List objects at a path
     *
     * @param path Directory path to list
     * @param callback Callback receiving the list of object names
     */
    virtual void List(const std::string& path,
                      CloudListCallback callback) = 0;

    /**
     * @brief Check if an object exists
     *
     * @param path Path to check
     * @param callback Callback receiving the result (success = exists)
     */
    virtual void Exists(const std::string& path,
                        CloudCallback callback) = 0;

    /**
     * @brief Get metadata for an object
     *
     * @param path Path to query
     * @param callback Callback receiving the metadata
     */
    virtual void GetMetadata(const std::string& path,
                              CloudMetadataCallback callback) = 0;

    /**
     * @brief Copy an object to a new location
     *
     * @param sourcePath Source path
     * @param destPath Destination path
     * @param callback Completion callback
     */
    virtual void Copy(const std::string& sourcePath,
                      const std::string& destPath,
                      CloudCallback callback) = 0;

    /**
     * @brief Move an object to a new location
     *
     * @param sourcePath Source path
     * @param destPath Destination path
     * @param callback Completion callback
     */
    virtual void Move(const std::string& sourcePath,
                      const std::string& destPath,
                      CloudCallback callback) = 0;

    // ========================================================================
    // Real-time Database Operations
    // ========================================================================

    /**
     * @brief Set a JSON value at a database path
     *
     * Overwrites any existing data at the path.
     *
     * @param path Database path
     * @param value JSON value to set
     * @param callback Completion callback
     */
    virtual void SetValue(const std::string& path,
                          const nlohmann::json& value,
                          CloudCallback callback) = 0;

    /**
     * @brief Get a JSON value from a database path
     *
     * @param path Database path
     * @param callback Callback receiving the JSON data
     */
    virtual void GetValue(const std::string& path,
                          CloudJsonCallback callback) = 0;

    /**
     * @brief Update a JSON value at a database path
     *
     * Merges the provided data with existing data.
     *
     * @param path Database path
     * @param value JSON value to merge
     * @param callback Completion callback
     */
    virtual void UpdateValue(const std::string& path,
                              const nlohmann::json& value,
                              CloudCallback callback) = 0;

    /**
     * @brief Push a new child to a database path
     *
     * Creates a new auto-generated key under the path.
     *
     * @param path Database path
     * @param value JSON value to push
     * @param callback Callback receiving success and the new key in error.details
     */
    virtual void PushValue(const std::string& path,
                            const nlohmann::json& value,
                            CloudCallback callback) = 0;

    /**
     * @brief Delete a value from the database
     *
     * @param path Database path
     * @param callback Completion callback
     */
    virtual void DeleteValue(const std::string& path,
                              CloudCallback callback) = 0;

    /**
     * @brief Run a transaction on a database path
     *
     * Atomically reads and updates a value.
     *
     * @param path Database path
     * @param updateFunc Function that transforms current value to new value
     * @param callback Completion callback
     */
    virtual void Transaction(const std::string& path,
                              std::function<nlohmann::json(const nlohmann::json&)> updateFunc,
                              CloudCallback callback) = 0;

    // ========================================================================
    // Real-time Subscriptions
    // ========================================================================

    /**
     * @brief Subscribe to changes at a database path
     *
     * The callback is invoked whenever data at the path changes.
     *
     * @param path Database path to watch
     * @param callback Callback invoked on data changes
     * @return Subscription ID for unsubscribing
     */
    virtual uint64_t Subscribe(const std::string& path,
                                CloudSubscriptionCallback callback) = 0;

    /**
     * @brief Unsubscribe from a path
     *
     * @param subscriptionId ID returned from Subscribe()
     */
    virtual void Unsubscribe(uint64_t subscriptionId) = 0;

    /**
     * @brief Unsubscribe from a path by path string
     *
     * @param path Database path to unsubscribe from
     */
    virtual void Unsubscribe(const std::string& path) = 0;

    /**
     * @brief Unsubscribe from all paths
     */
    virtual void UnsubscribeAll() = 0;

    // ========================================================================
    // Provider Information
    // ========================================================================

    /**
     * @brief Get the provider type
     * @return Provider type enum value
     */
    [[nodiscard]] virtual CloudProviderType GetType() const = 0;

    /**
     * @brief Get the provider name
     * @return Human-readable provider name
     */
    [[nodiscard]] virtual std::string_view GetName() const = 0;

    /**
     * @brief Get provider version string
     * @return Version string (e.g., "1.0.0")
     */
    [[nodiscard]] virtual std::string_view GetVersion() const = 0;

    /**
     * @brief Check if the provider supports a specific feature
     *
     * @param feature Feature name (e.g., "realtime_database", "storage", "transactions")
     * @return true if the feature is supported
     */
    [[nodiscard]] virtual bool SupportsFeature(std::string_view feature) const = 0;

    // ========================================================================
    // Statistics and Diagnostics
    // ========================================================================

    /**
     * @brief Provider operation statistics
     */
    struct Statistics {
        uint64_t totalRequests = 0;           ///< Total requests made
        uint64_t successfulRequests = 0;      ///< Requests that succeeded
        uint64_t failedRequests = 0;          ///< Requests that failed
        uint64_t bytesUploaded = 0;           ///< Total bytes uploaded
        uint64_t bytesDownloaded = 0;         ///< Total bytes downloaded
        uint64_t activeSubscriptions = 0;     ///< Current active subscriptions
        float averageLatencyMs = 0.0f;        ///< Average request latency
        std::chrono::system_clock::time_point lastRequestTime;  ///< Last request timestamp
    };

    /**
     * @brief Get provider statistics
     * @return Statistics struct
     */
    [[nodiscard]] virtual Statistics GetStatistics() const = 0;

    /**
     * @brief Reset statistics counters
     */
    virtual void ResetStatistics() = 0;

    /**
     * @brief Get the last error that occurred
     * @return Last CloudError
     */
    [[nodiscard]] virtual CloudError GetLastError() const = 0;

protected:
    // Protected default constructor (interface only)
    ICloudProvider() = default;

    // Non-copyable
    ICloudProvider(const ICloudProvider&) = delete;
    ICloudProvider& operator=(const ICloudProvider&) = delete;

    // Movable
    ICloudProvider(ICloudProvider&&) = default;
    ICloudProvider& operator=(ICloudProvider&&) = default;
};

/**
 * @brief Shared pointer type for cloud providers
 */
using CloudProviderPtr = std::shared_ptr<ICloudProvider>;

/**
 * @brief Unique pointer type for cloud providers
 */
using CloudProviderUniquePtr = std::unique_ptr<ICloudProvider>;

} // namespace Nova
