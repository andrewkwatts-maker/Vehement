/**
 * @file PrefabSystem.hpp
 * @brief Comprehensive prefab system for Nova3D/Vehement editor
 *
 * Provides prefab creation, instantiation, overrides, variants, and editing.
 * Supports nested prefabs, hot-reload, and integrates with undo/redo system.
 */

#pragma once

#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <filesystem>
#include <variant>
#include <optional>
#include <mutex>
#include <atomic>
#include <iomanip>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../scene/SceneNode.hpp"
#include "EditorCommand.hpp"

namespace fs = std::filesystem;

namespace Nova {

// Forward declarations
class Scene;
class Prefab;
class PrefabInstance;
class PrefabRegistry;
class CommandHistory;
class Texture;

// =============================================================================
// Type Definitions
// =============================================================================

/**
 * @brief Unique identifier for prefabs
 */
using PrefabId = uint64_t;

/**
 * @brief Invalid prefab ID constant
 */
constexpr PrefabId InvalidPrefabId = 0;

/**
 * @brief Property path for addressing nested properties
 *
 * Format: "node_name/property_name" or "node_name/child_name/property_name"
 */
using PropertyPath = std::string;

/**
 * @brief Variant type for property values
 */
using PropertyValue = std::variant<
    bool,
    int,
    float,
    double,
    std::string,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    glm::quat
>;

/**
 * @brief Property override entry
 */
struct PropertyOverride {
    PropertyPath path;
    PropertyValue value;
    uint64_t timestamp = 0;  // When override was applied
};

/**
 * @brief Prefab file format version
 */
constexpr uint32_t PrefabFormatVersion = 1;

/**
 * @brief Prefab change event types
 */
enum class PrefabEventType {
    Created,        // New prefab created
    Modified,       // Prefab content modified
    Saved,          // Prefab saved to file
    Loaded,         // Prefab loaded from file
    Deleted,        // Prefab deleted
    Reloaded,       // Prefab hot-reloaded from file
    InstanceCreated,    // New instance created
    InstanceDestroyed,  // Instance destroyed
    InstanceOverrideChanged  // Instance override modified
};

/**
 * @brief Callback signature for prefab change notifications
 */
using PrefabEventCallback = std::function<void(PrefabEventType, PrefabId, const std::string&)>;

// =============================================================================
// Prefab Class
// =============================================================================

/**
 * @brief Template definition for a reusable scene object hierarchy
 *
 * A Prefab stores a scene node hierarchy that can be instantiated multiple
 * times in a scene. Changes to the prefab propagate to all instances unless
 * overridden.
 *
 * Thread-safety: Prefab instances are NOT thread-safe. All operations must
 * be performed from the main/editor thread.
 */
class Prefab {
public:
    /**
     * @brief Create an empty prefab
     * @param name Display name for the prefab
     */
    explicit Prefab(const std::string& name = "New Prefab");

    /**
     * @brief Create a prefab from an existing scene node
     * @param name Display name for the prefab
     * @param sourceNode Node hierarchy to use as template (cloned)
     */
    Prefab(const std::string& name, const SceneNode& sourceNode);

    ~Prefab();

    // Non-copyable but movable
    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;
    Prefab(Prefab&&) noexcept = default;
    Prefab& operator=(Prefab&&) noexcept = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /**
     * @brief Get unique prefab identifier
     */
    [[nodiscard]] PrefabId GetId() const { return m_id; }

    /**
     * @brief Get prefab display name
     */
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    /**
     * @brief Set prefab display name
     */
    void SetName(const std::string& name);

    /**
     * @brief Get file path (empty if not saved)
     */
    [[nodiscard]] const std::string& GetFilePath() const { return m_filePath; }

    /**
     * @brief Set file path
     */
    void SetFilePath(const std::string& path) { m_filePath = path; }

    // =========================================================================
    // Template Content
    // =========================================================================

    /**
     * @brief Get root node of the prefab template
     */
    [[nodiscard]] SceneNode* GetRootNode() { return m_rootNode.get(); }
    [[nodiscard]] const SceneNode* GetRootNode() const { return m_rootNode.get(); }

    /**
     * @brief Set root node (takes ownership)
     */
    void SetRootNode(std::unique_ptr<SceneNode> root);

