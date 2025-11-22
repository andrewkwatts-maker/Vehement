#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <fstream>
#include <cstring>
#include <nlohmann/json.hpp>

#ifdef __has_include
#if __has_include(<zlib.h>)
#define NOVA_HAS_ZLIB 1
#include <zlib.h>
#endif
#endif

namespace Nova {

// ============================================================================
// Save System Constants
// ============================================================================

namespace SaveConstants {
    constexpr uint32_t kMagicNumber = 0x4E4F5641;  // "NOVA"
    constexpr uint32_t kCurrentVersion = 1;
    constexpr size_t kMaxSlots = 100;
    constexpr size_t kAutoSaveSlots = 3;
}

// ============================================================================
// Save Data Types
// ============================================================================

/**
 * @brief Save slot metadata
 */
struct SaveSlotInfo {
    int slotIndex = -1;
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    uint32_t playTime = 0;              ///< Play time in seconds
    uint32_t version = 0;               ///< Save format version
    std::string screenshotPath;         ///< Optional screenshot
    std::unordered_map<std::string, std::string> metadata;  ///< Custom metadata

    [[nodiscard]] bool IsEmpty() const { return slotIndex < 0 || name.empty(); }

    [[nodiscard]] std::string GetFormattedTimestamp() const {
        auto time = std::chrono::system_clock::to_time_t(timestamp);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        return buffer;
    }

    [[nodiscard]] std::string GetFormattedPlayTime() const {
        uint32_t hours = playTime / 3600;
        uint32_t minutes = (playTime % 3600) / 60;
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%02u:%02u", hours, minutes);
        return buffer;
    }
};

/**
 * @brief Save operation result
 */
enum class SaveResult {
    Success,
    FileError,
    CompressionError,
    EncryptionError,
    VersionMismatch,
    CorruptedData,
    SlotFull,
    InvalidSlot,
    CloudSyncFailed
};

/**
 * @brief Cloud save status
 */
enum class CloudSyncStatus {
    NotAvailable,
    Synced,
    LocalNewer,
    CloudNewer,
    Conflict,
    Syncing,
    Error
};

// ============================================================================
// Serialization Interface
// ============================================================================

/**
 * @brief Interface for objects that can be saved/loaded
 */
class ISerializable {
public:
    virtual ~ISerializable() = default;

    /**
     * @brief Serialize object to JSON
     */
    virtual nlohmann::json Serialize() const = 0;

    /**
     * @brief Deserialize object from JSON
     * @param data JSON data
     * @param version Save file version for migration
     * @return true if successful
     */
    virtual bool Deserialize(const nlohmann::json& data, uint32_t version) = 0;

    /**
     * @brief Get serialization type name
     */
    [[nodiscard]] virtual std::string GetSerializationType() const = 0;
};

// ============================================================================
// Save Data Container
// ============================================================================

/**
 * @brief Container for save data with type-safe access
 */
class SaveData {
public:
    SaveData() = default;

    // =========== Basic Types ===========

    void SetInt(const std::string& key, int64_t value) { m_data[key] = value; }
    void SetFloat(const std::string& key, double value) { m_data[key] = value; }
    void SetBool(const std::string& key, bool value) { m_data[key] = value; }
    void SetString(const std::string& key, const std::string& value) { m_data[key] = value; }

    [[nodiscard]] int64_t GetInt(const std::string& key, int64_t defaultVal = 0) const {
        return m_data.contains(key) ? m_data[key].get<int64_t>() : defaultVal;
    }

    [[nodiscard]] double GetFloat(const std::string& key, double defaultVal = 0.0) const {
        return m_data.contains(key) ? m_data[key].get<double>() : defaultVal;
    }

    [[nodiscard]] bool GetBool(const std::string& key, bool defaultVal = false) const {
        return m_data.contains(key) ? m_data[key].get<bool>() : defaultVal;
    }

    [[nodiscard]] std::string GetString(const std::string& key, const std::string& defaultVal = "") const {
        return m_data.contains(key) ? m_data[key].get<std::string>() : defaultVal;
    }

