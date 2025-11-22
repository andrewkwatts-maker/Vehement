#include "SpellManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <sys/stat.h>
#include <dirent.h>

namespace fs = std::filesystem;

namespace Vehement {
namespace Spells {

// ============================================================================
// Utility Functions
// ============================================================================

int64_t GetFileModificationTime(const std::string& filePath) {
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == 0) {
        return fileStat.st_mtime;
    }
    return 0;
}

bool FileWasModified(const std::string& filePath, int64_t lastKnownTime) {
    return GetFileModificationTime(filePath) > lastKnownTime;
}

std::vector<std::string> ListJsonFiles(const std::string& directory) {
    std::vector<std::string> files;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                files.push_back(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        // Directory doesn't exist or can't be read
    }

    return files;
}

// ============================================================================
// JSON Schema
// ============================================================================

std::string GetSpellJsonSchema() {
    return R"({
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Spell Definition",
  "type": "object",
  "required": ["id", "name"],
  "properties": {
    "id": {"type": "string", "description": "Unique spell identifier"},
    "name": {"type": "string", "description": "Display name"},
    "description": {"type": "string"},
    "icon": {"type": "string", "description": "Icon path"},
    "school": {"type": "string", "enum": ["fire", "frost", "nature", "arcane", "shadow", "holy", "physical"]},
    "tags": {"type": "array", "items": {"type": "string"}},
    "targeting": {
      "type": "object",
      "properties": {
        "mode": {"type": "string", "enum": ["self", "single", "passive_radius", "aoe", "line", "cone", "projectile", "chain"]},
        "range": {"type": "number", "minimum": 0},
        "min_range": {"type": "number", "minimum": 0},
        "radius": {"type": "number", "minimum": 0},
        "angle": {"type": "number", "minimum": 0, "maximum": 360},
        "width": {"type": "number", "minimum": 0},
        "max_targets": {"type": "integer", "minimum": 1},
        "priority": {"type": "string", "enum": ["nearest", "farthest", "lowest_health", "highest_health", "highest_threat", "random"]}
      }
    },
    "timing": {
      "type": "object",
      "properties": {
        "cast_time": {"type": "number", "minimum": 0},
        "channel_duration": {"type": "number", "minimum": 0},
        "cooldown": {"type": "number", "minimum": 0},
        "charges": {"type": "integer", "minimum": 1},
        "charge_recharge_time": {"type": "number", "minimum": 0}
      }
    },
    "cost": {
      "type": "object",
      "properties": {
        "mana": {"type": "number", "minimum": 0},
        "health": {"type": "number", "minimum": 0},
        "stamina": {"type": "number", "minimum": 0},
        "energy": {"type": "number", "minimum": 0},
        "rage": {"type": "number", "minimum": 0}
      }
    },
    "effects": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["type"],
        "properties": {
          "type": {"type": "string"},
          "amount": {"type": "number"},
          "duration": {"type": "number"},
          "tick_interval": {"type": "number"},
          "damage_type": {"type": "string"},
          "scaling": {
            "type": "object",
            "properties": {
              "stat": {"type": "string"},
              "coefficient": {"type": "number"}
            }
          }
        }
      }
    },
    "events": {
      "type": "object",
      "properties": {
        "on_cast_start": {"type": "string"},
        "on_cast_complete": {"type": "string"},
        "on_hit": {"type": "string"},
        "on_crit": {"type": "string"},
        "on_kill": {"type": "string"}
      }
    }
  }
})";
}

bool ValidateSpellJson(const std::string& json, std::vector<std::string>& errors) {
    // Basic validation - check for required fields
    if (json.find("\"id\"") == std::string::npos) {
        errors.push_back("Missing required field: id");
        return false;
    }

    if (json.find("\"name\"") == std::string::npos) {
        errors.push_back("Missing required field: name");
        return false;
    }

    return true;
}

