#include "CryptidRace.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>

namespace Vehement {
namespace RTS {

// ============================================================================
// Fear Manager Implementation
// ============================================================================

FearManager& FearManager::Instance() {
    static FearManager instance;
    return instance;
}

void FearManager::Update(float deltaTime) {
    std::vector<uint32_t> toRemove;

    for (auto& [entityId, status] : m_fearStatuses) {
        // Update each stack timer
        for (auto it = status.stackTimers.begin(); it != status.stackTimers.end();) {
            *it -= deltaTime;
            if (*it <= 0.0f) {
                it = status.stackTimers.erase(it);
                --status.stacks;
            } else {
                ++it;
            }
        }

        // Remove if no stacks remain
        if (status.stacks <= 0) {
            toRemove.push_back(entityId);
            continue;
        }

        // Update fleeing state
        status.isFleeing = status.ShouldFlee();

        // Update duration to oldest remaining stack
        if (!status.stackTimers.empty()) {
            status.duration = *std::min_element(status.stackTimers.begin(), status.stackTimers.end());
        }
    }

    for (uint32_t id : toRemove) {
        m_fearStatuses.erase(id);
    }
}

int FearManager::ApplyFear(uint32_t targetId, uint32_t sourceId, int stacks) {
    // Check immunity
    if (IsFearImmune(targetId)) {
        return 0;
    }

    FearStatus& status = m_fearStatuses[targetId];
    status.entityId = targetId;
    status.fearSource = sourceId;

    // Apply stacks up to max
    int stacksToAdd = std::min(stacks, CryptidConstants::MAX_FEAR_STACKS - status.stacks);
    for (int i = 0; i < stacksToAdd; ++i) {
        status.stackTimers.push_back(CryptidConstants::FEAR_STACK_DURATION * m_fearDurationModifier);
        ++status.stacks;
    }

    // Update fleeing state
    status.isFleeing = status.ShouldFlee();

    return status.stacks;
}

void FearManager::RemoveFear(uint32_t entityId, int stacks) {
    auto it = m_fearStatuses.find(entityId);
    if (it == m_fearStatuses.end()) {
        return;
    }

    if (stacks < 0) {
        // Remove all
        m_fearStatuses.erase(it);
    } else {
        // Remove specified stacks
        for (int i = 0; i < stacks && !it->second.stackTimers.empty(); ++i) {
            it->second.stackTimers.pop_back();
            --it->second.stacks;
        }

        if (it->second.stacks <= 0) {
            m_fearStatuses.erase(it);
        }
    }
}

FearStatus* FearManager::GetFearStatus(uint32_t entityId) {
    auto it = m_fearStatuses.find(entityId);
    return (it != m_fearStatuses.end()) ? &it->second : nullptr;
}

bool FearManager::IsFeared(uint32_t entityId) const {
    return m_fearStatuses.count(entityId) > 0;
}

bool FearManager::IsFleeing(uint32_t entityId) const {
    auto it = m_fearStatuses.find(entityId);
    return (it != m_fearStatuses.end()) ? it->second.isFleeing : false;
}

int FearManager::GetFearStacks(uint32_t entityId) const {
    auto it = m_fearStatuses.find(entityId);
    return (it != m_fearStatuses.end()) ? it->second.stacks : 0;
}

void FearManager::SpreadFear(uint32_t sourceEntityId, float radius, float spreadChance) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Would integrate with entity query system to find nearby enemies
    // and apply fear to those within radius based on spreadChance
}

bool FearManager::IsFearImmune(uint32_t entityId) const {
    return m_fearImmuneEntities.count(entityId) > 0;
}

void FearManager::RegisterFearImmune(uint32_t entityId) {
    m_fearImmuneEntities.insert(entityId);
}

void FearManager::UnregisterFearImmune(uint32_t entityId) {
    m_fearImmuneEntities.erase(entityId);
}

// ============================================================================
// Transformation Manager Implementation
// ============================================================================

TransformationManager& TransformationManager::Instance() {
    static TransformationManager instance;
    return instance;
}

void TransformationManager::Update(float deltaTime) {
    for (auto& [entityId, state] : m_transformers) {
        // Update global transform cooldown
        if (state.transformCooldown > 0.0f) {
            state.transformCooldown -= deltaTime;
        }

        // Update per-form cooldowns
        for (auto& [formId, cooldown] : state.formCooldowns) {
            if (cooldown > 0.0f) {
                cooldown -= deltaTime;
            }
        }

        // Update transformation progress
        if (state.isTransforming) {
            state.transformProgress += deltaTime / CryptidConstants::TRANSFORM_TRANSITION_TIME;
            if (state.transformProgress >= 1.0f) {
                CompleteTransformation(entityId);
            }
        }

        // Update form duration (for temporary forms)
        if (state.formDuration > 0.0f) {
            state.formDuration -= deltaTime;
            if (state.formDuration <= 0.0f) {
                RevertToBase(entityId);
            }
        }
    }
}

void TransformationManager::RegisterForm(const std::string& formId, const TransformForm& form) {
    m_forms[formId] = form;
}

const TransformForm* TransformationManager::GetForm(const std::string& formId) const {
    auto it = m_forms.find(formId);
    return (it != m_forms.end()) ? &it->second : nullptr;
}

void TransformationManager::RegisterTransformer(uint32_t entityId, const std::string& baseForm,
                                                  const std::vector<std::string>& availableForms) {
    TransformState state;
    state.entityId = entityId;
    state.baseForm = baseForm;
    state.currentForm = baseForm;
    state.transformCooldown = 0.0f;
    state.isTransforming = false;

    m_transformers[entityId] = state;
    m_availableForms[entityId] = availableForms;
}

void TransformationManager::UnregisterTransformer(uint32_t entityId) {
    m_transformers.erase(entityId);
    m_availableForms.erase(entityId);
}

bool TransformationManager::StartTransformation(uint32_t entityId, const std::string& targetForm) {
    if (!CanTransform(entityId, targetForm)) {
        return false;
    }

    auto& state = m_transformers[entityId];
    state.isTransforming = true;
    state.transformProgress = 0.0f;

    // Store target form for completion
    // The actual stats change happens in CompleteTransformation
    state.currentForm = targetForm;  // Will be applied on complete

    return true;
}

void TransformationManager::CancelTransformation(uint32_t entityId) {
    auto it = m_transformers.find(entityId);
    if (it != m_transformers.end() && it->second.isTransforming) {
        it->second.isTransforming = false;
        it->second.transformProgress = 0.0f;
    }
}

void TransformationManager::RevertToBase(uint32_t entityId) {
    auto it = m_transformers.find(entityId);
    if (it != m_transformers.end()) {
        it->second.currentForm = it->second.baseForm;
        it->second.formDuration = 0.0f;
        // Apply base form stats - would integrate with entity system
    }
}

std::string TransformationManager::GetCurrentForm(uint32_t entityId) const {
    auto it = m_transformers.find(entityId);
    return (it != m_transformers.end()) ? it->second.currentForm : "";
}

bool TransformationManager::CanTransform(uint32_t entityId, const std::string& targetForm) const {
    auto stateIt = m_transformers.find(entityId);
    if (stateIt == m_transformers.end()) {
        return false;
    }

    const auto& state = stateIt->second;

    // Check if currently transforming
    if (state.isTransforming) {
        return false;
    }

    // Check global cooldown
    if (state.transformCooldown > 0.0f) {
        return false;
    }

    // Check form-specific cooldown
    auto cdIt = state.formCooldowns.find(targetForm);
    if (cdIt != state.formCooldowns.end() && cdIt->second > 0.0f) {
        return false;
    }

    // Check if form is available
    auto formsIt = m_availableForms.find(entityId);
    if (formsIt == m_availableForms.end()) {
        return false;
    }

    const auto& forms = formsIt->second;
    return std::find(forms.begin(), forms.end(), targetForm) != forms.end();
}

TransformState* TransformationManager::GetTransformState(uint32_t entityId) {
    auto it = m_transformers.find(entityId);
    return (it != m_transformers.end()) ? &it->second : nullptr;
}

std::vector<std::string> TransformationManager::GetAvailableForms(uint32_t entityId) const {
    auto it = m_availableForms.find(entityId);
    return (it != m_availableForms.end()) ? it->second : std::vector<std::string>{};
}

bool TransformationManager::IsTransforming(uint32_t entityId) const {
    auto it = m_transformers.find(entityId);
    return (it != m_transformers.end()) ? it->second.isTransforming : false;
}

void TransformationManager::CompleteTransformation(uint32_t entityId) {
    auto it = m_transformers.find(entityId);
    if (it == m_transformers.end()) {
        return;
    }

    auto& state = it->second;
    state.isTransforming = false;
    state.transformProgress = 0.0f;

    // Set cooldowns
    const auto* form = GetForm(state.currentForm);
    if (form) {
        state.transformCooldown = CryptidConstants::BASE_TRANSFORM_COOLDOWN * m_cooldownModifier;
        state.formCooldowns[state.currentForm] = form->cooldown * m_cooldownModifier;
        state.formDuration = form->duration;  // 0 = permanent
    }

    // Apply form stats - would integrate with entity system
}

// ============================================================================
// Mist Manager Implementation
// ============================================================================

MistManager& MistManager::Instance() {
    static MistManager instance;
    return instance;
}

void MistManager::Update(float deltaTime) {
    // Update temporary mist decay
    for (auto it = m_temporaryMist.begin(); it != m_temporaryMist.end();) {
        it->second -= deltaTime;
        if (it->second <= 0.0f) {
            m_mistedTiles.erase(it->first);
            m_mistData.erase(it->first);
            it = m_temporaryMist.erase(it);
        } else {
            ++it;
        }
    }

    // Spread mist from permanent sources
    for (const auto& source : m_mistSources) {
        if (source.permanent) {
            glm::ivec2 tilePos(
                static_cast<int>(source.position.x),
                static_cast<int>(source.position.z)
            );
            SpreadMist(tilePos, source.radius, source.buildingId, true);
        }
    }
}

void MistManager::AddMistSource(uint32_t buildingId, const glm::vec3& position, float radius, bool permanent) {
    MistSource source;
    source.buildingId = buildingId;
    source.position = position;
    source.radius = radius;
    source.permanent = permanent;
    m_mistSources.push_back(source);

    // Initial mist spread
    glm::ivec2 tilePos(
        static_cast<int>(position.x),
        static_cast<int>(position.z)
    );
    SpreadMist(tilePos, radius, buildingId, permanent);
}

void MistManager::RemoveMistSource(uint32_t buildingId) {
    // Remove source
    m_mistSources.erase(
        std::remove_if(m_mistSources.begin(), m_mistSources.end(),
            [buildingId](const MistSource& s) { return s.buildingId == buildingId; }),
        m_mistSources.end()
    );

    // Remove mist tiles from this source
    std::vector<uint64_t> toRemove;
    for (const auto& [key, tile] : m_mistData) {
        if (tile.sourceBuildingId == buildingId && tile.isPermanent) {
            toRemove.push_back(key);
        }
    }

    for (uint64_t key : toRemove) {
        m_mistedTiles.erase(key);
        m_mistData.erase(key);
    }
}

void MistManager::CreateTemporaryMist(const glm::vec3& position, float radius, float duration) {
    glm::ivec2 center(
        static_cast<int>(position.x),
        static_cast<int>(position.z)
    );

    int intRadius = static_cast<int>(std::ceil(radius));

    for (int dx = -intRadius; dx <= intRadius; ++dx) {
        for (int dy = -intRadius; dy <= intRadius; ++dy) {
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (dist <= radius) {
                int x = center.x + dx;
                int y = center.y + dy;
                uint64_t key = TileKey(x, y);

                if (m_mistedTiles.count(key) == 0) {
                    m_mistedTiles.insert(key);

                    MistTile tile;
                    tile.position = {x, y};
                    tile.intensity = 1.0f - (dist / radius);
                    tile.isPermanent = false;
                    tile.decayTimer = duration;
                    m_mistData[key] = tile;

                    m_temporaryMist.emplace_back(key, duration);
                }
            }
        }
    }
}

bool MistManager::IsInMist(const glm::vec3& position) const {
    int x = static_cast<int>(position.x);
    int y = static_cast<int>(position.z);
    return m_mistedTiles.count(TileKey(x, y)) > 0;
}

float MistManager::GetMistIntensity(const glm::vec3& position) const {
    int x = static_cast<int>(position.x);
    int y = static_cast<int>(position.z);
    uint64_t key = TileKey(x, y);

    auto it = m_mistData.find(key);
    if (it != m_mistData.end()) {
        return it->second.intensity;
    }
    return 0.0f;
}

float MistManager::GetConcealmentBonus(const glm::vec3& position) const {
    return GetMistIntensity(position) * CryptidConstants::MIST_CONCEALMENT_BONUS;
}

void MistManager::ClearTemporaryMist() {
    for (const auto& [key, timer] : m_temporaryMist) {
        m_mistedTiles.erase(key);
        m_mistData.erase(key);
    }
    m_temporaryMist.clear();
}

void MistManager::SpreadMist(const glm::ivec2& center, float radius, uint32_t sourceId, bool permanent) {
    int intRadius = static_cast<int>(std::ceil(radius));

    for (int dx = -intRadius; dx <= intRadius; ++dx) {
        for (int dy = -intRadius; dy <= intRadius; ++dy) {
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (dist <= radius) {
                int x = center.x + dx;
                int y = center.y + dy;
                uint64_t key = TileKey(x, y);

                if (m_mistedTiles.count(key) == 0) {
                    m_mistedTiles.insert(key);

                    MistTile tile;
                    tile.position = {x, y};
                    tile.intensity = 1.0f - (dist / radius);
                    tile.sourceBuildingId = sourceId;
                    tile.isPermanent = permanent;
                    m_mistData[key] = tile;
                }
            }
        }
    }
}

// ============================================================================
// Mimicry Manager Implementation
// ============================================================================

MimicryManager& MimicryManager::Instance() {
    static MimicryManager instance;
    return instance;
}

void MimicryManager::Update(float deltaTime) {
    std::vector<uint32_t> toRemove;

    for (auto& [entityId, state] : m_disguises) {
        if (state.isActive && state.duration > 0.0f) {
            state.duration -= deltaTime;
            if (state.duration <= 0.0f) {
                toRemove.push_back(entityId);
            }
        }
    }

    for (uint32_t id : toRemove) {
        EndDisguise(id, false);
    }

    // Update identity theft victims
    // Similar decay logic would apply
}

bool MimicryManager::StartDisguise(uint32_t entityId, uint32_t targetId) {
    // Would get target stats from entity system
    DisguiseState state;
    state.entityId = entityId;
    state.copiedEntityId = targetId;
    state.duration = CryptidConstants::DISGUISE_DURATION;
    state.isActive = true;
    state.appearsAsEnemy = false;

    // Copy stats at reduced effectiveness
    // Would integrate with entity system to get actual stats
    state.copiedHealth = 100;  // Placeholder
    state.copiedDamage = 10;   // Placeholder
    state.copiedSpeed = 1.0f;  // Placeholder

    m_disguises[entityId] = state;
    return true;
}

void MimicryManager::EndDisguise(uint32_t entityId, bool explosive) {
    auto it = m_disguises.find(entityId);
    if (it == m_disguises.end()) {
        return;
    }

    if (explosive) {
        // Would trigger AoE damage and stun around the entity
        // Integrate with combat system
    }

    m_disguises.erase(it);
}

bool MimicryManager::CopyAbilities(uint32_t entityId, uint32_t targetId, int abilityCount) {
    auto it = m_disguises.find(entityId);
    if (it == m_disguises.end()) {
        // Create new disguise state for ability copying
        DisguiseState state;
        state.entityId = entityId;
        state.copiedEntityId = targetId;
        state.duration = CryptidConstants::COPY_ABILITY_DURATION;
        state.isActive = true;
        m_disguises[entityId] = state;
        it = m_disguises.find(entityId);
    }

    // Would get abilities from target and copy them
    // Integrate with ability system

    return true;
}

bool MimicryManager::ApplyIdentityTheft(uint32_t targetId, float duration) {
    // Would make target appear as enemy to their allies
    m_identityTheftVictims.insert(targetId);

    // Create a special disguise state
    DisguiseState state;
    state.entityId = targetId;
    state.duration = duration;
    state.isActive = true;
    state.appearsAsEnemy = true;

    m_disguises[targetId] = state;
    return true;
}

DisguiseState* MimicryManager::GetDisguiseState(uint32_t entityId) {
    auto it = m_disguises.find(entityId);
    return (it != m_disguises.end()) ? &it->second : nullptr;
}

bool MimicryManager::IsDisguised(uint32_t entityId) const {
    auto it = m_disguises.find(entityId);
    return (it != m_disguises.end()) ? it->second.isActive : false;
}

bool MimicryManager::AppearsAsEnemy(uint32_t entityId) const {
    auto it = m_disguises.find(entityId);
    return (it != m_disguises.end()) ? it->second.appearsAsEnemy : false;
}

std::string MimicryManager::GetApparentUnitType(uint32_t entityId) const {
    auto it = m_disguises.find(entityId);
    if (it != m_disguises.end() && it->second.isActive) {
        return it->second.copiedUnitType;
    }
    return "";
}

void MimicryManager::RevealDisguise(uint32_t entityId) {
    EndDisguise(entityId, false);
}

// ============================================================================
// Ambush Manager Implementation
// ============================================================================

AmbushManager& AmbushManager::Instance() {
    static AmbushManager instance;
    return instance;
}

void AmbushManager::Update(float deltaTime) {
    for (auto& [entityId, state] : m_stealthStates) {
        if (state.isStationary && !state.isStealthed) {
            // Progress toward stealth
            state.fadeProgress += deltaTime / CryptidConstants::STEALTH_FADE_TIME;
            if (state.fadeProgress >= 1.0f) {
                state.isStealthed = true;
                state.fadeProgress = 1.0f;
            }
        } else if (!state.isStationary && state.isStealthed && state.fadeProgress > 0.0f) {
            // If moving but still somewhat faded, could slowly reveal
            // Depending on game design, stealth may break immediately on move
        }

        // Accumulate bonus damage while stealthed
        if (state.isStealthed) {
            state.bonusDamage += deltaTime * 5.0f;  // Bonus damage accumulates
            state.bonusDamage = std::min(state.bonusDamage, 100.0f);  // Cap
        }
    }
}

void AmbushManager::RegisterStealthUnit(uint32_t entityId) {
    StealthState state;
    state.entityId = entityId;
    state.isStealthed = false;
    state.isStationary = false;
    state.fadeProgress = 0.0f;
    state.bonusDamage = 0.0f;
    m_stealthStates[entityId] = state;
}

void AmbushManager::UnregisterStealthUnit(uint32_t entityId) {
    m_stealthStates.erase(entityId);
}

bool AmbushManager::EnterStealth(uint32_t entityId, bool instant) {
    auto it = m_stealthStates.find(entityId);
    if (it == m_stealthStates.end()) {
        return false;
    }

    if (instant) {
        it->second.isStealthed = true;
        it->second.fadeProgress = 1.0f;
    } else {
        it->second.isStationary = true;
        // Will naturally fade into stealth over time
    }

    return true;
}

void AmbushManager::ExitStealth(uint32_t entityId) {
    auto it = m_stealthStates.find(entityId);
    if (it != m_stealthStates.end()) {
        it->second.isStealthed = false;
        it->second.fadeProgress = 0.0f;
        it->second.bonusDamage = 0.0f;
    }
}

StealthState* AmbushManager::GetStealthState(uint32_t entityId) {
    auto it = m_stealthStates.find(entityId);
    return (it != m_stealthStates.end()) ? &it->second : nullptr;
}

bool AmbushManager::IsStealthed(uint32_t entityId) const {
    auto it = m_stealthStates.find(entityId);
    return (it != m_stealthStates.end()) ? it->second.isStealthed : false;
}

float AmbushManager::GetAmbushDamageBonus(uint32_t entityId) const {
    auto it = m_stealthStates.find(entityId);
    if (it == m_stealthStates.end() || !it->second.isStealthed) {
        return 0.0f;
    }

    return (CryptidConstants::AMBUSH_DAMAGE_BONUS + it->second.bonusDamage / 100.0f) * m_ambushDamageModifier;
}

void AmbushManager::SetStationary(uint32_t entityId, bool stationary) {
    auto it = m_stealthStates.find(entityId);
    if (it != m_stealthStates.end()) {
        it->second.isStationary = stationary;
        if (!stationary) {
            // Movement breaks passive stealth progress
            it->second.fadeProgress = 0.0f;
        }
    }
}

float AmbushManager::CalculateDetectionChance(uint32_t stealthedId, uint32_t detectorId,
                                                float detectionRange) const {
    auto it = m_stealthStates.find(stealthedId);
    if (it == m_stealthStates.end() || !it->second.isStealthed) {
        return 1.0f;  // Not stealthed, always detected
    }

    // Base detection chance reduced by stealth
    float baseChance = 1.0f - (it->second.fadeProgress * m_detectionModifier);

    // Would also factor in:
    // - Distance between units
    // - Detector's detection ability
    // - Mist concealment
    // - Night time bonus

    return std::max(0.0f, std::min(1.0f, baseChance));
}

void AmbushManager::OnStealthAttack(uint32_t entityId) {
    auto it = m_stealthStates.find(entityId);
    if (it == m_stealthStates.end()) {
        return;
    }

    // Check for free attack from One with Darkness talent
    if (it->second.canAttackWithoutBreaking && it->second.freeAttacksRemaining > 0) {
        --it->second.freeAttacksRemaining;
    } else {
        // Break stealth on attack
        ExitStealth(entityId);
    }
}

// ============================================================================
// Cryptid Race Implementation
// ============================================================================

CryptidRace& CryptidRace::Instance() {
    static CryptidRace instance;
    return instance;
}

bool CryptidRace::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Load race configuration
    if (!LoadConfiguration("game/assets/configs/races/cryptids/race_cryptids.json")) {
        return false;
    }