    // =========== Arrays ===========

    template<typename T>
    void SetArray(const std::string& key, const std::vector<T>& values) {
        m_data[key] = values;
    }

    template<typename T>
    [[nodiscard]] std::vector<T> GetArray(const std::string& key) const {
        if (!m_data.contains(key)) return {};
        return m_data[key].get<std::vector<T>>();
    }

    // =========== Objects ===========

    void SetObject(const std::string& key, const nlohmann::json& obj) {
        m_data[key] = obj;
    }

    [[nodiscard]] nlohmann::json GetObject(const std::string& key) const {
        return m_data.contains(key) ? m_data[key] : nlohmann::json{};
    }

    // =========== Serializable Objects ===========

    void SetSerializable(const std::string& key, const ISerializable& obj) {
        m_data[key] = obj.Serialize();
        m_data[key]["__type"] = obj.GetSerializationType();
    }

    // =========== Nested SaveData ===========

    SaveData& GetSection(const std::string& key) {
        if (m_sections.find(key) == m_sections.end()) {
            m_sections[key] = std::make_unique<SaveData>();
        }
        return *m_sections[key];
    }

    [[nodiscard]] const SaveData* GetSectionConst(const std::string& key) const {
        auto it = m_sections.find(key);
        return (it != m_sections.end()) ? it->second.get() : nullptr;
    }

    // =========== Utility ===========

    [[nodiscard]] bool Has(const std::string& key) const {
        return m_data.contains(key);
    }

    void Remove(const std::string& key) {
        m_data.erase(key);
        m_sections.erase(key);
    }

    void Clear() {
        m_data.clear();
        m_sections.clear();
    }

    [[nodiscard]] std::vector<std::string> GetKeys() const {
        std::vector<std::string> keys;
        for (auto& [key, _] : m_data.items()) {
            keys.push_back(key);
        }
        return keys;
    }

    // =========== Raw Access ===========

    [[nodiscard]] nlohmann::json ToJson() const {
        nlohmann::json result = m_data;
        for (const auto& [key, section] : m_sections) {
            result["__sections"][key] = section->ToJson();
        }
        return result;
    }

    void FromJson(const nlohmann::json& json) {
        m_data = json;
        m_sections.clear();

        if (json.contains("__sections")) {
            for (auto& [key, value] : json["__sections"].items()) {
                m_sections[key] = std::make_unique<SaveData>();
                m_sections[key]->FromJson(value);
            }
            m_data.erase("__sections");
        }
    }

private:
    nlohmann::json m_data;
    std::unordered_map<std::string, std::unique_ptr<SaveData>> m_sections;
};

// ============================================================================
// Save Migration
// ============================================================================

/**
 * @brief Migration function type for upgrading save data between versions
 */
using MigrationFunc = std::function<bool(SaveData& data, uint32_t fromVersion)>;

/**
 * @brief Save migration registry
 */
class SaveMigration {
public:
    /**
     * @brief Register a migration from one version to the next
     */
    void RegisterMigration(uint32_t fromVersion, MigrationFunc func) {
        m_migrations[fromVersion] = std::move(func);
    }

    /**
     * @brief Apply all migrations from sourceVersion to targetVersion
     */
    bool Migrate(SaveData& data, uint32_t sourceVersion, uint32_t targetVersion) {
        for (uint32_t v = sourceVersion; v < targetVersion; ++v) {
            auto it = m_migrations.find(v);
            if (it == m_migrations.end()) {
                // No migration needed for this version
                continue;
            }
            if (!it->second(data, v)) {
                return false;
            }
        }
        return true;
    }

private:
    std::unordered_map<uint32_t, MigrationFunc> m_migrations;
};

// ============================================================================
// Cloud Save Interface
// ============================================================================

/**
 * @brief Interface for cloud save providers
 */
class ICloudSaveProvider {
public:
    virtual ~ICloudSaveProvider() = default;

