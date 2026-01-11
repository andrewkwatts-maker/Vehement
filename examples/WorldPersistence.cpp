#include "WorldPersistence.hpp"
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Nova {

// =============================================================================
// WorldEdit Implementation
// =============================================================================

nlohmann::json WorldEdit::ToJson() const {
    nlohmann::json json;
    json["id"] = id;
    json["useGeoCoordinates"] = useGeoCoordinates;

    if (useGeoCoordinates) {
        json["latitude"] = latitude;
        json["longitude"] = longitude;
        json["altitude"] = altitude;
    } else {
        json["position"] = {position.x, position.y, position.z};
    }

    json["type"] = static_cast<int>(type);
    json["editData"] = editData;
    json["timestamp"] = timestamp;
    json["userId"] = userId;
    json["worldId"] = worldId;
    json["version"] = version;
    json["chunkId"] = {chunkId.x, chunkId.y, chunkId.z};

    return json;
}

WorldEdit WorldEdit::FromJson(const nlohmann::json& json) {
    WorldEdit edit;

    edit.id = json.value("id", "");
    edit.useGeoCoordinates = json.value("useGeoCoordinates", false);

    if (edit.useGeoCoordinates) {
        edit.latitude = json.value("latitude", 0.0);
        edit.longitude = json.value("longitude", 0.0);
        edit.altitude = json.value("altitude", 0.0);
    } else {
        if (json.contains("position") && json["position"].is_array()) {
            edit.position = glm::vec3(
                json["position"][0].get<float>(),
                json["position"][1].get<float>(),
                json["position"][2].get<float>()
            );
        }
    }

    edit.type = static_cast<WorldEdit::Type>(json.value("type", 0));
    edit.editData = json.value("editData", "");
    edit.timestamp = json.value("timestamp", 0ULL);
    edit.userId = json.value("userId", "");
    edit.worldId = json.value("worldId", "");
    edit.version = json.value("version", 1);

    if (json.contains("chunkId") && json["chunkId"].is_array()) {
        edit.chunkId = glm::ivec3(
            json["chunkId"][0].get<int>(),
            json["chunkId"][1].get<int>(),
            json["chunkId"][2].get<int>()
        );
    }

    return edit;
}

void WorldEdit::UpdateChunkId(float chunkSize) {
    glm::vec3 worldPos = GetWorldPosition();
    chunkId.x = static_cast<int>(std::floor(worldPos.x / chunkSize));
    chunkId.y = static_cast<int>(std::floor(worldPos.y / chunkSize));
    chunkId.z = static_cast<int>(std::floor(worldPos.z / chunkSize));
}

glm::vec3 WorldEdit::GeoToWorld() const {
    // Simple equirectangular projection
    // For a real implementation, use proper coordinate transformation
    const float earthRadius = 6371000.0f; // meters
    float x = static_cast<float>(longitude * earthRadius * cos(latitude * M_PI / 180.0));
    float y = static_cast<float>(altitude);
    float z = static_cast<float>(latitude * earthRadius);
    return glm::vec3(x, y, z);
}

void WorldEdit::WorldToGeo(const glm::vec3& worldPos) {
    const float earthRadius = 6371000.0f;
    latitude = static_cast<double>(worldPos.z / earthRadius);
    longitude = static_cast<double>(worldPos.x / (earthRadius * cos(latitude * M_PI / 180.0)));
    altitude = static_cast<double>(worldPos.y);
}

// =============================================================================
// WorldPersistenceManager Implementation
// =============================================================================

WorldPersistenceManager::WorldPersistenceManager() = default;

WorldPersistenceManager::~WorldPersistenceManager() {
    Shutdown();
}

bool WorldPersistenceManager::Initialize(const Config& config) {
    if (m_initialized) {
        spdlog::warn("WorldPersistenceManager already initialized");
        return true;
    }

    m_config = config;

    // Initialize SQLite if offline or hybrid mode
    if (IsOfflineMode()) {
        if (!InitializeSQLite()) {
            spdlog::error("Failed to initialize SQLite database");
            return false;
        }
    }

    m_initialized = true;
    spdlog::info("WorldPersistenceManager initialized (mode: {})",
                 config.mode == StorageMode::Online ? "Online" :
                 config.mode == StorageMode::Offline ? "Offline" : "Hybrid");

    return true;
}

bool WorldPersistenceManager::InitializeWithFirebase(std::shared_ptr<FirebaseClient> firebaseClient,
                                                       const Config& config) {
    m_firebase = firebaseClient;
    return Initialize(config);
}

void WorldPersistenceManager::Shutdown() {
    if (!m_initialized) return;

    // Sync any pending uploads before shutdown
    if (m_config.mode != StorageMode::Offline && HasPendingUploads()) {
        spdlog::info("Syncing pending uploads before shutdown...");
        SyncToFirebase();
    }

    ShutdownSQLite();

    m_initialized = false;
    spdlog::info("WorldPersistenceManager shutdown complete");
}

void WorldPersistenceManager::Update(float deltaTime) {
    if (!m_initialized) return;

    m_timeSinceLastSync += deltaTime;

    // Auto-sync if enabled and interval reached
    if (m_config.autoSync && m_timeSinceLastSync >= m_config.syncInterval) {
        if (m_config.mode == StorageMode::Hybrid) {
            SyncBidirectional();
        } else if (m_config.mode == StorageMode::Online && HasPendingUploads()) {
            SyncToFirebase();
        }
        m_timeSinceLastSync = 0.0f;
    }
}

// =============================================================================
// Storage Operations
// =============================================================================

bool WorldPersistenceManager::SaveEdit(const WorldEdit& edit) {
    if (!m_initialized) {
        spdlog::error("WorldPersistenceManager not initialized");
        return false;
    }

    // Generate ID if not set
    WorldEdit mutableEdit = edit;
    if (mutableEdit.id.empty()) {
        mutableEdit.id = GenerateEditId();
    }

    // Set timestamp if not set
    if (mutableEdit.timestamp == 0) {
        mutableEdit.timestamp = GetCurrentTimestamp();
    }

    // Update chunk ID
    mutableEdit.UpdateChunkId();

    // Cache edit
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_editCache[mutableEdit.id] = mutableEdit;
        m_stats.totalEditsLocal++;
    }

    bool success = true;

    // Save to SQLite if offline mode enabled
    if (IsOfflineMode()) {
        success &= SaveEditToSQLite(mutableEdit);
    }

    // Save to Firebase if online mode enabled
    if (IsOnlineMode() && m_firebase) {
        if (m_config.mode == StorageMode::Hybrid) {
            // In hybrid mode, queue for later sync
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_pendingUploads.push_back(mutableEdit.id);
        } else {
            // In online-only mode, upload immediately
            success &= SaveEditToFirebase(mutableEdit);
        }
    }

    return success;
}

