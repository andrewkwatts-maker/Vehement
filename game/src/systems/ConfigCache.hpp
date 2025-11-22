#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <any>
#include <mutex>
#include <cstdint>

namespace Vehement {
namespace Systems {

/**
 * @brief Hashed string ID for fast config lookups
 *
 * Converts string IDs to 32-bit hashes at compile-time for O(1) lookups.
 */
class StringId {
public:
    using HashType = uint32_t;

    constexpr StringId() : m_hash(0) {}
    constexpr explicit StringId(HashType hash) : m_hash(hash) {}
    constexpr StringId(const char* str) : m_hash(Hash(str)) {}
    StringId(const std::string& str) : m_hash(Hash(str.c_str())) {}
    StringId(std::string_view str) : m_hash(Hash(str.data(), str.size())) {}

    [[nodiscard]] constexpr HashType GetHash() const { return m_hash; }
    [[nodiscard]] constexpr bool IsValid() const { return m_hash != 0; }

    constexpr bool operator==(const StringId& other) const { return m_hash == other.m_hash; }
    constexpr bool operator!=(const StringId& other) const { return m_hash != other.m_hash; }
    constexpr bool operator<(const StringId& other) const { return m_hash < other.m_hash; }

    // Compile-time FNV-1a hash
    static constexpr HashType Hash(const char* str) {
        HashType hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<HashType>(*str++);
            hash *= 16777619u;
        }
        return hash;
    }

    static constexpr HashType Hash(const char* str, size_t len) {
        HashType hash = 2166136261u;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<HashType>(str[i]);
            hash *= 16777619u;
        }
        return hash;
    }

private:
    HashType m_hash;
};

// String literal operator for compile-time string IDs
constexpr StringId operator""_sid(const char* str, size_t len) {
    return StringId(StringId::Hash(str, len));
}

} // namespace Systems
} // namespace Vehement

// Specialization for std::hash
template<>
struct std::hash<Vehement::Systems::StringId> {
    size_t operator()(const Vehement::Systems::StringId& id) const noexcept {
        return static_cast<size_t>(id.GetHash());
    }
};

namespace Vehement {
namespace Systems {

/**
 * @brief Variant type for config values
 */
using ConfigValue = std::variant<
    bool,
    int32_t,
    int64_t,
    float,
    double,
    std::string,
    std::vector<int32_t>,
    std::vector<float>,
    std::vector<std::string>
>;

/**
 * @brief Precomputed derived value
 */
struct DerivedValue {
    ConfigValue value;
    std::vector<StringId> dependencies;  // Source values this depends on
    std::function<ConfigValue(const class ConfigCache&)> computeFunc;
    bool isDirty = true;
};

/**
 * @brief Config entry with metadata
 */
struct ConfigEntry {
    StringId id;
    std::string name;           // Original string name (for debugging)
    ConfigValue value;
    bool isReadOnly = false;
    bool isHotReloadable = true;
    std::string category;       // For organization
    std::string description;
};

/**
 * @brief Table of indexed config entries
 */
template<typename T>
class ConfigTable {
public:
    /**
     * @brief Add entry to table
     * @return Index of entry
     */
    size_t Add(const T& entry) {
        size_t index = m_entries.size();
        m_entries.push_back(entry);
        return index;
    }

    /**
     * @brief Get entry by index (fast)
     */
    [[nodiscard]] const T& Get(size_t index) const {
        return m_entries[index];
    }

    /**
     * @brief Get mutable entry by index
     */
    T& GetMutable(size_t index) {
        return m_entries[index];
    }

    /**
     * @brief Get all entries
     */
    [[nodiscard]] const std::vector<T>& GetAll() const {
        return m_entries;
    }

    [[nodiscard]] size_t Size() const { return m_entries.size(); }
    [[nodiscard]] bool Empty() const { return m_entries.empty(); }