    // Register cryptid abilities
    RegisterCryptidAbilities();

    // Initialize resources
    m_essence = 100;  // Starting essence
    m_dread = 0;

    m_initialized = true;
    return true;
}

void CryptidRace::Shutdown() {
    m_cryptidUnits.clear();
    m_unitTypes.clear();
    m_wendigoGrowth.clear();
    m_buildings.clear();
    m_initialized = false;
}

void CryptidRace::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update all sub-managers
    FearManager::Instance().Update(deltaTime);
    TransformationManager::Instance().Update(deltaTime);
    MistManager::Instance().Update(deltaTime);
    MimicryManager::Instance().Update(deltaTime);
    AmbushManager::Instance().Update(deltaTime);

    // Generate essence from feared enemies
    GenerateEssenceFromFear();
}

bool CryptidRace::IsCryptidUnit(uint32_t entityId) const {
    return m_cryptidUnits.count(entityId) > 0;
}

void CryptidRace::RegisterCryptidUnit(uint32_t entityId, const std::string& unitType) {
    m_cryptidUnits.insert(entityId);
    m_unitTypes[entityId] = unitType;

    // Register for appropriate systems based on unit type
    // Would check unit config for capabilities

    // Register wendigos for growth tracking
    if (unitType == "wendigo" || unitType == "wendigo_ancient") {
        RegisterWendigo(entityId);
    }
}