bool WorldPersistenceManager::SaveEdits(const std::vector<WorldEdit>& edits) {
    bool allSuccess = true;
    for (const auto& edit : edits) {
        allSuccess &= SaveEdit(edit);
    }
    return allSuccess;
}

bool WorldPersistenceManager::DeleteEdit(const std::string& editId) {
    if (!m_initialized) return false;

    bool success = true;

    // Remove from cache
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_editCache.erase(editId);

        // Remove from pending uploads
        auto it = std::find(m_pendingUploads.begin(), m_pendingUploads.end(), editId);
        if (it != m_pendingUploads.end()) {
            m_pendingUploads.erase(it);
        }
    }

    // Delete from SQLite
    if (IsOfflineMode()) {
        success &= DeleteEditFromSQLite(editId);
    }

    // Delete from Firebase
    if (IsOnlineMode() && m_firebase) {
        success &= DeleteEditFromFirebase(editId);
    }

    return success;
}

// =============================================================================
// Loading Operations
// =============================================================================

std::vector<WorldEdit> WorldPersistenceManager::LoadEditsInChunk(const glm::ivec3& chunkId) {
    WorldEditQuery query;
    query.worldId = m_config.worldId;
    query.chunkMin = chunkId;
    query.chunkMax = chunkId;
    return QueryEdits(query);
}

std::vector<WorldEdit> WorldPersistenceManager::LoadEditsInRegion(const glm::ivec3& chunkMin,
                                                                    const glm::ivec3& chunkMax) {
    WorldEditQuery query;
    query.worldId = m_config.worldId;
    query.chunkMin = chunkMin;
    query.chunkMax = chunkMax;
    return QueryEdits(query);
}

