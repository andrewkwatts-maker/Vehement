#include "FairyRace.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// DayNightCycle Implementation
// ============================================================================

void DayNightCycle::Update(float deltaTime) {
    m_cycleTime += deltaTime;

    float totalCycleTime = FairyConstants::DAY_DURATION + FairyConstants::NIGHT_DURATION +
                          (2.0f * FairyConstants::DUSK_DAWN_DURATION);

    // Wrap cycle time
    while (m_cycleTime >= totalCycleTime) {
        m_cycleTime -= totalCycleTime;
    }

    // Determine current time of day
    float t = m_cycleTime;

    if (t < FairyConstants::DAY_DURATION) {
        m_currentTime = TimeOfDay::Day;
        m_transitionProgress = 0.0f;
    } else if (t < FairyConstants::DAY_DURATION + FairyConstants::DUSK_DAWN_DURATION) {
        m_currentTime = TimeOfDay::Dusk;
        m_transitionProgress = (t - FairyConstants::DAY_DURATION) / FairyConstants::DUSK_DAWN_DURATION;
    } else if (t < FairyConstants::DAY_DURATION + FairyConstants::DUSK_DAWN_DURATION + FairyConstants::NIGHT_DURATION) {
        m_currentTime = TimeOfDay::Night;
        m_transitionProgress = 1.0f;
    } else {
        m_currentTime = TimeOfDay::Dawn;
        float dawnStart = FairyConstants::DAY_DURATION + FairyConstants::DUSK_DAWN_DURATION + FairyConstants::NIGHT_DURATION;
        m_transitionProgress = 1.0f - ((t - dawnStart) / FairyConstants::DUSK_DAWN_DURATION);
    }
}

float DayNightCycle::GetNightIntensity() const {
    switch (m_currentTime) {
        case TimeOfDay::Day:
            return 0.0f;
        case TimeOfDay::Night:
            return 1.0f;
        case TimeOfDay::Dusk:
            return m_transitionProgress;
        case TimeOfDay::Dawn:
            return m_transitionProgress;
        default:
            return 0.0f;
    }
}

void DayNightCycle::SetTimeOfDay(TimeOfDay time) {
    m_currentTime = time;
    switch (time) {
        case TimeOfDay::Day:
            m_cycleTime = 0.0f;
            m_transitionProgress = 0.0f;
            break;
        case TimeOfDay::Dusk:
            m_cycleTime = FairyConstants::DAY_DURATION;
            m_transitionProgress = 0.5f;
            break;
        case TimeOfDay::Night:
            m_cycleTime = FairyConstants::DAY_DURATION + FairyConstants::DUSK_DAWN_DURATION;
            m_transitionProgress = 1.0f;
            break;
        case TimeOfDay::Dawn:
            m_cycleTime = FairyConstants::DAY_DURATION + FairyConstants::DUSK_DAWN_DURATION + FairyConstants::NIGHT_DURATION;
            m_transitionProgress = 0.5f;
            break;
    }
}

void DayNightCycle::Reset() {
    m_currentTime = TimeOfDay::Day;
    m_cycleTime = 0.0f;
    m_transitionProgress = 0.0f;
}

// ============================================================================
// NightPowerComponent Implementation
// ============================================================================

float NightPowerComponent::GetDamageMultiplier() const {
    float nightIntensity = DayNightCycle::Instance().GetNightIntensity();

    // Base calculation
    float nightBonus = (baseDamageBonus + talentDamageBonus) * nightIntensity;
    float dayPenaltyApplied = dayPenalty * (1.0f - nightIntensity);

    // Eternal Twilight talent: night bonuses partially active during day
    if (talentDayBonus > 0.0f && nightIntensity < 1.0f) {
        nightBonus += (baseDamageBonus + talentDamageBonus) * talentDayBonus * (1.0f - nightIntensity);
    }

    return 1.0f + nightBonus - dayPenaltyApplied;
}

float NightPowerComponent::GetHealingMultiplier() const {
    float nightIntensity = DayNightCycle::Instance().GetNightIntensity();
    float nightBonus = baseHealingBonus * nightIntensity;

    if (talentDayBonus > 0.0f && nightIntensity < 1.0f) {
        nightBonus += baseHealingBonus * talentDayBonus * (1.0f - nightIntensity);
    }

    return 1.0f + nightBonus;
}

float NightPowerComponent::GetSpeedMultiplier() const {
    float nightIntensity = DayNightCycle::Instance().GetNightIntensity();
    float nightBonus = baseSpeedBonus * nightIntensity;

    if (talentDayBonus > 0.0f && nightIntensity < 1.0f) {
        nightBonus += baseSpeedBonus * talentDayBonus * (1.0f - nightIntensity);
    }

    return 1.0f + nightBonus;
}

float NightPowerComponent::GetRegenMultiplier() const {
    float nightIntensity = DayNightCycle::Instance().GetNightIntensity();
    float nightBonus = baseRegenBonus * nightIntensity;

    if (talentDayBonus > 0.0f && nightIntensity < 1.0f) {
        nightBonus += baseRegenBonus * talentDayBonus * (1.0f - nightIntensity);
    }

    return 1.0f + nightBonus;
}

nlohmann::json NightPowerComponent::ToJson() const {
    return {
        {"baseDamageBonus", baseDamageBonus},
        {"baseHealingBonus", baseHealingBonus},
        {"baseSpeedBonus", baseSpeedBonus},
        {"baseRegenBonus", baseRegenBonus},
        {"dayPenalty", dayPenalty},
        {"talentDamageBonus", talentDamageBonus},
        {"talentDayBonus", talentDayBonus}
    };
}

