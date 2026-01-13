/**
 * @file CloudProviderRegistry.hpp
 * @brief Registry for managing cloud provider instances
 *
 * This file provides a singleton registry for registering, retrieving, and
 * managing cloud provider implementations. It supports multiple simultaneous
 * providers and automatic provider selection.
 *
 * @section registry_usage Basic Usage
 *
 * @code{.cpp}
 * #include <engine/networking/CloudProviderRegistry.hpp>
 * #include <engine/networking/FirebaseProvider.hpp>  // Concrete implementation
 *
 * // Register a provider
 * auto& registry = Nova::CloudProviderRegistry::Instance();
 * auto firebaseProvider = std::make_shared<Nova::FirebaseProvider>();
 * registry.RegisterProvider(firebaseProvider);
 *
 * // Get provider by type
 * auto provider = registry.GetProvider(Nova::CloudProviderType::Firebase);
 *
 * // Or get the default provider
 * auto defaultProvider = registry.GetDefaultProvider();
 * @endcode
 *
 * @section registry_factory Factory Pattern
 *
 * @code{.cpp}
 * // Register a factory for lazy instantiation
 * registry.RegisterFactory(Nova::CloudProviderType::AWS, []() {
 *     return std::make_shared<Nova::AWSProvider>();
 * });
 *
 * // Provider is created on first request
 * auto awsProvider = registry.GetProvider(Nova::CloudProviderType::AWS);
 * @endcode
 *
 * @see ICloudProvider
 *
 * @author Nova3D Team
 * @version 1.0.0
 */

#pragma once

#include "ICloudProvider.hpp"

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <optional>

namespace Nova {

/**
 * @brief Factory function type for creating cloud providers
 */
using CloudProviderFactory = std::function<CloudProviderPtr()>;

/**
 * @brief Configuration for provider initialization
 */
struct CloudProviderConfig {
    CloudProviderType type = CloudProviderType::Firebase;  ///< Provider type
    CloudCredentials credentials;                           ///< Authentication credentials
    bool autoConnect = true;                                ///< Connect immediately after creation
    bool setAsDefault = false;                              ///< Set as default provider

    /**
     * @brief Create config for Firebase
     */
    [[nodiscard]] static CloudProviderConfig Firebase(const std::string& apiKey,
                                                       const std::string& projectId,
                                                       const std::string& databaseUrl = "") {
        CloudProviderConfig config;
        config.type = CloudProviderType::Firebase;
        config.credentials.apiKey = apiKey;
        config.credentials.projectId = projectId;
        config.credentials.databaseUrl = databaseUrl.empty()
            ? "https://" + projectId + "-default-rtdb.firebaseio.com"
            : databaseUrl;
        return config;
    }

    /**
     * @brief Create config for AWS
     */
    [[nodiscard]] static CloudProviderConfig AWS(const std::string& accessKeyId,
                                                  const std::string& secretAccessKey,
                                                  const std::string& region = "us-east-1") {
        CloudProviderConfig config;
        config.type = CloudProviderType::AWS;
        config.credentials.apiKey = accessKeyId;
        config.credentials.secretAccessKey = secretAccessKey;
        config.credentials.region = region;
        return config;
    }

    /**
     * @brief Create config for Azure
     */
    [[nodiscard]] static CloudProviderConfig Azure(const std::string& connectionString) {
        CloudProviderConfig config;
        config.type = CloudProviderType::Azure;
        config.credentials.connectionString = connectionString;
        return config;
    }
};

/**
 * @brief Registry for cloud provider instances
 *
 * Thread-safe singleton that manages cloud provider lifecycle and provides
 * centralized access to provider instances. Supports multiple providers
 * simultaneously and provider factories for lazy initialization.
 *
 * Features:
 * - Multiple provider support (Firebase + AWS simultaneously)
 * - Factory pattern for deferred creation
 * - Default provider selection
 * - Automatic initialization and shutdown
 * - Provider health monitoring
 */
class CloudProviderRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the registry instance
     */
    [[nodiscard]] static CloudProviderRegistry& Instance() {
        static CloudProviderRegistry instance;
        return instance;
    }

