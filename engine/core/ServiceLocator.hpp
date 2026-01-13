#pragma once

/**
 * @file ServiceLocator.hpp
 * @brief Service Locator pattern implementation for the Vehement SDF Engine
 *
 * Provides a centralized registry for service interfaces with:
 * - Type-safe service registration and retrieval
 * - Support for interface-based services (dependency inversion)
 * - Thread-safe access with read-write locking
 * - Lazy initialization support via factory functions
 * - Service lifetime management and cleanup
 * - Debug utilities for service inspection
 *
 * @section usage Basic Usage
 * @code{.cpp}
 * // Register a concrete service implementation
 * auto logService = std::make_shared<ConsoleLogService>();
 * ServiceLocator::Register<ILogService>(logService);
 *
 * // Register with lazy initialization
 * ServiceLocator::RegisterLazy<IAssetService>([]() {
 *     return std::make_shared<AssetService>("assets/");
 * });
 *
 * // Retrieve services
 * ILogService& log = ServiceLocator::Get<ILogService>();
 * log.Info("Service retrieved successfully");
 *
 * // Check if service exists
 * if (ServiceLocator::Has<ISettingsService>()) {
 *     auto& settings = ServiceLocator::Get<ISettingsService>();
 *     // Use settings...
 * }
 *
 * // Safe retrieval (returns nullptr if not registered)
 * if (auto* input = ServiceLocator::TryGet<IInputService>()) {
 *     input->Update();
 * }
 * @endcode
 *
 * @author Nova Engine Team
 * @version 1.0.0
 */

#include <memory>
#include <shared_mutex>
#include <mutex>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
#include <stdexcept>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

// Platform-specific demangling support
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class ServiceLocator;

// ============================================================================
// ServiceNotFoundError - Exception for missing service access
// ============================================================================

/**
 * @brief Exception thrown when attempting to access an unregistered service
 */
class ServiceNotFoundError : public std::runtime_error {
public:
    explicit ServiceNotFoundError(const std::string& typeName)
        : std::runtime_error("Service not found: " + typeName)
        , m_typeName(typeName) {}

    [[nodiscard]] const std::string& GetTypeName() const noexcept {
        return m_typeName;
    }

private:
    std::string m_typeName;
};

// ============================================================================
// ServiceEntry - Internal storage for registered services
// ============================================================================

namespace detail {

/**
 * @brief Internal service entry storing either an instance or a factory
 */
struct ServiceEntry {
    std::shared_ptr<void> instance;
    std::function<std::shared_ptr<void>()> factory;
    std::string typeName;
    bool isLazy = false;
    std::atomic<bool> isInitialized{false};
    std::mutex initMutex;

    ServiceEntry() = default;

    // Non-copyable due to mutex
    ServiceEntry(const ServiceEntry&) = delete;
    ServiceEntry& operator=(const ServiceEntry&) = delete;

    // Move constructor for container insertion
    ServiceEntry(ServiceEntry&& other) noexcept
        : instance(std::move(other.instance))
        , factory(std::move(other.factory))
        , typeName(std::move(other.typeName))
        , isLazy(other.isLazy)
        , isInitialized(other.isInitialized.load()) {}

    ServiceEntry& operator=(ServiceEntry&& other) noexcept {
        if (this != &other) {
            instance = std::move(other.instance);
            factory = std::move(other.factory);
            typeName = std::move(other.typeName);
            isLazy = other.isLazy;
            isInitialized.store(other.isInitialized.load());
        }
        return *this;
    }

    /**
     * @brief Get or create the service instance
     * @return Shared pointer to the service
     */
    std::shared_ptr<void> GetOrCreate() {
        if (!isLazy || isInitialized.load(std::memory_order_acquire)) {
            return instance;
        }

        // Double-checked locking for lazy initialization
        std::lock_guard<std::mutex> lock(initMutex);
        if (!isInitialized.load(std::memory_order_relaxed)) {
            if (factory) {
                instance = factory();
            }
            isInitialized.store(true, std::memory_order_release);
        }
        return instance;
    }
};

/**
 * @brief Demangle type name for readable output (platform-specific)
 */
[[nodiscard]] inline std::string DemangleTypeName(const char* mangledName) {
#if defined(__GNUC__) || defined(__clang__)
    // GCC/Clang demangling
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangledName, nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        std::string result(demangled);
        std::free(demangled);
        return result;
    }
#endif
    // MSVC or fallback - name is already readable
    return std::string(mangledName);
}

} // namespace detail

