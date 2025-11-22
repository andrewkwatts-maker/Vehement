#include "FirebaseWorldStorage.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>

namespace Network {
namespace Firebase {

FirebaseWorldStorage& FirebaseWorldStorage::getInstance() {
    static FirebaseWorldStorage instance;
    return instance;
}

FirebaseWorldStorage::FirebaseWorldStorage() {}

FirebaseWorldStorage::~FirebaseWorldStorage() {
    shutdown();
}

bool FirebaseWorldStorage::initialize() {
    if (m_initialized) return true;

    if (!FirebaseCore::getInstance().isInitialized()) {
        return false;
    }

    // Load cached data
    if (m_cacheEnabled) {
        loadCacheFromDisk();
    }

    m_initialized = true;
    return true;
}

void FirebaseWorldStorage::shutdown() {
    if (!m_initialized) return;

    // Save cache and sync pending edits
    if (m_cacheEnabled) {
        saveCacheToDisk();
    }

    syncOfflineEdits();

    m_initialized = false;
}

void FirebaseWorldStorage::update(float deltaTime) {
    if (!m_initialized) return;

    // Batch upload timer
    m_uploadBatchTimer += deltaTime;
    if (m_uploadBatchTimer >= UPLOAD_BATCH_INTERVAL && !m_pendingUploads.empty()) {
        m_uploadBatchTimer = 0.0f;

        std::vector<WorldEdit> batch;
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            size_t count = std::min(m_pendingUploads.size(), MAX_BATCH_SIZE);
            batch.assign(m_pendingUploads.begin(), m_pendingUploads.begin() + count);
            m_pendingUploads.erase(m_pendingUploads.begin(), m_pendingUploads.begin() + count);
        }

        if (!batch.empty()) {
            uploadEdits(batch, nullptr);
        }
    }