std::vector<WorldEdit> WorldPersistenceManager::QueryEdits(const WorldEditQuery& query) {
    if (!m_initialized) return {};

    // For offline or hybrid mode, query SQLite
    if (IsOfflineMode()) {
        return LoadEditsFromSQLite(query);
    }

    // For online-only mode, query cache (Firebase queries would be async)
    std::vector<WorldEdit> results;
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        for (const auto& [id, edit] : m_editCache) {
            if (edit.worldId != query.worldId) continue;
            if (edit.chunkId.x < query.chunkMin.x || edit.chunkId.x > query.chunkMax.x) continue;
            if (edit.chunkId.y < query.chunkMin.y || edit.chunkId.y > query.chunkMax.y) continue;
            if (edit.chunkId.z < query.chunkMin.z || edit.chunkId.z > query.chunkMax.z) continue;
            if (edit.timestamp < query.sinceTimestamp) continue;

            if (!query.typeFilter.empty()) {
                bool matchesType = false;
                for (auto type : query.typeFilter) {
                    if (edit.type == type) {
                        matchesType = true;
                        break;
                    }
                }
                if (!matchesType) continue;
            }

            results.push_back(edit);
            if (static_cast<int>(results.size()) >= query.maxResults) break;
        }
    }

    return results;
}

WorldEdit* WorldPersistenceManager::GetEditById(const std::string& editId) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_editCache.find(editId);
    return (it != m_editCache.end()) ? &it->second : nullptr;
}

// =============================================================================
// Synchronization
// =============================================================================

void WorldPersistenceManager::SyncToFirebase() {
    if (!IsOnlineMode() || !m_firebase) {
        spdlog::warn("Cannot sync to Firebase: not in online mode or Firebase not initialized");
        return;
    }

    spdlog::info("Starting sync to Firebase...");
    auto startTime = std::chrono::steady_clock::now();

    PerformUpload();

    auto endTime = std::chrono::steady_clock::now();
    m_stats.lastSyncDuration = std::chrono::duration<float>(endTime - startTime).count();
    m_stats.lastSyncTime = GetCurrentTimestamp();

    if (OnSyncComplete) {
        OnSyncComplete(true, "Upload completed successfully");
    }
}

void WorldPersistenceManager::SyncFromFirebase() {
    if (!IsOnlineMode() || !m_firebase) {
        spdlog::warn("Cannot sync from Firebase: not in online mode or Firebase not initialized");
        return;
    }

    spdlog::info("Starting sync from Firebase...");
    auto startTime = std::chrono::steady_clock::now();

    PerformDownload();

    auto endTime = std::chrono::steady_clock::now();
    m_stats.lastSyncDuration = std::chrono::duration<float>(endTime - startTime).count();
    m_stats.lastSyncTime = GetCurrentTimestamp();

    if (OnSyncComplete) {
        OnSyncComplete(true, "Download completed successfully");
    }
}

void WorldPersistenceManager::SyncBidirectional() {
    if (m_config.mode != StorageMode::Hybrid) {
        spdlog::warn("Bidirectional sync only available in Hybrid mode");
        return;
    }

    spdlog::info("Starting bidirectional sync...");
    auto startTime = std::chrono::steady_clock::now();

    // First upload local changes
    PerformUpload();

    // Then download remote changes
    PerformDownload();

    auto endTime = std::chrono::steady_clock::now();
    m_stats.lastSyncDuration = std::chrono::duration<float>(endTime - startTime).count();
    m_stats.lastSyncTime = GetCurrentTimestamp();

    if (OnSyncComplete) {
        OnSyncComplete(true, "Bidirectional sync completed successfully");
    }
}

bool WorldPersistenceManager::HasPendingUploads() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return !m_pendingUploads.empty();
}

bool WorldPersistenceManager::HasPendingDownloads() const {
    // This would check if Firebase has newer edits
    // For now, return false
    return false;
}

int WorldPersistenceManager::GetPendingUploadCount() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return static_cast<int>(m_pendingUploads.size());
}

// =============================================================================
// Procedural Generation Integration
// =============================================================================

void WorldPersistenceManager::SetProceduralGenerator(ProceduralGenerator generator) {
    m_proceduralGenerator = std::move(generator);
}

void WorldPersistenceManager::ApplyEditsToProceduralTerrain(const glm::ivec3& chunkId,
                                                              std::vector<float>& terrainData) {
    // First generate procedural terrain if generator is set
    if (m_proceduralGenerator) {
        m_proceduralGenerator(chunkId, terrainData);
    }

    // Load edits for this chunk
    auto edits = LoadEditsInChunk(chunkId);

    // Apply edits to terrain data
    for (const auto& edit : edits) {
        if (edit.type == WorldEdit::Type::TerrainHeight) {
            // Parse edit data and apply height modifications
            try {
                auto data = nlohmann::json::parse(edit.editData);
                // Apply the height modification based on edit data
                // This is a simplified example
                if (data.contains("height") && data.contains("radius")) {
                    float height = data["height"].get<float>();
                    float radius = data["radius"].get<float>();
                    // Apply to terrain data...
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to parse edit data: {}", e.what());
            }
        }
    }
}

// =============================================================================
// Conflict Resolution
// =============================================================================

void WorldPersistenceManager::SetConflictResolutionStrategy(ConflictResolution strategy) {
    m_config.defaultConflictResolution = strategy;
}

void WorldPersistenceManager::RegisterConflictCallback(
    std::function<ConflictResolution(const EditConflict&)> callback) {
    m_conflictCallback = std::move(callback);
}

std::vector<EditConflict> WorldPersistenceManager::GetPendingConflicts() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_pendingConflicts;
}