    /**
     * @brief Clone the root node for instantiation
     * @return New node hierarchy (caller owns)
     */
    [[nodiscard]] std::unique_ptr<SceneNode> CloneRootNode() const;

    /**
     * @brief Get all node paths in the prefab hierarchy
     * @return Vector of property paths to all nodes
     */
    [[nodiscard]] std::vector<PropertyPath> GetAllNodePaths() const;

    // =========================================================================
    // Metadata
    // =========================================================================

    /**
     * @brief Get thumbnail texture path
     */
    [[nodiscard]] const std::string& GetThumbnailPath() const { return m_thumbnailPath; }

    /**
     * @brief Set thumbnail texture path
     */
    void SetThumbnailPath(const std::string& path) { m_thumbnailPath = path; }

    /**
     * @brief Get cached thumbnail texture
     */
    [[nodiscard]] std::shared_ptr<Texture> GetThumbnail() const { return m_thumbnail; }

    /**
     * @brief Set cached thumbnail texture
     */
    void SetThumbnail(std::shared_ptr<Texture> texture) { m_thumbnail = std::move(texture); }

    // =========================================================================
    // Tags
    // =========================================================================

    /**
     * @brief Get all tags assigned to this prefab
     */
    [[nodiscard]] const std::vector<std::string>& GetTags() const { return m_tags; }

    /**
     * @brief Add a tag to this prefab
     */
    void AddTag(const std::string& tag);

    /**
     * @brief Remove a tag from this prefab
     */
    void RemoveTag(const std::string& tag);

    /**
     * @brief Check if prefab has a specific tag
     */
    [[nodiscard]] bool HasTag(const std::string& tag) const;

    /**
     * @brief Clear all tags
     */
    void ClearTags() { m_tags.clear(); }

    // =========================================================================
    // Versioning
    // =========================================================================

    /**
     * @brief Get prefab version number (incremented on save)
     */
    [[nodiscard]] uint32_t GetVersion() const { return m_version; }

    /**
     * @brief Increment version (called internally on modification)
     */
    void IncrementVersion() { ++m_version; }

    /**
     * @brief Get timestamp of last modification
     */
    [[nodiscard]] uint64_t GetLastModified() const { return m_lastModified; }

    /**
     * @brief Mark as modified (updates timestamp)
     */
    void MarkModified();

    /**
     * @brief Check if prefab has unsaved changes
     */
    [[nodiscard]] bool IsDirty() const { return m_isDirty; }

    /**
     * @brief Clear dirty flag (called after save)
     */
    void ClearDirty() { m_isDirty = false; }

    // =========================================================================
    // Variant Support
    // =========================================================================

    /**
     * @brief Get base prefab ID (for variants)
     */
    [[nodiscard]] PrefabId GetBasePrefabId() const { return m_basePrefabId; }

    /**
     * @brief Set base prefab for variant
     */
    void SetBasePrefabId(PrefabId baseId) { m_basePrefabId = baseId; }

    /**
     * @brief Check if this is a variant prefab
     */
    [[nodiscard]] bool IsVariant() const { return m_basePrefabId != InvalidPrefabId; }

    /**
     * @brief Get overrides from base prefab (for variants)
     */
    [[nodiscard]] const std::vector<PropertyOverride>& GetVariantOverrides() const { return m_variantOverrides; }

    /**
     * @brief Add/update variant override
     */
    void SetVariantOverride(const PropertyPath& path, const PropertyValue& value);

    /**
     * @brief Remove variant override
     */
    void RemoveVariantOverride(const PropertyPath& path);

    /**
     * @brief Clear all variant overrides
     */
    void ClearVariantOverrides() { m_variantOverrides.clear(); }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize prefab to JSON string
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Deserialize prefab from JSON string
     * @return true on success
     */
    bool FromJson(const std::string& json);

    /**
     * @brief Save prefab to file
     * @param path File path (updates internal path if empty)
     * @return true on success
     */
    bool SaveToFile(const std::string& path = "");

    /**
     * @brief Load prefab from file
     * @param path File path
     * @return true on success
     */
    bool LoadFromFile(const std::string& path);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Callback invoked when prefab is modified
     */
    std::function<void(Prefab*)> OnModified;

private:
    static std::atomic<PrefabId> s_nextId;

