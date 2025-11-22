#include "ScriptContext.hpp"
#include <game/src/entities/Entity.hpp>
#include <game/src/entities/EntityManager.hpp>
#include <game/src/rts/Building.hpp>
#include <game/src/rts/Resource.hpp>
#include <engine/core/Logger.hpp>
#include <engine/math/Random.hpp>

#include <random>
#include <cmath>

namespace Nova {
namespace Scripting {

// ============================================================================
// VariableScope Implementation
// ============================================================================

VariableScope::VariableScope(std::shared_ptr<VariableScope> parent)
    : m_parent(std::move(parent)) {}

void VariableScope::Set(const std::string& name, const ScriptVar& value) {
    m_variables[name] = value;
}

std::optional<ScriptVar> VariableScope::Get(const std::string& name) const {
    auto it = m_variables.find(name);
    if (it != m_variables.end()) {
        return it->second;
    }
    if (m_parent) {
        return m_parent->Get(name);
    }
    return std::nullopt;
}

bool VariableScope::Has(const std::string& name) const {
    if (m_variables.contains(name)) {
        return true;
    }
    if (m_parent) {
        return m_parent->Has(name);
    }
    return false;
}

void VariableScope::Remove(const std::string& name) {
    m_variables.erase(name);
}

void VariableScope::Clear() {
    m_variables.clear();
}

std::vector<std::string> VariableScope::GetVariableNames() const {
    std::vector<std::string> names;
    names.reserve(m_variables.size());
    for (const auto& [name, _] : m_variables) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// ScriptContext Implementation
// ============================================================================

ScriptContext::ScriptContext() {
    m_globalScope = std::make_shared<VariableScope>();
    m_currentScope = m_globalScope;

    RegisterBuiltinFunctions();
}

ScriptContext::~ScriptContext() = default;

// ============================================================================
// API Function Registration
// ============================================================================

void ScriptContext::RegisterFunction(const std::string& name, ApiFunction func,
                                     const std::string& doc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_apiFunctions[name] = {std::move(func), doc};
}

void ScriptContext::UnregisterFunction(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_apiFunctions.erase(name);
}

std::vector<std::string> ScriptContext::GetRegisteredFunctions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_apiFunctions.size());
    for (const auto& [name, _] : m_apiFunctions) {
        names.push_back(name);
    }
    return names;
}

std::string ScriptContext::GetFunctionDoc(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_apiFunctions.find(name);
    if (it != m_apiFunctions.end()) {
        return it->second.documentation;
    }
    return "";
}

ScriptVar ScriptContext::CallFunction(const std::string& name,
                                      const std::vector<ScriptVar>& args) {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_apiFunctions.find(name);
    if (it == m_apiFunctions.end()) {
        LogError("Unknown API function: " + name);
        return std::monostate{};
    }

    auto result = it->second.function(args);

    auto endTime = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_metrics.RecordApiCall(name, timeMs);

    return result;
}

// ============================================================================
// Built-in API Functions
// ============================================================================

void ScriptContext::RegisterBuiltinFunctions() {
    // Entity functions
    RegisterFunction("spawn_entity", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 4) return 0;
        auto type = std::get_if<std::string>(&args[0]);
        auto x = std::get_if<float>(&args[1]);
        auto y = std::get_if<float>(&args[2]);
        auto z = std::get_if<float>(&args[3]);
        if (!type || !x || !y || !z) return 0;
        return static_cast<int>(SpawnEntity(*type, *x, *y, *z));
    }, "Spawn an entity of the given type at position (x, y, z). Returns entity ID.");

    RegisterFunction("despawn_entity", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return false;
        auto id = std::get_if<int>(&args[0]);
        if (!id) return false;
        DespawnEntity(static_cast<uint32_t>(*id));
        return true;
    }, "Remove an entity from the game world.");

    RegisterFunction("get_entity_position", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return glm::vec3(0.0f);
        auto id = std::get_if<int>(&args[0]);
        if (!id) return glm::vec3(0.0f);
        return GetEntityPosition(static_cast<uint32_t>(*id));
    }, "Get the position of an entity as (x, y, z).");

    RegisterFunction("set_entity_position", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 4) return false;
        auto id = std::get_if<int>(&args[0]);
        auto x = std::get_if<float>(&args[1]);
        auto y = std::get_if<float>(&args[2]);
        auto z = std::get_if<float>(&args[3]);
        if (!id || !x || !y || !z) return false;
        SetEntityPosition(static_cast<uint32_t>(*id), *x, *y, *z);
        return true;
    }, "Set the position of an entity.");

    RegisterFunction("get_entity_health", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return 0.0f;
        auto id = std::get_if<int>(&args[0]);
        if (!id) return 0.0f;
        return GetEntityHealth(static_cast<uint32_t>(*id));
    }, "Get the current health of an entity.");

    RegisterFunction("damage_entity", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 2) return false;
        auto id = std::get_if<int>(&args[0]);
        auto damage = std::get_if<float>(&args[1]);
        if (!id || !damage) return false;
        uint32_t sourceId = 0;
        if (args.size() > 2) {
            if (auto src = std::get_if<int>(&args[2])) {
                sourceId = static_cast<uint32_t>(*src);
            }
        }
        DamageEntity(static_cast<uint32_t>(*id), *damage, sourceId);
        return true;
    }, "Apply damage to an entity. Optional source entity ID.");

    RegisterFunction("heal_entity", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 2) return false;
        auto id = std::get_if<int>(&args[0]);
        auto amount = std::get_if<float>(&args[1]);
        if (!id || !amount) return false;
        HealEntity(static_cast<uint32_t>(*id), *amount);
        return true;
    }, "Heal an entity by the specified amount.");

    RegisterFunction("is_entity_alive", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return false;
        auto id = std::get_if<int>(&args[0]);
        if (!id) return false;
        return IsEntityAlive(static_cast<uint32_t>(*id));
    }, "Check if an entity is alive (health > 0).");

    // Spatial queries
    RegisterFunction("find_entities_in_radius", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 4) return std::string("");
        auto x = std::get_if<float>(&args[0]);
        auto y = std::get_if<float>(&args[1]);
        auto z = std::get_if<float>(&args[2]);
        auto radius = std::get_if<float>(&args[3]);
        if (!x || !y || !z || !radius) return std::string("");

        auto entities = FindEntitiesInRadius(*x, *y, *z, *radius);
        // Return as comma-separated string of IDs
        std::string result;
        for (size_t i = 0; i < entities.size(); ++i) {
            if (i > 0) result += ",";
            result += std::to_string(entities[i]);
        }
        return result;
    }, "Find all entities within radius. Returns comma-separated entity IDs.");

    RegisterFunction("get_nearest_entity", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 3) return 0;
        auto x = std::get_if<float>(&args[0]);
        auto y = std::get_if<float>(&args[1]);
        auto z = std::get_if<float>(&args[2]);
        if (!x || !y || !z) return 0;

        std::string type;
        if (args.size() > 3) {
            if (auto t = std::get_if<std::string>(&args[3])) {
                type = *t;
            }
        }
        return static_cast<int>(GetNearestEntity(*x, *y, *z, type));
    }, "Get the nearest entity to position. Optional type filter.");

    RegisterFunction("get_distance", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 2) return 0.0f;
        auto id1 = std::get_if<int>(&args[0]);
        auto id2 = std::get_if<int>(&args[1]);
        if (!id1 || !id2) return 0.0f;
        return GetDistance(static_cast<uint32_t>(*id1), static_cast<uint32_t>(*id2));
    }, "Get distance between two entities.");

    // Resource functions
    RegisterFunction("get_resource", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return 0;
        auto type = std::get_if<std::string>(&args[0]);
        if (!type) return 0;
        return GetResourceAmount(*type);
    }, "Get the current amount of a resource type.");

    RegisterFunction("add_resource", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 2) return false;
        auto type = std::get_if<std::string>(&args[0]);
        auto amount = std::get_if<int>(&args[1]);
        if (!type || !amount) return false;
        return AddResource(*type, *amount);
    }, "Add resources to the player's stockpile.");

    RegisterFunction("remove_resource", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 2) return false;
        auto type = std::get_if<std::string>(&args[0]);
        auto amount = std::get_if<int>(&args[1]);
        if (!type || !amount) return false;
        return RemoveResource(*type, *amount);
    }, "Remove resources from the player's stockpile.");

    RegisterFunction("can_afford", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 2) return false;
        auto type = std::get_if<std::string>(&args[0]);
        auto amount = std::get_if<int>(&args[1]);
        if (!type || !amount) return false;
        return CanAfford(*type, *amount);
    }, "Check if player can afford a resource cost.");

    // Sound and effects
    RegisterFunction("play_sound", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return false;
        auto name = std::get_if<std::string>(&args[0]);
        if (!name) return false;
        float x = 0, y = 0, z = 0;
        if (args.size() > 1) if (auto v = std::get_if<float>(&args[1])) x = *v;
        if (args.size() > 2) if (auto v = std::get_if<float>(&args[2])) y = *v;
        if (args.size() > 3) if (auto v = std::get_if<float>(&args[3])) z = *v;
        PlaySound(*name, x, y, z);
        return true;
    }, "Play a sound effect. Optional 3D position.");

    RegisterFunction("spawn_effect", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 4) return false;
        auto name = std::get_if<std::string>(&args[0]);
        auto x = std::get_if<float>(&args[1]);
        auto y = std::get_if<float>(&args[2]);
        auto z = std::get_if<float>(&args[3]);
        if (!name || !x || !y || !z) return false;
        SpawnEffect(*name, *x, *y, *z);
        return true;
    }, "Spawn a visual effect at position.");

    RegisterFunction("spawn_particles", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 5) return false;
        auto type = std::get_if<std::string>(&args[0]);
        auto x = std::get_if<float>(&args[1]);
        auto y = std::get_if<float>(&args[2]);
        auto z = std::get_if<float>(&args[3]);
        auto count = std::get_if<int>(&args[4]);
        if (!type || !x || !y || !z || !count) return false;
        SpawnParticles(*type, *x, *y, *z, *count);
        return true;
    }, "Spawn particles at position.");

    // UI
    RegisterFunction("show_notification", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return false;
        auto msg = std::get_if<std::string>(&args[0]);
        if (!msg) return false;
        float duration = 3.0f;
        if (args.size() > 1) if (auto d = std::get_if<float>(&args[1])) duration = *d;
        ShowNotification(*msg, duration);
        return true;
    }, "Show a notification message to the player.");

    RegisterFunction("show_warning", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return false;
        auto msg = std::get_if<std::string>(&args[0]);
        if (!msg) return false;
        ShowWarning(*msg);
        return true;
    }, "Show a warning message to the player.");

    // Time
    RegisterFunction("get_delta_time", [this](const std::vector<ScriptVar>&) -> ScriptVar {
        return GetDeltaTime();
    }, "Get time since last frame in seconds.");

    RegisterFunction("get_game_time", [this](const std::vector<ScriptVar>&) -> ScriptVar {
        return GetGameTime();
    }, "Get total game time in seconds.");

    RegisterFunction("get_day_number", [this](const std::vector<ScriptVar>&) -> ScriptVar {
        return GetDayNumber();
    }, "Get current in-game day number.");

    RegisterFunction("is_night", [this](const std::vector<ScriptVar>&) -> ScriptVar {
        return IsNight();
    }, "Check if it's currently nighttime.");

    // Math
    RegisterFunction("random", [this](const std::vector<ScriptVar>&) -> ScriptVar {
        return Random();
    }, "Get a random float between 0 and 1.");

    RegisterFunction("random_range", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 2) return 0.0f;
        auto min = std::get_if<float>(&args[0]);
        auto max = std::get_if<float>(&args[1]);
        if (!min || !max) return 0.0f;
        return RandomRange(*min, *max);
    }, "Get a random float between min and max.");

    RegisterFunction("random_int", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 2) return 0;
        auto min = std::get_if<int>(&args[0]);
        auto max = std::get_if<int>(&args[1]);
        if (!min || !max) return 0;
        return RandomInt(*min, *max);
    }, "Get a random integer between min and max (inclusive).");

    // Logging
    RegisterFunction("log_info", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return false;
        auto msg = std::get_if<std::string>(&args[0]);
        if (!msg) return false;
        LogInfo(*msg);
        return true;
    }, "Log an info message.");

    RegisterFunction("log_warning", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return false;
        auto msg = std::get_if<std::string>(&args[0]);
        if (!msg) return false;
        LogWarning(*msg);
        return true;
    }, "Log a warning message.");

    RegisterFunction("log_error", [this](const std::vector<ScriptVar>& args) -> ScriptVar {
        if (args.size() < 1) return false;
        auto msg = std::get_if<std::string>(&args[0]);
        if (!msg) return false;
        LogError(*msg);
        return true;
    }, "Log an error message.");
}