// ============================================================================
// ServiceLocator - Main service registry
// ============================================================================

/**
 * @brief Centralized service registry with thread-safe access
 *
 * The ServiceLocator provides a global registry for service interfaces,
 * allowing for loose coupling between components. Services are registered
 * by interface type and can be retrieved type-safely.
 *
 * Thread Safety:
 * - Registration/unregistration operations are serialized
 * - Retrieval operations can proceed concurrently
 * - Lazy initialization uses double-checked locking
 *
 * Lifetime Management:
 * - Services are stored as shared_ptr for automatic cleanup
 * - Clear() releases all service references
 * - Shutdown() provides ordered cleanup with logging
 */
class ServiceLocator {
public:
    // -------------------------------------------------------------------------
    // Service Registration
    // -------------------------------------------------------------------------

    /**
     * @brief Register a service instance
     *
     * @tparam TInterface The interface type to register under
     * @tparam TImpl The implementation type (must be derived from TInterface)
     * @param instance Shared pointer to the service implementation
     *
     * @code{.cpp}
     * auto service = std::make_shared<ConcreteService>();
     * ServiceLocator::Register<IService>(service);
     * @endcode
     */
    template<typename TInterface, typename TImpl>
    static void Register(std::shared_ptr<TImpl> instance) {
        static_assert(std::is_base_of_v<TInterface, TImpl>,
            "TImpl must be derived from TInterface");

        std::unique_lock lock(GetMutex());

        detail::ServiceEntry entry;
        entry.instance = std::static_pointer_cast<void>(instance);
        entry.typeName = detail::DemangleTypeName(typeid(TInterface).name());
        entry.isLazy = false;
        entry.isInitialized.store(true);

        GetServices()[std::type_index(typeid(TInterface))] = std::move(entry);
    }

    /**
     * @brief Register a service with an explicit interface (convenience overload)
     *
     * @tparam TInterface The interface type
     * @param instance Shared pointer to the service
     */
    template<typename TInterface>
    static void Register(std::shared_ptr<TInterface> instance) {
        Register<TInterface, TInterface>(std::move(instance));
    }

    /**
     * @brief Register a service with lazy initialization
     *
     * The factory function will be called on first access to create the service.
     * Thread-safe: if multiple threads access simultaneously, only one will
     * call the factory.
     *
     * @tparam TInterface The interface type to register under
     * @param factory Function that creates the service instance
     *
     * @code{.cpp}
     * ServiceLocator::RegisterLazy<IDatabase>([]() {
     *     return std::make_shared<DatabaseConnection>("localhost:5432");
     * });
     * @endcode
     */
    template<typename TInterface>
    static void RegisterLazy(std::function<std::shared_ptr<TInterface>()> factory) {
        std::unique_lock lock(GetMutex());

        detail::ServiceEntry entry;
        entry.factory = [f = std::move(factory)]() -> std::shared_ptr<void> {
            return std::static_pointer_cast<void>(f());
        };
        entry.typeName = detail::DemangleTypeName(typeid(TInterface).name());
        entry.isLazy = true;
        entry.isInitialized.store(false);

        GetServices()[std::type_index(typeid(TInterface))] = std::move(entry);
    }

    /**
     * @brief Unregister a service
     *
     * @tparam TInterface The interface type to unregister
     *
     * @note This will release the shared_ptr reference. If this is the last
     *       reference, the service will be destroyed.
     */
    template<typename TInterface>
    static void Unregister() {
        std::unique_lock lock(GetMutex());
        GetServices().erase(std::type_index(typeid(TInterface)));
    }

    // -------------------------------------------------------------------------
    // Service Retrieval
    // -------------------------------------------------------------------------