    /**
     * @brief Check if cloud saves are available
     */
    [[nodiscard]] virtual bool IsAvailable() const = 0;

    /**
     * @brief Upload save data to cloud
     */
    virtual bool Upload(int slot, const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Download save data from cloud
     */
    virtual bool Download(int slot, std::vector<uint8_t>& data) = 0;

    /**
     * @brief Get cloud save timestamp
     */
    [[nodiscard]] virtual std::chrono::system_clock::time_point GetCloudTimestamp(int slot) const = 0;

    /**
     * @brief Delete cloud save
     */
    virtual bool Delete(int slot) = 0;

    /**
     * @brief Sync all saves
     */
    virtual void SyncAll() = 0;
};

// ============================================================================
// Save Manager
// ============================================================================

/**
 * @brief Complete save game management system
 *
 * Features:
 * - Multiple save slots
 * - Auto-save support
 * - Save versioning and migration
 * - Optional compression (zlib)
 * - Optional encryption (XOR or custom)
 * - Cloud save integration
 * - Screenshot capture for save slots
 *
 * Usage:
 * @code
 * auto& saves = SaveManager::Instance();
 * saves.Initialize("saves");
 *
 * // Save game
 * SaveData data;
 * data.SetInt("player.level", 10);
 * data.SetFloat("player.health", 100.0f);
 * data.GetSection("inventory").SetArray("items", itemList);
 *
 * saves.Save(0, "My Save", data);
 *
 * // Load game
 * SaveData loaded;
 * if (saves.Load(0, loaded)) {
 *     int level = loaded.GetInt("player.level");
 * }
 * @endcode
 */
class SaveManager {
public:
    /**
     * @brief Get singleton instance
     */
    static SaveManager& Instance();

    // Non-copyable
    SaveManager(const SaveManager&) = delete;
    SaveManager& operator=(const SaveManager&) = delete;

    /**
     * @brief Initialize save system
     * @param saveDirectory Directory for save files
     * @param gameId Unique game identifier
     */
    bool Initialize(const std::string& saveDirectory, const std::string& gameId = "game");

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    // =========== Save/Load ===========

    /**
     * @brief Save game to a slot
     * @param slot Slot index (0-99)
     * @param name Save name
     * @param data Save data
     * @param description Optional description
     * @return Save result
     */
    SaveResult Save(int slot, const std::string& name, const SaveData& data,
                    const std::string& description = "");

    /**
     * @brief Load game from a slot
     * @param slot Slot index
     * @param data Output save data
     * @return Save result
     */
    SaveResult Load(int slot, SaveData& data);

    /**
     * @brief Delete a save slot
     */
    SaveResult Delete(int slot);

    /**
     * @brief Quick save to next auto-save slot
     */
    SaveResult QuickSave(const SaveData& data);

    /**
     * @brief Load most recent save
     */
    SaveResult QuickLoad(SaveData& data);

    // =========== Auto-Save ===========

    /**
     * @brief Perform auto-save
     */
    SaveResult AutoSave(const SaveData& data);

    /**
     * @brief Get auto-save interval
     */
    void SetAutoSaveInterval(float seconds) { m_autoSaveInterval = seconds; }

    /**
     * @brief Check and perform auto-save if needed
     * @param deltaTime Time since last frame
     * @param getData Function to get current save data
     */
    void UpdateAutoSave(float deltaTime, std::function<SaveData()> getData);

    // =========== Slot Information ===========

    /**
     * @brief Get information about a save slot
     */
    [[nodiscard]] SaveSlotInfo GetSlotInfo(int slot) const;

    /**
     * @brief Get all save slot infos
     */
    [[nodiscard]] std::vector<SaveSlotInfo> GetAllSlots() const;

    /**
     * @brief Check if slot has a save
     */
    [[nodiscard]] bool SlotExists(int slot) const;

    /**
     * @brief Get next empty slot
     */
    [[nodiscard]] int GetNextEmptySlot() const;

