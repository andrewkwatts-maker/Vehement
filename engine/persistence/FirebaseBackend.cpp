#include "FirebaseBackend.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

namespace Nova {

namespace {
    uint64_t GetCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
}

FirebaseBackend::FirebaseBackend()
    : m_firebase(std::make_shared<FirebaseClient>())
    , m_ownFirebaseClient(true) {
}

FirebaseBackend::FirebaseBackend(std::shared_ptr<FirebaseClient> firebaseClient)
    : m_firebase(firebaseClient)
    , m_ownFirebaseClient(false) {
}

FirebaseBackend::~FirebaseBackend() {
    Shutdown();
}

bool FirebaseBackend::Initialize(const nlohmann::json& config) {
    if (m_initialized) {
        spdlog::warn("FirebaseBackend already initialized");
        return true;
    }

    // Parse configuration
    if (config.contains("project_id")) {
        m_config.projectId = config["project_id"].get<std::string>();
    }
    if (config.contains("api_key")) {
        m_config.apiKey = config["api_key"].get<std::string>();
    }
    if (config.contains("database_url")) {
        m_config.databaseUrl = config["database_url"].get<std::string>();
    }
    if (config.contains("sync_interval")) {
        m_config.syncInterval = config["sync_interval"].get<float>();
    }
    if (config.contains("auto_sync")) {
        m_config.autoSync = config["auto_sync"].get<bool>();
    }

    spdlog::info("Initializing Firebase backend: {}", m_config.projectId);

    // Initialize Firebase client if we own it
    if (m_ownFirebaseClient) {
        FirebaseClient::Config fbConfig;
        fbConfig.projectId = m_config.projectId;
        fbConfig.apiKey = m_config.apiKey;
        fbConfig.databaseUrl = m_config.databaseUrl;
        fbConfig.authDomain = m_config.authDomain;

        if (!m_firebase->Initialize(fbConfig)) {
            spdlog::error("Failed to initialize Firebase client");
            return false;
        }
    }

    // Auto sign-in if configured
    if (m_config.requireAuth) {
        if (m_config.allowAnonymous) {
            SignInAnonymously([](bool success, const std::string& error) {
                if (success) {
                    spdlog::info("Signed in to Firebase anonymously");
                } else {
                    spdlog::error("Failed to sign in to Firebase: {}", error);
                }
            });
        }
    }

    m_initialized = true;
    m_syncStatus.online = true;
    spdlog::info("Firebase backend initialized successfully");
    return true;
}

void FirebaseBackend::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Process any remaining queued operations
    ProcessQueue();

    // Unsubscribe from all listeners
    for (const auto& [subId, listenerId] : m_subscriptions) {
        m_firebase->RemoveListener(listenerId);
    }
    m_subscriptions.clear();

    // Shutdown Firebase client if we own it
    if (m_ownFirebaseClient && m_firebase) {
        m_firebase->Shutdown();
    }

    m_initialized = false;
    spdlog::info("Firebase backend shutdown");
}

void FirebaseBackend::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update Firebase client
    m_firebase->Update(deltaTime);

    // Update online status
    m_syncStatus.online = m_firebase->IsOnline();

    // Process operation queue
    ProcessQueue();

    // Auto-sync if enabled
    if (m_config.autoSync) {
        m_syncTimer += deltaTime;
        if (m_syncTimer >= m_config.syncInterval) {
            m_syncTimer = 0.0f;
            Sync();
        }
    }

    // Clean up expired locks
    {
        std::lock_guard<std::mutex> lock(m_lockMutex);
        uint64_t now = GetCurrentTimestamp();
        for (auto it = m_assetLocks.begin(); it != m_assetLocks.end();) {
            if (it->second.expiresAt < now) {
                it = m_assetLocks.erase(it);
            } else {
                ++it;
            }
        }
    }
}