NightPowerComponent NightPowerComponent::FromJson(const nlohmann::json& j) {
    NightPowerComponent comp;
    comp.baseDamageBonus = j.value("baseDamageBonus", FairyConstants::NIGHT_DAMAGE_BONUS);
    comp.baseHealingBonus = j.value("baseHealingBonus", FairyConstants::NIGHT_HEALING_BONUS);
    comp.baseSpeedBonus = j.value("baseSpeedBonus", FairyConstants::NIGHT_SPEED_BONUS);
    comp.baseRegenBonus = j.value("baseRegenBonus", FairyConstants::NIGHT_REGEN_BONUS);
    comp.dayPenalty = j.value("dayPenalty", FairyConstants::DAY_PENALTY);
    comp.talentDamageBonus = j.value("talentDamageBonus", 0.0f);
    comp.talentDayBonus = j.value("talentDayBonus", 0.0f);
    return comp;
}

// ============================================================================
// IllusionManager Implementation
// ============================================================================

uint32_t IllusionManager::CreateIllusion(uint32_t sourceUnitId, const std::string& unitType,
                                          const glm::vec3& position, float duration,
                                          float damageDealt, float damageTaken) {
    uint32_t illusionId = m_nextIllusionId++;

    IllusionInstance illusion;
    illusion.illusionId = illusionId;
    illusion.sourceUnitId = sourceUnitId;
    illusion.unitType = unitType;
    illusion.remainingDuration = duration;
    illusion.damageDealtMultiplier = damageDealt;
    illusion.damageTakenMultiplier = damageTaken;
    illusion.isRevealed = false;

    m_illusions[illusionId] = illusion;
    m_sourceToIllusions[sourceUnitId].push_back(illusionId);

    return illusionId;
}

std::vector<uint32_t> IllusionManager::CreateMirrorImages(uint32_t sourceUnitId, int count, float duration) {
    std::vector<uint32_t> created;

    // Get source unit info (would integrate with entity system)
    std::string unitType = "unknown"; // Would be retrieved from entity system

    for (int i = 0; i < count; ++i) {
        // Calculate offset position
        float angle = (2.0f * 3.14159f * i) / count;
        float radius = 1.5f;
        glm::vec3 offset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        glm::vec3 position = offset; // Would add to source unit position

        uint32_t id = CreateIllusion(sourceUnitId, unitType, position, duration);
        if (id > 0) {
            created.push_back(id);
        }
    }

    return created;
}

std::vector<uint32_t> IllusionManager::CreateMassIllusion(const glm::vec3& center, float radius,
                                                           float duration, int copiesPerUnit) {
    std::vector<uint32_t> created;

    // Would find all friendly units in radius and create illusions
    // This is a placeholder implementation

    return created;
}

void IllusionManager::DestroyIllusion(uint32_t illusionId) {
    auto it = m_illusions.find(illusionId);
    if (it != m_illusions.end()) {
        uint32_t sourceId = it->second.sourceUnitId;
        m_illusions.erase(it);

        // Remove from source mapping
        auto sourceIt = m_sourceToIllusions.find(sourceId);
        if (sourceIt != m_sourceToIllusions.end()) {
            auto& vec = sourceIt->second;
            vec.erase(std::remove(vec.begin(), vec.end(), illusionId), vec.end());
            if (vec.empty()) {
                m_sourceToIllusions.erase(sourceIt);
            }
        }
    }
}

void IllusionManager::DestroyIllusionsFromSource(uint32_t sourceUnitId) {
    auto it = m_sourceToIllusions.find(sourceUnitId);
    if (it != m_sourceToIllusions.end()) {
        for (uint32_t illusionId : it->second) {
            m_illusions.erase(illusionId);
        }
        m_sourceToIllusions.erase(it);
    }
}

bool IllusionManager::IsIllusion(uint32_t entityId) const {
    return m_illusions.find(entityId) != m_illusions.end();
}

const IllusionInstance* IllusionManager::GetIllusion(uint32_t illusionId) const {
    auto it = m_illusions.find(illusionId);
    return it != m_illusions.end() ? &it->second : nullptr;
}

IllusionInstance* IllusionManager::GetIllusion(uint32_t illusionId) {
    auto it = m_illusions.find(illusionId);
    return it != m_illusions.end() ? &it->second : nullptr;
}

std::vector<uint32_t> IllusionManager::GetIllusionsOfUnit(uint32_t sourceUnitId) const {
    auto it = m_sourceToIllusions.find(sourceUnitId);
    if (it != m_sourceToIllusions.end()) {
        return it->second;
    }
    return {};
}

void IllusionManager::RevealIllusion(uint32_t illusionId) {
    auto* illusion = GetIllusion(illusionId);
    if (illusion) {
        illusion->isRevealed = true;
    }
}

void IllusionManager::RevealIllusionsInRadius(const glm::vec3& center, float radius) {
    float radiusSq = radius * radius;

    for (auto& [id, illusion] : m_illusions) {
        // Would check distance to center (need position from entity system)
        // For now, mark all as revealed in debug mode
    }
}