void WorldPersistenceManager::ResolveConflict(const std::string& editId, ConflictResolution resolution) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = std::find_if(m_pendingConflicts.begin(), m_pendingConflicts.end(),
                           [&editId](const EditConflict& c) { return c.localEdit.id == editId; });

    if (it != m_pendingConflicts.end()) {
        ResolveConflictInternal(*it, resolution);
        m_pendingConflicts.erase(it);
        m_stats.conflictsResolved++;
    }
}

// =============================================================================
// Mode Switching
// =============================================================================

void WorldPersistenceManager::SetStorageMode(StorageMode mode) {
    if (m_config.mode == mode) return;

    spdlog::info("Switching storage mode from {} to {}",
                 static_cast<int>(m_config.mode), static_cast<int>(mode));

    // Sync before mode switch if needed
    if (m_config.mode == StorageMode::Hybrid && HasPendingUploads()) {
        SyncToFirebase();
    }

    m_config.mode = mode;

    // Initialize/shutdown components based on new mode
    if (mode == StorageMode::Offline || mode == StorageMode::Hybrid) {
        if (!m_db) {
            InitializeSQLite();
        }
    }
}

void WorldPersistenceManager::ResetStats() {
    m_stats = Stats{};
}

// =============================================================================
// SQLite Operations
// =============================================================================

bool WorldPersistenceManager::InitializeSQLite() {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    int rc = sqlite3_open(m_config.sqlitePath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to open SQLite database: {}", sqlite3_errmsg(m_db));
        return false;
    }

    spdlog::info("SQLite database opened: {}", m_config.sqlitePath);

    return CreateTables();
}

void WorldPersistenceManager::ShutdownSQLite() {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
        spdlog::info("SQLite database closed");
    }
}

bool WorldPersistenceManager::CreateTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS world_edits (
            id TEXT PRIMARY KEY,
            world_id TEXT NOT NULL,
            chunk_x INTEGER NOT NULL,
            chunk_y INTEGER NOT NULL,
            chunk_z INTEGER NOT NULL,
            use_geo INTEGER NOT NULL,
            latitude REAL,
            longitude REAL,
            altitude REAL,
            pos_x REAL,
            pos_y REAL,
            pos_z REAL,
            edit_type INTEGER NOT NULL,
            edit_data TEXT,
            timestamp INTEGER NOT NULL,
            user_id TEXT,
            version INTEGER NOT NULL,
            synced INTEGER NOT NULL DEFAULT 0
        );

        CREATE INDEX IF NOT EXISTS idx_world_chunk ON world_edits(world_id, chunk_x, chunk_y, chunk_z);
        CREATE INDEX IF NOT EXISTS idx_timestamp ON world_edits(timestamp);
        CREATE INDEX IF NOT EXISTS idx_synced ON world_edits(synced);
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        spdlog::error("Failed to create tables: {}", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    spdlog::info("SQLite tables created successfully");
    return true;
}

bool WorldPersistenceManager::SaveEditToSQLite(const WorldEdit& edit) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = R"(
        INSERT OR REPLACE INTO world_edits
        (id, world_id, chunk_x, chunk_y, chunk_z, use_geo, latitude, longitude, altitude,
         pos_x, pos_y, pos_z, edit_type, edit_data, timestamp, user_id, version, synced)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, edit.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, edit.worldId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, edit.chunkId.x);
    sqlite3_bind_int(stmt, 4, edit.chunkId.y);
    sqlite3_bind_int(stmt, 5, edit.chunkId.z);
    sqlite3_bind_int(stmt, 6, edit.useGeoCoordinates ? 1 : 0);
    sqlite3_bind_double(stmt, 7, edit.latitude);
    sqlite3_bind_double(stmt, 8, edit.longitude);
    sqlite3_bind_double(stmt, 9, edit.altitude);
    sqlite3_bind_double(stmt, 10, edit.position.x);
    sqlite3_bind_double(stmt, 11, edit.position.y);
    sqlite3_bind_double(stmt, 12, edit.position.z);
    sqlite3_bind_int(stmt, 13, static_cast<int>(edit.type));
    sqlite3_bind_text(stmt, 14, edit.editData.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 15, edit.timestamp);
    sqlite3_bind_text(stmt, 16, edit.userId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 17, edit.version);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to insert edit: {}", sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool WorldPersistenceManager::DeleteEditFromSQLite(const std::string& editId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = "DELETE FROM world_edits WHERE id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare delete statement: {}", sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, editId.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

