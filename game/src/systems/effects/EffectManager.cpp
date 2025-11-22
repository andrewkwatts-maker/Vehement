#include "EffectManager.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Vehement {
namespace Effects {

// ============================================================================
// Global Instance
// ============================================================================

static EffectManager* s_globalManager = nullptr;

EffectManager& GetEffectManager() {
    static EffectManager defaultManager;
    return s_globalManager ? *s_globalManager : defaultManager;
}

void SetEffectManager(EffectManager* manager) {
    s_globalManager = manager;
}

// ============================================================================
// Effect Manager
// ============================================================================

EffectManager::EffectManager()
    : m_instancePool(128) {
}

EffectManager::~EffectManager() {
    Shutdown();
}

bool EffectManager::Initialize(const EffectManagerConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    // Set up aura manager
    m_auraManager.SetEffectManager(this);

    // Load effects from default path
    if (!config.effectsPath.empty()) {
        LoadEffectsFromDirectory(config.effectsPath);
    }

    m_initialized = true;
    return true;
}

void EffectManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_containers.clear();
    m_definitions.clear();
    m_definitionFilePaths.clear();
    m_fileModificationTimes.clear();

    m_initialized = false;
}

// -------------------------------------------------------------------------
// Loading
// -------------------------------------------------------------------------

