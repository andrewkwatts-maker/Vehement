#include "PersistenceManager.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

namespace Nova {

PersistenceManager& PersistenceManager::Instance() {
    static PersistenceManager instance;
    return instance;
}

bool PersistenceManager::Initialize(const Config& config) {
    if (m_initialized) {
        spdlog::warn("PersistenceManager already initialized");
        return true;
    }

    m_config = config;
    spdlog::info("Initializing Persistence Manager");

    // Initialize SQLite backend
    if (m_config.enableSQLite) {
        m_sqliteBackend = std::make_unique<SQLiteBackend>();
        if (!m_sqliteBackend->Initialize(m_config.sqliteConfig)) {
            spdlog::error("Failed to initialize SQLite backend");
            return false;
        }

        // Setup callbacks
        m_sqliteBackend->OnAssetChanged = [this](const std::string& id, const nlohmann::json& data) {
            if (OnAssetChanged) OnAssetChanged(id, data);
        };
        m_sqliteBackend->OnAssetDeleted = [this](const std::string& id) {
            if (OnAssetDeleted) OnAssetDeleted(id);
        };

        spdlog::info("SQLite backend initialized");
    }

    // Initialize Firebase backend
    if (m_config.enableFirebase) {
        m_firebaseBackend = std::make_unique<FirebaseBackend>();
        if (!m_firebaseBackend->Initialize(m_config.firebaseConfig)) {
            spdlog::warn("Failed to initialize Firebase backend - continuing without cloud sync");
            m_firebaseBackend.reset();
        } else {
            // Setup callbacks
            m_firebaseBackend->OnAssetChanged = [this](const std::string& id, const nlohmann::json& data) {
                // Update local cache
                {
                    std::lock_guard<std::mutex> lock(m_cacheMutex);
                    m_assetCache[id] = data;
                }
                if (OnAssetChanged) OnAssetChanged(id, data);
            };
            m_firebaseBackend->OnAssetDeleted = [this](const std::string& id) {
                {
                    std::lock_guard<std::mutex> lock(m_cacheMutex);
                    m_assetCache.erase(id);
                }
                if (OnAssetDeleted) OnAssetDeleted(id);
            };
            m_firebaseBackend->OnConflictDetected = [this](const std::string& id) {
                {
                    std::lock_guard<std::mutex> lock(m_conflictMutex);
                    if (std::find(m_conflictedAssets.begin(), m_conflictedAssets.end(), id) == m_conflictedAssets.end()) {
                        m_conflictedAssets.push_back(id);
                    }
                }
                if (OnConflictDetected) OnConflictDetected(id);
            };

            spdlog::info("Firebase backend initialized");
        }
    }

    // Start background sync thread if enabled
    if (m_config.syncInBackground && m_firebaseBackend) {
        m_syncThreadRunning = true;
        m_syncThread = std::make_unique<std::thread>(&PersistenceManager::BackgroundSyncThread, this);
        spdlog::info("Background sync thread started");
    }

    m_initialized = true;
    spdlog::info("Persistence Manager initialized successfully");
    return true;
}

void PersistenceManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    spdlog::info("Shutting down Persistence Manager");

    // Stop background sync thread
    if (m_syncThread && m_syncThreadRunning) {
        m_syncThreadRunning = false;
        if (m_syncThread->joinable()) {
            m_syncThread->join();
        }
        m_syncThread.reset();
    }

    // Final sync before shutdown
    if (m_firebaseBackend) {
        ForceSync();
    }

    // Shutdown backends
    if (m_sqliteBackend) {
        m_sqliteBackend->Shutdown();
        m_sqliteBackend.reset();
    }

    if (m_firebaseBackend) {
        m_firebaseBackend->Shutdown();
        m_firebaseBackend.reset();
    }

    m_initialized = false;
    spdlog::info("Persistence Manager shutdown complete");
}

void PersistenceManager::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update backends
    if (m_sqliteBackend) {
        m_sqliteBackend->Update(deltaTime);
    }

    if (m_firebaseBackend) {
        m_firebaseBackend->Update(deltaTime);
    }

    // Auto-sync if enabled
    if (m_config.autoSync && !m_config.syncInBackground && m_firebaseBackend) {
        m_syncTimer += deltaTime;
        if (m_syncTimer >= m_config.syncInterval) {
            m_syncTimer = 0.0f;
            ForceSync();
        }
    }
}

