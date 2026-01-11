#include "FirebasePersistence.hpp"
#include "../terrain/VoxelTerrain.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Nova {

// ============================================================================
// TerrainModificationBatch Implementation
// ============================================================================

bool TerrainModificationBatch::CanMergeWith(const TerrainModificationBatch& other) const {
    // Can merge if same type, same material, and within merge radius
    if (type != other.type) return false;
    if (material != other.material) return false;

    float dist = glm::length(position - other.position);
    float maxDist = std::max(size.x, std::max(size.y, size.z)) +
                    std::max(other.size.x, std::max(other.size.y, other.size.z));

    return dist < maxDist;
}

void TerrainModificationBatch::MergeWith(const TerrainModificationBatch& other) {
    // Expand bounding box to include both modifications
    glm::vec3 minA = position - size;
    glm::vec3 maxA = position + size;
    glm::vec3 minB = other.position - other.size;
    glm::vec3 maxB = other.position + other.size;

    glm::vec3 newMin = glm::min(minA, minB);
    glm::vec3 newMax = glm::max(maxA, maxB);

    position = (newMin + newMax) * 0.5f;
    size = (newMax - newMin) * 0.5f;

    // Use latest timestamp
    timestamp = std::max(timestamp, other.timestamp);

    // Average color
    color = (color + other.color) * 0.5f;
}

// ============================================================================
// FirebasePersistence Implementation
// ============================================================================

FirebasePersistence& FirebasePersistence::Instance() {
    static FirebasePersistence instance;
    return instance;
}

void FirebasePersistence::Initialize(std::shared_ptr<FirebaseClient> client, const Config& config) {
    m_firebase = std::move(client);
    m_config = config;

    // Load any pending local backup
    if (m_config.saveLocalBackup) {
        LoadLocalBackup();
    }

    m_lastSyncTime = std::chrono::steady_clock::now();
    m_initialized = true;
}

void FirebasePersistence::Shutdown() {
    if (!m_initialized) return;

    // Force sync any pending changes
    if (HasPendingSync()) {
        ForceSync();
    }

    // Save local backup
    if (m_config.saveLocalBackup) {
        SaveLocalBackup();
    }

    m_initialized = false;
}

void FirebasePersistence::Update(float deltaTime) {
    if (!m_initialized || !m_firebase) return;

    m_timeSinceLastModification += deltaTime;
    m_timeSinceLastSync += deltaTime;

    // Clean up old bandwidth tracking entries (older than 1 minute)
    auto now = std::chrono::steady_clock::now();
    m_recentOperations.erase(
        std::remove_if(m_recentOperations.begin(), m_recentOperations.end(),
            [now](const auto& entry) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.first);
                return age.count() > 60;
            }),
        m_recentOperations.end()
    );

    // Check if we should sync
    CheckAndSync();
}

// ============================================================================
// Terrain Persistence
// ============================================================================

void FirebasePersistence::RecordTerrainModification(const TerrainModificationBatch& mod) {
    std::lock_guard<std::mutex> lock(m_modMutex);

    // Try to merge with existing modifications
    if (m_config.mergeOverlappingMods) {
        for (auto& existing : m_pendingModifications) {
            if (existing.CanMergeWith(mod)) {
                existing.MergeWith(mod);
                m_stats.mergedModifications++;
                m_timeSinceLastModification = 0.0f;

                if (OnPendingChanged) OnPendingChanged(m_pendingModifications.size());
                return;
            }
        }
    }

    m_pendingModifications.push_back(mod);
    m_stats.totalModificationsRecorded++;
    m_timeSinceLastModification = 0.0f;

    if (OnPendingChanged) OnPendingChanged(m_pendingModifications.size());

    // Check if we've hit the batch limit
    if (static_cast<int>(m_pendingModifications.size()) >= m_config.maxModificationsPerBatch) {
        CheckAndSync();
    }
}

void FirebasePersistence::MarkChunkModified(const glm::ivec3& chunkPos) {
    std::lock_guard<std::mutex> lock(m_modMutex);

    uint64_t key = GetChunkKey(chunkPos);
    auto& chunk = m_modifiedChunks[key];
    chunk.position = chunkPos;
    chunk.modified = true;
    chunk.lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    chunk.modificationCount++;
}

