#include "VampireRace.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>

namespace Vehement {
namespace RTS {

// ============================================================================
// Day/Night Manager Implementation
// ============================================================================

DayNightManager& DayNightManager::Instance() {
    static DayNightManager instance;
    return instance;
}

void DayNightManager::Update(float deltaTime) {
    // Update forced night
    if (m_forcedNightDuration > 0.0f) {
        m_forcedNightDuration -= deltaTime;
        if (m_forcedNightDuration <= 0.0f) {
            m_forcedNightDuration = 0.0f;
        }
        return; // Don't update normal cycle during forced night
    }

    // Update cycle timer
    m_cycleTimer += deltaTime;

    TimeOfDay previousTime = m_currentTime;

    // Determine current time of day
    if (m_currentTime == TimeOfDay::Day) {
        if (m_cycleTimer >= VampireConstants::DAY_DURATION) {
            m_cycleTimer = 0.0f;
            m_cycleDuration = VampireConstants::TWILIGHT_TRANSITION;
            m_currentTime = TimeOfDay::Twilight;
        }
    } else if (m_currentTime == TimeOfDay::Twilight) {
        if (m_cycleTimer >= VampireConstants::TWILIGHT_TRANSITION) {
            m_cycleTimer = 0.0f;
            // Alternate between going to night and going to day
            if (m_cycleDuration == VampireConstants::TWILIGHT_TRANSITION) {
                m_cycleDuration = VampireConstants::NIGHT_DURATION;
                m_currentTime = TimeOfDay::Night;
            }
        }
    } else { // Night
        if (m_cycleTimer >= VampireConstants::NIGHT_DURATION) {
            m_cycleTimer = 0.0f;
            m_cycleDuration = VampireConstants::DAY_DURATION;
            m_currentTime = TimeOfDay::Day;
        }
    }

    // Notify on change
    if (previousTime != m_currentTime && m_onTimeChange) {
        m_onTimeChange(m_currentTime);
    }
}

float DayNightManager::GetVampireStatModifier() const {
    if (m_forcedNightDuration > 0.0f || m_currentTime == TimeOfDay::Night) {
        return 1.0f + VampireConstants::NIGHT_BONUS_STATS;
    } else if (m_currentTime == TimeOfDay::Day) {
        return 1.0f - VampireConstants::DAY_PENALTY_STATS;
    } else { // Twilight
        return 1.0f; // Neutral during transition
    }
}

void DayNightManager::ForceNight(float duration) {
    m_forcedNightDuration = duration;

    if (m_currentTime != TimeOfDay::Night && m_onTimeChange) {
        m_onTimeChange(TimeOfDay::Night);
    }
}

// ============================================================================
// Blood Resource Manager Implementation
// ============================================================================

BloodResourceManager& BloodResourceManager::Instance() {
    static BloodResourceManager instance;
    return instance;
}

void BloodResourceManager::Update(float deltaTime) {
    // Blood decay
    m_bloodDecayAccumulator += deltaTime;
    float decayInterval = 60.0f; // Decay per minute

    while (m_bloodDecayAccumulator >= decayInterval) {
        m_bloodDecayAccumulator -= decayInterval;
        int decayAmount = static_cast<int>(VampireConstants::BLOOD_DECAY_RATE);
        m_currentBlood = std::max(0, m_currentBlood - decayAmount);
    }
}

void BloodResourceManager::AddBlood(int amount) {
    m_currentBlood = std::min(m_currentBlood + amount, m_maxBlood);
}

bool BloodResourceManager::SpendBlood(int amount) {
    int adjustedCost = static_cast<int>(amount * m_bloodEfficiency);
    if (m_currentBlood >= adjustedCost) {
        m_currentBlood -= adjustedCost;
        return true;
    }
    return false;
}

void BloodResourceManager::OnUnitKilled(const std::string& unitType, bool isWorker, bool isHero) {
    int bloodGain = VampireConstants::BLOOD_PER_KILL;

    if (isHero) {
        bloodGain = VampireConstants::BLOOD_PER_HERO_KILL;
    } else if (isWorker) {
        bloodGain = VampireConstants::BLOOD_PER_WORKER_KILL;
    }

    AddBlood(bloodGain);
}

// ============================================================================
// Transformation Manager Implementation
// ============================================================================

TransformationManager& TransformationManager::Instance() {
    static TransformationManager instance;
    return instance;
}

void TransformationManager::Update(float deltaTime) {
    std::vector<uint32_t> toRevert;

    for (auto& [entityId, state] : m_transformStates) {
        // Update cooldown
        if (state.transformCooldown > 0.0f) {
            state.transformCooldown -= deltaTime;
        }

        // Update temporary form duration
        if (state.isTemporary && state.currentForm != VampireForm::Humanoid) {
            state.formDuration -= deltaTime;
            if (state.formDuration <= 0.0f) {
                toRevert.push_back(entityId);
            }
        }
    }

    // Revert expired transformations
    for (uint32_t id : toRevert) {
        RevertForm(id);
    }
}

bool TransformationManager::Transform(uint32_t entityId, VampireForm newForm, float duration) {
    auto it = m_transformStates.find(entityId);
    if (it == m_transformStates.end()) {
        return false;
    }

    if (it->second.transformCooldown > 0.0f) {
        return false;
    }

    it->second.currentForm = newForm;
    it->second.transformCooldown = VampireConstants::TRANSFORM_COOLDOWN;
    it->second.isTemporary = (duration > 0.0f);
    it->second.formDuration = duration;

    return true;
}

bool TransformationManager::RevertForm(uint32_t entityId) {
    auto it = m_transformStates.find(entityId);
    if (it == m_transformStates.end()) {
        return false;
    }

    it->second.currentForm = VampireForm::Humanoid;
    it->second.isTemporary = false;
    it->second.formDuration = 0.0f;

    return true;
}

VampireForm TransformationManager::GetForm(uint32_t entityId) const {
    auto it = m_transformStates.find(entityId);
    if (it != m_transformStates.end()) {
        return it->second.currentForm;
    }
    return VampireForm::Humanoid;
}

bool TransformationManager::CanTransform(uint32_t entityId) const {
    auto it = m_transformStates.find(entityId);
    if (it == m_transformStates.end()) {
        return false;
    }
    return it->second.transformCooldown <= 0.0f;
}

const TransformationState* TransformationManager::GetState(uint32_t entityId) const {
    auto it = m_transformStates.find(entityId);
    return (it != m_transformStates.end()) ? &it->second : nullptr;
}

void TransformationManager::RegisterEntity(uint32_t entityId) {
    m_transformStates[entityId] = TransformationState{};
}

void TransformationManager::UnregisterEntity(uint32_t entityId) {
    m_transformStates.erase(entityId);
}

TransformationManager::FormModifiers TransformationManager::GetFormModifiers(VampireForm form) const {
    FormModifiers mods;

    switch (form) {
        case VampireForm::Bat:
            mods.speedBonus = VampireConstants::BAT_FORM_SPEED_BONUS;
            mods.damageBonus = -0.5f; // Penalty
            mods.armorBonus = -3.0f;  // Penalty
            mods.isFlying = true;
            break;

        case VampireForm::Wolf:
            mods.speedBonus = VampireConstants::WOLF_FORM_SPEED_BONUS;
            mods.damageBonus = VampireConstants::WOLF_FORM_DAMAGE_BONUS;
            break;

        case VampireForm::Mist:
            mods.speedBonus = 0.3f;
            mods.isInvulnerable = true;
            mods.canAttack = false;
            mods.canPassThroughUnits = true;
            break;

        case VampireForm::Swarm:
            mods.speedBonus = 0.5f;
            mods.isInvulnerable = true;
            mods.canPassThroughUnits = true;
            break;

        default:
            break;
    }

    return mods;
}

// ============================================================================
// Domination Manager Implementation
// ============================================================================

DominationManager& DominationManager::Instance() {
    static DominationManager instance;
    return instance;
}

void DominationManager::Update(float deltaTime) {
    std::vector<uint32_t> toRelease;

    for (auto& [unitId, info] : m_dominatedUnits) {
        if (info.duration > 0.0f) {
            info.duration -= deltaTime;
            if (info.duration <= 0.0f) {
                toRelease.push_back(unitId);
            }
        }
    }

    for (uint32_t id : toRelease) {
        Release(id);
    }
}

bool DominationManager::Dominate(uint32_t targetId, uint32_t dominatorId, float duration) {
    if (!CanDominateMore()) {
        return false;
    }

    DominatedUnit info;
    info.unitId = targetId;
    info.dominatorId = dominatorId;
    info.duration = duration;
    info.statPenalty = VampireConstants::DOMINATED_STAT_PENALTY;

    m_dominatedUnits[targetId] = info;

    return true;
}

void DominationManager::Release(uint32_t unitId) {
    m_dominatedUnits.erase(unitId);
}

bool DominationManager::IsDominated(uint32_t unitId) const {
    return m_dominatedUnits.count(unitId) > 0;
}

const DominatedUnit* DominationManager::GetDominationInfo(uint32_t unitId) const {
    auto it = m_dominatedUnits.find(unitId);
    return (it != m_dominatedUnits.end()) ? &it->second : nullptr;
}

bool DominationManager::CanDominateMore() const {
    return m_dominatedUnits.size() < static_cast<size_t>(VampireConstants::MAX_DOMINATED_UNITS);
}

// ============================================================================
// Hero Revival Manager Implementation
// ============================================================================

HeroRevivalManager& HeroRevivalManager::Instance() {
    static HeroRevivalManager instance;
    return instance;
}

void HeroRevivalManager::Update(float deltaTime) {
    std::vector<size_t> toRemove;

    for (size_t i = 0; i < m_revivingHeroes.size(); ++i) {
        m_revivingHeroes[i].timeRemaining -= deltaTime;

        if (m_revivingHeroes[i].timeRemaining <= 0.0f) {
            // Hero revives at coffin vault
            // Would spawn hero at m_coffinVaultPosition
            toRemove.push_back(i);
        }
    }

    // Remove revived heroes (reverse order to preserve indices)
    for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
        m_revivingHeroes.erase(m_revivingHeroes.begin() + *it);
    }
}

void HeroRevivalManager::OnHeroDeath(uint32_t heroId, int level, const glm::vec3& deathPosition) {
    if (!HasCoffinVault()) {
        return;
    }

    float baseTime = VampireConstants::HERO_REVIVE_TIME_BASE;
    float levelTime = VampireConstants::HERO_REVIVE_TIME_PER_LEVEL * level;
    float totalTime = (baseTime + levelTime) * (1.0f - m_revivalTimeReduction);

    RevivingHero hero;
    hero.heroId = heroId;
    hero.level = level;
    hero.timeRemaining = totalTime;
    hero.deathPosition = deathPosition;

    m_revivingHeroes.push_back(hero);
}

float HeroRevivalManager::GetRevivalTimeRemaining(uint32_t heroId) const {
    for (const auto& hero : m_revivingHeroes) {
        if (hero.heroId == heroId) {
            return hero.timeRemaining;
        }
    }
    return -1.0f; // Not reviving
}

bool HeroRevivalManager::InstantRevive(uint32_t heroId) {
    for (auto it = m_revivingHeroes.begin(); it != m_revivingHeroes.end(); ++it) {
        if (it->heroId == heroId) {
            // Calculate cost and check resources
            // Spawn hero at coffin vault
            m_revivingHeroes.erase(it);
            return true;
        }
    }
    return false;
}

void HeroRevivalManager::SetCoffinVault(uint32_t buildingId, const glm::vec3& position) {
    m_coffinVaultId = buildingId;
    m_coffinVaultPosition = position;
}

// ============================================================================
// Vampire Race Implementation
// ============================================================================

VampireRace& VampireRace::Instance() {
    static VampireRace instance;
    return instance;
}

bool VampireRace::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Load race configuration
    if (!LoadConfiguration("game/assets/configs/races/vampires/race_vampires.json")) {
        return false;
    }