    void Clear() { m_entries.clear(); }

private:
    std::vector<T> m_entries;
};

// ============================================================================
// Config Cache - High-Performance Config Access
// ============================================================================

/**
 * @brief High-performance cached config system
 *
 * Features:
 * - String ID hashing for O(1) lookups
 * - Index-based access for hot paths
 * - Precomputed derived values
 * - Type-safe value access
 * - Hot-reload support
 * - Category organization
 */
class ConfigCache {
public:
    ConfigCache();
    ~ConfigCache() = default;

    // Non-copyable, movable
    ConfigCache(const ConfigCache&) = delete;
    ConfigCache& operator=(const ConfigCache&) = delete;
    ConfigCache(ConfigCache&&) noexcept = default;
    ConfigCache& operator=(ConfigCache&&) noexcept = default;

    // =========================================================================
    // Registration
    // =========================================================================

    /**
     * @brief Register a config value
     * @param name String name (hashed for fast lookup)
     * @param value Initial value
     * @param category Optional category for organization
     * @return Index for fast access
     */
    size_t Register(const std::string& name, const ConfigValue& value,
                    const std::string& category = "");

    /**
     * @brief Register a config value with string ID
     */
    size_t Register(StringId id, const std::string& name, const ConfigValue& value,
                    const std::string& category = "");

    /**
     * @brief Register a derived value (computed from other values)
     */
    size_t RegisterDerived(const std::string& name,
                           const std::vector<std::string>& dependencies,
                           std::function<ConfigValue(const ConfigCache&)> computeFunc,
                           const std::string& category = "");

    /**
     * @brief Unregister a value
     */
    bool Unregister(StringId id);

    // =========================================================================
    // Access by Name (Hashed)
    // =========================================================================

    /**
     * @brief Get value by string ID
     */
    [[nodiscard]] const ConfigValue* Get(StringId id) const;

    /**
     * @brief Get typed value by string ID
     */
    template<typename T>
    [[nodiscard]] std::optional<T> GetAs(StringId id) const {
        const ConfigValue* value = Get(id);
        if (value && std::holds_alternative<T>(*value)) {
            return std::get<T>(*value);
        }
        return std::nullopt;
    }

    /**
     * @brief Get typed value with default
     */
    template<typename T>
    [[nodiscard]] T GetOr(StringId id, const T& defaultValue) const {
        auto value = GetAs<T>(id);
        return value.has_value() ? *value : defaultValue;
    }

    /**
     * @brief Check if value exists
     */
    [[nodiscard]] bool Has(StringId id) const;

    /**
     * @brief Get index for a string ID
     */
    [[nodiscard]] std::optional<size_t> GetIndex(StringId id) const;

    // =========================================================================
    // Access by Index (Fastest)
    // =========================================================================

    /**
     * @brief Get value by index (fastest access)
     */
    [[nodiscard]] const ConfigValue& GetByIndex(size_t index) const;

    /**
     * @brief Get typed value by index
     */
    template<typename T>
    [[nodiscard]] const T& GetByIndexAs(size_t index) const {
        return std::get<T>(GetByIndex(index));
    }

    /**
     * @brief Get entry by index
     */
    [[nodiscard]] const ConfigEntry& GetEntry(size_t index) const;

    // =========================================================================
    // Modification
    // =========================================================================

    /**
     * @brief Set value by string ID
     */
    bool Set(StringId id, const ConfigValue& value);

    /**
     * @brief Set value by index
     */
    bool SetByIndex(size_t index, const ConfigValue& value);

    /**
     * @brief Set multiple values at once
     */
    void SetBatch(const std::vector<std::pair<StringId, ConfigValue>>& values);

    // =========================================================================
    // Derived Values
    // =========================================================================

    /**
     * @brief Recompute all dirty derived values
     */
    void RecomputeDerivedValues();

    /**
     * @brief Mark a derived value as needing recomputation
     */
    void InvalidateDerived(StringId id);

