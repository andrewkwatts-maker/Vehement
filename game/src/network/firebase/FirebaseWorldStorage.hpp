#pragma once

#include "FirebaseCore.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <optional>

namespace Network {
namespace Firebase {

// World edit types
enum class WorldEditType {
    Terrain,
    Building,
    Resource,
    Decoration,
    Path,
    Water,
    Vegetation,
    Custom
};

// Edit operation types
enum class EditOperation {
    Create,
    Modify,
    Delete,
    Move,
    Rotate,
    Scale,
    BatchUpdate
};

// Permission levels for world editing
enum class WorldPermission {
    None = 0,
    View = 1,
    Edit = 2,
    Admin = 4,
    Owner = 8
};

// Location/region identifier
struct WorldLocation {
    int64_t regionX = 0;
    int64_t regionY = 0;
    int64_t regionZ = 0;
    std::string worldId;
    std::string layerId;

    std::string getKey() const {
        return worldId + "_" + std::to_string(regionX) + "_" +
               std::to_string(regionY) + "_" + std::to_string(regionZ);
    }

    bool operator==(const WorldLocation& other) const {
        return regionX == other.regionX && regionY == other.regionY &&
               regionZ == other.regionZ && worldId == other.worldId;
    }
};

// Hash for WorldLocation
struct WorldLocationHash {
    size_t operator()(const WorldLocation& loc) const {
        return std::hash<std::string>()(loc.getKey());
    }
};

// Individual world edit
struct WorldEdit {
    std::string editId;
    std::string entityId;
    WorldEditType type = WorldEditType::Custom;
    EditOperation operation = EditOperation::Create;
    WorldLocation location;

    // Transform data
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0, rotW = 1;
    float scaleX = 1, scaleY = 1, scaleZ = 1;

    // Entity-specific data
    std::string entityType;
    std::unordered_map<std::string, std::string> properties;
    std::string serializedData;

    // Metadata
    std::string authorId;
    std::string authorName;
    std::chrono::system_clock::time_point timestamp;
    uint64_t version = 0;

    // For terrain edits
    std::vector<float> heightData;
    std::vector<uint8_t> textureData;
};

// World state snapshot for a region
struct WorldRegionState {
    WorldLocation location;
    std::vector<WorldEdit> edits;
    uint64_t version = 0;
    std::chrono::system_clock::time_point lastModified;
    std::string checksum;

    size_t getEditCount() const { return edits.size(); }
};

// Version info for conflict resolution
struct WorldVersion {
    uint64_t version = 0;
    std::string commitId;
    std::string authorId;
    std::chrono::system_clock::time_point timestamp;
    std::string description;
    std::vector<std::string> editIds;
};

// Conflict when merging edits
struct EditConflict {
    WorldEdit localEdit;
    WorldEdit remoteEdit;
    enum class Resolution {
        UseLocal,
        UseRemote,
        Merge,
        Manual
    } resolution = Resolution::Manual;
};

// World metadata
struct WorldInfo {
    std::string worldId;
    std::string name;
    std::string description;
    std::string ownerId;
    std::string ownerName;

    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastModified;
    uint64_t currentVersion = 0;

    bool isPublic = false;
    bool allowEditing = true;
    int maxEditors = 10;

    std::unordered_map<std::string, WorldPermission> permissions;
    std::unordered_map<std::string, std::string> metadata;

    int64_t sizeBytes = 0;
    int editCount = 0;
    int regionCount = 0;
};

// Edit history entry
struct EditHistoryEntry {
    std::string commitId;
    std::string authorId;
    std::string authorName;
    std::chrono::system_clock::time_point timestamp;
    std::string description;
    int editCount = 0;
    std::vector<std::string> affectedRegions;
};

// Delta/incremental update
struct WorldDelta {
    WorldLocation location;
    uint64_t fromVersion = 0;
    uint64_t toVersion = 0;
    std::vector<WorldEdit> addedEdits;
    std::vector<std::string> removedEditIds;
    std::vector<WorldEdit> modifiedEdits;

