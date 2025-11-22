#include "ConfigRegistry.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace Vehement {
namespace Config {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// ConfigRegistry Implementation
// ============================================================================

ConfigRegistry& ConfigRegistry::Instance() {
    static ConfigRegistry instance;
    return instance;
}

size_t ConfigRegistry::LoadDirectory(const std::string& rootPath,
                                     const std::vector<std::string>& extensions) {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t loaded = 0;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            bool validExt = extensions.empty();
            for (const auto& allowedExt : extensions) {
                if (ext == allowedExt) {
                    validExt = true;
                    break;
                }
            }

            if (!validExt) continue;

            std::string filePath = entry.path().string();
            auto config = CreateConfigForFile(filePath);

            if (config && !config->GetId().empty()) {
                std::string id = config->GetId();

                // Store file timestamp for hot reload
                auto ftime = fs::last_write_time(entry.path());
                m_fileTimestamps[filePath] = ftime.time_since_epoch().count();

                // Register the config
                m_configs[id] = config;
                m_pathToId[filePath] = id;

                // Update indices
                std::string type = config->GetConfigType();
                m_typeIndex[type].insert(id);

                for (const auto& tag : config->GetTags()) {
                    m_tagIndex[tag].insert(id);
                }

                ++loaded;
            }
        }
    } catch (const fs::filesystem_error& e) {
        // Directory doesn't exist or access denied
    }

    // Resolve inheritance after loading all configs
    ResolveInheritance();

    return loaded;
}

bool ConfigRegistry::LoadFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto config = CreateConfigForFile(filePath);
    if (!config || config->GetId().empty()) {
        return false;
    }

    std::string id = config->GetId();
    bool isNew = (m_configs.find(id) == m_configs.end());

    // Store file timestamp
    try {
        auto ftime = fs::last_write_time(filePath);
        m_fileTimestamps[filePath] = ftime.time_since_epoch().count();
    } catch (...) {}

    m_configs[id] = config;
    m_pathToId[filePath] = id;

    // Update indices
    std::string type = config->GetConfigType();
    m_typeIndex[type].insert(id);

    for (const auto& tag : config->GetTags()) {
        m_tagIndex[tag].insert(id);
    }

    // Resolve inheritance for this config
    std::unordered_set<std::string> visited;
    ResolveInheritanceFor(id, visited);

    // Notify subscribers
    ConfigChangeEvent event;
    event.changeType = isNew ? ConfigChangeEvent::Type::Added
                             : ConfigChangeEvent::Type::Modified;
    event.configId = id;
    event.configPath = filePath;
    event.config = config;
    NotifyChange(event);

    return true;
}

bool ConfigRegistry::ReloadConfig(const std::string& configId) {
    std::string sourcePath;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_configs.find(configId);
        if (it == m_configs.end()) {
            return false;
        }
        sourcePath = it->second->GetSourcePath();
    }

    if (sourcePath.empty()) {
        return false;
    }

    return LoadFile(sourcePath);
}

size_t ConfigRegistry::ReloadAll() {
    std::vector<std::string> paths;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [path, id] : m_pathToId) {
            paths.push_back(path);
        }
    }

    size_t reloaded = 0;
    for (const auto& path : paths) {
        if (LoadFile(path)) {
            ++reloaded;
        }
    }

    return reloaded;
}

void ConfigRegistry::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_configs.clear();
    m_pathToId.clear();
    m_typeIndex.clear();
    m_tagIndex.clear();
    m_fileTimestamps.clear();
}

void ConfigRegistry::EnableHotReload(const std::string& rootPath, int pollIntervalMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_hotReloadEnabled = true;
    m_watchPath = rootPath;
    m_pollInterval = pollIntervalMs;

    // Initialize timestamps for all files
    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                auto ftime = fs::last_write_time(entry.path());
                m_fileTimestamps[path] = ftime.time_since_epoch().count();
            }
        }
    } catch (...) {}
}

void ConfigRegistry::DisableHotReload() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hotReloadEnabled = false;
}

