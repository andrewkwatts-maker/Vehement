#pragma once

#include "FirebaseClient.hpp"
#include "ReplicationSystem.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>

namespace Nova {

/**
 * @brief Represents a terrain modification for batching
 */
struct TerrainModificationBatch {
    enum class Type {
        Sculpt,
        Paint,
        Tunnel,
        Cave,
        Flatten,
        Smooth
    };

    Type type;
    glm::vec3 position;
    glm::vec3 size;
    glm::vec4 params;  // Type-specific parameters
    uint8_t material;
    glm::vec3 color;
    uint64_t timestamp;
    uint32_t clientId;

    // For merging similar modifications
    bool CanMergeWith(const TerrainModificationBatch& other) const;
    void MergeWith(const TerrainModificationBatch& other);
};

/**
 * @brief Chunk modification tracking
 */
struct ChunkModificationState {
    glm::ivec3 position;
    bool modified = false;
    uint64_t lastModified = 0;
    uint64_t lastSynced = 0;
    int modificationCount = 0;

    // For delta compression - only store changes
    std::vector<std::pair<glm::ivec3, float>> densityChanges;  // Local pos -> new density
    std::vector<std::pair<glm::ivec3, uint8_t>> materialChanges;
};

/**
 * @brief Firebase persistence manager with batching and throttling
 *
 * Features:
 * - Batches terrain changes over configurable time period
 * - Merges similar/overlapping modifications
 * - Delta compression (only stores changes)
 * - Prioritizes recent changes
 * - Configurable sync intervals
 * - Bandwidth-aware throttling
 * - Offline queue with local backup
 */
class FirebasePersistence {
public:
    struct Config {
        // Timing
        float minSyncInterval = 30.0f;      // Minimum seconds between syncs
        float maxSyncInterval = 300.0f;     // Maximum seconds before forced sync
        float idleSyncDelay = 60.0f;        // Sync after this many seconds of no changes

        // Batching
        int maxModificationsPerBatch = 100; // Max modifications before auto-sync
        int maxChunksPerSync = 10;          // Max chunks to sync per push
        bool mergeOverlappingMods = true;   // Merge modifications in same area
        float mergeRadius = 2.0f;           // Radius for merging modifications

        // Bandwidth
        int maxBytesPerMinute = 50000;      // ~50KB/min bandwidth limit
        int maxOperationsPerMinute = 30;    // Max Firebase operations per minute

        // Compression
        bool useDeltaCompression = true;    // Only send changes
        bool compressData = true;           // Compress before sending
        float compressionThreshold = 0.1f;  // Min change to record

        // Reliability
        int maxRetries = 3;
        float retryDelay = 5.0f;
        bool saveLocalBackup = true;
        std::string localBackupPath = "terrain_backup/";
    };

    static FirebasePersistence& Instance();

    void Initialize(std::shared_ptr<FirebaseClient> client, const Config& config = {});
    void Shutdown();

    /**
     * @brief Update persistence (call every frame)
     */
    void Update(float deltaTime);

    // =========================================================================
    // Terrain Persistence
    // =========================================================================

    /**
     * @brief Record a terrain modification (batched, not immediately sent)
     */
    void RecordTerrainModification(const TerrainModificationBatch& mod);

    /**
     * @brief Record chunk as modified
     */
    void MarkChunkModified(const glm::ivec3& chunkPos);

    /**
     * @brief Record individual voxel change
     */
    void RecordVoxelChange(const glm::ivec3& chunkPos, const glm::ivec3& localPos,
                           float newDensity, uint8_t newMaterial);

    /**
     * @brief Force immediate sync (for important changes or shutdown)
     */
    void ForceSync();

    /**
     * @brief Check if sync is pending
     */
    [[nodiscard]] bool HasPendingSync() const;

    /**
     * @brief Get pending modification count
     */
    [[nodiscard]] size_t GetPendingModificationCount() const;

    /**
     * @brief Get pending chunk count
     */
    [[nodiscard]] size_t GetPendingChunkCount() const;

    // =========================================================================
    // World Loading
    // =========================================================================

    /**
     * @brief Set current world ID
     */
    void SetWorldId(const std::string& worldId);

    /**
     * @brief Load world terrain from Firebase
     */
    void LoadWorld(std::function<void(bool)> callback);

    /**
     * @brief Load specific chunks
     */
    void LoadChunks(const std::vector<glm::ivec3>& chunks,
                    std::function<void(const glm::ivec3&, const nlohmann::json&)> onChunkLoaded,
                    std::function<void()> onComplete);

