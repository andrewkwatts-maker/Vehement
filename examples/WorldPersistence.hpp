#pragma once

#include "networking/FirebaseClient.hpp"
#include "networking/FirebasePersistence.hpp"
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>

// Forward declaration for SQLite
struct sqlite3;

namespace Nova {

/**
 * @brief Represents a world edit/modification with geographic coordinates
 */
struct WorldEdit {
    enum class Type {
        TerrainHeight,      // Modify terrain height
        PlacedObject,       // Place an object (building, tree, etc.)
        RemovedObject,      // Remove an object
        PaintTexture,       // Change terrain texture/material
        Sculpt,             // General sculpting operation
        Custom              // Custom user-defined edit
    };

    // Unique identifier
    std::string id;

    // World location (supports both geo-coordinates and XYZ)
    bool useGeoCoordinates = false;

    // Geographic coordinates (lat/long/alt)
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;

    // Cartesian coordinates (XYZ)
    glm::vec3 position = glm::vec3(0.0f);

    // Edit properties
    Type type = Type::TerrainHeight;
    std::string editData;  // JSON data specific to edit type

    // Metadata
    uint64_t timestamp = 0;
    std::string userId;
    std::string worldId;
    int version = 1;

    // Chunk/region tracking
    glm::ivec3 chunkId = glm::ivec3(0);

    // Serialization
    nlohmann::json ToJson() const;
    static WorldEdit FromJson(const nlohmann::json& json);

    // Helper to calculate chunk ID from position
    void UpdateChunkId(float chunkSize = 32.0f);

    // Geographic utilities
    glm::vec3 GetWorldPosition() const { return useGeoCoordinates ? GeoToWorld() : position; }
    glm::vec3 GeoToWorld() const;
    void WorldToGeo(const glm::vec3& worldPos);
};

/**
 * @brief Query parameters for loading world edits
 */
struct WorldEditQuery {
    std::string worldId;
    glm::ivec3 chunkMin = glm::ivec3(INT_MIN);
    glm::ivec3 chunkMax = glm::ivec3(INT_MAX);
    uint64_t sinceTimestamp = 0;
    std::vector<WorldEdit::Type> typeFilter;
    int maxResults = 1000;

    // Geographic bounding box
    double minLat = -90.0, maxLat = 90.0;
    double minLon = -180.0, maxLon = 180.0;
};

/**
 * @brief Conflict resolution strategy for world edits
 */
enum class ConflictResolution {
    KeepLocal,      // Keep local version
    KeepRemote,     // Use remote version
    KeepBoth,       // Keep both as separate edits
    MergeChanges,   // Attempt to merge both changes
    AskUser         // Prompt user to resolve
};

/**
 * @brief Represents a conflict between local and remote edits
 */
struct EditConflict {
    WorldEdit localEdit;
    WorldEdit remoteEdit;
    std::string conflictReason;
    ConflictResolution suggestedResolution = ConflictResolution::AskUser;
};

/**
 * @brief World Persistence Manager
 *
 * Manages persistent storage of world edits using both Firebase (online)
 * and SQLite (offline/local). Handles syncing, conflict resolution,
 * and querying edits by region.
 */
class WorldPersistenceManager {
public:
    enum class StorageMode {
        Online,     // Use Firebase only
        Offline,    // Use SQLite only
        Hybrid      // Use both with automatic syncing
    };

    struct Config {
        StorageMode mode = StorageMode::Hybrid;
        std::string sqlitePath = "world_edits.db";
        std::string worldId = "default_world";
        bool autoSync = true;
        float syncInterval = 30.0f;  // Seconds between syncs
        int maxEditsPerSync = 100;
        ConflictResolution defaultConflictResolution = ConflictResolution::MergeChanges;
        bool enableConflictCallbacks = true;
    };

    WorldPersistenceManager();
    ~WorldPersistenceManager();

    // Initialization
    bool Initialize(const Config& config);
    bool InitializeWithFirebase(std::shared_ptr<FirebaseClient> firebaseClient, const Config& config);
    void Shutdown();

    // Frame update
    void Update(float deltaTime);

    // Storage operations
    bool SaveEdit(const WorldEdit& edit);
    bool SaveEdits(const std::vector<WorldEdit>& edits);
    bool DeleteEdit(const std::string& editId);

    // Loading operations
    std::vector<WorldEdit> LoadEditsInChunk(const glm::ivec3& chunkId);
    std::vector<WorldEdit> LoadEditsInRegion(const glm::ivec3& chunkMin, const glm::ivec3& chunkMax);
    std::vector<WorldEdit> QueryEdits(const WorldEditQuery& query);
    WorldEdit* GetEditById(const std::string& editId);

    // Synchronization
    void SyncToFirebase();              // Upload local edits to Firebase
    void SyncFromFirebase();            // Download latest edits from Firebase
    void SyncBidirectional();           // Full two-way sync
    bool HasPendingUploads() const;
    bool HasPendingDownloads() const;
    int GetPendingUploadCount() const;

