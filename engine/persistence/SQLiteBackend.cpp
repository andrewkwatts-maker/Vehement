#include "SQLiteBackend.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace Nova {

namespace {
    // Database schema version
    constexpr int SCHEMA_VERSION = 1;

    // SQL statements
    const char* SQL_CREATE_ASSETS = R"(
        CREATE TABLE IF NOT EXISTS assets (
            id TEXT PRIMARY KEY,
            type TEXT NOT NULL,
            data TEXT NOT NULL,
            version INTEGER DEFAULT 1,
            created_at INTEGER NOT NULL,
            modified_at INTEGER NOT NULL,
            checksum TEXT NOT NULL,
            user_id TEXT,
            custom_data TEXT
        );
    )";

    const char* SQL_CREATE_ASSET_VERSIONS = R"(
        CREATE TABLE IF NOT EXISTS asset_versions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            asset_id TEXT NOT NULL,
            version INTEGER NOT NULL,
            data TEXT NOT NULL,
            checksum TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            user_id TEXT,
            FOREIGN KEY(asset_id) REFERENCES assets(id) ON DELETE CASCADE,
            UNIQUE(asset_id, version)
        );
    )";

    const char* SQL_CREATE_CHANGES = R"(
        CREATE TABLE IF NOT EXISTS changes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            asset_id TEXT NOT NULL,
            change_type TEXT NOT NULL,
            old_data TEXT,
            new_data TEXT,
            timestamp INTEGER NOT NULL,
            synced INTEGER DEFAULT 0,
            user_id TEXT,
            FOREIGN KEY(asset_id) REFERENCES assets(id) ON DELETE CASCADE
        );
    )";

    const char* SQL_CREATE_METADATA = R"(
        CREATE TABLE IF NOT EXISTS metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )";

    const char* SQL_CREATE_INDICES = R"(
        CREATE INDEX IF NOT EXISTS idx_assets_type ON assets(type);
        CREATE INDEX IF NOT EXISTS idx_assets_modified ON assets(modified_at);
        CREATE INDEX IF NOT EXISTS idx_changes_asset ON changes(asset_id);
        CREATE INDEX IF NOT EXISTS idx_changes_synced ON changes(synced);
        CREATE INDEX IF NOT EXISTS idx_versions_asset ON asset_versions(asset_id);
    )";

    const char* SQL_CREATE_FTS = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS assets_fts USING fts5(
            id,
            type,
            data,
            content='assets',
            content_rowid='rowid'
        );
    )";
}

SQLiteBackend::SQLiteBackend() = default;

SQLiteBackend::~SQLiteBackend() {
    Shutdown();
}

bool SQLiteBackend::Initialize(const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (m_initialized) {
        spdlog::warn("SQLiteBackend already initialized");
        return true;
    }

    // Parse configuration
    if (config.contains("database_path")) {
        m_config.databasePath = config["database_path"].get<std::string>();
    }
    if (config.contains("enable_wal")) {
        m_config.enableWAL = config["enable_wal"].get<bool>();
    }
    if (config.contains("enable_fts")) {
        m_config.enableFTS = config["enable_fts"].get<bool>();
    }
    if (config.contains("cache_size")) {
        m_config.cacheSize = config["cache_size"].get<int>();
    }
    if (config.contains("max_versions_per_asset")) {
        m_config.maxVersionsPerAsset = config["max_versions_per_asset"].get<int>();
    }

    spdlog::info("Initializing SQLite backend: {}", m_config.databasePath);

    // Open database
    if (!OpenDatabase()) {
        return false;
    }

    // Create schema
    if (!CreateSchema()) {
        CloseDatabase();
        return false;
    }

    m_initialized = true;
    spdlog::info("SQLite backend initialized successfully");
    return true;
}

void SQLiteBackend::Shutdown() {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized) {
        return;
    }

    // Finalize all prepared statements
    for (auto& [sql, stmt] : m_preparedStatements) {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
    m_preparedStatements.clear();

    CloseDatabase();
    m_initialized = false;
    spdlog::info("SQLite backend shutdown");
}

void SQLiteBackend::Update(float deltaTime) {
    // SQLite is synchronous, no async operations to update
}