void CryptidRace::UnregisterCryptidUnit(uint32_t entityId) {
    m_cryptidUnits.erase(entityId);
    m_unitTypes.erase(entityId);
    m_wendigoGrowth.erase(entityId);

    // Unregister from sub-managers
    TransformationManager::Instance().UnregisterTransformer(entityId);
    AmbushManager::Instance().UnregisterStealthUnit(entityId);
}

void CryptidRace::ApplyCryptidBonuses(uint32_t entityId) {
    // Apply race-specific bonuses:
    // - Night time bonuses
    // - Mist concealment
    // - Fear aura for certain units
    // Would integrate with entity/component system
}

float CryptidRace::GetTimeOfDayMultiplier() const {
    if (IsNight()) {
        return 1.0f + CryptidConstants::NIGHT_DAMAGE_BONUS;
    } else {
        return 1.0f - CryptidConstants::DAY_DAMAGE_PENALTY;
    }
}

int CryptidRace::ApplyFearToTarget(uint32_t targetId, uint32_t sourceId, int stacks) {
    int resultStacks = FearManager::Instance().ApplyFear(targetId, sourceId, stacks);

    if (m_onFearApplied && resultStacks > 0) {
        m_onFearApplied(targetId, resultStacks);
    }

    return resultStacks;
}

