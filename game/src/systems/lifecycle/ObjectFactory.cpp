#include "ObjectFactory.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <sys/stat.h>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// JsonConfigLoader Implementation
// ============================================================================

bool JsonConfigLoader::Load(const std::string& json, ObjectDefinition& outDef) {
    // Simple JSON parsing (in production, use nlohmann::json)
    // This is a basic implementation for demonstration

    outDef.rawJson = std::make_shared<std::string>(json);

    // Extract "id" field
    size_t idPos = json.find("\"id\"");
    if (idPos != std::string::npos) {
        size_t colonPos = json.find(':', idPos);
        size_t quoteStart = json.find('"', colonPos);
        size_t quoteEnd = json.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            outDef.id = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }

    // Extract "type" field
    size_t typePos = json.find("\"type\"");
    if (typePos != std::string::npos) {
        size_t colonPos = json.find(':', typePos);
        size_t quoteStart = json.find('"', colonPos);
        size_t quoteEnd = json.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            outDef.type = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }

    // Extract "extends" / "base" field for inheritance
    size_t basePos = json.find("\"extends\"");
    if (basePos == std::string::npos) {
        basePos = json.find("\"base\"");
    }
    if (basePos != std::string::npos) {
        size_t colonPos = json.find(':', basePos);
        size_t quoteStart = json.find('"', colonPos);
        size_t quoteEnd = json.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            outDef.baseId = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }

    // Extract lifecycle config
    size_t lifecyclePos = json.find("\"lifecycle\"");
    if (lifecyclePos != std::string::npos) {
        // Extract tick_group
        size_t tickGroupPos = json.find("\"tick_group\"", lifecyclePos);
        if (tickGroupPos != std::string::npos) {
            size_t colonPos = json.find(':', tickGroupPos);
            size_t quoteStart = json.find('"', colonPos);
            size_t quoteEnd = json.find('"', quoteStart + 1);
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                outDef.lifecycle.tickGroup = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            }
        }

        // Extract tick_interval
        size_t tickIntervalPos = json.find("\"tick_interval\"", lifecyclePos);
        if (tickIntervalPos != std::string::npos) {
            size_t colonPos = json.find(':', tickIntervalPos);
            size_t numStart = colonPos + 1;
            while (numStart < json.size() && (json[numStart] == ' ' || json[numStart] == '\t')) {
                numStart++;
            }
            size_t numEnd = numStart;
            while (numEnd < json.size() && (isdigit(json[numEnd]) || json[numEnd] == '.')) {
                numEnd++;
            }
            if (numEnd > numStart) {
                outDef.lifecycle.tickInterval = std::stof(json.substr(numStart, numEnd - numStart));
            }
        }

        // Extract event scripts
        size_t eventsPos = json.find("\"events\"", lifecyclePos);
        if (eventsPos != std::string::npos) {
            // on_create
            size_t onCreatePos = json.find("\"on_create\"", eventsPos);
            if (onCreatePos != std::string::npos) {
                size_t colonPos = json.find(':', onCreatePos);
                size_t quoteStart = json.find('"', colonPos);
                size_t quoteEnd = json.find('"', quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    outDef.lifecycle.events.onCreate = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }

            // on_destroy
            size_t onDestroyPos = json.find("\"on_destroy\"", eventsPos);
            if (onDestroyPos != std::string::npos) {
                size_t colonPos = json.find(':', onDestroyPos);
                size_t quoteStart = json.find('"', colonPos);
                size_t quoteEnd = json.find('"', quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    outDef.lifecycle.events.onDestroy = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }

            // on_damaged
            size_t onDamagedPos = json.find("\"on_damaged\"", eventsPos);
            if (onDamagedPos != std::string::npos) {
                size_t colonPos = json.find(':', onDamagedPos);
                size_t quoteStart = json.find('"', colonPos);
                size_t quoteEnd = json.find('"', quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    outDef.lifecycle.events.onDamaged = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }

            // on_kill
            size_t onKillPos = json.find("\"on_kill\"", eventsPos);
            if (onKillPos != std::string::npos) {
                size_t colonPos = json.find(':', onKillPos);
                size_t quoteStart = json.find('"', colonPos);
                size_t quoteEnd = json.find('"', quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    outDef.lifecycle.events.onKill = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
    }

    // Extract components array
    size_t componentsPos = json.find("\"components\"");
    if (componentsPos != std::string::npos) {
        size_t arrayStart = json.find('[', componentsPos);
        size_t arrayEnd = json.find(']', arrayStart);
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string arrayContent = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

            // Parse component names
            size_t pos = 0;
            while (pos < arrayContent.size()) {
                size_t quoteStart = arrayContent.find('"', pos);
                if (quoteStart == std::string::npos) break;
                size_t quoteEnd = arrayContent.find('"', quoteStart + 1);
                if (quoteEnd == std::string::npos) break;

                outDef.components.push_back(
                    arrayContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
                pos = quoteEnd + 1;
            }
        }
    }

    outDef.isValid = !outDef.id.empty() && !outDef.type.empty();
    return outDef.isValid;
}

bool JsonConfigLoader::Validate(const ObjectDefinition& def,
                               std::vector<std::string>& outErrors) {
    if (def.id.empty()) {
        outErrors.push_back("Definition missing 'id' field");
    }

    if (def.type.empty()) {
        outErrors.push_back("Definition missing 'type' field");
    }

    // Validate tick group
    if (!def.lifecycle.tickGroup.empty()) {
        if (def.lifecycle.tickGroup != "PrePhysics" &&
            def.lifecycle.tickGroup != "Physics" &&
            def.lifecycle.tickGroup != "PostPhysics" &&
            def.lifecycle.tickGroup != "Animation" &&
            def.lifecycle.tickGroup != "AI" &&
            def.lifecycle.tickGroup != "Late") {
            outErrors.push_back("Invalid tick_group: " + def.lifecycle.tickGroup);
        }
    }

    // Validate tick interval
    if (def.lifecycle.tickInterval < 0.0f) {
        outErrors.push_back("tick_interval cannot be negative");
    }

    return outErrors.empty();
}

// ============================================================================
// Prototype Implementation
// ============================================================================

std::unique_ptr<ILifecycle> Prototype::Clone() const {
    // In a real implementation, this would deep-clone the object
    // For now, we return nullptr - requires type-specific clone support
    return nullptr;
}

// ============================================================================
// ObjectFactory Implementation
// ============================================================================

ObjectFactory::ObjectFactory() {
    m_defaultLoader = std::make_unique<JsonConfigLoader>();
}

ObjectFactory::~ObjectFactory() = default;

void ObjectFactory::RegisterType(const std::string& typeName, CreatorFunc creator) {
    std::lock_guard<std::mutex> lock(m_mutex);

    TypeInfo info;
    info.creator = std::move(creator);
    m_types[typeName] = std::move(info);

    m_stats.registeredTypes = m_types.size();
}

void ObjectFactory::RegisterConfigLoader(const std::string& typeName,
                                        std::unique_ptr<IConfigLoader> loader) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_types.find(typeName);
    if (it != m_types.end()) {
        it->second.loader = std::move(loader);
    }
}

bool ObjectFactory::IsTypeRegistered(const std::string& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_types.find(typeName) != m_types.end();
}

std::vector<std::string> ObjectFactory::GetRegisteredTypes() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> types;
    types.reserve(m_types.size());
    for (const auto& pair : m_types) {
        types.push_back(pair.first);
    }
    return types;
}

bool ObjectFactory::LoadDefinition(const std::string& filePath) {
    // Read file
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Parse
    ObjectDefinition def;
    def.sourcePath = filePath;
    def.lastModified = GetFileModTime(filePath);

    IConfigLoader* loader = m_defaultLoader.get();
    if (!loader->Load(content, def)) {
        return false;
    }

    // Validate
    std::vector<std::string> errors;
    if (!loader->Validate(def, errors)) {
        def.validationErrors = errors;
        def.isValid = false;
    }

    // Resolve inheritance
    if (!def.baseId.empty()) {
        ResolveInheritance(def);
    }

    // Store
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_definitions[def.id] = std::move(def);
        m_fileModTimes[filePath] = def.lastModified;
        m_stats.loadedDefinitions = m_definitions.size();
    }

    return true;
}

bool ObjectFactory::LoadDefinitionFromString(const std::string& id,
                                            const std::string& json) {
    ObjectDefinition def;
    def.id = id;

    if (!m_defaultLoader->Load(json, def)) {
        return false;
    }

    // Override ID if provided
    if (!id.empty()) {
        def.id = id;
    }

    // Resolve inheritance
    if (!def.baseId.empty()) {
        ResolveInheritance(def);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_definitions[def.id] = std::move(def);
        m_stats.loadedDefinitions = m_definitions.size();
    }

    return true;
}

size_t ObjectFactory::LoadDefinitionsFromDirectory(const std::string& dirPath,
                                                   bool recursive) {
    size_t loaded = 0;

    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().string();
                    if (ext == ".json" || ext == ".JSON") {
                        if (LoadDefinition(entry.path().string())) {
                            loaded++;
                        }
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().string();
                    if (ext == ".json" || ext == ".JSON") {
                        if (LoadDefinition(entry.path().string())) {
                            loaded++;
                        }
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // Directory doesn't exist or permission denied
        (void)e;
    }

    return loaded;
}

void ObjectFactory::UnloadDefinition(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_definitions.find(id);
    if (it != m_definitions.end()) {
        // Remove from file mod times
        if (!it->second.sourcePath.empty()) {
            m_fileModTimes.erase(it->second.sourcePath);
        }

        m_definitions.erase(it);
        m_stats.loadedDefinitions = m_definitions.size();
    }
}

void ObjectFactory::UnloadAllDefinitions() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_definitions.clear();
    m_fileModTimes.clear();
    m_stats.loadedDefinitions = 0;
}

const ObjectDefinition* ObjectFactory::GetDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_definitions.find(id);
    return it != m_definitions.end() ? &it->second : nullptr;
}

std::vector<std::string> ObjectFactory::GetDefinitionIds() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> ids;
    ids.reserve(m_definitions.size());
    for (const auto& pair : m_definitions) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<const ObjectDefinition*> ObjectFactory::GetDefinitionsByType(
    const std::string& typeName) const {

    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<const ObjectDefinition*> result;
    for (const auto& pair : m_definitions) {
        if (pair.second.type == typeName) {
            result.push_back(&pair.second);
        }
    }
    return result;
}

std::unique_ptr<ILifecycle> ObjectFactory::CreateFromDefinition(
    const std::string& definitionId,
    const nlohmann::json* configOverride) {

    std::lock_guard<std::mutex> lock(m_mutex);

    // Find definition
    auto defIt = m_definitions.find(definitionId);
    if (defIt == m_definitions.end()) {
        return nullptr;
    }

    const ObjectDefinition& def = defIt->second;

    // Find type creator
    auto typeIt = m_types.find(def.type);
    if (typeIt == m_types.end()) {
        return nullptr;
    }

    // Create object
    auto object = typeIt->second.creator();
    if (!object) {
        return nullptr;
    }

    // Initialize with config
    nlohmann::json config;
    // In production, merge def config with override
    object->OnCreate(config);

    m_stats.objectsCreated++;
    return object;
}

std::unique_ptr<ILifecycle> ObjectFactory::CreateByType(const std::string& typeName,
                                                        const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_types.find(typeName);
    if (it == m_types.end()) {
        return nullptr;
    }

    auto object = it->second.creator();
    if (object) {
        object->OnCreate(config);
        m_stats.objectsCreated++;
    }

    return object;
}

bool ObjectFactory::CreatePrototype(const std::string& definitionId) {
    auto object = CreateFromDefinition(definitionId);
    if (!object) {
        return false;
    }

    auto prototype = std::make_unique<Prototype>();
    prototype->id = definitionId;
    prototype->instance = std::move(object);

    const ObjectDefinition* def = GetDefinition(definitionId);
    if (def) {
        prototype->definition = *def;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_prototypes[definitionId] = std::move(prototype);
        m_stats.prototypes = m_prototypes.size();
    }

    return true;
}

std::unique_ptr<ILifecycle> ObjectFactory::CloneFromPrototype(const std::string& prototypeId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_prototypes.find(prototypeId);
    if (it == m_prototypes.end() || !it->second) {
        return nullptr;
    }

    auto clone = it->second->Clone();
    if (clone) {
        m_stats.clonesCreated++;
    }

    return clone;
}

bool ObjectFactory::HasPrototype(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_prototypes.find(id) != m_prototypes.end();
}

void ObjectFactory::RemovePrototype(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_prototypes.erase(id);
    m_stats.prototypes = m_prototypes.size();
}

void ObjectFactory::ClearPrototypes() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_prototypes.clear();
    m_stats.prototypes = 0;
}