    // Register vampire abilities
    RegisterVampireAbilities();

    m_initialized = true;
    return true;
}

void VampireRace::Shutdown() {
    m_vampireUnits.clear();
    m_unitTypes.clear();
    m_sunlightVulnerableUnits.clear();
    m_buildings.clear();
    m_initialized = false;
}

void VampireRace::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update day/night cycle
    DayNightManager::Instance().Update(deltaTime);

    // Update blood resource
    BloodResourceManager::Instance().Update(deltaTime);

    // Update transformations
    TransformationManager::Instance().Update(deltaTime);

    // Update dominated units
    DominationManager::Instance().Update(deltaTime);

    // Update hero revival
    HeroRevivalManager::Instance().Update(deltaTime);

    // Apply day/night effects
    ApplyTimeOfDayEffects();

    // Apply sunlight damage during day
    if (DayNightManager::Instance().IsDay()) {
        ApplySunlightDamage(deltaTime);
    }
}

bool VampireRace::IsVampireUnit(uint32_t entityId) const {
    return m_vampireUnits.count(entityId) > 0;
}

void VampireRace::RegisterVampireUnit(uint32_t entityId, const std::string& unitType) {
    m_vampireUnits.insert(entityId);
    m_unitTypes[entityId] = unitType;

    // Check if can transform
    if (UnitCanTransform(unitType)) {
        TransformationManager::Instance().RegisterEntity(entityId);
    }

    // Check if vulnerable to sunlight
    if (UnitVulnerableToSunlight(unitType)) {
        m_sunlightVulnerableUnits.insert(entityId);
    }
}