void FirebasePersistence::RecordVoxelChange(const glm::ivec3& chunkPos, const glm::ivec3& localPos,
                                             float newDensity, uint8_t newMaterial) {
    if (!m_config.useDeltaCompression) {
        MarkChunkModified(chunkPos);
        return;
    }

    std::lock_guard<std::mutex> lock(m_modMutex);

    uint64_t key = GetChunkKey(chunkPos);
    auto& chunk = m_modifiedChunks[key];
    chunk.position = chunkPos;
    chunk.modified = true;
    chunk.lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Record delta change
    chunk.densityChanges.emplace_back(localPos, newDensity);
    chunk.materialChanges.emplace_back(localPos, newMaterial);
}

void FirebasePersistence::ForceSync() {
    if (!m_initialized || m_syncInProgress) return;

    PerformSync();
}

bool FirebasePersistence::HasPendingSync() const {
    std::lock_guard<std::mutex> lock(m_modMutex);
    return !m_pendingModifications.empty() || !m_modifiedChunks.empty();
}

size_t FirebasePersistence::GetPendingModificationCount() const {
    std::lock_guard<std::mutex> lock(m_modMutex);
    return m_pendingModifications.size();
}

size_t FirebasePersistence::GetPendingChunkCount() const {
    std::lock_guard<std::mutex> lock(m_modMutex);
    return m_modifiedChunks.size();
}

// ============================================================================
// World Loading
// ============================================================================

void FirebasePersistence::SetWorldId(const std::string& worldId) {
    // Sync current world before switching
    if (!m_worldId.empty() && HasPendingSync()) {
        ForceSync();
    }

    m_worldId = worldId;

    // Clear pending modifications for new world
    std::lock_guard<std::mutex> lock(m_modMutex);
    m_pendingModifications.clear();
    m_modifiedChunks.clear();
}

void FirebasePersistence::LoadWorld(std::function<void(bool)> callback) {
    if (!m_firebase || m_worldId.empty()) {
        if (callback) callback(false);
        return;
    }

    // Load world metadata first
    std::string path = "worlds/" + m_worldId + "/metadata";
    m_firebase->Get(path, [this, callback](const FirebaseResult& result) {
        if (callback) callback(result.success);
    });
}

void FirebasePersistence::LoadChunks(const std::vector<glm::ivec3>& chunks,
                                      std::function<void(const glm::ivec3&, const nlohmann::json&)> onChunkLoaded,
                                      std::function<void()> onComplete) {
    if (!m_firebase || m_worldId.empty() || chunks.empty()) {
        if (onComplete) onComplete();
        return;
    }

    auto remaining = std::make_shared<int>(static_cast<int>(chunks.size()));

    for (const auto& chunkPos : chunks) {
        std::stringstream ss;
        ss << chunkPos.x << "_" << chunkPos.y << "_" << chunkPos.z;

        m_firebase->LoadTerrainChunk(m_worldId, chunkPos.x, chunkPos.y, chunkPos.z,
            [onChunkLoaded, onComplete, remaining, chunkPos](bool success, const nlohmann::json& data) {
                if (success && onChunkLoaded) {
                    onChunkLoaded(chunkPos, data);
                }

                (*remaining)--;
                if (*remaining <= 0 && onComplete) {
                    onComplete();
                }
            });
    }
}

uint64_t FirebasePersistence::SubscribeToChanges(std::function<void(const TerrainModificationBatch&)> callback) {
    uint64_t id = m_nextSubscriptionId++;
    m_changeSubscriptions.emplace_back(id, std::move(callback));

    // Also subscribe via Firebase for real-time updates
    if (m_firebase && !m_worldId.empty()) {
        m_firebase->SubscribeToTerrainModifications(m_worldId,
            [this](const nlohmann::json& data) {
                auto mod = DeserializeModification(data);
                for (const auto& [subId, cb] : m_changeSubscriptions) {
                    if (cb) cb(mod);
                }
            });
    }

    return id;
}

void FirebasePersistence::UnsubscribeFromChanges(uint64_t subscriptionId) {
    m_changeSubscriptions.erase(
        std::remove_if(m_changeSubscriptions.begin(), m_changeSubscriptions.end(),
            [subscriptionId](const auto& pair) { return pair.first == subscriptionId; }),
        m_changeSubscriptions.end()
    );
}

// ============================================================================
// Editor Configuration
// ============================================================================

bool FirebasePersistence::ShouldPersist(const std::string& eventType) const {
    // Check for override
    auto it = m_persistenceOverrides.find(eventType);
    if (it != m_persistenceOverrides.end()) {
        return it->second;
    }

    // Check default config from EventTypeRegistry
    if (const auto* config = EventTypeRegistry::Instance().GetConfig(eventType)) {
        return config->defaultPersistenceMode != PersistenceMode::None;
    }

    return false;
}