void ObjectFactory::SetHotReloadEnabled(bool enabled, uint32_t pollIntervalMs) {
    m_hotReloadEnabled = enabled;
    m_hotReloadPollMs = pollIntervalMs;
}

size_t ObjectFactory::CheckForReloads() {
    if (!m_hotReloadEnabled) {
        return 0;
    }

    size_t reloaded = 0;
    std::vector<std::string> toReload;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (const auto& pair : m_definitions) {
            const auto& def = pair.second;
            if (def.sourcePath.empty()) continue;

            int64_t currentModTime = GetFileModTime(def.sourcePath);
            auto modIt = m_fileModTimes.find(def.sourcePath);

            if (modIt != m_fileModTimes.end() && currentModTime > modIt->second) {
                toReload.push_back(pair.first);
            }
        }
    }

    // Reload outside of lock
    for (const auto& id : toReload) {
        if (ReloadDefinition(id)) {
            reloaded++;
            m_stats.reloadsPerformed++;

            if (m_onReloaded) {
                m_onReloaded(id);
            }
        }
    }

    return reloaded;
}

bool ObjectFactory::ReloadDefinition(const std::string& id) {
    const ObjectDefinition* def = GetDefinition(id);
    if (!def || def->sourcePath.empty()) {
        return false;
    }

    std::string path = def->sourcePath;
    UnloadDefinition(id);
    return LoadDefinition(path);
}