    /**
     * @brief Get a service reference
     *
     * @tparam TInterface The interface type to retrieve
     * @return Reference to the service
     * @throws ServiceNotFoundError if service is not registered
     *
     * @code{.cpp}
     * ILogService& log = ServiceLocator::Get<ILogService>();
     * log.Info("Hello");
     * @endcode
     */
    template<typename TInterface>
    [[nodiscard]] static TInterface& Get() {
        auto* service = TryGet<TInterface>();
        if (!service) {
            throw ServiceNotFoundError(
                detail::DemangleTypeName(typeid(TInterface).name()));
        }
        return *service;
    }

    /**
     * @brief Try to get a service pointer
     *
     * @tparam TInterface The interface type to retrieve
     * @return Pointer to the service, or nullptr if not registered
     *
     * @code{.cpp}
     * if (auto* settings = ServiceLocator::TryGet<ISettingsService>()) {
     *     settings->Load("config.json");
     * }
     * @endcode
     */
    template<typename TInterface>
    [[nodiscard]] static TInterface* TryGet() {
        std::shared_lock lock(GetMutex());

        auto& services = GetServices();
        auto it = services.find(std::type_index(typeid(TInterface)));
        if (it == services.end()) {
            return nullptr;
        }

        // For lazy services, GetOrCreate handles thread-safe initialization
        auto ptr = it->second.GetOrCreate();
        return static_cast<TInterface*>(ptr.get());
    }

    /**
     * @brief Get a service as a shared pointer
     *
     * @tparam TInterface The interface type to retrieve
     * @return Shared pointer to the service, or nullptr if not registered
     *
     * @code{.cpp}
     * auto log = ServiceLocator::GetShared<ILogService>();
     * if (log) {
     *     log->Debug("Got shared reference");
     * }
     * @endcode
     */
    template<typename TInterface>
    [[nodiscard]] static std::shared_ptr<TInterface> GetShared() {
        std::shared_lock lock(GetMutex());

        auto& services = GetServices();
        auto it = services.find(std::type_index(typeid(TInterface)));
        if (it == services.end()) {
            return nullptr;
        }

        auto ptr = it->second.GetOrCreate();
        return std::static_pointer_cast<TInterface>(ptr);
    }

    /**
     * @brief Check if a service is registered
     *
     * @tparam TInterface The interface type to check
     * @return true if service is registered (may not be initialized yet for lazy)
     */
    template<typename TInterface>
    [[nodiscard]] static bool Has() {
        std::shared_lock lock(GetMutex());
        return GetServices().find(std::type_index(typeid(TInterface))) != GetServices().end();
    }

    // -------------------------------------------------------------------------
    // Lifecycle Management
    // -------------------------------------------------------------------------

    /**
     * @brief Clear all registered services
     *
     * Releases all shared_ptr references. Services will be destroyed if
     * no other references exist.
     */
    static void Clear() {
        std::unique_lock lock(GetMutex());
        GetServices().clear();
    }

    /**
     * @brief Initialize the service locator
     *
     * Call this at application startup. Currently a no-op but provides
     * a hook for future initialization logic.
     */
    static void Initialize() {
        std::unique_lock lock(GetMutex());
        GetInitialized() = true;
    }

    /**
     * @brief Shutdown the service locator
     *
     * Clears all services and marks as uninitialized. Call this at
     * application shutdown for clean resource release.
     */
    static void Shutdown() {
        std::unique_lock lock(GetMutex());
        GetServices().clear();
        GetInitialized() = false;
    }

    /**
     * @brief Check if service locator is initialized
     */
    [[nodiscard]] static bool IsInitialized() {
        std::shared_lock lock(GetMutex());
        return GetInitialized();
    }

    // -------------------------------------------------------------------------
    // Debug Utilities
    // -------------------------------------------------------------------------

    /**
     * @brief Get list of all registered service type names
     *
     * @return Vector of demangled type names
     */
    [[nodiscard]] static std::vector<std::string> GetRegisteredServices() {
        std::shared_lock lock(GetMutex());

        std::vector<std::string> result;
        result.reserve(GetServices().size());

        for (const auto& [typeIndex, entry] : GetServices()) {
            std::string status = entry.isLazy
                ? (entry.isInitialized.load() ? " [lazy:initialized]" : " [lazy:pending]")
                : " [eager]";
            result.push_back(entry.typeName + status);
        }

        return result;
    }