    /**
     * @brief Shutdown all providers and clear the registry
     *
     * Should be called before application exit.
     */
    void Shutdown();

    // ========================================================================
    // Provider Registration
    // ========================================================================

    /**
     * @brief Register an existing provider instance
     *
     * @param provider Provider instance to register
     * @param setAsDefault Set this provider as the default
     * @return true if registration succeeded
     */
    bool RegisterProvider(CloudProviderPtr provider, bool setAsDefault = false);

    /**
     * @brief Register a factory for lazy provider creation
     *
     * The factory is called when GetProvider() is first called for this type.
     *
     * @param type Provider type
     * @param factory Factory function
     * @return true if registration succeeded
     */
    bool RegisterFactory(CloudProviderType type, CloudProviderFactory factory);

    /**
     * @brief Register and initialize a provider from config
     *
     * Creates, initializes, and registers a provider in one step.
     * Requires a factory to be registered for the provider type.
     *
     * @param config Provider configuration
     * @return Initialized provider or nullptr on failure
     */
    CloudProviderPtr CreateProvider(const CloudProviderConfig& config);

    /**
     * @brief Unregister a provider
     *
     * @param type Provider type to unregister
     * @return true if a provider was unregistered
     */
    bool UnregisterProvider(CloudProviderType type);

    /**
     * @brief Unregister all providers
     */
    void UnregisterAll();

    // ========================================================================
    // Provider Access
    // ========================================================================

    /**
     * @brief Get a provider by type
     *
     * If a factory is registered but no instance exists, creates one.
     *
     * @param type Provider type
     * @return Provider instance or nullptr if not available
     */
    [[nodiscard]] CloudProviderPtr GetProvider(CloudProviderType type);

    /**
     * @brief Get the default provider
     *
     * Returns the explicitly set default, or the first registered provider.
     *
     * @return Default provider or nullptr if none registered
     */
    [[nodiscard]] CloudProviderPtr GetDefaultProvider();

    /**
     * @brief Get a provider by name
     *
     * @param name Provider name (e.g., "Firebase", "AWS")
     * @return Provider instance or nullptr if not found
     */
    [[nodiscard]] CloudProviderPtr GetProviderByName(std::string_view name);

    /**
     * @brief Set the default provider
     *
     * @param type Provider type to set as default
     * @return true if the provider exists and was set as default
     */
    bool SetDefaultProvider(CloudProviderType type);

    /**
     * @brief Check if a provider type is registered
     *
     * @param type Provider type to check
     * @return true if provider or factory is registered
     */
    [[nodiscard]] bool HasProvider(CloudProviderType type) const;

    /**
     * @brief Check if a provider is initialized and connected
     *
     * @param type Provider type to check
     * @return true if provider is ready for operations
     */
    [[nodiscard]] bool IsProviderReady(CloudProviderType type) const;

    /**
     * @brief Get all registered provider types
     * @return Vector of registered provider types
     */
    [[nodiscard]] std::vector<CloudProviderType> GetRegisteredTypes() const;

    /**
     * @brief Get the number of registered providers
     * @return Count of registered providers and factories
     */
    [[nodiscard]] size_t GetProviderCount() const;

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    /**
     * @brief Update all registered providers
     *
     * Should be called each frame to process callbacks and maintain connections.
     *
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Initialize a specific provider with credentials
     *
     * @param type Provider type
     * @param credentials Authentication credentials
     * @return true if initialization succeeded
     */
    bool InitializeProvider(CloudProviderType type, const CloudCredentials& credentials);

    /**
     * @brief Shutdown a specific provider
     *
     * @param type Provider type
     */
    void ShutdownProvider(CloudProviderType type);

    // ========================================================================
    // Convenience Methods
    // ========================================================================

    /**
     * @brief Upload data using the default provider
     *
     * @param path Destination path
     * @param data Binary data
     * @param callback Completion callback
     * @return true if the operation was initiated
     */
    bool Upload(const std::string& path,
                const std::vector<uint8_t>& data,
                CloudCallback callback = nullptr);

