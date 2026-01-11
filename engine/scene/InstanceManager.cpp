#include "InstanceManager.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

namespace Nova {

bool InstanceManager::Initialize(const std::string& archetypeDirectory, const std::string& instanceDirectory) {
    m_archetypeDirectory = archetypeDirectory;
    m_instanceDirectory = instanceDirectory;

    // Create directories if they don't exist
    try {
        std::filesystem::create_directories(archetypeDirectory);
        std::filesystem::create_directories(instanceDirectory);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create directories: {}", e.what());
        return false;
    }

    spdlog::info("InstanceManager initialized");
    spdlog::info("  Archetype directory: {}", m_archetypeDirectory);
    spdlog::info("  Instance directory: {}", m_instanceDirectory);

    return true;
}

InstanceData InstanceManager::LoadInstance(const std::string& path) {
    InstanceData instance = InstanceData::LoadFromFile(path);

    if (!instance.instanceId.empty()) {
        RegisterInstance(instance);
    }

    return instance;
}

bool InstanceManager::SaveInstance(const std::string& path, const InstanceData& instance) {
    // Create parent directory if needed
    try {
        std::filesystem::path filePath(path);
        std::filesystem::create_directories(filePath.parent_path());
    } catch (const std::exception& e) {
        spdlog::error("Failed to create directory for instance: {}", e.what());
        return false;
    }

    bool success = instance.SaveToFile(path);

    if (success) {
        // Update in memory and mark as clean
        auto it = m_instances.find(instance.instanceId);
        if (it != m_instances.end()) {
            it->second = instance;
            it->second.isDirty = false;
        }
        m_dirtyInstances.erase(instance.instanceId);
    }

    return success;
}

bool InstanceManager::SaveInstanceToMap(const std::string& mapName, const InstanceData& instance) {
    std::string path = GetInstancePath(mapName, instance.instanceId);
    return SaveInstance(path, instance);
}

std::vector<InstanceData> InstanceManager::LoadMapInstances(const std::string& mapName) {
    std::vector<InstanceData> instances;

    std::string mapInstanceDir = m_instanceDirectory + mapName + "/instances/";

    if (!std::filesystem::exists(mapInstanceDir)) {
        spdlog::info("No instance directory for map: {}", mapName);
        return instances;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(mapInstanceDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                InstanceData instance = LoadInstance(entry.path().string());
                if (!instance.instanceId.empty()) {
                    instances.push_back(instance);
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to load map instances: {}", e.what());
    }

    spdlog::info("Loaded {} instances for map: {}", instances.size(), mapName);
    return instances;
}

nlohmann::json InstanceManager::LoadArchetype(const std::string& archetypeId) {
    // Check cache first
    auto it = m_archetypeCache.find(archetypeId);
    if (it != m_archetypeCache.end()) {
        return it->second;
    }

    // Load from file
    std::string path = ArchetypeIdToPath(archetypeId);

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::warn("Archetype file not found: {}", path);
            return nlohmann::json::object();
        }

        nlohmann::json config;
        file >> config;
        file.close();

        // Cache it
        m_archetypeCache[archetypeId] = config;

        spdlog::debug("Loaded archetype: {}", archetypeId);
        return config;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load archetype {}: {}", archetypeId, e.what());
        return nlohmann::json::object();
    }
}

nlohmann::json InstanceManager::ApplyOverrides(const nlohmann::json& baseConfig, const nlohmann::json& overrides) {
    nlohmann::json result = baseConfig;
    MergeJson(result, overrides);
    return result;
}

nlohmann::json InstanceManager::GetEffectiveConfig(const InstanceData& instance) {
    nlohmann::json baseConfig = LoadArchetype(instance.archetypeId);

    if (instance.overrides.empty()) {
        return baseConfig;
    }

    return ApplyOverrides(baseConfig, instance.overrides);
}

void InstanceManager::RegisterInstance(const InstanceData& instance) {
    if (instance.instanceId.empty()) {
        spdlog::warn("Cannot register instance with empty ID");
        return;
    }

    m_instances[instance.instanceId] = instance;

    if (instance.isDirty) {
        m_dirtyInstances.insert(instance.instanceId);
    }

    spdlog::debug("Registered instance: {}", instance.instanceId);
}

void InstanceManager::UnregisterInstance(const std::string& instanceId) {
    m_instances.erase(instanceId);
    m_dirtyInstances.erase(instanceId);
}

InstanceData* InstanceManager::GetInstance(const std::string& instanceId) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        return &it->second;
    }
    return nullptr;
}