bool FirebaseBackend::SaveAsset(const std::string& id, const nlohmann::json& data,
                               const AssetMetadata* metadata) {
    if (!m_initialized) {
        spdlog::error("FirebaseBackend not initialized");
        return false;
    }

    // Build metadata
    AssetMetadata meta;
    if (metadata) {
        meta = *metadata;
    } else {
        meta.id = id;
        meta.type = data.contains("type") ? data["type"].get<std::string>() : "unknown";
        meta.userId = GetCurrentUserId();
    }

    uint64_t timestamp = GetCurrentTimestamp();
    if (meta.createdAt == 0) {
        meta.createdAt = timestamp;
    }
    meta.modifiedAt = timestamp;
    meta.version++;

    // Save asset data
    nlohmann::json assetData;
    assetData["data"] = data;
    assetData["metadata"] = SerializeMetadata(meta);

    // Queue operation if offline
    if (!IsOnline()) {
        QueuedOperation op;
        op.type = QueuedOperation::Type::Save;
        op.assetId = id;
        op.data = data;
        op.metadata = meta;
        QueueOperation(op);
        return true;
    }

    // Save to Firebase
    m_firebase->Set(GetAssetPath(id), assetData, [this, id, data](const FirebaseResult& result) {
        if (result.success) {
            spdlog::debug("Asset saved to Firebase: {}", id);
            if (OnAssetChanged) {
                OnAssetChanged(id, data);
            }
        } else {
            spdlog::error("Failed to save asset to Firebase: {}", result.errorMessage);
        }
    });

    // Record change
    RecordChange(id, ChangeEntry::Type::Update, nlohmann::json(), data);

    return true;
}

nlohmann::json FirebaseBackend::LoadAsset(const std::string& id) {
    if (!m_initialized) {
        return nlohmann::json();
    }

    // Synchronous load - in production, this would be async
    nlohmann::json result;
    bool loaded = false;

    m_firebase->Get(GetAssetPath(id), [&result, &loaded](const FirebaseResult& fbResult) {
        if (fbResult.success && !fbResult.data.is_null()) {
            if (fbResult.data.contains("data")) {
                result = fbResult.data["data"];
            }
        }
        loaded = true;
    });

    // Wait for callback (simplified - in production use proper async handling)
    // For now, return empty if not loaded
    return result;
}

bool FirebaseBackend::DeleteAsset(const std::string& id) {
    if (!m_initialized) {
        return false;
    }

    // Get old data for change tracking
    nlohmann::json oldData = LoadAsset(id);

    // Queue operation if offline
    if (!IsOnline()) {
        QueuedOperation op;
        op.type = QueuedOperation::Type::Delete;
        op.assetId = id;
        QueueOperation(op);
        return true;
    }

    // Delete from Firebase
    m_firebase->Delete(GetAssetPath(id), [this, id](const FirebaseResult& result) {
        if (result.success) {
            spdlog::debug("Asset deleted from Firebase: {}", id);
            if (OnAssetDeleted) {
                OnAssetDeleted(id);
            }
        } else {
            spdlog::error("Failed to delete asset from Firebase: {}", result.errorMessage);
        }
    });

    // Record change
    RecordChange(id, ChangeEntry::Type::Delete, oldData, nlohmann::json());

    return true;
}

bool FirebaseBackend::AssetExists(const std::string& id) {
    // Simplified check - in production, implement proper async check
    return !LoadAsset(id).is_null();
}

std::vector<std::string> FirebaseBackend::ListAssets(const AssetFilter& filter) {
    std::vector<std::string> assets;

    // Query Firebase for assets
    std::string path = m_config.assetsPath;

    m_firebase->Get(path, [&assets, &filter](const FirebaseResult& result) {
        if (result.success && result.data.is_object()) {
            for (auto& [key, value] : result.data.items()) {
                // Apply filter
                if (!filter.type.empty() && value.contains("metadata")) {
                    auto meta = value["metadata"];
                    if (meta.contains("type") && meta["type"] != filter.type) {
                        continue;
                    }
                }
                assets.push_back(key);
            }
        }
    });

    return assets;
}

AssetMetadata FirebaseBackend::GetMetadata(const std::string& id) {
    AssetMetadata metadata;

    m_firebase->Get(GetAssetMetadataPath(id), [&metadata, this](const FirebaseResult& result) {
        if (result.success && !result.data.is_null()) {
            metadata = DeserializeMetadata(result.data);
        }
    });

    return metadata;
}