std::vector<std::string> ObjectFactory::ValidateAllDefinitions() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> allErrors;
    for (const auto& pair : m_definitions) {
        for (const auto& error : pair.second.validationErrors) {
            allErrors.push_back(pair.first + ": " + error);
        }
    }
    return allErrors;
}

std::vector<std::string> ObjectFactory::ValidateDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_definitions.find(id);
    if (it == m_definitions.end()) {
        return { "Definition not found: " + id };
    }

    return it->second.validationErrors;
}

ObjectFactory::Stats ObjectFactory::GetStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

bool ObjectFactory::ResolveInheritance(ObjectDefinition& def) {
    if (def.baseId.empty()) {
        return true;
    }

    auto it = m_definitions.find(def.baseId);
    if (it == m_definitions.end()) {
        def.validationErrors.push_back("Base definition not found: " + def.baseId);
        return false;
    }

    const ObjectDefinition& baseDef = it->second;

    // Inherit type if not specified
    if (def.type.empty()) {
        def.type = baseDef.type;
    }

    // Inherit lifecycle config (if not overridden)
    if (def.lifecycle.tickGroup == "AI" && !baseDef.lifecycle.tickGroup.empty()) {
        def.lifecycle.tickGroup = baseDef.lifecycle.tickGroup;
    }
    if (def.lifecycle.tickInterval == 0.0f) {
        def.lifecycle.tickInterval = baseDef.lifecycle.tickInterval;
    }

    // Merge components (base first, then derived)
    std::vector<std::string> mergedComponents = baseDef.components;
    for (const auto& comp : def.components) {
        if (std::find(mergedComponents.begin(), mergedComponents.end(), comp) ==
            mergedComponents.end()) {
            mergedComponents.push_back(comp);
        }
    }
    def.components = mergedComponents;

    return true;
}

int64_t ObjectFactory::GetFileModTime(const std::string& path) const {
    struct stat statBuf;
    if (stat(path.c_str(), &statBuf) == 0) {
        return static_cast<int64_t>(statBuf.st_mtime);
    }
    return 0;
}

IConfigLoader* ObjectFactory::GetLoader(const std::string& typeName) {
    auto it = m_types.find(typeName);
    if (it != m_types.end() && it->second.loader) {
        return it->second.loader.get();
    }
    return m_defaultLoader.get();
}

// ============================================================================
// Global Factory
// ============================================================================

namespace {
    std::unique_ptr<ObjectFactory> g_globalFactory;
    std::once_flag g_factoryInitFlag;
}

ObjectFactory& GetGlobalObjectFactory() {
    std::call_once(g_factoryInitFlag, []() {
        g_globalFactory = std::make_unique<ObjectFactory>();
    });
    return *g_globalFactory;
}

} // namespace Lifecycle
} // namespace Vehement
