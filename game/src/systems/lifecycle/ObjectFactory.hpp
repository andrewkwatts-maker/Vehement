#pragma once

#include "ILifecycle.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <mutex>

// Forward declare nlohmann json
namespace nlohmann {
    template<typename, typename, typename...> class basic_json;
    using json = basic_json<std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, double, std::allocator, void>;
}

namespace Vehement {
namespace Lifecycle {

// Forward declarations
class LifecycleManager;

// ============================================================================
// Object Definition
// ============================================================================

/**
 * @brief Loaded object definition from JSON config
 *
 * Stores the parsed configuration for an object type, allowing
 * efficient creation of instances.
 */
struct ObjectDefinition {
    std::string id;                 // Unique identifier
    std::string type;               // Base type name
    std::string baseId;             // Parent definition ID (inheritance)
    std::string displayName;        // Human-readable name
    std::string description;

    // Lifecycle configuration
    struct LifecycleConfig {
        std::string tickGroup = "AI";
        float tickInterval = 0.0f;
        int priority = 0;

        struct EventScripts {
            std::string onCreate;
            std::string onTick;
            std::string onDamaged;
            std::string onKill;
            std::string onDestroy;
            std::unordered_map<std::string, std::string> custom;
        } events;
    } lifecycle;

    // Components
    std::vector<std::string> components;

    // Raw JSON for full config access
    std::shared_ptr<std::string> rawJson;

    // Source file info
    std::string sourcePath;
    int64_t lastModified = 0;

    // Validation
    bool isValid = false;
    std::vector<std::string> validationErrors;
};

// ============================================================================
// Config Loader
// ============================================================================

/**
 * @brief Interface for loading object configs
 */
class IConfigLoader {
public:
    virtual ~IConfigLoader() = default;

    /**
     * @brief Load config from JSON string
     */
    virtual bool Load(const std::string& json, ObjectDefinition& outDef) = 0;

    /**
     * @brief Validate a definition
     */
    virtual bool Validate(const ObjectDefinition& def,
                         std::vector<std::string>& outErrors) = 0;
};

/**
 * @brief Default JSON config loader
 */
class JsonConfigLoader : public IConfigLoader {
public:
    bool Load(const std::string& json, ObjectDefinition& outDef) override;
    bool Validate(const ObjectDefinition& def,
                 std::vector<std::string>& outErrors) override;
};

// ============================================================================
// Prototype
// ============================================================================

/**
 * @brief Prototype object for cloning
 *
 * Stores a pre-configured object that can be efficiently cloned.
 */
struct Prototype {
    std::string id;
    std::unique_ptr<ILifecycle> instance;
    ObjectDefinition definition;

    // Clone the prototype
    std::unique_ptr<ILifecycle> Clone() const;
};

// ============================================================================
// ObjectFactory
// ============================================================================

/**
 * @brief Factory for creating game objects from configuration
 *
 * Features:
 * - Register types with JSON config loaders
 * - Create instances from config files
 * - Prototype pattern for cloning
 * - Hot-reload object definitions
 * - Definition inheritance
 * - Async loading support
 *
 * Usage:
 * 1. Register types: RegisterType<Unit>("unit")
 * 2. Load definitions: LoadDefinition("configs/units/soldier.json")
 * 3. Create instances: CreateFromDefinition("soldier")
 */
class ObjectFactory {
public:
    ObjectFactory();
    ~ObjectFactory();

    // Disable copy
    ObjectFactory(const ObjectFactory&) = delete;
    ObjectFactory& operator=(const ObjectFactory&) = delete;

    // =========================================================================
    // Type Registration
    // =========================================================================

    /**
     * @brief Creator function type
     */
    using CreatorFunc = std::function<std::unique_ptr<ILifecycle>()>;

    /**
     * @brief Register a type with creator function
     *
     * @param typeName Type identifier used in configs
     * @param creator Function to create instances
     */
    void RegisterType(const std::string& typeName, CreatorFunc creator);

    /**
     * @brief Register a type (template version)
     */
    template<typename T>
    void RegisterType(const std::string& typeName);

    /**
     * @brief Register a custom config loader for a type
     */
    void RegisterConfigLoader(const std::string& typeName,
                             std::unique_ptr<IConfigLoader> loader);

    /**
     * @brief Check if type is registered
     */
    [[nodiscard]] bool IsTypeRegistered(const std::string& typeName) const;

    /**
     * @brief Get all registered type names
     */
    [[nodiscard]] std::vector<std::string> GetRegisteredTypes() const;

    // =========================================================================
    // Definition Loading
    // =========================================================================

    /**
     * @brief Load definition from file
     *
     * @param filePath Path to JSON config file
     * @return true if loaded successfully
     */
    bool LoadDefinition(const std::string& filePath);

    /**
     * @brief Load definition from JSON string
     *
     * @param id Definition ID
     * @param json JSON config string
     * @return true if loaded successfully
     */
    bool LoadDefinitionFromString(const std::string& id, const std::string& json);

    /**
     * @brief Load all definitions from directory
     *
     * @param dirPath Directory containing JSON configs
     * @param recursive Search subdirectories
     * @return Number of definitions loaded
     */
    size_t LoadDefinitionsFromDirectory(const std::string& dirPath, bool recursive = true);

    /**
     * @brief Unload a definition
     */
    void UnloadDefinition(const std::string& id);

    /**
     * @brief Unload all definitions
     */
    void UnloadAllDefinitions();

    /**
     * @brief Get definition by ID
     */
    [[nodiscard]] const ObjectDefinition* GetDefinition(const std::string& id) const;