int EffectManager::LoadEffectsFromDirectory(const std::string& path) {
    int loaded = 0;

    try {
        std::filesystem::path dirPath(path);
        if (!std::filesystem::exists(dirPath)) {
            if (m_config.logWarnings) {
                // Log warning: directory does not exist
            }
            return 0;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".json") {
                    if (LoadEffectFromFile(entry.path().string())) {
                        loaded++;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        if (m_config.logWarnings) {
            // Log error
        }
    }

    m_statistics.definitionsLoaded = m_definitions.size();
    return loaded;
}

bool EffectManager::LoadEffectFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return LoadEffectFromJson(content, filePath);
}

bool EffectManager::LoadEffectFromJson(const std::string& jsonStr, const std::string& sourcePath) {
    auto definition = std::make_unique<EffectDefinition>();

    if (!definition->LoadFromJson(jsonStr)) {
        return false;
    }

    std::string effectId = definition->GetId();
    if (effectId.empty()) {
        if (m_config.logWarnings) {
            // Log warning: effect has no ID
        }
        return false;
    }

    // Validate if configured
    if (m_config.strictValidation) {
        auto errors = definition->Validate();
        if (!errors.empty()) {
            if (m_config.logWarnings) {
                // Log validation errors
            }
            return false;
        }
    }

    // Track file for hot reload
    if (!sourcePath.empty()) {
        m_definitionFilePaths[effectId] = sourcePath;

        try {
            auto ftime = std::filesystem::last_write_time(sourcePath);
            m_fileModificationTimes[effectId] = ftime.time_since_epoch().count();
        } catch (...) {}
    }

    // Store definition
    m_definitions[effectId] = std::move(definition);
    m_statistics.definitionsLoaded = m_definitions.size();

    return true;
}

bool EffectManager::RegisterDefinition(std::unique_ptr<EffectDefinition> definition) {
    if (!definition) {
        return false;
    }

    std::string effectId = definition->GetId();
    if (effectId.empty()) {
        return false;
    }

    m_definitions[effectId] = std::move(definition);
    m_statistics.definitionsLoaded = m_definitions.size();

    return true;
}

bool EffectManager::UnloadEffect(const std::string& effectId) {
    auto it = m_definitions.find(effectId);
    if (it == m_definitions.end()) {
        return false;
    }

    m_definitions.erase(it);
    m_definitionFilePaths.erase(effectId);
    m_fileModificationTimes.erase(effectId);

    m_statistics.definitionsLoaded = m_definitions.size();
    return true;
}

void EffectManager::UnloadAll() {
    m_definitions.clear();
    m_definitionFilePaths.clear();
    m_fileModificationTimes.clear();
    m_statistics.definitionsLoaded = 0;
}

// -------------------------------------------------------------------------
// Hot Reload
// -------------------------------------------------------------------------

void EffectManager::UpdateHotReload(float deltaTime) {
    if (!m_config.enableHotReload) {
        return;
    }

    m_hotReloadTimer += deltaTime;
    if (m_hotReloadTimer >= m_config.hotReloadCheckInterval) {
        m_hotReloadTimer = 0.0f;
        CheckHotReload();
    }
}

void EffectManager::CheckHotReload() {
    for (const auto& [effectId, filePath] : m_definitionFilePaths) {
        try {
            auto ftime = std::filesystem::last_write_time(filePath);
            int64_t currentTime = ftime.time_since_epoch().count();

            auto storedIt = m_fileModificationTimes.find(effectId);
            if (storedIt != m_fileModificationTimes.end()) {
                if (currentTime != storedIt->second) {
                    ProcessReload(effectId, filePath);
                }
            }
        } catch (...) {
            // File might be deleted or locked
        }
    }
}

void EffectManager::ProcessReload(const std::string& effectId, const std::string& filePath) {
    // Store old definition in case reload fails
    auto oldIt = m_definitions.find(effectId);
    std::unique_ptr<EffectDefinition> oldDef;
    if (oldIt != m_definitions.end()) {
        oldDef = std::move(oldIt->second);
        m_definitions.erase(oldIt);
    }

    // Try to load new definition
    if (LoadEffectFromFile(filePath)) {
        m_statistics.reloadsPerformed++;

        if (m_onEffectReloaded) {
            m_onEffectReloaded(effectId);
        }
    } else {
        // Restore old definition
        if (oldDef) {
            m_definitions[effectId] = std::move(oldDef);
        }
    }
}

bool EffectManager::ReloadEffect(const std::string& effectId) {
    auto pathIt = m_definitionFilePaths.find(effectId);
    if (pathIt == m_definitionFilePaths.end()) {
        return false;
    }

    ProcessReload(effectId, pathIt->second);
    return true;
}

int EffectManager::ReloadAll() {
    int reloaded = 0;
    for (const auto& [effectId, filePath] : m_definitionFilePaths) {
        if (ReloadEffect(effectId)) {
            reloaded++;
        }
    }
    return reloaded;
}

// -------------------------------------------------------------------------
// Definition Access
// -------------------------------------------------------------------------

const EffectDefinition* EffectManager::GetDefinition(const std::string& effectId) const {
    auto it = m_definitions.find(effectId);
    return (it != m_definitions.end()) ? it->second.get() : nullptr;
}

bool EffectManager::HasEffect(const std::string& effectId) const {
    return m_definitions.find(effectId) != m_definitions.end();
}

std::vector<std::string> EffectManager::GetAllEffectIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_definitions.size());
    for (const auto& [id, def] : m_definitions) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<const EffectDefinition*> EffectManager::GetEffectsByType(EffectType type) const {
    std::vector<const EffectDefinition*> result;
    for (const auto& [id, def] : m_definitions) {
        if (def->GetType() == type) {
            result.push_back(def.get());
        }
    }
    return result;
}

std::vector<const EffectDefinition*> EffectManager::GetEffectsByTag(const std::string& tag) const {
    std::vector<const EffectDefinition*> result;
    for (const auto& [id, def] : m_definitions) {
        if (def->HasTag(tag)) {
            result.push_back(def.get());
        }
    }
    return result;
}

std::vector<const EffectDefinition*> EffectManager::QueryEffects(
    std::function<bool(const EffectDefinition*)> predicate
) const {
    std::vector<const EffectDefinition*> result;
    for (const auto& [id, def] : m_definitions) {
        if (predicate(def.get())) {
            result.push_back(def.get());
        }
    }
    return result;
}

// -------------------------------------------------------------------------
// Instance Creation
// -------------------------------------------------------------------------

std::unique_ptr<EffectInstance> EffectManager::CreateInstance(const std::string& effectId) {
    const EffectDefinition* def = GetDefinition(effectId);
    if (!def) {
        return nullptr;
    }
    return CreateInstance(def);
}

