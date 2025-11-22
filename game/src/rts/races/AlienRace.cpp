#include "AlienRace.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// ShieldComponent Implementation
// ============================================================================

void ShieldComponent::Update(float deltaTime) {
    timeSinceLastDamage += deltaTime;

    // Start regenerating after delay
    if (timeSinceLastDamage >= regenDelay && currentShield < maxShield) {
        isRegenerating = true;
        currentShield += regenRate * deltaTime;
        currentShield = std::min(currentShield, maxShield);
    }

    if (currentShield >= maxShield) {
        isRegenerating = false;
    }
}

float ShieldComponent::TakeDamage(float damage) {
    timeSinceLastDamage = 0.0f;
    isRegenerating = false;

    // Apply shield armor
    float effectiveDamage = damage;
    if (shieldArmor > 0.0f) {
        effectiveDamage = std::max(0.5f, damage - shieldArmor);
    }

    if (currentShield >= effectiveDamage) {
        currentShield -= effectiveDamage;
        return 0.0f; // All damage absorbed
    } else {
        float remaining = effectiveDamage - currentShield;
        currentShield = 0.0f;
        return remaining; // Return damage that goes to health
    }
}

void ShieldComponent::RestoreShields(float amount) {
    currentShield = std::min(maxShield, currentShield + amount);
}

nlohmann::json ShieldComponent::ToJson() const {
    return {
        {"maxShield", maxShield},
        {"currentShield", currentShield},
        {"shieldArmor", shieldArmor},
        {"regenRate", regenRate},
        {"regenDelay", regenDelay}
    };
}

ShieldComponent ShieldComponent::FromJson(const nlohmann::json& j) {
    ShieldComponent shield;
    shield.maxShield = j.value("maxShield", 0.0f);
    shield.currentShield = j.value("currentShield", shield.maxShield);
    shield.shieldArmor = j.value("shieldArmor", 0.0f);
    shield.regenRate = j.value("regenRate", 2.0f);
    shield.regenDelay = j.value("regenDelay", 10.0f);
    return shield;
}

// ============================================================================
// PowerSource Implementation
// ============================================================================

bool PowerSource::IsPowering(const glm::vec3& pos) const {
    if (!isActive) return false;

    float dx = pos.x - position.x;
    float dz = pos.z - position.z;
    float distSq = dx * dx + dz * dz;
    return distSq <= (radius * radius);
}

// ============================================================================
// PowerGridManager Implementation
// ============================================================================

void PowerGridManager::RegisterPowerSource(const PowerSource& source) {
    m_powerSources[source.entityId] = source;
}

void PowerGridManager::UnregisterPowerSource(uint32_t entityId) {
    m_powerSources.erase(entityId);
}

void PowerGridManager::UpdatePowerSourcePosition(uint32_t entityId, const glm::vec3& position) {
    auto it = m_powerSources.find(entityId);
    if (it != m_powerSources.end()) {
        it->second.position = position;
    }
}

bool PowerGridManager::HasPower(const glm::vec3& position) const {
    for (const auto& [id, source] : m_powerSources) {
        if (source.IsPowering(position)) {
            return true;
        }
    }
    return false;
}

bool PowerGridManager::CanWarpAt(const glm::vec3& position) const {
    for (const auto& [id, source] : m_powerSources) {
        if (source.IsPowering(position) && source.allowsWarpIn) {
            return true;
        }
    }
    return false;
}

std::vector<const PowerSource*> PowerGridManager::GetPowerSourcesAt(const glm::vec3& position) const {
    std::vector<const PowerSource*> sources;
    for (const auto& [id, source] : m_powerSources) {
        if (source.IsPowering(position)) {
            sources.push_back(&source);
        }
    }
    return sources;
}

std::vector<glm::vec3> PowerGridManager::GetWarpLocations() const {
    std::vector<glm::vec3> locations;
    for (const auto& [id, source] : m_powerSources) {
        if (source.isActive && source.allowsWarpIn) {
            locations.push_back(source.position);
        }
    }
    return locations;
}

void PowerGridManager::Clear() {
    m_powerSources.clear();
}

// ============================================================================
// WarpGateState Implementation
// ============================================================================

void WarpGateState::Update(float deltaTime) {
    if (cooldownRemaining > 0.0f) {
        cooldownRemaining -= deltaTime;
        if (cooldownRemaining <= 0.0f) {
            cooldownRemaining = 0.0f;
            isReady = true;
        }
    }
}

void WarpGateState::StartCooldown(float duration) {
    cooldownRemaining = duration;
    isReady = false;
}

