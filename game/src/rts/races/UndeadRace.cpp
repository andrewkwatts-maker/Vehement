#include "UndeadRace.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>

namespace Vehement {
namespace RTS {

// ============================================================================
// Corpse Manager Implementation
// ============================================================================

CorpseManager& CorpseManager::Instance() {
    static CorpseManager instance;
    return instance;
}

void CorpseManager::Update(float deltaTime) {
    // Update decay timers and remove decayed corpses
    std::vector<uint32_t> toRemove;

    for (auto& [id, corpse] : m_corpses) {
        corpse.decayTimer -= deltaTime;
        if (corpse.IsDecayed()) {
            toRemove.push_back(id);
        }
    }

    for (uint32_t id : toRemove) {
        RemoveCorpse(id);
    }
}

uint32_t CorpseManager::CreateCorpse(const glm::vec3& position, const std::string& unitType, int health) {
    Corpse corpse;
    corpse.id = m_nextCorpseId++;
    corpse.position = position;
    corpse.sourceUnitType = unitType;
    corpse.sourceUnitHealth = health;
    corpse.decayTimer = UndeadConstants::CORPSE_DECAY_TIME;
    corpse.isReserved = false;

    m_corpses[corpse.id] = corpse;
    return corpse.id;
}

void CorpseManager::RemoveCorpse(uint32_t corpseId) {
    m_corpses.erase(corpseId);
}

std::vector<Corpse*> CorpseManager::FindCorpsesInRadius(const glm::vec3& position, float radius) {
    std::vector<Corpse*> result;
    float radiusSq = radius * radius;

    for (auto& [id, corpse] : m_corpses) {
        glm::vec3 diff = corpse.position - position;
        float distSq = glm::dot(diff, diff);
        if (distSq <= radiusSq && !corpse.isReserved) {
            result.push_back(&corpse);
        }
    }

    // Sort by distance
    std::sort(result.begin(), result.end(), [&position](const Corpse* a, const Corpse* b) {
        float distA = glm::length(a->position - position);
        float distB = glm::length(b->position - position);
        return distA < distB;
    });

    return result;
}

Corpse* CorpseManager::GetNearestCorpse(const glm::vec3& position, float maxRange) {
    auto corpses = FindCorpsesInRadius(position, maxRange);
    return corpses.empty() ? nullptr : corpses[0];
}

bool CorpseManager::ReserveCorpse(uint32_t corpseId, uint32_t entityId) {
    auto it = m_corpses.find(corpseId);
    if (it == m_corpses.end() || it->second.isReserved) {
        return false;
    }

    it->second.isReserved = true;
    it->second.reservedBy = entityId;
    return true;
}

void CorpseManager::ReleaseCorpse(uint32_t corpseId) {
    auto it = m_corpses.find(corpseId);
    if (it != m_corpses.end()) {
        it->second.isReserved = false;
        it->second.reservedBy = 0;
    }
}

bool CorpseManager::ConsumeCorpse(uint32_t corpseId) {
    auto it = m_corpses.find(corpseId);
    if (it == m_corpses.end()) {
        return false;
    }

    RemoveCorpse(corpseId);
    return true;
}

Corpse* CorpseManager::GetCorpse(uint32_t corpseId) {
    auto it = m_corpses.find(corpseId);
    return (it != m_corpses.end()) ? &it->second : nullptr;
}

// ============================================================================
// Blight Manager Implementation
// ============================================================================

BlightManager& BlightManager::Instance() {
    static BlightManager instance;
    return instance;
}

void BlightManager::Update(float deltaTime) {
    // Spread blight from sources
    for (const auto& source : m_blightSources) {
        glm::ivec2 tilePos(
            static_cast<int>(source.position.x),
            static_cast<int>(source.position.z)
        );
        SpreadBlight(tilePos, source.radius * m_spreadRateMultiplier);
    }
}

void BlightManager::AddBlightSource(uint32_t buildingId, const glm::vec3& position, float radius) {
    BlightSource source;
    source.buildingId = buildingId;
    source.position = position;
    source.radius = radius;
    m_blightSources.push_back(source);

    // Initial blight spread
    glm::ivec2 tilePos(
        static_cast<int>(position.x),
        static_cast<int>(position.z)
    );
    SpreadBlight(tilePos, radius);
}

void BlightManager::RemoveBlightSource(uint32_t buildingId) {
    m_blightSources.erase(
        std::remove_if(m_blightSources.begin(), m_blightSources.end(),
            [buildingId](const BlightSource& s) { return s.buildingId == buildingId; }),
        m_blightSources.end()
    );
}

bool BlightManager::IsOnBlight(const glm::vec3& position) const {
    int x = static_cast<int>(position.x);
    int y = static_cast<int>(position.z);
    return m_blightedTiles.count(TileKey(x, y)) > 0;
}

bool BlightManager::CanBuildHere(const glm::vec3& position, bool requiresBlight) const {
    if (!requiresBlight) {
        return true;
    }
    return IsOnBlight(position);
}

float BlightManager::GetBlightIntensity(const glm::vec3& position) const {
    int x = static_cast<int>(position.x);
    int y = static_cast<int>(position.z);
    uint64_t key = TileKey(x, y);

    auto it = m_blightData.find(key);
    if (it != m_blightData.end()) {
        return it->second.intensity;
    }
    return 0.0f;
}

void BlightManager::SpreadBlight(const glm::ivec2& center, float radius) {
    int intRadius = static_cast<int>(std::ceil(radius));

    for (int dx = -intRadius; dx <= intRadius; ++dx) {
        for (int dy = -intRadius; dy <= intRadius; ++dy) {
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (dist <= radius) {
                int x = center.x + dx;
                int y = center.y + dy;
                uint64_t key = TileKey(x, y);

                if (m_blightedTiles.count(key) == 0) {
                    m_blightedTiles.insert(key);

                    BlightTile tile;
                    tile.position = {x, y};
                    tile.intensity = 1.0f - (dist / radius);
                    m_blightData[key] = tile;
                }
            }
        }
    }
}

// ============================================================================
// Undead Race Implementation
// ============================================================================

UndeadRace& UndeadRace::Instance() {
    static UndeadRace instance;
    return instance;
}

bool UndeadRace::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Load race configuration
    if (!LoadConfiguration("game/assets/configs/races/undead/race_undead.json")) {
        return false;
    }