void IllusionManager::Update(float deltaTime) {
    std::vector<uint32_t> expiredIllusions;

    for (auto& [id, illusion] : m_illusions) {
        illusion.Update(deltaTime);
        if (illusion.IsExpired()) {
            expiredIllusions.push_back(id);
        }
    }

    for (uint32_t id : expiredIllusions) {
        DestroyIllusion(id);
    }
}

void IllusionManager::Clear() {
    m_illusions.clear();
    m_sourceToIllusions.clear();
}

// ============================================================================
// MoonWellState Implementation
// ============================================================================

void MoonWellState::Update(float deltaTime) {
    if (!isActive) return;

    // Calculate regen rate (faster at night)
    float effectiveRate = regenRate * (1.0f + regenBonus);
    if (DayNightCycle::Instance().IsNight()) {
        effectiveRate *= FairyConstants::MOON_WELL_NIGHT_REGEN_BONUS;
    }

    // Regenerate mana
    float effectiveMax = maxMana * (1.0f + capacityBonus);
    if (currentMana < effectiveMax) {
        currentMana += effectiveRate * deltaTime;
        currentMana = std::min(currentMana, effectiveMax);
    }
}

float MoonWellState::UseMana(float amount) {
    float used = std::min(amount, currentMana);
    currentMana -= used;
    return used;
}

bool MoonWellState::IsInRange(const glm::vec3& pos) const {
    float dx = pos.x - position.x;
    float dz = pos.z - position.z;
    float distSq = dx * dx + dz * dz;
    return distSq <= (radius * radius);
}

nlohmann::json MoonWellState::ToJson() const {
    return {
        {"buildingId", buildingId},
        {"position", {position.x, position.y, position.z}},
        {"currentMana", currentMana},
        {"maxMana", maxMana},
        {"regenRate", regenRate},
        {"radius", radius},
        {"isActive", isActive},
        {"autoHeal", autoHeal},
        {"autoMana", autoMana}
    };
}

MoonWellState MoonWellState::FromJson(const nlohmann::json& j) {
    MoonWellState state;
    state.buildingId = j.value("buildingId", 0u);
    if (j.contains("position")) {
        auto& pos = j["position"];
        state.position = glm::vec3(pos[0], pos[1], pos[2]);
    }
    state.currentMana = j.value("currentMana", FairyConstants::MOON_WELL_MAX_MANA);
    state.maxMana = j.value("maxMana", FairyConstants::MOON_WELL_MAX_MANA);
    state.regenRate = j.value("regenRate", FairyConstants::MOON_WELL_REGEN_RATE);
    state.radius = j.value("radius", FairyConstants::MOON_WELL_RADIUS);
    state.isActive = j.value("isActive", true);
    state.autoHeal = j.value("autoHeal", true);
    state.autoMana = j.value("autoMana", true);
    return state;
}

// ============================================================================
// MoonWellManager Implementation
// ============================================================================

void MoonWellManager::RegisterMoonWell(const MoonWellState& well) {
    m_moonWells[well.buildingId] = well;
}

void MoonWellManager::UnregisterMoonWell(uint32_t buildingId) {
    m_moonWells.erase(buildingId);
}

MoonWellState* MoonWellManager::GetMoonWell(uint32_t buildingId) {
    auto it = m_moonWells.find(buildingId);
    return it != m_moonWells.end() ? &it->second : nullptr;
}

const MoonWellState* MoonWellManager::GetMoonWell(uint32_t buildingId) const {
    auto it = m_moonWells.find(buildingId);
    return it != m_moonWells.end() ? &it->second : nullptr;
}

MoonWellState* MoonWellManager::GetNearestMoonWell(const glm::vec3& position) {
    MoonWellState* nearest = nullptr;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (auto& [id, well] : m_moonWells) {
        if (!well.isActive || well.currentMana <= 0.0f) continue;

        float dx = position.x - well.position.x;
        float dz = position.z - well.position.z;
        float distSq = dx * dx + dz * dz;

        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = &well;
        }
    }

    return nearest;
}

std::vector<MoonWellState*> MoonWellManager::GetMoonWellsInRange(const glm::vec3& position) {
    std::vector<MoonWellState*> inRange;

    for (auto& [id, well] : m_moonWells) {
        if (well.isActive && well.IsInRange(position)) {
            inRange.push_back(&well);
        }
    }

    return inRange;
}

void MoonWellManager::Update(float deltaTime) {
    for (auto& [id, well] : m_moonWells) {
        well.Update(deltaTime);
    }
}

void MoonWellManager::ProcessAutoRestore(float deltaTime) {
    // Would integrate with entity system to find nearby units
    // and automatically heal/restore mana
}

float MoonWellManager::GetTotalMana() const {
    float total = 0.0f;
    for (const auto& [id, well] : m_moonWells) {
        total += well.currentMana;
    }
    return total;
}

void MoonWellManager::Clear() {
    m_moonWells.clear();
}

// ============================================================================
// LivingBuildingComponent Implementation
// ============================================================================

void LivingBuildingComponent::StartUproot() {
    if (state == LivingBuildingState::Rooted) {
        state = LivingBuildingState::Uprooting;
        transitionProgress = 0.0f;
        transitionTime = FairyConstants::UPROOT_TIME;
    }
}

void LivingBuildingComponent::StartRoot() {
    if (state == LivingBuildingState::Uprooted) {
        state = LivingBuildingState::Rooting;
        transitionProgress = 0.0f;
        transitionTime = FairyConstants::ROOT_TIME;
    }
}

