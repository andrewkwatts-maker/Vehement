#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <optional>

namespace Vehement {
namespace WebEditor {
    class JSBridge;
}

/**
 * @brief Config file metadata
 */
struct ConfigFileInfo {
    std::string path;
    std::string relativePath;
    std::string schemaPath;
    std::string category;
    std::filesystem::file_time_type lastModified;
    size_t fileSize = 0;
    bool isValid = true;
    std::vector<std::string> dependencies;
    std::vector<std::string> dependents;
    int loadOrder = 0;
};

/**
 * @brief Config change event
 */
struct ConfigChangeEvent {
    std::string filePath;
    std::string jsonPath;        // Path within JSON (e.g., "$.entities[0].health")
    std::string oldValue;
    std::string newValue;
    std::string changeType;      // "set", "delete", "add", "move"
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Config binding definition
 */
struct ConfigBinding {
    std::string id;
    std::string configPath;      // File path
    std::string jsonPath;        // JSON path pattern (supports wildcards)
    std::string callbackId;      // Registered callback ID
    bool enabled = true;
    bool triggerOnLoad = false;  // Trigger when config is first loaded
    bool debounce = true;
    float debounceTime = 0.1f;   // seconds
};

/**
 * @brief Version control diff entry
 */
struct VersionDiff {
    std::string path;
    std::string status;  // "modified", "added", "deleted", "renamed"
    std::string oldPath; // For renamed files
    int additions = 0;
    int deletions = 0;
};

/**
 * @brief Merge conflict information
 */
struct MergeConflict {
    std::string filePath;
    int startLine;
    int endLine;
    std::string baseContent;
    std::string oursContent;
    std::string theirsContent;
    bool resolved = false;
    std::string resolution;  // "ours", "theirs", "merged"
    std::string mergedContent;
};

/**
 * @brief Config Binding System
 *
 * Bind configs to events with full version control integration:
 * - Load any JSON config as event source
 * - Bind config changes to callbacks
 * - Hot-reload on file change
 * - Version control integration (show diffs)
 * - Merge conflict resolution UI
 */
class ConfigBindingSystem {
public:
    /**
     * @brief Configuration
     */
    struct Config {
        std::string configBasePath = "assets/configs/";
        std::string schemaBasePath = "assets/schemas/";
        float fileWatchInterval = 0.5f;  // seconds
        bool enableHotReload = true;
        bool enableVersionControl = true;
        std::string vcsType = "git";     // "git", "svn", "none"
        int maxHistorySize = 100;
    };

    /**
     * @brief Callback for config changes
     */
    using ConfigChangeCallback = std::function<void(const ConfigChangeEvent&)>;

    /**
     * @brief Callback for conflict resolution
     */
    using ConflictResolvedCallback = std::function<void(const MergeConflict&)>;

    ConfigBindingSystem();
    ~ConfigBindingSystem();