bool SQLiteBackend::OpenDatabase() {
    // Create parent directory if it doesn't exist
    std::filesystem::path dbPath(m_config.databasePath);
    if (dbPath.has_parent_path()) {
        std::filesystem::create_directories(dbPath.parent_path());
    }

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    int result = sqlite3_open_v2(m_config.databasePath.c_str(), &m_db, flags, nullptr);

    if (result != SQLITE_OK) {
        spdlog::error("Failed to open SQLite database: {}", sqlite3_errmsg(m_db));
        return false;
    }

    // Set pragmas for performance and reliability
    if (m_config.enableWAL) {
        ExecuteStatement("PRAGMA journal_mode=WAL;");
    }
    if (m_config.foreignKeys) {
        ExecuteStatement("PRAGMA foreign_keys=ON;");
    }
    if (m_config.autoVacuum) {
        ExecuteStatement("PRAGMA auto_vacuum=INCREMENTAL;");
    }

    ExecuteStatement("PRAGMA cache_size=" + std::to_string(-m_config.cacheSize) + ";");
    ExecuteStatement("PRAGMA busy_timeout=" + std::to_string(m_config.busyTimeout) + ";");
    ExecuteStatement("PRAGMA synchronous=NORMAL;");  // Balance between safety and performance

    return true;
}

void SQLiteBackend::CloseDatabase() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool SQLiteBackend::CreateSchema() {
    // Check if schema exists
    auto result = ExecuteQuery("SELECT value FROM metadata WHERE key='schema_version';");
    int currentVersion = 0;
    if (!result.empty() && result.is_array() && !result[0].empty()) {
        currentVersion = std::stoi(result[0]["value"].get<std::string>());
    }

    if (currentVersion == SCHEMA_VERSION) {
        return true;  // Schema up to date
    }

    if (currentVersion > SCHEMA_VERSION) {
        spdlog::error("Database schema version {} is newer than supported version {}",
                     currentVersion, SCHEMA_VERSION);
        return false;
    }

    // Create or migrate schema
    if (currentVersion == 0) {
        spdlog::info("Creating database schema version {}", SCHEMA_VERSION);

        if (!ExecuteStatement(SQL_CREATE_ASSETS)) return false;
        if (!ExecuteStatement(SQL_CREATE_ASSET_VERSIONS)) return false;
        if (!ExecuteStatement(SQL_CREATE_CHANGES)) return false;
        if (!ExecuteStatement(SQL_CREATE_METADATA)) return false;
        if (!ExecuteStatement(SQL_CREATE_INDICES)) return false;

        if (m_config.enableFTS) {
            if (!ExecuteStatement(SQL_CREATE_FTS)) return false;
        }

        // Store schema version
        ExecuteStatement("INSERT OR REPLACE INTO metadata (key, value) VALUES ('schema_version', '" +
                        std::to_string(SCHEMA_VERSION) + "');");
    } else {
        // Migrate from older version
        return MigrateSchema(currentVersion);
    }

    return true;
}

bool SQLiteBackend::MigrateSchema(int currentVersion) {
    spdlog::info("Migrating database schema from version {} to {}", currentVersion, SCHEMA_VERSION);
    // Add migration logic here when schema changes
    return true;
}