void LivingBuildingComponent::Update(float deltaTime) {
    if (IsTransitioning()) {
        transitionProgress += deltaTime;
        if (transitionProgress >= transitionTime) {
            transitionProgress = transitionTime;
            if (state == LivingBuildingState::Uprooting) {
                state = LivingBuildingState::Uprooted;
            } else if (state == LivingBuildingState::Rooting) {
                state = LivingBuildingState::Rooted;
            }
        }
    }
}

float LivingBuildingComponent::GetRegenMultiplier() const {
    if (state == LivingBuildingState::Rooted) {
        return rootedRegenBonus;
    }
    return 1.0f;
}

float LivingBuildingComponent::GetArmorModifier() const {
    if (state == LivingBuildingState::Uprooted) {
        return -armorPenalty;
    }
    return 0.0f;
}

nlohmann::json LivingBuildingComponent::ToJson() const {
    return {
        {"buildingId", buildingId},
        {"state", static_cast<int>(state)},
        {"transitionProgress", transitionProgress},
        {"uprootedSpeed", uprootedSpeed},
        {"armorPenalty", armorPenalty},
        {"rootedRegenBonus", rootedRegenBonus},
        {"canAttackWhileUprooted", canAttackWhileUprooted}
    };
}

LivingBuildingComponent LivingBuildingComponent::FromJson(const nlohmann::json& j) {
    LivingBuildingComponent comp;
    comp.buildingId = j.value("buildingId", 0u);
    comp.state = static_cast<LivingBuildingState>(j.value("state", 0));
    comp.transitionProgress = j.value("transitionProgress", 0.0f);
    comp.uprootedSpeed = j.value("uprootedSpeed", FairyConstants::UPROOTED_SPEED);
    comp.armorPenalty = j.value("armorPenalty", FairyConstants::UPROOTED_ARMOR_PENALTY);
    comp.rootedRegenBonus = j.value("rootedRegenBonus", FairyConstants::ROOTED_REGEN_BONUS);
    comp.canAttackWhileUprooted = j.value("canAttackWhileUprooted", false);
    return comp;
}

// ============================================================================
// FairyRingNode Implementation
// ============================================================================

void FairyRingNode::Update(float deltaTime) {
    if (cooldownRemaining > 0.0f) {
        cooldownRemaining -= deltaTime;
        if (cooldownRemaining < 0.0f) {
            cooldownRemaining = 0.0f;
        }
    }
}

// ============================================================================
// FairyRingNetwork Implementation
// ============================================================================

void FairyRingNetwork::RegisterRing(const FairyRingNode& ring) {
    m_rings[ring.buildingId] = ring;
}

void FairyRingNetwork::UnregisterRing(uint32_t buildingId) {
    m_rings.erase(buildingId);
}

std::vector<const FairyRingNode*> FairyRingNetwork::GetAvailableDestinations(uint32_t sourceRingId) const {
    std::vector<const FairyRingNode*> destinations;

    for (const auto& [id, ring] : m_rings) {
        if (id != sourceRingId && ring.isActive) {
            destinations.push_back(&ring);
        }
    }

    return destinations;
}

bool FairyRingNetwork::TeleportUnits(uint32_t sourceRingId, uint32_t destRingId,
                                      const std::vector<uint32_t>& unitIds) {
    auto sourceIt = m_rings.find(sourceRingId);
    auto destIt = m_rings.find(destRingId);

    if (sourceIt == m_rings.end() || destIt == m_rings.end()) {
        return false;
    }

    if (!sourceIt->second.IsReady()) {
        return false;
    }

    if (unitIds.size() > FairyConstants::FAIRY_RING_MAX_UNITS) {
        return false;
    }

    // Perform teleport (would integrate with entity system)
    // For now, just start cooldown
    sourceIt->second.StartCooldown();

    return true;
}

void FairyRingNetwork::Update(float deltaTime) {
    for (auto& [id, ring] : m_rings) {
        ring.Update(deltaTime);
    }
}

void FairyRingNetwork::Clear() {
    m_rings.clear();
}

// ============================================================================
// NatureBondComponent Implementation
// ============================================================================

void NatureBondComponent::UpdateTreeProximity(const glm::vec3& position) {
    // Would integrate with world tree positions
    // For now, placeholder
    nearbyTreeCount = 0;
    isNearTree = nearbyTreeCount > 0;
    isInForest = nearbyTreeCount >= 5;
}

float NatureBondComponent::GetTreeRegenBonus() const {
    if (isNearTree) {
        return treeProximityBonus * std::min(nearbyTreeCount, 5);
    }
    return 0.0f;
}

float NatureBondComponent::GetForestDamageBonus() const {
    if (isInForest) {
        return forestDamageBonus;
    }
    return 0.0f;
}

// ============================================================================
// FairyRace Implementation
// ============================================================================

FairyRace& FairyRace::Instance() {
    static FairyRace instance;
    return instance;
}

bool FairyRace::Initialize(const std::string& configPath) {
    if (m_initialized) {
        return true;
    }

    m_configBasePath = configPath.empty() ?
        "game/assets/configs/races/fairies/" : configPath;

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

void FairyRace::Shutdown() {
    m_nightPowers.clear();
    m_livingBuildings.clear();
    m_natureBonds.clear();
    IllusionManager::Instance().Clear();
    MoonWellManager::Instance().Clear();
    FairyRingNetwork::Instance().Clear();
    DayNightCycle::Instance().Reset();
    m_initialized = false;
}

void FairyRace::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update day/night cycle
    DayNightCycle::Instance().Update(deltaTime);
    CheckTimeOfDayTransition();

    UpdateNightPower(deltaTime);
    UpdateIllusions(deltaTime);
    UpdateMoonWells(deltaTime);
    UpdateLivingBuildings(deltaTime);
    UpdateFairyRings(deltaTime);
    UpdateNatureBonds();
}