    // Register undead abilities
    RegisterUndeadAbilities();

    m_initialized = true;
    return true;
}

void UndeadRace::Shutdown() {
    m_undeadUnits.clear();
    m_unitTypes.clear();
    m_storedCorpses.clear();
    m_buildings.clear();
    m_initialized = false;
}

void UndeadRace::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update corpse manager
    CorpseManager::Instance().Update(deltaTime);

    // Update blight manager
    BlightManager::Instance().Update(deltaTime);

    // Apply blight regeneration to undead units
    ApplyBlightRegeneration(deltaTime);
}

bool UndeadRace::IsUndeadUnit(uint32_t entityId) const {
    return m_undeadUnits.count(entityId) > 0;
}

void UndeadRace::RegisterUndeadUnit(uint32_t entityId, const std::string& unitType) {
    m_undeadUnits.insert(entityId);
    m_unitTypes[entityId] = unitType;
}

void UndeadRace::UnregisterUndeadUnit(uint32_t entityId) {
    m_undeadUnits.erase(entityId);
    m_unitTypes.erase(entityId);
    m_storedCorpses.erase(entityId);
}

void UndeadRace::ApplyUndeadBonuses(uint32_t entityId) {
    // Apply undead-specific bonuses:
    // - Immunity to poison
    // - Immunity to disease
    // - Holy vulnerability
    // - Fire vulnerability
    // - Ice resistance
    // This would integrate with the entity/component system
}

float UndeadRace::GetDamageMultiplier(const std::string& damageType) const {
    if (damageType == "holy" || damageType == "light") {
        return UndeadConstants::HOLY_DAMAGE_MULTIPLIER;
    }
    if (damageType == "fire") {
        return UndeadConstants::FIRE_DAMAGE_MULTIPLIER;
    }
    if (damageType == "ice" || damageType == "frost") {
        return UndeadConstants::ICE_DAMAGE_MULTIPLIER;
    }
    if (damageType == "poison") {
        return UndeadConstants::POISON_DAMAGE_MULTIPLIER;
    }
    return 1.0f;
}

void UndeadRace::OnUnitDeath(uint32_t entityId, const glm::vec3& position,
                              const std::string& unitType, int health) {
    // Check if unit leaves a corpse
    if (!UnitLeavesCorpse(unitType)) {
        return;
    }

    // Create corpse
    uint32_t corpseId = CorpseManager::Instance().CreateCorpse(position, unitType, health);

    // Notify listeners
    if (m_onCorpseCreated) {
        m_onCorpseCreated(corpseId, position);
    }

    // Try auto-raise if enemy unit
    if (!IsUndeadUnit(entityId)) {
        TryAutoRaise(position, unitType);
    }

    // Unregister if undead
    UnregisterUndeadUnit(entityId);
}

