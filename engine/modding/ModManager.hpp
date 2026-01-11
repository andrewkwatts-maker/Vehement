#pragma once

#include "JsonSchema.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <optional>

namespace Nova {

/**
 * @brief Mod dependency specification
 */
struct ModDependency {
    std::string modId;
    std::string minVersion;
    std::string maxVersion;
    bool optional = false;
};

/**
 * @brief Mod metadata
 */
struct ModInfo {
    // Identity
    std::string id;                    // Unique mod identifier
    std::string name;                  // Display name
    std::string version;               // Semantic version (e.g., "1.0.0")
    std::string description;           // Short description
    std::string author;                // Author name
    std::string website;               // Optional website URL
    std::string license;               // License (e.g., "MIT", "GPL-3.0")

    // Classification
    std::vector<std::string> tags;     // Tags for filtering
    std::string category;              // Primary category

    // Media
    std::string iconPath;              // Path to icon image
    std::string bannerPath;            // Path to banner image
    std::vector<std::string> screenshots;

    // Technical
    std::vector<ModDependency> dependencies;
    std::string engineMinVersion;      // Minimum engine version
    std::string engineMaxVersion;      // Maximum engine version
    std::vector<std::string> conflicts; // Conflicting mod IDs

    // Status
    bool enabled = true;
    int loadOrder = 0;
    std::chrono::system_clock::time_point installDate;
    std::chrono::system_clock::time_point updateDate;

    // Workshop
    std::string workshopId;            // Steam/Workshop ID
    uint64_t workshopSubscribers = 0;
    float workshopRating = 0.0f;
};

/**
 * @brief Mod load status
 */
enum class ModStatus {
    NotLoaded,
    Loading,
    Loaded,
    Error,
    Disabled,
    IncompatibleVersion,
    MissingDependency,
    Conflict
};

/**
 * @brief Represents a loaded mod
 */
class Mod {
public:
    Mod(const ModInfo& info, const std::string& path);
    ~Mod();

    // Info
    [[nodiscard]] const ModInfo& GetInfo() const { return m_info; }
    [[nodiscard]] const std::string& GetPath() const { return m_path; }
    [[nodiscard]] ModStatus GetStatus() const { return m_status; }
    [[nodiscard]] const std::string& GetErrorMessage() const { return m_errorMessage; }

    // Lifecycle
    bool Load();
    void Unload();
    bool Reload();

    // Assets
    [[nodiscard]] std::string GetAssetPath(const std::string& relativePath) const;
    [[nodiscard]] std::vector<std::string> GetAssets(const std::string& extension = "") const;
    [[nodiscard]] bool HasAsset(const std::string& relativePath) const;

    // Data
    [[nodiscard]] std::string ReadTextFile(const std::string& relativePath) const;
    [[nodiscard]] std::vector<uint8_t> ReadBinaryFile(const std::string& relativePath) const;

    // Scripts
    [[nodiscard]] std::vector<std::string> GetScripts() const;
    bool ExecuteScript(const std::string& scriptPath);

    // Validation
    [[nodiscard]] ValidationResult Validate() const;

private:
    bool LoadManifest();
    bool ValidateDependencies();
    bool LoadAssets();
    bool ExecuteInitScript();

    ModInfo m_info;
    std::string m_path;
    ModStatus m_status = ModStatus::NotLoaded;
    std::string m_errorMessage;

    std::unordered_map<std::string, std::string> m_assetOverrides;
    std::vector<std::string> m_loadedScripts;
};

/**
 * @brief Mod loading events
 */
struct ModLoadEvent {
    std::shared_ptr<Mod> mod;
    ModStatus previousStatus;
    ModStatus newStatus;
    std::string message;
};

/**
 * @brief Mod Manager - handles loading, unloading, and managing mods
 */
class ModManager {
public:
    using ModLoadCallback = std::function<void(const ModLoadEvent&)>;
    using ProgressCallback = std::function<void(float progress, const std::string& message)>;

    static ModManager& Instance();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the mods directory
     */
    void SetModsDirectory(const std::string& path);
    [[nodiscard]] const std::string& GetModsDirectory() const { return m_modsDirectory; }

    /**
     * @brief Set the workshop directory (for Steam Workshop, etc.)
     */
    void SetWorkshopDirectory(const std::string& path);
    [[nodiscard]] const std::string& GetWorkshopDirectory() const { return m_workshopDirectory; }

    /**
     * @brief Set engine version for compatibility checking
     */
    void SetEngineVersion(const std::string& version) { m_engineVersion = version; }
    [[nodiscard]] const std::string& GetEngineVersion() const { return m_engineVersion; }

    // =========================================================================
    // Discovery
    // =========================================================================

    /**
     * @brief Scan directories for available mods
     */
    void ScanForMods();

    /**
     * @brief Get all discovered mods
     */
    [[nodiscard]] std::vector<ModInfo> GetAvailableMods() const;

    /**
     * @brief Get mod info by ID
     */
    [[nodiscard]] std::optional<ModInfo> GetModInfo(const std::string& modId) const;