void FirebasePersistence::SetPersistenceOverride(const std::string& eventType, bool persist) {
    m_persistenceOverrides[eventType] = persist;
}

void FirebasePersistence::ClearPersistenceOverride(const std::string& eventType) {
    m_persistenceOverrides.erase(eventType);
}

// ============================================================================
// Private Methods
// ============================================================================

void FirebasePersistence::CheckAndSync() {
    if (m_syncInProgress) return;
    if (!HasPendingSync()) return;

    bool shouldSync = false;

    // Check conditions for sync
    if (m_timeSinceLastSync >= m_config.maxSyncInterval) {
        // Maximum time reached
        shouldSync = true;
    } else if (m_timeSinceLastModification >= m_config.idleSyncDelay &&
               m_timeSinceLastSync >= m_config.minSyncInterval) {
        // Idle for long enough and minimum interval passed
        shouldSync = true;
    } else if (GetPendingModificationCount() >= static_cast<size_t>(m_config.maxModificationsPerBatch)) {
        // Batch is full
        shouldSync = true;
    }

    if (shouldSync && CanPerformOperation()) {
        PerformSync();
    }
}

void FirebasePersistence::PerformSync() {
    if (!m_firebase || m_worldId.empty()) return;

    m_syncInProgress = true;

    if (OnSyncStarted) OnSyncStarted();

    auto startTime = std::chrono::steady_clock::now();

    // Process merging first
    ProcessMerging();

    // Sync modifications batch
    SyncModificationBatch();

    // Sync modified chunks
    SyncChunks();

    auto endTime = std::chrono::steady_clock::now();
    float syncDuration = std::chrono::duration<float>(endTime - startTime).count();

    m_stats.lastSyncTime = syncDuration;
    m_stats.avgSyncTime = (m_stats.avgSyncTime + syncDuration) * 0.5f;

    m_timeSinceLastSync = 0.0f;
    m_lastSyncTime = std::chrono::steady_clock::now();
    m_syncInProgress = false;
    m_retryCount = 0;

    if (OnSyncCompleted) OnSyncCompleted(true);
}

void FirebasePersistence::SyncModificationBatch() {
    std::vector<TerrainModificationBatch> toSync;

    {
        std::lock_guard<std::mutex> lock(m_modMutex);
        if (m_pendingModifications.empty()) return;

        // Take up to maxModificationsPerBatch
        int count = std::min(static_cast<int>(m_pendingModifications.size()),
                             m_config.maxModificationsPerBatch);

        toSync.assign(m_pendingModifications.begin(),
                      m_pendingModifications.begin() + count);

        m_pendingModifications.erase(m_pendingModifications.begin(),
                                      m_pendingModifications.begin() + count);
    }

    // Build batch update
    nlohmann::json batch;
    for (const auto& mod : toSync) {
        std::string key = m_firebase->GeneratePushId();
        batch[key] = SerializeModification(mod);
    }

    std::string path = "worlds/" + m_worldId + "/modifications";
    size_t dataSize = batch.dump().size();

    m_firebase->Update(path, batch, [this, count = toSync.size(), dataSize](const FirebaseResult& result) {
        if (result.success) {
            m_stats.totalModificationsSynced += count;
            RecordOperation(dataSize);
        } else {
            m_stats.failedSyncs++;
            if (OnSyncError) OnSyncError(result.errorMessage);
        }
    });
}