bool SQLiteBackend::SaveAsset(const std::string& id, const nlohmann::json& data,
                             const AssetMetadata* metadata) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized) {
        spdlog::error("SQLiteBackend not initialized");
        return false;
    }

    // Get existing asset data for change tracking
    nlohmann::json oldData;
    bool isUpdate = AssetExists(id);
    if (isUpdate) {
        oldData = LoadAsset(id);
    }

    // Build metadata
    AssetMetadata meta;
    if (metadata) {
        meta = *metadata;
    } else {
        meta.id = id;
        meta.type = data.contains("type") ? data["type"].get<std::string>() : "unknown";
        meta.userId = "local";  // Default user
    }

    uint64_t timestamp = GetCurrentTimestamp();
    if (!isUpdate) {
        meta.createdAt = timestamp;
    }
    meta.modifiedAt = timestamp;
    meta.checksum = CalculateChecksum(data);

    std::string dataStr = SerializeJson(data);
    std::string customDataStr = SerializeJson(meta.customData);

    // Update or insert asset
    const char* sql = isUpdate ?
        "UPDATE assets SET type=?, data=?, version=version+1, modified_at=?, checksum=?, user_id=?, custom_data=? WHERE id=?" :
        "INSERT INTO assets (type, data, version, created_at, modified_at, checksum, user_id, custom_data, id) VALUES (?, ?, 1, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) return false;

    if (isUpdate) {
        sqlite3_bind_text(stmt, 1, meta.type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, dataStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, meta.modifiedAt);
        sqlite3_bind_text(stmt, 4, meta.checksum.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, meta.userId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, customDataStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, id.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(stmt, 1, meta.type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, dataStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, meta.createdAt);
        sqlite3_bind_int64(stmt, 4, meta.modifiedAt);
        sqlite3_bind_text(stmt, 5, meta.checksum.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, meta.userId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, customDataStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, id.c_str(), -1, SQLITE_TRANSIENT);
    }

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        spdlog::error("Failed to save asset {}: {}", id, sqlite3_errmsg(m_db));
        return false;
    }

    // Save version
    SaveAssetVersion(id, data, meta);

    // Record change
    RecordChange(id, isUpdate ? ChangeEntry::Type::Update : ChangeEntry::Type::Create,
                oldData, data);

    // Prune old versions if needed
    if (m_config.maxVersionsPerAsset > 0) {
        PruneOldVersions(id);
    }

    // Trigger callback
    if (OnAssetChanged) {
        OnAssetChanged(id, data);
    }

    return true;
}

nlohmann::json SQLiteBackend::LoadAsset(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized) {
        return nlohmann::json();
    }

    sqlite3_stmt* stmt = PrepareStatement("SELECT data FROM assets WHERE id=?");
    if (!stmt) return nlohmann::json();

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    nlohmann::json result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* dataStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (dataStr) {
            result = DeserializeJson(dataStr);
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool SQLiteBackend::DeleteAsset(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized) {
        return false;
    }

    // Get old data for change tracking
    nlohmann::json oldData = LoadAsset(id);
    if (oldData.is_null()) {
        return false;  // Asset doesn't exist
    }

    sqlite3_stmt* stmt = PrepareStatement("DELETE FROM assets WHERE id=?");
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        spdlog::error("Failed to delete asset {}: {}", id, sqlite3_errmsg(m_db));
        return false;
    }

    // Record change
    RecordChange(id, ChangeEntry::Type::Delete, oldData, nlohmann::json());

    // Trigger callback
    if (OnAssetDeleted) {
        OnAssetDeleted(id);
    }

    return true;
}

bool SQLiteBackend::AssetExists(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized) {
        return false;
    }

    sqlite3_stmt* stmt = PrepareStatement("SELECT 1 FROM assets WHERE id=? LIMIT 1");
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return exists;
}

std::vector<std::string> SQLiteBackend::ListAssets(const AssetFilter& filter) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    std::vector<std::string> assets;
    if (!m_initialized) {
        return assets;
    }

    // Build query with filters
    std::string sql = "SELECT id FROM assets WHERE 1=1";
    std::vector<std::string> params;

    if (!filter.type.empty()) {
        sql += " AND type=?";
        params.push_back(filter.type);
    }
    if (filter.modifiedAfter > 0) {
        sql += " AND modified_at >= ?";
        params.push_back(std::to_string(filter.modifiedAfter));
    }
    if (filter.modifiedBefore > 0) {
        sql += " AND modified_at <= ?";
        params.push_back(std::to_string(filter.modifiedBefore));
    }
    if (!filter.userId.empty()) {
        sql += " AND user_id=?";
        params.push_back(filter.userId);
    }

    auto result = ExecuteQuery(sql, params);
    if (result.is_array()) {
        for (const auto& row : result) {
            if (row.contains("id")) {
                assets.push_back(row["id"].get<std::string>());
            }
        }
    }

    return assets;
}