    /**
     * @brief Dump all registered services to stdout (debug)
     */
    static void DumpServices() {
        auto services = GetRegisteredServices();
        std::printf("=== ServiceLocator Registry ===\n");
        std::printf("Total services: %zu\n", services.size());
        for (const auto& service : services) {
            std::printf("  - %s\n", service.c_str());
        }
        std::printf("===============================\n");
    }

    /**
     * @brief Get the number of registered services
     */
    [[nodiscard]] static size_t GetServiceCount() {
        std::shared_lock lock(GetMutex());
        return GetServices().size();
    }

private:
    // Prevent instantiation - this is a static utility class
    ServiceLocator() = delete;
    ~ServiceLocator() = delete;
    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;

    // Static storage with function-local statics for initialization order safety
    [[nodiscard]] static std::shared_mutex& GetMutex() {
        static std::shared_mutex mutex;
        return mutex;
    }

    [[nodiscard]] static std::unordered_map<std::type_index, detail::ServiceEntry>& GetServices() {
        static std::unordered_map<std::type_index, detail::ServiceEntry> services;
        return services;
    }

    [[nodiscard]] static bool& GetInitialized() {
        static bool initialized = false;
        return initialized;
    }
};

// ============================================================================
// Service Interface Definitions
// ============================================================================

// ----------------------------------------------------------------------------
// ILogService - Logging abstraction
// ----------------------------------------------------------------------------

/**
 * @brief Log severity levels for service interface
 */
enum class ServiceLogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5
};

/**
 * @brief Logging service interface
 *
 * Provides a minimal, focused interface for logging messages.
 * Implementations may route to console, file, network, etc.
 *
 * Single Responsibility: Logging messages to configured outputs.
 */
class ILogService {
public:
    virtual ~ILogService() = default;

    /**
     * @brief Log a message at the specified level
     * @param level Severity level
     * @param category Logger category/tag
     * @param message The message to log
     */
    virtual void Log(ServiceLogLevel level, std::string_view category,
                     std::string_view message) = 0;

    /**
     * @brief Convenience: Log trace message
     */
    virtual void Trace(std::string_view category, std::string_view message) {
        Log(ServiceLogLevel::Trace, category, message);
    }

    /**
     * @brief Convenience: Log debug message
     */
    virtual void Debug(std::string_view category, std::string_view message) {
        Log(ServiceLogLevel::Debug, category, message);
    }

    /**
     * @brief Convenience: Log info message
     */
    virtual void Info(std::string_view category, std::string_view message) {
        Log(ServiceLogLevel::Info, category, message);
    }

    /**
     * @brief Convenience: Log warning message
     */
    virtual void Warn(std::string_view category, std::string_view message) {
        Log(ServiceLogLevel::Warn, category, message);
    }

    /**
     * @brief Convenience: Log error message
     */
    virtual void Error(std::string_view category, std::string_view message) {
        Log(ServiceLogLevel::Error, category, message);
    }

    /**
     * @brief Convenience: Log fatal message
     */
    virtual void Fatal(std::string_view category, std::string_view message) {
        Log(ServiceLogLevel::Fatal, category, message);
    }

    /**
     * @brief Set minimum log level (messages below this level are ignored)
     */
    virtual void SetLevel(ServiceLogLevel level) = 0;

    /**
     * @brief Get current minimum log level
     */
    [[nodiscard]] virtual ServiceLogLevel GetLevel() const = 0;

    /**
     * @brief Flush any buffered log messages
     */
    virtual void Flush() = 0;
};

// ----------------------------------------------------------------------------
// ISettingsService - Settings/configuration access
// ----------------------------------------------------------------------------

/**
 * @brief Settings service interface
 *
 * Provides a minimal, focused interface for reading and writing
 * configuration values. Implementations may use JSON, INI, registry, etc.
 *
 * Single Responsibility: Reading and persisting configuration values.
 */
class ISettingsService {
public:
    virtual ~ISettingsService() = default;