void InstanceManager::MarkDirty(const std::string& instanceId) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.isDirty = true;
        m_dirtyInstances.insert(instanceId);
    }
}

bool InstanceManager::IsDirty(const std::string& instanceId) const {
    return m_dirtyInstances.find(instanceId) != m_dirtyInstances.end();
}

std::vector<std::string> InstanceManager::GetDirtyInstances() const {
    return std::vector<std::string>(m_dirtyInstances.begin(), m_dirtyInstances.end());
}

int InstanceManager::SaveDirtyInstances(const std::string& mapName) {
    int savedCount = 0;

    for (const auto& instanceId : m_dirtyInstances) {
        auto it = m_instances.find(instanceId);
        if (it != m_instances.end()) {
            if (SaveInstanceToMap(mapName, it->second)) {
                savedCount++;
            }
        }
    }

    spdlog::info("Saved {} dirty instances", savedCount);
    return savedCount;
}

InstanceData InstanceManager::CreateInstance(const std::string& archetypeId, const glm::vec3& position) {
    InstanceData instance(archetypeId);
    instance.instanceId = InstanceData::GenerateInstanceId();
    instance.position = position;
    instance.isDirty = true;

    // Load archetype to get default name
    nlohmann::json archetype = LoadArchetype(archetypeId);
    if (archetype.contains("name")) {
        instance.name = archetype["name"].get<std::string>();
    } else {
        instance.name = archetypeId;
    }

    RegisterInstance(instance);
    return instance;
}

std::vector<std::string> InstanceManager::ListArchetypes() const {
    std::vector<std::string> archetypes;

    if (!std::filesystem::exists(m_archetypeDirectory)) {
        return archetypes;
    }

    try {
        // Recursively scan archetype directory
        for (const auto& entry : std::filesystem::recursive_directory_iterator(m_archetypeDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                // Convert path to archetype ID
                std::string relativePath = std::filesystem::relative(entry.path(), m_archetypeDirectory).string();

                // Remove extension and convert path separators to dots
                relativePath = relativePath.substr(0, relativePath.length() - 5);  // Remove .json

                // Replace backslashes with forward slashes, then with dots
                for (char& c : relativePath) {
                    if (c == '\\' || c == '/') {
                        c = '.';
                    }
                }

                archetypes.push_back(relativePath);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to list archetypes: {}", e.what());
    }

    return archetypes;
}

std::string InstanceManager::ArchetypeIdToPath(const std::string& archetypeId) const {
    // Convert "humans.units.footman" to "humans/units/footman.json"
    std::string path = m_archetypeDirectory;

    for (char c : archetypeId) {
        if (c == '.') {
            path += '/';
        } else {
            path += c;
        }
    }

    path += ".json";
    return path;
}

std::string InstanceManager::GetInstancePath(const std::string& mapName, const std::string& instanceId) const {
    return m_instanceDirectory + mapName + "/instances/" + instanceId + ".json";
}

void InstanceManager::MergeJson(nlohmann::json& target, const nlohmann::json& source) {
    if (!source.is_object()) {
        return;
    }

    for (auto it = source.begin(); it != source.end(); ++it) {
        const std::string& key = it.key();
        const nlohmann::json& value = it.value();

        if (value.is_object() && target.contains(key) && target[key].is_object()) {
            // Recursively merge objects
            MergeJson(target[key], value);
        } else {
            // Override value
            target[key] = value;
        }
    }
}

} // namespace Nova