    /**
     * @brief Get most recent save slot
     */
    [[nodiscard]] int GetMostRecentSlot() const;

    // =========== Settings ===========

    /**
     * @brief Enable/disable compression
     */
    void SetCompressionEnabled(bool enabled) { m_compressionEnabled = enabled; }
    [[nodiscard]] bool IsCompressionEnabled() const { return m_compressionEnabled; }

    /**
     * @brief Enable/disable encryption
     */
    void SetEncryptionEnabled(bool enabled) { m_encryptionEnabled = enabled; }
    [[nodiscard]] bool IsEncryptionEnabled() const { return m_encryptionEnabled; }

    /**
     * @brief Set encryption key
     */
    void SetEncryptionKey(const std::string& key) { m_encryptionKey = key; }

    /**
     * @brief Set custom migration registry
     */
    void SetMigration(std::shared_ptr<SaveMigration> migration) {
        m_migration = std::move(migration);
    }

    /**
     * @brief Register a migration
     */
    void RegisterMigration(uint32_t fromVersion, MigrationFunc func) {
        if (!m_migration) {
            m_migration = std::make_shared<SaveMigration>();
        }
        m_migration->RegisterMigration(fromVersion, std::move(func));
    }

    // =========== Cloud Saves ===========

    /**
     * @brief Set cloud save provider
     */
    void SetCloudProvider(std::shared_ptr<ICloudSaveProvider> provider);

    /**
     * @brief Get cloud sync status for a slot
     */
    [[nodiscard]] CloudSyncStatus GetCloudStatus(int slot) const;

    /**
     * @brief Sync a slot with cloud
     */
    SaveResult SyncWithCloud(int slot);

    /**
     * @brief Sync all slots with cloud
     */
    void SyncAllWithCloud();

    /**
     * @brief Resolve a cloud conflict (use local or cloud)
     */
    SaveResult ResolveCloudConflict(int slot, bool useCloud);

    // =========== Screenshots ===========

    /**
     * @brief Capture screenshot for save slot
     * @param slot Slot index
     * @param imageData PNG image data
     */
    void SetSlotScreenshot(int slot, const std::vector<uint8_t>& imageData);

    /**
     * @brief Get screenshot for save slot
     */
    [[nodiscard]] std::vector<uint8_t> GetSlotScreenshot(int slot) const;

    // =========== Callbacks ===========

    using SaveCallback = std::function<void(int slot, SaveResult result)>;
    using LoadCallback = std::function<void(int slot, SaveResult result, const SaveData& data)>;

    void SetSaveCallback(SaveCallback callback) { m_saveCallback = std::move(callback); }
    void SetLoadCallback(LoadCallback callback) { m_loadCallback = std::move(callback); }

    // =========== Statistics ===========

    /**
     * @brief Get total play time across all saves
     */
    [[nodiscard]] uint32_t GetTotalPlayTime() const;

    /**
     * @brief Get number of used save slots
     */
    [[nodiscard]] size_t GetUsedSlotCount() const;

private:
    SaveManager() = default;
    ~SaveManager() = default;

    // File operations
    std::string GetSlotPath(int slot) const;
    std::string GetInfoPath(int slot) const;
    std::string GetScreenshotPath(int slot) const;

    bool WriteSlotInfo(int slot, const SaveSlotInfo& info);
    bool ReadSlotInfo(int slot, SaveSlotInfo& info) const;

    std::vector<uint8_t> SerializeData(const SaveData& data) const;
    bool DeserializeData(const std::vector<uint8_t>& bytes, SaveData& data, uint32_t& version) const;