// ============================================================================
// Entity API Implementation
// ============================================================================

uint32_t ScriptContext::SpawnEntity(const std::string& type, float x, float y, float z) {
    if (!m_entityManager) return 0;

    // TODO: Implement entity spawning based on type
    // For now, return 0 as placeholder
    LogInfo("SpawnEntity: " + type + " at (" + std::to_string(x) + ", " +
            std::to_string(y) + ", " + std::to_string(z) + ")");
    return 0;
}

void ScriptContext::DespawnEntity(uint32_t entityId) {
    if (!m_entityManager) return;

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        entity->MarkForRemoval();
    }
}

glm::vec3 ScriptContext::GetEntityPosition(uint32_t entityId) const {
    if (!m_entityManager) return glm::vec3(0.0f);

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        return entity->GetPosition();
    }
    return glm::vec3(0.0f);
}

void ScriptContext::SetEntityPosition(uint32_t entityId, float x, float y, float z) {
    if (!m_entityManager) return;

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        entity->SetPosition(glm::vec3(x, y, z));
    }
}

float ScriptContext::GetEntityHealth(uint32_t entityId) const {
    if (!m_entityManager) return 0.0f;

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        return entity->GetHealth();
    }
    return 0.0f;
}

void ScriptContext::SetEntityHealth(uint32_t entityId, float health) {
    if (!m_entityManager) return;

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        entity->SetHealth(health);
    }
}