    PrefabId m_id;
    std::string m_name;
    std::string m_filePath;
    std::string m_thumbnailPath;
    std::shared_ptr<Texture> m_thumbnail;

    std::unique_ptr<SceneNode> m_rootNode;
    std::vector<std::string> m_tags;

    uint32_t m_version = 1;
    uint64_t m_lastModified = 0;
    bool m_isDirty = false;

    // Variant support
    PrefabId m_basePrefabId = InvalidPrefabId;
    std::vector<PropertyOverride> m_variantOverrides;

    // Helper methods
    void CollectNodePaths(const SceneNode* node, const std::string& prefix,
                          std::vector<PropertyPath>& paths) const;
    std::unique_ptr<SceneNode> CloneNode(const SceneNode* source) const;
    void SerializeNodeToJson(std::ostringstream& ss, const SceneNode* node, int indent) const;
};

// =============================================================================
// PrefabInstance Class
// =============================================================================

/**
 * @brief Instance of a prefab in a scene with optional property overrides
 *
 * PrefabInstance wraps a SceneNode hierarchy and tracks which properties
 * have been modified from the source prefab. When the source prefab changes,
 * non-overridden properties are updated automatically.
 */
class PrefabInstance {
public:
    /**
     * @brief Create an instance referencing a prefab
     * @param prefabId Source prefab ID
     * @param rootNode Instantiated root node (takes ownership)
     */
    PrefabInstance(PrefabId prefabId, std::unique_ptr<SceneNode> rootNode);

    ~PrefabInstance();

    // Non-copyable but movable
    PrefabInstance(const PrefabInstance&) = delete;
    PrefabInstance& operator=(const PrefabInstance&) = delete;
    PrefabInstance(PrefabInstance&&) noexcept = default;
    PrefabInstance& operator=(PrefabInstance&&) noexcept = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /**
     * @brief Get unique instance identifier
     */
    [[nodiscard]] uint64_t GetInstanceId() const { return m_instanceId; }

    /**
     * @brief Get source prefab ID
     */
    [[nodiscard]] PrefabId GetPrefabId() const { return m_prefabId; }

    /**
     * @brief Get the instantiated root node
     */
    [[nodiscard]] SceneNode* GetRootNode() { return m_rootNode.get(); }
    [[nodiscard]] const SceneNode* GetRootNode() const { return m_rootNode.get(); }

    /**
     * @brief Transfer root node ownership (for unpacking)
     */
    [[nodiscard]] std::unique_ptr<SceneNode> ReleaseRootNode() { return std::move(m_rootNode); }

    /**
     * @brief Get version of prefab when instance was created
     */
    [[nodiscard]] uint32_t GetSourceVersion() const { return m_sourceVersion; }

    /**
     * @brief Update source version (after syncing with prefab)
     */
    void SetSourceVersion(uint32_t version) { m_sourceVersion = version; }

    // =========================================================================
    // Override System
    // =========================================================================

    /**
     * @brief Check if a property is overridden from the source prefab
     * @param path Property path (e.g., "Child/position")
     * @return true if property has been overridden
     */
    [[nodiscard]] bool IsOverridden(const PropertyPath& path) const;

    /**
     * @brief Get all overridden property paths
     */
    [[nodiscard]] std::vector<PropertyPath> GetOverriddenPaths() const;

    /**
     * @brief Get override value for a property
     * @param path Property path
     * @return Override value or nullopt if not overridden
     */
    [[nodiscard]] std::optional<PropertyValue> GetOverride(const PropertyPath& path) const;

    /**
     * @brief Apply an override to a property
     * @param path Property path
     * @param value New value
     */
    void ApplyOverride(const PropertyPath& path, const PropertyValue& value);

    /**
     * @brief Revert a single property override
     * @param path Property path to revert
     * @return true if override was removed
     */
    bool RevertOverride(const PropertyPath& path);

    /**
     * @brief Revert all property overrides
     */
    void RevertAllOverrides();

    /**
     * @brief Get count of active overrides
     */
    [[nodiscard]] size_t GetOverrideCount() const { return m_overrides.size(); }