bool PersistenceManager::SaveAsset(const std::string& id, const nlohmann::json& data,
                                  const AssetMetadata* metadata) {
    if (!m_initialized) {
        spdlog::error("PersistenceManager not initialized");
        return false;
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_assetCache[id] = data;
    }

    bool success = true;

    // Save to SQLite immediately (local persistence)
    if (m_sqliteBackend) {
        success = m_sqliteBackend->SaveAsset(id, data, metadata);
        if (!success) {
            spdlog::error("Failed to save asset to SQLite: {}", id);
        }
    }

    // Queue for Firebase sync
    if (m_firebaseBackend) {
        if (m_firebaseBackend->IsOnline()) {
            m_firebaseBackend->SaveAsset(id, data, metadata);
        } else {
            // Will be synced when connection is restored
            spdlog::debug("Queued asset for Firebase sync: {}", id);
        }
    }

    return success;
}

nlohmann::json PersistenceManager::LoadAsset(const std::string& id) {
    if (!m_initialized) {
        return nlohmann::json();
    }

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        if (m_assetCache.count(id)) {
            return m_assetCache[id];
        }
    }

    // Load from SQLite (local)
    nlohmann::json result;
    if (m_sqliteBackend) {
        result = m_sqliteBackend->LoadAsset(id);
        if (!result.is_null()) {
            // Update cache
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_assetCache[id] = result;
            return result;
        }
    }

    // Fallback to Firebase if not found locally
    if (m_firebaseBackend && m_firebaseBackend->IsOnline()) {
        result = m_firebaseBackend->LoadAsset(id);
        if (!result.is_null()) {
            // Save to local SQLite for caching
            if (m_sqliteBackend) {
                m_sqliteBackend->SaveAsset(id, result);
            }
            // Update cache
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_assetCache[id] = result;
        }
    }

    return result;
}

bool PersistenceManager::DeleteAsset(const std::string& id) {
    if (!m_initialized) {
        return false;
    }

    // Remove from cache
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_assetCache.erase(id);
    }

    bool success = true;

    // Delete from SQLite
    if (m_sqliteBackend) {
        success = m_sqliteBackend->DeleteAsset(id);
    }

    // Delete from Firebase
    if (m_firebaseBackend) {
        m_firebaseBackend->DeleteAsset(id);
    }

    return success;
}

bool PersistenceManager::AssetExists(const std::string& id) {
    if (!m_initialized) {
        return false;
    }

    // Check cache
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        if (m_assetCache.count(id)) {
            return true;
        }
    }

    // Check SQLite
    if (m_sqliteBackend && m_sqliteBackend->AssetExists(id)) {
        return true;
    }

    // Check Firebase
    if (m_firebaseBackend && m_firebaseBackend->IsOnline()) {
        return m_firebaseBackend->AssetExists(id);
    }

    return false;
}

std::vector<std::string> PersistenceManager::ListAssets(const AssetFilter& filter) {
    if (!m_initialized) {
        return {};
    }

    // Use SQLite as primary source
    if (m_sqliteBackend) {
        return m_sqliteBackend->ListAssets(filter);
    }

    // Fallback to Firebase
    if (m_firebaseBackend) {
        return m_firebaseBackend->ListAssets(filter);
    }

    return {};
}

AssetMetadata PersistenceManager::GetMetadata(const std::string& id) {
    if (!m_initialized) {
        return AssetMetadata();
    }

    // Get from SQLite first
    if (m_sqliteBackend) {
        return m_sqliteBackend->GetMetadata(id);
    }

    // Fallback to Firebase
    if (m_firebaseBackend) {
        return m_firebaseBackend->GetMetadata(id);
    }

    return AssetMetadata();
}

nlohmann::json PersistenceManager::GetAssetVersion(const std::string& id, int version) {
    if (m_sqliteBackend) {
        return m_sqliteBackend->GetAssetVersion(id, version);
    }
    return nlohmann::json();
}