    /**
     * @brief Mark all derived values as dirty
     */
    void InvalidateAllDerived();

    // =========================================================================
    // Loading/Saving
    // =========================================================================

    /**
     * @brief Load config from JSON string
     * @return Number of values loaded
     */
    size_t LoadFromJson(const std::string& json);

    /**
     * @brief Load config from JSON file
     */
    size_t LoadFromFile(const std::string& filepath);

    /**
     * @brief Save config to JSON string
     */
    [[nodiscard]] std::string SaveToJson() const;

    /**
     * @brief Save config to file
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * @brief Reload from original file (for hot-reload)
     */
    bool Reload();

    // =========================================================================
    // Convenience Type-Specific Getters
    // =========================================================================

    [[nodiscard]] bool GetBool(StringId id, bool defaultValue = false) const {
        return GetOr<bool>(id, defaultValue);
    }

    [[nodiscard]] int32_t GetInt(StringId id, int32_t defaultValue = 0) const {
        return GetOr<int32_t>(id, defaultValue);
    }

    [[nodiscard]] float GetFloat(StringId id, float defaultValue = 0.0f) const {
        return GetOr<float>(id, defaultValue);
    }

    [[nodiscard]] double GetDouble(StringId id, double defaultValue = 0.0) const {
        return GetOr<double>(id, defaultValue);
    }

    [[nodiscard]] std::string GetString(StringId id, const std::string& defaultValue = "") const {
        return GetOr<std::string>(id, defaultValue);
    }

    // =========================================================================
    // Iteration and Queries
    // =========================================================================

    /**
     * @brief Get all entries in a category
     */
    [[nodiscard]] std::vector<const ConfigEntry*> GetByCategory(const std::string& category) const;

    /**
     * @brief Get all category names
     */
    [[nodiscard]] std::vector<std::string> GetCategories() const;

    /**
     * @brief Iterate over all entries
     */
    void ForEach(std::function<void(const ConfigEntry&)> callback) const;

    /**
     * @brief Get total entry count
     */
    [[nodiscard]] size_t GetCount() const { return m_entries.Size(); }

    // =========================================================================
    // Change Notifications
    // =========================================================================

    using ChangeCallback = std::function<void(StringId, const ConfigValue&)>;

    /**
     * @brief Subscribe to changes
     * @return Subscription ID
     */
    int Subscribe(ChangeCallback callback);

    /**
     * @brief Subscribe to changes for specific value
     */
    int Subscribe(StringId id, ChangeCallback callback);

    /**
     * @brief Unsubscribe from changes
     */
    void Unsubscribe(int subscriptionId);

    // =========================================================================
    // Debug
    // =========================================================================

    /**
     * @brief Get string name for a hash (debug only)
     */
    [[nodiscard]] std::string GetNameForId(StringId id) const;

    /**
     * @brief Dump all config to string
     */
    [[nodiscard]] std::string DebugDump() const;

private:
    void NotifyChange(StringId id, const ConfigValue& value);
    void MarkDependentsDirty(StringId changedId);

    // Entry storage
    ConfigTable<ConfigEntry> m_entries;
    std::unordered_map<StringId, size_t> m_idToIndex;

    // Derived values
    std::unordered_map<StringId, DerivedValue> m_derivedValues;

    // Change notifications
    std::unordered_map<int, ChangeCallback> m_globalSubscribers;
    std::unordered_map<StringId, std::unordered_map<int, ChangeCallback>> m_valueSubscribers;
    int m_nextSubscriberId = 1;

    // Thread safety
    mutable std::mutex m_mutex;