size_t ConfigRegistry::CheckForChanges() {
    if (!m_hotReloadEnabled || m_watchPath.empty()) {
        return 0;
    }

    size_t changes = 0;
    std::vector<std::string> modifiedFiles;
    std::vector<std::string> newFiles;
    std::vector<std::string> deletedFiles;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check existing files
        std::unordered_set<std::string> currentFiles;

        try {
            for (const auto& entry : fs::recursive_directory_iterator(m_watchPath)) {
                if (!entry.is_regular_file()) continue;

                std::string ext = entry.path().extension().string();
                if (ext != ".json") continue;

                std::string path = entry.path().string();
                currentFiles.insert(path);

                auto ftime = fs::last_write_time(entry.path());
                int64_t timestamp = ftime.time_since_epoch().count();

                auto it = m_fileTimestamps.find(path);
                if (it == m_fileTimestamps.end()) {
                    // New file
                    newFiles.push_back(path);
                    m_fileTimestamps[path] = timestamp;
                } else if (it->second != timestamp) {
                    // Modified file
                    modifiedFiles.push_back(path);
                    m_fileTimestamps[path] = timestamp;
                }
            }
        } catch (...) {}

        // Check for deleted files
        for (const auto& [path, timestamp] : m_fileTimestamps) {
            if (currentFiles.find(path) == currentFiles.end()) {
                deletedFiles.push_back(path);
            }
        }
    }

    // Process changes outside the lock
    for (const auto& path : newFiles) {
        if (LoadFile(path)) {
            ++changes;
        }
    }

    for (const auto& path : modifiedFiles) {
        if (LoadFile(path)) {
            ++changes;
        }
    }

    for (const auto& path : deletedFiles) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto pathIt = m_pathToId.find(path);
        if (pathIt != m_pathToId.end()) {
            std::string id = pathIt->second;

            // Remove from indices
            auto configIt = m_configs.find(id);
            if (configIt != m_configs.end()) {
                std::string type = configIt->second->GetConfigType();
                m_typeIndex[type].erase(id);

                for (const auto& tag : configIt->second->GetTags()) {
                    m_tagIndex[tag].erase(id);
                }
            }

            // Notify
            ConfigChangeEvent event;
            event.changeType = ConfigChangeEvent::Type::Removed;
            event.configId = id;
            event.configPath = path;
            NotifyChange(event);

            m_configs.erase(id);
            m_pathToId.erase(path);
            m_fileTimestamps.erase(path);

            ++changes;
        }
    }

    return changes;
}

std::shared_ptr<EntityConfig> ConfigRegistry::Get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_configs.find(id);
    return (it != m_configs.end()) ? it->second : nullptr;
}

bool ConfigRegistry::Has(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_configs.find(id) != m_configs.end();
}

std::vector<std::shared_ptr<EntityConfig>> ConfigRegistry::GetByType(
    const std::string& type) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<EntityConfig>> result;

    auto it = m_typeIndex.find(type);
    if (it != m_typeIndex.end()) {
        for (const auto& id : it->second) {
            auto configIt = m_configs.find(id);
            if (configIt != m_configs.end()) {
                result.push_back(configIt->second);
            }
        }
    }

    return result;
}

std::vector<std::shared_ptr<EntityConfig>> ConfigRegistry::Query(
    const ConfigQuery& query) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<EntityConfig>> result;

    // Start with all configs or filtered by type
    std::unordered_set<std::string> candidates;
    if (!query.type.empty()) {
        auto it = m_typeIndex.find(query.type);
        if (it != m_typeIndex.end()) {
            candidates = it->second;
        }
    } else {
        for (const auto& [id, _] : m_configs) {
            candidates.insert(id);
        }
    }

    // Filter by tags (AND)
    for (const auto& tag : query.tags) {
        auto it = m_tagIndex.find(tag);
        if (it == m_tagIndex.end()) {
            candidates.clear();
            break;
        }

        std::unordered_set<std::string> intersection;
        for (const auto& id : candidates) {
            if (it->second.find(id) != it->second.end()) {
                intersection.insert(id);
            }
        }
        candidates = std::move(intersection);
    }

    // Filter by anyTags (OR)
    if (!query.anyTags.empty()) {
        std::unordered_set<std::string> matching;
        for (const auto& tag : query.anyTags) {
            auto it = m_tagIndex.find(tag);
            if (it != m_tagIndex.end()) {
                for (const auto& id : it->second) {
                    if (candidates.find(id) != candidates.end()) {
                        matching.insert(id);
                    }
                }
            }
        }
        candidates = std::move(matching);
    }

    // Apply regex filters
    std::regex nameRegex, idRegex;
    bool hasNamePattern = !query.namePattern.empty();
    bool hasIdPattern = !query.idPattern.empty();

    if (hasNamePattern) {
        try {
            nameRegex = std::regex(query.namePattern, std::regex::icase);
        } catch (...) {
            hasNamePattern = false;
        }
    }

    if (hasIdPattern) {
        try {
            idRegex = std::regex(query.idPattern, std::regex::icase);
        } catch (...) {
            hasIdPattern = false;
        }
    }

    for (const auto& id : candidates) {
        auto it = m_configs.find(id);
        if (it == m_configs.end()) continue;

        const auto& config = it->second;

        if (hasIdPattern && !std::regex_search(id, idRegex)) {
            continue;
        }

        if (hasNamePattern && !std::regex_search(config->GetName(), nameRegex)) {
            continue;
        }

        result.push_back(config);
    }

    return result;
}