void FairyRace::CheckTimeOfDayTransition() {
    TimeOfDay currentTime = DayNightCycle::Instance().GetTimeOfDay();

    if (currentTime != m_previousTimeOfDay) {
        // Check for nightfall
        if (m_previousTimeOfDay == TimeOfDay::Dusk && currentTime == TimeOfDay::Night) {
            if (m_onNightFall) {
                m_onNightFall();
            }
        }
        // Check for daybreak
        else if (m_previousTimeOfDay == TimeOfDay::Dawn && currentTime == TimeOfDay::Day) {
            if (m_onDaybreak) {
                m_onDaybreak();
            }
        }

        m_previousTimeOfDay = currentTime;
    }
}

void FairyRace::UpdateNightPower(float deltaTime) {
    // Night power bonuses are calculated on-demand via GetDamageMultiplier etc.
    // No per-frame updates needed
}

void FairyRace::UpdateIllusions(float deltaTime) {
    IllusionManager::Instance().Update(deltaTime);
}

void FairyRace::UpdateMoonWells(float deltaTime) {
    MoonWellManager::Instance().Update(deltaTime);
    MoonWellManager::Instance().ProcessAutoRestore(deltaTime);
}

void FairyRace::UpdateLivingBuildings(float deltaTime) {
    for (auto& [id, building] : m_livingBuildings) {
        LivingBuildingState prevState = building.state;
        building.Update(deltaTime);

        // Check for uproot completion
        if (prevState == LivingBuildingState::Uprooting &&
            building.state == LivingBuildingState::Uprooted) {
            if (m_onBuildingUprooted) {
                m_onBuildingUprooted(id);
            }
        }
    }
}

void FairyRace::UpdateFairyRings(float deltaTime) {
    FairyRingNetwork::Instance().Update(deltaTime);
}

void FairyRace::UpdateNatureBonds() {
    for (auto& [unitId, bond] : m_natureBonds) {
        // Would get position from entity system
        glm::vec3 position{0.0f}; // Placeholder
        bond.UpdateTreeProximity(position);
    }
}

void FairyRace::LoadConfiguration(const std::string& configPath) {
    std::string racePath = configPath + "race_fairies.json";
    std::ifstream file(racePath);
    if (file.is_open()) {
        file >> m_raceConfig;
    }
}

void FairyRace::InitializeDefaultConfigs() {
    // Initialize any default configurations not loaded from files
}

// =========================================================================
// Night Power Management
// =========================================================================

void FairyRace::RegisterNightPower(uint32_t unitId, const NightPowerComponent& nightPower) {
    m_nightPowers[unitId] = nightPower;
}

void FairyRace::UnregisterNightPower(uint32_t unitId) {
    m_nightPowers.erase(unitId);
}

NightPowerComponent* FairyRace::GetNightPower(uint32_t unitId) {
    auto it = m_nightPowers.find(unitId);
    return it != m_nightPowers.end() ? &it->second : nullptr;
}

const NightPowerComponent* FairyRace::GetNightPower(uint32_t unitId) const {
    auto it = m_nightPowers.find(unitId);
    return it != m_nightPowers.end() ? &it->second : nullptr;
}

// =========================================================================
// Illusion Management
// =========================================================================

uint32_t FairyRace::CreateIllusion(uint32_t sourceUnitId, const glm::vec3& position, float duration) {
    std::string unitType = "unknown"; // Would get from entity system
    return IllusionManager::Instance().CreateIllusion(
        sourceUnitId, unitType, position, duration);
}

std::vector<uint32_t> FairyRace::CreateMirrorImages(uint32_t sourceUnitId, int count, float duration) {
    return IllusionManager::Instance().CreateMirrorImages(sourceUnitId, count, duration);
}

std::vector<uint32_t> FairyRace::CreateMassIllusion(const glm::vec3& center, float radius,
                                                     float duration, int copiesPerUnit) {
    return IllusionManager::Instance().CreateMassIllusion(center, radius, duration, copiesPerUnit);
}

void FairyRace::DestroyIllusion(uint32_t illusionId) {
    auto* illusion = IllusionManager::Instance().GetIllusion(illusionId);
    if (illusion && m_onIllusionDestroyed) {
        m_onIllusionDestroyed(illusionId, illusion->sourceUnitId);
    }
    IllusionManager::Instance().DestroyIllusion(illusionId);
}

bool FairyRace::IsIllusion(uint32_t entityId) const {
    return IllusionManager::Instance().IsIllusion(entityId);
}

float FairyRace::ApplyDamage(uint32_t entityId, float damage, bool isIronWeapon) {
    float effectiveDamage = damage;

    // Check if target is an illusion (takes more damage)
    auto* illusion = IllusionManager::Instance().GetIllusion(entityId);
    if (illusion) {
        effectiveDamage *= illusion->damageTakenMultiplier;
    }

    // Apply iron vulnerability
    if (isIronWeapon) {
        effectiveDamage *= (1.0f + FairyConstants::IRON_DAMAGE_VULNERABILITY);
    }

    return effectiveDamage;
}