std::unique_ptr<EffectInstance> EffectManager::CreateInstance(const EffectDefinition* definition) {
    if (!definition) {
        return nullptr;
    }

    auto instance = m_instancePool.Acquire();
    instance->Initialize(definition);

    m_statistics.instancesCreated++;
    return instance;
}

// -------------------------------------------------------------------------
// Container Management
// -------------------------------------------------------------------------

std::unique_ptr<EffectContainer> EffectManager::CreateContainer(uint32_t ownerId) {
    auto container = std::make_unique<EffectContainer>(ownerId);
    container->SetEffectManager(this);
    return container;
}

void EffectManager::RegisterContainer(EffectContainer* container) {
    if (container) {
        m_containers[container->GetOwnerId()] = container;
    }
}

void EffectManager::UnregisterContainer(EffectContainer* container) {
    if (container) {
        m_containers.erase(container->GetOwnerId());
    }
}

EffectContainer* EffectManager::GetContainer(uint32_t ownerId) const {
    auto it = m_containers.find(ownerId);
    return (it != m_containers.end()) ? it->second : nullptr;
}

// -------------------------------------------------------------------------
// Global Operations
// -------------------------------------------------------------------------

void EffectManager::Update(float deltaTime) {
    // Update hot reload
    UpdateHotReload(deltaTime);

    // Update all registered containers
    for (auto& [ownerId, container] : m_containers) {
        container->Update(deltaTime);
    }
}

EffectApplicationResult EffectManager::ApplyEffect(
    const std::string& effectId,
    uint32_t sourceId,
    uint32_t targetId
) {
    EffectContainer* container = GetContainer(targetId);
    if (!container) {
        EffectApplicationResult result;
        result.status = EffectApplicationResult::Status::Failed;
        result.message = "Target container not found";
        return result;
    }

    return container->ApplyEffect(effectId, sourceId);
}

bool EffectManager::RemoveEffect(const std::string& effectId, uint32_t targetId) {
    EffectContainer* container = GetContainer(targetId);
    if (!container) {
        return false;
    }

    return container->RemoveEffectById(effectId) > 0;
}

void EffectManager::ProcessGlobalTrigger(const TriggerEventData& eventData) {
    for (auto& [ownerId, container] : m_containers) {
        container->ProcessTriggers(eventData);
    }
}

// -------------------------------------------------------------------------
// Statistics
// -------------------------------------------------------------------------

size_t EffectManager::GetActiveInstanceCount() const {
    size_t count = 0;
    for (const auto& [ownerId, container] : m_containers) {
        count += container->GetEffectCount();
    }
    return count;
}

EffectManager::Statistics EffectManager::GetStatistics() const {
    m_statistics.definitionsLoaded = m_definitions.size();
    m_statistics.activeInstances = GetActiveInstanceCount();
    m_statistics.activeContainers = m_containers.size();
    // m_statistics.activeAuras would come from aura manager
    return m_statistics;
}

// -------------------------------------------------------------------------
// Validation
// -------------------------------------------------------------------------

std::unordered_map<std::string, std::vector<std::string>> EffectManager::ValidateAll() const {
    std::unordered_map<std::string, std::vector<std::string>> errors;

    for (const auto& [id, def] : m_definitions) {
        auto defErrors = def->Validate();
        if (!defErrors.empty()) {
            errors[id] = defErrors;
        }
    }

    return errors;
}

std::vector<std::string> EffectManager::FindMissingReferences() const {
    std::vector<std::string> missing;

    for (const auto& [id, def] : m_definitions) {
        // Check triggers that reference other effects
        for (const auto& trigger : def->GetTriggers()) {
            if (!trigger.effectId.empty() && !HasEffect(trigger.effectId)) {
                missing.push_back("Effect '" + id + "' references missing effect '" +
                                  trigger.effectId + "' in trigger");
            }
        }
    }

    return missing;
}

} // namespace Effects
} // namespace Vehement
