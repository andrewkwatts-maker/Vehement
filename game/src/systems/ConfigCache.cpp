#include "ConfigCache.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Vehement {
namespace Systems {

// ============================================================================
// ConfigCache Implementation
// ============================================================================

ConfigCache::ConfigCache() {
}

size_t ConfigCache::Register(const std::string& name, const ConfigValue& value,
                              const std::string& category) {
    return Register(StringId(name), name, value, category);
}

size_t ConfigCache::Register(StringId id, const std::string& name, const ConfigValue& value,
                              const std::string& category) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if already registered
    auto it = m_idToIndex.find(id);
    if (it != m_idToIndex.end()) {
        // Update existing entry
        m_entries.GetMutable(it->second).value = value;
        return it->second;
    }

    // Create new entry
    ConfigEntry entry;
    entry.id = id;
    entry.name = name;
    entry.value = value;
    entry.category = category;

    size_t index = m_entries.Add(entry);
    m_idToIndex[id] = index;

    return index;
}

size_t ConfigCache::RegisterDerived(const std::string& name,
                                     const std::vector<std::string>& dependencies,
                                     std::function<ConfigValue(const ConfigCache&)> computeFunc,
                                     const std::string& category) {
    StringId id(name);

    // Register placeholder entry
    size_t index = Register(id, name, ConfigValue{0}, category);

    // Create derived value
    DerivedValue derived;
    derived.computeFunc = computeFunc;
    derived.isDirty = true;

    for (const auto& dep : dependencies) {
        derived.dependencies.push_back(StringId(dep));
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_derivedValues[id] = std::move(derived);
    }

    return index;
}

bool ConfigCache::Unregister(StringId id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) {
        return false;
    }

    // Note: Can't actually remove from vector without invalidating indices
    // Mark as invalid instead
    m_idToIndex.erase(it);
    m_derivedValues.erase(id);

    return true;
}

const ConfigValue* ConfigCache::Get(StringId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_idToIndex.find(id);
    if (it != m_idToIndex.end()) {
        return &m_entries.Get(it->second).value;
    }
    return nullptr;
}

bool ConfigCache::Has(StringId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idToIndex.count(id) > 0;
}

std::optional<size_t> ConfigCache::GetIndex(StringId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_idToIndex.find(id);
    if (it != m_idToIndex.end()) {
        return it->second;
    }
    return std::nullopt;
}

const ConfigValue& ConfigCache::GetByIndex(size_t index) const {
    // No lock needed - entries are never moved after registration
    return m_entries.Get(index).value;
}

const ConfigEntry& ConfigCache::GetEntry(size_t index) const {
    return m_entries.Get(index);
}

bool ConfigCache::Set(StringId id, const ConfigValue& value) {
    size_t index;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_idToIndex.find(id);
        if (it == m_idToIndex.end()) {
            return false;
        }

        index = it->second;
        ConfigEntry& entry = m_entries.GetMutable(index);

        if (entry.isReadOnly) {
            return false;
        }

        entry.value = value;
    }

    // Notify outside lock
    NotifyChange(id, value);
    MarkDependentsDirty(id);

    return true;
}

bool ConfigCache::SetByIndex(size_t index, const ConfigValue& value) {
    StringId id;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (index >= m_entries.Size()) {
            return false;
        }

        ConfigEntry& entry = m_entries.GetMutable(index);

        if (entry.isReadOnly) {
            return false;
        }

        entry.value = value;
        id = entry.id;
    }

    NotifyChange(id, value);
    MarkDependentsDirty(id);

    return true;
}

void ConfigCache::SetBatch(const std::vector<std::pair<StringId, ConfigValue>>& values) {
    std::vector<std::pair<StringId, ConfigValue>> changed;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (const auto& [id, value] : values) {
            auto it = m_idToIndex.find(id);
            if (it != m_idToIndex.end()) {
                ConfigEntry& entry = m_entries.GetMutable(it->second);
                if (!entry.isReadOnly) {
                    entry.value = value;
                    changed.emplace_back(id, value);
                }
            }
        }
    }

    // Notify changes outside lock
    for (const auto& [id, value] : changed) {
        NotifyChange(id, value);
        MarkDependentsDirty(id);
    }
}

void ConfigCache::RecomputeDerivedValues() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, derived] : m_derivedValues) {
        if (derived.isDirty && derived.computeFunc) {
            derived.value = derived.computeFunc(*this);
            derived.isDirty = false;

            // Update entry
            auto it = m_idToIndex.find(id);
            if (it != m_idToIndex.end()) {
                m_entries.GetMutable(it->second).value = derived.value;
            }
        }
    }
}

void ConfigCache::InvalidateDerived(StringId id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_derivedValues.find(id);
    if (it != m_derivedValues.end()) {
        it->second.isDirty = true;
    }
}

void ConfigCache::InvalidateAllDerived() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, derived] : m_derivedValues) {
        derived.isDirty = true;
    }
}