nlohmann::json FirebaseBackend::GetAssetVersion(const std::string& id, int version) {
    // Load version from Firebase versions path
    nlohmann::json result;

    m_firebase->Get(GetAssetVersionPath(id, version), [&result](const FirebaseResult& fbResult) {
        if (fbResult.success && !fbResult.data.is_null()) {
            result = fbResult.data;
        }
    });

    return result;
}

std::vector<int> FirebaseBackend::GetAssetVersions(const std::string& id) {
    std::vector<int> versions;
    // Query Firebase for all versions
    // Simplified implementation
    return versions;
}

bool FirebaseBackend::RevertToVersion(const std::string& id, int version) {
    nlohmann::json versionData = GetAssetVersion(id, version);
    if (versionData.is_null()) {
        return false;
    }
    return SaveAsset(id, versionData);
}

std::vector<ChangeEntry> FirebaseBackend::GetChangeHistory(const std::string& id, size_t limit) {
    std::lock_guard<std::mutex> lock(m_changeMutex);

    std::vector<ChangeEntry> history;
    if (m_changeHistory.count(id)) {
        history = m_changeHistory[id];
        if (limit > 0 && history.size() > limit) {
            history.resize(limit);
        }
    }

    return history;
}

std::vector<ChangeEntry> FirebaseBackend::GetUnsyncedChanges() {
    std::lock_guard<std::mutex> lock(m_changeMutex);
    return m_unsyncedChanges;
}

bool FirebaseBackend::MarkChangesSynced(const std::vector<uint64_t>& changeIds) {
    std::lock_guard<std::mutex> lock(m_changeMutex);

    for (uint64_t id : changeIds) {
        auto it = std::find_if(m_unsyncedChanges.begin(), m_unsyncedChanges.end(),
                              [id](const ChangeEntry& entry) { return entry.id == id; });
        if (it != m_unsyncedChanges.end()) {
            it->synced = true;
        }
    }

    // Remove synced changes
    m_unsyncedChanges.erase(
        std::remove_if(m_unsyncedChanges.begin(), m_unsyncedChanges.end(),
                      [](const ChangeEntry& entry) { return entry.synced; }),
        m_unsyncedChanges.end()
    );

    m_syncStatus.syncedChanges += changeIds.size();
    m_syncStatus.pendingChanges = m_unsyncedChanges.size();

    return true;
}

bool FirebaseBackend::IsOnline() const {
    return m_firebase && m_firebase->IsOnline();
}

void FirebaseBackend::Sync(std::function<void(bool, const std::string&)> callback) {
    if (!m_initialized) {
        if (callback) callback(false, "Backend not initialized");
        return;
    }

    if (!IsOnline()) {
        if (callback) callback(false, "Offline");
        return;
    }

    spdlog::info("Starting Firebase sync...");
    m_syncStatus.lastSyncTime = GetCurrentTimestamp();

    // Process queued operations
    ProcessQueue();

    // Sync pending changes
    m_firebase->SyncOfflineOperations();

    if (callback) {
        callback(true, "Sync completed");
    }

    if (OnSyncCompleted) {
        OnSyncCompleted(true, "");
    }
}

SyncStatus FirebaseBackend::GetSyncStatus() const {
    std::lock_guard<std::mutex> lock(m_syncMutex);
    return m_syncStatus;
}

bool FirebaseBackend::BeginTransaction() {
    m_transaction.active = true;
    m_transaction.operations.clear();
    return true;
}

bool FirebaseBackend::CommitTransaction() {
    if (!m_transaction.active) {
        return false;
    }

    // Execute all operations
    for (auto& op : m_transaction.operations) {
        QueueOperation(op);
    }

    m_transaction.active = false;
    m_transaction.operations.clear();
    return true;
}

bool FirebaseBackend::RollbackTransaction() {
    if (!m_transaction.active) {
        return false;
    }

    m_transaction.active = false;
    m_transaction.operations.clear();
    return true;
}

bool FirebaseBackend::HasConflicts(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_conflictMutex);
    return m_conflicts.count(id) > 0;
}