    // Hot reload
    std::string m_loadedFilePath;
};

// ============================================================================
// Typed Config Table - For Entity/Unit Definitions
// ============================================================================

/**
 * @brief Typed config table for entity definitions
 *
 * Provides index-based access to entity configurations with
 * compile-time type safety.
 */
template<typename ConfigType>
class TypedConfigTable {
public:
    /**
     * @brief Register a configuration
     * @return Index for fast access
     */
    size_t Register(const std::string& id, const ConfigType& config) {
        StringId stringId(id);
        size_t index = m_configs.size();

        m_configs.push_back(config);
        m_idToIndex[stringId] = index;
        m_indexToId.push_back(stringId);
        m_names.push_back(id);

        return index;
    }

    /**
     * @brief Get config by string ID
     */
    [[nodiscard]] const ConfigType* Get(StringId id) const {
        auto it = m_idToIndex.find(id);
        if (it != m_idToIndex.end()) {
            return &m_configs[it->second];
        }
        return nullptr;
    }

    /**
     * @brief Get config by index (fastest)
     */
    [[nodiscard]] const ConfigType& GetByIndex(size_t index) const {
        return m_configs[index];
    }

    /**
     * @brief Get mutable config by index
     */
    ConfigType& GetMutableByIndex(size_t index) {
        return m_configs[index];
    }

    /**
     * @brief Get index for string ID
     */
    [[nodiscard]] std::optional<size_t> GetIndex(StringId id) const {
        auto it = m_idToIndex.find(id);
        if (it != m_idToIndex.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Get string name by index
     */
    [[nodiscard]] const std::string& GetName(size_t index) const {
        return m_names[index];
    }

    /**
     * @brief Check if ID exists
     */
    [[nodiscard]] bool Has(StringId id) const {
        return m_idToIndex.count(id) > 0;
    }

    /**
     * @brief Get all configs
     */
    [[nodiscard]] const std::vector<ConfigType>& GetAll() const {
        return m_configs;
    }

    [[nodiscard]] size_t Size() const { return m_configs.size(); }

    void Clear() {
        m_configs.clear();
        m_idToIndex.clear();
        m_indexToId.clear();
        m_names.clear();
    }

private:
    std::vector<ConfigType> m_configs;
    std::unordered_map<StringId, size_t> m_idToIndex;
    std::vector<StringId> m_indexToId;
    std::vector<std::string> m_names;
};

// ============================================================================
// Precomputed Stats Cache
// ============================================================================

/**
 * @brief Cache for precomputed game statistics
 *
 * Stores derived/calculated values that are expensive to compute
 * but frequently accessed.
 */
class StatsCache {
public:
    struct CachedStat {
        float value = 0.0f;
        float baseValue = 0.0f;
        std::vector<float> modifiers;     // Additive modifiers
        std::vector<float> multipliers;   // Multiplicative modifiers
        bool isDirty = true;
    };

    /**
     * @brief Register a stat for caching
     */
    void RegisterStat(StringId id, float baseValue);

    /**
     * @brief Get computed stat value (uses cache)
     */
    [[nodiscard]] float GetStat(StringId id);

    /**
     * @brief Set base value (marks as dirty)
     */
    void SetBaseValue(StringId id, float value);

    /**
     * @brief Add additive modifier
     */
    void AddModifier(StringId id, float modifier);

    /**
     * @brief Add multiplicative modifier
     */
    void AddMultiplier(StringId id, float multiplier);

    /**
     * @brief Clear all modifiers for a stat
     */
    void ClearModifiers(StringId id);

    /**
     * @brief Recompute all dirty stats
     */
    void RecomputeDirty();

    /**
     * @brief Mark all stats as dirty
     */
    void InvalidateAll();

private:
    float ComputeStat(CachedStat& stat);

    std::unordered_map<StringId, CachedStat> m_stats;
};

// ============================================================================
// Global Config Instance
// ============================================================================

/**
 * @brief Get the global config cache instance
 */
ConfigCache& GetGlobalConfig();

/**
 * @brief Initialize global config from file
 */
bool InitializeGlobalConfig(const std::string& filepath);

} // namespace Systems
} // namespace Vehement