float CryptidRace::GetFearBonusDamage(uint32_t targetId) const {
    int stacks = FearManager::Instance().GetFearStacks(targetId);
    // 10% bonus damage per fear stack
    return static_cast<float>(stacks) * 0.10f;
}

bool CryptidRace::ShouldTargetFlee(uint32_t targetId) const {
    return FearManager::Instance().IsFleeing(targetId);
}

bool CryptidRace::TransformUnit(uint32_t entityId, const std::string& targetForm) {
    auto& transManager = TransformationManager::Instance();

    if (!transManager.CanTransform(entityId, targetForm)) {
        return false;
    }

    // Check essence cost
    int cost = GetTransformEssenceCost(entityId, targetForm);
    if (!SpendEssence(cost)) {
        return false;
    }

    bool success = transManager.StartTransformation(entityId, targetForm);

    if (success && m_onTransform) {
        m_onTransform(entityId, targetForm);
    }

    return success;
}

void CryptidRace::RevertUnitForm(uint32_t entityId) {
    TransformationManager::Instance().RevertToBase(entityId);
}

int CryptidRace::GetTransformEssenceCost(uint32_t entityId, const std::string& targetForm) const {
    const auto* form = TransformationManager::Instance().GetForm(targetForm);
    if (form) {
        return form->essenceCost;
    }
    return CryptidConstants::BASE_TRANSFORM_ESSENCE_COST;
}

