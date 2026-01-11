#pragma once

#include "IPersistenceBackend.hpp"
#include "SQLiteBackend.hpp"
#include "FirebaseBackend.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>

namespace Nova {

/**
 * @brief Manages multiple persistence backends with automatic sync
 *
 * Features:
 * - Dual backend support (SQLite + Firebase)
 * - Write-through cache: saves to SQLite immediately, queues for Firebase
 * - Background sync thread
 * - Conflict detection and resolution
 * - Asset locking for multi-user editing
 * - Automatic retry on failure
 */
class PersistenceManager {
public:
    struct Config {
        // Backend configuration
        nlohmann::json sqliteConfig;
        nlohmann::json firebaseConfig;
        bool enableSQLite = true;
        bool enableFirebase = true;

        // Sync settings
        float syncInterval = 30.0f;         // Seconds between auto-sync
        bool autoSync = true;
        bool syncInBackground = true;

        // Conflict resolution
        enum class ConflictResolution {
            PreferLocal,                    // Use local version
            PreferRemote,                   // Use remote version
            Manual,                         // Require manual resolution
            MostRecent                      // Use most recently modified
        };
        ConflictResolution conflictResolution = ConflictResolution::Manual;

        // Performance
        int maxConcurrentSyncs = 5;
        int syncBatchSize = 10;             // Max assets to sync per batch
    };

    static PersistenceManager& Instance();

    bool Initialize(const Config& config);
    void Shutdown();
    void Update(float deltaTime);

    // =========================================================================
    // Asset Operations (multi-backend)
    // =========================================================================

    /**
     * @brief Save asset to all backends
     */
    bool SaveAsset(const std::string& id, const nlohmann::json& data,
                  const AssetMetadata* metadata = nullptr);

    /**
     * @brief Load asset (prioritizes local cache, then SQLite, then Firebase)
     */
    nlohmann::json LoadAsset(const std::string& id);

    /**
     * @brief Delete asset from all backends
     */
    bool DeleteAsset(const std::string& id);

    /**
     * @brief Check if asset exists in any backend
     */
    bool AssetExists(const std::string& id);

    /**
     * @brief List all assets from primary backend
     */
    std::vector<std::string> ListAssets(const AssetFilter& filter = {});

    /**
     * @brief Get asset metadata
     */
    AssetMetadata GetMetadata(const std::string& id);

    // =========================================================================
    // Versioning
    // =========================================================================

    nlohmann::json GetAssetVersion(const std::string& id, int version);
    std::vector<int> GetAssetVersions(const std::string& id);
    bool RevertToVersion(const std::string& id, int version);

    // =========================================================================
    // Change Tracking & Undo/Redo
    // =========================================================================

    std::vector<ChangeEntry> GetChangeHistory(const std::string& id, size_t limit = 0);

    /**
     * @brief Undo last change to asset
     */
    bool UndoChange(const std::string& id);

    /**
     * @brief Redo previously undone change
     */
    bool RedoChange(const std::string& id);

    /**
     * @brief Check if undo is available
     */
    bool CanUndo(const std::string& id) const;

    /**
     * @brief Check if redo is available
     */
    bool CanRedo(const std::string& id) const;

    // =========================================================================
    // Sync Operations
    // =========================================================================

    /**
     * @brief Force immediate sync with remote backend
     */
    void ForceSync(std::function<void(bool, const std::string&)> callback = nullptr);

    /**
     * @brief Check if online (Firebase available)
     */
    bool IsOnline() const;

    /**
     * @brief Get combined sync status from all backends
     */
    SyncStatus GetSyncStatus() const;

    /**
     * @brief Get pending change count
     */
    size_t GetPendingChangeCount() const;

    // =========================================================================
    // Conflict Resolution
    // =========================================================================

    /**
     * @brief Get list of assets with conflicts
     */
    std::vector<std::string> GetConflictedAssets() const;

    /**
     * @brief Check if specific asset has conflicts
     */
    bool HasConflicts(const std::string& id) const;

    /**
     * @brief Get conflict data for asset
     */
    nlohmann::json GetConflictData(const std::string& id);

    /**
     * @brief Resolve conflict manually
     */
    bool ResolveConflict(const std::string& id, bool useLocal);

    /**
     * @brief Auto-resolve all conflicts using configured strategy
     */
    void AutoResolveConflicts();

    // =========================================================================
    // Multi-User Support
    // =========================================================================

    /**
     * @brief Lock asset for editing
     */
    bool LockAsset(const std::string& id, float durationSeconds = 300.0f);

    /**
     * @brief Unlock asset
     */
    bool UnlockAsset(const std::string& id);

    /**
     * @brief Check if asset is locked
     */
    bool IsAssetLocked(const std::string& id) const;

    /**
     * @brief Get user who locked the asset
     */
    std::string GetAssetLockOwner(const std::string& id) const;

    // =========================================================================
    // Backend Access
    // =========================================================================

    /**
     * @brief Get SQLite backend (for direct access)
     */
    SQLiteBackend* GetSQLiteBackend() { return m_sqliteBackend.get(); }

    /**
     * @brief Get Firebase backend (for direct access)
     */
    FirebaseBackend* GetFirebaseBackend() { return m_firebaseBackend.get(); }

    // =========================================================================
    // Configuration
    // =========================================================================

    const Config& GetConfig() const { return m_config; }
    void SetConfig(const Config& config);

    /**
     * @brief Enable/disable auto-sync
     */
    void SetAutoSync(bool enabled);

    /**
     * @brief Set sync interval
     */
    void SetSyncInterval(float seconds);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string& assetId, const nlohmann::json& data)> OnAssetChanged;
    std::function<void(const std::string& assetId)> OnAssetDeleted;
    std::function<void(const std::string& assetId)> OnConflictDetected;
    std::function<void(bool success, const std::string& error)> OnSyncCompleted;
    std::function<void(size_t pending)> OnPendingChanged;

private:
    PersistenceManager() = default;
    ~PersistenceManager() = default;

    // Prevent copying
    PersistenceManager(const PersistenceManager&) = delete;
    PersistenceManager& operator=(const PersistenceManager&) = delete;

    // Sync logic
    void PerformSync();
    void SyncAsset(const std::string& id);
    void SyncChangesFromSQLiteToFirebase();
    void BackgroundSyncThread();

    // Conflict detection
    void DetectConflicts();
    bool CompareAssetVersions(const std::string& id);

    // Undo/Redo management
    struct UndoState {
        std::vector<ChangeEntry> undoStack;
        std::vector<ChangeEntry> redoStack;
        size_t currentIndex = 0;
    };
    std::unordered_map<std::string, UndoState> m_undoStates;

    Config m_config;
    bool m_initialized = false;

    // Backends
    std::unique_ptr<SQLiteBackend> m_sqliteBackend;
    std::unique_ptr<FirebaseBackend> m_firebaseBackend;

    // Sync state
    float m_syncTimer = 0.0f;
    std::atomic<bool> m_syncInProgress{false};
    mutable std::mutex m_syncMutex;

    // Background sync thread
    std::unique_ptr<std::thread> m_syncThread;
    std::atomic<bool> m_syncThreadRunning{false};

    // Conflict tracking
    std::vector<std::string> m_conflictedAssets;
    mutable std::mutex m_conflictMutex;

    // In-memory cache for performance
    std::unordered_map<std::string, nlohmann::json> m_assetCache;
    mutable std::mutex m_cacheMutex;
};

} // namespace Nova