void ScriptContext::DamageEntity(uint32_t entityId, float damage, uint32_t sourceId) {
    if (!m_entityManager) return;

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        entity->TakeDamage(damage, sourceId);
    }
}

void ScriptContext::HealEntity(uint32_t entityId, float amount) {
    if (!m_entityManager) return;

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        entity->Heal(amount);
    }
}

bool ScriptContext::IsEntityAlive(uint32_t entityId) const {
    if (!m_entityManager) return false;

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        return entity->IsAlive();
    }
    return false;
}

std::string ScriptContext::GetEntityType(uint32_t entityId) const {
    if (!m_entityManager) return "";

    auto* entity = m_entityManager->GetEntity(entityId);
    if (entity) {
        return Vehement::EntityTypeToString(entity->GetType());
    }
    return "";
}

// ============================================================================
// Spatial Query Implementation
// ============================================================================

std::vector<uint32_t> ScriptContext::FindEntitiesInRadius(float x, float y, float z, float radius) {
    std::vector<uint32_t> result;
    if (!m_entityManager) return result;

    auto entities = m_entityManager->FindEntitiesInRadius(glm::vec3(x, y, z), radius);
    result.reserve(entities.size());
    for (auto* entity : entities) {
        result.push_back(entity->GetId());
    }
    return result;
}