    /**
     * @brief Download data using the default provider
     *
     * @param path Source path
     * @param callback Data callback
     * @return true if the operation was initiated
     */
    bool Download(const std::string& path, CloudDataCallback callback);

    /**
     * @brief Set a value using the default provider
     *
     * @param path Database path
     * @param value JSON value
     * @param callback Completion callback
     * @return true if the operation was initiated
     */
    bool SetValue(const std::string& path,
                  const nlohmann::json& value,
                  CloudCallback callback = nullptr);

    /**
     * @brief Get a value using the default provider
     *
     * @param path Database path
     * @param callback JSON callback
     * @return true if the operation was initiated
     */
    bool GetValue(const std::string& path, CloudJsonCallback callback);

    /**
     * @brief Subscribe using the default provider
     *
     * @param path Database path
     * @param callback Subscription callback
     * @return Subscription ID or 0 on failure
     */
    uint64_t Subscribe(const std::string& path, CloudSubscriptionCallback callback);

    /**
     * @brief Unsubscribe using the default provider
     *
     * @param subscriptionId Subscription ID
     */
    void Unsubscribe(uint64_t subscriptionId);

    // ========================================================================
    // Statistics and Diagnostics
    // ========================================================================

    /**
     * @brief Aggregated statistics across all providers
     */
    struct AggregatedStatistics {
        size_t activeProviders = 0;
        size_t connectedProviders = 0;
        uint64_t totalRequests = 0;
        uint64_t totalBytesUploaded = 0;
        uint64_t totalBytesDownloaded = 0;
        uint64_t totalActiveSubscriptions = 0;
    };

    /**
     * @brief Get aggregated statistics
     * @return Aggregated statistics struct
     */
    [[nodiscard]] AggregatedStatistics GetAggregatedStatistics() const;

    /**
     * @brief Get health status for all providers
     * @return Map of provider type to connection status
     */
    [[nodiscard]] std::unordered_map<CloudProviderType, bool> GetHealthStatus() const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    /**
     * @brief Callback invoked when a provider is registered
     */
    std::function<void(CloudProviderType type, CloudProviderPtr provider)> OnProviderRegistered;

    /**
     * @brief Callback invoked when a provider is unregistered
     */
    std::function<void(CloudProviderType type)> OnProviderUnregistered;

    /**
     * @brief Callback invoked when a provider connection state changes
     */
    std::function<void(CloudProviderType type, bool connected)> OnProviderConnectionChanged;

    /**
     * @brief Callback invoked when a provider encounters an error
     */
    std::function<void(CloudProviderType type, const CloudError& error)> OnProviderError;

private:
    CloudProviderRegistry() = default;
    ~CloudProviderRegistry() { Shutdown(); }

    // Non-copyable, non-movable (singleton)
    CloudProviderRegistry(const CloudProviderRegistry&) = delete;
    CloudProviderRegistry& operator=(const CloudProviderRegistry&) = delete;
    CloudProviderRegistry(CloudProviderRegistry&&) = delete;
    CloudProviderRegistry& operator=(CloudProviderRegistry&&) = delete;

    /**
     * @brief Create provider from factory if needed
     */
    CloudProviderPtr GetOrCreateProvider(CloudProviderType type);

    /**
     * @brief Check and update provider connection states
     */
    void CheckConnectionStates();

    // Thread safety
    mutable std::mutex m_mutex;

    // Provider storage
    std::unordered_map<CloudProviderType, CloudProviderPtr> m_providers;
    std::unordered_map<CloudProviderType, CloudProviderFactory> m_factories;

    // Default provider
    std::optional<CloudProviderType> m_defaultType;

    // Connection state tracking
    std::unordered_map<CloudProviderType, bool> m_lastConnectionState;
    float m_connectionCheckTimer = 0.0f;
    static constexpr float CONNECTION_CHECK_INTERVAL = 5.0f;
};

// ============================================================================
// Helper Macros
// ============================================================================