    /**
     * @brief Get all definition IDs
     */
    [[nodiscard]] std::vector<std::string> GetDefinitionIds() const;

    /**
     * @brief Get definitions by type
     */
    [[nodiscard]] std::vector<const ObjectDefinition*> GetDefinitionsByType(
        const std::string& typeName) const;

    // =========================================================================
    // Object Creation
    // =========================================================================

    /**
     * @brief Create object from definition ID
     *
     * @param definitionId Definition to use
     * @param configOverride Optional JSON to override definition values
     * @return Created object (ownership transferred to caller)
     */
    std::unique_ptr<ILifecycle> CreateFromDefinition(
        const std::string& definitionId,
        const nlohmann::json* configOverride = nullptr);

    /**
     * @brief Create object by type name
     *
     * @param typeName Registered type name
     * @param config JSON configuration
     * @return Created object
     */
    std::unique_ptr<ILifecycle> CreateByType(const std::string& typeName,
                                             const nlohmann::json& config);

    /**
     * @brief Create object (template version)
     */
    template<typename T>
    std::unique_ptr<T> Create(const nlohmann::json& config = {});

    // =========================================================================
    // Prototypes
    // =========================================================================

    /**
     * @brief Create a prototype from definition
     *
     * @param definitionId Definition to use
     * @return true if prototype created
     */
    bool CreatePrototype(const std::string& definitionId);

    /**
     * @brief Clone from prototype
     *
     * @param prototypeId Prototype to clone
     * @return Cloned object
     */
    std::unique_ptr<ILifecycle> CloneFromPrototype(const std::string& prototypeId);

    /**
     * @brief Check if prototype exists
     */
    [[nodiscard]] bool HasPrototype(const std::string& id) const;

    /**
     * @brief Remove a prototype
     */
    void RemovePrototype(const std::string& id);

    /**
     * @brief Clear all prototypes
     */
    void ClearPrototypes();

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /**
     * @brief Enable hot reload monitoring
     *
     * @param enabled Enable/disable
     * @param pollIntervalMs How often to check for changes (ms)
     */
    void SetHotReloadEnabled(bool enabled, uint32_t pollIntervalMs = 1000);

    /**
     * @brief Check for and apply definition changes
     *
     * Call this periodically or in response to file system events.
     *
     * @return Number of definitions reloaded
     */
    size_t CheckForReloads();

    /**
     * @brief Force reload a definition
     */
    bool ReloadDefinition(const std::string& id);

    /**
     * @brief Callback when definition reloaded
     */
    using ReloadCallback = std::function<void(const std::string& definitionId)>;
    void SetOnDefinitionReloaded(ReloadCallback callback) {
        m_onReloaded = std::move(callback);
    }

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate all loaded definitions
     *
     * @return List of validation errors (empty if all valid)
     */
    [[nodiscard]] std::vector<std::string> ValidateAllDefinitions() const;

    /**
     * @brief Validate single definition
     */
    [[nodiscard]] std::vector<std::string> ValidateDefinition(
        const std::string& id) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        size_t registeredTypes = 0;
        size_t loadedDefinitions = 0;
        size_t prototypes = 0;
        size_t objectsCreated = 0;
        size_t clonesCreated = 0;
        size_t reloadsPerformed = 0;
    };

    [[nodiscard]] Stats GetStats() const;

private:
    // Type registry
    struct TypeInfo {
        CreatorFunc creator;
        std::unique_ptr<IConfigLoader> loader;
    };
    std::unordered_map<std::string, TypeInfo> m_types;

    // Definitions
    std::unordered_map<std::string, ObjectDefinition> m_definitions;

    // Prototypes
    std::unordered_map<std::string, std::unique_ptr<Prototype>> m_prototypes;

    // Hot reload
    bool m_hotReloadEnabled = false;
    uint32_t m_hotReloadPollMs = 1000;
    std::unordered_map<std::string, int64_t> m_fileModTimes;
    ReloadCallback m_onReloaded;

    // Default loader
    std::unique_ptr<JsonConfigLoader> m_defaultLoader;

    // Stats
    mutable Stats m_stats;

    // Thread safety
    mutable std::mutex m_mutex;

    // Helpers
    bool ResolveInheritance(ObjectDefinition& def);
    int64_t GetFileModTime(const std::string& path) const;
    IConfigLoader* GetLoader(const std::string& typeName);
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T>
void ObjectFactory::RegisterType(const std::string& typeName) {
    static_assert(std::is_base_of_v<ILifecycle, T>,
                  "Type must derive from ILifecycle");

    RegisterType(typeName, []() -> std::unique_ptr<ILifecycle> {
        return std::make_unique<T>();
    });
}

template<typename T>
std::unique_ptr<T> ObjectFactory::Create(const nlohmann::json& config) {
    static_assert(std::is_base_of_v<ILifecycle, T>,
                  "Type must derive from ILifecycle");

    auto obj = std::make_unique<T>();
    obj->OnCreate(config);
    m_stats.objectsCreated++;
    return obj;
}

// ============================================================================
// Global Factory Access
// ============================================================================

/**
 * @brief Get global object factory instance
 */
ObjectFactory& GetGlobalObjectFactory();

// ============================================================================
// Registration Macros
// ============================================================================

/**
 * @brief Register type at static initialization
 */
#define REGISTER_LIFECYCLE_TYPE(TypeName, ClassName) \
    namespace { \
        struct ClassName##_Registrar { \
            ClassName##_Registrar() { \
                GetGlobalObjectFactory().RegisterType<ClassName>(TypeName); \
            } \
        }; \
        static ClassName##_Registrar g_##ClassName##_registrar; \
    }

} // namespace Lifecycle
} // namespace Vehement