std::vector<WorldEdit> WorldPersistenceManager::LoadEditsFromSQLite(const WorldEditQuery& query) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<WorldEdit> results;

    if (!m_db) return results;

    std::stringstream sql;
    sql << "SELECT * FROM world_edits WHERE world_id = ?";
    sql << " AND chunk_x >= ? AND chunk_x <= ?";
    sql << " AND chunk_y >= ? AND chunk_y <= ?";
    sql << " AND chunk_z >= ? AND chunk_z <= ?";
    sql << " AND timestamp >= ?";
    sql << " LIMIT ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare query: {}", sqlite3_errmsg(m_db));
        return results;
    }

    sqlite3_bind_text(stmt, 1, query.worldId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, query.chunkMin.x);
    sqlite3_bind_int(stmt, 3, query.chunkMax.x);
    sqlite3_bind_int(stmt, 4, query.chunkMin.y);
    sqlite3_bind_int(stmt, 5, query.chunkMax.y);
    sqlite3_bind_int(stmt, 6, query.chunkMin.z);
    sqlite3_bind_int(stmt, 7, query.chunkMax.z);
    sqlite3_bind_int64(stmt, 8, query.sinceTimestamp);
    sqlite3_bind_int(stmt, 9, query.maxResults);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        WorldEdit edit;
        edit.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        edit.worldId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        edit.chunkId = glm::ivec3(
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4)
        );
        edit.useGeoCoordinates = sqlite3_column_int(stmt, 5) != 0;
        edit.latitude = sqlite3_column_double(stmt, 6);
        edit.longitude = sqlite3_column_double(stmt, 7);
        edit.altitude = sqlite3_column_double(stmt, 8);
        edit.position = glm::vec3(
            static_cast<float>(sqlite3_column_double(stmt, 9)),
            static_cast<float>(sqlite3_column_double(stmt, 10)),
            static_cast<float>(sqlite3_column_double(stmt, 11))
        );
        edit.type = static_cast<WorldEdit::Type>(sqlite3_column_int(stmt, 12));
        edit.editData = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        edit.timestamp = sqlite3_column_int64(stmt, 14);
        edit.userId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        edit.version = sqlite3_column_int(stmt, 16);

        results.push_back(edit);
    }

    sqlite3_finalize(stmt);

    return results;
}