void FirebasePersistence::SyncChunks() {
    std::vector<std::pair<uint64_t, ChunkModificationState>> toSync;

    {
        std::lock_guard<std::mutex> lock(m_modMutex);
        if (m_modifiedChunks.empty()) return;

        // Take up to maxChunksPerSync, prioritizing by modification count
        std::vector<std::pair<uint64_t, int>> priorities;
        for (const auto& [key, chunk] : m_modifiedChunks) {
            if (chunk.modified) {
                priorities.emplace_back(key, chunk.modificationCount);
            }
        }

        std::sort(priorities.begin(), priorities.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        int count = std::min(static_cast<int>(priorities.size()), m_config.maxChunksPerSync);
        for (int i = 0; i < count; i++) {
            uint64_t key = priorities[i].first;
            toSync.emplace_back(key, m_modifiedChunks[key]);
            m_modifiedChunks[key].modified = false;
            m_modifiedChunks[key].densityChanges.clear();
            m_modifiedChunks[key].materialChanges.clear();
        }
    }

    for (const auto& [key, chunk] : toSync) {
        nlohmann::json chunkData;

        if (m_config.useDeltaCompression && !chunk.densityChanges.empty()) {
            chunkData = SerializeChunkDelta(chunk);
        } else {
            // Full chunk data would come from VoxelTerrain
            // For now, just save metadata
            chunkData["position"] = {chunk.position.x, chunk.position.y, chunk.position.z};
            chunkData["lastModified"] = chunk.lastModified;
        }

        size_t dataSize = chunkData.dump().size();

        m_firebase->SaveTerrainChunk(m_worldId,
            chunk.position.x, chunk.position.y, chunk.position.z,
            chunkData,
            [this, dataSize](bool success) {
                if (success) {
                    m_stats.totalChunksSynced++;
                    RecordOperation(dataSize);
                } else {
                    m_stats.failedSyncs++;
                }
            });
    }
}

void FirebasePersistence::ProcessMerging() {
    if (!m_config.mergeOverlappingMods) return;

    std::lock_guard<std::mutex> lock(m_modMutex);

    // Simple O(n^2) merging - could optimize with spatial partitioning
    bool merged;
    do {
        merged = false;
        for (size_t i = 0; i < m_pendingModifications.size() && !merged; i++) {
            for (size_t j = i + 1; j < m_pendingModifications.size() && !merged; j++) {
                if (m_pendingModifications[i].CanMergeWith(m_pendingModifications[j])) {
                    m_pendingModifications[i].MergeWith(m_pendingModifications[j]);
                    m_pendingModifications.erase(m_pendingModifications.begin() + j);
                    m_stats.mergedModifications++;
                    merged = true;
                }
            }
        }
    } while (merged);
}

nlohmann::json FirebasePersistence::SerializeModification(const TerrainModificationBatch& mod) const {
    nlohmann::json json;
    json["type"] = static_cast<int>(mod.type);
    json["position"] = {mod.position.x, mod.position.y, mod.position.z};
    json["size"] = {mod.size.x, mod.size.y, mod.size.z};
    json["params"] = {mod.params.x, mod.params.y, mod.params.z, mod.params.w};
    json["material"] = mod.material;
    json["color"] = {mod.color.x, mod.color.y, mod.color.z};
    json["timestamp"] = mod.timestamp;
    json["clientId"] = mod.clientId;
    return json;
}

TerrainModificationBatch FirebasePersistence::DeserializeModification(const nlohmann::json& json) const {
    TerrainModificationBatch mod;
    mod.type = static_cast<TerrainModificationBatch::Type>(json.value("type", 0));

    if (json.contains("position") && json["position"].is_array()) {
        auto& p = json["position"];
        mod.position = glm::vec3(p[0], p[1], p[2]);
    }

    if (json.contains("size") && json["size"].is_array()) {
        auto& s = json["size"];
        mod.size = glm::vec3(s[0], s[1], s[2]);
    }

    if (json.contains("params") && json["params"].is_array()) {
        auto& pr = json["params"];
        mod.params = glm::vec4(pr[0], pr[1], pr[2], pr[3]);
    }

    mod.material = json.value("material", 0);

    if (json.contains("color") && json["color"].is_array()) {
        auto& c = json["color"];
        mod.color = glm::vec3(c[0], c[1], c[2]);
    }

    mod.timestamp = json.value("timestamp", 0ULL);
    mod.clientId = json.value("clientId", 0U);

    return mod;
}

nlohmann::json FirebasePersistence::SerializeChunkDelta(const ChunkModificationState& chunk) const {
    nlohmann::json json;
    json["position"] = {chunk.position.x, chunk.position.y, chunk.position.z};
    json["lastModified"] = chunk.lastModified;
    json["isDelta"] = true;

    // Serialize density changes
    if (!chunk.densityChanges.empty()) {
        nlohmann::json densities = nlohmann::json::array();
        for (const auto& [pos, density] : chunk.densityChanges) {
            densities.push_back({pos.x, pos.y, pos.z, density});
        }
        json["densityChanges"] = densities;
    }

    // Serialize material changes
    if (!chunk.materialChanges.empty()) {
        nlohmann::json materials = nlohmann::json::array();
        for (const auto& [pos, mat] : chunk.materialChanges) {
            materials.push_back({pos.x, pos.y, pos.z, mat});
        }
        json["materialChanges"] = materials;
    }

    return json;
}

bool FirebasePersistence::CanPerformOperation() const {
    // Check operations per minute
    if (static_cast<int>(m_recentOperations.size()) >= m_config.maxOperationsPerMinute) {
        return false;
    }

    // Check bandwidth per minute
    size_t totalBytes = 0;
    for (const auto& [time, bytes] : m_recentOperations) {
        totalBytes += bytes;
    }

    return totalBytes < static_cast<size_t>(m_config.maxBytesPerMinute);
}

void FirebasePersistence::RecordOperation(size_t bytes) {
    m_recentOperations.emplace_back(std::chrono::steady_clock::now(), bytes);
    m_stats.totalBytesSent += bytes;
    m_stats.totalOperations++;
}

void FirebasePersistence::SaveLocalBackup() {
    if (m_worldId.empty()) return;

    try {
        std::filesystem::create_directories(m_config.localBackupPath);

        std::string filename = m_config.localBackupPath + m_worldId + "_pending.json";
        std::ofstream file(filename);
        if (!file.is_open()) return;

        nlohmann::json backup;

        // Save pending modifications
        nlohmann::json mods = nlohmann::json::array();
        {
            std::lock_guard<std::mutex> lock(m_modMutex);
            for (const auto& mod : m_pendingModifications) {
                mods.push_back(SerializeModification(mod));
            }
        }
        backup["modifications"] = mods;

        // Save modified chunk positions
        nlohmann::json chunks = nlohmann::json::array();
        {
            std::lock_guard<std::mutex> lock(m_modMutex);
            for (const auto& [key, chunk] : m_modifiedChunks) {
                if (chunk.modified) {
                    chunks.push_back({chunk.position.x, chunk.position.y, chunk.position.z});
                }
            }
        }
        backup["modifiedChunks"] = chunks;

        file << backup.dump(2);
    } catch (...) {
        // Ignore backup errors
    }
}

void FirebasePersistence::LoadLocalBackup() {
    if (m_worldId.empty()) return;

    try {
        std::string filename = m_config.localBackupPath + m_worldId + "_pending.json";
        std::ifstream file(filename);
        if (!file.is_open()) return;

        nlohmann::json backup;
        file >> backup;

        // Load pending modifications
        if (backup.contains("modifications") && backup["modifications"].is_array()) {
            std::lock_guard<std::mutex> lock(m_modMutex);
            for (const auto& modJson : backup["modifications"]) {
                m_pendingModifications.push_back(DeserializeModification(modJson));
            }
        }

        // Load modified chunk positions
        if (backup.contains("modifiedChunks") && backup["modifiedChunks"].is_array()) {
            std::lock_guard<std::mutex> lock(m_modMutex);
            for (const auto& chunkJson : backup["modifiedChunks"]) {
                if (chunkJson.is_array() && chunkJson.size() >= 3) {
                    glm::ivec3 pos(chunkJson[0], chunkJson[1], chunkJson[2]);
                    MarkChunkModified(pos);
                }
            }
        }

        // Delete backup after loading
        std::filesystem::remove(filename);
    } catch (...) {
        // Ignore load errors
    }
}

// ============================================================================
// TerrainPersistenceIntegration Implementation
// ============================================================================

static VoxelTerrain* s_connectedTerrain = nullptr;

void TerrainPersistenceIntegration::Connect(VoxelTerrain* terrain) {
    if (!terrain) return;

    s_connectedTerrain = terrain;

    terrain->OnTerrainModified = [](const TerrainModification& mod) {
        OnTerrainModified(mod);
    };
}

void TerrainPersistenceIntegration::Disconnect() {
    if (s_connectedTerrain) {
        s_connectedTerrain->OnTerrainModified = nullptr;
        s_connectedTerrain = nullptr;
    }
}

void TerrainPersistenceIntegration::OnTerrainModified(const TerrainModification& mod) {
    auto& persistence = FirebasePersistence::Instance();

    // Record chunk as modified
    persistence.MarkChunkModified(mod.chunkPos);

    // Create modification batch entry
    TerrainModificationBatch batch;
    batch.type = TerrainModificationBatch::Type::Sculpt;  // Default
    batch.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    batch.clientId = ReplicationSystem::Instance().GetLocalClientId();

    // Calculate bounds from changed voxels
    if (!mod.newVoxels.empty()) {
        glm::vec3 minPos(std::numeric_limits<float>::max());
        glm::vec3 maxPos(std::numeric_limits<float>::lowest());

        for (const auto& [pos, voxel] : mod.newVoxels) {
            minPos = glm::min(minPos, glm::vec3(pos));
            maxPos = glm::max(maxPos, glm::vec3(pos));
            batch.material = static_cast<uint8_t>(voxel.material);
            batch.color = voxel.color;
        }

        batch.position = (minPos + maxPos) * 0.5f + glm::vec3(mod.chunkPos * 32);
        batch.size = (maxPos - minPos + glm::vec3(1.0f)) * 0.5f;
    }

    persistence.RecordTerrainModification(batch);
}

} // namespace Nova