// ============================================================================
// SpellManager Implementation
// ============================================================================

SpellManager& SpellManager::Instance() {
    static SpellManager instance;
    return instance;
}

bool SpellManager::Initialize(const std::string& spellConfigPath) {
    if (m_initialized) {
        return true;
    }

    m_configPath = spellConfigPath;

    // Create directory if it doesn't exist
    try {
        fs::create_directories(spellConfigPath);
    } catch (...) {
        // Ignore errors
    }

    // Load all spells
    int loaded = LoadAllSpells();

    m_initialized = true;
    return loaded >= 0;
}

void SpellManager::Shutdown() {
    DisableHotReload();
    m_spells.clear();
    m_categories.clear();
    m_sharedEffects.clear();
    m_initialized = false;
}

int SpellManager::LoadAllSpells() {
    if (m_configPath.empty()) {
        return -1;
    }

    int count = 0;
    auto files = ListJsonFiles(m_configPath);

    for (const auto& file : files) {
        if (LoadSpell(file)) {
            count++;
        }
    }

    return count;
}

bool SpellManager::LoadSpell(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    auto spell = std::make_shared<SpellDefinition>();
    if (!spell->LoadFromString(content)) {
        return false;
    }

    // Validate
    std::vector<std::string> errors;
    spell->Validate(errors);

    // Register the spell
    SpellRegistryEntry entry;
    entry.definition = std::move(spell);
    entry.filePath = filePath;
    entry.lastModified = GetFileModificationTime(filePath);
    entry.isLoaded = true;
    entry.validationErrors = std::move(errors);

    std::string id = entry.definition->GetId();
    m_spells[id] = std::move(entry);

    // Track file for hot reload
    m_fileModTimes[filePath] = entry.lastModified;

    return true;
}

bool SpellManager::LoadSpellFromString(const std::string& id, const std::string& jsonString) {
    auto spell = std::make_shared<SpellDefinition>();
    if (!spell->LoadFromString(jsonString)) {
        return false;
    }

    spell->SetId(id);

    SpellRegistryEntry entry;
    entry.definition = std::move(spell);
    entry.isLoaded = true;

    std::vector<std::string> errors;
    entry.definition->Validate(errors);
    entry.validationErrors = std::move(errors);

    m_spells[id] = std::move(entry);
    return true;
}

bool SpellManager::ReloadSpell(const std::string& spellId) {
    auto it = m_spells.find(spellId);
    if (it == m_spells.end()) {
        return false;
    }

    const std::string& filePath = it->second.filePath;
    if (filePath.empty()) {
        return false;
    }

    return LoadSpell(filePath);
}

void SpellManager::UnloadSpell(const std::string& spellId) {
    auto it = m_spells.find(spellId);
    if (it != m_spells.end()) {
        // Remove from file tracking
        if (!it->second.filePath.empty()) {
            m_fileModTimes.erase(it->second.filePath);
        }
        m_spells.erase(it);
    }
}

const SpellDefinition* SpellManager::GetSpell(const std::string& spellId) const {
    auto it = m_spells.find(spellId);
    if (it != m_spells.end() && it->second.isLoaded) {
        return it->second.definition.get();
    }
    return nullptr;
}

SpellDefinition* SpellManager::GetSpellMutable(const std::string& spellId) {
    auto it = m_spells.find(spellId);
    if (it != m_spells.end() && it->second.isLoaded) {
        return it->second.definition.get();
    }
    return nullptr;
}

bool SpellManager::HasSpell(const std::string& spellId) const {
    return m_spells.find(spellId) != m_spells.end();
}

std::vector<std::string> SpellManager::GetAllSpellIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_spells.size());

    for (const auto& [id, entry] : m_spells) {
        ids.push_back(id);
    }

    return ids;
}