// ============================================================================
// PsionicComponent Implementation
// ============================================================================

void PsionicComponent::Update(float deltaTime) {
    if (!isChanneling && energy < maxEnergy) {
        energy += energyRegen * deltaTime;
        energy = std::min(energy, maxEnergy);
    }
}

bool PsionicComponent::ConsumeEnergy(float amount) {
    if (energy >= amount) {
        energy -= amount;
        return true;
    }
    return false;
}

void PsionicComponent::RestoreEnergy(float amount) {
    energy = std::min(maxEnergy, energy + amount);
}

// ============================================================================
// AlienRace Implementation
// ============================================================================

AlienRace& AlienRace::Instance() {
    static AlienRace instance;
    return instance;
}

bool AlienRace::Initialize(const std::string& configPath) {
    if (m_initialized) {
        return true;
    }

    m_configBasePath = configPath.empty() ?
        "game/assets/configs/races/aliens/" : configPath;

    try {
        LoadConfiguration(m_configBasePath);
        InitializeDefaultConfigs();
        m_initialized = true;
        return true;
    } catch (const std::exception& e) {
        // Log error
        return false;
    }
}

void AlienRace::Shutdown() {
    m_shields.clear();
    m_warpGates.clear();
    m_activeWarpIns.clear();
    m_psionics.clear();
    m_buildingPowerStatus.clear();
    m_buildingPositions.clear();
    PowerGridManager::Instance().Clear();
    m_initialized = false;
}

void AlienRace::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateShields(deltaTime);
    UpdateWarpIns(deltaTime);
    UpdatePsionics(deltaTime);
    UpdatePowerStatus();
}

void AlienRace::UpdateShields(float deltaTime) {
    for (auto& [entityId, shield] : m_shields) {
        float prevShield = shield.currentShield;
        shield.Update(deltaTime);

        // Check for shield depletion
        if (prevShield > 0.0f && shield.currentShield <= 0.0f) {
            if (m_onShieldDepleted) {
                m_onShieldDepleted(entityId);
            }
        }
    }
}

void AlienRace::UpdateWarpIns(float deltaTime) {
    std::vector<uint32_t> completedWarps;

    for (auto& [unitId, warpIn] : m_activeWarpIns) {
        warpIn.warpProgress += deltaTime;

        if (warpIn.IsComplete()) {
            completedWarps.push_back(unitId);
            if (m_onWarpComplete) {
                m_onWarpComplete(unitId, warpIn.warpPosition);
            }
        }
    }

    for (uint32_t id : completedWarps) {
        m_activeWarpIns.erase(id);
    }

    // Update warp gate cooldowns
    for (auto& [gateId, gate] : m_warpGates) {
        gate.Update(deltaTime);
    }
}

void AlienRace::UpdatePsionics(float deltaTime) {
    for (auto& [unitId, psionic] : m_psionics) {
        psionic.Update(deltaTime);
    }
}

void AlienRace::UpdatePowerStatus() {
    auto& powerGrid = PowerGridManager::Instance();

    for (auto& [buildingId, isPowered] : m_buildingPowerStatus) {
        auto posIt = m_buildingPositions.find(buildingId);
        if (posIt != m_buildingPositions.end()) {
            bool wasPowered = isPowered;
            isPowered = powerGrid.HasPower(posIt->second);

            // Power lost callback
            if (wasPowered && !isPowered && m_onPowerLost) {
                m_onPowerLost(buildingId);
            }
        }
    }
}

void AlienRace::LoadConfiguration(const std::string& configPath) {
    std::string racePath = configPath + "race_aliens.json";
    std::ifstream file(racePath);
    if (file.is_open()) {
        file >> m_raceConfig;
    }
}

void AlienRace::InitializeDefaultConfigs() {
    // Initialize any default configurations not loaded from files
}

// =========================================================================
// Shield Management
// =========================================================================

void AlienRace::RegisterShield(uint32_t entityId, const ShieldComponent& shield) {
    m_shields[entityId] = shield;
}

void AlienRace::UnregisterShield(uint32_t entityId) {
    m_shields.erase(entityId);
}

ShieldComponent* AlienRace::GetShield(uint32_t entityId) {
    auto it = m_shields.find(entityId);
    return it != m_shields.end() ? &it->second : nullptr;
}

const ShieldComponent* AlienRace::GetShield(uint32_t entityId) const {
    auto it = m_shields.find(entityId);
    return it != m_shields.end() ? &it->second : nullptr;
}