    /**
     * @brief Check if mod is available
     */
    [[nodiscard]] bool IsModAvailable(const std::string& modId) const;

    // =========================================================================
    // Loading
    // =========================================================================

    /**
     * @brief Load all enabled mods
     */
    bool LoadAllMods(ProgressCallback progress = nullptr);

    /**
     * @brief Load a specific mod
     */
    bool LoadMod(const std::string& modId);

    /**
     * @brief Unload a specific mod
     */
    void UnloadMod(const std::string& modId);

    /**
     * @brief Unload all mods
     */
    void UnloadAllMods();

    /**
     * @brief Reload a specific mod
     */
    bool ReloadMod(const std::string& modId);

    /**
     * @brief Reload all mods
     */
    bool ReloadAllMods();

    /**
     * @brief Get loaded mod by ID
     */
    [[nodiscard]] std::shared_ptr<Mod> GetMod(const std::string& modId) const;

    /**
     * @brief Get all loaded mods
     */
    [[nodiscard]] std::vector<std::shared_ptr<Mod>> GetLoadedMods() const;

    /**
     * @brief Check if mod is loaded
     */
    [[nodiscard]] bool IsModLoaded(const std::string& modId) const;

    // =========================================================================
    // Enable/Disable
    // =========================================================================

    /**
     * @brief Enable a mod
     */
    void EnableMod(const std::string& modId);

    /**
     * @brief Disable a mod
     */
    void DisableMod(const std::string& modId);

    /**
     * @brief Check if mod is enabled
     */
    [[nodiscard]] bool IsModEnabled(const std::string& modId) const;

    /**
     * @brief Get list of enabled mod IDs
     */
    [[nodiscard]] std::vector<std::string> GetEnabledMods() const;

    // =========================================================================
    // Load Order
    // =========================================================================

    /**
     * @brief Set load order for mods
     */
    void SetLoadOrder(const std::vector<std::string>& order);

    /**
     * @brief Get current load order
     */
    [[nodiscard]] std::vector<std::string> GetLoadOrder() const;

    /**
     * @brief Move mod up in load order
     */
    void MoveModUp(const std::string& modId);

    /**
     * @brief Move mod down in load order
     */
    void MoveModDown(const std::string& modId);

    /**
     * @brief Auto-sort load order based on dependencies
     */
    void AutoSortLoadOrder();

    // =========================================================================
    // Dependencies
    // =========================================================================

    /**
     * @brief Check if all dependencies are met for a mod
     */
    [[nodiscard]] bool AreDependenciesMet(const std::string& modId) const;

    /**
     * @brief Get missing dependencies for a mod
     */
    [[nodiscard]] std::vector<ModDependency> GetMissingDependencies(const std::string& modId) const;

    /**
     * @brief Check for conflicts between enabled mods
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> GetConflicts() const;

    // =========================================================================
    // Asset Resolution
    // =========================================================================

    /**
     * @brief Resolve asset path (checks mods first, then base game)
     */
    [[nodiscard]] std::string ResolveAssetPath(const std::string& path) const;

    /**
     * @brief Get all asset overrides
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> GetAssetOverrides() const;

    /**
     * @brief Check if asset is overridden by mod
     */
    [[nodiscard]] bool IsAssetOverridden(const std::string& path) const;

    /**
     * @brief Get mod that overrides an asset
     */
    [[nodiscard]] std::string GetAssetOverridingMod(const std::string& path) const;

    // =========================================================================
    // Creation
    // =========================================================================

    /**
     * @brief Create a new mod template
     */
    bool CreateModTemplate(const std::string& modId, const ModInfo& info);

    /**
     * @brief Export mod to ZIP file
     */
    bool ExportMod(const std::string& modId, const std::string& outputPath);

    /**
     * @brief Import mod from ZIP file
     */
    bool ImportMod(const std::string& zipPath);

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate a mod's structure and content
     */
    [[nodiscard]] ValidationResult ValidateMod(const std::string& modId) const;

    /**
     * @brief Validate all mods
     */
    [[nodiscard]] std::unordered_map<std::string, ValidationResult> ValidateAllMods() const;

    // =========================================================================
    // Configuration Persistence
    // =========================================================================

    /**
     * @brief Save mod configuration (enabled mods, load order)
     */
    bool SaveConfiguration(const std::string& path = "");

    /**
     * @brief Load mod configuration
     */
    bool LoadConfiguration(const std::string& path = "");

    // =========================================================================
    // Callbacks
    // =========================================================================

    void OnModLoaded(ModLoadCallback callback) { m_onModLoaded = callback; }
    void OnModUnloaded(ModLoadCallback callback) { m_onModUnloaded = callback; }
    void OnModError(ModLoadCallback callback) { m_onModError = callback; }

private:
    ModManager() = default;

    ModInfo ParseManifest(const std::string& manifestPath);
    bool CompareVersions(const std::string& version1, const std::string& version2) const;
    std::vector<std::string> TopologicalSort(const std::vector<std::string>& modIds) const;