    // Non-copyable
    ConfigBindingSystem(const ConfigBindingSystem&) = delete;
    ConfigBindingSystem& operator=(const ConfigBindingSystem&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the binding system
     * @param bridge Reference to JSBridge
     * @param config Configuration
     * @return true if successful
     */
    bool Initialize(WebEditor::JSBridge& bridge, const Config& config = {});

    /**
     * @brief Shutdown
     */
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update file watching and process pending changes
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Config File Management
    // =========================================================================

    /**
     * @brief Load a config file
     * @param path File path (relative to config base)
     * @return true if successful
     */
    bool LoadConfig(const std::string& path);

    /**
     * @brief Reload a config file
     * @param path File path
     * @return true if successful
     */
    bool ReloadConfig(const std::string& path);

    /**
     * @brief Unload a config file
     * @param path File path
     */
    void UnloadConfig(const std::string& path);

    /**
     * @brief Save a config file
     * @param path File path
     * @return true if successful
     */
    bool SaveConfig(const std::string& path);

    /**
     * @brief Get loaded config content
     * @param path File path
     * @return Config content as JSON string
     */
    [[nodiscard]] std::string GetConfigContent(const std::string& path) const;

    /**
     * @brief Set config content
     * @param path File path
     * @param content New content
     * @param save Save immediately
     * @return true if successful
     */
    bool SetConfigContent(const std::string& path, const std::string& content, bool save = false);

    /**
     * @brief Get value at JSON path
     * @param filePath Config file path
     * @param jsonPath JSON path
     * @return Value as JSON string
     */
    [[nodiscard]] std::string GetValue(const std::string& filePath,
                                        const std::string& jsonPath) const;

    /**
     * @brief Set value at JSON path
     * @param filePath Config file path
     * @param jsonPath JSON path
     * @param value New value as JSON string
     * @return true if successful
     */
    bool SetValue(const std::string& filePath,
                  const std::string& jsonPath,
                  const std::string& value);

    /**
     * @brief Get all loaded configs
     */
    [[nodiscard]] std::vector<ConfigFileInfo> GetLoadedConfigs() const;

    /**
     * @brief Get config info
     * @param path File path
     */
    [[nodiscard]] std::optional<ConfigFileInfo> GetConfigInfo(const std::string& path) const;

    // =========================================================================
    // Event Binding
    // =========================================================================

    /**
     * @brief Create a binding for config changes
     * @param configPath Config file path
     * @param jsonPath JSON path pattern (supports * wildcard)
     * @param callback Callback function
     * @return Binding ID
     */
    std::string CreateBinding(const std::string& configPath,
                              const std::string& jsonPath,
                              ConfigChangeCallback callback);

    /**
     * @brief Remove a binding
     * @param bindingId Binding ID
     */
    void RemoveBinding(const std::string& bindingId);

    /**
     * @brief Enable/disable a binding
     * @param bindingId Binding ID
     * @param enabled Enable state
     */
    void SetBindingEnabled(const std::string& bindingId, bool enabled);

    /**
     * @brief Get all bindings
     */
    [[nodiscard]] std::vector<ConfigBinding> GetBindings() const;

    /**
     * @brief Get bindings for a config file
     * @param configPath Config file path
     */
    [[nodiscard]] std::vector<ConfigBinding> GetBindingsForConfig(
        const std::string& configPath) const;

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /**
     * @brief Enable/disable hot reload
     */
    void SetHotReloadEnabled(bool enabled);

    /**
     * @brief Check if hot reload is enabled
     */
    [[nodiscard]] bool IsHotReloadEnabled() const { return m_config.enableHotReload; }

    /**
     * @brief Force check for file changes
     */
    void CheckForChanges();

    /**
     * @brief Get files that have changed since load
     */
    [[nodiscard]] std::vector<std::string> GetChangedFiles() const;

    // =========================================================================
    // Version Control
    // =========================================================================

    /**
     * @brief Get version control status
     * @return Vector of diffs
     */
    [[nodiscard]] std::vector<VersionDiff> GetVCSStatus() const;

    /**
     * @brief Get diff for a specific file
     * @param path File path
     * @return Diff string
     */
    [[nodiscard]] std::string GetFileDiff(const std::string& path) const;

    /**
     * @brief Get file history
     * @param path File path
     * @param maxEntries Maximum entries to return
     * @return Vector of commit info (simplified)
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> GetFileHistory(
        const std::string& path, int maxEntries = 10) const;

    /**
     * @brief Revert file to last committed version
     * @param path File path
     * @return true if successful
     */
    bool RevertFile(const std::string& path);

    /**
     * @brief Stage file for commit
     * @param path File path
     * @return true if successful
     */
    bool StageFile(const std::string& path);

    /**
     * @brief Commit staged changes
     * @param message Commit message
     * @return true if successful
     */
    bool Commit(const std::string& message);

    // =========================================================================
    // Merge Conflict Resolution
    // =========================================================================

    /**
     * @brief Get current merge conflicts
     */
    [[nodiscard]] std::vector<MergeConflict> GetMergeConflicts() const;

    /**
     * @brief Resolve conflict by choosing a side
     * @param filePath File path
     * @param resolution "ours", "theirs", or "merged"
     * @param mergedContent Content for "merged" resolution
     * @return true if successful
     */
    bool ResolveConflict(const std::string& filePath,
                         const std::string& resolution,
                         const std::string& mergedContent = "");

    /**
     * @brief Mark all conflicts as resolved
     */
    void MarkAllResolved();

    /**
     * @brief Render merge conflict resolution UI (ImGui)
     */
    void RenderConflictUI();

    // =========================================================================
    // Change History
    // =========================================================================

    /**
     * @brief Get change history
     * @param maxEntries Maximum entries
     */
    [[nodiscard]] std::vector<ConfigChangeEvent> GetChangeHistory(size_t maxEntries = 100) const;

    /**
     * @brief Clear change history
     */
    void ClearHistory();

    // =========================================================================
    // Dependencies
    // =========================================================================

    /**
     * @brief Get config dependencies
     * @param path File path
     * @return Vector of dependency paths
     */
    [[nodiscard]] std::vector<std::string> GetDependencies(const std::string& path) const;

    /**
     * @brief Get configs that depend on this one
     * @param path File path
     * @return Vector of dependent paths
     */
    [[nodiscard]] std::vector<std::string> GetDependents(const std::string& path) const;

    /**
     * @brief Get load order for all configs
     * @return Sorted vector of config paths
     */
    [[nodiscard]] std::vector<std::string> GetLoadOrder() const;

    /**
     * @brief Reload config and all dependents
     * @param path File path
     * @return Number of configs reloaded
     */
    int ReloadWithDependents(const std::string& path);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string& path)> OnConfigLoaded;
    std::function<void(const std::string& path)> OnConfigUnloaded;
    std::function<void(const std::string& path)> OnConfigSaved;
    std::function<void(const std::string& path)> OnConfigChanged;
    std::function<void(const std::string& path)> OnFileModified;
    ConflictResolvedCallback OnConflictResolved;

private:
    // File watching
    void StartFileWatching();
    void StopFileWatching();
    void ProcessFileChanges();