bool WorldPersistenceManager::MarkEditAsSynced(const std::string& editId, bool synced) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = "UPDATE world_edits SET synced = ? WHERE id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, synced ? 1 : 0);
    sqlite3_bind_text(stmt, 2, editId.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

// =============================================================================
// Firebase Operations
// =============================================================================

bool WorldPersistenceManager::SaveEditToFirebase(const WorldEdit& edit) {
    if (!m_firebase) return false;

    std::string path = "worlds/" + edit.worldId + "/edits/" +
                       ChunkIdToString(edit.chunkId) + "/" + edit.id;

    nlohmann::json data = edit.ToJson();

    m_firebase->Set(path, data, [this, editId = edit.id](const FirebaseResult& result) {
        if (result.success) {
            MarkEditAsSynced(editId, true);
            m_stats.editsUploaded++;
        } else {
            spdlog::error("Failed to save edit to Firebase: {}", result.errorMessage);
        }
    });

    return true;
}

bool WorldPersistenceManager::DeleteEditFromFirebase(const std::string& editId) {
    if (!m_firebase) return false;

    // Would need to know chunk ID to construct path
    // For now, just log
    spdlog::warn("Delete from Firebase not fully implemented (need chunk ID lookup)");
    return false;
}

void WorldPersistenceManager::LoadEditsFromFirebase(const WorldEditQuery& query,
                                                     std::function<void(const std::vector<WorldEdit>&)> callback) {
    if (!m_firebase) {
        if (callback) callback({});
        return;
    }

    // Load edits for each chunk in the query range
    std::string basePath = "worlds/" + query.worldId + "/edits";

    m_firebase->Get(basePath, [this, callback](const FirebaseResult& result) {
        std::vector<WorldEdit> edits;

        if (result.success && result.data.is_object()) {
            for (const auto& [chunkKey, chunkData] : result.data.items()) {
                if (chunkData.is_object()) {
                    for (const auto& [editId, editData] : chunkData.items()) {
                        try {
                            WorldEdit edit = WorldEdit::FromJson(editData);
                            edits.push_back(edit);
                        } catch (const std::exception& e) {
                            spdlog::warn("Failed to parse edit from Firebase: {}", e.what());
                        }
                    }
                }
            }
        }

        if (callback) callback(edits);
    });
}

// =============================================================================
// Conflict Detection and Resolution
// =============================================================================

bool WorldPersistenceManager::DetectConflict(const WorldEdit& localEdit, const WorldEdit& remoteEdit) {
    // Conflict if same ID but different content or timestamp
    if (localEdit.id != remoteEdit.id) return false;

    // Check if timestamps differ significantly
    int64_t timeDiff = std::abs(static_cast<int64_t>(localEdit.timestamp) -
                                 static_cast<int64_t>(remoteEdit.timestamp));

    return timeDiff > 1000; // More than 1 second difference
}

EditConflict WorldPersistenceManager::CreateConflict(const WorldEdit& localEdit,
                                                      const WorldEdit& remoteEdit) {
    EditConflict conflict;
    conflict.localEdit = localEdit;
    conflict.remoteEdit = remoteEdit;
    conflict.conflictReason = "Timestamp mismatch";
    conflict.suggestedResolution = m_config.defaultConflictResolution;
    return conflict;
}

void WorldPersistenceManager::ResolveConflictInternal(const EditConflict& conflict,
                                                       ConflictResolution resolution) {
    switch (resolution) {
        case ConflictResolution::KeepLocal:
            // Keep local version, mark as needing upload
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_pendingUploads.push_back(conflict.localEdit.id);
            }
            break;

        case ConflictResolution::KeepRemote:
            // Overwrite local with remote
            SaveEditToSQLite(conflict.remoteEdit);
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_editCache[conflict.remoteEdit.id] = conflict.remoteEdit;
            }
            break;

        case ConflictResolution::KeepBoth:
            // Generate new ID for local edit
            {
                WorldEdit newLocal = conflict.localEdit;
                newLocal.id = GenerateEditId();
                SaveEdit(newLocal);
                SaveEdit(conflict.remoteEdit);
            }
            break;

        case ConflictResolution::MergeChanges:
            // Attempt to merge (simplified: use newer timestamp)
            {
                const WorldEdit& newer = (conflict.localEdit.timestamp > conflict.remoteEdit.timestamp)
                                          ? conflict.localEdit : conflict.remoteEdit;
                SaveEdit(newer);
            }
            break;

        case ConflictResolution::AskUser:
            // Should not reach here if properly handled
            spdlog::warn("AskUser resolution strategy reached ResolveConflictInternal");
            break;
    }
}

// =============================================================================
// Sync Helpers
// =============================================================================

void WorldPersistenceManager::PerformUpload() {
    std::vector<std::string> toUpload;
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        toUpload = m_pendingUploads;
    }

    int uploaded = 0;
    for (const auto& editId : toUpload) {
        auto* edit = GetEditById(editId);
        if (edit && SaveEditToFirebase(*edit)) {
            uploaded++;

            if (OnSyncProgress) {
                OnSyncProgress(uploaded, static_cast<int>(toUpload.size()));
            }

            // Limit uploads per sync
            if (uploaded >= m_config.maxEditsPerSync) {
                break;
            }
        }
    }

    // Remove uploaded edits from pending list
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_pendingUploads.erase(
            std::remove_if(m_pendingUploads.begin(), m_pendingUploads.end(),
                           [&toUpload, uploaded](const std::string& id) {
                               auto it = std::find(toUpload.begin(), toUpload.begin() + uploaded, id);
                               return it != toUpload.begin() + uploaded;
                           }),
            m_pendingUploads.end()
        );
    }

    spdlog::info("Uploaded {} edits to Firebase", uploaded);
}

void WorldPersistenceManager::PerformDownload() {
    WorldEditQuery query;
    query.worldId = m_config.worldId;
    query.maxResults = m_config.maxEditsPerSync;

    LoadEditsFromFirebase(query, [this](const std::vector<WorldEdit>& remoteEdits) {
        ProcessSyncedEdits(remoteEdits);
    });
}