void VampireRace::UnregisterVampireUnit(uint32_t entityId) {
    m_vampireUnits.erase(entityId);
    m_unitTypes.erase(entityId);
    m_sunlightVulnerableUnits.erase(entityId);
    TransformationManager::Instance().UnregisterEntity(entityId);
}

void VampireRace::ApplyVampireBonuses(uint32_t entityId) {
    // Apply vampire-specific bonuses:
    // - Life steal
    // - Holy vulnerability
    // - Fire vulnerability
    // - Poison immunity
    // - Day/night stat modifiers
    // This would integrate with the entity/component system
}

float VampireRace::GetDamageMultiplier(const std::string& damageType) const {
    if (damageType == "holy" || damageType == "light") {
        return VampireConstants::HOLY_DAMAGE_MULTIPLIER;
    }
    if (damageType == "fire") {
        return VampireConstants::FIRE_DAMAGE_MULTIPLIER;
    }
    if (damageType == "ice" || damageType == "frost") {
        return VampireConstants::ICE_DAMAGE_MULTIPLIER;
    }
    if (damageType == "poison") {
        return VampireConstants::POISON_DAMAGE_MULTIPLIER;
    }
    return 1.0f;
}

float VampireRace::CalculateLifeSteal(uint32_t entityId, float damageDealt) const {
    auto it = m_unitTypes.find(entityId);
    if (it == m_unitTypes.end()) {
        return 0.0f;
    }

    float baseLifeSteal = GetBaseLifeSteal(it->second);
    float totalLifeSteal = baseLifeSteal + m_bonusLifeSteal;
    totalLifeSteal = std::min(totalLifeSteal, VampireConstants::MAX_LIFE_STEAL);

    // Night bonus to life steal
    if (DayNightManager::Instance().IsNight()) {
        totalLifeSteal += 0.1f; // +10% at night
    }

    return damageDealt * totalLifeSteal;
}