float AlienRace::ApplyDamage(uint32_t entityId, float damage) {
    auto* shield = GetShield(entityId);
    if (shield) {
        return shield->TakeDamage(damage);
    }
    return damage; // No shield, all damage goes through
}

void AlienRace::RestoreShields(uint32_t entityId, float amount) {
    auto* shield = GetShield(entityId);
    if (shield) {
        shield->RestoreShields(amount);
    }
}

// =========================================================================
// Power Grid
// =========================================================================

bool AlienRace::BuildingHasPower(uint32_t buildingId) const {
    auto it = m_buildingPowerStatus.find(buildingId);
    return it != m_buildingPowerStatus.end() ? it->second : false;
}

bool AlienRace::CanBuildAt(const glm::vec3& position, const std::string& buildingType) const {
    // Pylons and Nexuses don't require power
    if (buildingType == "pylon" || buildingType == "nexus") {
        return true;
    }
    return PowerGridManager::Instance().HasPower(position);
}

// =========================================================================
// Warp System
// =========================================================================

bool AlienRace::StartWarpIn(uint32_t gateId, const std::string& unitType, const glm::vec3& position) {
    auto* gate = GetWarpGateState(gateId);
    if (!gate || !gate->isReady) {
        return false;
    }

    if (!PowerGridManager::Instance().CanWarpAt(position)) {
        return false;
    }

    // Create warp-in state (unit ID would be generated by entity system)
    static uint32_t nextWarpUnitId = 100000; // Temporary ID generation
    uint32_t unitId = nextWarpUnitId++;

    WarpInState warpIn;
    warpIn.unitId = unitId;
    warpIn.unitType = unitType;
    warpIn.warpPosition = position;
    warpIn.warpProgress = 0.0f;
    warpIn.warpTime = AlienConstants::WARP_IN_TIME;
    warpIn.sourceGateId = gateId;

    m_activeWarpIns[unitId] = warpIn;
    gate->StartCooldown(4.0f); // Standard warp gate cooldown

    return true;
}

bool AlienRace::CancelWarpIn(uint32_t unitId) {
    auto it = m_activeWarpIns.find(unitId);
    if (it != m_activeWarpIns.end()) {
        m_activeWarpIns.erase(it);
        return true;
    }
    return false;
}

WarpGateState* AlienRace::GetWarpGateState(uint32_t gateId) {
    auto it = m_warpGates.find(gateId);
    return it != m_warpGates.end() ? &it->second : nullptr;
}

void AlienRace::RegisterWarpGate(uint32_t buildingId, const std::vector<std::string>& units) {
    WarpGateState gate;
    gate.buildingId = buildingId;
    gate.availableUnits = units;
    gate.isReady = true;
    m_warpGates[buildingId] = gate;
}

void AlienRace::UnregisterWarpGate(uint32_t buildingId) {
    m_warpGates.erase(buildingId);
}

// =========================================================================
// Psionic System
// =========================================================================

void AlienRace::RegisterPsionic(uint32_t unitId, const PsionicComponent& psionic) {
    m_psionics[unitId] = psionic;
}

void AlienRace::UnregisterPsionic(uint32_t unitId) {
    m_psionics.erase(unitId);
}

PsionicComponent* AlienRace::GetPsionic(uint32_t unitId) {
    auto it = m_psionics.find(unitId);
    return it != m_psionics.end() ? &it->second : nullptr;
}

float AlienRace::GetPsionicDamageMultiplier(PsionicRank rank) const {
    switch (rank) {
        case PsionicRank::Latent:   return 1.0f;
        case PsionicRank::Adept:    return 1.1f;
        case PsionicRank::Templar:  return 1.25f;
        case PsionicRank::Archon:   return 1.5f;
        case PsionicRank::Master:   return 1.75f;
        default:                     return 1.0f;
    }
}

// =========================================================================
// Resource Modifiers
// =========================================================================

float AlienRace::GetGatherRateModifier(const std::string& resourceType) const {
    if (resourceType == "minerals") {
        return AlienConstants::MINERAL_GATHER_RATE;
    } else if (resourceType == "vespene") {
        return AlienConstants::VESPENE_GATHER_RATE;
    }
    return 1.0f;
}

float AlienRace::GetCostModifier(const std::string& entityType) const {
    // Check if it's a building
    if (entityType.find("building") != std::string::npos ||
        entityType == "nexus" || entityType == "pylon" ||
        entityType == "gateway" || entityType == "stargate") {
        return AlienConstants::BUILDING_COST_MULTIPLIER;
    }
    return AlienConstants::UNIT_COST_MULTIPLIER;
}