    // =========================================================================
    // Nested Prefab Support
    // =========================================================================

    /**
     * @brief Get nested prefab instances within this instance
     */
    [[nodiscard]] const std::vector<std::unique_ptr<PrefabInstance>>& GetNestedInstances() const {
        return m_nestedInstances;
    }

    /**
     * @brief Add a nested prefab instance
     */
    void AddNestedInstance(std::unique_ptr<PrefabInstance> instance);

    /**
     * @brief Remove a nested prefab instance
     */
    std::unique_ptr<PrefabInstance> RemoveNestedInstance(uint64_t instanceId);

    /**
     * @brief Find nested instance by ID
     */
    [[nodiscard]] PrefabInstance* FindNestedInstance(uint64_t instanceId);

    // =========================================================================
    // Synchronization
    // =========================================================================

    /**
     * @brief Sync instance with source prefab
     *
     * Updates non-overridden properties to match the current prefab state.
     * Called automatically when prefab is modified.
     *
     * @param prefab Source prefab
     * @return true if any properties were updated
     */
    bool SyncWithPrefab(const Prefab& prefab);

    /**
     * @brief Check if instance is out of sync with prefab
     */
    [[nodiscard]] bool NeedsSync(const Prefab& prefab) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize instance state to JSON
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Deserialize instance state from JSON
     */
    bool FromJson(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Callback invoked when overrides change
     */
    std::function<void(PrefabInstance*, const PropertyPath&)> OnOverrideChanged;

private:
    static std::atomic<uint64_t> s_nextInstanceId;

    uint64_t m_instanceId;
    PrefabId m_prefabId;
    std::unique_ptr<SceneNode> m_rootNode;
    uint32_t m_sourceVersion = 0;

    // Override storage (path -> value)
    std::unordered_map<PropertyPath, PropertyOverride> m_overrides;

    // Nested instances
    std::vector<std::unique_ptr<PrefabInstance>> m_nestedInstances;

    // Helper methods
    void ApplyPropertyToNode(SceneNode* node, const PropertyPath& path, const PropertyValue& value);
    PropertyValue GetPropertyFromNode(const SceneNode* node, const PropertyPath& path) const;
    SceneNode* FindNodeByPath(const PropertyPath& path) const;
    void SyncNodeWithTemplate(SceneNode* instance, const SceneNode* templateNode,
                               const std::string& pathPrefix);
};

// =============================================================================
// PrefabRegistry Class (Singleton)
// =============================================================================

/**
 * @brief Central registry for managing all prefabs
 *
 * Provides loading, saving, querying, and instance tracking for prefabs.
 * Implements hot-reload when prefab files change on disk.
 *
 * Thread-safety: Registry operations are protected by a mutex for basic
 * thread-safety, but extensive editing should be done from the main thread.
 */
class PrefabRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static PrefabRegistry& Instance();