/**
 * @brief Quick access to the cloud provider registry
 */
#define CLOUD_REGISTRY Nova::CloudProviderRegistry::Instance()

/**
 * @brief Quick access to the default cloud provider
 */
#define CLOUD_PROVIDER Nova::CloudProviderRegistry::Instance().GetDefaultProvider()

// ============================================================================
// RAII Helper
// ============================================================================

/**
 * @brief RAII helper for managing provider lifecycle
 *
 * Automatically initializes and shuts down a provider within a scope.
 *
 * @code{.cpp}
 * {
 *     CloudProviderScope scope(CloudProviderType::Firebase, credentials);
 *     if (scope.IsValid()) {
 *         scope->Upload("path", data, callback);
 *     }
 * } // Provider is shut down here
 * @endcode
 */
class CloudProviderScope {
public:
    /**
     * @brief Construct and initialize provider
     */
    CloudProviderScope(CloudProviderType type, const CloudCredentials& credentials)
        : m_type(type)
    {
        auto& registry = CloudProviderRegistry::Instance();
        m_provider = registry.GetProvider(type);
        if (m_provider && !m_provider->IsInitialized()) {
            if (!m_provider->Initialize(credentials)) {
                m_provider = nullptr;
            }
        }
    }

    /**
     * @brief Destructor - shuts down the provider
     */
    ~CloudProviderScope() {
        if (m_provider) {
            m_provider->Shutdown();
        }
    }

    // Non-copyable
    CloudProviderScope(const CloudProviderScope&) = delete;
    CloudProviderScope& operator=(const CloudProviderScope&) = delete;

    // Movable
    CloudProviderScope(CloudProviderScope&& other) noexcept
        : m_type(other.m_type)
        , m_provider(std::move(other.m_provider))
    {
        other.m_provider = nullptr;
    }

    CloudProviderScope& operator=(CloudProviderScope&& other) noexcept {
        if (this != &other) {
            if (m_provider) {
                m_provider->Shutdown();
            }
            m_type = other.m_type;
            m_provider = std::move(other.m_provider);
            other.m_provider = nullptr;
        }
        return *this;
    }

    /**
     * @brief Check if provider is valid and initialized
     */
    [[nodiscard]] bool IsValid() const {
        return m_provider && m_provider->IsInitialized();
    }

    /**
     * @brief Access the provider
     */
    [[nodiscard]] ICloudProvider* operator->() const {
        return m_provider.get();
    }

    /**
     * @brief Get the provider pointer
     */
    [[nodiscard]] CloudProviderPtr Get() const {
        return m_provider;
    }

    /**
     * @brief Implicit conversion to bool
     */
    [[nodiscard]] explicit operator bool() const {
        return IsValid();
    }

private:
    CloudProviderType m_type;
    CloudProviderPtr m_provider;
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline void CloudProviderRegistry::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [type, provider] : m_providers) {
        if (provider) {
            provider->Shutdown();
        }
    }

    m_providers.clear();
    m_factories.clear();
    m_defaultType.reset();
    m_lastConnectionState.clear();
}

inline bool CloudProviderRegistry::RegisterProvider(CloudProviderPtr provider, bool setAsDefault) {
    if (!provider) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    CloudProviderType type = provider->GetType();
    m_providers[type] = provider;

    if (setAsDefault || !m_defaultType.has_value()) {
        m_defaultType = type;
    }

    if (OnProviderRegistered) {
        OnProviderRegistered(type, provider);
    }

    return true;
}

inline bool CloudProviderRegistry::RegisterFactory(CloudProviderType type,
                                                    CloudProviderFactory factory) {
    if (!factory) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_factories[type] = std::move(factory);
    return true;
}

inline CloudProviderPtr CloudProviderRegistry::CreateProvider(const CloudProviderConfig& config) {
    auto provider = GetOrCreateProvider(config.type);
    if (!provider) {
        return nullptr;
    }

    if (config.autoConnect && !provider->IsInitialized()) {
        if (!provider->Initialize(config.credentials)) {
            return nullptr;
        }
    }

    if (config.setAsDefault) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_defaultType = config.type;
    }

    return provider;
}

