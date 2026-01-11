#pragma once

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

namespace Nova {

/**
 * @brief Asset metadata structure
 */
struct AssetMetadata {
    std::string id;
    std::string type;
    int version = 1;
    uint64_t createdAt = 0;
    uint64_t modifiedAt = 0;
    std::string checksum;
    std::string userId;  // For multi-user tracking
    nlohmann::json customData;  // Additional metadata
};

/**
 * @brief Change tracking entry
 */
struct ChangeEntry {
    enum class Type {
        Create,
        Update,
        Delete
    };

    uint64_t id = 0;
    std::string assetId;
    Type changeType = Type::Update;
    nlohmann::json oldData;
    nlohmann::json newData;
    uint64_t timestamp = 0;
    bool synced = false;
    std::string userId;
};

/**
 * @brief Sync status information
 */
struct SyncStatus {
    bool online = false;
    size_t pendingChanges = 0;
    size_t syncedChanges = 0;
    uint64_t lastSyncTime = 0;
    std::string lastError;
};

/**
 * @brief Asset query filter
 */
struct AssetFilter {
    std::string type;           // Filter by type
    std::string namePattern;    // Regex pattern for name
    uint64_t modifiedAfter = 0; // Filter by modification time
    uint64_t modifiedBefore = 0;
    int minVersion = 0;         // Filter by version
    int maxVersion = 0;
    std::string userId;         // Filter by user
};

/**
 * @brief Generic persistence backend interface
 *
 * Provides abstraction for different storage backends (SQLite, Firebase, etc.)
 * Supports asset versioning, change tracking, and sync operations.
 */
class IPersistenceBackend {
public:
    virtual ~IPersistenceBackend() = default;

    /**
     * @brief Initialize the backend
     * @param config Backend-specific configuration
     * @return True if initialization succeeded
     */
    virtual bool Initialize(const nlohmann::json& config) = 0;

    /**
     * @brief Shutdown the backend
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Update the backend (process queues, handle async operations)
     * @param deltaTime Time since last update
     */
    virtual void Update(float deltaTime) = 0;

    // =========================================================================
    // Asset Operations
    // =========================================================================

    /**
     * @brief Save an asset
     * @param id Unique asset identifier
     * @param data Asset data as JSON
     * @param metadata Optional metadata (auto-generated if null)
     * @return True if save succeeded
     */
    virtual bool SaveAsset(const std::string& id, const nlohmann::json& data,
                          const AssetMetadata* metadata = nullptr) = 0;

    /**
     * @brief Load an asset
     * @param id Unique asset identifier
     * @return Asset data, or null JSON if not found
     */
    virtual nlohmann::json LoadAsset(const std::string& id) = 0;

    /**
     * @brief Delete an asset
     * @param id Unique asset identifier
     * @return True if delete succeeded
     */
    virtual bool DeleteAsset(const std::string& id) = 0;

    /**
     * @brief Check if asset exists
     * @param id Unique asset identifier
     * @return True if asset exists
     */
    virtual bool AssetExists(const std::string& id) = 0;

    /**
     * @brief List all assets matching filter
     * @param filter Asset filter criteria
     * @return List of asset IDs
     */
    virtual std::vector<std::string> ListAssets(const AssetFilter& filter = {}) = 0;

    /**
     * @brief Get asset metadata
     * @param id Unique asset identifier
     * @return Asset metadata, or empty metadata if not found
     */
    virtual AssetMetadata GetMetadata(const std::string& id) = 0;

    // =========================================================================
    // Versioning
    // =========================================================================

    /**
     * @brief Get specific version of asset
     * @param id Unique asset identifier
     * @param version Version number (0 = latest)
     * @return Asset data at specified version
     */
    virtual nlohmann::json GetAssetVersion(const std::string& id, int version) = 0;

    /**
     * @brief List all versions of an asset
     * @param id Unique asset identifier
     * @return List of version numbers
     */
    virtual std::vector<int> GetAssetVersions(const std::string& id) = 0;

    /**
     * @brief Revert asset to specific version
     * @param id Unique asset identifier
     * @param version Version number to revert to
     * @return True if revert succeeded
     */
    virtual bool RevertToVersion(const std::string& id, int version) = 0;

    // =========================================================================
    // Change Tracking
    // =========================================================================

    /**
     * @brief Get change history for asset
     * @param id Unique asset identifier
     * @param limit Maximum number of changes (0 = all)
     * @return List of change entries
     */
    virtual std::vector<ChangeEntry> GetChangeHistory(const std::string& id, size_t limit = 0) = 0;

    /**
     * @brief Get all unsynced changes
     * @return List of change entries that haven't been synced
     */
    virtual std::vector<ChangeEntry> GetUnsyncedChanges() = 0;

    /**
     * @brief Mark changes as synced
     * @param changeIds List of change IDs to mark as synced
     * @return True if operation succeeded
     */
    virtual bool MarkChangesSynced(const std::vector<uint64_t>& changeIds) = 0;

    // =========================================================================
    // Sync Operations
    // =========================================================================

    /**
     * @brief Check if backend is online (can perform remote operations)
     * @return True if online
     */
    virtual bool IsOnline() const = 0;

    /**
     * @brief Synchronize with remote backend
     * @param callback Called when sync completes (success, error message)
     */
    virtual void Sync(std::function<void(bool, const std::string&)> callback = nullptr) = 0;

    /**
     * @brief Get current sync status
     * @return Sync status information
     */
    virtual SyncStatus GetSyncStatus() const = 0;

    // =========================================================================
    // Transactions
    // =========================================================================

    /**
     * @brief Begin a transaction
     * @return True if transaction started successfully
     */
    virtual bool BeginTransaction() = 0;

    /**
     * @brief Commit current transaction
     * @return True if commit succeeded
     */
    virtual bool CommitTransaction() = 0;

    /**
     * @brief Rollback current transaction
     * @return True if rollback succeeded
     */
    virtual bool RollbackTransaction() = 0;

    // =========================================================================
    // Conflict Resolution
    // =========================================================================

    /**
     * @brief Detect conflicts with remote version
     * @param id Asset identifier
     * @return True if conflicts detected
     */
    virtual bool HasConflicts(const std::string& id) = 0;

    /**
     * @brief Get conflicting versions
     * @param id Asset identifier
     * @return JSON object with local and remote versions
     */
    virtual nlohmann::json GetConflictData(const std::string& id) = 0;

    /**
     * @brief Resolve conflict by choosing version
     * @param id Asset identifier
     * @param useLocal True to keep local version, false for remote
     * @return True if resolution succeeded
     */
    virtual bool ResolveConflict(const std::string& id, bool useLocal) = 0;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string& assetId, const nlohmann::json& data)> OnAssetChanged;
    std::function<void(const std::string& assetId)> OnAssetDeleted;
    std::function<void(const std::string& assetId)> OnConflictDetected;
    std::function<void(bool success, const std::string& error)> OnSyncCompleted;
};

} // namespace Nova