std::vector<uint32_t> ScriptContext::FindEntitiesByType(const std::string& type) {
    std::vector<uint32_t> result;
    if (!m_entityManager) return result;

    // Map string to EntityType
    Vehement::EntityType entityType = Vehement::EntityType::None;
    if (type == "Player") entityType = Vehement::EntityType::Player;
    else if (type == "Zombie") entityType = Vehement::EntityType::Zombie;
    else if (type == "NPC") entityType = Vehement::EntityType::NPC;
    else if (type == "Projectile") entityType = Vehement::EntityType::Projectile;
    else if (type == "Pickup") entityType = Vehement::EntityType::Pickup;
    else if (type == "Effect") entityType = Vehement::EntityType::Effect;

    auto entities = m_entityManager->GetEntitiesByType(entityType);
    result.reserve(entities.size());
    for (auto* entity : entities) {
        result.push_back(entity->GetId());
    }
    return result;
}

uint32_t ScriptContext::GetNearestEntity(float x, float y, float z, const std::string& type) {
    if (!m_entityManager) return 0;

    Vehement::Entity* nearest = nullptr;
    if (type.empty()) {
        nearest = m_entityManager->GetNearestEntity(glm::vec3(x, y, z));
    } else {
        // Map type string and find
        Vehement::EntityType entityType = Vehement::EntityType::None;
        if (type == "Player") entityType = Vehement::EntityType::Player;
        else if (type == "Zombie") entityType = Vehement::EntityType::Zombie;
        else if (type == "NPC") entityType = Vehement::EntityType::NPC;

        nearest = m_entityManager->GetNearestEntity(glm::vec3(x, y, z), entityType);
    }

    return nearest ? nearest->GetId() : 0;
}