    bool isEmpty() const {
        return addedEdits.empty() && removedEditIds.empty() && modifiedEdits.empty();
    }
};

// Callbacks
using WorldLoadCallback = std::function<void(const WorldRegionState&, const FirebaseError&)>;
using WorldSaveCallback = std::function<void(uint64_t newVersion, const FirebaseError&)>;
using WorldInfoCallback = std::function<void(const WorldInfo&, const FirebaseError&)>;
using DeltaCallback = std::function<void(const WorldDelta&, const FirebaseError&)>;
using HistoryCallback = std::function<void(const std::vector<EditHistoryEntry>&, const FirebaseError&)>;
using ConflictCallback = std::function<EditConflict::Resolution(const EditConflict&)>;

/**
 * FirebaseWorldStorage - World edit storage with versioning
 *
 * Features:
 * - Save/load world edits to Firestore
 * - Versioning and conflict resolution
 * - Incremental sync (delta updates)
 * - Edit history with rollback
 * - Shared editing permissions
 */
class FirebaseWorldStorage {
public:
    static FirebaseWorldStorage& getInstance();

    // Initialization
    bool initialize();
    void shutdown();
    void update(float deltaTime);

    // World management
    void createWorld(const std::string& name, const std::string& description,
                     bool isPublic, WorldInfoCallback callback);
    void deleteWorld(const std::string& worldId, std::function<void(const FirebaseError&)> callback);
    void getWorldInfo(const std::string& worldId, WorldInfoCallback callback);
    void listMyWorlds(std::function<void(const std::vector<WorldInfo>&, const FirebaseError&)> callback);
    void listPublicWorlds(int limit, int offset,
                          std::function<void(const std::vector<WorldInfo>&, const FirebaseError&)> callback);

    // Load world state
    void loadRegion(const WorldLocation& location, WorldLoadCallback callback);
    void loadRegions(const std::vector<WorldLocation>& locations,
                     std::function<void(const std::vector<WorldRegionState>&, const FirebaseError&)> callback);
    void loadNearbyRegions(const WorldLocation& center, int radius, WorldLoadCallback callback);

    // Save world edits
    void saveEdit(const WorldEdit& edit, WorldSaveCallback callback);
    void saveEdits(const std::vector<WorldEdit>& edits, WorldSaveCallback callback);
    void deleteEdit(const std::string& editId, WorldSaveCallback callback);

    // Incremental sync
    void getDelta(const WorldLocation& location, uint64_t fromVersion, DeltaCallback callback);
    void subscribeToDelta(const WorldLocation& location,
                          std::function<void(const WorldDelta&)> callback);
    void unsubscribeFromDelta(const WorldLocation& location);

    // Versioning
    uint64_t getCurrentVersion(const WorldLocation& location) const;
    void getVersionHistory(const std::string& worldId, int count, int offset, HistoryCallback callback);
    void getVersion(const std::string& worldId, uint64_t version,
                    std::function<void(const WorldVersion&, const FirebaseError&)> callback);

    // Rollback
    void rollbackToVersion(const WorldLocation& location, uint64_t version, WorldSaveCallback callback);
    void rollbackEdit(const std::string& editId, WorldSaveCallback callback);

    // Conflict resolution
    void setConflictResolver(ConflictCallback resolver) { m_conflictResolver = resolver; }
    void resolveConflict(const EditConflict& conflict, EditConflict::Resolution resolution);

    // Permissions
    void setPermission(const std::string& worldId, const std::string& userId,
                       WorldPermission permission, std::function<void(const FirebaseError&)> callback);
    void removePermission(const std::string& worldId, const std::string& userId,
                          std::function<void(const FirebaseError&)> callback);
    void getPermissions(const std::string& worldId,
                        std::function<void(const std::unordered_map<std::string, WorldPermission>&, const FirebaseError&)> callback);
    WorldPermission getMyPermission(const std::string& worldId) const;