    // Procedural generation integration
    using ProceduralGenerator = std::function<void(const glm::ivec3&, std::vector<float>&)>;
    void SetProceduralGenerator(ProceduralGenerator generator);
    void ApplyEditsToProceduralTerrain(const glm::ivec3& chunkId, std::vector<float>& terrainData);

    // Conflict resolution
    void SetConflictResolutionStrategy(ConflictResolution strategy);
    void RegisterConflictCallback(std::function<ConflictResolution(const EditConflict&)> callback);
    std::vector<EditConflict> GetPendingConflicts() const;
    void ResolveConflict(const std::string& editId, ConflictResolution resolution);

    // Mode switching
    void SetStorageMode(StorageMode mode);
    StorageMode GetStorageMode() const { return m_config.mode; }
    bool IsOnlineMode() const { return m_config.mode == StorageMode::Online || m_config.mode == StorageMode::Hybrid; }
    bool IsOfflineMode() const { return m_config.mode == StorageMode::Offline || m_config.mode == StorageMode::Hybrid; }

    // Statistics
    struct Stats {
        uint64_t totalEditsLocal = 0;
        uint64_t totalEditsRemote = 0;
        uint64_t editsUploaded = 0;
        uint64_t editsDownloaded = 0;
        uint64_t conflictsDetected = 0;
        uint64_t conflictsResolved = 0;
        float lastSyncDuration = 0.0f;
        uint64_t lastSyncTime = 0;
    };

    const Stats& GetStats() const { return m_stats; }
    void ResetStats();

    // Callbacks
    std::function<void(bool success, const std::string& message)> OnSyncComplete;
    std::function<void(const EditConflict& conflict)> OnConflictDetected;
    std::function<void(int progress, int total)> OnSyncProgress;

private:
    // SQLite operations
    bool InitializeSQLite();
    void ShutdownSQLite();
    bool CreateTables();
    bool SaveEditToSQLite(const WorldEdit& edit);
    bool DeleteEditFromSQLite(const std::string& editId);
    std::vector<WorldEdit> LoadEditsFromSQLite(const WorldEditQuery& query);
    bool MarkEditAsSynced(const std::string& editId, bool synced);

    // Firebase operations
    bool SaveEditToFirebase(const WorldEdit& edit);
    bool DeleteEditFromFirebase(const std::string& editId);
    void LoadEditsFromFirebase(const WorldEditQuery& query,
                               std::function<void(const std::vector<WorldEdit>&)> callback);

    // Conflict detection and resolution
    bool DetectConflict(const WorldEdit& localEdit, const WorldEdit& remoteEdit);
    EditConflict CreateConflict(const WorldEdit& localEdit, const WorldEdit& remoteEdit);
    void ResolveConflictInternal(const EditConflict& conflict, ConflictResolution resolution);

    // Sync helpers
    void PerformUpload();
    void PerformDownload();
    void ProcessSyncedEdits(const std::vector<WorldEdit>& remoteEdits);

    // Utilities
    std::string GenerateEditId() const;
    uint64_t GetCurrentTimestamp() const;
    std::string ChunkIdToString(const glm::ivec3& chunkId) const;

    Config m_config;
    bool m_initialized = false;

    // SQLite database
    sqlite3* m_db = nullptr;
    mutable std::mutex m_dbMutex;

    // Firebase client
    std::shared_ptr<FirebaseClient> m_firebase;

    // Local cache
    std::unordered_map<std::string, WorldEdit> m_editCache;
    std::vector<std::string> m_pendingUploads;
    std::vector<EditConflict> m_pendingConflicts;
    mutable std::mutex m_cacheMutex;

    // Procedural generation
    ProceduralGenerator m_proceduralGenerator;

    // Conflict resolution
    std::function<ConflictResolution(const EditConflict&)> m_conflictCallback;

    // Sync timing
    float m_timeSinceLastSync = 0.0f;

    // Statistics
    Stats m_stats;
};

/**
 * @brief UI Panel for World Persistence Management
 * Integrates with StandaloneEditor to provide persistence controls
 */
class WorldPersistenceUI {
public:
    WorldPersistenceUI();

    void Initialize(WorldPersistenceManager* manager);
    void Render();

    // Configuration
    void ShowConflictResolutionDialog(const EditConflict& conflict);

private:
    void RenderModeSelector();
    void RenderSyncControls();
    void RenderStatistics();
    void RenderConflictList();
    void RenderSettingsPanel();

    WorldPersistenceManager* m_manager = nullptr;

    // UI state
    bool m_showSettings = false;
    bool m_showConflicts = false;
    bool m_syncInProgress = false;
    int m_selectedConflictIndex = -1;

    // Temporary config for editing
    WorldPersistenceManager::Config m_tempConfig;
};

} // namespace Nova