AssetMetadata SQLiteBackend::GetMetadata(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    AssetMetadata metadata;
    if (!m_initialized) {
        return metadata;
    }

    sqlite3_stmt* stmt = PrepareStatement(
        "SELECT type, version, created_at, modified_at, checksum, user_id, custom_data FROM assets WHERE id=?");
    if (!stmt) return metadata;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        metadata.id = id;
        metadata.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        metadata.version = sqlite3_column_int(stmt, 1);
        metadata.createdAt = sqlite3_column_int64(stmt, 2);
        metadata.modifiedAt = sqlite3_column_int64(stmt, 3);
        metadata.checksum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        metadata.userId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

        const char* customDataStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (customDataStr) {
            metadata.customData = DeserializeJson(customDataStr);
        }
    }

    sqlite3_finalize(stmt);
    return metadata;
}

nlohmann::json SQLiteBackend::GetAssetVersion(const std::string& id, int version) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized) {
        return nlohmann::json();
    }

    const char* sql = version == 0 ?
        "SELECT data FROM asset_versions WHERE asset_id=? ORDER BY version DESC LIMIT 1" :
        "SELECT data FROM asset_versions WHERE asset_id=? AND version=?";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) return nlohmann::json();

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    if (version > 0) {
        sqlite3_bind_int(stmt, 2, version);
    }

    nlohmann::json result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* dataStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (dataStr) {
            result = DeserializeJson(dataStr);
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<int> SQLiteBackend::GetAssetVersions(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    std::vector<int> versions;
    if (!m_initialized) {
        return versions;
    }

    auto result = ExecuteQuery("SELECT version FROM asset_versions WHERE asset_id=? ORDER BY version ASC",
                              {id});
    if (result.is_array()) {
        for (const auto& row : result) {
            if (row.contains("version")) {
                versions.push_back(row["version"].get<int>());
            }
        }
    }

    return versions;
}

bool SQLiteBackend::RevertToVersion(const std::string& id, int version) {
    nlohmann::json versionData = GetAssetVersion(id, version);
    if (versionData.is_null()) {
        return false;
    }

    return SaveAsset(id, versionData);
}

std::vector<ChangeEntry> SQLiteBackend::GetChangeHistory(const std::string& id, size_t limit) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    std::vector<ChangeEntry> history;
    if (!m_initialized) {
        return history;
    }

    std::string sql = "SELECT id, asset_id, change_type, old_data, new_data, timestamp, synced, user_id FROM changes WHERE asset_id=? ORDER BY timestamp DESC";
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    auto result = ExecuteQuery(sql, {id});
    if (result.is_array()) {
        for (const auto& row : result) {
            ChangeEntry entry;
            entry.id = row["id"].get<uint64_t>();
            entry.assetId = row["asset_id"].get<std::string>();

            std::string typeStr = row["change_type"].get<std::string>();
            if (typeStr == "create") entry.changeType = ChangeEntry::Type::Create;
            else if (typeStr == "update") entry.changeType = ChangeEntry::Type::Update;
            else if (typeStr == "delete") entry.changeType = ChangeEntry::Type::Delete;

            if (!row["old_data"].is_null()) {
                entry.oldData = DeserializeJson(row["old_data"].get<std::string>());
            }
            if (!row["new_data"].is_null()) {
                entry.newData = DeserializeJson(row["new_data"].get<std::string>());
            }

            entry.timestamp = row["timestamp"].get<uint64_t>();
            entry.synced = row["synced"].get<int>() != 0;
            entry.userId = row["user_id"].get<std::string>();

            history.push_back(entry);
        }
    }

    return history;
}

std::vector<ChangeEntry> SQLiteBackend::GetUnsyncedChanges() {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    std::vector<ChangeEntry> changes;
    if (!m_initialized) {
        return changes;
    }

    auto result = ExecuteQuery(
        "SELECT id, asset_id, change_type, old_data, new_data, timestamp, synced, user_id FROM changes WHERE synced=0 ORDER BY timestamp ASC");

    if (result.is_array()) {
        for (const auto& row : result) {
            ChangeEntry entry;
            entry.id = row["id"].get<uint64_t>();
            entry.assetId = row["asset_id"].get<std::string>();

            std::string typeStr = row["change_type"].get<std::string>();
            if (typeStr == "create") entry.changeType = ChangeEntry::Type::Create;
            else if (typeStr == "update") entry.changeType = ChangeEntry::Type::Update;
            else if (typeStr == "delete") entry.changeType = ChangeEntry::Type::Delete;

            if (!row["old_data"].is_null()) {
                entry.oldData = DeserializeJson(row["old_data"].get<std::string>());
            }
            if (!row["new_data"].is_null()) {
                entry.newData = DeserializeJson(row["new_data"].get<std::string>());
            }

            entry.timestamp = row["timestamp"].get<uint64_t>();
            entry.synced = false;
            entry.userId = row["user_id"].get<std::string>();

            changes.push_back(entry);
        }
    }

    m_syncStatus.pendingChanges = changes.size();
    return changes;
}