    // Delete copy/move operations for singleton
    PrefabRegistry(const PrefabRegistry&) = delete;
    PrefabRegistry& operator=(const PrefabRegistry&) = delete;
    PrefabRegistry(PrefabRegistry&&) = delete;
    PrefabRegistry& operator=(PrefabRegistry&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize registry with asset path
     * @param prefabDirectory Root directory for .prefab files
     */
    bool Initialize(const std::string& prefabDirectory);

    /**
     * @brief Shutdown registry and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if registry is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Prefab Management
    // =========================================================================

    /**
     * @brief Load a prefab from file
     * @param path File path (.prefab)
     * @return Loaded prefab or nullptr on failure
     */
    Prefab* LoadPrefab(const std::string& path);

    /**
     * @brief Create a new prefab from a scene node
     * @param sourceNode Node hierarchy to convert to prefab
     * @param name Prefab name
     * @return New prefab (owned by registry)
     */
    Prefab* CreatePrefab(const SceneNode& sourceNode, const std::string& name);

    /**
     * @brief Create an empty prefab
     * @param name Prefab name
     * @return New prefab (owned by registry)
     */
    Prefab* CreateEmptyPrefab(const std::string& name);

    /**
     * @brief Save a prefab to file
     * @param prefab Prefab to save
     * @param path File path (uses prefab's path if empty)
     * @return true on success
     */
    bool SavePrefab(Prefab* prefab, const std::string& path = "");

    /**
     * @brief Delete a prefab from registry
     * @param id Prefab ID to delete
     * @return true if prefab was deleted
     */
    bool DeletePrefab(PrefabId id);

    /**
     * @brief Get prefab by ID
     * @param id Prefab ID
     * @return Prefab pointer or nullptr
     */
    [[nodiscard]] Prefab* GetPrefab(PrefabId id);
    [[nodiscard]] const Prefab* GetPrefab(PrefabId id) const;

    /**
     * @brief Get prefab by file path
     */
    [[nodiscard]] Prefab* GetPrefabByPath(const std::string& path);

    // =========================================================================
    // Querying
    // =========================================================================

    /**
     * @brief Get all registered prefabs
     */
    [[nodiscard]] std::vector<Prefab*> GetAllPrefabs();
    [[nodiscard]] std::vector<const Prefab*> GetAllPrefabs() const;

    /**
     * @brief Find prefab by name
     * @param name Prefab name (case-insensitive)
     * @return Prefab pointer or nullptr
     */
    [[nodiscard]] Prefab* FindPrefabByName(const std::string& name);

    /**
     * @brief Find all prefabs with a specific tag
     * @param tag Tag to search for
     * @return Vector of matching prefabs
     */
    [[nodiscard]] std::vector<Prefab*> FindPrefabsByTag(const std::string& tag);

    /**
     * @brief Find prefabs matching a predicate
     */
    [[nodiscard]] std::vector<Prefab*> FindPrefabs(
        const std::function<bool(const Prefab&)>& predicate);

    /**
     * @brief Get total prefab count
     */
    [[nodiscard]] size_t GetPrefabCount() const;

    // =========================================================================
    // Instantiation
    // =========================================================================

    /**
     * @brief Instantiate a prefab in a scene
     * @param prefab Prefab to instantiate
     * @param parent Parent node (nullptr for root)
     * @param position Initial position
     * @return New instance (ownership retained by registry)
     */
    PrefabInstance* InstantiatePrefab(Prefab* prefab, SceneNode* parent,
                                       const glm::vec3& position = glm::vec3(0.0f));

    /**
     * @brief Instantiate a prefab by ID
     */
    PrefabInstance* InstantiatePrefab(PrefabId prefabId, SceneNode* parent,
                                       const glm::vec3& position = glm::vec3(0.0f));

    /**
     * @brief Unpack a prefab instance (break prefab link)
     *
     * Removes the prefab reference, leaving behind a regular scene node.
     * Overrides are baked into the node properties.
     *
     * @param instance Instance to unpack
     * @return Root node (removed from instance tracking)
     */
    std::unique_ptr<SceneNode> UnpackPrefab(PrefabInstance* instance);

    /**
     * @brief Destroy a prefab instance
     * @param instanceId Instance ID to destroy
     */
    void DestroyInstance(uint64_t instanceId);

    /**
     * @brief Get all instances of a prefab
     */
    [[nodiscard]] std::vector<PrefabInstance*> GetInstancesOf(PrefabId prefabId);

    /**
     * @brief Get instance by ID
     */
    [[nodiscard]] PrefabInstance* GetInstance(uint64_t instanceId);

    /**
     * @brief Get total instance count
     */
    [[nodiscard]] size_t GetInstanceCount() const;

    // =========================================================================
    // Prefab Operations
    // =========================================================================

    /**
     * @brief Update prefab from an instance (apply overrides to source)
     * @param instance Instance with overrides to apply
     * @return true on success
     */
    bool UpdatePrefabFromInstance(PrefabInstance* instance);

    /**
     * @brief Create variant prefab from an existing prefab
     * @param basePrefab Base prefab
     * @param variantName Name for the variant
     * @return New variant prefab
     */
    Prefab* CreateVariant(Prefab* basePrefab, const std::string& variantName);

    /**
     * @brief Sync all instances of a prefab
     * @param prefabId Prefab ID
     * @return Number of instances synced
     */
    size_t SyncAllInstances(PrefabId prefabId);

    // =========================================================================
    // Prefab Editing Mode
    // =========================================================================

    /**
     * @brief Open a prefab for isolated editing
     * @param prefab Prefab to edit
     * @return true if edit mode entered successfully
     */
    bool OpenPrefabForEditing(Prefab* prefab);

    /**
     * @brief Save changes made in prefab edit mode
     * @return true on success
     */
    bool SavePrefabChanges();

    /**
     * @brief Discard changes and exit prefab edit mode
     */
    void DiscardPrefabChanges();

    /**
     * @brief Check if in prefab editing mode
     */
    [[nodiscard]] bool IsEditingPrefab() const { return m_editingPrefab != nullptr; }

    /**
     * @brief Get currently editing prefab
     */
    [[nodiscard]] Prefab* GetEditingPrefab() { return m_editingPrefab; }

    /**
     * @brief Get isolated scene for prefab editing
     */
    [[nodiscard]] Scene* GetEditingScene() { return m_editingScene.get(); }

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /**
     * @brief Check for file changes and reload modified prefabs
     */
    void CheckForFileChanges();

    /**
     * @brief Enable/disable automatic hot reload
     */
    void SetHotReloadEnabled(bool enabled) { m_hotReloadEnabled = enabled; }

    /**
     * @brief Check if hot reload is enabled
     */
    [[nodiscard]] bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }

    // =========================================================================
    // Events
    // =========================================================================

    /**
     * @brief Register callback for prefab events
     * @param callback Event handler
     * @return Registration ID for unregistering
     */
    uint32_t RegisterEventCallback(PrefabEventCallback callback);

    /**
     * @brief Unregister event callback
     * @param id Registration ID
     */
    void UnregisterEventCallback(uint32_t id);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get prefab directory
     */
    [[nodiscard]] const std::string& GetPrefabDirectory() const { return m_prefabDirectory; }

    /**
     * @brief Set prefab directory
     */
    void SetPrefabDirectory(const std::string& path) { m_prefabDirectory = path; }

private:
    PrefabRegistry();
    ~PrefabRegistry();

    // Prefab storage
    std::unordered_map<PrefabId, std::unique_ptr<Prefab>> m_prefabs;
    std::unordered_map<std::string, PrefabId> m_pathToId;

    // Instance storage
    std::unordered_map<uint64_t, std::unique_ptr<PrefabInstance>> m_instances;
    std::unordered_map<PrefabId, std::unordered_set<uint64_t>> m_prefabInstances;

    // Thread safety
    mutable std::mutex m_mutex;

    // Configuration
    std::string m_prefabDirectory;
    bool m_initialized = false;
    bool m_hotReloadEnabled = true;

    // Prefab editing mode
    Prefab* m_editingPrefab = nullptr;
    std::unique_ptr<Prefab> m_editingBackup;
    std::unique_ptr<Scene> m_editingScene;

    // File monitoring
    std::unordered_map<std::string, uint64_t> m_fileTimestamps;

    // Event system
    std::unordered_map<uint32_t, PrefabEventCallback> m_eventCallbacks;
    std::atomic<uint32_t> m_nextCallbackId{1};

    // Helper methods
    void NotifyEvent(PrefabEventType type, PrefabId prefabId, const std::string& data = "");
    uint64_t GetFileTimestamp(const std::string& path) const;
    void ScanPrefabDirectory();
};

// =============================================================================
// Prefab Commands for Undo/Redo
// =============================================================================

/**
 * @brief Command for creating a new prefab
 */
class CreatePrefabCommand : public ICommand {
public:
    /**
     * @brief Create a command to create a prefab from selection
     * @param sourceNode Source node to convert
     * @param name Prefab name
     */
    CreatePrefabCommand(const SceneNode& sourceNode, const std::string& name);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    /**
     * @brief Get the created prefab (valid after Execute)
     */
    [[nodiscard]] Prefab* GetCreatedPrefab() const { return m_createdPrefab; }

private:
    static std::unique_ptr<SceneNode> CloneNodeDeep(const SceneNode* source);