inline bool CloudProviderRegistry::UnregisterProvider(CloudProviderType type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_providers.find(type);
    if (it != m_providers.end()) {
        if (it->second) {
            it->second->Shutdown();
        }
        m_providers.erase(it);

        if (m_defaultType == type) {
            m_defaultType.reset();
            if (!m_providers.empty()) {
                m_defaultType = m_providers.begin()->first;
            }
        }

        if (OnProviderUnregistered) {
            OnProviderUnregistered(type);
        }

        return true;
    }
    return false;
}

inline void CloudProviderRegistry::UnregisterAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [type, provider] : m_providers) {
        if (provider) {
            provider->Shutdown();
        }
        if (OnProviderUnregistered) {
            OnProviderUnregistered(type);
        }
    }

    m_providers.clear();
    m_defaultType.reset();
}

inline CloudProviderPtr CloudProviderRegistry::GetProvider(CloudProviderType type) {
    return GetOrCreateProvider(type);
}

inline CloudProviderPtr CloudProviderRegistry::GetDefaultProvider() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_defaultType.has_value()) {
        auto it = m_providers.find(*m_defaultType);
        if (it != m_providers.end()) {
            return it->second;
        }
    }

    // Return first available if no default set
    if (!m_providers.empty()) {
        return m_providers.begin()->second;
    }

    return nullptr;
}

inline CloudProviderPtr CloudProviderRegistry::GetProviderByName(std::string_view name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [type, provider] : m_providers) {
        if (provider && provider->GetName() == name) {
            return provider;
        }
    }
    return nullptr;
}

inline bool CloudProviderRegistry::SetDefaultProvider(CloudProviderType type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_providers.find(type) != m_providers.end() ||
        m_factories.find(type) != m_factories.end()) {
        m_defaultType = type;
        return true;
    }
    return false;
}

inline bool CloudProviderRegistry::HasProvider(CloudProviderType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_providers.find(type) != m_providers.end() ||
           m_factories.find(type) != m_factories.end();
}

inline bool CloudProviderRegistry::IsProviderReady(CloudProviderType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_providers.find(type);
    if (it != m_providers.end() && it->second) {
        return it->second->IsInitialized() && it->second->IsConnected();
    }
    return false;
}

inline std::vector<CloudProviderType> CloudProviderRegistry::GetRegisteredTypes() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<CloudProviderType> types;
    types.reserve(m_providers.size() + m_factories.size());

    for (const auto& [type, _] : m_providers) {
        types.push_back(type);
    }

    for (const auto& [type, _] : m_factories) {
        if (m_providers.find(type) == m_providers.end()) {
            types.push_back(type);
        }
    }

    return types;
}

inline size_t CloudProviderRegistry::GetProviderCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_providers.size();
}

inline void CloudProviderRegistry::Update(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [type, provider] : m_providers) {
        if (provider) {
            provider->Update(deltaTime);
        }
    }

    m_connectionCheckTimer += deltaTime;
    if (m_connectionCheckTimer >= CONNECTION_CHECK_INTERVAL) {
        m_connectionCheckTimer = 0.0f;
        CheckConnectionStates();
    }
}

inline bool CloudProviderRegistry::InitializeProvider(CloudProviderType type,
                                                       const CloudCredentials& credentials) {
    auto provider = GetOrCreateProvider(type);
    if (provider) {
        return provider->Initialize(credentials);
    }
    return false;
}

inline void CloudProviderRegistry::ShutdownProvider(CloudProviderType type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_providers.find(type);
    if (it != m_providers.end() && it->second) {
        it->second->Shutdown();
    }
}

inline CloudProviderPtr CloudProviderRegistry::GetOrCreateProvider(CloudProviderType type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check existing providers
    auto provIt = m_providers.find(type);
    if (provIt != m_providers.end()) {
        return provIt->second;
    }

    // Try to create from factory
    auto factIt = m_factories.find(type);
    if (factIt != m_factories.end() && factIt->second) {
        auto provider = factIt->second();
        if (provider) {
            m_providers[type] = provider;

            if (!m_defaultType.has_value()) {
                m_defaultType = type;
            }

            if (OnProviderRegistered) {
                OnProviderRegistered(type, provider);
            }

            return provider;
        }
    }

    return nullptr;
}