float CryptidRace::CalculateAmbushDamage(uint32_t attackerId, float baseDamage) const {
    float bonus = AmbushManager::Instance().GetAmbushDamageBonus(attackerId);

    // Add mist concealment bonus
    // Would get attacker position from entity system
    glm::vec3 position{0.0f};  // Placeholder
    bonus += MistManager::Instance().GetConcealmentBonus(position);

    // Add night bonus
    if (IsNight()) {
        bonus += CryptidConstants::NIGHT_DAMAGE_BONUS;
    }

    // Add fear bonus against target
    // Would need target ID to check fear stacks

    return baseDamage * (1.0f + bonus);
}

bool CryptidRace::IsConcealed(uint32_t entityId) const {
    // Check if stealthed
    if (AmbushManager::Instance().IsStealthed(entityId)) {
        return true;
    }

    // Check if in mist
    // Would get entity position from entity system
    glm::vec3 position{0.0f};  // Placeholder
    return MistManager::Instance().IsInMist(position);
}

bool CryptidRace::CopyEnemyUnit(uint32_t entityId, uint32_t targetId) {
    return MimicryManager::Instance().StartDisguise(entityId, targetId);
}

std::string CryptidRace::GetUnitAppearance(uint32_t entityId) const {
    // Check if disguised
    std::string apparent = MimicryManager::Instance().GetApparentUnitType(entityId);
    if (!apparent.empty()) {
        return apparent;
    }

    // Check current transformation form
    std::string form = TransformationManager::Instance().GetCurrentForm(entityId);
    if (!form.empty()) {
        return form;
    }

    // Return actual unit type
    auto it = m_unitTypes.find(entityId);
    return (it != m_unitTypes.end()) ? it->second : "";
}