    // Binding helpers
    bool MatchesJsonPath(const std::string& actualPath, const std::string& pattern) const;
    void TriggerBindings(const ConfigChangeEvent& event);

    // VCS helpers
    std::string ExecuteGitCommand(const std::string& command) const;
    void ParseGitStatus(const std::string& output, std::vector<VersionDiff>& diffs) const;
    void ParseMergeConflicts();

    // JSON helpers
    void ParseDependencies(const std::string& path, const std::string& content);

    // Debouncing
    void ProcessPendingChanges();

    // JSBridge registration
    void RegisterBridgeFunctions();

    // Utility
    std::string GenerateBindingId();
    std::string NormalizePath(const std::string& path) const;

    // State
    bool m_initialized = false;
    Config m_config;
    WebEditor::JSBridge* m_bridge = nullptr;

    // Loaded configs
    std::unordered_map<std::string, std::string> m_configContents;
    std::unordered_map<std::string, ConfigFileInfo> m_configInfo;

    // Bindings
    std::unordered_map<std::string, ConfigBinding> m_bindings;
    std::unordered_map<std::string, ConfigChangeCallback> m_callbacks;

    // File watching
    float m_fileWatchTimer = 0.0f;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_lastModifiedTimes;

    // Change history
    std::deque<ConfigChangeEvent> m_changeHistory;

    // Pending changes (for debouncing)
    struct PendingChange {
        ConfigChangeEvent event;
        float timer;
        std::vector<std::string> bindingIds;
    };
    std::unordered_map<std::string, PendingChange> m_pendingChanges;

    // Merge conflicts
    std::vector<MergeConflict> m_mergeConflicts;

    // Thread safety for file watching
    std::mutex m_mutex;
};

} // namespace Vehement