nlohmann::json FirebaseBackend::GetConflictData(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_conflictMutex);

    if (m_conflicts.count(id)) {
        return m_conflicts[id];
    }

    return nlohmann::json();
}

bool FirebaseBackend::ResolveConflict(const std::string& id, bool useLocal) {
    std::lock_guard<std::mutex> lock(m_conflictMutex);

    if (!m_conflicts.count(id)) {
        return false;
    }

    auto conflictData = m_conflicts[id];
    nlohmann::json resolvedData = useLocal ? conflictData["local"] : conflictData["remote"];

    // Save resolved version
    SaveAsset(id, resolvedData);

    // Clear conflict
    m_conflicts.erase(id);

    return true;
}

void FirebaseBackend::SignIn(const std::string& email, const std::string& password,
                             std::function<void(bool, const std::string&)> callback) {
    m_firebase->SignInWithEmail(email, password, callback);
}

void FirebaseBackend::SignInAnonymously(std::function<void(bool, const std::string&)> callback) {
    m_firebase->SignInAnonymously(callback);
}

void FirebaseBackend::SignOut() {
    m_firebase->SignOut();
}

std::string FirebaseBackend::GetCurrentUserId() const {
    return m_firebase ? m_firebase->GetUserId() : "local";
}

uint64_t FirebaseBackend::SubscribeToAssetChanges(const std::string& id,
                                                  std::function<void(const nlohmann::json&)> callback) {
    uint64_t firebaseListenerId = m_firebase->AddValueListener(GetAssetPath(id),
        [callback](const nlohmann::json& data) {
            if (data.contains("data")) {
                callback(data["data"]);
            }
        });

    uint64_t subId = m_nextSubscriptionId++;
    m_subscriptions[subId] = firebaseListenerId;
    return subId;
}

void FirebaseBackend::UnsubscribeFromAssetChanges(uint64_t subscriptionId) {
    if (m_subscriptions.count(subscriptionId)) {
        m_firebase->RemoveListener(m_subscriptions[subscriptionId]);
        m_subscriptions.erase(subscriptionId);
    }
}

bool FirebaseBackend::LockAsset(const std::string& id, float lockDurationSeconds) {
    std::lock_guard<std::mutex> lock(m_lockMutex);

    uint64_t now = GetCurrentTimestamp();
    uint64_t expiresAt = now + static_cast<uint64_t>(lockDurationSeconds * 1000);

    AssetLock assetLock;
    assetLock.userId = GetCurrentUserId();
    assetLock.expiresAt = expiresAt;

    m_assetLocks[id] = assetLock;

    // Save lock to Firebase
    nlohmann::json lockData;
    lockData["userId"] = assetLock.userId;
    lockData["expiresAt"] = assetLock.expiresAt;
    m_firebase->Set(GetLockPath(id), lockData);

    return true;
}

bool FirebaseBackend::UnlockAsset(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_lockMutex);

    m_assetLocks.erase(id);
    m_firebase->Delete(GetLockPath(id));

    return true;
}

bool FirebaseBackend::IsAssetLocked(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_lockMutex);

    if (!m_assetLocks.count(id)) {
        return false;
    }

    uint64_t now = GetCurrentTimestamp();
    return m_assetLocks.at(id).expiresAt > now;
}

std::string FirebaseBackend::GetAssetLockOwner(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_lockMutex);

    if (m_assetLocks.count(id)) {
        return m_assetLocks.at(id).userId;
    }

    return "";
}

std::string FirebaseBackend::GetAssetPath(const std::string& id) const {
    return m_config.assetsPath + "/" + id;
}

std::string FirebaseBackend::GetAssetMetadataPath(const std::string& id) const {
    return GetAssetPath(id) + "/metadata";
}

std::string FirebaseBackend::GetAssetVersionPath(const std::string& id, int version) const {
    return GetAssetPath(id) + "/versions/" + std::to_string(version);
}

std::string FirebaseBackend::GetChangePath(uint64_t changeId) const {
    return m_config.changesPath + "/" + std::to_string(changeId);
}

std::string FirebaseBackend::GetLockPath(const std::string& id) const {
    return "locks/" + id;
}