// =========================================================================
// Moon Well Management
// =========================================================================

void FairyRace::RegisterMoonWell(uint32_t buildingId, const glm::vec3& position) {
    MoonWellState well;
    well.buildingId = buildingId;
    well.position = position;
    well.currentMana = FairyConstants::MOON_WELL_MAX_MANA;
    well.maxMana = FairyConstants::MOON_WELL_MAX_MANA;
    MoonWellManager::Instance().RegisterMoonWell(well);
}

void FairyRace::UnregisterMoonWell(uint32_t buildingId) {
    MoonWellManager::Instance().UnregisterMoonWell(buildingId);
}

float FairyRace::HealFromMoonWell(uint32_t unitId, float amount) {
    // Would get unit position from entity system
    glm::vec3 position{0.0f}; // Placeholder

    auto* well = MoonWellManager::Instance().GetNearestMoonWell(position);
    if (well && well->IsInRange(position)) {
        float manaNeeded = amount / FairyConstants::MOON_WELL_HEAL_RATE;
        float manaUsed = well->UseMana(manaNeeded);
        float healed = manaUsed * FairyConstants::MOON_WELL_HEAL_RATE;

        if (well->currentMana <= 0.0f && m_onMoonWellEmpty) {
            m_onMoonWellEmpty(well->buildingId);
        }

        return healed;
    }
    return 0.0f;
}

float FairyRace::RestoreManaFromMoonWell(uint32_t unitId, float amount) {
    // Would get unit position from entity system
    glm::vec3 position{0.0f}; // Placeholder

    auto* well = MoonWellManager::Instance().GetNearestMoonWell(position);
    if (well && well->IsInRange(position)) {
        float manaNeeded = amount / FairyConstants::MOON_WELL_MANA_RESTORE_RATE;
        float manaUsed = well->UseMana(manaNeeded);
        float restored = manaUsed * FairyConstants::MOON_WELL_MANA_RESTORE_RATE;

        if (well->currentMana <= 0.0f && m_onMoonWellEmpty) {
            m_onMoonWellEmpty(well->buildingId);
        }

        return restored;
    }
    return 0.0f;
}

// =========================================================================
// Living Building Management
// =========================================================================

void FairyRace::RegisterLivingBuilding(uint32_t buildingId, const LivingBuildingComponent& component) {
    m_livingBuildings[buildingId] = component;
}

void FairyRace::UnregisterLivingBuilding(uint32_t buildingId) {
    m_livingBuildings.erase(buildingId);
}

LivingBuildingComponent* FairyRace::GetLivingBuilding(uint32_t buildingId) {
    auto it = m_livingBuildings.find(buildingId);
    return it != m_livingBuildings.end() ? &it->second : nullptr;
}

const LivingBuildingComponent* FairyRace::GetLivingBuilding(uint32_t buildingId) const {
    auto it = m_livingBuildings.find(buildingId);
    return it != m_livingBuildings.end() ? &it->second : nullptr;
}

bool FairyRace::UprootBuilding(uint32_t buildingId) {
    auto* building = GetLivingBuilding(buildingId);
    if (building && building->state == LivingBuildingState::Rooted) {
        building->StartUproot();
        return true;
    }
    return false;
}

bool FairyRace::RootBuilding(uint32_t buildingId) {
    auto* building = GetLivingBuilding(buildingId);
    if (building && building->state == LivingBuildingState::Uprooted) {
        building->StartRoot();
        return true;
    }
    return false;
}

bool FairyRace::CanBuildingProduce(uint32_t buildingId) const {
    auto* building = GetLivingBuilding(buildingId);
    if (building) {
        return building->CanProduce();
    }
    return true; // Non-living buildings can always produce
}

// =========================================================================
// Fairy Ring Network
// =========================================================================

void FairyRace::RegisterFairyRing(uint32_t buildingId, const glm::vec3& position, const std::string& name) {
    FairyRingNode ring;
    ring.buildingId = buildingId;
    ring.position = position;
    ring.ringName = name;
    FairyRingNetwork::Instance().RegisterRing(ring);
}

void FairyRace::UnregisterFairyRing(uint32_t buildingId) {
    FairyRingNetwork::Instance().UnregisterRing(buildingId);
}

bool FairyRace::TeleportViaFairyRing(uint32_t sourceRingId, uint32_t destRingId,
                                      const std::vector<uint32_t>& unitIds) {
    bool success = FairyRingNetwork::Instance().TeleportUnits(sourceRingId, destRingId, unitIds);
    if (success && m_onTeleportComplete) {
        m_onTeleportComplete(destRingId, unitIds);
    }
    return success;
}

// =========================================================================
// Nature Bond
// =========================================================================

void FairyRace::RegisterNatureBond(uint32_t unitId, const NatureBondComponent& component) {
    m_natureBonds[unitId] = component;
}

void FairyRace::UnregisterNatureBond(uint32_t unitId) {
    m_natureBonds.erase(unitId);
}

NatureBondComponent* FairyRace::GetNatureBond(uint32_t unitId) {
    auto it = m_natureBonds.find(unitId);
    return it != m_natureBonds.end() ? &it->second : nullptr;
}

// =========================================================================
// Resource Modifiers
// =========================================================================