void VampireRace::ApplyTimeOfDayEffects() {
    float modifier = DayNightManager::Instance().GetVampireStatModifier();

    // Apply modifier to all vampire units
    // This would integrate with the entity/stat system
    for (uint32_t entityId : m_vampireUnits) {
        // Apply stat modifiers based on time of day
    }
}

void VampireRace::ApplySunlightDamage(float deltaTime) {
    float damage = VampireConstants::SUN_DAMAGE_PER_SECOND * deltaTime;

    for (uint32_t entityId : m_sunlightVulnerableUnits) {
        // Apply damage to entity
        // This would integrate with the health/damage system
    }
}

bool VampireRace::IsVulnerableToSunlight(uint32_t entityId) const {
    return m_sunlightVulnerableUnits.count(entityId) > 0;
}

void VampireRace::OnUnitKilled(uint32_t killerId, uint32_t victimId, const std::string& victimType) {
    // Check if killer is a vampire
    if (!IsVampireUnit(killerId)) {
        return;
    }

    // Determine blood gain
    bool isWorker = (victimType.find("worker") != std::string::npos ||
                     victimType.find("peasant") != std::string::npos ||
                     victimType.find("thrall") != std::string::npos);
    bool isHero = (victimType.find("hero") != std::string::npos);

    BloodResourceManager::Instance().OnUnitKilled(victimType, isWorker, isHero);
}

int VampireRace::GetPopulationCap() const {
    int cap = VampireConstants::BASE_POPULATION_CAP;
    cap += m_darkObeliskCount * VampireConstants::DARK_OBELISK_POPULATION;
    cap += m_bloodFountainCount * VampireConstants::BLOOD_FOUNTAIN_POPULATION;
    return std::min(cap, VampireConstants::MAX_POPULATION);
}