float ScriptContext::GetDistance(uint32_t entity1, uint32_t entity2) const {
    if (!m_entityManager) return 0.0f;

    auto* e1 = m_entityManager->GetEntity(entity1);
    auto* e2 = m_entityManager->GetEntity(entity2);

    if (e1 && e2) {
        return e1->DistanceTo(*e2);
    }
    return 0.0f;
}

ScriptContext::RaycastResult ScriptContext::Raycast(float startX, float startY, float startZ,
                                                    float dirX, float dirY, float dirZ,
                                                    float maxDistance) {
    RaycastResult result;
    // TODO: Implement raycast using physics or spatial queries
    // For now, return empty result
    return result;
}

// ============================================================================
// Resource API Implementation
// ============================================================================

int ScriptContext::GetResourceAmount(const std::string& resourceType) const {
    if (!m_resourceStock) return 0;

    // Map string to ResourceType
    using namespace Vehement::RTS;
    ResourceType type = ResourceType::Food;  // Default

    if (resourceType == "Food") type = ResourceType::Food;
    else if (resourceType == "Wood") type = ResourceType::Wood;
    else if (resourceType == "Stone") type = ResourceType::Stone;
    else if (resourceType == "Metal") type = ResourceType::Metal;
    else if (resourceType == "Coins") type = ResourceType::Coins;
    else if (resourceType == "Fuel") type = ResourceType::Fuel;
    else if (resourceType == "Medicine") type = ResourceType::Medicine;
    else if (resourceType == "Ammunition") type = ResourceType::Ammunition;

    return m_resourceStock->GetAmount(type);
}