std::vector<std::shared_ptr<EntityConfig>> ConfigRegistry::GetByTag(
    const std::string& tag) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<EntityConfig>> result;

    auto it = m_tagIndex.find(tag);
    if (it != m_tagIndex.end()) {
        for (const auto& id : it->second) {
            auto configIt = m_configs.find(id);
            if (configIt != m_configs.end()) {
                result.push_back(configIt->second);
            }
        }
    }

    return result;
}

std::vector<std::string> ConfigRegistry::GetAllIds() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> ids;
    ids.reserve(m_configs.size());
    for (const auto& [id, _] : m_configs) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<std::string> ConfigRegistry::GetAllTypes() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> types;
    types.reserve(m_typeIndex.size());
    for (const auto& [type, _] : m_typeIndex) {
        types.push_back(type);
    }
    return types;
}

std::vector<std::shared_ptr<UnitConfig>> ConfigRegistry::GetAllUnits() const {
    auto configs = GetByType("unit");
    std::vector<std::shared_ptr<UnitConfig>> units;
    units.reserve(configs.size());
    for (const auto& config : configs) {
        if (auto unit = std::dynamic_pointer_cast<UnitConfig>(config)) {
            units.push_back(unit);
        }
    }
    return units;
}

std::vector<std::shared_ptr<BuildingConfig>> ConfigRegistry::GetAllBuildings() const {
    auto configs = GetByType("building");
    std::vector<std::shared_ptr<BuildingConfig>> buildings;
    buildings.reserve(configs.size());
    for (const auto& config : configs) {
        if (auto building = std::dynamic_pointer_cast<BuildingConfig>(config)) {
            buildings.push_back(building);
        }
    }
    return buildings;
}

std::vector<std::shared_ptr<TileConfig>> ConfigRegistry::GetAllTiles() const {
    auto configs = GetByType("tile");
    std::vector<std::shared_ptr<TileConfig>> tiles;
    tiles.reserve(configs.size());
    for (const auto& config : configs) {
        if (auto tile = std::dynamic_pointer_cast<TileConfig>(config)) {
            tiles.push_back(tile);
        }
    }
    return tiles;
}

std::unordered_map<std::string, ValidationResult> ConfigRegistry::ValidateAll() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::unordered_map<std::string, ValidationResult> results;

    for (const auto& [id, config] : m_configs) {
        results[id] = config->Validate();
    }

    return results;
}

ValidationResult ConfigRegistry::ValidateConfig(const std::string& id) const {
    auto config = Get(id);
    if (!config) {
        ValidationResult result;
        result.AddError("", "Config not found: " + id);
        return result;
    }
    return config->Validate();
}

std::vector<std::string> ConfigRegistry::FindCircularDependencies() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> circular;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> inStack;

    std::function<bool(const std::string&)> detectCycle = [&](const std::string& id) -> bool {
        if (inStack.find(id) != inStack.end()) {
            return true;  // Cycle detected
        }

        if (visited.find(id) != visited.end()) {
            return false;  // Already processed
        }

        visited.insert(id);
        inStack.insert(id);

        auto it = m_configs.find(id);
        if (it != m_configs.end() && it->second->HasBaseConfig()) {
            if (detectCycle(it->second->GetBaseConfigId())) {
                circular.push_back(id);
                return true;
            }
        }

        inStack.erase(id);
        return false;
    };

    for (const auto& [id, _] : m_configs) {
        if (visited.find(id) == visited.end()) {
            detectCycle(id);
        }
    }

    return circular;
}