void VampireRace::OnBuildingConstructed(uint32_t buildingId, const std::string& buildingType,
                                         const glm::vec3& position) {
    m_buildings[buildingId] = buildingType;

    // Track population buildings
    if (buildingType == "dark_obelisk" || buildingType == "tower_of_blood") {
        ++m_darkObeliskCount;
    } else if (buildingType == "blood_fountain") {
        ++m_bloodFountainCount;
    } else if (buildingType == "coffin_vault") {
        HeroRevivalManager::Instance().SetCoffinVault(buildingId, position);
    }
}

void VampireRace::OnBuildingDestroyed(uint32_t buildingId) {
    auto it = m_buildings.find(buildingId);
    if (it != m_buildings.end()) {
        // Track population buildings
        if (it->second == "dark_obelisk" || it->second == "tower_of_blood") {
            --m_darkObeliskCount;
        } else if (it->second == "blood_fountain") {
            --m_bloodFountainCount;
        }
        m_buildings.erase(it);
    }
}

bool VampireRace::LoadConfiguration(const std::string& configPath) {
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

const nlohmann::json* VampireRace::GetUnitConfig(const std::string& unitType) const {
    auto it = m_unitConfigs.find(unitType);
    return (it != m_unitConfigs.end()) ? &it->second : nullptr;
}

const nlohmann::json* VampireRace::GetBuildingConfig(const std::string& buildingType) const {
    auto it = m_buildingConfigs.find(buildingType);
    return (it != m_buildingConfigs.end()) ? &it->second : nullptr;
}

const nlohmann::json* VampireRace::GetHeroConfig(const std::string& heroType) const {
    auto it = m_heroConfigs.find(heroType);
    return (it != m_heroConfigs.end()) ? &it->second : nullptr;
}

// ============================================================================
// Ability Implementations
// ============================================================================

bool LifeDrainAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return AbilityBehavior::CanCast(context, data) && context.targetUnit != nullptr;
}

AbilityCastResult LifeDrainAbility::Execute(const AbilityCastContext& context,
                                             const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    m_targetId = context.targetUnit->GetId();
    m_channelTime = 0.0f;
    result.success = true;

    return result;
}

void LifeDrainAbility::Update(const AbilityCastContext& context, const AbilityData& data,
                               float deltaTime) {
    const auto& levelData = data.GetLevelData(context.abilityLevel);

    m_channelTime += deltaTime;

    // Deal damage and heal
    float damage = levelData.damage * deltaTime;
    float heal = damage; // 100% life steal on drain

    // Apply damage to target
    // Apply heal to caster
}

void LifeDrainAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    m_targetId = 0;
    m_channelTime = 0.0f;
}

AbilityCastResult TransformAbility::Execute(const AbilityCastContext& context,
                                             const AbilityData& data) {
    AbilityCastResult result;

    auto& transformManager = TransformationManager::Instance();

    // Determine target form from ability data
    VampireForm targetForm = VampireForm::Bat; // Would be parsed from data
    float duration = data.GetLevelData(context.abilityLevel).duration;

    if (transformManager.Transform(context.casterId, targetForm, duration)) {
        result.success = true;
    } else {
        result.success = false;
        result.failReason = "Cannot transform yet";
    }

    return result;
}

bool DominateAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    if (!context.targetUnit) {
        return false;
    }

    // Check if target is valid (not hero, not mechanical, etc.)
    // Check if we can dominate more units

    return DominationManager::Instance().CanDominateMore();
}

AbilityCastResult DominateAbility::Execute(const AbilityCastContext& context,
                                            const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);
    float duration = levelData.duration; // -1 for permanent

    if (DominationManager::Instance().Dominate(context.targetUnit->GetId(),
                                                context.casterId, duration)) {
        result.success = true;
        result.unitsAffected = 1;
    } else {
        result.success = false;
        result.failReason = "Cannot dominate more units";
    }

    return result;
}

AbilityCastResult BloodStormAbility::Execute(const AbilityCastContext& context,
                                              const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    m_wavesRemaining = levelData.summonCount; // Using summonCount for wave count
    m_waveTimer = 0.0f;

    result.success = true;

    return result;
}