float AlienRace::GetProductionTimeModifier() const {
    return AlienConstants::PRODUCTION_TIME_MULTIPLIER;
}

// =========================================================================
// Statistics
// =========================================================================

float AlienRace::GetTotalActiveShields() const {
    float total = 0.0f;
    for (const auto& [id, shield] : m_shields) {
        total += shield.currentShield;
    }
    return total;
}

int AlienRace::GetPoweredBuildingCount() const {
    int count = 0;
    for (const auto& [id, powered] : m_buildingPowerStatus) {
        if (powered) count++;
    }
    return count;
}

int AlienRace::GetActiveWarpGateCount() const {
    int count = 0;
    for (const auto& [id, gate] : m_warpGates) {
        if (gate.isReady) count++;
    }
    return count;
}

// ============================================================================
// Ability Implementations
// ============================================================================

// Blink Ability
bool BlinkAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) return false;

    // Check range
    if (context.caster) {
        float dist = glm::distance(context.caster->GetPosition(), context.targetPoint);
        const auto& levelData = data.GetLevelData(context.abilityLevel);
        if (dist > levelData.range) return false;
    }

    return true;
}

AbilityCastResult BlinkAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.caster) {
        // Teleport the caster
        context.caster->SetPosition(context.targetPoint);
        result.success = true;
    }

    return result;
}

// Psionic Storm Ability
bool PsionicStormAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) return false;

    // Check energy
    if (context.caster) {
        auto* psionic = AlienRace::Instance().GetPsionic(context.caster->GetId());
        if (psionic) {
            const auto& levelData = data.GetLevelData(context.abilityLevel);
            if (psionic->energy < levelData.manaCost) return false;
        }
    }

    return true;
}

AbilityCastResult PsionicStormAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Create storm instance
    StormInstance storm;
    storm.position = context.targetPoint;
    storm.remainingDuration = levelData.duration;
    storm.tickTimer = 0.0f;

    m_activeStorms.push_back(storm);
    result.success = true;

    return result;
}

void PsionicStormAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    const auto& levelData = data.GetLevelData(context.abilityLevel);
    float tickInterval = 0.5f;
    float damagePerTick = levelData.damage / (levelData.duration / tickInterval);

    for (auto it = m_activeStorms.begin(); it != m_activeStorms.end();) {
        it->remainingDuration -= deltaTime;
        it->tickTimer += deltaTime;

        if (it->tickTimer >= tickInterval) {
            it->tickTimer = 0.0f;
            // Apply damage to enemies in radius
            // (Would integrate with entity system to find targets)
        }

        if (it->remainingDuration <= 0.0f) {
            it = m_activeStorms.erase(it);
        } else {
            ++it;
        }
    }
}

// Feedback Ability
AbilityCastResult FeedbackAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.targetUnit) {
        // Get target's energy
        auto* targetPsionic = AlienRace::Instance().GetPsionic(context.targetUnit->GetId());
        if (targetPsionic && targetPsionic->energy > 0.0f) {
            float energyDrained = targetPsionic->energy;
            targetPsionic->energy = 0.0f;

            // Deal damage equal to energy drained
            result.damageDealt = energyDrained;
            result.success = true;
        }
    }

    return result;
}

// Chrono Boost Ability
AbilityCastResult ChronoBoostAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    // Apply production speed boost to target building
    // (Would integrate with building production system)
    result.success = true;

    return result;
}

void ChronoBoostAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    // Track chrono boost duration
}

void ChronoBoostAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Remove production speed boost
}

// Mass Recall Ability
bool MassRecallAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) return false;

    // Check energy
    if (context.caster) {
        auto* psionic = AlienRace::Instance().GetPsionic(context.caster->GetId());
        if (psionic) {
            const auto& levelData = data.GetLevelData(context.abilityLevel);
            if (psionic->energy < levelData.manaCost) return false;
        }
    }

    return true;
}

AbilityCastResult MassRecallAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Recall all units at target point to caster location
    // (Would integrate with entity system)
    result.success = true;

    return result;
}

// Graviton Beam Ability
bool GravitonBeamAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) return false;

    if (context.targetUnit) {
        // Check if target is valid (ground, non-massive)
        // Would check unit attributes
    }

    return true;
}

AbilityCastResult GravitonBeamAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.targetUnit) {
        // Lift target unit
        // (Would set lifted state on target)
        result.success = true;
        result.unitsAffected = 1;
    }

    return result;
}

void GravitonBeamAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    // Track beam duration, keep target lifted
}

void GravitonBeamAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Drop lifted target
}

} // namespace RTS
} // namespace Vehement