bool SQLiteBackend::MarkChangesSynced(const std::vector<uint64_t>& changeIds) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized || changeIds.empty()) {
        return false;
    }

    // Build IN clause
    std::string sql = "UPDATE changes SET synced=1 WHERE id IN (";
    for (size_t i = 0; i < changeIds.size(); ++i) {
        if (i > 0) sql += ",";
        sql += std::to_string(changeIds[i]);
    }
    sql += ")";

    bool success = ExecuteStatement(sql);
    if (success) {
        m_syncStatus.syncedChanges += changeIds.size();
        m_syncStatus.pendingChanges = GetUnsyncedChanges().size();
    }

    return success;
}

void SQLiteBackend::Sync(std::function<void(bool, const std::string&)> callback) {
    // SQLite is local-only, nothing to sync
    if (callback) {
        callback(true, "SQLite backend is local-only");
    }
}

SyncStatus SQLiteBackend::GetSyncStatus() const {
    return m_syncStatus;
}

bool SQLiteBackend::BeginTransaction() {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized) {
        return false;
    }

    if (m_transactionDepth == 0) {
        if (!ExecuteStatement("BEGIN TRANSACTION;")) {
            return false;
        }
    }

    ++m_transactionDepth;
    return true;
}

bool SQLiteBackend::CommitTransaction() {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized || m_transactionDepth == 0) {
        return false;
    }

    --m_transactionDepth;

    if (m_transactionDepth == 0) {
        return ExecuteStatement("COMMIT;");
    }

    return true;
}

bool SQLiteBackend::RollbackTransaction() {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_initialized || m_transactionDepth == 0) {
        return false;
    }

    m_transactionDepth = 0;
    return ExecuteStatement("ROLLBACK;");
}

nlohmann::json SQLiteBackend::GetConflictData(const std::string& id) {
    // SQLite doesn't have remote conflicts
    return nlohmann::json();
}

nlohmann::json SQLiteBackend::ExecuteQuery(const std::string& sql,
                                          const std::vector<std::string>& params) {
    nlohmann::json results = nlohmann::json::array();

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) return results;

    BindParameters(stmt, params);

    int columnCount = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        nlohmann::json row;
        for (int i = 0; i < columnCount; ++i) {
            const char* columnName = sqlite3_column_name(stmt, i);
            int columnType = sqlite3_column_type(stmt, i);

            switch (columnType) {
                case SQLITE_INTEGER:
                    row[columnName] = sqlite3_column_int64(stmt, i);
                    break;
                case SQLITE_FLOAT:
                    row[columnName] = sqlite3_column_double(stmt, i);
                    break;
                case SQLITE_TEXT:
                    row[columnName] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                    break;
                case SQLITE_NULL:
                    row[columnName] = nullptr;
                    break;
                default:
                    break;
            }
        }
        results.push_back(row);
    }

    sqlite3_finalize(stmt);
    return results;
}

bool SQLiteBackend::ExecuteStatement(const std::string& sql,
                                    const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) return false;

    BindParameters(stmt, params);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE && result != SQLITE_ROW) {
        spdlog::error("SQL execution failed: {}", sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

void SQLiteBackend::Vacuum() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    ExecuteStatement("VACUUM;");
}

uint64_t SQLiteBackend::GetDatabaseSize() const {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    try {
        return std::filesystem::file_size(m_config.databasePath);
    } catch (...) {
        return 0;
    }
}

bool SQLiteBackend::ExportToFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    // Use SQLite backup API
    sqlite3* targetDb = nullptr;
    int result = sqlite3_open(filepath.c_str(), &targetDb);
    if (result != SQLITE_OK) {
        spdlog::error("Failed to open target database for export: {}", sqlite3_errmsg(targetDb));
        return false;
    }

    sqlite3_backup* backup = sqlite3_backup_init(targetDb, "main", m_db, "main");
    if (!backup) {
        spdlog::error("Failed to initialize backup: {}", sqlite3_errmsg(targetDb));
        sqlite3_close(targetDb);
        return false;
    }

    sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(targetDb);

    spdlog::info("Database exported to: {}", filepath);
    return true;
}