void WorldPersistenceManager::ProcessSyncedEdits(const std::vector<WorldEdit>& remoteEdits) {
    int downloaded = 0;

    for (const auto& remoteEdit : remoteEdits) {
        // Check if we have a local version
        auto* localEdit = GetEditById(remoteEdit.id);

        if (localEdit) {
            // Check for conflict
            if (DetectConflict(*localEdit, remoteEdit)) {
                EditConflict conflict = CreateConflict(*localEdit, remoteEdit);

                if (m_config.enableConflictCallbacks && OnConflictDetected) {
                    OnConflictDetected(conflict);
                }

                if (m_config.defaultConflictResolution == ConflictResolution::AskUser) {
                    // Queue for user resolution
                    std::lock_guard<std::mutex> lock(m_cacheMutex);
                    m_pendingConflicts.push_back(conflict);
                    m_stats.conflictsDetected++;
                } else {
                    // Auto-resolve
                    ResolveConflictInternal(conflict, m_config.defaultConflictResolution);
                    m_stats.conflictsResolved++;
                }

                continue;
            }
        }

        // No conflict, just save the remote edit
        SaveEditToSQLite(remoteEdit);
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_editCache[remoteEdit.id] = remoteEdit;
        }

        downloaded++;
        m_stats.editsDownloaded++;
        m_stats.totalEditsRemote++;
    }

    spdlog::info("Downloaded {} edits from Firebase", downloaded);
}

// =============================================================================
// Utilities
// =============================================================================

std::string WorldPersistenceManager::GenerateEditId() const {
    // Generate a unique ID using timestamp + random component
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    ss << std::hex << timestamp << "_" << rand();
    return ss.str();
}

uint64_t WorldPersistenceManager::GetCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string WorldPersistenceManager::ChunkIdToString(const glm::ivec3& chunkId) const {
    std::stringstream ss;
    ss << chunkId.x << "_" << chunkId.y << "_" << chunkId.z;
    return ss.str();
}

// =============================================================================
// WorldPersistenceUI Implementation
// =============================================================================

WorldPersistenceUI::WorldPersistenceUI() = default;

void WorldPersistenceUI::Initialize(WorldPersistenceManager* manager) {
    m_manager = manager;
    if (m_manager) {
        m_tempConfig = m_manager->GetConfig();
    }
}