std::vector<const SpellDefinition*> SpellManager::GetSpellsByTag(const std::string& tag) const {
    std::vector<const SpellDefinition*> results;

    for (const auto& [id, entry] : m_spells) {
        if (entry.isLoaded && entry.definition) {
            const auto& tags = entry.definition->GetTags();
            if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
                results.push_back(entry.definition.get());
            }
        }
    }

    return results;
}

std::vector<const SpellDefinition*> SpellManager::GetSpellsBySchool(const std::string& school) const {
    std::vector<const SpellDefinition*> results;

    for (const auto& [id, entry] : m_spells) {
        if (entry.isLoaded && entry.definition) {
            if (entry.definition->GetSchool() == school) {
                results.push_back(entry.definition.get());
            }
        }
    }

    return results;
}

std::vector<const SpellDefinition*> SpellManager::GetSpellsByTargetingMode(TargetingMode mode) const {
    std::vector<const SpellDefinition*> results;

    for (const auto& [id, entry] : m_spells) {
        if (entry.isLoaded && entry.definition) {
            if (entry.definition->GetTargetingMode() == mode) {
                results.push_back(entry.definition.get());
            }
        }
    }

    return results;
}

std::vector<const SpellDefinition*> SpellManager::SearchSpells(const std::string& query) const {
    std::vector<const SpellDefinition*> results;

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& [id, entry] : m_spells) {
        if (entry.isLoaded && entry.definition) {
            std::string name = entry.definition->GetName();
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            if (name.find(lowerQuery) != std::string::npos) {
                results.push_back(entry.definition.get());
            }
        }
    }

    return results;
}

void SpellManager::RegisterCategory(const SpellCategory& category) {
    m_categories[category.id] = category;
}

