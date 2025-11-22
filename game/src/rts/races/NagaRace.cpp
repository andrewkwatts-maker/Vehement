#include "NagaRace.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>

namespace Vehement {
namespace RTS {

// ============================================================================
// VenomEffect Implementation
// ============================================================================

float VenomEffect::Update(float deltaTime) {
    float damageDealt = 0.0f;
    remainingDuration -= deltaTime;
    timeSinceLastTick += deltaTime;

    if (timeSinceLastTick >= tickInterval) {
        damageDealt = GetTotalDamagePerTick();
        timeSinceLastTick = 0.0f;
    }

    return damageDealt;
}

void VenomEffect::AddStack(float damage, float duration) {
    if (stacks < maxStacks) {
        stacks++;
    }
    // Refresh duration
    remainingDuration = std::max(remainingDuration, duration);
    // Use highest damage
    damagePerTick = std::max(damagePerTick, damage);
}

nlohmann::json VenomEffect::ToJson() const {
    return {
        {"targetId", targetId},
        {"sourceId", sourceId},
        {"damagePerTick", damagePerTick},
        {"tickInterval", tickInterval},
        {"remainingDuration", remainingDuration},
        {"stacks", stacks},
        {"maxStacks", maxStacks},
        {"healingReduction", healingReduction}
    };
}

VenomEffect VenomEffect::FromJson(const nlohmann::json& j) {
    VenomEffect effect;
    effect.targetId = j.value("targetId", 0u);
    effect.sourceId = j.value("sourceId", 0u);
    effect.damagePerTick = j.value("damagePerTick", NagaConstants::BASE_VENOM_DAMAGE);
    effect.tickInterval = j.value("tickInterval", NagaConstants::VENOM_TICK_INTERVAL);
    effect.remainingDuration = j.value("remainingDuration", NagaConstants::VENOM_DURATION);
    effect.stacks = j.value("stacks", 1);
    effect.maxStacks = j.value("maxStacks", NagaConstants::VENOM_MAX_STACKS);
    effect.healingReduction = j.value("healingReduction", NagaConstants::VENOM_HEALING_REDUCTION);
    return effect;
}

// ============================================================================
// VenomManager Implementation
// ============================================================================

void VenomManager::ApplyVenom(uint32_t targetId, uint32_t sourceId, float damage, float duration, int maxStacks) {
    auto it = m_activeVenoms.find(targetId);
    if (it != m_activeVenoms.end()) {
        it->second.AddStack(damage, duration);
    } else {
        VenomEffect effect;
        effect.targetId = targetId;
        effect.sourceId = sourceId;
        effect.damagePerTick = damage;
        effect.remainingDuration = duration;
        effect.maxStacks = maxStacks;
        m_activeVenoms[targetId] = effect;
    }
}

void VenomManager::ApplyNeurotoxin(uint32_t targetId, uint32_t sourceId, float damage, float duration, float slowAmount) {
    ApplyVenom(targetId, sourceId, damage, duration, NagaConstants::VENOM_MAX_STACKS);
    auto it = m_activeVenoms.find(targetId);
    if (it != m_activeVenoms.end()) {
        it->second.appliesSlowEffect = true;
        it->second.slowAmount = slowAmount;
    }
}

void VenomManager::RemoveVenom(uint32_t targetId) {
    m_activeVenoms.erase(targetId);
}

void VenomManager::Update(float deltaTime) {
    std::vector<uint32_t> expiredVenoms;

    for (auto& [targetId, venom] : m_activeVenoms) {
        float damage = venom.Update(deltaTime);
        if (damage > 0.0f) {
            // Apply damage to target (implementation would interface with entity system)
            // EntityManager::Instance().ApplyDamage(targetId, damage, DamageType::Poison);
        }
        if (venom.IsExpired()) {
            expiredVenoms.push_back(targetId);
        }
    }

    for (uint32_t id : expiredVenoms) {
        m_activeVenoms.erase(id);
    }
}

bool VenomManager::HasVenom(uint32_t targetId) const {
    return m_activeVenoms.count(targetId) > 0;
}

int VenomManager::GetVenomStacks(uint32_t targetId) const {
    auto it = m_activeVenoms.find(targetId);
    return it != m_activeVenoms.end() ? it->second.stacks : 0;
}

float VenomManager::GetHealingReduction(uint32_t targetId) const {
    auto it = m_activeVenoms.find(targetId);
    return it != m_activeVenoms.end() ? it->second.healingReduction * it->second.stacks : 0.0f;
}

void VenomManager::Clear() {
    m_activeVenoms.clear();
}

// ============================================================================
// TidalPowerManager Implementation
// ============================================================================

uint64_t TidalPowerManager::HashPosition(const glm::vec3& pos) const {
    // Simple spatial hash
    int x = static_cast<int>(pos.x);
    int y = static_cast<int>(pos.y);
    int z = static_cast<int>(pos.z);
    return (static_cast<uint64_t>(x) << 40) | (static_cast<uint64_t>(y) << 20) | static_cast<uint64_t>(z);
}

void TidalPowerManager::RegisterWaterTile(const glm::vec3& position, bool isDeepWater) {
    WaterTile tile;
    tile.position = position;
    tile.isDeepWater = isDeepWater;
    tile.powerBonus = isDeepWater ? 1.5f : 1.0f;
    m_waterTiles.push_back(tile);
    m_waterPositionHashes.insert(HashPosition(position));
}

bool TidalPowerManager::IsNearWater(const glm::vec3& position, float radius) const {
    for (const auto& tile : m_waterTiles) {
        float dist = glm::distance(position, tile.position);
        if (dist <= radius) {
            return true;
        }
    }
    return false;
}

bool TidalPowerManager::IsInWater(const glm::vec3& position) const {
    return m_waterPositionHashes.count(HashPosition(position)) > 0;
}

bool TidalPowerManager::IsInDeepWater(const glm::vec3& position) const {
    for (const auto& tile : m_waterTiles) {
        if (tile.isDeepWater) {
            float dist = glm::distance(position, tile.position);
            if (dist < 1.0f) {
                return true;
            }
        }
    }
    return false;
}

float TidalPowerManager::GetTidalPowerBonus(const glm::vec3& position) const {
    if (IsInDeepWater(position)) return 1.5f;
    if (IsInWater(position)) return 1.25f;
    if (IsNearWater(position)) return 1.0f;
    return 0.0f;
}

float TidalPowerManager::GetDamageBonus(const glm::vec3& position) const {
    if (IsNearWater(position)) {
        return NagaConstants::TIDAL_DAMAGE_BONUS;
    }
    return 0.0f;
}

float TidalPowerManager::GetAbilityPowerBonus(const glm::vec3& position) const {
    if (IsNearWater(position)) {
        return NagaConstants::TIDAL_ABILITY_POWER_BONUS;
    }
    return 0.0f;
}

float TidalPowerManager::GetMovementModifier(const glm::vec3& position, bool isAmphibious) const {
    if (!isAmphibious) return 1.0f;

    if (IsInDeepWater(position)) {
        return 1.0f + NagaConstants::DEEP_WATER_SPEED_BONUS;
    }
    if (IsInWater(position)) {
        return 1.0f + NagaConstants::WATER_SPEED_BONUS;
    }
    return 1.0f;
}

float TidalPowerManager::GetHealthRegenRate(const glm::vec3& position) const {
    if (IsInWater(position)) {
        return NagaConstants::WATER_HEALTH_REGEN_PERCENT;
    }
    if (IsNearWater(position)) {
        return NagaConstants::NEAR_WATER_REGEN_BONUS;
    }
    return 0.0f;
}

void TidalPowerManager::Clear() {
    m_waterTiles.clear();
    m_waterPositionHashes.clear();
}

void TidalPowerManager::LoadFromMapData(const nlohmann::json& mapData) {
    Clear();
    if (mapData.contains("water_tiles")) {
        for (const auto& tile : mapData["water_tiles"]) {
            glm::vec3 pos(
                tile.value("x", 0.0f),
                tile.value("y", 0.0f),
                tile.value("z", 0.0f)
            );
            bool isDeep = tile.value("deep", false);
            RegisterWaterTile(pos, isDeep);
        }
    }
}

// ============================================================================
// AmphibiousComponent Implementation
// ============================================================================

void AmphibiousComponent::Update(float deltaTime, bool inWater) {
    if (isSubmerged) {
        if (!inWater) {
            isSubmerged = false;
            submergeDuration = 0.0f;
        } else {
            submergeDuration += deltaTime;
            if (submergeDuration >= maxSubmergeDuration) {
                isSubmerged = false;
            }
        }
    }
}

bool AmphibiousComponent::ToggleSubmerge(bool inWater) {
    if (!canDive || !inWater) return false;

    if (isSubmerged) {
        isSubmerged = false;
        submergeDuration = 0.0f;
    } else {
        isSubmerged = true;
        submergeDuration = 0.0f;
    }
    return true;
}

float AmphibiousComponent::GetMovementMultiplier(bool inWater, bool inDeepWater, bool inDesert) const {
    if (inDesert && !inWater) return desertPenalty;
    if (inDeepWater && canSwim) return swimSpeed * 1.1f;
    if (inWater && canSwim) return swimSpeed;
    return landSpeed;
}

// ============================================================================
// HeadComponent Implementation
// ============================================================================

void HeadComponent::Initialize(int count, int max, float damage) {
    headCount = count;
    maxHeads = max;
    damagePerHead = damage;
    headActive.resize(max, false);
    for (int i = 0; i < count; ++i) {
        headActive[i] = true;
    }
}

bool HeadComponent::LoseHead() {
    if (headCount <= 0) return false;

    for (int i = headCount - 1; i >= 0; --i) {
        if (headActive[i]) {
            headActive[i] = false;
            headCount--;

            // Start regen for this head
            if (twoHeadsPerLost && headCount < maxHeads - 1) {
                // Ancient Hydra: queue two heads to regrow
                currentRegenProgress = 0.0f;
            }
            return true;
        }
    }
    return false;
}

void HeadComponent::Update(float deltaTime) {
    if (headCount < maxHeads) {
        currentRegenProgress += deltaTime;
        if (currentRegenProgress >= regenTimePerHead) {
            currentRegenProgress = 0.0f;

            // Regrow head(s)
            int headsToGrow = twoHeadsPerLost ? 2 : 1;
            for (int i = 0; i < headsToGrow && headCount < maxHeads; ++i) {
                for (int j = 0; j < maxHeads; ++j) {
                    if (!headActive[j]) {
                        headActive[j] = true;
                        headCount++;
                        break;
                    }
                }
            }
        }
    }
}

// ============================================================================
// NagaRace Implementation
// ============================================================================

NagaRace& NagaRace::Instance() {
    static NagaRace instance;
    return instance;
}

bool NagaRace::Initialize(const std::string& configPath) {
    if (m_initialized) return true;

    m_configBasePath = configPath.empty() ? "game/assets/configs/races/naga/" : configPath;
    LoadConfiguration(m_configBasePath);

    m_initialized = true;
    return true;
}

void NagaRace::Shutdown() {
    VenomManager::Instance().Clear();
    TidalPowerManager::Instance().Clear();
    m_amphibiousComponents.clear();
    m_headComponents.clear();
    m_unitPositions.clear();
    m_initialized = false;
}

void NagaRace::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateVenom(deltaTime);
    UpdateAmphibious(deltaTime);
    UpdateHeads(deltaTime);
    UpdateWaterRegeneration(deltaTime);
    UpdateTidalPowerBonuses();
}

void NagaRace::UpdateVenom(float deltaTime) {
    VenomManager::Instance().Update(deltaTime);
}

void NagaRace::UpdateAmphibious(float deltaTime) {
    auto& tidalMgr = TidalPowerManager::Instance();
    for (auto& [unitId, component] : m_amphibiousComponents) {
        auto posIt = m_unitPositions.find(unitId);
        if (posIt != m_unitPositions.end()) {
            bool inWater = tidalMgr.IsInWater(posIt->second);
            component.Update(deltaTime, inWater);
        }
    }
}

void NagaRace::UpdateHeads(float deltaTime) {
    for (auto& [unitId, component] : m_headComponents) {
        int prevHeads = component.headCount;
        component.Update(deltaTime);
        if (component.headCount > prevHeads && m_onHeadRegenerated) {
            m_onHeadRegenerated(unitId, component.headCount);
        }
    }
}

void NagaRace::UpdateWaterRegeneration(float deltaTime) {
    auto& tidalMgr = TidalPowerManager::Instance();
    for (const auto& [unitId, position] : m_unitPositions) {
        float regenRate = tidalMgr.GetHealthRegenRate(position);
        if (regenRate > 0.0f) {
            // Apply regeneration (would interface with health system)
            // float healAmount = regenRate * maxHealth * deltaTime;
            // EntityManager::Instance().Heal(unitId, healAmount);
        }
    }
}

void NagaRace::UpdateTidalPowerBonuses() {
    auto& tidalMgr = TidalPowerManager::Instance();
    for (const auto& [unitId, position] : m_unitPositions) {
        float bonus = tidalMgr.GetTidalPowerBonus(position);
        if (bonus > 0.0f && m_onTidalPowerActivated) {
            m_onTidalPowerActivated(unitId, bonus);
        }
    }
}

void NagaRace::ApplyVenom(uint32_t targetId, uint32_t sourceId, float damage) {
    VenomManager::Instance().ApplyVenom(targetId, sourceId, damage, NagaConstants::VENOM_DURATION, NagaConstants::VENOM_MAX_STACKS);
    if (m_onVenomApplied) {
        m_onVenomApplied(targetId, VenomManager::Instance().GetVenomStacks(targetId));
    }
}

void NagaRace::ApplyEnhancedVenom(uint32_t targetId, uint32_t sourceId) {
    float damage = NagaConstants::BASE_VENOM_DAMAGE * 1.5f;
    float duration = NagaConstants::VENOM_DURATION * 1.2f;
    VenomManager::Instance().ApplyVenom(targetId, sourceId, damage, duration, NagaConstants::VENOM_MAX_STACKS);
    if (m_onVenomApplied) {
        m_onVenomApplied(targetId, VenomManager::Instance().GetVenomStacks(targetId));
    }
}

void NagaRace::ApplyNeurotoxin(uint32_t targetId, uint32_t sourceId, float slowAmount) {
    VenomManager::Instance().ApplyNeurotoxin(targetId, sourceId, NagaConstants::BASE_VENOM_DAMAGE * 2.0f, NagaConstants::VENOM_DURATION, slowAmount);
    if (m_onVenomApplied) {
        m_onVenomApplied(targetId, VenomManager::Instance().GetVenomStacks(targetId));
    }
}

float NagaRace::CalculateDamage(uint32_t attackerId, float baseDamage) const {
    auto posIt = m_unitPositions.find(attackerId);
    if (posIt != m_unitPositions.end()) {
        float bonus = TidalPowerManager::Instance().GetDamageBonus(posIt->second);
        return baseDamage * (1.0f + bonus);
    }
    return baseDamage;
}

float NagaRace::CalculateAbilityPower(uint32_t casterId, float baseAbilityPower) const {
    auto posIt = m_unitPositions.find(casterId);
    if (posIt != m_unitPositions.end()) {
        float bonus = TidalPowerManager::Instance().GetAbilityPowerBonus(posIt->second);
        return baseAbilityPower * (1.0f + bonus);
    }
    return baseAbilityPower;
}

void NagaRace::RegisterAmphibious(uint32_t unitId, const AmphibiousComponent& component) {
    m_amphibiousComponents[unitId] = component;
}

void NagaRace::UnregisterAmphibious(uint32_t unitId) {
    m_amphibiousComponents.erase(unitId);
}

AmphibiousComponent* NagaRace::GetAmphibious(uint32_t unitId) {
    auto it = m_amphibiousComponents.find(unitId);
    return it != m_amphibiousComponents.end() ? &it->second : nullptr;
}

bool NagaRace::ToggleSubmerge(uint32_t unitId) {
    auto* component = GetAmphibious(unitId);
    if (!component) return false;

    auto posIt = m_unitPositions.find(unitId);
    if (posIt == m_unitPositions.end()) return false;

    bool inWater = TidalPowerManager::Instance().IsInWater(posIt->second);
    return component->ToggleSubmerge(inWater);
}

void NagaRace::RegisterHeadComponent(uint32_t unitId, const HeadComponent& component) {
    m_headComponents[unitId] = component;
}

void NagaRace::UnregisterHeadComponent(uint32_t unitId) {
    m_headComponents.erase(unitId);
}

HeadComponent* NagaRace::GetHeadComponent(uint32_t unitId) {
    auto it = m_headComponents.find(unitId);
    return it != m_headComponents.end() ? &it->second : nullptr;
}

void NagaRace::OnHeadLost(uint32_t unitId) {
    auto* component = GetHeadComponent(unitId);
    if (component) {
        component->LoseHead();
    }
}

void NagaRace::OnKill(uint32_t killerUnitId) {
    // For Ancient Hydra: grow a head on kill
    auto* component = GetHeadComponent(killerUnitId);
    if (component && component->headCount < component->maxHeads) {
        for (int i = 0; i < component->maxHeads; ++i) {
            if (!component->headActive[i]) {
                component->headActive[i] = true;
                component->headCount++;
                if (m_onHeadRegenerated) {
                    m_onHeadRegenerated(killerUnitId, component->headCount);
                }
                break;
            }
        }
    }
}

float NagaRace::ApplyFireVulnerability(uint32_t targetId, float damage, const std::string& damageType) const {
    if (damageType == "fire") {
        return damage * NagaConstants::FIRE_DAMAGE_MULTIPLIER;
    }
    return damage;
}

float NagaRace::GetBuildingCost(const std::string& buildingType, const glm::vec3& position) const {
    float baseCost = NagaConstants::BUILDING_COST_MULTIPLIER;
    if (TidalPowerManager::Instance().IsNearWater(position)) {
        baseCost -= NagaConstants::WATER_ADJACENT_BUILDING_BONUS;
    }
    return baseCost;
}

float NagaRace::GetGatherRate(const std::string& resourceType, const glm::vec3& position) const {
    bool nearWater = TidalPowerManager::Instance().IsNearWater(position);
    bool inWater = TidalPowerManager::Instance().IsInWater(position);

    if (resourceType == "coral") {
        float rate = NagaConstants::CORAL_GATHER_RATE;
        if (nearWater) rate *= NagaConstants::CORAL_WATER_BONUS;
        return rate;
    }
    if (resourceType == "pearls") {
        float rate = NagaConstants::PEARL_GATHER_RATE;
        if (inWater) rate *= NagaConstants::PEARL_WATER_BONUS;
        return rate;
    }
    return 1.0f;
}

nlohmann::json NagaRace::LoadUnitConfig(const std::string& unitId) const {
    auto it = m_unitConfigs.find(unitId);
    if (it != m_unitConfigs.end()) {
        return it->second;
    }

    std::string path = m_configBasePath + "units/" + unitId + ".json";
    std::ifstream file(path);
    if (file.is_open()) {
        nlohmann::json config;
        file >> config;
        return config;
    }
    return nlohmann::json{};
}

nlohmann::json NagaRace::LoadBuildingConfig(const std::string& buildingId) const {
    auto it = m_buildingConfigs.find(buildingId);
    if (it != m_buildingConfigs.end()) {
        return it->second;
    }

    std::string path = m_configBasePath + "buildings/" + buildingId + ".json";
    std::ifstream file(path);
    if (file.is_open()) {
        nlohmann::json config;
        file >> config;
        return config;
    }
    return nlohmann::json{};
}

nlohmann::json NagaRace::LoadAbilityConfig(const std::string& abilityId) const {
    std::string path = m_configBasePath + "abilities/" + abilityId + ".json";
    std::ifstream file(path);
    if (file.is_open()) {
        nlohmann::json config;
        file >> config;
        return config;
    }
    return nlohmann::json{};
}

int NagaRace::GetUnitsWithTidalPower() const {
    int count = 0;
    auto& tidalMgr = TidalPowerManager::Instance();
    for (const auto& [unitId, position] : m_unitPositions) {
        if (tidalMgr.IsNearWater(position)) {
            count++;
        }
    }
    return count;
}

int NagaRace::GetTotalActiveHeads() const {
    int total = 0;
    for (const auto& [unitId, component] : m_headComponents) {
        total += component.headCount;
    }
    return total;
}

void NagaRace::LoadConfiguration(const std::string& configPath) {
    std::string raceConfigPath = configPath + "race_naga.json";
    std::ifstream file(raceConfigPath);
    if (file.is_open()) {
        file >> m_raceConfig;
    } else {
        InitializeDefaultConfigs();
    }
}

void NagaRace::InitializeDefaultConfigs() {
    m_raceConfig = {
        {"id", "race_naga"},
        {"name", "Depths of Nazjatar"},
        {"theme", "aquatic_serpentine"}
    };
}

// ============================================================================
// Ability Implementations
// ============================================================================

bool TidalWaveAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return context.casterMana >= data.manaCost;
}