bool ScriptContext::AddResource(const std::string& resourceType, int amount) {
    if (!m_resourceStock) return false;

    using namespace Vehement::RTS;
    ResourceType type = ResourceType::Food;

    if (resourceType == "Food") type = ResourceType::Food;
    else if (resourceType == "Wood") type = ResourceType::Wood;
    else if (resourceType == "Stone") type = ResourceType::Stone;
    else if (resourceType == "Metal") type = ResourceType::Metal;
    else if (resourceType == "Coins") type = ResourceType::Coins;
    else if (resourceType == "Fuel") type = ResourceType::Fuel;
    else if (resourceType == "Medicine") type = ResourceType::Medicine;
    else if (resourceType == "Ammunition") type = ResourceType::Ammunition;

    m_resourceStock->Add(type, amount);
    return true;
}

bool ScriptContext::RemoveResource(const std::string& resourceType, int amount) {
    if (!m_resourceStock) return false;

    using namespace Vehement::RTS;
    ResourceType type = ResourceType::Food;

    if (resourceType == "Food") type = ResourceType::Food;
    else if (resourceType == "Wood") type = ResourceType::Wood;
    else if (resourceType == "Stone") type = ResourceType::Stone;
    else if (resourceType == "Metal") type = ResourceType::Metal;
    else if (resourceType == "Coins") type = ResourceType::Coins;
    else if (resourceType == "Fuel") type = ResourceType::Fuel;
    else if (resourceType == "Medicine") type = ResourceType::Medicine;
    else if (resourceType == "Ammunition") type = ResourceType::Ammunition;

    return m_resourceStock->Remove(type, amount);
}

bool ScriptContext::CanAfford(const std::string& resourceType, int amount) const {
    if (!m_resourceStock) return false;

    using namespace Vehement::RTS;
    ResourceType type = ResourceType::Food;

    if (resourceType == "Food") type = ResourceType::Food;
    else if (resourceType == "Wood") type = ResourceType::Wood;
    else if (resourceType == "Stone") type = ResourceType::Stone;
    else if (resourceType == "Metal") type = ResourceType::Metal;
    else if (resourceType == "Coins") type = ResourceType::Coins;
    else if (resourceType == "Fuel") type = ResourceType::Fuel;
    else if (resourceType == "Medicine") type = ResourceType::Medicine;
    else if (resourceType == "Ammunition") type = ResourceType::Ammunition;

    return m_resourceStock->CanAfford(type, amount);
}

// ============================================================================
// Building API Implementation
// ============================================================================

uint32_t ScriptContext::GetBuildingAt(int tileX, int tileY) const {
    // TODO: Implement building lookup by tile position
    return 0;
}

std::string ScriptContext::GetBuildingType(uint32_t buildingId) const {
    // TODO: Implement building type lookup
    return "";
}

bool ScriptContext::IsBuildingOperational(uint32_t buildingId) const {
    // TODO: Implement building state check
    return false;
}

float ScriptContext::GetBuildingProgress(uint32_t buildingId) const {
    // TODO: Implement building progress check
    return 0.0f;
}

// ============================================================================
// Sound and Effects Implementation
// ============================================================================

void ScriptContext::PlaySound(const std::string& soundName, float x, float y, float z) {
    // TODO: Integrate with audio system
    LogDebug("PlaySound: " + soundName + " at (" + std::to_string(x) + ", " +
             std::to_string(y) + ", " + std::to_string(z) + ")");
}

void ScriptContext::PlayMusic(const std::string& musicName) {
    // TODO: Integrate with audio system
    LogDebug("PlayMusic: " + musicName);
}

void ScriptContext::SpawnEffect(const std::string& effectName, float x, float y, float z) {
    // TODO: Integrate with particle/effect system
    LogDebug("SpawnEffect: " + effectName + " at (" + std::to_string(x) + ", " +
             std::to_string(y) + ", " + std::to_string(z) + ")");
}

void ScriptContext::SpawnParticles(const std::string& particleType, float x, float y, float z, int count) {
    // TODO: Integrate with particle system
    LogDebug("SpawnParticles: " + particleType + " x" + std::to_string(count));
}