inline void CloudProviderRegistry::CheckConnectionStates() {
    // Note: m_mutex is already held by the caller (Update)

    for (const auto& [type, provider] : m_providers) {
        if (provider) {
            bool connected = provider->IsConnected();
            auto lastIt = m_lastConnectionState.find(type);

            if (lastIt == m_lastConnectionState.end() || lastIt->second != connected) {
                m_lastConnectionState[type] = connected;
                if (OnProviderConnectionChanged) {
                    OnProviderConnectionChanged(type, connected);
                }
            }
        }
    }
}

inline bool CloudProviderRegistry::Upload(const std::string& path,
                                           const std::vector<uint8_t>& data,
                                           CloudCallback callback) {
    auto provider = GetDefaultProvider();
    if (provider) {
        provider->Upload(path, data, callback);
        return true;
    }
    if (callback) {
        callback(false, CloudError::Make(CloudErrorCode::ProviderNotInitialized,
                                          "No default provider available"));
    }
    return false;
}

inline bool CloudProviderRegistry::Download(const std::string& path, CloudDataCallback callback) {
    auto provider = GetDefaultProvider();
    if (provider) {
        provider->Download(path, callback);
        return true;
    }
    if (callback) {
        callback(false, {}, CloudError::Make(CloudErrorCode::ProviderNotInitialized,
                                               "No default provider available"));
    }
    return false;
}

inline bool CloudProviderRegistry::SetValue(const std::string& path,
                                             const nlohmann::json& value,
                                             CloudCallback callback) {
    auto provider = GetDefaultProvider();
    if (provider) {
        provider->SetValue(path, value, callback);
        return true;
    }
    if (callback) {
        callback(false, CloudError::Make(CloudErrorCode::ProviderNotInitialized,
                                          "No default provider available"));
    }
    return false;
}

inline bool CloudProviderRegistry::GetValue(const std::string& path, CloudJsonCallback callback) {
    auto provider = GetDefaultProvider();
    if (provider) {
        provider->GetValue(path, callback);
        return true;
    }
    if (callback) {
        callback(false, {}, CloudError::Make(CloudErrorCode::ProviderNotInitialized,
                                               "No default provider available"));
    }
    return false;
}

inline uint64_t CloudProviderRegistry::Subscribe(const std::string& path,
                                                  CloudSubscriptionCallback callback) {
    auto provider = GetDefaultProvider();
    if (provider) {
        return provider->Subscribe(path, callback);
    }
    return 0;
}

inline void CloudProviderRegistry::Unsubscribe(uint64_t subscriptionId) {
    auto provider = GetDefaultProvider();
    if (provider) {
        provider->Unsubscribe(subscriptionId);
    }
}

inline CloudProviderRegistry::AggregatedStatistics
CloudProviderRegistry::GetAggregatedStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    AggregatedStatistics stats;
    stats.activeProviders = m_providers.size();

    for (const auto& [type, provider] : m_providers) {
        if (provider) {
            if (provider->IsConnected()) {
                ++stats.connectedProviders;
            }

            auto providerStats = provider->GetStatistics();
            stats.totalRequests += providerStats.totalRequests;
            stats.totalBytesUploaded += providerStats.bytesUploaded;
            stats.totalBytesDownloaded += providerStats.bytesDownloaded;
            stats.totalActiveSubscriptions += providerStats.activeSubscriptions;
        }
    }

    return stats;
}

inline std::unordered_map<CloudProviderType, bool>
CloudProviderRegistry::GetHealthStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::unordered_map<CloudProviderType, bool> status;
    for (const auto& [type, provider] : m_providers) {
        status[type] = provider && provider->IsConnected();
    }
    return status;
}

} // namespace Nova