void WorldPersistenceUI::Render() {
    if (!m_manager) {
        ImGui::Text("World Persistence Manager not initialized");
        return;
    }

    if (ImGui::BeginTabBar("PersistenceTabs")) {
        if (ImGui::BeginTabItem("Controls")) {
            RenderModeSelector();
            ImGui::Separator();
            RenderSyncControls();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Statistics")) {
            RenderStatistics();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Conflicts")) {
            RenderConflictList();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings")) {
            RenderSettingsPanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void WorldPersistenceUI::RenderModeSelector() {
    ImGui::Text("Storage Mode:");

    auto currentMode = m_manager->GetStorageMode();
    const char* modes[] = {"Online (Firebase)", "Offline (SQLite)", "Hybrid (Both)"};
    int modeIndex = static_cast<int>(currentMode);

    if (ImGui::Combo("##StorageMode", &modeIndex, modes, 3)) {
        m_manager->SetStorageMode(static_cast<WorldPersistenceManager::StorageMode>(modeIndex));
    }

    // Show status indicators
    ImGui::Spacing();
    if (m_manager->IsOnlineMode()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Firebase: Connected");
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Firebase: Offline");
    }

    if (m_manager->IsOfflineMode()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "SQLite: Active");
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "SQLite: Inactive");
    }
}

void WorldPersistenceUI::RenderSyncControls() {
    ImGui::Text("Synchronization:");

    int pendingUploads = m_manager->GetPendingUploadCount();
    ImGui::Text("Pending Uploads: %d", pendingUploads);

    ImGui::BeginDisabled(m_syncInProgress);

    if (ImGui::Button("Upload Local Edits")) {
        m_syncInProgress = true;
        m_manager->SyncToFirebase();
        m_syncInProgress = false;
    }

    ImGui::SameLine();
    if (ImGui::Button("Download Latest Edits")) {
        m_syncInProgress = true;
        m_manager->SyncFromFirebase();
        m_syncInProgress = false;
    }

    if (ImGui::Button("Full Sync (Bidirectional)")) {
        m_syncInProgress = true;
        m_manager->SyncBidirectional();
        m_syncInProgress = false;
    }

    ImGui::EndDisabled();

    if (m_syncInProgress) {
        ImGui::SameLine();
        ImGui::Text("Syncing...");
    }
}

void WorldPersistenceUI::RenderStatistics() {
    const auto& stats = m_manager->GetStats();

    ImGui::Text("Total Edits (Local): %llu", stats.totalEditsLocal);
    ImGui::Text("Total Edits (Remote): %llu", stats.totalEditsRemote);
    ImGui::Text("Edits Uploaded: %llu", stats.editsUploaded);
    ImGui::Text("Edits Downloaded: %llu", stats.editsDownloaded);
    ImGui::Separator();
    ImGui::Text("Conflicts Detected: %llu", stats.conflictsDetected);
    ImGui::Text("Conflicts Resolved: %llu", stats.conflictsResolved);
    ImGui::Separator();
    ImGui::Text("Last Sync Duration: %.2f seconds", stats.lastSyncDuration);

    if (stats.lastSyncTime > 0) {
        // Convert timestamp to readable format
        time_t time = static_cast<time_t>(stats.lastSyncTime / 1000);
        ImGui::Text("Last Sync: %s", ctime(&time));
    }

    if (ImGui::Button("Reset Statistics")) {
        m_manager->ResetStats();
    }
}

void WorldPersistenceUI::RenderConflictList() {
    auto conflicts = m_manager->GetPendingConflicts();

    ImGui::Text("Pending Conflicts: %d", static_cast<int>(conflicts.size()));

    if (conflicts.empty()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "No conflicts to resolve");
        return;
    }

    ImGui::Separator();

    for (size_t i = 0; i < conflicts.size(); i++) {
        const auto& conflict = conflicts[i];

        ImGui::PushID(static_cast<int>(i));

        if (ImGui::TreeNode("Conflict", "Conflict #%d: %s", static_cast<int>(i + 1),
                            conflict.conflictReason.c_str())) {

            ImGui::Text("Edit ID: %s", conflict.localEdit.id.c_str());
            ImGui::Text("Local Timestamp: %llu", conflict.localEdit.timestamp);
            ImGui::Text("Remote Timestamp: %llu", conflict.remoteEdit.timestamp);

            if (ImGui::Button("Keep Local")) {
                m_manager->ResolveConflict(conflict.localEdit.id, ConflictResolution::KeepLocal);
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep Remote")) {
                m_manager->ResolveConflict(conflict.localEdit.id, ConflictResolution::KeepRemote);
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep Both")) {
                m_manager->ResolveConflict(conflict.localEdit.id, ConflictResolution::KeepBoth);
            }
            ImGui::SameLine();
            if (ImGui::Button("Merge")) {
                m_manager->ResolveConflict(conflict.localEdit.id, ConflictResolution::MergeChanges);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void WorldPersistenceUI::RenderSettingsPanel() {
    ImGui::Text("Persistence Settings:");

    ImGui::Checkbox("Auto Sync", &m_tempConfig.autoSync);
    ImGui::SliderFloat("Sync Interval (seconds)", &m_tempConfig.syncInterval, 5.0f, 300.0f);
    ImGui::SliderInt("Max Edits Per Sync", &m_tempConfig.maxEditsPerSync, 10, 500);

    const char* conflictStrategies[] = {"Keep Local", "Keep Remote", "Keep Both", "Merge Changes", "Ask User"};
    int strategyIndex = static_cast<int>(m_tempConfig.defaultConflictResolution);
    if (ImGui::Combo("Default Conflict Resolution", &strategyIndex, conflictStrategies, 5)) {
        m_tempConfig.defaultConflictResolution = static_cast<ConflictResolution>(strategyIndex);
    }

    ImGui::Checkbox("Enable Conflict Callbacks", &m_tempConfig.enableConflictCallbacks);

    if (ImGui::Button("Apply Settings")) {
        m_manager->SetConfig(m_tempConfig);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Current")) {
        m_tempConfig = m_manager->GetConfig();
    }
}

void WorldPersistenceUI::ShowConflictResolutionDialog(const EditConflict& conflict) {
    ImGui::OpenPopup("Resolve Conflict");

    if (ImGui::BeginPopupModal("Resolve Conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Conflict detected for edit:");
        ImGui::Text("Edit ID: %s", conflict.localEdit.id.c_str());
        ImGui::Text("Reason: %s", conflict.conflictReason.c_str());

        ImGui::Separator();
        ImGui::Text("How would you like to resolve this conflict?");

        if (ImGui::Button("Keep Local Version")) {
            m_manager->ResolveConflict(conflict.localEdit.id, ConflictResolution::KeepLocal);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep Remote Version")) {
            m_manager->ResolveConflict(conflict.localEdit.id, ConflictResolution::KeepRemote);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Keep Both Versions")) {
            m_manager->ResolveConflict(conflict.localEdit.id, ConflictResolution::KeepBoth);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Merge Changes")) {
            m_manager->ResolveConflict(conflict.localEdit.id, ConflictResolution::MergeChanges);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace Nova