bool UndeadRace::RaiseFromCorpse(uint32_t corpseId, uint32_t casterId,
                                  const std::string& summonType) {
    auto& corpseManager = CorpseManager::Instance();
    Corpse* corpse = corpseManager.GetCorpse(corpseId);

    if (!corpse) {
        return false;
    }

    // Consume the corpse
    glm::vec3 spawnPos = corpse->position;
    if (!corpseManager.ConsumeCorpse(corpseId)) {
        return false;
    }

    // Spawn the raised unit
    // This would integrate with the entity spawning system
    // For now, just notify listeners
    if (m_onUnitRaised) {
        m_onUnitRaised(0, spawnPos); // Unit ID would come from spawn system
    }

    return true;
}

bool UndeadRace::CannibalizeCorpse(uint32_t corpseId, uint32_t consumerId, float& healAmount) {
    auto& corpseManager = CorpseManager::Instance();
    Corpse* corpse = corpseManager.GetCorpse(corpseId);

    if (!corpse) {
        return false;
    }

    // Calculate heal based on corpse's original health
    healAmount = static_cast<float>(corpse->sourceUnitHealth) * 0.5f;

    // Consume the corpse
    return corpseManager.ConsumeCorpse(corpseId);
}

int UndeadRace::GetStoredCorpses(uint32_t entityId) const {
    auto it = m_storedCorpses.find(entityId);
    if (it != m_storedCorpses.end()) {
        return static_cast<int>(it->second.size());
    }
    return 0;
}

bool UndeadRace::StoreCorpse(uint32_t entityId, uint32_t corpseId) {
    if (GetStoredCorpses(entityId) >= UndeadConstants::MAX_STORED_CORPSES) {
        return false;
    }

    auto& corpseManager = CorpseManager::Instance();
    Corpse* corpse = corpseManager.GetCorpse(corpseId);

    if (!corpse) {
        return false;
    }

    // Store corpse reference and remove from world
    m_storedCorpses[entityId].push_back(corpseId);
    corpseManager.RemoveCorpse(corpseId);

    return true;
}

bool UndeadRace::DropCorpse(uint32_t entityId, const glm::vec3& position) {
    auto it = m_storedCorpses.find(entityId);
    if (it == m_storedCorpses.end() || it->second.empty()) {
        return false;
    }

    // Create new corpse at drop location
    CorpseManager::Instance().CreateCorpse(position, "dropped_corpse", 100);
    it->second.pop_back();

    return true;
}

void UndeadRace::ApplyBlightRegeneration(float deltaTime) {
    auto& blightManager = BlightManager::Instance();

    for (uint32_t entityId : m_undeadUnits) {
        // Get entity position - would integrate with entity system
        // For now, assume we have a way to get position
        glm::vec3 position{0.0f}; // Would be fetched from entity

        if (blightManager.IsOnBlight(position)) {
            float intensity = blightManager.GetBlightIntensity(position);
            float healAmount = UndeadConstants::BLIGHT_REGEN_RATE * intensity * deltaTime;
            // Apply healing to entity - would integrate with health component
        }
    }
}

bool UndeadRace::CanPlaceBuilding(const std::string& buildingType, const glm::vec3& position) const {
    // Check if building requires blight
    auto it = m_buildingConfigs.find(buildingType);
    if (it == m_buildingConfigs.end()) {
        return false;
    }

    bool requiresBlight = true;
    if (it->second.contains("construction")) {
        requiresBlight = it->second["construction"].value("requiresBlight", true);
    }

    return BlightManager::Instance().CanBuildHere(position, requiresBlight);
}

void UndeadRace::OnBuildingConstructed(uint32_t buildingId, const std::string& buildingType,
                                        const glm::vec3& position) {
    m_buildings[buildingId] = buildingType;

    // Get blight radius from config
    float blightRadius = UndeadConstants::BLIGHT_BASE_RADIUS;
    auto it = m_buildingConfigs.find(buildingType);
    if (it != m_buildingConfigs.end() && it->second.contains("construction")) {
        blightRadius = it->second["construction"].value("blightRadius", blightRadius);
    }

    // Add blight source
    BlightManager::Instance().AddBlightSource(buildingId, position, blightRadius / 128.0f);

    // Track ziggurats for population
    if (buildingType == "ziggurat" || buildingType == "spirit_tower" ||
        buildingType == "nerubian_tower") {
        ++m_zigguratCount;
    }
}