    std::vector<uint8_t> Compress(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> Decompress(const std::vector<uint8_t>& data) const;

    void Encrypt(std::vector<uint8_t>& data) const;
    void Decrypt(std::vector<uint8_t>& data) const;

    std::string m_saveDirectory;
    std::string m_gameId;

    bool m_compressionEnabled = true;
    bool m_encryptionEnabled = false;
    std::string m_encryptionKey;

    std::shared_ptr<SaveMigration> m_migration;
    std::shared_ptr<ICloudSaveProvider> m_cloudProvider;

    float m_autoSaveInterval = 300.0f;  // 5 minutes
    float m_autoSaveTimer = 0.0f;
    int m_currentAutoSaveSlot = 0;

    uint32_t m_currentPlayTime = 0;

    SaveCallback m_saveCallback;
    LoadCallback m_loadCallback;

    bool m_initialized = false;
};

// ============================================================================
// SaveManager Implementation
// ============================================================================

inline SaveManager& SaveManager::Instance() {
    static SaveManager instance;
    return instance;
}

inline bool SaveManager::Initialize(const std::string& saveDirectory, const std::string& gameId) {
    m_saveDirectory = saveDirectory;
    m_gameId = gameId;

    // Create save directory if it doesn't exist
    #ifdef _WIN32
    _mkdir(saveDirectory.c_str());
    #else
    mkdir(saveDirectory.c_str(), 0755);
    #endif

    m_initialized = true;
    return true;
}

inline void SaveManager::Shutdown() {
    m_initialized = false;
}

inline std::string SaveManager::GetSlotPath(int slot) const {
    return m_saveDirectory + "/" + m_gameId + "_save" + std::to_string(slot) + ".sav";
}

inline std::string SaveManager::GetInfoPath(int slot) const {
    return m_saveDirectory + "/" + m_gameId + "_save" + std::to_string(slot) + ".info";
}

inline std::string SaveManager::GetScreenshotPath(int slot) const {
    return m_saveDirectory + "/" + m_gameId + "_save" + std::to_string(slot) + ".png";
}

inline SaveResult SaveManager::Save(int slot, const std::string& name, const SaveData& data,
                                     const std::string& description) {
    if (slot < 0 || slot >= static_cast<int>(SaveConstants::kMaxSlots)) {
        return SaveResult::InvalidSlot;
    }

    // Serialize data
    auto bytes = SerializeData(data);

    // Compress if enabled
    if (m_compressionEnabled) {
        bytes = Compress(bytes);
    }

    // Encrypt if enabled
    if (m_encryptionEnabled) {
        Encrypt(bytes);
    }

    // Write save file
    std::string path = GetSlotPath(slot);
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return SaveResult::FileError;
    }

    // Write header
    file.write(reinterpret_cast<const char*>(&SaveConstants::kMagicNumber), sizeof(uint32_t));
    file.write(reinterpret_cast<const char*>(&SaveConstants::kCurrentVersion), sizeof(uint32_t));

    uint8_t flags = 0;
    if (m_compressionEnabled) flags |= 0x01;
    if (m_encryptionEnabled) flags |= 0x02;
    file.write(reinterpret_cast<const char*>(&flags), sizeof(uint8_t));

    uint32_t dataSize = static_cast<uint32_t>(bytes.size());
    file.write(reinterpret_cast<const char*>(&dataSize), sizeof(uint32_t));

    // Write data
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    file.close();

    // Write slot info
    SaveSlotInfo info;
    info.slotIndex = slot;
    info.name = name;
    info.description = description;
    info.timestamp = std::chrono::system_clock::now();
    info.playTime = m_currentPlayTime;
    info.version = SaveConstants::kCurrentVersion;
    WriteSlotInfo(slot, info);

    if (m_saveCallback) {
        m_saveCallback(slot, SaveResult::Success);
    }

    return SaveResult::Success;
}

inline SaveResult SaveManager::Load(int slot, SaveData& data) {
    if (slot < 0 || slot >= static_cast<int>(SaveConstants::kMaxSlots)) {
        return SaveResult::InvalidSlot;
    }

    std::string path = GetSlotPath(slot);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return SaveResult::FileError;
    }