void CryptidRace::AddEssence(int amount) {
    m_essence = std::min(m_essence + amount, m_essenceCap);
}

bool CryptidRace::SpendEssence(int amount) {
    if (m_essence < amount) {
        return false;
    }
    m_essence -= amount;
    return true;
}

void CryptidRace::AddDread(int amount) {
    m_dread = std::min(m_dread + amount, m_dreadCap);
}

bool CryptidRace::SpendDread(int amount) {
    if (m_dread < amount) {
        return false;
    }
    m_dread -= amount;
    return true;
}

void CryptidRace::GenerateEssenceFromFear() {
    const auto& fearStatuses = FearManager::Instance().GetAllFearStatuses();
    int totalStacks = 0;

    for (const auto& [entityId, status] : fearStatuses) {
        totalStacks += status.stacks;
    }

    // Generate essence based on total fear stacks
    float essenceGenerated = static_cast<float>(totalStacks) * CryptidConstants::ESSENCE_GENERATION_RATE;
    AddEssence(static_cast<int>(essenceGenerated));
}

bool CryptidRace::CanPlaceBuilding(const std::string& buildingType, const glm::vec3& position) const {
    // Cryptids don't require special terrain like blight
    // But some buildings may have specific requirements

    auto it = m_buildingConfigs.find(buildingType);
    if (it == m_buildingConfigs.end()) {
        return false;
    }

    // Check for mist requirement (some buildings need existing mist)
    if (it->second.contains("construction")) {
        bool requiresMist = it->second["construction"].value("requiresMist", false);
        if (requiresMist && !MistManager::Instance().IsInMist(position)) {
            return false;
        }
    }

    return true;
}

void CryptidRace::OnBuildingConstructed(uint32_t buildingId, const std::string& buildingType,
                                         const glm::vec3& position) {
    m_buildings[buildingId] = buildingType;

    // Handle mist generator
    if (buildingType == "mist_generator") {
        float mistRadius = CryptidConstants::MIST_BASE_RADIUS;
        auto it = m_buildingConfigs.find(buildingType);
        if (it != m_buildingConfigs.end() && it->second.contains("mechanics")) {
            mistRadius = it->second["mechanics"].value("mistRadius", mistRadius);
        }
        MistManager::Instance().AddMistSource(buildingId, position, mistRadius / 128.0f, true);
    }

    // Track dens for population
    if (buildingType == "den" || buildingType == "breeding_pit") {
        ++m_denCount;
    }
}

void CryptidRace::OnBuildingDestroyed(uint32_t buildingId) {
    auto it = m_buildings.find(buildingId);
    if (it != m_buildings.end()) {
        // Track dens
        if (it->second == "den" || it->second == "breeding_pit") {
            --m_denCount;
        }

        // Remove mist source
        if (it->second == "mist_generator") {
            MistManager::Instance().RemoveMistSource(buildingId);
        }

        m_buildings.erase(it);
    }
}

