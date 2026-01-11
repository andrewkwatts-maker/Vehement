#pragma once

#include "IPersistenceBackend.hpp"
#include "networking/FirebaseClient.hpp"
#include <memory>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Nova {

/**
 * @brief Firebase-based persistence backend
 *
 * Features:
 * - Cloud storage via Firebase Realtime Database
 * - Real-time sync across multiple clients
 * - Automatic conflict detection and resolution
 * - Offline queue with retry logic
 * - Multi-user collaboration support
 * - Authentication integration
 */
class FirebaseBackend : public IPersistenceBackend {
public:
    struct Config {
        std::string projectId;
        std::string apiKey;
        std::string databaseUrl;
        std::string authDomain;

        // Paths
        std::string assetsPath = "assets";      // Root path for assets in Firebase
        std::string changesPath = "changes";    // Path for change tracking
        std::string usersPath = "users";        // Path for user data

        // Sync settings
        float syncInterval = 30.0f;             // Seconds between auto-sync
        int maxRetries = 3;
        float retryDelay = 5.0f;
        bool autoSync = true;

        // Conflict resolution
        enum class ConflictStrategy {
            LastWriteWins,          // Most recent write takes precedence
            FirstWriteWins,         // First write takes precedence
            Manual,                 // Require manual resolution
            MergeJson               // Attempt to merge JSON fields
        };
        ConflictStrategy conflictStrategy = ConflictStrategy::Manual;

        // Authentication
        bool requireAuth = true;
        bool allowAnonymous = true;
    };

    FirebaseBackend();
    explicit FirebaseBackend(std::shared_ptr<FirebaseClient> firebaseClient);
    ~FirebaseBackend() override;

    // IPersistenceBackend implementation
    bool Initialize(const nlohmann::json& config) override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    bool SaveAsset(const std::string& id, const nlohmann::json& data,
                  const AssetMetadata* metadata = nullptr) override;
    nlohmann::json LoadAsset(const std::string& id) override;
    bool DeleteAsset(const std::string& id) override;
    bool AssetExists(const std::string& id) override;
    std::vector<std::string> ListAssets(const AssetFilter& filter = {}) override;
    AssetMetadata GetMetadata(const std::string& id) override;

    nlohmann::json GetAssetVersion(const std::string& id, int version) override;
    std::vector<int> GetAssetVersions(const std::string& id) override;
    bool RevertToVersion(const std::string& id, int version) override;

    std::vector<ChangeEntry> GetChangeHistory(const std::string& id, size_t limit = 0) override;
    std::vector<ChangeEntry> GetUnsyncedChanges() override;
    bool MarkChangesSynced(const std::vector<uint64_t>& changeIds) override;

    bool IsOnline() const override;
    void Sync(std::function<void(bool, const std::string&)> callback = nullptr) override;
    SyncStatus GetSyncStatus() const override;

    bool BeginTransaction() override;
    bool CommitTransaction() override;
    bool RollbackTransaction() override;

    bool HasConflicts(const std::string& id) override;
    nlohmann::json GetConflictData(const std::string& id) override;
    bool ResolveConflict(const std::string& id, bool useLocal) override;

    // Firebase-specific operations

    /**
     * @brief Authenticate with email/password
     */
    void SignIn(const std::string& email, const std::string& password,
                std::function<void(bool, const std::string&)> callback);

    /**
     * @brief Authenticate anonymously
     */
    void SignInAnonymously(std::function<void(bool, const std::string&)> callback);

    /**
     * @brief Sign out current user
     */
    void SignOut();

    /**
     * @brief Get current user ID
     */
    std::string GetCurrentUserId() const;

    /**
     * @brief Subscribe to asset changes (real-time updates)
     */
    uint64_t SubscribeToAssetChanges(const std::string& id,
                                     std::function<void(const nlohmann::json&)> callback);

    /**
     * @brief Unsubscribe from asset changes
     */
    void UnsubscribeFromAssetChanges(uint64_t subscriptionId);

    /**
     * @brief Lock asset for editing (multi-user coordination)
     */
    bool LockAsset(const std::string& id, float lockDurationSeconds = 300.0f);

    /**
     * @brief Unlock asset
     */
    bool UnlockAsset(const std::string& id);

    /**
     * @brief Check if asset is locked by another user
     */
    bool IsAssetLocked(const std::string& id) const;

    /**
     * @brief Get user who locked the asset
     */
    std::string GetAssetLockOwner(const std::string& id) const;

private:
    // Path builders
    std::string GetAssetPath(const std::string& id) const;
    std::string GetAssetMetadataPath(const std::string& id) const;
    std::string GetAssetVersionPath(const std::string& id, int version) const;
    std::string GetChangePath(uint64_t changeId) const;
    std::string GetLockPath(const std::string& id) const;

    // Serialization
    nlohmann::json SerializeMetadata(const AssetMetadata& metadata) const;
    AssetMetadata DeserializeMetadata(const nlohmann::json& json) const;
    nlohmann::json SerializeChange(const ChangeEntry& change) const;
    ChangeEntry DeserializeChange(const nlohmann::json& json) const;

    // Conflict detection
    void DetectConflicts(const std::string& id);
    bool AutoResolveConflict(const std::string& id,
                            const nlohmann::json& localData,
                            const nlohmann::json& remoteData);

    // Queue management
    struct QueuedOperation {
        enum class Type {
            Save,
            Delete,
            MarkSynced
        };

        Type type;
        std::string assetId;
        nlohmann::json data;
        AssetMetadata metadata;
        std::vector<uint64_t> changeIds;
        int retryCount = 0;
    };

    void QueueOperation(const QueuedOperation& op);
    void ProcessQueue();
    void ProcessSaveOperation(QueuedOperation& op);
    void ProcessDeleteOperation(QueuedOperation& op);

    // Change tracking
    uint64_t GenerateChangeId() const;
    void RecordChange(const std::string& assetId, ChangeEntry::Type type,
                     const nlohmann::json& oldData, const nlohmann::json& newData);

    // Real-time listeners
    void SetupAssetListener(const std::string& id);
    void OnAssetDataChanged(const std::string& id, const nlohmann::json& data);

    Config m_config;
    std::shared_ptr<FirebaseClient> m_firebase;
    bool m_initialized = false;
    bool m_ownFirebaseClient = false;

    // Queue for offline operations
    std::queue<QueuedOperation> m_operationQueue;
    mutable std::mutex m_queueMutex;

    // Sync state
    SyncStatus m_syncStatus;
    float m_syncTimer = 0.0f;
    mutable std::mutex m_syncMutex;

    // Conflict tracking
    std::unordered_map<std::string, nlohmann::json> m_conflicts;  // assetId -> conflict data
    mutable std::mutex m_conflictMutex;

    // Asset locks (for multi-user editing)
    struct AssetLock {
        std::string userId;
        uint64_t expiresAt = 0;
    };
    std::unordered_map<std::string, AssetLock> m_assetLocks;
    mutable std::mutex m_lockMutex;

    // Real-time subscriptions
    std::unordered_map<uint64_t, uint64_t> m_subscriptions;  // subscriptionId -> firebaseListenerId
    uint64_t m_nextSubscriptionId = 1;

    // Transaction state
    struct TransactionData {
        std::vector<QueuedOperation> operations;
        bool active = false;
    };
    TransactionData m_transaction;

    // Change tracking
    std::unordered_map<std::string, std::vector<ChangeEntry>> m_changeHistory;
    std::vector<ChangeEntry> m_unsyncedChanges;
    mutable std::mutex m_changeMutex;
    uint64_t m_nextChangeId = 1;
};

} // namespace Nova