bool ConfigRegistry::Register(std::shared_ptr<EntityConfig> config) {
    if (!config || config->GetId().empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    std::string id = config->GetId();
    if (m_configs.find(id) != m_configs.end()) {
        return false;  // Already exists
    }

    m_configs[id] = config;

    // Update indices
    std::string type = config->GetConfigType();
    m_typeIndex[type].insert(id);

    for (const auto& tag : config->GetTags()) {
        m_tagIndex[tag].insert(id);
    }

    // Notify
    ConfigChangeEvent event;
    event.changeType = ConfigChangeEvent::Type::Added;
    event.configId = id;
    event.config = config;
    NotifyChange(event);

    return true;
}

bool ConfigRegistry::Unregister(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_configs.find(id);
    if (it == m_configs.end()) {
        return false;
    }

    // Remove from indices
    std::string type = it->second->GetConfigType();
    m_typeIndex[type].erase(id);

    for (const auto& tag : it->second->GetTags()) {
        m_tagIndex[tag].erase(id);
    }

    // Notify
    ConfigChangeEvent event;
    event.changeType = ConfigChangeEvent::Type::Removed;
    event.configId = id;
    NotifyChange(event);

    m_configs.erase(it);

    return true;
}

int ConfigRegistry::Subscribe(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int id = m_nextSubscriberId++;
    m_subscribers[id] = std::move(callback);
    return id;
}

void ConfigRegistry::Unsubscribe(int subscriptionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subscribers.erase(subscriptionId);
}

void ConfigRegistry::ResolveInheritance() {
    std::unordered_set<std::string> visited;

    for (const auto& [id, _] : m_configs) {
        ResolveInheritanceFor(id, visited);
    }
}

std::vector<std::string> ConfigRegistry::GetInheritanceChain(
    const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> chain;
    std::unordered_set<std::string> seen;
    std::string current = id;

    while (!current.empty() && seen.find(current) == seen.end()) {
        seen.insert(current);
        chain.push_back(current);

        auto it = m_configs.find(current);
        if (it != m_configs.end() && it->second->HasBaseConfig()) {
            current = it->second->GetBaseConfigId();
        } else {
            break;
        }
    }

    std::reverse(chain.begin(), chain.end());
    return chain;
}

void ConfigRegistry::RegisterSchema(const ConfigSchemaDefinition& schema) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_schemas[schema.id] = schema;
}

const ConfigSchemaDefinition* ConfigRegistry::GetSchema(
    const std::string& schemaId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_schemas.find(schemaId);
    return (it != m_schemas.end()) ? &it->second : nullptr;
}

std::shared_ptr<EntityConfig> ConfigRegistry::CreateConfigForFile(
    const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Strip comments
    std::string cleanJson = EntityConfig::StripComments(content);

    // Determine type
    std::string type = DetermineConfigType(cleanJson);

    // Create appropriate config type
    auto config = EntityConfigFactory::Instance().Create(type);
    if (!config) {
        config = std::make_unique<EntityConfig>();
    }

    if (config->LoadFromString(content)) {
        return config;
    }

    return nullptr;
}

std::string ConfigRegistry::DetermineConfigType(const std::string& jsonContent) {
    try {
        json j = json::parse(jsonContent);

        if (j.contains("type")) {
            return j["type"].get<std::string>();
        }

        // Infer from content
        if (j.contains("movement") || j.contains("combat") || j.contains("abilities")) {
            return "unit";
        }
        if (j.contains("footprint") || j.contains("production") || j.contains("garrison")) {
            return "building";
        }
        if (j.contains("walkable") || j.contains("movementCost") || j.contains("transitions")) {
            return "tile";
        }

    } catch (...) {}

    return "entity";
}

void ConfigRegistry::NotifyChange(const ConfigChangeEvent& event) {
    // Copy subscribers to avoid holding lock during callbacks
    std::vector<ConfigChangeCallback> callbacks;
    {
        for (const auto& [_, callback] : m_subscribers) {
            callbacks.push_back(callback);
        }
    }

    for (const auto& callback : callbacks) {
        try {
            callback(event);
        } catch (...) {
            // Ignore callback errors
        }
    }
}

bool ConfigRegistry::ResolveInheritanceFor(const std::string& id,
                                           std::unordered_set<std::string>& visited) {
    if (visited.find(id) != visited.end()) {
        return true;  // Already resolved or circular
    }

    auto it = m_configs.find(id);
    if (it == m_configs.end()) {
        return false;
    }

    auto& config = it->second;
    if (!config->HasBaseConfig()) {
        visited.insert(id);
        return true;
    }

    std::string baseId = config->GetBaseConfigId();

    // Check for circular dependency
    if (visited.find(baseId) != visited.end()) {
        return false;
    }

    // Resolve base first
    auto baseIt = m_configs.find(baseId);
    if (baseIt == m_configs.end()) {
        // Base config not found
        visited.insert(id);
        return true;
    }

    // Recursively resolve base
    if (!ResolveInheritanceFor(baseId, visited)) {
        return false;
    }

    // Apply base config
    config->ApplyBaseConfig(*baseIt->second);
    visited.insert(id);

    return true;
}

} // namespace Config
} // namespace Vehement