void CryptidRace::RegisterWendigo(uint32_t entityId) {
    WendigoGrowth growth;
    growth.entityId = entityId;
    m_wendigoGrowth[entityId] = growth;
}

void CryptidRace::OnWendigoKill(uint32_t wendigoId) {
    auto it = m_wendigoGrowth.find(wendigoId);
    if (it != m_wendigoGrowth.end()) {
        it->second.AddKill();
        // Apply bonus stats to entity - would integrate with entity system
    }

    // Generate dread from kill
    AddDread(static_cast<int>(CryptidConstants::DREAD_GENERATION_RATE * 10));
}

const WendigoGrowth* CryptidRace::GetWendigoGrowth(uint32_t entityId) const {
    auto it = m_wendigoGrowth.find(entityId);
    return (it != m_wendigoGrowth.end()) ? &it->second : nullptr;
}

int CryptidRace::GetPopulationCap() const {
    int cap = CryptidConstants::BASE_POPULATION_CAP;
    cap += m_denCount * CryptidConstants::DEN_POPULATION;
    return std::min(cap, CryptidConstants::MAX_POPULATION);
}

bool CryptidRace::LoadConfiguration(const std::string& configPath) {
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

const nlohmann::json* CryptidRace::GetUnitConfig(const std::string& unitType) const {
    auto it = m_unitConfigs.find(unitType);
    return (it != m_unitConfigs.end()) ? &it->second : nullptr;
}

const nlohmann::json* CryptidRace::GetBuildingConfig(const std::string& buildingType) const {
    auto it = m_buildingConfigs.find(buildingType);
    return (it != m_buildingConfigs.end()) ? &it->second : nullptr;
}

const nlohmann::json* CryptidRace::GetHeroConfig(const std::string& heroType) const {
    auto it = m_heroConfigs.find(heroType);
    return (it != m_heroConfigs.end()) ? &it->second : nullptr;
}

// ============================================================================
// Ability Implementations
// ============================================================================

bool TransformAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    // Check if can transform
    std::string targetForm = data.GetLevelData(context.abilityLevel).targetType;
    return TransformationManager::Instance().CanTransform(context.casterId, targetForm);
}

AbilityCastResult TransformAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    std::string targetForm = data.GetLevelData(context.abilityLevel).targetType;
    result.success = CryptidRace::Instance().TransformUnit(context.casterId, targetForm);

    return result;
}

AbilityCastResult TerrifyAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Apply fear to all enemies in radius
    // Would integrate with entity query system
    int stacks = static_cast<int>(levelData.damage);  // Using damage field for stacks

    // For each enemy in radius, apply fear
    // CryptidRace::Instance().ApplyFearToTarget(enemyId, context.casterId, stacks);

    result.success = true;
    return result;
}

bool ParalyzingGazeAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return AbilityBehavior::CanCast(context, data) && context.targetUnit != nullptr;
}

AbilityCastResult ParalyzingGazeAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Apply stun
    // Would integrate with status effect system

    // Apply fear
    int stacks = static_cast<int>(levelData.damage);
    CryptidRace::Instance().ApplyFearToTarget(context.targetUnitId, context.casterId, stacks);

    result.success = true;
    result.unitsAffected = 1;
    return result;
}

AbilityCastResult ShadowMeldAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    result.success = AmbushManager::Instance().EnterStealth(context.casterId, true);
    return result;
}

bool PounceAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return AbilityBehavior::CanCast(context, data) && context.targetUnit != nullptr;
}

AbilityCastResult PounceAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Move caster to target
    // Would integrate with movement system

    // Deal damage
    result.damageDealt = levelData.damage;

    // Apply stun
    // Would integrate with status effect system

    // Break stealth
    AmbushManager::Instance().OnStealthAttack(context.casterId);

    result.success = true;
    result.unitsAffected = 1;
    return result;
}

bool CopyFormAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    if (!context.targetUnit) {
        return false;
    }

    // Check if target can be mimicked
    // Would get target unit type and check CanBeMimicked
    return true;
}

AbilityCastResult CopyFormAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    result.success = CryptidRace::Instance().CopyEnemyUnit(context.casterId, context.targetUnitId);
    return result;
}

bool RevealTrueFormAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    // Must be disguised
    return MimicryManager::Instance().IsDisguised(context.casterId);
}

AbilityCastResult RevealTrueFormAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // End disguise explosively
    MimicryManager::Instance().EndDisguise(context.casterId, true);

    // Deal AoE damage and stun
    // Would integrate with combat and status effect systems
    result.damageDealt = levelData.damage;

    result.success = true;
    return result;
}

AbilityCastResult LifeDrainAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    m_targetId = context.targetUnitId;
    m_channelTime = 0.0f;

    result.success = true;
    return result;
}

void LifeDrainAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    m_channelTime += deltaTime;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Drain health per tick
    float drainPerSecond = levelData.damage;
    float drainAmount = drainPerSecond * deltaTime;

    // Deal damage to target
    // Heal caster
    // Would integrate with health component
}

void LifeDrainAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    m_targetId = 0;
    m_channelTime = 0.0f;
}