    // Read header
    uint32_t magic, version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));

    if (magic != SaveConstants::kMagicNumber) {
        return SaveResult::CorruptedData;
    }

    uint8_t flags;
    file.read(reinterpret_cast<char*>(&flags), sizeof(uint8_t));

    uint32_t dataSize;
    file.read(reinterpret_cast<char*>(&dataSize), sizeof(uint32_t));

    // Read data
    std::vector<uint8_t> bytes(dataSize);
    file.read(reinterpret_cast<char*>(bytes.data()), dataSize);
    file.close();

    // Decrypt if encrypted
    if (flags & 0x02) {
        Decrypt(bytes);
    }

    // Decompress if compressed
    if (flags & 0x01) {
        bytes = Decompress(bytes);
    }

    // Deserialize
    uint32_t dataVersion;
    if (!DeserializeData(bytes, data, dataVersion)) {
        return SaveResult::CorruptedData;
    }

    // Migrate if needed
    if (dataVersion < SaveConstants::kCurrentVersion && m_migration) {
        if (!m_migration->Migrate(data, dataVersion, SaveConstants::kCurrentVersion)) {
            return SaveResult::VersionMismatch;
        }
    }

    if (m_loadCallback) {
        m_loadCallback(slot, SaveResult::Success, data);
    }

    return SaveResult::Success;
}

inline SaveResult SaveManager::Delete(int slot) {
    if (slot < 0 || slot >= static_cast<int>(SaveConstants::kMaxSlots)) {
        return SaveResult::InvalidSlot;
    }

    std::remove(GetSlotPath(slot).c_str());
    std::remove(GetInfoPath(slot).c_str());
    std::remove(GetScreenshotPath(slot).c_str());

    return SaveResult::Success;
}

inline SaveResult SaveManager::QuickSave(const SaveData& data) {
    int slot = GetMostRecentSlot();
    if (slot < 0) slot = 0;
    return Save(slot, "Quick Save", data);
}

inline SaveResult SaveManager::QuickLoad(SaveData& data) {
    int slot = GetMostRecentSlot();
    if (slot < 0) return SaveResult::InvalidSlot;
    return Load(slot, data);
}

inline SaveResult SaveManager::AutoSave(const SaveData& data) {
    int slot = 90 + m_currentAutoSaveSlot;  // Auto-save slots 90-92
    m_currentAutoSaveSlot = (m_currentAutoSaveSlot + 1) % SaveConstants::kAutoSaveSlots;
    return Save(slot, "Auto Save", data);
}

inline void SaveManager::UpdateAutoSave(float deltaTime, std::function<SaveData()> getData) {
    m_currentPlayTime += static_cast<uint32_t>(deltaTime);

    m_autoSaveTimer += deltaTime;
    if (m_autoSaveTimer >= m_autoSaveInterval) {
        m_autoSaveTimer = 0.0f;
        if (getData) {
            AutoSave(getData());
        }
    }
}

inline SaveSlotInfo SaveManager::GetSlotInfo(int slot) const {
    SaveSlotInfo info;
    ReadSlotInfo(slot, info);
    return info;
}

inline std::vector<SaveSlotInfo> SaveManager::GetAllSlots() const {
    std::vector<SaveSlotInfo> slots;
    for (int i = 0; i < static_cast<int>(SaveConstants::kMaxSlots); ++i) {
        SaveSlotInfo info;
        if (ReadSlotInfo(i, info) && !info.IsEmpty()) {
            slots.push_back(info);
        }
    }
    return slots;
}

inline bool SaveManager::SlotExists(int slot) const {
    std::ifstream file(GetSlotPath(slot));
    return file.good();
}

inline int SaveManager::GetNextEmptySlot() const {
    for (int i = 0; i < static_cast<int>(SaveConstants::kMaxSlots - SaveConstants::kAutoSaveSlots); ++i) {
        if (!SlotExists(i)) return i;
    }
    return -1;
}