    // -------------------------------------------------------------------------
    // String values
    // -------------------------------------------------------------------------

    /**
     * @brief Get a string setting value
     * @param key Setting key (may use dot notation for sections: "graphics.resolution")
     * @param defaultValue Value to return if key doesn't exist
     * @return The setting value or default
     */
    [[nodiscard]] virtual std::string GetString(std::string_view key,
                                                 std::string_view defaultValue = "") const = 0;

    /**
     * @brief Set a string setting value
     * @param key Setting key
     * @param value Value to set
     */
    virtual void SetString(std::string_view key, std::string_view value) = 0;

    // -------------------------------------------------------------------------
    // Integer values
    // -------------------------------------------------------------------------

    /**
     * @brief Get an integer setting value
     */
    [[nodiscard]] virtual int GetInt(std::string_view key, int defaultValue = 0) const = 0;

    /**
     * @brief Set an integer setting value
     */
    virtual void SetInt(std::string_view key, int value) = 0;

    // -------------------------------------------------------------------------
    // Float values
    // -------------------------------------------------------------------------

    /**
     * @brief Get a float setting value
     */
    [[nodiscard]] virtual float GetFloat(std::string_view key, float defaultValue = 0.0f) const = 0;

    /**
     * @brief Set a float setting value
     */
    virtual void SetFloat(std::string_view key, float value) = 0;

    // -------------------------------------------------------------------------
    // Boolean values
    // -------------------------------------------------------------------------

    /**
     * @brief Get a boolean setting value
     */
    [[nodiscard]] virtual bool GetBool(std::string_view key, bool defaultValue = false) const = 0;

    /**
     * @brief Set a boolean setting value
     */
    virtual void SetBool(std::string_view key, bool value) = 0;

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    /**
     * @brief Check if a setting key exists
     */
    [[nodiscard]] virtual bool Has(std::string_view key) const = 0;

    /**
     * @brief Remove a setting
     */
    virtual void Remove(std::string_view key) = 0;

    /**
     * @brief Load settings from storage
     * @param path Path to settings file
     * @return true on success
     */
    [[nodiscard]] virtual bool Load(std::string_view path) = 0;

    /**
     * @brief Save settings to storage
     * @param path Path to settings file (empty = use last loaded path)
     * @return true on success
     */
    [[nodiscard]] virtual bool Save(std::string_view path = "") = 0;

    /**
     * @brief Clear all settings
     */
    virtual void Clear() = 0;
};

// ----------------------------------------------------------------------------
// IAssetService - Asset loading/management
// ----------------------------------------------------------------------------

/**
 * @brief Asset load result
 */
enum class AssetLoadResult {
    Success,
    NotFound,
    InvalidFormat,
    IOError,
    OutOfMemory
};

/**
 * @brief Asset service interface
 *
 * Provides a minimal, focused interface for loading and managing assets.
 * Implementations handle caching, streaming, and format-specific loading.
 *
 * Single Responsibility: Loading and lifecycle management of assets.
 */
class IAssetService {
public:
    virtual ~IAssetService() = default;

    /**
     * @brief Load an asset by path
     * @tparam T Asset type
     * @param path Asset path (relative to asset root)
     * @return Shared pointer to loaded asset, or nullptr on failure
     */
    template<typename T>
    [[nodiscard]] std::shared_ptr<T> Load(std::string_view path) {
        return std::static_pointer_cast<T>(LoadAsset(path, typeid(T)));
    }

    /**
     * @brief Async load an asset
     * @tparam T Asset type
     * @param path Asset path
     * @param callback Called when load completes
     */
    template<typename T>
    void LoadAsync(std::string_view path,
                   std::function<void(std::shared_ptr<T>)> callback) {
        LoadAssetAsync(path, typeid(T), [cb = std::move(callback)](std::shared_ptr<void> asset) {
            cb(std::static_pointer_cast<T>(asset));
        });
    }

    /**
     * @brief Check if an asset is loaded
     * @param path Asset path
     * @return true if asset is in cache
     */
    [[nodiscard]] virtual bool IsLoaded(std::string_view path) const = 0;