std::vector<int> PersistenceManager::GetAssetVersions(const std::string& id) {
    if (m_sqliteBackend) {
        return m_sqliteBackend->GetAssetVersions(id);
    }
    return {};
}

bool PersistenceManager::RevertToVersion(const std::string& id, int version) {
    if (!m_initialized) {
        return false;
    }

    nlohmann::json versionData = GetAssetVersion(id, version);
    if (versionData.is_null()) {
        return false;
    }

    return SaveAsset(id, versionData);
}

std::vector<ChangeEntry> PersistenceManager::GetChangeHistory(const std::string& id, size_t limit) {
    if (m_sqliteBackend) {
        return m_sqliteBackend->GetChangeHistory(id, limit);
    }
    return {};
}

bool PersistenceManager::UndoChange(const std::string& id) {
    if (!m_undoStates.count(id)) {
        return false;
    }

    auto& state = m_undoStates[id];
    if (state.undoStack.empty()) {
        return false;
    }

    // Get last change
    ChangeEntry change = state.undoStack.back();
    state.undoStack.pop_back();

    // Move to redo stack
    state.redoStack.push_back(change);

    // Revert to old data
    if (!change.oldData.is_null()) {
        return SaveAsset(id, change.oldData);
    }

    return false;
}

bool PersistenceManager::RedoChange(const std::string& id) {
    if (!m_undoStates.count(id)) {
        return false;
    }

    auto& state = m_undoStates[id];
    if (state.redoStack.empty()) {
        return false;
    }

    // Get last undone change
    ChangeEntry change = state.redoStack.back();
    state.redoStack.pop_back();

    // Move back to undo stack
    state.undoStack.push_back(change);

    // Apply new data
    if (!change.newData.is_null()) {
        return SaveAsset(id, change.newData);
    }

    return false;
}

bool PersistenceManager::CanUndo(const std::string& id) const {
    return m_undoStates.count(id) && !m_undoStates.at(id).undoStack.empty();
}

bool PersistenceManager::CanRedo(const std::string& id) const {
    return m_undoStates.count(id) && !m_undoStates.at(id).redoStack.empty();
}

void PersistenceManager::ForceSync(std::function<void(bool, const std::string&)> callback) {
    if (!m_firebaseBackend || m_syncInProgress) {
        if (callback) callback(false, "Sync not available or already in progress");
        return;
    }

    spdlog::info("Starting forced sync...");
    m_syncInProgress = true;

    // Sync unsynced changes from SQLite to Firebase
    SyncChangesFromSQLiteToFirebase();

    // Trigger Firebase sync
    m_firebaseBackend->Sync([this, callback](bool success, const std::string& error) {
        m_syncInProgress = false;

        if (success) {
            spdlog::info("Sync completed successfully");
        } else {
            spdlog::error("Sync failed: {}", error);
        }

        if (OnSyncCompleted) {
            OnSyncCompleted(success, error);
        }

        if (callback) {
            callback(success, error);
        }
    });
}

bool PersistenceManager::IsOnline() const {
    return m_firebaseBackend && m_firebaseBackend->IsOnline();
}

SyncStatus PersistenceManager::GetSyncStatus() const {
    SyncStatus status;

    if (m_sqliteBackend) {
        auto sqliteStatus = m_sqliteBackend->GetSyncStatus();
        status.pendingChanges += sqliteStatus.pendingChanges;
        status.syncedChanges += sqliteStatus.syncedChanges;
    }

    if (m_firebaseBackend) {
        auto firebaseStatus = m_firebaseBackend->GetSyncStatus();
        status.online = firebaseStatus.online;
        status.pendingChanges += firebaseStatus.pendingChanges;
        status.syncedChanges += firebaseStatus.syncedChanges;
        status.lastSyncTime = firebaseStatus.lastSyncTime;
        status.lastError = firebaseStatus.lastError;
    }

    return status;
}

size_t PersistenceManager::GetPendingChangeCount() const {
    size_t count = 0;

    if (m_sqliteBackend) {
        count += m_sqliteBackend->GetUnsyncedChanges().size();
    }

    return count;
}