// Simple JSON parser (basic implementation)
namespace {
    std::string Trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    ConfigValue ParseJsonValue(const std::string& str) {
        std::string trimmed = Trim(str);

        if (trimmed.empty()) {
            return std::string("");
        }

        // Boolean
        if (trimmed == "true") return true;
        if (trimmed == "false") return false;

        // String
        if (trimmed.front() == '"' && trimmed.back() == '"') {
            return trimmed.substr(1, trimmed.size() - 2);
        }

        // Number
        bool hasDecimal = trimmed.find('.') != std::string::npos;
        try {
            if (hasDecimal) {
                return std::stof(trimmed);
            } else {
                return std::stoi(trimmed);
            }
        } catch (...) {
            return trimmed;
        }
    }
}

size_t ConfigCache::LoadFromJson(const std::string& json) {
    size_t count = 0;

    // Very basic JSON parsing
    // In production, use nlohmann::json or similar
    std::string current = json;

    // Find key-value pairs
    size_t pos = 0;
    while ((pos = current.find('"', pos)) != std::string::npos) {
        size_t keyStart = pos + 1;
        size_t keyEnd = current.find('"', keyStart);
        if (keyEnd == std::string::npos) break;

        std::string key = current.substr(keyStart, keyEnd - keyStart);

        // Find colon
        size_t colonPos = current.find(':', keyEnd);
        if (colonPos == std::string::npos) break;

        // Find value end (comma or closing brace)
        size_t valueStart = colonPos + 1;
        size_t valueEnd = current.find_first_of(",}", valueStart);
        if (valueEnd == std::string::npos) valueEnd = current.size();

        std::string valueStr = Trim(current.substr(valueStart, valueEnd - valueStart));

        // Handle nested objects
        if (valueStr.front() == '{') {
            // Skip nested object for now
            pos = valueEnd + 1;
            continue;
        }

        ConfigValue value = ParseJsonValue(valueStr);
        Register(key, value);
        ++count;

        pos = valueEnd + 1;
    }

    return count;
}

size_t ConfigCache::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return 0;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    m_loadedFilePath = filepath;

    return LoadFromJson(buffer.str());
}

std::string ConfigCache::SaveToJson() const {
    std::stringstream ss;
    ss << "{\n";

    bool first = true;
    for (size_t i = 0; i < m_entries.Size(); ++i) {
        const ConfigEntry& entry = m_entries.Get(i);

        if (!first) ss << ",\n";
        first = false;

        ss << "  \"" << entry.name << "\": ";

        std::visit([&ss](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                ss << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int32_t>) {
                ss << arg;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                ss << arg;
            } else if constexpr (std::is_same_v<T, float>) {
                ss << arg;
            } else if constexpr (std::is_same_v<T, double>) {
                ss << arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
                ss << "\"" << arg << "\"";
            } else if constexpr (std::is_same_v<T, std::vector<int32_t>>) {
                ss << "[";
                for (size_t j = 0; j < arg.size(); ++j) {
                    if (j > 0) ss << ", ";
                    ss << arg[j];
                }
                ss << "]";
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                ss << "[";
                for (size_t j = 0; j < arg.size(); ++j) {
                    if (j > 0) ss << ", ";
                    ss << arg[j];
                }
                ss << "]";
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                ss << "[";
                for (size_t j = 0; j < arg.size(); ++j) {
                    if (j > 0) ss << ", ";
                    ss << "\"" << arg[j] << "\"";
                }
                ss << "]";
            }
        }, entry.value);
    }

    ss << "\n}";
    return ss.str();
}

bool ConfigCache::SaveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << SaveToJson();
    return true;
}

bool ConfigCache::Reload() {
    if (m_loadedFilePath.empty()) {
        return false;
    }

    return LoadFromFile(m_loadedFilePath) > 0;
}

std::vector<const ConfigEntry*> ConfigCache::GetByCategory(const std::string& category) const {
    std::vector<const ConfigEntry*> result;

    for (size_t i = 0; i < m_entries.Size(); ++i) {
        const ConfigEntry& entry = m_entries.Get(i);
        if (entry.category == category) {
            result.push_back(&entry);
        }
    }

    return result;
}

std::vector<std::string> ConfigCache::GetCategories() const {
    std::vector<std::string> categories;

    for (size_t i = 0; i < m_entries.Size(); ++i) {
        const ConfigEntry& entry = m_entries.Get(i);
        if (!entry.category.empty() &&
            std::find(categories.begin(), categories.end(), entry.category) == categories.end()) {
            categories.push_back(entry.category);
        }
    }

    return categories;
}

void ConfigCache::ForEach(std::function<void(const ConfigEntry&)> callback) const {
    for (size_t i = 0; i < m_entries.Size(); ++i) {
        callback(m_entries.Get(i));
    }
}

int ConfigCache::Subscribe(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int id = m_nextSubscriberId++;
    m_globalSubscribers[id] = callback;
    return id;
}

