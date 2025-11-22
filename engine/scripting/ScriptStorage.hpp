#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <chrono>
#include <functional>

namespace Nova {
namespace Scripting {

/**
 * @brief Storage location types for scripts
 */
enum class StorageLocation {
    Inline,     // Stored inline in JSON config
    Adjacent,   // Stored in .py file adjacent to config
    Central     // Stored in central scripts folder
};

/**
 * @brief Information about a stored script
 */
struct ScriptInfo {
    std::string name;
    std::string path;
    std::string category;
    StorageLocation location;
    std::string content;
    size_t contentHash;
    std::chrono::system_clock::time_point lastModified;
    std::chrono::system_clock::time_point lastAccessed;
    bool isValid = true;
    std::string validationError;

    // Metadata
    std::string author;
    std::string description;
    std::string version;
    std::vector<std::string> tags;
    std::vector<std::string> dependencies;
};

/**
 * @brief Script search criteria
 */
struct ScriptSearchCriteria {
    std::string namePattern;        // Glob pattern for name
    std::string category;           // Filter by category
    StorageLocation location = StorageLocation::Central;
    std::vector<std::string> tags;  // Must have all these tags
    bool validOnly = false;         // Only return valid scripts
};

/**
 * @brief Manages script storage across different locations
 *
 * Supports three storage modes:
 * 1. Inline - Scripts stored directly in JSON config files
 * 2. Adjacent - Scripts in .py files next to their configs
 * 3. Central - Scripts in organized scripts/ folder structure
 *
 * Usage:
 * @code
 * ScriptStorage storage;
 * storage.Initialize("game/assets");
 *
 * // Store inline in JSON
 * storage.StoreInline("configs/units/zombie.json", "on_damage",
 *                     "def on_damage(entity, damage):\n    pass");
 *
 * // Store in adjacent .py file
 * storage.StoreAdjacent("configs/units/zombie.json",
 *                       "# Zombie AI Script\ndef think(): pass");
 *
 * // Store in central location
 * storage.StoreCentral("ai", "patrol", "def patrol(): pass");
 *
 * // Retrieve
 * auto script = storage.GetScript("scripts/ai/patrol.py");
 * @endcode
 */
class ScriptStorage {
public:
    ScriptStorage();
    ~ScriptStorage();