std::vector<std::string> PersistenceManager::GetConflictedAssets() const {
    std::lock_guard<std::mutex> lock(m_conflictMutex);
    return m_conflictedAssets;
}

bool PersistenceManager::HasConflicts(const std::string& id) const {
    if (m_firebaseBackend) {
        return m_firebaseBackend->HasConflicts(id);
    }
    return false;
}

nlohmann::json PersistenceManager::GetConflictData(const std::string& id) {
    if (m_firebaseBackend) {
        return m_firebaseBackend->GetConflictData(id);
    }
    return nlohmann::json();
}

bool PersistenceManager::ResolveConflict(const std::string& id, bool useLocal) {
    if (m_firebaseBackend && m_firebaseBackend->ResolveConflict(id, useLocal)) {
        // Remove from conflict list
        std::lock_guard<std::mutex> lock(m_conflictMutex);
        m_conflictedAssets.erase(
            std::remove(m_conflictedAssets.begin(), m_conflictedAssets.end(), id),
            m_conflictedAssets.end()
        );
        return true;
    }
    return false;
}

void PersistenceManager::AutoResolveConflicts() {
    auto conflicts = GetConflictedAssets();

    for (const auto& id : conflicts) {
        bool useLocal = (m_config.conflictResolution == Config::ConflictResolution::PreferLocal);
        ResolveConflict(id, useLocal);
    }
}

bool PersistenceManager::LockAsset(const std::string& id, float durationSeconds) {
    if (m_firebaseBackend) {
        return m_firebaseBackend->LockAsset(id, durationSeconds);
    }
    return true;  // No locking needed if only local
}

bool PersistenceManager::UnlockAsset(const std::string& id) {
    if (m_firebaseBackend) {
        return m_firebaseBackend->UnlockAsset(id);
    }
    return true;
}

bool PersistenceManager::IsAssetLocked(const std::string& id) const {
    if (m_firebaseBackend) {
        return m_firebaseBackend->IsAssetLocked(id);
    }
    return false;
}

std::string PersistenceManager::GetAssetLockOwner(const std::string& id) const {
    if (m_firebaseBackend) {
        return m_firebaseBackend->GetAssetLockOwner(id);
    }
    return "";
}

void PersistenceManager::SetConfig(const Config& config) {
    m_config = config;
}

void PersistenceManager::SetAutoSync(bool enabled) {
    m_config.autoSync = enabled;
}

void PersistenceManager::SetSyncInterval(float seconds) {
    m_config.syncInterval = seconds;
}

void PersistenceManager::SyncChangesFromSQLiteToFirebase() {
    if (!m_sqliteBackend || !m_firebaseBackend) {
        return;
    }

    auto unsyncedChanges = m_sqliteBackend->GetUnsyncedChanges();
    if (unsyncedChanges.empty()) {
        return;
    }

    spdlog::info("Syncing {} unsynced changes to Firebase", unsyncedChanges.size());

    std::vector<uint64_t> syncedIds;

    for (const auto& change : unsyncedChanges) {
        // Apply change to Firebase
        switch (change.changeType) {
            case ChangeEntry::Type::Create:
            case ChangeEntry::Type::Update:
                if (!change.newData.is_null()) {
                    m_firebaseBackend->SaveAsset(change.assetId, change.newData);
                    syncedIds.push_back(change.id);
                }
                break;

            case ChangeEntry::Type::Delete:
                m_firebaseBackend->DeleteAsset(change.assetId);
                syncedIds.push_back(change.id);
                break;
        }
    }

    // Mark changes as synced
    if (!syncedIds.empty()) {
        m_sqliteBackend->MarkChangesSynced(syncedIds);
    }
}

void PersistenceManager::BackgroundSyncThread() {
    spdlog::info("Background sync thread running");

    while (m_syncThreadRunning) {
        if (m_config.autoSync && IsOnline() && !m_syncInProgress) {
            SyncChangesFromSQLiteToFirebase();
        }

        // Sleep for sync interval
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(m_config.syncInterval * 1000))
        );
    }

    spdlog::info("Background sync thread stopped");
}

} // namespace Nova