AbilityCastResult TidalWaveAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;
    result.success = true;
    // Implementation would create wave projectile
    return result;
}

AbilityCastResult NagaFrostNovaAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;
    result.success = true;
    // Implementation would freeze nearby enemies
    return result;
}

bool WhirlpoolAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return context.casterMana >= data.manaCost;
}

AbilityCastResult WhirlpoolAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;
    result.success = true;

    WhirlpoolInstance instance;
    instance.position = context.targetPosition;
    instance.remainingDuration = 8.0f;
    instance.pullStrength = 4.0f;
    instance.damagePerSecond = 50.0f;
    instance.tickTimer = 0.0f;
    m_activeWhirlpools.push_back(instance);

    return result;
}

void WhirlpoolAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    for (auto& pool : m_activeWhirlpools) {
        pool.remainingDuration -= deltaTime;
        pool.tickTimer += deltaTime;
        if (pool.tickTimer >= 1.0f) {
            pool.tickTimer = 0.0f;
            // Apply pull and damage to nearby enemies
        }
    }

    m_activeWhirlpools.erase(
        std::remove_if(m_activeWhirlpools.begin(), m_activeWhirlpools.end(),
            [](const WhirlpoolInstance& p) { return p.remainingDuration <= 0.0f; }),
        m_activeWhirlpools.end()
    );
}

void WhirlpoolAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    m_activeWhirlpools.clear();
}

bool SongOfSirenAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return context.casterMana >= data.manaCost;
}

AbilityCastResult SongOfSirenAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;
    result.success = true;
    // Implementation would sleep nearby enemies
    return result;
}

AbilityCastResult RavageAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;
    result.success = true;
    // Implementation would spawn tentacles
    return result;
}

void RavageAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    // Update tentacle expansion
}

bool MassCharmAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return context.casterMana >= data.manaCost;
}

AbilityCastResult MassCharmAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;
    result.success = true;
    // Implementation would charm enemies
    return result;
}

void MassCharmAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    m_charmedUnits.clear();
}

AbilityCastResult KrakenWrathAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;
    result.success = true;
    // Implementation would deal global damage
    return result;
}

} // namespace RTS
} // namespace Vehement