AbilityCastResult MindShatterAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Apply massive fear to all enemies in radius
    // Would integrate with entity query system

    // Deal damage based on fear stacks
    // Higher fear = more damage

    result.success = true;
    result.damageDealt = levelData.damage;
    return result;
}

AbilityCastResult NightmareWalkAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Teleport to target location
    // Would integrate with movement system

    // Fear all enemies at destination
    // Would integrate with entity query system

    // Leave nightmare trail (temporary mist)
    MistManager::Instance().CreateTemporaryMist(context.targetPoint, 200.0f, 10.0f);

    result.success = true;
    return result;
}

bool ProphecyOfDoomAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    return AbilityBehavior::CanCast(context, data) && context.targetUnit != nullptr;
}

AbilityCastResult ProphecyOfDoomAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    m_targetId = context.targetUnitId;
    m_doomTimer = data.GetLevelData(context.abilityLevel).duration;

    // Apply doom mark visual effect
    // Would integrate with VFX system

    result.success = true;
    return result;
}

void ProphecyOfDoomAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    m_doomTimer -= deltaTime;

    if (m_doomTimer <= 0.0f) {
        // Doom triggers - deal massive damage
        const auto& levelData = data.GetLevelData(context.abilityLevel);
        // Would deal levelData.damage to target
    }
}

bool ConsumeAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    // Target must exist and be below health threshold
    if (!context.targetUnit) {
        return false;
    }

    // Would check if target health is below threshold
    return true;
}

AbilityCastResult ConsumeAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.targetUnit) {
        result.success = false;
        result.failReason = "No target";
        return result;
    }

    // Kill target instantly
    // Would integrate with combat system

    // Grant kill bonus to wendigo
    CryptidRace::Instance().OnWendigoKill(context.casterId);

    // Heal wendigo
    const auto& levelData = data.GetLevelData(context.abilityLevel);
    // Would heal caster by levelData.healAmount

    result.success = true;
    result.unitsAffected = 1;
    return result;
}

AbilityCastResult EldritchBlastAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Deal damage in a line or area
    // Would integrate with combat system

    // Apply sanity damage (fear)
    // CryptidRace::Instance().ApplyFearToTarget() for each hit

    result.success = true;
    result.damageDealt = levelData.damage;
    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

void RegisterCryptidAbilities() {
    auto& manager = AbilityManager::Instance();

    // Register custom ability behaviors
    manager.RegisterBehavior(200, std::make_unique<TransformAbility>());
    manager.RegisterBehavior(201, std::make_unique<TerrifyAbility>());
    manager.RegisterBehavior(202, std::make_unique<ParalyzingGazeAbility>());
    manager.RegisterBehavior(203, std::make_unique<ShadowMeldAbility>());
    manager.RegisterBehavior(204, std::make_unique<PounceAbility>());
    manager.RegisterBehavior(205, std::make_unique<CopyFormAbility>());
    manager.RegisterBehavior(206, std::make_unique<RevealTrueFormAbility>());
    manager.RegisterBehavior(207, std::make_unique<LifeDrainAbility>());
    manager.RegisterBehavior(208, std::make_unique<MindShatterAbility>());
    manager.RegisterBehavior(209, std::make_unique<NightmareWalkAbility>());
    manager.RegisterBehavior(210, std::make_unique<ProphecyOfDoomAbility>());
    manager.RegisterBehavior(211, std::make_unique<ConsumeAbility>());
    manager.RegisterBehavior(212, std::make_unique<EldritchBlastAbility>());
}

float GetFearResistance(const std::string& unitType) {
    // Units that resist fear
    static const std::unordered_map<std::string, float> fearResistances = {
        // Heroes have some resistance
        {"skinwalker_shaman", 0.5f},
        {"mothman_prophet", 0.75f},
        {"wendigo_alpha", 0.75f},
        {"shadow_thing", 1.0f},  // Immune
        // Ultimate units
        {"eldritch_horror", 1.0f},  // Immune
        // Undead are typically resistant
        {"death_knight", 0.75f},
        {"lich", 0.75f}
    };

    auto it = fearResistances.find(unitType);
    return (it != fearResistances.end()) ? it->second : 0.0f;
}

bool CanBeMimicked(const std::string& unitType) {
    // Units that cannot be mimicked
    static const std::unordered_set<std::string> nonMimickable = {
        // Buildings
        "hidden_grove", "sacred_ground", "nexus_of_fear",
        // Massive units
        "eldritch_horror", "frost_wyrm", "bone_colossus",
        // Other mimics/shapeshifters
        "mimic", "doppelganger",
        // Structures
        "tower", "wall"
    };

    return nonMimickable.count(unitType) == 0;
}

std::vector<std::string> GetTransformForms(const std::string& unitType) {
    static const std::unordered_map<std::string, std::vector<std::string>> transformForms = {
        {"skin_walker_initiate", {"wolf_form", "human_form"}},
        {"skinwalker_elder", {"wolf_form", "bear_form", "crow_form", "true_form"}},
        {"skinwalker_shaman", {"beast_form", "bird_form", "serpent_form", "true_form"}}
    };

    auto it = transformForms.find(unitType);
    return (it != transformForms.end()) ? it->second : std::vector<std::string>{};
}

} // namespace RTS
} // namespace Vehement