void UndeadRace::OnBuildingDestroyed(uint32_t buildingId) {
    auto it = m_buildings.find(buildingId);
    if (it != m_buildings.end()) {
        // Track ziggurats
        if (it->second == "ziggurat" || it->second == "spirit_tower" ||
            it->second == "nerubian_tower") {
            --m_zigguratCount;
        }
        m_buildings.erase(it);
    }

    BlightManager::Instance().RemoveBlightSource(buildingId);
}

bool UndeadRace::TryAutoRaise(const glm::vec3& position, const std::string& unitType) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    if (dist(gen) < m_raiseChance) {
        std::string raisedType = GetRaisedUnitType(unitType);
        // Spawn raised unit - would integrate with entity system
        if (m_onUnitRaised) {
            m_onUnitRaised(0, position);
        }
        return true;
    }
    return false;
}

int UndeadRace::GetPopulationCap() const {
    int cap = UndeadConstants::BASE_POPULATION_CAP;
    cap += m_zigguratCount * UndeadConstants::ZIGGURAT_POPULATION;
    return std::min(cap, UndeadConstants::MAX_POPULATION);
}

bool UndeadRace::LoadConfiguration(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return false;
        }

        file >> m_raceConfig;

        // Load unit configurations
        // Would iterate through unit files and load them

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

const nlohmann::json* UndeadRace::GetUnitConfig(const std::string& unitType) const {
    auto it = m_unitConfigs.find(unitType);
    return (it != m_unitConfigs.end()) ? &it->second : nullptr;
}

const nlohmann::json* UndeadRace::GetBuildingConfig(const std::string& buildingType) const {
    auto it = m_buildingConfigs.find(buildingType);
    return (it != m_buildingConfigs.end()) ? &it->second : nullptr;
}

const nlohmann::json* UndeadRace::GetHeroConfig(const std::string& heroType) const {
    auto it = m_heroConfigs.find(heroType);
    return (it != m_heroConfigs.end()) ? &it->second : nullptr;
}

// ============================================================================
// Ability Implementations
// ============================================================================

bool RaiseDeadAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    // Need a corpse nearby
    auto* corpse = CorpseManager::Instance().GetNearestCorpse(
        context.targetPoint,
        data.GetLevelData(context.abilityLevel).range
    );
    return corpse != nullptr;
}

AbilityCastResult RaiseDeadAbility::Execute(const AbilityCastContext& context,
                                             const AbilityData& data) {
    AbilityCastResult result;

    auto* corpse = CorpseManager::Instance().GetNearestCorpse(
        context.targetPoint,
        data.GetLevelData(context.abilityLevel).range
    );

    if (!corpse) {
        result.success = false;
        result.failReason = "No corpse nearby";
        return result;
    }

    // Raise skeletons
    int summonCount = data.GetLevelData(context.abilityLevel).summonCount;
    glm::vec3 spawnPos = corpse->position;

    // Consume corpse
    CorpseManager::Instance().ConsumeCorpse(corpse->id);

    // Spawn summons - would integrate with entity system
    result.success = true;
    result.unitsAffected = summonCount;

    return result;
}

bool CannibalizeAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    auto* corpse = CorpseManager::Instance().GetNearestCorpse(context.targetPoint, 200.0f);
    return corpse != nullptr;
}

AbilityCastResult CannibalizeAbility::Execute(const AbilityCastContext& context,
                                               const AbilityData& data) {
    AbilityCastResult result;

    auto* corpse = CorpseManager::Instance().GetNearestCorpse(context.targetPoint, 200.0f);
    if (!corpse) {
        result.success = false;
        result.failReason = "No corpse nearby";
        return result;
    }

    m_targetCorpse = corpse->id;
    m_channelTime = 0.0f;
    result.success = true;

    return result;
}

void CannibalizeAbility::Update(const AbilityCastContext& context, const AbilityData& data,
                                 float deltaTime) {
    m_channelTime += deltaTime;

    // Heal per second
    float healPerSecond = data.GetLevelData(context.abilityLevel).damage; // Using damage field for heal/sec
    float healAmount = healPerSecond * deltaTime;

    // Apply healing - would integrate with health component
}

void CannibalizeAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Consume the corpse when channeling ends
    CorpseManager::Instance().ConsumeCorpse(m_targetCorpse);
    m_targetCorpse = 0;
}

bool DeathCoilAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return AbilityBehavior::CanCast(context, data) && context.targetUnit != nullptr;
}

AbilityCastResult DeathCoilAbility::Execute(const AbilityCastContext& context,
                                             const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Check if target is undead (heal) or enemy (damage)
    // This would integrate with the faction/alliance system

    result.success = true;
    result.unitsAffected = 1;

    return result;
}

AbilityCastResult FrostNovaAbility::Execute(const AbilityCastContext& context,
                                             const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Find all enemies in radius and apply damage + slow
    // Would integrate with entity query system

    result.success = true;
    result.damageDealt = levelData.damage;

    return result;
}

AbilityCastResult UnholyAuraAbility::Execute(const AbilityCastContext& context,
                                              const AbilityData& data) {
    // Passive aura - just return success
    AbilityCastResult result;
    result.success = true;
    return result;
}

void UnholyAuraAbility::Update(const AbilityCastContext& context, const AbilityData& data,
                                float deltaTime) {
    // Apply aura effects to nearby allies
    // Would integrate with entity query and buff systems
}

bool DarkRitualAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    // Target must be a friendly undead unit
    if (!context.targetUnit) {
        return false;
    }

    // Would check if target is undead and friendly
    return true;
}

AbilityCastResult DarkRitualAbility::Execute(const AbilityCastContext& context,
                                              const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    // Get target's health and convert to mana
    // Kill the target
    // Add mana to caster

    result.success = true;
    result.unitsAffected = 1;

    return result;
}

bool AnimateDeadAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    // Check for corpses in range
    auto corpses = CorpseManager::Instance().FindCorpsesInRadius(
        context.targetPoint,
        data.GetLevelData(context.abilityLevel).radius
    );

    return !corpses.empty();
}

AbilityCastResult AnimateDeadAbility::Execute(const AbilityCastContext& context,
                                               const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    auto corpses = CorpseManager::Instance().FindCorpsesInRadius(
        context.targetPoint,
        levelData.radius
    );

    int raised = 0;
    int maxRaise = levelData.summonCount;

    for (auto* corpse : corpses) {
        if (raised >= maxRaise) {
            break;
        }

        // Raise as invulnerable undead
        // Would spawn entity with invulnerability buff

        CorpseManager::Instance().ConsumeCorpse(corpse->id);
        ++raised;
    }

    result.success = raised > 0;
    result.unitsAffected = raised;

    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

void RegisterUndeadAbilities() {
    auto& manager = AbilityManager::Instance();

    // Register custom ability behaviors
    manager.RegisterBehavior(100, std::make_unique<RaiseDeadAbility>());
    manager.RegisterBehavior(101, std::make_unique<CannibalizeAbility>());
    manager.RegisterBehavior(102, std::make_unique<DeathCoilAbility>());
    manager.RegisterBehavior(103, std::make_unique<FrostNovaAbility>());
    manager.RegisterBehavior(104, std::make_unique<UnholyAuraAbility>());
    manager.RegisterBehavior(105, std::make_unique<DarkRitualAbility>());
    manager.RegisterBehavior(106, std::make_unique<AnimateDeadAbility>());
}

bool UnitLeavesCorpse(const std::string& unitType) {
    // Undead units typically don't leave corpses (except zombies)
    // Mechanical units don't leave corpses
    // Most living units leave corpses

    static const std::unordered_set<std::string> noCorpseUnits = {
        "skeleton", "shade", "banshee", "gargoyle", "obsidian_statue",
        "meat_wagon", "frost_wyrm", "bone_colossus"
    };

    return noCorpseUnits.count(unitType) == 0;
}

std::string GetRaisedUnitType(const std::string& corpseSource) {
    // Map corpse source to raised unit type
    // By default, raise skeleton warriors

    static const std::unordered_map<std::string, std::string> raiseMap = {
        {"worker", "skeleton_warrior"},
        {"footman", "skeleton_warrior"},
        {"knight", "skeleton_warrior"},
        {"archer", "skeleton_archer"},
        {"mage", "skeleton_mage"}
    };

    auto it = raiseMap.find(corpseSource);
    return (it != raiseMap.end()) ? it->second : "skeleton_warrior";
}

} // namespace RTS
} // namespace Vehement