const SpellCategory* SpellManager::GetCategory(const std::string& categoryId) const {
    auto it = m_categories.find(categoryId);
    if (it != m_categories.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<const SpellCategory*> SpellManager::GetAllCategories() const {
    std::vector<const SpellCategory*> categories;
    categories.reserve(m_categories.size());

    for (const auto& [id, category] : m_categories) {
        categories.push_back(&category);
    }

    // Sort by sort order
    std::sort(categories.begin(), categories.end(),
        [](const SpellCategory* a, const SpellCategory* b) {
            return a->sortOrder < b->sortOrder;
        });

    return categories;
}

std::vector<const SpellDefinition*> SpellManager::GetSpellsInCategory(const std::string& categoryId) const {
    std::vector<const SpellDefinition*> results;

    auto it = m_categories.find(categoryId);
    if (it != m_categories.end()) {
        for (const auto& spellId : it->second.spellIds) {
            const SpellDefinition* spell = GetSpell(spellId);
            if (spell) {
                results.push_back(spell);
            }
        }
    }

    return results;
}

std::unique_ptr<SpellInstance> SpellManager::CreateInstance(
    const std::string& spellId,
    uint32_t casterId
) {
    const SpellDefinition* spell = GetSpell(spellId);
    if (!spell) {
        return nullptr;
    }

    auto instance = std::make_unique<SpellInstance>(spell, casterId);
    spell->OnCreate(*instance);

    return instance;
}

std::unique_ptr<SpellInstance> SpellManager::CreateInstance(
    const std::string& spellId,
    uint32_t casterId,
    uint32_t targetId,
    const glm::vec3& targetPosition,
    const glm::vec3& targetDirection
) {
    auto instance = CreateInstance(spellId, casterId);
    if (instance) {
        instance->SetTargetId(targetId);
        instance->SetTargetPosition(targetPosition);
        instance->SetTargetDirection(targetDirection);
    }
    return instance;
}

bool SpellManager::ValidateSpell(const SpellDefinition& spell, std::vector<std::string>& errors) const {
    return spell.Validate(errors);
}

std::unordered_map<std::string, std::vector<std::string>> SpellManager::ValidateAllSpells() const {
    std::unordered_map<std::string, std::vector<std::string>> allErrors;

    for (const auto& [id, entry] : m_spells) {
        if (entry.isLoaded && entry.definition) {
            std::vector<std::string> errors;
            entry.definition->Validate(errors);
            if (!errors.empty()) {
                allErrors[id] = std::move(errors);
            }
        }
    }

    return allErrors;
}

std::vector<std::string> SpellManager::GetValidationErrors(const std::string& spellId) const {
    auto it = m_spells.find(spellId);
    if (it != m_spells.end()) {
        return it->second.validationErrors;
    }
    return {};
}

void SpellManager::EnableHotReload() {
    m_hotReloadEnabled = true;
    UpdateFileModificationTimes();
}

void SpellManager::DisableHotReload() {
    m_hotReloadEnabled = false;
}

void SpellManager::PollFileChanges() {
    if (!m_hotReloadEnabled) return;

    // Check for modified files
    for (auto& [filePath, lastTime] : m_fileModTimes) {
        int64_t currentTime = GetFileModificationTime(filePath);
        if (currentTime > lastTime) {
            // File was modified - reload it
            std::string spellId;

            // Find spell ID for this file
            for (const auto& [id, entry] : m_spells) {
                if (entry.filePath == filePath) {
                    spellId = id;
                    break;
                }
            }

            bool success = LoadSpell(filePath);

            HotReloadEvent event;
            event.type = HotReloadEvent::Type::Modified;
            event.spellId = spellId;
            event.filePath = filePath;
            event.success = success;

            if (!success) {
                event.errorMessage = "Failed to reload spell";
            }

            NotifyHotReload(event);

            lastTime = currentTime;
        }
    }

    // Check for new files
    auto files = ListJsonFiles(m_configPath);
    for (const auto& file : files) {
        if (m_fileModTimes.find(file) == m_fileModTimes.end()) {
            bool success = LoadSpell(file);

            if (success) {
                // Find the spell ID that was just loaded
                std::string spellId;
                for (const auto& [id, entry] : m_spells) {
                    if (entry.filePath == file) {
                        spellId = id;
                        break;
                    }
                }

                HotReloadEvent event;
                event.type = HotReloadEvent::Type::Added;
                event.spellId = spellId;
                event.filePath = file;
                event.success = true;

                NotifyHotReload(event);
            }
        }
    }
}

void SpellManager::RegisterEffect(const std::string& effectId, std::shared_ptr<SpellEffect> effect) {
    m_sharedEffects[effectId] = std::move(effect);
}

std::shared_ptr<SpellEffect> SpellManager::GetEffect(const std::string& effectId) const {
    auto it = m_sharedEffects.find(effectId);
    if (it != m_sharedEffects.end()) {
        return it->second;
    }
    return nullptr;
}

bool SpellManager::ExecuteScript(
    const std::string& scriptPath,
    const std::string& function,
    SpellInstance& instance
) {
    if (m_scriptExecutor) {
        return m_scriptExecutor(scriptPath, function, instance);
    }
    return false;
}

void SpellManager::ScanDirectory(const std::string& path) {
    auto files = ListJsonFiles(path);
    for (const auto& file : files) {
        ProcessJsonFile(file);
    }
}

void SpellManager::ProcessJsonFile(const std::string& filePath) {
    LoadSpell(filePath);
}

bool SpellManager::ParseSpellJson(const std::string& jsonContent, SpellDefinition& spell) {
    return spell.LoadFromString(jsonContent);
}

void SpellManager::UpdateFileModificationTimes() {
    for (const auto& [id, entry] : m_spells) {
        if (!entry.filePath.empty()) {
            m_fileModTimes[entry.filePath] = GetFileModificationTime(entry.filePath);
        }
    }
}

void SpellManager::NotifyHotReload(const HotReloadEvent& event) {
    if (m_hotReloadCallback) {
        m_hotReloadCallback(event);
    }
}

} // namespace Spells
} // namespace Vehement