bool SQLiteBackend::ImportFromFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    sqlite3* sourceDb = nullptr;
    int result = sqlite3_open(filepath.c_str(), &sourceDb);
    if (result != SQLITE_OK) {
        spdlog::error("Failed to open source database for import: {}", sqlite3_errmsg(sourceDb));
        return false;
    }

    sqlite3_backup* backup = sqlite3_backup_init(m_db, "main", sourceDb, "main");
    if (!backup) {
        spdlog::error("Failed to initialize import: {}", sqlite3_errmsg(m_db));
        sqlite3_close(sourceDb);
        return false;
    }

    sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(sourceDb);

    spdlog::info("Database imported from: {}", filepath);
    return true;
}

std::string SQLiteBackend::CalculateChecksum(const nlohmann::json& data) const {
    // Simple checksum based on string hash (use a real hash function in production)
    std::string dataStr = SerializeJson(data);
    return std::to_string(std::hash<std::string>{}(dataStr));
}

uint64_t SQLiteBackend::GetCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string SQLiteBackend::SerializeJson(const nlohmann::json& data) const {
    return data.dump();
}

nlohmann::json SQLiteBackend::DeserializeJson(const std::string& data) const {
    try {
        return nlohmann::json::parse(data);
    } catch (...) {
        return nlohmann::json();
    }
}

bool SQLiteBackend::SaveAssetVersion(const std::string& id, const nlohmann::json& data,
                                    const AssetMetadata& metadata) {
    const char* sql = "INSERT INTO asset_versions (asset_id, version, data, checksum, created_at, user_id) VALUES (?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) return false;

    std::string dataStr = SerializeJson(data);

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, metadata.version);
    sqlite3_bind_text(stmt, 3, dataStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, metadata.checksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, metadata.modifiedAt);
    sqlite3_bind_text(stmt, 6, metadata.userId.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

void SQLiteBackend::PruneOldVersions(const std::string& id) {
    if (m_config.maxVersionsPerAsset == 0) {
        return;  // Unlimited versions
    }

    const char* sql = "DELETE FROM asset_versions WHERE asset_id=? AND version NOT IN "
                     "(SELECT version FROM asset_versions WHERE asset_id=? ORDER BY version DESC LIMIT ?)";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) return;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, m_config.maxVersionsPerAsset);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

bool SQLiteBackend::RecordChange(const std::string& assetId, ChangeEntry::Type type,
                                const nlohmann::json& oldData, const nlohmann::json& newData) {
    const char* sql = "INSERT INTO changes (asset_id, change_type, old_data, new_data, timestamp, synced, user_id) VALUES (?, ?, ?, ?, ?, 0, ?)";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) return false;

    std::string typeStr;
    switch (type) {
        case ChangeEntry::Type::Create: typeStr = "create"; break;
        case ChangeEntry::Type::Update: typeStr = "update"; break;
        case ChangeEntry::Type::Delete: typeStr = "delete"; break;
    }

    std::string oldDataStr = oldData.is_null() ? "" : SerializeJson(oldData);
    std::string newDataStr = newData.is_null() ? "" : SerializeJson(newData);

    sqlite3_bind_text(stmt, 1, assetId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, typeStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, oldDataStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, newDataStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, GetCurrentTimestamp());
    sqlite3_bind_text(stmt, 6, "local", -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result == SQLITE_DONE) {
        ++m_syncStatus.pendingChanges;
        return true;
    }

    return false;
}

sqlite3_stmt* SQLiteBackend::PrepareStatement(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        spdlog::error("Failed to prepare SQL statement: {}", sqlite3_errmsg(m_db));
        return nullptr;
    }

    return stmt;
}

void SQLiteBackend::FinalizeStatement(sqlite3_stmt* stmt) {
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

bool SQLiteBackend::BindParameters(sqlite3_stmt* stmt, const std::vector<std::string>& params) {
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    return true;
}

} // namespace Nova
#endif // NOVA_HAS_SQLITE3