inline int SaveManager::GetMostRecentSlot() const {
    int mostRecent = -1;
    std::chrono::system_clock::time_point mostRecentTime{};

    for (int i = 0; i < static_cast<int>(SaveConstants::kMaxSlots); ++i) {
        SaveSlotInfo info;
        if (ReadSlotInfo(i, info) && !info.IsEmpty()) {
            if (info.timestamp > mostRecentTime) {
                mostRecentTime = info.timestamp;
                mostRecent = i;
            }
        }
    }

    return mostRecent;
}

inline bool SaveManager::WriteSlotInfo(int slot, const SaveSlotInfo& info) {
    nlohmann::json json;
    json["slot"] = info.slotIndex;
    json["name"] = info.name;
    json["description"] = info.description;
    json["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        info.timestamp.time_since_epoch()).count();
    json["playTime"] = info.playTime;
    json["version"] = info.version;
    json["metadata"] = info.metadata;

    std::ofstream file(GetInfoPath(slot));
    if (!file) return false;
    file << json.dump(2);
    return true;
}

inline bool SaveManager::ReadSlotInfo(int slot, SaveSlotInfo& info) const {
    std::ifstream file(GetInfoPath(slot));
    if (!file) return false;

    try {
        nlohmann::json json;
        file >> json;

        info.slotIndex = json.value("slot", -1);
        info.name = json.value("name", "");
        info.description = json.value("description", "");
        info.timestamp = std::chrono::system_clock::time_point(
            std::chrono::seconds(json.value("timestamp", 0LL)));
        info.playTime = json.value("playTime", 0u);
        info.version = json.value("version", 0u);
        info.screenshotPath = GetScreenshotPath(slot);

        if (json.contains("metadata")) {
            info.metadata = json["metadata"].get<std::unordered_map<std::string, std::string>>();
        }

        return true;
    } catch (...) {
        return false;
    }
}

inline std::vector<uint8_t> SaveManager::SerializeData(const SaveData& data) const {
    nlohmann::json json = data.ToJson();
    std::string str = json.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

inline bool SaveManager::DeserializeData(const std::vector<uint8_t>& bytes, SaveData& data, uint32_t& version) const {
    try {
        std::string str(bytes.begin(), bytes.end());
        nlohmann::json json = nlohmann::json::parse(str);
        data.FromJson(json);
        version = SaveConstants::kCurrentVersion;  // Simplified
        return true;
    } catch (...) {
        return false;
    }
}

inline std::vector<uint8_t> SaveManager::Compress(const std::vector<uint8_t>& data) const {
#ifdef NOVA_HAS_ZLIB
    uLongf compressedSize = compressBound(static_cast<uLong>(data.size()));
    std::vector<uint8_t> compressed(compressedSize + sizeof(uint32_t));

    // Store original size
    uint32_t originalSize = static_cast<uint32_t>(data.size());
    std::memcpy(compressed.data(), &originalSize, sizeof(uint32_t));

    if (compress2(compressed.data() + sizeof(uint32_t), &compressedSize,
                  data.data(), static_cast<uLong>(data.size()), Z_BEST_COMPRESSION) == Z_OK) {
        compressed.resize(compressedSize + sizeof(uint32_t));
        return compressed;
    }
#endif
    return data;
}

inline std::vector<uint8_t> SaveManager::Decompress(const std::vector<uint8_t>& data) const {
#ifdef NOVA_HAS_ZLIB
    if (data.size() < sizeof(uint32_t)) return data;

    uint32_t originalSize;
    std::memcpy(&originalSize, data.data(), sizeof(uint32_t));

    std::vector<uint8_t> decompressed(originalSize);
    uLongf destLen = originalSize;

    if (uncompress(decompressed.data(), &destLen,
                   data.data() + sizeof(uint32_t),
                   static_cast<uLong>(data.size() - sizeof(uint32_t))) == Z_OK) {
        return decompressed;
    }
#endif
    return data;
}

inline void SaveManager::Encrypt(std::vector<uint8_t>& data) const {
    if (m_encryptionKey.empty()) return;

    // Simple XOR encryption (for demo - use proper encryption in production)
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= m_encryptionKey[i % m_encryptionKey.size()];
    }
}

inline void SaveManager::Decrypt(std::vector<uint8_t>& data) const {
    Encrypt(data);  // XOR is symmetric
}

inline void SaveManager::SetCloudProvider(std::shared_ptr<ICloudSaveProvider> provider) {
    m_cloudProvider = std::move(provider);
}

inline CloudSyncStatus SaveManager::GetCloudStatus(int slot) const {
    if (!m_cloudProvider || !m_cloudProvider->IsAvailable()) {
        return CloudSyncStatus::NotAvailable;
    }

    SaveSlotInfo localInfo;
    if (!ReadSlotInfo(slot, localInfo)) {
        return CloudSyncStatus::CloudNewer;
    }

    auto cloudTime = m_cloudProvider->GetCloudTimestamp(slot);
    if (cloudTime == std::chrono::system_clock::time_point{}) {
        return CloudSyncStatus::LocalNewer;
    }

    if (localInfo.timestamp > cloudTime) {
        return CloudSyncStatus::LocalNewer;
    } else if (cloudTime > localInfo.timestamp) {
        return CloudSyncStatus::CloudNewer;
    }

    return CloudSyncStatus::Synced;
}

inline SaveResult SaveManager::SyncWithCloud(int slot) {
    if (!m_cloudProvider) return SaveResult::CloudSyncFailed;

    auto status = GetCloudStatus(slot);

    if (status == CloudSyncStatus::LocalNewer) {
        std::ifstream file(GetSlotPath(slot), std::ios::binary);
        if (!file) return SaveResult::FileError;

        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());

        if (!m_cloudProvider->Upload(slot, data)) {
            return SaveResult::CloudSyncFailed;
        }
    } else if (status == CloudSyncStatus::CloudNewer) {
        std::vector<uint8_t> data;
        if (!m_cloudProvider->Download(slot, data)) {
            return SaveResult::CloudSyncFailed;
        }

        std::ofstream file(GetSlotPath(slot), std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    return SaveResult::Success;
}

inline void SaveManager::SyncAllWithCloud() {
    if (!m_cloudProvider) return;
    m_cloudProvider->SyncAll();
}

inline SaveResult SaveManager::ResolveCloudConflict(int slot, bool useCloud) {
    if (!m_cloudProvider) return SaveResult::CloudSyncFailed;

    if (useCloud) {
        std::vector<uint8_t> data;
        if (!m_cloudProvider->Download(slot, data)) {
            return SaveResult::CloudSyncFailed;
        }

        std::ofstream file(GetSlotPath(slot), std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    } else {
        std::ifstream file(GetSlotPath(slot), std::ios::binary);
        if (!file) return SaveResult::FileError;

        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());

        if (!m_cloudProvider->Upload(slot, data)) {
            return SaveResult::CloudSyncFailed;
        }
    }

    return SaveResult::Success;
}

inline void SaveManager::SetSlotScreenshot(int slot, const std::vector<uint8_t>& imageData) {
    std::ofstream file(GetScreenshotPath(slot), std::ios::binary);
    if (file) {
        file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
    }
}

inline std::vector<uint8_t> SaveManager::GetSlotScreenshot(int slot) const {
    std::ifstream file(GetScreenshotPath(slot), std::ios::binary);
    if (!file) return {};

    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
}

inline uint32_t SaveManager::GetTotalPlayTime() const {
    uint32_t total = 0;
    for (int i = 0; i < static_cast<int>(SaveConstants::kMaxSlots); ++i) {
        SaveSlotInfo info;
        if (ReadSlotInfo(i, info)) {
            total = std::max(total, info.playTime);
        }
    }
    return total;
}

inline size_t SaveManager::GetUsedSlotCount() const {
    size_t count = 0;
    for (int i = 0; i < static_cast<int>(SaveConstants::kMaxSlots); ++i) {
        if (SlotExists(i)) ++count;
    }
    return count;
}

} // namespace Nova

#ifndef _WIN32
#include <sys/stat.h>
#endif