int ConfigCache::Subscribe(StringId id, ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int subId = m_nextSubscriberId++;
    m_valueSubscribers[id][subId] = callback;
    return subId;
}

void ConfigCache::Unsubscribe(int subscriptionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_globalSubscribers.erase(subscriptionId);

    for (auto& [id, subscribers] : m_valueSubscribers) {
        subscribers.erase(subscriptionId);
    }
}

std::string ConfigCache::GetNameForId(StringId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_idToIndex.find(id);
    if (it != m_idToIndex.end()) {
        return m_entries.Get(it->second).name;
    }
    return "";
}

std::string ConfigCache::DebugDump() const {
    std::stringstream ss;
    ss << "ConfigCache dump (" << m_entries.Size() << " entries):\n";

    for (size_t i = 0; i < m_entries.Size(); ++i) {
        const ConfigEntry& entry = m_entries.Get(i);
        ss << "  [" << i << "] " << entry.name << " (0x"
           << std::hex << entry.id.GetHash() << std::dec << "): ";

        std::visit([&ss](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                ss << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, std::string>) {
                ss << "\"" << arg << "\"";
            } else if constexpr (std::is_arithmetic_v<T>) {
                ss << arg;
            } else {
                ss << "(array)";
            }
        }, entry.value);

        if (!entry.category.empty()) {
            ss << " [" << entry.category << "]";
        }

        ss << "\n";
    }

    return ss.str();
}

void ConfigCache::NotifyChange(StringId id, const ConfigValue& value) {
    // Copy callbacks to avoid holding lock during callback
    std::vector<ChangeCallback> callbacks;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (const auto& [subId, callback] : m_globalSubscribers) {
            callbacks.push_back(callback);
        }

        auto it = m_valueSubscribers.find(id);
        if (it != m_valueSubscribers.end()) {
            for (const auto& [subId, callback] : it->second) {
                callbacks.push_back(callback);
            }
        }
    }

    for (const auto& callback : callbacks) {
        callback(id, value);
    }
}

void ConfigCache::MarkDependentsDirty(StringId changedId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, derived] : m_derivedValues) {
        for (const auto& dep : derived.dependencies) {
            if (dep == changedId) {
                derived.isDirty = true;
                break;
            }
        }
    }
}

// ============================================================================
// StatsCache Implementation
// ============================================================================

void StatsCache::RegisterStat(StringId id, float baseValue) {
    CachedStat stat;
    stat.baseValue = baseValue;
    stat.value = baseValue;
    stat.isDirty = false;

    m_stats[id] = stat;
}

float StatsCache::GetStat(StringId id) {
    auto it = m_stats.find(id);
    if (it == m_stats.end()) {
        return 0.0f;
    }

    if (it->second.isDirty) {
        ComputeStat(it->second);
    }

    return it->second.value;
}

void StatsCache::SetBaseValue(StringId id, float value) {
    auto it = m_stats.find(id);
    if (it != m_stats.end()) {
        it->second.baseValue = value;
        it->second.isDirty = true;
    }
}

void StatsCache::AddModifier(StringId id, float modifier) {
    auto it = m_stats.find(id);
    if (it != m_stats.end()) {
        it->second.modifiers.push_back(modifier);
        it->second.isDirty = true;
    }
}

void StatsCache::AddMultiplier(StringId id, float multiplier) {
    auto it = m_stats.find(id);
    if (it != m_stats.end()) {
        it->second.multipliers.push_back(multiplier);
        it->second.isDirty = true;
    }
}

void StatsCache::ClearModifiers(StringId id) {
    auto it = m_stats.find(id);
    if (it != m_stats.end()) {
        it->second.modifiers.clear();
        it->second.multipliers.clear();
        it->second.isDirty = true;
    }
}

void StatsCache::RecomputeDirty() {
    for (auto& [id, stat] : m_stats) {
        if (stat.isDirty) {
            ComputeStat(stat);
        }
    }
}

void StatsCache::InvalidateAll() {
    for (auto& [id, stat] : m_stats) {
        stat.isDirty = true;
    }
}

float StatsCache::ComputeStat(CachedStat& stat) {
    // Formula: (base + sum(modifiers)) * product(multipliers)
    float value = stat.baseValue;

    // Add modifiers
    for (float mod : stat.modifiers) {
        value += mod;
    }

    // Apply multipliers
    for (float mult : stat.multipliers) {
        value *= mult;
    }

    stat.value = value;
    stat.isDirty = false;

    return value;
}

// ============================================================================
// Global Instance
// ============================================================================

namespace {
    std::unique_ptr<ConfigCache> g_globalConfig;
}

ConfigCache& GetGlobalConfig() {
    if (!g_globalConfig) {
        g_globalConfig = std::make_unique<ConfigCache>();
    }
    return *g_globalConfig;
}

bool InitializeGlobalConfig(const std::string& filepath) {
    return GetGlobalConfig().LoadFromFile(filepath) > 0;
}

} // namespace Systems
} // namespace Vehement