nlohmann::json FirebaseBackend::SerializeMetadata(const AssetMetadata& metadata) const {
    nlohmann::json json;
    json["id"] = metadata.id;
    json["type"] = metadata.type;
    json["version"] = metadata.version;
    json["createdAt"] = metadata.createdAt;
    json["modifiedAt"] = metadata.modifiedAt;
    json["checksum"] = metadata.checksum;
    json["userId"] = metadata.userId;
    json["customData"] = metadata.customData;
    return json;
}

AssetMetadata FirebaseBackend::DeserializeMetadata(const nlohmann::json& json) const {
    AssetMetadata metadata;
    if (json.contains("id")) metadata.id = json["id"];
    if (json.contains("type")) metadata.type = json["type"];
    if (json.contains("version")) metadata.version = json["version"];
    if (json.contains("createdAt")) metadata.createdAt = json["createdAt"];
    if (json.contains("modifiedAt")) metadata.modifiedAt = json["modifiedAt"];
    if (json.contains("checksum")) metadata.checksum = json["checksum"];
    if (json.contains("userId")) metadata.userId = json["userId"];
    if (json.contains("customData")) metadata.customData = json["customData"];
    return metadata;
}

nlohmann::json FirebaseBackend::SerializeChange(const ChangeEntry& change) const {
    nlohmann::json json;
    json["id"] = change.id;
    json["assetId"] = change.assetId;
    json["changeType"] = static_cast<int>(change.changeType);
    json["oldData"] = change.oldData;
    json["newData"] = change.newData;
    json["timestamp"] = change.timestamp;
    json["synced"] = change.synced;
    json["userId"] = change.userId;
    return json;
}

ChangeEntry FirebaseBackend::DeserializeChange(const nlohmann::json& json) const {
    ChangeEntry change;
    if (json.contains("id")) change.id = json["id"];
    if (json.contains("assetId")) change.assetId = json["assetId"];
    if (json.contains("changeType")) change.changeType = static_cast<ChangeEntry::Type>(json["changeType"].get<int>());
    if (json.contains("oldData")) change.oldData = json["oldData"];
    if (json.contains("newData")) change.newData = json["newData"];
    if (json.contains("timestamp")) change.timestamp = json["timestamp"];
    if (json.contains("synced")) change.synced = json["synced"];
    if (json.contains("userId")) change.userId = json["userId"];
    return change;
}

void FirebaseBackend::QueueOperation(const QueuedOperation& op) {
    if (m_transaction.active) {
        m_transaction.operations.push_back(op);
    } else {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_operationQueue.push(op);
    }
}

void FirebaseBackend::ProcessQueue() {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    while (!m_operationQueue.empty() && IsOnline()) {
        QueuedOperation op = m_operationQueue.front();
        m_operationQueue.pop();

        switch (op.type) {
            case QueuedOperation::Type::Save:
                ProcessSaveOperation(op);
                break;
            case QueuedOperation::Type::Delete:
                ProcessDeleteOperation(op);
                break;
            case QueuedOperation::Type::MarkSynced:
                MarkChangesSynced(op.changeIds);
                break;
        }
    }
}

void FirebaseBackend::ProcessSaveOperation(QueuedOperation& op) {
    SaveAsset(op.assetId, op.data, &op.metadata);
}

void FirebaseBackend::ProcessDeleteOperation(QueuedOperation& op) {
    DeleteAsset(op.assetId);
}

uint64_t FirebaseBackend::GenerateChangeId() const {
    return GetCurrentTimestamp();
}

void FirebaseBackend::RecordChange(const std::string& assetId, ChangeEntry::Type type,
                                   const nlohmann::json& oldData, const nlohmann::json& newData) {
    std::lock_guard<std::mutex> lock(m_changeMutex);

    ChangeEntry change;
    change.id = m_nextChangeId++;
    change.assetId = assetId;
    change.changeType = type;
    change.oldData = oldData;
    change.newData = newData;
    change.timestamp = GetCurrentTimestamp();
    change.synced = false;
    change.userId = GetCurrentUserId();

    m_changeHistory[assetId].push_back(change);
    m_unsyncedChanges.push_back(change);

    ++m_syncStatus.pendingChanges;
}

} // namespace Nova