    // Periodic sync
    m_syncTimer += deltaTime;
    if (m_syncTimer >= SYNC_INTERVAL) {
        m_syncTimer = 0.0f;

        if (m_offlineEnabled) {
            syncOfflineEdits();
        }
    }
}

void FirebaseWorldStorage::createWorld(const std::string& name, const std::string& description,
                                       bool isPublic, WorldInfoCallback callback) {
    auto& core = FirebaseCore::getInstance();
    if (!core.isSignedIn()) {
        FirebaseError error;
        error.type = FirebaseErrorType::AuthError;
        error.message = "Not signed in";
        callback(WorldInfo(), error);
        return;
    }

    WorldInfo world;
    world.worldId = generateWorldId();
    world.name = name;
    world.description = description;
    world.ownerId = core.getCurrentUser().uid;
    world.ownerName = core.getCurrentUser().displayName;
    world.createdAt = std::chrono::system_clock::now();
    world.lastModified = world.createdAt;
    world.isPublic = isPublic;
    world.currentVersion = 1;

    // Owner has full permissions
    world.permissions[world.ownerId] = WorldPermission::Owner;

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + "/worlds?documentId=" + world.worldId;
    request.headers["Content-Type"] = "application/json";
    request.body = serializeWorldInfo(world);

    core.makeAuthenticatedRequest(request, [this, world, callback](const HttpResponse& response) {
        if (response.statusCode == 200 || response.statusCode == 201) {
            m_worldInfoCache[world.worldId] = world;
            callback(world, FirebaseError());
        } else {
            callback(WorldInfo(), parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::deleteWorld(const std::string& worldId,
                                       std::function<void(const FirebaseError&)> callback) {
    auto& core = FirebaseCore::getInstance();

    // Check permission
    if (getMyPermission(worldId) != WorldPermission::Owner) {
        FirebaseError error;
        error.type = FirebaseErrorType::PermissionDenied;
        error.message = "Only owner can delete world";
        callback(error);
        return;
    }

    HttpRequest request;
    request.method = "DELETE";
    request.url = core.getConfig().getFirestoreUrl() + "/worlds/" + worldId;

    core.makeAuthenticatedRequest(request, [this, worldId, callback](const HttpResponse& response) {
        if (response.statusCode == 200 || response.statusCode == 204) {
            m_worldInfoCache.erase(worldId);
            callback(FirebaseError());
        } else {
            callback(parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::getWorldInfo(const std::string& worldId, WorldInfoCallback callback) {
    // Check cache first
    auto it = m_worldInfoCache.find(worldId);
    if (it != m_worldInfoCache.end()) {
        callback(it->second, FirebaseError());
        return;
    }

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "GET";
    request.url = core.getConfig().getFirestoreUrl() + "/worlds/" + worldId;

    core.makeAuthenticatedRequest(request, [this, worldId, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            WorldInfo info = deserializeWorldInfo(response.body);
            m_worldInfoCache[worldId] = info;
            callback(info, FirebaseError());
        } else {
            callback(WorldInfo(), parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::listMyWorlds(
    std::function<void(const std::vector<WorldInfo>&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();
    std::string oderId = core.getCurrentUser().uid;

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"worlds\"}],"
          << "\"where\":{"
          << "\"fieldFilter\":{"
          << "\"field\":{\"fieldPath\":\"ownerId\"},"
          << "\"op\":\"EQUAL\","
          << "\"value\":{\"stringValue\":\"" << oderId << "\"}"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"lastModified\"},\"direction\":\"DESCENDING\"}]"
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<WorldInfo> worlds = parseWorldList(response.body);
            callback(worlds, FirebaseError());
        } else {
            callback({}, parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::listPublicWorlds(int limit, int offset,
    std::function<void(const std::vector<WorldInfo>&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"worlds\"}],"
          << "\"where\":{"
          << "\"fieldFilter\":{"
          << "\"field\":{\"fieldPath\":\"isPublic\"},"
          << "\"op\":\"EQUAL\","
          << "\"value\":{\"booleanValue\":true}"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"editCount\"},\"direction\":\"DESCENDING\"}],"
          << "\"offset\":" << offset << ","
          << "\"limit\":" << limit
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<WorldInfo> worlds = parseWorldList(response.body);
            callback(worlds, FirebaseError());
        } else {
            callback({}, parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::loadRegion(const WorldLocation& location, WorldLoadCallback callback) {
    // Check cache first
    if (m_cacheEnabled) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_regionCache.find(location);
        if (it != m_regionCache.end()) {
            callback(it->second, FirebaseError());
            return;
        }
    }

    downloadRegion(location, callback);
}

void FirebaseWorldStorage::loadRegions(const std::vector<WorldLocation>& locations,
    std::function<void(const std::vector<WorldRegionState>&, const FirebaseError&)> callback) {

    if (locations.empty()) {
        callback({}, FirebaseError());
        return;
    }

    auto results = std::make_shared<std::vector<WorldRegionState>>();
    auto remaining = std::make_shared<int>(static_cast<int>(locations.size()));
    auto hasError = std::make_shared<bool>(false);
    auto mutex = std::make_shared<std::mutex>();

    for (const auto& loc : locations) {
        loadRegion(loc, [results, remaining, hasError, mutex, callback](
            const WorldRegionState& state, const FirebaseError& error) {

            std::lock_guard<std::mutex> lock(*mutex);

            if (error) {
                *hasError = true;
            } else {
                results->push_back(state);
            }

            (*remaining)--;
            if (*remaining == 0) {
                if (*hasError) {
                    FirebaseError err;
                    err.type = FirebaseErrorType::ServerError;
                    err.message = "Failed to load some regions";
                    callback(*results, err);
                } else {
                    callback(*results, FirebaseError());
                }
            }
        });
    }
}

void FirebaseWorldStorage::loadNearbyRegions(const WorldLocation& center, int radius,
                                              WorldLoadCallback callback) {
    std::vector<WorldLocation> locations;

    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            for (int z = -radius; z <= radius; z++) {
                WorldLocation loc = center;
                loc.regionX += x;
                loc.regionY += y;
                loc.regionZ += z;
                locations.push_back(loc);
            }
        }
    }

    loadRegions(locations, [callback](const std::vector<WorldRegionState>& states,
                                       const FirebaseError& error) {
        // Return first region for simple callback
        if (!states.empty()) {
            callback(states[0], error);
        } else {
            callback(WorldRegionState(), error);
        }
    });
}

void FirebaseWorldStorage::saveEdit(const WorldEdit& edit, WorldSaveCallback callback) {
    // Check permissions
    WorldPermission perm = getMyPermission(edit.location.worldId);
    if (static_cast<int>(perm) < static_cast<int>(WorldPermission::Edit)) {
        FirebaseError error;
        error.type = FirebaseErrorType::PermissionDenied;
        error.message = "No edit permission for this world";
        if (callback) callback(0, error);
        return;
    }

    // Queue for batch upload
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_pendingUploads.push_back(edit);
    }

    // Update local cache immediately
    if (m_cacheEnabled) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto& region = m_regionCache[edit.location];
        region.location = edit.location;
        region.edits.push_back(edit);
        region.version = incrementVersion(edit.location);
        region.lastModified = std::chrono::system_clock::now();
    }

    if (callback) {
        callback(getCurrentVersion(edit.location), FirebaseError());
    }
}

void FirebaseWorldStorage::saveEdits(const std::vector<WorldEdit>& edits, WorldSaveCallback callback) {
    if (edits.empty()) {
        if (callback) callback(0, FirebaseError());
        return;
    }

    // Queue all edits
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_pendingUploads.insert(m_pendingUploads.end(), edits.begin(), edits.end());
    }

    // Update local cache
    if (m_cacheEnabled) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        for (const auto& edit : edits) {
            auto& region = m_regionCache[edit.location];
            region.location = edit.location;
            region.edits.push_back(edit);
        }
    }

    if (callback) {
        callback(getCurrentVersion(edits[0].location), FirebaseError());
    }
}

void FirebaseWorldStorage::deleteEdit(const std::string& editId, WorldSaveCallback callback) {
    // Find and remove edit from cache
    WorldLocation location;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        for (auto& [loc, region] : m_regionCache) {
            auto it = std::find_if(region.edits.begin(), region.edits.end(),
                [&editId](const WorldEdit& e) { return e.editId == editId; });

            if (it != region.edits.end()) {
                location = loc;
                region.edits.erase(it);
                found = true;
                break;
            }
        }
    }

    if (!found) {
        FirebaseError error;
        error.type = FirebaseErrorType::NotFound;
        error.message = "Edit not found";
        if (callback) callback(0, error);
        return;
    }

    // Queue delete operation
    WorldEdit deleteOp;
    deleteOp.editId = editId;
    deleteOp.operation = EditOperation::Delete;
    deleteOp.location = location;

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_pendingUploads.push_back(deleteOp);
    }

    if (callback) {
        callback(incrementVersion(location), FirebaseError());
    }
}

void FirebaseWorldStorage::getDelta(const WorldLocation& location, uint64_t fromVersion,
                                     DeltaCallback callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::string regionKey = location.getKey();

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"edits\"}],"
          << "\"where\":{"
          << "\"compositeFilter\":{"
          << "\"op\":\"AND\","
          << "\"filters\":["
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"regionKey\"},\"op\":\"EQUAL\",\"value\":{\"stringValue\":\"" << regionKey << "\"}}},"
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"version\"},\"op\":\"GREATER_THAN\",\"value\":{\"integerValue\":" << fromVersion << "}}}"
          << "]"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"version\"},\"direction\":\"ASCENDING\"}]"
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [this, location, fromVersion, callback](
        const HttpResponse& response) {

        if (response.statusCode == 200) {
            WorldDelta delta;
            delta.location = location;
            delta.fromVersion = fromVersion;
            delta.toVersion = getCurrentVersion(location);

            // Parse edits from response
            std::vector<WorldEdit> edits = parseEditsFromQuery(response.body);

            for (const auto& edit : edits) {
                if (edit.operation == EditOperation::Delete) {
                    delta.removedEditIds.push_back(edit.editId);
                } else if (edit.operation == EditOperation::Modify) {
                    delta.modifiedEdits.push_back(edit);
                } else {
                    delta.addedEdits.push_back(edit);
                }
            }

            callback(delta, FirebaseError());
        } else {
            callback(WorldDelta(), parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::subscribeToDelta(const WorldLocation& location,
                                             std::function<void(const WorldDelta&)> callback) {
    m_deltaSubscriptions[location] = callback;

    // In production, set up Firestore listener
    // This would use server-sent events or WebSocket
}

void FirebaseWorldStorage::unsubscribeFromDelta(const WorldLocation& location) {
    m_deltaSubscriptions.erase(location);
}

uint64_t FirebaseWorldStorage::getCurrentVersion(const WorldLocation& location) const {
    std::lock_guard<std::mutex> lock(m_versionMutex);
    auto it = m_versionMap.find(location);
    return it != m_versionMap.end() ? it->second : 0;
}

void FirebaseWorldStorage::getVersionHistory(const std::string& worldId, int count, int offset,
                                              HistoryCallback callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"versions\"}],"
          << "\"where\":{"
          << "\"fieldFilter\":{"
          << "\"field\":{\"fieldPath\":\"worldId\"},"
          << "\"op\":\"EQUAL\","
          << "\"value\":{\"stringValue\":\"" << worldId << "\"}"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"timestamp\"},\"direction\":\"DESCENDING\"}],"
          << "\"offset\":" << offset << ","
          << "\"limit\":" << count
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<EditHistoryEntry> history = parseHistoryFromQuery(response.body);
            callback(history, FirebaseError());
        } else {
            callback({}, parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::getVersion(const std::string& worldId, uint64_t version,
    std::function<void(const WorldVersion&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "GET";
    request.url = core.getConfig().getFirestoreUrl() +
                  "/worlds/" + worldId + "/versions/" + std::to_string(version);

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            WorldVersion ver = parseVersion(response.body);
            callback(ver, FirebaseError());
        } else {
            callback(WorldVersion(), parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::rollbackToVersion(const WorldLocation& location, uint64_t version,
                                              WorldSaveCallback callback) {
    // Get the version state
    getVersion(location.worldId, version, [this, location, callback](
        const WorldVersion& ver, const FirebaseError& error) {

        if (error) {
            callback(0, error);
            return;
        }

        // Load the region state at that version
        // This would restore all edits from that version
        // For now, just update the version
        {
            std::lock_guard<std::mutex> lock(m_versionMutex);
            m_versionMap[location] = ver.version;
        }

        callback(ver.version, FirebaseError());
    });
}

void FirebaseWorldStorage::rollbackEdit(const std::string& editId, WorldSaveCallback callback) {
    // Find the edit and create an inverse operation
    deleteEdit(editId, callback);
}

void FirebaseWorldStorage::resolveConflict(const EditConflict& conflict,
                                            EditConflict::Resolution resolution) {
    WorldEdit resolvedEdit;

    switch (resolution) {
        case EditConflict::Resolution::UseLocal:
            resolvedEdit = conflict.localEdit;
            break;
        case EditConflict::Resolution::UseRemote:
            resolvedEdit = conflict.remoteEdit;
            break;
        case EditConflict::Resolution::Merge:
            // Merge properties from both edits
            resolvedEdit = conflict.localEdit;
            for (const auto& [key, value] : conflict.remoteEdit.properties) {
                if (resolvedEdit.properties.find(key) == resolvedEdit.properties.end()) {
                    resolvedEdit.properties[key] = value;
                }
            }
            break;
        case EditConflict::Resolution::Manual:
            // User needs to resolve manually
            return;
    }

    saveEdit(resolvedEdit, nullptr);
}

void FirebaseWorldStorage::setPermission(const std::string& worldId, const std::string& userId,
                                          WorldPermission permission,
                                          std::function<void(const FirebaseError&)> callback) {
    // Check if caller has admin permission
    if (static_cast<int>(getMyPermission(worldId)) < static_cast<int>(WorldPermission::Admin)) {
        FirebaseError error;
        error.type = FirebaseErrorType::PermissionDenied;
        error.message = "No permission to modify access";
        callback(error);
        return;
    }

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/worlds/" + worldId;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream json;
    json << "{\"fields\":{\"permissions." << userId << "\":{\"integerValue\":"
         << static_cast<int>(permission) << "}}}";
    request.body = json.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            callback(FirebaseError());
        } else {
            callback(parseFirestoreError(response));
        }
    });
}

void FirebaseWorldStorage::removePermission(const std::string& worldId, const std::string& userId,
                                             std::function<void(const FirebaseError&)> callback) {
    setPermission(worldId, userId, WorldPermission::None, callback);
}

void FirebaseWorldStorage::getPermissions(const std::string& worldId,
    std::function<void(const std::unordered_map<std::string, WorldPermission>&, const FirebaseError&)> callback) {

    getWorldInfo(worldId, [callback](const WorldInfo& info, const FirebaseError& error) {
        if (error) {
            callback({}, error);
        } else {
            callback(info.permissions, FirebaseError());
        }
    });
}

WorldPermission FirebaseWorldStorage::getMyPermission(const std::string& worldId) const {
    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    auto cacheIt = m_permissionCache.find(worldId + "_" + myId);
    if (cacheIt != m_permissionCache.end()) {
        return cacheIt->second;
    }

    auto infoIt = m_worldInfoCache.find(worldId);
    if (infoIt != m_worldInfoCache.end()) {
        auto permIt = infoIt->second.permissions.find(myId);
        if (permIt != infoIt->second.permissions.end()) {
            return permIt->second;
        }

        // Check if public and allows editing
        if (infoIt->second.isPublic && infoIt->second.allowEditing) {
            return WorldPermission::Edit;
        }
    }

    return WorldPermission::None;
}

void FirebaseWorldStorage::clearLocalCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_regionCache.clear();
    m_worldInfoCache.clear();
}

void FirebaseWorldStorage::preloadRegions(const std::vector<WorldLocation>& locations) {
    loadRegions(locations, [](const std::vector<WorldRegionState>&, const FirebaseError&) {
        // Preload completed silently
    });
}

bool FirebaseWorldStorage::isRegionCached(const WorldLocation& location) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_regionCache.find(location) != m_regionCache.end();
}

const WorldRegionState* FirebaseWorldStorage::getCachedRegion(const WorldLocation& location) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_regionCache.find(location);
    return it != m_regionCache.end() ? &it->second : nullptr;
}

void FirebaseWorldStorage::queueOfflineEdit(const WorldEdit& edit) {
    std::lock_guard<std::mutex> lock(m_offlineQueueMutex);
    m_offlineQueue.push_back(edit);
}

void FirebaseWorldStorage::syncOfflineEdits() {
    if (!FirebaseCore::getInstance().isOnline()) return;

    std::vector<WorldEdit> editsToSync;
    {
        std::lock_guard<std::mutex> lock(m_offlineQueueMutex);
        editsToSync = std::move(m_offlineQueue);
        m_offlineQueue.clear();
    }

    if (!editsToSync.empty()) {
        uploadEdits(editsToSync, nullptr);
    }
}

size_t FirebaseWorldStorage::getPendingEditCount() const {
    std::lock_guard<std::mutex> lock(m_offlineQueueMutex);
    return m_offlineQueue.size() + m_pendingUploads.size();
}

// Private methods

void FirebaseWorldStorage::uploadEdit(const WorldEdit& edit, WorldSaveCallback callback) {
    uploadEdits({edit}, callback);
}

void FirebaseWorldStorage::uploadEdits(const std::vector<WorldEdit>& edits, WorldSaveCallback callback) {
    if (edits.empty()) {
        if (callback) callback(0, FirebaseError());
        return;
    }

    auto& core = FirebaseCore::getInstance();

    // Use batch write for multiple edits
    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":commit";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream json;
    json << "{\"writes\":[";

    bool first = true;
    for (const auto& edit : edits) {
        if (!first) json << ",";
        first = false;

        std::string editJson = serializeEdit(edit);
        if (m_compressionEnabled) {
            editJson = compressData(editJson);
        }

        if (edit.operation == EditOperation::Delete) {
            json << "{\"delete\":\"" << core.getConfig().getFirestoreUrl()
                 << "/edits/" << edit.editId << "\"}";
        } else {
            json << "{\"update\":{\"name\":\"" << core.getConfig().getFirestoreUrl()
                 << "/edits/" << edit.editId << "\",\"fields\":" << editJson << "}}";
        }
    }

    json << "]}";
    request.body = json.str();

    core.makeAuthenticatedRequest(request, [this, edits, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            // Update versions
            for (const auto& edit : edits) {
                recordVersion(edit.location, {edit});
            }

            if (callback) {
                callback(getCurrentVersion(edits[0].location), FirebaseError());
            }
        } else {
            // Queue for retry if offline
            if (!FirebaseCore::getInstance().isOnline()) {
                std::lock_guard<std::mutex> lock(m_offlineQueueMutex);
                m_offlineQueue.insert(m_offlineQueue.end(), edits.begin(), edits.end());
            }

            if (callback) {
                callback(0, parseFirestoreError(response));
            }
        }
    });
}

void FirebaseWorldStorage::downloadRegion(const WorldLocation& location, WorldLoadCallback callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::string regionKey = location.getKey();

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"edits\"}],"
          << "\"where\":{"
          << "\"fieldFilter\":{"
          << "\"field\":{\"fieldPath\":\"regionKey\"},"
          << "\"op\":\"EQUAL\","
          << "\"value\":{\"stringValue\":\"" << regionKey << "\"}"
          << "}"
          << "}"
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [this, location, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            WorldRegionState region;
            region.location = location;
            region.edits = parseEditsFromQuery(response.body);
            region.lastModified = std::chrono::system_clock::now();

            // Calculate version and checksum
            if (!region.edits.empty()) {
                region.version = region.edits.back().version;
            }
            region.checksum = calculateChecksum(region);

            // Cache the region
            cacheRegion(region);

            callback(region, FirebaseError());
        } else {
            callback(WorldRegionState(), parseFirestoreError(response));
        }
    });
}

uint64_t FirebaseWorldStorage::incrementVersion(const WorldLocation& location) {
    std::lock_guard<std::mutex> lock(m_versionMutex);
    return ++m_versionMap[location];
}

void FirebaseWorldStorage::recordVersion(const WorldLocation& location,
                                          const std::vector<WorldEdit>& edits) {
    auto& core = FirebaseCore::getInstance();

    WorldVersion version;
    version.version = incrementVersion(location);
    version.authorId = core.getCurrentUser().uid;
    version.timestamp = std::chrono::system_clock::now();

    for (const auto& edit : edits) {
        version.editIds.push_back(edit.editId);
    }

    // Store version record
    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() +
                  "/worlds/" + location.worldId + "/versions";
    request.headers["Content-Type"] = "application/json";
    request.body = serializeVersion(version);

    core.makeAuthenticatedRequest(request, [](const HttpResponse& response) {
        // Version recorded silently
    });
}

std::vector<EditConflict> FirebaseWorldStorage::detectConflicts(
    const std::vector<WorldEdit>& localEdits, const std::vector<WorldEdit>& remoteEdits) {

    std::vector<EditConflict> conflicts;

    for (const auto& local : localEdits) {
        for (const auto& remote : remoteEdits) {
            if (local.entityId == remote.entityId && local.editId != remote.editId) {
                // Same entity edited by both local and remote
                EditConflict conflict;
                conflict.localEdit = local;
                conflict.remoteEdit = remote;
                conflicts.push_back(conflict);
            }
        }
    }

    return conflicts;
}

std::vector<WorldEdit> FirebaseWorldStorage::mergeEdits(const std::vector<EditConflict>& conflicts) {
    std::vector<WorldEdit> merged;

    for (const auto& conflict : conflicts) {
        if (m_conflictResolver) {
            EditConflict::Resolution resolution = m_conflictResolver(conflict);
            EditConflict resolvedConflict = conflict;
            resolvedConflict.resolution = resolution;

            switch (resolution) {
                case EditConflict::Resolution::UseLocal:
                    merged.push_back(conflict.localEdit);
                    break;
                case EditConflict::Resolution::UseRemote:
                    merged.push_back(conflict.remoteEdit);
                    break;
                case EditConflict::Resolution::Merge:
                    // Use local but merge properties
                    {
                        WorldEdit mergedEdit = conflict.localEdit;
                        for (const auto& [key, value] : conflict.remoteEdit.properties) {
                            mergedEdit.properties[key] = value;
                        }
                        merged.push_back(mergedEdit);
                    }
                    break;
                case EditConflict::Resolution::Manual:
                    m_pendingConflicts.push_back(conflict);
                    break;
            }
        } else {
            // Default: use remote (server wins)
            merged.push_back(conflict.remoteEdit);
        }
    }

    return merged;
}

std::string FirebaseWorldStorage::serializeEdit(const WorldEdit& edit) {
    std::ostringstream json;
    json << "{"
         << "\"editId\":{\"stringValue\":\"" << edit.editId << "\"},"
         << "\"entityId\":{\"stringValue\":\"" << edit.entityId << "\"},"
         << "\"type\":{\"integerValue\":" << static_cast<int>(edit.type) << "},"
         << "\"operation\":{\"integerValue\":" << static_cast<int>(edit.operation) << "},"
         << "\"regionKey\":{\"stringValue\":\"" << edit.location.getKey() << "\"},"
         << "\"posX\":{\"doubleValue\":" << edit.posX << "},"
         << "\"posY\":{\"doubleValue\":" << edit.posY << "},"
         << "\"posZ\":{\"doubleValue\":" << edit.posZ << "},"
         << "\"rotX\":{\"doubleValue\":" << edit.rotX << "},"
         << "\"rotY\":{\"doubleValue\":" << edit.rotY << "},"
         << "\"rotZ\":{\"doubleValue\":" << edit.rotZ << "},"
         << "\"rotW\":{\"doubleValue\":" << edit.rotW << "},"
         << "\"scaleX\":{\"doubleValue\":" << edit.scaleX << "},"
         << "\"scaleY\":{\"doubleValue\":" << edit.scaleY << "},"
         << "\"scaleZ\":{\"doubleValue\":" << edit.scaleZ << "},"
         << "\"authorId\":{\"stringValue\":\"" << edit.authorId << "\"},"
         << "\"version\":{\"integerValue\":" << edit.version << "}"
         << "}";
    return json.str();
}

WorldEdit FirebaseWorldStorage::deserializeEdit(const std::string& json) {
    WorldEdit edit;
    // Parse JSON - simplified
    return edit;
}

std::string FirebaseWorldStorage::serializeRegion(const WorldRegionState& region) {
    std::ostringstream json;
    json << "{\"location\":\"" << region.location.getKey() << "\","
         << "\"version\":" << region.version << ","
         << "\"edits\":[";

    bool first = true;
    for (const auto& edit : region.edits) {
        if (!first) json << ",";
        first = false;
        json << serializeEdit(edit);
    }

    json << "]}";
    return json.str();
}

WorldRegionState FirebaseWorldStorage::deserializeRegion(const std::string& json) {
    WorldRegionState region;
    // Parse JSON - simplified
    return region;
}

std::string FirebaseWorldStorage::compressData(const std::string& data) {
    // Simple RLE compression for demonstration
    // In production, use zlib or similar
    return data;
}

std::string FirebaseWorldStorage::decompressData(const std::string& data) {
    return data;
}

std::string FirebaseWorldStorage::calculateChecksum(const WorldRegionState& region) {
    // Simple checksum - in production use proper hash
    size_t hash = 0;
    for (const auto& edit : region.edits) {
        hash ^= std::hash<std::string>()(edit.editId);
    }
    return std::to_string(hash);
}

bool FirebaseWorldStorage::verifyChecksum(const WorldRegionState& region) {
    return calculateChecksum(region) == region.checksum;
}

void FirebaseWorldStorage::cacheRegion(const WorldRegionState& region) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_regionCache[region.location] = region;

    std::lock_guard<std::mutex> versionLock(m_versionMutex);
    m_versionMap[region.location] = region.version;
}

void FirebaseWorldStorage::invalidateCache(const WorldLocation& location) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_regionCache.erase(location);
}

void FirebaseWorldStorage::saveCacheToDisk() {
    // Serialize cache to disk for persistence
}

void FirebaseWorldStorage::loadCacheFromDisk() {
    // Load cached regions from disk
}

// Static helper functions
namespace {

std::string generateWorldId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id = "world_";
    for (int i = 0; i < 16; i++) {
        id += hex[dis(gen)];
    }
    return id;
}

std::string serializeWorldInfo(const WorldInfo& info) {
    std::ostringstream json;
    json << "{\"fields\":{"
         << "\"worldId\":{\"stringValue\":\"" << info.worldId << "\"},"
         << "\"name\":{\"stringValue\":\"" << info.name << "\"},"
         << "\"description\":{\"stringValue\":\"" << info.description << "\"},"
         << "\"ownerId\":{\"stringValue\":\"" << info.ownerId << "\"},"
         << "\"ownerName\":{\"stringValue\":\"" << info.ownerName << "\"},"
         << "\"isPublic\":{\"booleanValue\":" << (info.isPublic ? "true" : "false") << "},"
         << "\"allowEditing\":{\"booleanValue\":" << (info.allowEditing ? "true" : "false") << "},"
         << "\"currentVersion\":{\"integerValue\":" << info.currentVersion << "}"
         << "}}";
    return json.str();
}

WorldInfo deserializeWorldInfo(const std::string& json) {
    WorldInfo info;
    // Parse JSON - simplified
    return info;
}

std::string serializeVersion(const WorldVersion& version) {
    std::ostringstream json;
    json << "{\"fields\":{"
         << "\"version\":{\"integerValue\":" << version.version << "},"
         << "\"authorId\":{\"stringValue\":\"" << version.authorId << "\"},"
         << "\"description\":{\"stringValue\":\"" << version.description << "\"}"
         << "}}";
    return json.str();
}

WorldVersion parseVersion(const std::string& json) {
    WorldVersion version;
    // Parse JSON - simplified
    return version;
}

std::vector<WorldInfo> parseWorldList(const std::string& json) {
    std::vector<WorldInfo> worlds;
    // Parse JSON array - simplified
    return worlds;
}

std::vector<WorldEdit> parseEditsFromQuery(const std::string& json) {
    std::vector<WorldEdit> edits;
    // Parse JSON array - simplified
    return edits;
}

std::vector<EditHistoryEntry> parseHistoryFromQuery(const std::string& json) {
    std::vector<EditHistoryEntry> history;
    // Parse JSON array - simplified
    return history;
}

FirebaseError parseFirestoreError(const HttpResponse& response) {
    FirebaseError error;
    error.code = response.statusCode;
    error.type = FirebaseErrorType::ServerError;
    error.message = "Firestore request failed";
    return error;
}

} // anonymous namespace

} // namespace Firebase
} // namespace Network