    /**
     * @brief Unload an asset from cache
     * @param path Asset path
     */
    virtual void Unload(std::string_view path) = 0;

    /**
     * @brief Unload all assets of a type
     */
    virtual void UnloadAll() = 0;

    /**
     * @brief Reload an asset from disk
     * @param path Asset path
     * @return true on success
     */
    [[nodiscard]] virtual bool Reload(std::string_view path) = 0;

    /**
     * @brief Check if an asset file exists
     * @param path Asset path
     * @return true if file exists
     */
    [[nodiscard]] virtual bool Exists(std::string_view path) const = 0;

    /**
     * @brief Get the asset root directory
     */
    [[nodiscard]] virtual std::string_view GetAssetRoot() const = 0;

    /**
     * @brief Get number of loaded assets
     */
    [[nodiscard]] virtual size_t GetLoadedCount() const = 0;

    /**
     * @brief Get total memory used by loaded assets (approximate)
     */
    [[nodiscard]] virtual size_t GetMemoryUsage() const = 0;

protected:
    /**
     * @brief Internal asset loading (type-erased)
     */
    virtual std::shared_ptr<void> LoadAsset(std::string_view path,
                                            const std::type_info& type) = 0;

    /**
     * @brief Internal async asset loading (type-erased)
     */
    virtual void LoadAssetAsync(std::string_view path,
                                const std::type_info& type,
                                std::function<void(std::shared_ptr<void>)> callback) = 0;
};

// ----------------------------------------------------------------------------
// IInputService - Input handling
// ----------------------------------------------------------------------------

/**
 * @brief Input service interface
 *
 * Provides a minimal, focused interface for input queries.
 * Abstracts away the underlying input system (GLFW, SDL, etc.)
 *
 * Single Responsibility: Querying input device state.
 */
class IInputService {
public:
    virtual ~IInputService() = default;

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------

    /**
     * @brief Update input state (call once per frame)
     */
    virtual void Update() = 0;

    // -------------------------------------------------------------------------
    // Keyboard
    // -------------------------------------------------------------------------

    /**
     * @brief Check if a key is currently held down
     * @param keyCode Platform-independent key code
     */
    [[nodiscard]] virtual bool IsKeyDown(int keyCode) const = 0;

    /**
     * @brief Check if a key was pressed this frame
     */
    [[nodiscard]] virtual bool IsKeyPressed(int keyCode) const = 0;

    /**
     * @brief Check if a key was released this frame
     */
    [[nodiscard]] virtual bool IsKeyReleased(int keyCode) const = 0;

    // -------------------------------------------------------------------------
    // Mouse
    // -------------------------------------------------------------------------

    /**
     * @brief Check if a mouse button is down
     * @param button Button index (0=left, 1=right, 2=middle)
     */
    [[nodiscard]] virtual bool IsMouseButtonDown(int button) const = 0;

    /**
     * @brief Check if a mouse button was pressed this frame
     */
    [[nodiscard]] virtual bool IsMouseButtonPressed(int button) const = 0;

    /**
     * @brief Check if a mouse button was released this frame
     */
    [[nodiscard]] virtual bool IsMouseButtonReleased(int button) const = 0;

    /**
     * @brief Get mouse position in window coordinates
     * @param[out] x X position
     * @param[out] y Y position
     */
    virtual void GetMousePosition(float& x, float& y) const = 0;

    /**
     * @brief Get mouse movement since last frame
     * @param[out] dx Delta X
     * @param[out] dy Delta Y
     */
    virtual void GetMouseDelta(float& dx, float& dy) const = 0;

    /**
     * @brief Get scroll wheel delta
     */
    [[nodiscard]] virtual float GetScrollDelta() const = 0;

    // -------------------------------------------------------------------------
    // Cursor control
    // -------------------------------------------------------------------------

    /**
     * @brief Lock/unlock cursor to window
     */
    virtual void SetCursorLocked(bool locked) = 0;

    /**
     * @brief Show/hide cursor
     */
    virtual void SetCursorVisible(bool visible) = 0;

    /**
     * @brief Check if cursor is locked
     */
    [[nodiscard]] virtual bool IsCursorLocked() const = 0;