    std::string m_modsDirectory = "mods";
    std::string m_workshopDirectory = "workshop";
    std::string m_engineVersion = "1.0.0";
    std::string m_configPath = "config/mods.json";

    std::unordered_map<std::string, ModInfo> m_availableMods;
    std::unordered_map<std::string, std::shared_ptr<Mod>> m_loadedMods;
    std::vector<std::string> m_loadOrder;
    std::unordered_map<std::string, bool> m_enabledMods;

    // Asset override map: original path -> mod path
    std::unordered_map<std::string, std::pair<std::string, std::string>> m_assetOverrides;

    ModLoadCallback m_onModLoaded;
    ModLoadCallback m_onModUnloaded;
    ModLoadCallback m_onModError;
};

/**
 * @brief Mod creation wizard helper
 */
class ModCreator {
public:
    ModCreator(const std::string& modId);

    // Basic info
    ModCreator& Name(const std::string& name);
    ModCreator& Description(const std::string& desc);
    ModCreator& Author(const std::string& author);
    ModCreator& Version(const std::string& version);
    ModCreator& Website(const std::string& url);
    ModCreator& License(const std::string& license);

    // Classification
    ModCreator& Tag(const std::string& tag);
    ModCreator& Category(const std::string& category);

    // Dependencies
    ModCreator& Dependency(const std::string& modId, const std::string& minVersion = "");
    ModCreator& OptionalDependency(const std::string& modId);
    ModCreator& Conflicts(const std::string& modId);

    // Engine compatibility
    ModCreator& MinEngineVersion(const std::string& version);
    ModCreator& MaxEngineVersion(const std::string& version);

    // Create the mod
    bool Create(const std::string& outputPath = "");

    // Get info
    [[nodiscard]] const ModInfo& GetInfo() const { return m_info; }

private:
    ModInfo m_info;
    std::vector<std::string> m_directories;  // Directories to create
    std::vector<std::pair<std::string, std::string>> m_templateFiles;  // path, content
};

/**
 * @brief Workshop integration interface
 */
class WorkshopIntegration {
public:
    virtual ~WorkshopIntegration() = default;

    // Search
    virtual std::vector<ModInfo> SearchMods(const std::string& query, int page = 0, int pageSize = 20) = 0;
    virtual std::vector<ModInfo> GetPopularMods(int page = 0, int pageSize = 20) = 0;
    virtual std::vector<ModInfo> GetRecentMods(int page = 0, int pageSize = 20) = 0;

    // Subscription
    virtual bool Subscribe(const std::string& workshopId) = 0;
    virtual bool Unsubscribe(const std::string& workshopId) = 0;
    virtual std::vector<std::string> GetSubscribedMods() = 0;
    virtual bool IsSubscribed(const std::string& workshopId) = 0;

    // Download
    virtual bool DownloadMod(const std::string& workshopId, const std::string& targetPath) = 0;
    virtual float GetDownloadProgress(const std::string& workshopId) = 0;

    // Upload
    virtual bool UploadMod(const std::string& modPath, ModInfo& info) = 0;
    virtual bool UpdateMod(const std::string& workshopId, const std::string& modPath, const std::string& changeNotes) = 0;

    // Rating
    virtual bool RateMod(const std::string& workshopId, int rating) = 0;
    virtual int GetUserRating(const std::string& workshopId) = 0;

    // Authentication
    virtual bool IsAuthenticated() = 0;
    virtual std::string GetUserId() = 0;
    virtual std::string GetUserName() = 0;
};

/**
 * @brief Local filesystem workshop (for offline/testing)
 */
class LocalWorkshop : public WorkshopIntegration {
public:
    LocalWorkshop(const std::string& workshopPath);

    std::vector<ModInfo> SearchMods(const std::string& query, int page, int pageSize) override;
    std::vector<ModInfo> GetPopularMods(int page, int pageSize) override;
    std::vector<ModInfo> GetRecentMods(int page, int pageSize) override;

    bool Subscribe(const std::string& workshopId) override;
    bool Unsubscribe(const std::string& workshopId) override;
    std::vector<std::string> GetSubscribedMods() override;
    bool IsSubscribed(const std::string& workshopId) override;

    bool DownloadMod(const std::string& workshopId, const std::string& targetPath) override;
    float GetDownloadProgress(const std::string& workshopId) override;

    bool UploadMod(const std::string& modPath, ModInfo& info) override;
    bool UpdateMod(const std::string& workshopId, const std::string& modPath, const std::string& changeNotes) override;

    bool RateMod(const std::string& workshopId, int rating) override;
    int GetUserRating(const std::string& workshopId) override;

    bool IsAuthenticated() override { return true; }
    std::string GetUserId() override { return "local_user"; }
    std::string GetUserName() override { return "Local User"; }

private:
    std::string m_workshopPath;
    std::vector<std::string> m_subscribed;
};

} // namespace Nova