    // Non-copyable
    ScriptStorage(const ScriptStorage&) = delete;
    ScriptStorage& operator=(const ScriptStorage&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize storage with base path
     * @param basePath Base path for all script operations
     * @return true if initialization succeeded
     */
    bool Initialize(const std::string& basePath);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Set the scripts folder path
     */
    void SetScriptsPath(const std::string& path) { m_scriptsPath = path; }

    /**
     * @brief Get the scripts folder path
     */
    [[nodiscard]] const std::string& GetScriptsPath() const { return m_scriptsPath; }

    // =========================================================================
    // Inline Storage (in JSON configs)
    // =========================================================================

    /**
     * @brief Store script inline in a JSON config
     * @param configPath Path to the JSON config file
     * @param functionName Name of the function/script
     * @param code Python source code
     * @param jsonPath JSON path where to store (default: "scripts.{functionName}")
     * @return true if successful
     */
    bool StoreInline(const std::string& configPath,
                     const std::string& functionName,
                     const std::string& code,
                     const std::string& jsonPath = "");

    /**
     * @brief Get script stored inline in JSON
     * @param configPath Path to the JSON config file
     * @param functionName Name of the function
     * @return Script content or empty if not found
     */
    std::string GetInlineScript(const std::string& configPath,
                                const std::string& functionName);

    /**
     * @brief List all inline scripts in a config
     */
    std::vector<std::string> ListInlineScripts(const std::string& configPath);

    /**
     * @brief Remove inline script from config
     */
    bool RemoveInlineScript(const std::string& configPath,
                           const std::string& functionName);

    // =========================================================================
    // Adjacent Storage (.py files next to configs)
    // =========================================================================

    /**
     * @brief Store script in .py file adjacent to config
     * @param configPath Path to the config file
     * @param code Python source code
     * @param scriptName Optional script name (default: config name)
     * @return Path to created script file
     */
    std::string StoreAdjacent(const std::string& configPath,
                              const std::string& code,
                              const std::string& scriptName = "");

    /**
     * @brief Get adjacent script path for a config
     */
    std::string GetAdjacentScriptPath(const std::string& configPath,
                                      const std::string& scriptName = "") const;

    /**
     * @brief Check if adjacent script exists
     */
    bool HasAdjacentScript(const std::string& configPath,
                          const std::string& scriptName = "") const;

    // =========================================================================
    // Central Storage (organized scripts folder)
    // =========================================================================

    /**
     * @brief Store script in central scripts folder
     * @param category Category/subfolder (ai, events, pcg, etc.)
     * @param functionName Script/function name
     * @param code Python source code
     * @return Full path to created script
     */
    std::string StoreCentral(const std::string& category,
                             const std::string& functionName,
                             const std::string& code);

    /**
     * @brief Get central script path
     */
    std::string GetCentralScriptPath(const std::string& category,
                                     const std::string& functionName) const;

    /**
     * @brief List all categories in central storage
     */
    std::vector<std::string> ListCategories() const;

    /**
     * @brief List scripts in a category
     */
    std::vector<std::string> ListCategoryScripts(const std::string& category) const;

    // =========================================================================
    // Generic Script Operations
    // =========================================================================

    /**
     * @brief Get script content from any path
     * @param path Script path (file path or identifier)
     * @return Script content or empty if not found
     */
    std::string GetScript(const std::string& path);

    /**
     * @brief Save script to path
     * @param path Destination path
     * @param code Script content
     * @return true if successful
     */
    bool SaveScript(const std::string& path, const std::string& code);

    /**
     * @brief Delete a script
     */
    bool DeleteScript(const std::string& path);

    /**
     * @brief Check if script exists
     */
    bool ScriptExists(const std::string& path) const;

    /**
     * @brief Get script info
     */
    std::optional<ScriptInfo> GetScriptInfo(const std::string& path) const;

    /**
     * @brief Get all scripts matching criteria
     */
    std::vector<ScriptInfo> GetAllScripts(const ScriptSearchCriteria& criteria = {});

    /**
     * @brief Search scripts by name pattern
     */
    std::vector<ScriptInfo> SearchScripts(const std::string& pattern);

    // =========================================================================
    // Script Metadata
    // =========================================================================

    /**
     * @brief Extract metadata from script header comments
     */
    ScriptInfo ExtractMetadata(const std::string& code) const;

    /**
     * @brief Add/update metadata header in script
     */
    std::string AddMetadataHeader(const std::string& code, const ScriptInfo& info) const;

    /**
     * @brief Set script description
     */
    bool SetScriptDescription(const std::string& path, const std::string& description);

    /**
     * @brief Add tag to script
     */
    bool AddScriptTag(const std::string& path, const std::string& tag);

    // =========================================================================
    // File Watching and Hot-Reload
    // =========================================================================

    /**
     * @brief Enable file watching for hot-reload
     */
    void EnableFileWatching(bool enable);

    /**
     * @brief Check for changed files
     * @return List of changed file paths
     */
    std::vector<std::string> CheckForChanges();

    /**
     * @brief Set callback for script changes
     */
    using ChangeCallback = std::function<void(const std::string& path)>;
    void SetOnScriptChanged(ChangeCallback callback) { m_onScriptChanged = callback; }

    // =========================================================================
    // Import/Export
    // =========================================================================

    /**
     * @brief Import scripts from a zip/folder
     */
    int ImportScripts(const std::string& sourcePath, const std::string& category);

    /**
     * @brief Export scripts to zip/folder
     */
    bool ExportScripts(const std::vector<std::string>& paths, const std::string& destPath);

    /**
     * @brief Export all scripts in category
     */
    bool ExportCategory(const std::string& category, const std::string& destPath);

    // =========================================================================
    // Caching
    // =========================================================================

    /**
     * @brief Clear the script cache
     */
    void ClearCache();

    /**
     * @brief Get cached script (may be stale)
     */
    std::string GetCached(const std::string& path) const;

    /**
     * @brief Preload scripts into cache
     */
    void PreloadScripts(const std::vector<std::string>& paths);

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t cachedScripts = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t totalBytes = 0;
    };
    [[nodiscard]] CacheStats GetCacheStats() const { return m_cacheStats; }

private:
    // Helpers
    std::string ResolvePath(const std::string& path) const;
    std::string NormalizePath(const std::string& path) const;
    bool EnsureDirectoryExists(const std::string& path);
    std::string ReadFile(const std::string& path) const;
    bool WriteFile(const std::string& path, const std::string& content);
    size_t ComputeHash(const std::string& content) const;
    std::chrono::system_clock::time_point GetFileModTime(const std::string& path) const;

    // JSON operations
    bool UpdateJsonScript(const std::string& configPath, const std::string& jsonPath,
                         const std::string& code);
    std::string ReadJsonScript(const std::string& configPath, const std::string& jsonPath);

    // Discovery
    void DiscoverScripts(const std::string& directory);
    void IndexScript(const std::string& path);

    // State
    bool m_initialized = false;
    std::string m_basePath;
    std::string m_scriptsPath;

    // Cache
    struct CacheEntry {
        std::string content;
        size_t hash;
        std::chrono::system_clock::time_point loadTime;
        std::chrono::system_clock::time_point fileModTime;
    };
    mutable std::unordered_map<std::string, CacheEntry> m_cache;
    mutable CacheStats m_cacheStats;

    // Script index
    std::unordered_map<std::string, ScriptInfo> m_scriptIndex;

    // File watching
    bool m_fileWatchingEnabled = false;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> m_watchedFiles;

    // Callbacks
    ChangeCallback m_onScriptChanged;
};

} // namespace Scripting
} // namespace Nova