// ============================================================================
// UI Notification Implementation
// ============================================================================

void ScriptContext::ShowNotification(const std::string& message, float duration) {
    // TODO: Integrate with UI system
    LogInfo("[NOTIFICATION] " + message);
}

void ScriptContext::ShowWarning(const std::string& message) {
    // TODO: Integrate with UI system
    LogWarning("[WARNING] " + message);
}

void ScriptContext::ShowError(const std::string& message) {
    // TODO: Integrate with UI system
    LogError("[ERROR] " + message);
}

void ScriptContext::ShowTooltip(const std::string& text, float x, float y) {
    // TODO: Integrate with UI system
}

// ============================================================================
// Math Utility Implementation
// ============================================================================

float ScriptContext::Random() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    return dis(gen);
}

float ScriptContext::RandomRange(float min, float max) const {
    return min + Random() * (max - min);
}

int ScriptContext::RandomInt(int min, int max) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}

glm::vec3 ScriptContext::RandomDirection() const {
    float theta = Random() * 2.0f * 3.14159265f;
    float phi = std::acos(2.0f * Random() - 1.0f);
    return glm::vec3(
        std::sin(phi) * std::cos(theta),
        std::sin(phi) * std::sin(theta),
        std::cos(phi)
    );
}

// ============================================================================
// Logging Implementation
// ============================================================================

void ScriptContext::LogInfo(const std::string& message) {
    // TODO: Use Nova::Logger
    std::printf("[Script INFO] %s\n", message.c_str());
}

void ScriptContext::LogWarning(const std::string& message) {
    std::printf("[Script WARN] %s\n", message.c_str());
}

void ScriptContext::LogError(const std::string& message) {
    std::fprintf(stderr, "[Script ERROR] %s\n", message.c_str());
}

void ScriptContext::LogDebug(const std::string& message) {
    std::printf("[Script DEBUG] %s\n", message.c_str());
}

// ============================================================================
// Variable Scope Management
// ============================================================================

void ScriptContext::PushScope() {
    auto newScope = std::make_shared<VariableScope>(m_currentScope);
    m_scopeStack.push_back(m_currentScope);
    m_currentScope = newScope;
}

void ScriptContext::PopScope() {
    if (!m_scopeStack.empty()) {
        m_currentScope = m_scopeStack.back();
        m_scopeStack.pop_back();
    }
}

void ScriptContext::SetVariable(const std::string& name, const ScriptVar& value) {
    m_currentScope->Set(name, value);
}

std::optional<ScriptVar> ScriptContext::GetVariable(const std::string& name) const {
    return m_currentScope->Get(name);
}

void ScriptContext::SetGlobal(const std::string& name, const ScriptVar& value) {
    m_globalScope->Set(name, value);
}

std::optional<ScriptVar> ScriptContext::GetGlobal(const std::string& name) const {
    return m_globalScope->Get(name);
}

// ============================================================================
// Execution Limits
// ============================================================================

bool ScriptContext::IsOperationAllowed(const std::string& operation) const {
    if (operation == "file_access" && !m_limits.allowFileAccess) return false;
    if (operation == "network_access" && !m_limits.allowNetworkAccess) return false;
    if (operation == "system_call" && !m_limits.allowSystemCalls) return false;
    return true;
}

void ScriptContext::BeginExecution() {
    m_executionStartTime = std::chrono::high_resolution_clock::now();
    m_inExecution = true;
}

void ScriptContext::EndExecution() {
    m_inExecution = false;
}

bool ScriptContext::IsTimeLimitExceeded() const {
    if (!m_inExecution) return false;

    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_executionStartTime);
    return elapsed > m_limits.maxExecutionTime;
}

// ============================================================================
// Update
// ============================================================================

void ScriptContext::Update(float deltaTime) {
    m_deltaTime = deltaTime;
    m_gameTime += deltaTime;
}

} // namespace Scripting
} // namespace Nova