    /**
     * @brief Subscribe to terrain changes (for multiplayer)
     */
    uint64_t SubscribeToChanges(std::function<void(const TerrainModificationBatch&)> callback);
    void UnsubscribeFromChanges(uint64_t subscriptionId);

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        uint64_t totalModificationsRecorded = 0;
        uint64_t totalModificationsSynced = 0;
        uint64_t totalChunksSynced = 0;
        uint64_t totalBytesSent = 0;
        uint64_t totalOperations = 0;
        uint64_t mergedModifications = 0;
        uint64_t failedSyncs = 0;
        float lastSyncTime = 0.0f;
        float avgSyncTime = 0.0f;
    };

    [[nodiscard]] const Stats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats = Stats{}; }

    // =========================================================================
    // Editor Configuration
    // =========================================================================

    /**
     * @brief Get current config (for editor)
     */
    [[nodiscard]] Config& GetConfig() { return m_config; }
    [[nodiscard]] const Config& GetConfig() const { return m_config; }

    /**
     * @brief Apply new config
     */
    void SetConfig(const Config& config) { m_config = config; }

    /**
     * @brief Check if specific event type should be persisted
     */
    [[nodiscard]] bool ShouldPersist(const std::string& eventType) const;

    /**
     * @brief Set persistence override for event type
     */
    void SetPersistenceOverride(const std::string& eventType, bool persist);
    void ClearPersistenceOverride(const std::string& eventType);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnSyncStarted;
    std::function<void(bool success)> OnSyncCompleted;
    std::function<void(const std::string&)> OnSyncError;
    std::function<void(size_t pending)> OnPendingChanged;

private:
    FirebasePersistence() = default;

    // Sync logic
    void CheckAndSync();
    void PerformSync();
    void SyncModificationBatch();
    void SyncChunks();
    void ProcessMerging();

    // Serialization
    nlohmann::json SerializeModification(const TerrainModificationBatch& mod) const;
    TerrainModificationBatch DeserializeModification(const nlohmann::json& json) const;
    nlohmann::json SerializeChunkDelta(const ChunkModificationState& chunk) const;

    // Bandwidth tracking
    bool CanPerformOperation() const;
    void RecordOperation(size_t bytes);

    // Local backup
    void SaveLocalBackup();
    void LoadLocalBackup();

    Config m_config;
    std::shared_ptr<FirebaseClient> m_firebase;
    std::string m_worldId;
    bool m_initialized = false;

    // Modification batching
    std::vector<TerrainModificationBatch> m_pendingModifications;
    std::unordered_map<uint64_t, ChunkModificationState> m_modifiedChunks;
    mutable std::mutex m_modMutex;

    // Timing
    float m_timeSinceLastModification = 0.0f;
    float m_timeSinceLastSync = 0.0f;
    std::chrono::steady_clock::time_point m_lastSyncTime;

    // Bandwidth tracking
    std::vector<std::pair<std::chrono::steady_clock::time_point, size_t>> m_recentOperations;

    // Persistence overrides
    std::unordered_map<std::string, bool> m_persistenceOverrides;

    // Subscriptions for real-time updates
    std::vector<std::pair<uint64_t, std::function<void(const TerrainModificationBatch&)>>> m_changeSubscriptions;
    uint64_t m_nextSubscriptionId = 1;

    // Sync state
    bool m_syncInProgress = false;
    int m_retryCount = 0;

    Stats m_stats;

    // Helper to get chunk key
    static uint64_t GetChunkKey(const glm::ivec3& pos) {
        uint64_t x = static_cast<uint64_t>(pos.x + (1 << 20)) & 0x1FFFFF;
        uint64_t y = static_cast<uint64_t>(pos.y + (1 << 20)) & 0x1FFFFF;
        uint64_t z = static_cast<uint64_t>(pos.z + (1 << 20)) & 0x1FFFFF;
        return x | (y << 21) | (z << 42);
    }
};

/**
 * @brief Integration helper for VoxelTerrain
 * Automatically records terrain changes for Firebase persistence
 */
class TerrainPersistenceIntegration {
public:
    /**
     * @brief Connect to VoxelTerrain callbacks
     */
    static void Connect(class VoxelTerrain* terrain);

    /**
     * @brief Disconnect from VoxelTerrain
     */
    static void Disconnect();

private:
    static void OnTerrainModified(const struct TerrainModification& mod);
};

} // namespace Nova