void BloodStormAbility::Update(const AbilityCastContext& context, const AbilityData& data,
                                float deltaTime) {
    if (m_wavesRemaining <= 0) {
        return;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);
    float waveInterval = levelData.duration / levelData.summonCount;

    m_waveTimer += deltaTime;

    while (m_waveTimer >= waveInterval && m_wavesRemaining > 0) {
        m_waveTimer -= waveInterval;
        --m_wavesRemaining;

        // Deal damage wave in radius
        // Heal allies in radius
    }
}

bool ShadowStepAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    return context.targetUnit != nullptr;
}

AbilityCastResult ShadowStepAbility::Execute(const AbilityCastContext& context,
                                              const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Teleport behind target
    // Would calculate position behind target and move caster

    // Apply bonus damage
    result.success = true;
    result.damageDealt = levelData.damage;
    result.unitsAffected = 1;

    return result;
}

AbilityCastResult CrimsonNightAbility::Execute(const AbilityCastContext& context,
                                                const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);
    m_remainingDuration = levelData.duration;

    // Force night in the area
    DayNightManager::Instance().ForceNight(levelData.duration);

    result.success = true;

    return result;
}

void CrimsonNightAbility::Update(const AbilityCastContext& context, const AbilityData& data,
                                  float deltaTime) {
    const auto& levelData = data.GetLevelData(context.abilityLevel);

    m_remainingDuration -= deltaTime;

    // Deal damage to enemies in radius
    // Heal allies in radius
    // Apply night bonuses to vampires in radius
}

void CrimsonNightAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    m_remainingDuration = 0.0f;
    // Night will revert automatically through DayNightManager
}

// ============================================================================
// Utility Functions
// ============================================================================

void RegisterVampireAbilities() {
    auto& manager = AbilityManager::Instance();

    // Register custom ability behaviors
    manager.RegisterBehavior(200, std::make_unique<LifeDrainAbility>());
    manager.RegisterBehavior(201, std::make_unique<TransformAbility>());
    manager.RegisterBehavior(202, std::make_unique<DominateAbility>());
    manager.RegisterBehavior(203, std::make_unique<BloodStormAbility>());
    manager.RegisterBehavior(204, std::make_unique<ShadowStepAbility>());
    manager.RegisterBehavior(205, std::make_unique<CrimsonNightAbility>());
}

bool UnitCanTransform(const std::string& unitType) {
    static const std::unordered_set<std::string> transformableUnits = {
        "vampire_count", "vampire_knight", "elder_vampire", "vampire_lord",
        "shadow_lord", "nosferatu_elder", "nightmare"
    };

    return transformableUnits.count(unitType) > 0;
}

bool UnitVulnerableToSunlight(const std::string& unitType) {
    static const std::unordered_set<std::string> sunlightVulnerable = {
        "vampire_spawn", "nosferatu", "elder_vampire", "ancient_one",
        "vampire_count", "shadow_lord", "nosferatu_elder", "blood_dragon"
    };

    return sunlightVulnerable.count(unitType) > 0;
}

float GetBaseLifeSteal(const std::string& unitType) {
    static const std::unordered_map<std::string, float> lifeStealValues = {
        {"thrall", 0.0f},
        {"vampire_spawn", 0.20f},
        {"blood_seeker", 0.15f},
        {"night_creature", 0.10f},
        {"vampire_knight", 0.20f},
        {"blood_mage", 0.15f},
        {"nosferatu", 0.25f},
        {"shadow_dancer", 0.20f},
        {"gargoyle_servant", 0.0f},
        {"elder_vampire", 0.30f},
        {"blood_countess", 0.20f},
        {"nightmare", 0.20f},
        {"crimson_bat", 0.25f},
        {"flesh_golem", 0.0f},
        {"ancient_one", 0.35f},
        {"blood_dragon", 0.30f},
        {"vampire_lord", 0.30f},
        {"abyssal_horror", 0.20f},
        {"vampire_count", 0.25f},
        {"blood_queen", 0.20f},
        {"shadow_lord", 0.25f},
        {"nosferatu_elder", 0.30f}
    };

    auto it = lifeStealValues.find(unitType);
    return (it != lifeStealValues.end()) ? it->second : VampireConstants::BASE_LIFE_STEAL;
}

} // namespace RTS
} // namespace Vehement