    // -------------------------------------------------------------------------
    // Action mapping
    // -------------------------------------------------------------------------

    /**
     * @brief Check if a named action is active
     * @param actionName Action name as registered
     */
    [[nodiscard]] virtual bool IsActionDown(std::string_view actionName) const = 0;

    /**
     * @brief Check if a named action was triggered this frame
     */
    [[nodiscard]] virtual bool IsActionPressed(std::string_view actionName) const = 0;

    /**
     * @brief Check if a named action was released this frame
     */
    [[nodiscard]] virtual bool IsActionReleased(std::string_view actionName) const = 0;
};

// ----------------------------------------------------------------------------
// IJobService - Job/task scheduling
// ----------------------------------------------------------------------------

/**
 * @brief Job priority levels
 */
enum class ServiceJobPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief Job handle for tracking completion
 */
class IJobHandle {
public:
    virtual ~IJobHandle() = default;

    /**
     * @brief Check if job is complete
     */
    [[nodiscard]] virtual bool IsComplete() const = 0;

    /**
     * @brief Wait for job completion (blocking)
     */
    virtual void Wait() const = 0;
};

/**
 * @brief Job service interface
 *
 * Provides a minimal, focused interface for scheduling parallel work.
 * Implementations may use a thread pool, fiber system, etc.
 *
 * Single Responsibility: Scheduling and executing parallel tasks.
 */
class IJobService {
public:
    virtual ~IJobService() = default;

    /**
     * @brief Submit a job for execution
     * @param job The work to execute
     * @param priority Job priority
     * @return Handle to track completion
     */
    [[nodiscard]] virtual std::unique_ptr<IJobHandle> Submit(
        std::function<void()> job,
        ServiceJobPriority priority = ServiceJobPriority::Normal) = 0;

    /**
     * @brief Submit multiple jobs and wait for completion
     * @param jobs Vector of jobs to execute
     * @param priority Job priority
     */
    virtual void SubmitAndWait(const std::vector<std::function<void()>>& jobs,
                               ServiceJobPriority priority = ServiceJobPriority::Normal) = 0;

    /**
     * @brief Execute a parallel for loop
     * @param start Start index (inclusive)
     * @param end End index (exclusive)
     * @param func Function to call with each index
     */
    virtual void ParallelFor(size_t start, size_t end,
                             const std::function<void(size_t)>& func) = 0;

    /**
     * @brief Execute a parallel for loop with batch size
     * @param start Start index
     * @param end End index
     * @param batchSize Number of iterations per job
     * @param func Function to call with each index
     */
    virtual void ParallelFor(size_t start, size_t end, size_t batchSize,
                             const std::function<void(size_t)>& func) = 0;

    /**
     * @brief Get number of worker threads
     */
    [[nodiscard]] virtual uint32_t GetWorkerCount() const = 0;

    /**
     * @brief Get approximate number of pending jobs
     */
    [[nodiscard]] virtual size_t GetPendingJobCount() const = 0;

    /**
     * @brief Check if current thread is a worker thread
     */
    [[nodiscard]] virtual bool IsWorkerThread() const = 0;
};

// ============================================================================
// RAII Service Registration Helper
// ============================================================================

/**
 * @brief RAII helper for automatic service unregistration
 *
 * @code{.cpp}
 * {
 *     ScopedService<ILogService> scopedLog(std::make_shared<MyLogger>());
 *     // Service is available via ServiceLocator::Get<ILogService>()
 * } // Service automatically unregistered
 * @endcode
 */
template<typename TInterface>
class ScopedService {
public:
    template<typename TImpl>
    explicit ScopedService(std::shared_ptr<TImpl> instance) {
        ServiceLocator::Register<TInterface>(std::move(instance));
    }

    ~ScopedService() {
        ServiceLocator::Unregister<TInterface>();
    }

    // Non-copyable
    ScopedService(const ScopedService&) = delete;
    ScopedService& operator=(const ScopedService&) = delete;

    // Movable
    ScopedService(ScopedService&&) noexcept = default;
    ScopedService& operator=(ScopedService&&) noexcept = default;
};

} // namespace Nova