    // Cache management
    void enableLocalCache(bool enabled) { m_cacheEnabled = enabled; }
    void clearLocalCache();
    void preloadRegions(const std::vector<WorldLocation>& locations);
    bool isRegionCached(const WorldLocation& location) const;
    const WorldRegionState* getCachedRegion(const WorldLocation& location) const;

    // Offline support
    void enableOfflineMode(bool enabled) { m_offlineEnabled = enabled; }
    void queueOfflineEdit(const WorldEdit& edit);
    void syncOfflineEdits();
    size_t getPendingEditCount() const;

    // Compression
    void setCompressionEnabled(bool enabled) { m_compressionEnabled = enabled; }
    void setCompressionLevel(int level) { m_compressionLevel = level; }

private:
    FirebaseWorldStorage();
    ~FirebaseWorldStorage();
    FirebaseWorldStorage(const FirebaseWorldStorage&) = delete;
    FirebaseWorldStorage& operator=(const FirebaseWorldStorage&) = delete;

    // Internal operations
    void uploadEdit(const WorldEdit& edit, WorldSaveCallback callback);
    void uploadEdits(const std::vector<WorldEdit>& edits, WorldSaveCallback callback);
    void downloadRegion(const WorldLocation& location, WorldLoadCallback callback);

    // Version management
    uint64_t incrementVersion(const WorldLocation& location);
    void recordVersion(const WorldLocation& location, const std::vector<WorldEdit>& edits);

    // Conflict detection and resolution
    std::vector<EditConflict> detectConflicts(const std::vector<WorldEdit>& localEdits,
                                               const std::vector<WorldEdit>& remoteEdits);
    std::vector<WorldEdit> mergeEdits(const std::vector<EditConflict>& conflicts);

    // Serialization
    std::string serializeEdit(const WorldEdit& edit);
    WorldEdit deserializeEdit(const std::string& json);
    std::string serializeRegion(const WorldRegionState& region);
    WorldRegionState deserializeRegion(const std::string& json);

    // Compression
    std::string compressData(const std::string& data);
    std::string decompressData(const std::string& data);

    // Checksum
    std::string calculateChecksum(const WorldRegionState& region);
    bool verifyChecksum(const WorldRegionState& region);

    // Cache operations
    void cacheRegion(const WorldRegionState& region);
    void invalidateCache(const WorldLocation& location);
    void saveCacheToDisk();
    void loadCacheFromDisk();

private:
    bool m_initialized = false;

    // Cache
    bool m_cacheEnabled = true;
    std::unordered_map<WorldLocation, WorldRegionState, WorldLocationHash> m_regionCache;
    std::unordered_map<std::string, WorldInfo> m_worldInfoCache;
    mutable std::mutex m_cacheMutex;

    // Version tracking
    std::unordered_map<WorldLocation, uint64_t, WorldLocationHash> m_versionMap;
    mutable std::mutex m_versionMutex;

    // Permissions cache
    std::unordered_map<std::string, WorldPermission> m_permissionCache;

    // Offline queue
    bool m_offlineEnabled = true;
    std::vector<WorldEdit> m_offlineQueue;
    mutable std::mutex m_offlineQueueMutex;

    // Delta subscriptions
    std::unordered_map<WorldLocation, std::function<void(const WorldDelta&)>, WorldLocationHash> m_deltaSubscriptions;

    // Conflict resolution
    ConflictCallback m_conflictResolver;
    std::vector<EditConflict> m_pendingConflicts;

    // Compression
    bool m_compressionEnabled = true;
    int m_compressionLevel = 6;  // 1-9, higher = better compression

    // Sync state
    float m_syncTimer = 0.0f;
    static constexpr float SYNC_INTERVAL = 5.0f;

    // Batch upload
    std::vector<WorldEdit> m_pendingUploads;
    float m_uploadBatchTimer = 0.0f;
    static constexpr float UPLOAD_BATCH_INTERVAL = 1.0f;
    static constexpr size_t MAX_BATCH_SIZE = 100;
};

} // namespace Firebase
} // namespace Network