float FairyRace::GetGatherRateModifier(const std::string& resourceType) const {
    if (resourceType == "wood") {
        return 1.0f + FairyConstants::WOOD_GATHER_BONUS;
    } else if (resourceType == "gold") {
        return FairyConstants::WISP_HARVEST_RATE;
    }
    return 1.0f;
}

float FairyRace::GetCostModifier(const std::string& entityType) const {
    // Fairies have standard costs
    return 1.0f;
}

// =========================================================================
// Statistics
// =========================================================================

int FairyRace::GetActiveIllusionCount() const {
    return IllusionManager::Instance().GetActiveIllusionCount();
}

float FairyRace::GetTotalMoonWellMana() const {
    return MoonWellManager::Instance().GetTotalMana();
}

int FairyRace::GetLivingBuildingCount() const {
    return static_cast<int>(m_livingBuildings.size());
}

int FairyRace::GetUprootedBuildingCount() const {
    int count = 0;
    for (const auto& [id, building] : m_livingBuildings) {
        if (building.state == LivingBuildingState::Uprooted) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Ability Implementations
// ============================================================================

// Entangling Roots Ability
bool EntanglingRootsAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) return false;

    // Can't root air units or massive units
    if (context.targetUnit) {
        // Would check unit attributes
    }

    return true;
}

AbilityCastResult EntanglingRootsAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.targetUnit) {
        const auto& levelData = data.GetLevelData(context.abilityLevel);

        RootInstance root;
        root.targetId = context.targetUnit->GetId();
        root.position = context.targetUnit->GetPosition();
        root.remainingDuration = levelData.duration;
        root.damagePerSecond = levelData.damage / levelData.duration;
        root.tickTimer = 0.0f;

        m_activeRoots.push_back(root);
        result.success = true;
        result.unitsAffected = 1;
    }

    return result;
}

void EntanglingRootsAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    float tickInterval = 1.0f;

    for (auto it = m_activeRoots.begin(); it != m_activeRoots.end();) {
        it->remainingDuration -= deltaTime;
        it->tickTimer += deltaTime;

        if (it->tickTimer >= tickInterval) {
            it->tickTimer = 0.0f;
            // Apply damage tick to target
        }

        if (it->remainingDuration <= 0.0f) {
            it = m_activeRoots.erase(it);
        } else {
            ++it;
        }
    }
}

void EntanglingRootsAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Remove root effect from target
}

// Force of Nature Ability
AbilityCastResult ForceOfNatureAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    int treantCount = m_baseTreantCount + m_talentBonusTreants;
    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Calculate treant stats based on ability level
    // Would spawn Treant entities
    result.success = true;
    result.unitsCreated = treantCount;

    return result;
}

// Mirror Image Ability
bool MirrorImageAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) return false;

    // Check existing illusion count
    if (context.caster) {
        auto illusions = IllusionManager::Instance().GetIllusionsOfUnit(context.caster->GetId());
        if (illusions.size() >= FairyConstants::MAX_ILLUSIONS_PER_UNIT) {
            return false;
        }
    }

    return true;
}

AbilityCastResult MirrorImageAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.caster) {
        const auto& levelData = data.GetLevelData(context.abilityLevel);
        int imageCount = static_cast<int>(levelData.values[0]); // First value is image count

        auto illusions = FairyRace::Instance().CreateMirrorImages(
            context.caster->GetId(), imageCount, levelData.duration);

        result.success = !illusions.empty();
        result.unitsCreated = static_cast<int>(illusions.size());
    }

    return result;
}

// Charm Ability
bool CharmAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) return false;

    if (context.targetUnit) {
        // Can't charm heroes, massive, mechanical, or undead
        // Would check unit tags/attributes
    }

    return true;
}

AbilityCastResult CharmAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.targetUnit) {
        const auto& levelData = data.GetLevelData(context.abilityLevel);

        uint32_t targetId = context.targetUnit->GetId();
        // Would store original owner
        m_charmedUnits[targetId] = 0; // Placeholder for owner ID
        m_charmDurations[targetId] = levelData.duration;

        // Change unit ownership
        result.success = true;
        result.unitsAffected = 1;
    }

    return result;
}

void CharmAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    std::vector<uint32_t> expiredCharms;

    for (auto& [unitId, duration] : m_charmDurations) {
        duration -= deltaTime;
        if (duration <= 0.0f) {
            expiredCharms.push_back(unitId);
        }
    }

    for (uint32_t unitId : expiredCharms) {
        // Return unit to original owner
        m_charmedUnits.erase(unitId);
        m_charmDurations.erase(unitId);
    }
}

void CharmAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Cleanup remaining charms
}

// Tranquility Ability
bool TranquilityAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return AbilityBehavior::CanCast(context, data);
}

AbilityCastResult TranquilityAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.caster) {
        const auto& levelData = data.GetLevelData(context.abilityLevel);

        TranquilityInstance instance;
        instance.casterId = context.caster->GetId();
        instance.position = context.caster->GetPosition();
        instance.remainingDuration = levelData.duration;
        instance.healPerSecond = levelData.values[0]; // Healing per second
        instance.radius = levelData.radius;
        instance.tickTimer = 0.0f;

        m_activeInstances.push_back(instance);
        result.success = true;
    }

    return result;
}

void TranquilityAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    float tickInterval = 1.0f;

    for (auto it = m_activeInstances.begin(); it != m_activeInstances.end();) {
        it->remainingDuration -= deltaTime;
        it->tickTimer += deltaTime;

        if (it->tickTimer >= tickInterval) {
            it->tickTimer = 0.0f;
            // Apply healing to all friendly units in radius
            // Healing is enhanced at night
            float healAmount = it->healPerSecond;
            if (FairyRace::Instance().IsNightTime()) {
                healAmount *= (1.0f + FairyConstants::NIGHT_HEALING_BONUS);
            }
        }

        if (it->remainingDuration <= 0.0f) {
            it = m_activeInstances.erase(it);
        } else {
            ++it;
        }
    }
}

void TranquilityAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Channel interrupted - stop healing
}

// Starfall Ability
bool StarfallAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return AbilityBehavior::CanCast(context, data);
}

AbilityCastResult StarfallAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.caster) {
        const auto& levelData = data.GetLevelData(context.abilityLevel);

        StarfallInstance instance;
        instance.casterId = context.caster->GetId();
        instance.position = context.caster->GetPosition();
        instance.remainingDuration = levelData.duration;
        instance.damagePerWave = levelData.damage / (levelData.duration / 0.5f);
        instance.radius = levelData.radius;
        instance.waveTimer = 0.0f;
        instance.waveInterval = 0.5f;

        m_activeInstances.push_back(instance);
        result.success = true;
    }

    return result;
}

void StarfallAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    for (auto it = m_activeInstances.begin(); it != m_activeInstances.end();) {
        it->remainingDuration -= deltaTime;
        it->waveTimer += deltaTime;

        if (it->waveTimer >= it->waveInterval) {
            it->waveTimer = 0.0f;
            // Apply damage to random enemies in radius
            float damage = it->damagePerWave;
            if (FairyRace::Instance().IsNightTime()) {
                damage *= (1.0f + FairyConstants::NIGHT_DAMAGE_BONUS);
            }
        }

        if (it->remainingDuration <= 0.0f) {
            it = m_activeInstances.erase(it);
        } else {
            ++it;
        }
    }
}

void StarfallAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Channel interrupted
}

// Mass Illusion Ability
AbilityCastResult MassIllusionAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.caster) {
        const auto& levelData = data.GetLevelData(context.abilityLevel);
        int copies = m_copiesPerUnit + m_talentBonusCopies;

        auto illusions = FairyRace::Instance().CreateMassIllusion(
            context.caster->GetPosition(),
            levelData.radius,
            levelData.duration,
            copies);

        result.success = !illusions.empty();
        result.unitsCreated = static_cast<int>(illusions.size());
    }

    return result;
}

// Phase Shift Ability
bool PhaseShiftAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) return false;

    // Can't phase shift if already phased
    if (context.caster && m_phasedUnits.count(context.caster->GetId()) > 0) {
        return false;
    }

    return true;
}

AbilityCastResult PhaseShiftAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (context.caster) {
        const auto& levelData = data.GetLevelData(context.abilityLevel);
        uint32_t unitId = context.caster->GetId();

        m_phasedUnits.insert(unitId);
        m_phaseDurations[unitId] = levelData.duration;

        // Make unit invulnerable/untargetable
        result.success = true;
    }

    return result;
}

void PhaseShiftAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    std::vector<uint32_t> expiredPhases;

    for (auto& [unitId, duration] : m_phaseDurations) {
        duration -= deltaTime;
        if (duration <= 0.0f) {
            expiredPhases.push_back(unitId);
        }
    }

    for (uint32_t unitId : expiredPhases) {
        m_phasedUnits.erase(unitId);
        m_phaseDurations.erase(unitId);
        // Restore unit targetability
    }
}

void PhaseShiftAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Early end - restore unit
}

// Rebirth Ability (Phoenix)
void RebirthAbility::OnDeath(const AbilityCastContext& context, const AbilityData& data) {
    if (context.caster) {
        uint32_t phoenixId = context.caster->GetId();

        // Check cooldown
        auto cooldownIt = m_rebirthCooldowns.find(phoenixId);
        if (cooldownIt != m_rebirthCooldowns.end() && cooldownIt->second > 0.0f) {
            return; // Rebirth on cooldown
        }

        const auto& levelData = data.GetLevelData(context.abilityLevel);

        RebirthState rebirth;
        rebirth.phoenixId = phoenixId;
        rebirth.deathPosition = context.caster->GetPosition();
        rebirth.respawnTimer = 3.0f; // Time until rebirth
        rebirth.healthPercent = levelData.values[0]; // Rebirth health percentage

        m_pendingRebirths[phoenixId] = rebirth;
    }
}

void RebirthAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    // Update rebirth cooldowns
    for (auto& [phoenixId, cooldown] : m_rebirthCooldowns) {
        if (cooldown > 0.0f) {
            cooldown -= deltaTime;
        }
    }

    // Process pending rebirths
    std::vector<uint32_t> completedRebirths;

    for (auto& [phoenixId, rebirth] : m_pendingRebirths) {
        rebirth.respawnTimer -= deltaTime;

        if (rebirth.respawnTimer <= 0.0f) {
            // Respawn the phoenix
            // Would create new phoenix entity at death position with reduced health
            completedRebirths.push_back(phoenixId);

            // Start cooldown
            const auto& levelData = data.GetLevelData(context.abilityLevel);
            m_rebirthCooldowns[phoenixId] = levelData.cooldown;
        }
    }

    for (uint32_t id : completedRebirths) {
        m_pendingRebirths.erase(id);
    }
}

} // namespace RTS
} // namespace Vehement