    std::unique_ptr<SceneNode> m_sourceClone;
    std::string m_prefabName;
    Prefab* m_createdPrefab = nullptr;
    PrefabId m_createdId = InvalidPrefabId;
};

/**
 * @brief Command for instantiating a prefab
 */
class InstantiatePrefabCommand : public ICommand {
public:
    /**
     * @brief Create a command to instantiate a prefab
     * @param prefab Prefab to instantiate
     * @param parent Parent scene node
     * @param position Initial position
     */
    InstantiatePrefabCommand(Prefab* prefab, SceneNode* parent, const glm::vec3& position);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    /**
     * @brief Get the created instance (valid after Execute)
     */
    [[nodiscard]] PrefabInstance* GetCreatedInstance() const { return m_createdInstance; }

private:
    Prefab* m_prefab;
    SceneNode* m_parent;
    glm::vec3 m_position;
    PrefabInstance* m_createdInstance = nullptr;
    uint64_t m_instanceId = 0;
};

/**
 * @brief Command for unpacking a prefab instance
 */
class UnpackPrefabCommand : public ICommand {
public:
    /**
     * @brief Create a command to unpack a prefab instance
     * @param instance Instance to unpack
     */
    explicit UnpackPrefabCommand(PrefabInstance* instance);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    uint64_t m_instanceId;
    PrefabId m_prefabId;
    std::string m_overridesJson;  // Store overrides for undo
    SceneNode* m_unpackedNode = nullptr;
};

/**
 * @brief Command for modifying a prefab instance override
 */
class OverridePrefabPropertyCommand : public ICommand {
public:
    /**
     * @brief Create a command to override a property
     * @param instance Target instance
     * @param path Property path
     * @param newValue New value
     */
    OverridePrefabPropertyCommand(PrefabInstance* instance, const PropertyPath& path,
                                   const PropertyValue& newValue);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

private:
    uint64_t m_instanceId;
    PropertyPath m_path;
    PropertyValue m_oldValue;
    PropertyValue m_newValue;
    bool m_hadPreviousOverride = false;
};

/**
 * @brief Command for reverting a prefab instance override
 */
class RevertOverrideCommand : public ICommand {
public:
    /**
     * @brief Create a command to revert an override
     * @param instance Target instance
     * @param path Property path to revert
     */
    RevertOverrideCommand(PrefabInstance* instance, const PropertyPath& path);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    uint64_t m_instanceId;
    PropertyPath m_path;
    PropertyOverride m_removedOverride;
};

/**
 * @brief Command for applying instance changes to prefab
 */
class ApplyToPrefabCommand : public ICommand {
public:
    /**
     * @brief Create a command to apply instance changes to prefab
     * @param instance Source instance with modifications
     */
    explicit ApplyToPrefabCommand(PrefabInstance* instance);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    uint64_t m_instanceId;
    PrefabId m_prefabId;
    std::string m_oldPrefabJson;  // Backup for undo
    std::vector<PropertyOverride> m_appliedOverrides;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Create a prefab from the current selection
 * @param selectedNodes Nodes to include in prefab
 * @param name Prefab name
 * @param history Command history for undo support
 * @return Created prefab or nullptr
 */
Prefab* CreatePrefabFromSelection(const std::vector<SceneNode*>& selectedNodes,
                                   const std::string& name,
                                   CommandHistory* history = nullptr);

/**
 * @brief Find prefab instance containing a scene node
 * @param node Node to search for
 * @return Instance containing the node, or nullptr
 */
PrefabInstance* FindInstanceForNode(const SceneNode* node);

/**
 * @brief Check if a node is part of a prefab instance
 */
bool IsPartOfPrefab(const SceneNode* node);

/**
 * @brief Get the root node of a prefab instance containing a node
 */
SceneNode* GetPrefabRoot(SceneNode* node);

/**
 * @brief Build property path from root to a specific node
 */
PropertyPath BuildPropertyPath(const SceneNode* root, const SceneNode* target,
                                const std::string& propertyName);

/**
 * @brief Parse property path into node path and property name
 */
std::pair<std::string, std::string> ParsePropertyPath(const PropertyPath& path);

/**
 * @brief Convert PropertyValue to string representation
 */
std::string PropertyValueToString(const PropertyValue& value);

/**
 * @brief Parse string to PropertyValue
 */
PropertyValue StringToPropertyValue(const std::string& str, const std::string& typeHint);

} // namespace Nova
