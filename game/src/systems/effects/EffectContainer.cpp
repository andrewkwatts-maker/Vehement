#include "EffectContainer.hpp"
#include "EffectManager.hpp"
#include <algorithm>
#include <sstream>

namespace Vehement {
namespace Effects {

// ============================================================================
// Effect Container
// ============================================================================

EffectContainer::EffectContainer() = default;

EffectContainer::EffectContainer(uint32_t ownerId)
    : m_ownerId(ownerId) {
}

EffectContainer::~EffectContainer() = default;

// -------------------------------------------------------------------------
// Effect Application
// -------------------------------------------------------------------------

EffectApplicationResult EffectContainer::ApplyEffect(
    const EffectDefinition* definition,
    uint32_t sourceId
) {
    EffectApplicationResult result;

    if (!definition) {
        result.status = EffectApplicationResult::Status::Failed;
        result.message = "Null effect definition";
        return result;
    }

    // Check immunity
    if (IsImmuneToEffect(definition)) {
        result.status = EffectApplicationResult::Status::Immune;
        result.message = "Target is immune";
        return result;
    }

    // Check for existing effect
    EffectInstance* existing = FindExistingEffect(definition->GetId(), sourceId);

    if (existing) {
        const StackingConfig& stacking = definition->GetStacking();

        switch (stacking.mode) {
            case StackingMode::None:
            case StackingMode::Refresh:
                // Refresh existing effect
                existing->OnRefresh();
                result.status = EffectApplicationResult::Status::Refreshed;
                result.instance = existing;
                break;

            case StackingMode::Duration:
                // Add duration
                existing->ExtendDuration(definition->GetBaseDuration());
                result.status = EffectApplicationResult::Status::Refreshed;
                result.instance = existing;
                break;

            case StackingMode::Intensity: {
                // Add stacks
                int currentStacks = existing->GetStacks();
                if (currentStacks >= stacking.maxStacks) {
                    // At max stacks, just refresh
                    existing->OnRefresh();
                    result.status = EffectApplicationResult::Status::MaxStacks;
                    result.instance = existing;
                } else {
                    existing->OnStackAdded(currentStacks + 1);
                    result.status = EffectApplicationResult::Status::Stacked;
                    result.instance = existing;
                }
                break;
            }

            case StackingMode::Separate:
                // Fall through to create new instance
                break;
        }

        if (result.instance) {
            m_statCacheDirty = true;
            return result;
        }
    }

    // Create new instance
    auto instance = std::make_unique<EffectInstance>(definition);
    instance->OnApply(sourceId, m_ownerId);

    EffectInstance* instancePtr = instance.get();

    // Add to collections
    m_effectsByDefinitionId.emplace(definition->GetId(), instancePtr);
    m_effects.push_back(std::move(instance));

    // Grant immunities
    for (const auto& tag : definition->GetImmunityTags()) {
        m_immunityRefCounts[tag]++;
        m_immunities.insert(tag);
    }

    result.status = EffectApplicationResult::Status::Applied;
    result.instance = instancePtr;
    m_statCacheDirty = true;

    // Fire callback
    if (m_onEffectApplied) {
        m_onEffectApplied(instancePtr, "applied");
    }

    return result;
}

EffectApplicationResult EffectContainer::ApplyEffect(
    const std::string& effectId,
    uint32_t sourceId
) {
    if (!m_effectManager) {
        EffectApplicationResult result;
        result.status = EffectApplicationResult::Status::Failed;
        result.message = "No effect manager set";
        return result;
    }

    const EffectDefinition* definition = m_effectManager->GetDefinition(effectId);
    if (!definition) {
        EffectApplicationResult result;
        result.status = EffectApplicationResult::Status::Failed;
        result.message = "Effect not found: " + effectId;
        return result;
    }

    return ApplyEffect(definition, sourceId);
}

EffectApplicationResult EffectContainer::ApplyInstance(
    std::unique_ptr<EffectInstance> instance
) {
    EffectApplicationResult result;

    if (!instance) {
        result.status = EffectApplicationResult::Status::Failed;
        result.message = "Null instance";
        return result;
    }

    const EffectDefinition* definition = instance->GetDefinition();

    // Check immunity
    if (definition && IsImmuneToEffect(definition)) {
        result.status = EffectApplicationResult::Status::Immune;
        result.message = "Target is immune";
        return result;
    }

    instance->SetTargetId(m_ownerId);
    EffectInstance* instancePtr = instance.get();

    // Add to collections
    m_effectsByDefinitionId.emplace(instance->GetEffectId(), instancePtr);
    m_effects.push_back(std::move(instance));

    // Grant immunities
    if (definition) {
        for (const auto& tag : definition->GetImmunityTags()) {
            m_immunityRefCounts[tag]++;
            m_immunities.insert(tag);
        }
    }

    result.status = EffectApplicationResult::Status::Applied;
    result.instance = instancePtr;
    m_statCacheDirty = true;

    if (m_onEffectApplied) {
        m_onEffectApplied(instancePtr, "applied");
    }

    return result;
}

// -------------------------------------------------------------------------
// Effect Removal
// -------------------------------------------------------------------------

bool EffectContainer::RemoveEffect(EffectInstance::InstanceId instanceId) {
    auto it = std::find_if(m_effects.begin(), m_effects.end(),
        [instanceId](const auto& effect) {
            return effect->GetId() == instanceId;
        });

    if (it == m_effects.end()) {
        return false;
    }

    EffectInstance* instance = it->get();
    const EffectDefinition* definition = instance->GetDefinition();

    // Fire callback
    if (m_onEffectRemoved) {
        m_onEffectRemoved(instance, "removed");
    }

    instance->OnRemove();

    // Remove immunities
    if (definition) {
        for (const auto& tag : definition->GetImmunityTags()) {
            auto refIt = m_immunityRefCounts.find(tag);
            if (refIt != m_immunityRefCounts.end()) {
                refIt->second--;
                if (refIt->second <= 0) {
                    m_immunities.erase(tag);
                    m_immunityRefCounts.erase(refIt);
                }
            }
        }
    }

    // Remove from index
    auto range = m_effectsByDefinitionId.equal_range(instance->GetEffectId());
    for (auto indexIt = range.first; indexIt != range.second; ++indexIt) {
        if (indexIt->second == instance) {
            m_effectsByDefinitionId.erase(indexIt);
            break;
        }
    }

    m_effects.erase(it);
    m_statCacheDirty = true;
    return true;
}

int EffectContainer::RemoveEffectById(const std::string& effectId) {
    int removed = 0;
    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        if ((*it)->GetEffectId() == effectId) {
            if (m_onEffectRemoved) {
                m_onEffectRemoved(it->get(), "removed");
            }
            (*it)->OnRemove();

            // Update index
            auto range = m_effectsByDefinitionId.equal_range(effectId);
            for (auto indexIt = range.first; indexIt != range.second; ++indexIt) {
                if (indexIt->second == it->get()) {
                    m_effectsByDefinitionId.erase(indexIt);
                    break;
                }
            }

            it = m_effects.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        m_statCacheDirty = true;
        UpdateImmunities();
    }

    return removed;
}

int EffectContainer::RemoveEffectsBySource(uint32_t sourceId) {
    int removed = 0;
    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        if ((*it)->GetSourceId() == sourceId) {
            if (m_onEffectRemoved) {
                m_onEffectRemoved(it->get(), "removed");
            }
            (*it)->OnRemove();

            auto range = m_effectsByDefinitionId.equal_range((*it)->GetEffectId());
            for (auto indexIt = range.first; indexIt != range.second; ++indexIt) {
                if (indexIt->second == it->get()) {
                    m_effectsByDefinitionId.erase(indexIt);
                    break;
                }
            }

            it = m_effects.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        m_statCacheDirty = true;
        UpdateImmunities();
    }

    return removed;
}

int EffectContainer::RemoveEffectsByTag(const std::string& tag) {
    int removed = 0;
    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        const EffectDefinition* def = (*it)->GetDefinition();
        if (def && def->HasTag(tag)) {
            if (m_onEffectRemoved) {
                m_onEffectRemoved(it->get(), "removed");
            }
            (*it)->OnRemove();

            auto range = m_effectsByDefinitionId.equal_range((*it)->GetEffectId());
            for (auto indexIt = range.first; indexIt != range.second; ++indexIt) {
                if (indexIt->second == it->get()) {
                    m_effectsByDefinitionId.erase(indexIt);
                    break;
                }
            }

            it = m_effects.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        m_statCacheDirty = true;
        UpdateImmunities();
    }

    return removed;
}

int EffectContainer::RemoveEffectsByCategory(const std::string& category) {
    int removed = 0;
    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        const EffectDefinition* def = (*it)->GetDefinition();
        if (def) {
            const auto& categories = def->GetCategories();
            if (std::find(categories.begin(), categories.end(), category) != categories.end()) {
                if (m_onEffectRemoved) {
                    m_onEffectRemoved(it->get(), "removed");
                }
                (*it)->OnRemove();

                auto range = m_effectsByDefinitionId.equal_range((*it)->GetEffectId());
                for (auto indexIt = range.first; indexIt != range.second; ++indexIt) {
                    if (indexIt->second == it->get()) {
                        m_effectsByDefinitionId.erase(indexIt);
                        break;
                    }
                }

                it = m_effects.erase(it);
                removed++;
                continue;
            }
        }
        ++it;
    }

    if (removed > 0) {
        m_statCacheDirty = true;
        UpdateImmunities();
    }

    return removed;
}

int EffectContainer::RemoveAllBuffs() {
    int removed = 0;
    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        const EffectDefinition* def = (*it)->GetDefinition();
        if (def && def->GetType() == EffectType::Buff) {
            if (m_onEffectRemoved) {
                m_onEffectRemoved(it->get(), "removed");
            }
            (*it)->OnRemove();

            auto range = m_effectsByDefinitionId.equal_range((*it)->GetEffectId());
            for (auto indexIt = range.first; indexIt != range.second; ++indexIt) {
                if (indexIt->second == it->get()) {
                    m_effectsByDefinitionId.erase(indexIt);
                    break;
                }
            }

            it = m_effects.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        m_statCacheDirty = true;
        UpdateImmunities();
    }

    return removed;
}

int EffectContainer::RemoveAllDebuffs() {
    int removed = 0;
    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        const EffectDefinition* def = (*it)->GetDefinition();
        if (def && def->GetType() == EffectType::Debuff) {
            if (m_onEffectRemoved) {
                m_onEffectRemoved(it->get(), "removed");
            }
            (*it)->OnRemove();

            auto range = m_effectsByDefinitionId.equal_range((*it)->GetEffectId());
            for (auto indexIt = range.first; indexIt != range.second; ++indexIt) {
                if (indexIt->second == it->get()) {
                    m_effectsByDefinitionId.erase(indexIt);
                    break;
                }
            }

            it = m_effects.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        m_statCacheDirty = true;
        UpdateImmunities();
    }

    return removed;
}

int EffectContainer::DispelEffects(int maxDispels, bool buffsOnly, bool debuffsOnly) {
    int dispelled = 0;

    // Sort by priority (lower priority dispelled first)
    std::vector<EffectInstance*> candidates;
    for (const auto& effect : m_effects) {
        if (!effect->IsDispellable()) continue;

        const EffectDefinition* def = effect->GetDefinition();
        if (!def) continue;

        if (buffsOnly && def->GetType() != EffectType::Buff) continue;
        if (debuffsOnly && def->GetType() != EffectType::Debuff) continue;

        candidates.push_back(effect.get());
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const EffectInstance* a, const EffectInstance* b) {
            return a->GetPriority() < b->GetPriority();
        });

    for (EffectInstance* effect : candidates) {
        if (maxDispels >= 0 && dispelled >= maxDispels) break;

        if (m_onEffectRemoved) {
            m_onEffectRemoved(effect, "dispelled");
        }
        effect->OnDispel();
        dispelled++;
    }

    if (dispelled > 0) {
        CleanupExpiredEffects();
    }

    return dispelled;
}

int EffectContainer::PurgeEffects(int maxPurges) {
    int purged = 0;

    std::vector<EffectInstance*> candidates;
    for (const auto& effect : m_effects) {
        if (!effect->IsPurgeable()) continue;
        candidates.push_back(effect.get());
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const EffectInstance* a, const EffectInstance* b) {
            return a->GetPriority() < b->GetPriority();
        });

    for (EffectInstance* effect : candidates) {
        if (maxPurges >= 0 && purged >= maxPurges) break;

        if (m_onEffectRemoved) {
            m_onEffectRemoved(effect, "purged");
        }
        effect->OnRemove();
        purged++;
    }

    if (purged > 0) {
        CleanupExpiredEffects();
    }

    return purged;
}

void EffectContainer::ClearAll() {
    for (const auto& effect : m_effects) {
        if (m_onEffectRemoved) {
            m_onEffectRemoved(effect.get(), "cleared");
        }
        effect->OnRemove();
    }

    m_effects.clear();
    m_effectsByDefinitionId.clear();
    m_immunities.clear();
    m_immunityRefCounts.clear();
    m_statCacheDirty = true;
}

// -------------------------------------------------------------------------
// Querying
// -------------------------------------------------------------------------

bool EffectContainer::HasEffect(const std::string& effectId) const {
    return m_effectsByDefinitionId.find(effectId) != m_effectsByDefinitionId.end();
}

bool EffectContainer::HasEffectWithTag(const std::string& tag) const {
    for (const auto& effect : m_effects) {
        const EffectDefinition* def = effect->GetDefinition();
        if (def && def->HasTag(tag)) {
            return true;
        }
    }
    return false;
}

EffectInstance* EffectContainer::GetEffect(EffectInstance::InstanceId instanceId) const {
    for (const auto& effect : m_effects) {
        if (effect->GetId() == instanceId) {
            return effect.get();
        }
    }
    return nullptr;
}

EffectInstance* EffectContainer::GetEffectById(const std::string& effectId) const {
    auto it = m_effectsByDefinitionId.find(effectId);
    if (it != m_effectsByDefinitionId.end()) {
        return it->second;
    }
    return nullptr;
}

EffectQuery EffectContainer::QueryByType(EffectType type) const {
    EffectQuery query;
    for (const auto& effect : m_effects) {
        const EffectDefinition* def = effect->GetDefinition();
        if (def && def->GetType() == type) {
            query.results.push_back(effect.get());
        }
    }
    return query;
}

EffectQuery EffectContainer::QueryByTag(const std::string& tag) const {
    EffectQuery query;
    for (const auto& effect : m_effects) {
        const EffectDefinition* def = effect->GetDefinition();
        if (def && def->HasTag(tag)) {
            query.results.push_back(effect.get());
        }
    }
    return query;
}

EffectQuery EffectContainer::QueryBySource(uint32_t sourceId) const {
    EffectQuery query;
    for (const auto& effect : m_effects) {
        if (effect->GetSourceId() == sourceId) {
            query.results.push_back(effect.get());
        }
    }
    return query;
}

EffectQuery EffectContainer::Query(
    std::function<bool(const EffectInstance*)> predicate
) const {
    EffectQuery query;
    for (const auto& effect : m_effects) {
        if (predicate(effect.get())) {
            query.results.push_back(effect.get());
        }
    }
    return query;
}

size_t EffectContainer::GetBuffCount() const {
    return QueryByType(EffectType::Buff).Count();
}

size_t EffectContainer::GetDebuffCount() const {
    return QueryByType(EffectType::Debuff).Count();
}

// -------------------------------------------------------------------------
// Update
// -------------------------------------------------------------------------

void EffectContainer::Update(float deltaTime) {
    bool needsCleanup = false;

    for (const auto& effect : m_effects) {
        effect->Update(deltaTime);

        if (effect->IsExpired()) {
            needsCleanup = true;
            if (m_onEffectExpired) {
                m_onEffectExpired(effect.get(), "expired");
            }
        }
    }

    if (needsCleanup) {
        CleanupExpiredEffects();
    }
}

std::vector<std::pair<EffectInstance*, const EffectTrigger*>> EffectContainer::ProcessTriggers(
    const TriggerEventData& eventData
) {
    std::vector<std::pair<EffectInstance*, const EffectTrigger*>> firedTriggers;

    for (const auto& effect : m_effects) {
        if (!effect->IsActive()) continue;

        auto triggers = effect->ProcessTriggerEvent(eventData);
        for (const auto* trigger : triggers) {
            firedTriggers.emplace_back(effect.get(), trigger);
        }
    }

    return firedTriggers;
}

void EffectContainer::ResetCombat() {
    for (const auto& effect : m_effects) {
        effect->ResetCombatTriggers();
    }
}

// -------------------------------------------------------------------------
// Stat Calculation
// -------------------------------------------------------------------------

float EffectContainer::CalculateStat(
    StatType stat,
    float baseValue,
    const std::unordered_map<std::string, float>& context
) const {
    float currentValue = baseValue;

    // Collect all modifiers for this stat
    std::vector<StatModifier> modifiers = GetModifiersForStat(stat);

    // Sort by priority
    std::sort(modifiers.begin(), modifiers.end());

    // Apply in order: Add, Percent, Multiply, Set/Min/Max
    // First pass: Add
    for (const auto& mod : modifiers) {
        if (mod.operation == ModifierOp::Add && mod.ShouldApply(context)) {
            currentValue = mod.Apply(baseValue, currentValue);
        }
    }

    // Second pass: Percent
    for (const auto& mod : modifiers) {
        if (mod.operation == ModifierOp::Percent && mod.ShouldApply(context)) {
            currentValue = mod.Apply(baseValue, currentValue);
        }
    }

    // Third pass: Multiply
    for (const auto& mod : modifiers) {
        if (mod.operation == ModifierOp::Multiply && mod.ShouldApply(context)) {
            currentValue = mod.Apply(baseValue, currentValue);
        }
    }

    // Fourth pass: Set/Min/Max
    for (const auto& mod : modifiers) {
        if ((mod.operation == ModifierOp::Set ||
             mod.operation == ModifierOp::Min ||
             mod.operation == ModifierOp::Max) && mod.ShouldApply(context)) {
            currentValue = mod.Apply(baseValue, currentValue);
        }
    }

    return currentValue;
}

std::vector<StatModifier> EffectContainer::GetModifiersForStat(StatType stat) const {
    std::vector<StatModifier> result;

    for (const auto& effect : m_effects) {
        if (!effect->IsActive()) continue;

        auto mods = effect->GetActiveModifiers();
        for (const auto& mod : mods) {
            if (mod.stat == stat) {
                result.push_back(mod);
            }
        }
    }

    return result;
}

std::vector<StatModifier> EffectContainer::GetAllModifiers() const {
    std::vector<StatModifier> result;

    for (const auto& effect : m_effects) {
        if (!effect->IsActive()) continue;

        auto mods = effect->GetActiveModifiers();
        result.insert(result.end(), mods.begin(), mods.end());
    }

    return result;
}

void EffectContainer::InvalidateStatCache() {
    m_statCacheDirty = true;
}

// -------------------------------------------------------------------------
// Immunity
// -------------------------------------------------------------------------

void EffectContainer::AddImmunity(const std::string& tag) {
    m_immunities.insert(tag);
}

void EffectContainer::RemoveImmunity(const std::string& tag) {
    // Only remove if not granted by an effect
    if (m_immunityRefCounts.find(tag) == m_immunityRefCounts.end()) {
        m_immunities.erase(tag);
    }
}

bool EffectContainer::IsImmuneTo(const std::string& tag) const {
    return m_immunities.find(tag) != m_immunities.end();
}

bool EffectContainer::IsImmuneToEffect(const EffectDefinition* definition) const {
    if (!definition) return false;

    // Check tags
    for (const auto& tag : definition->GetTags()) {
        if (IsImmuneTo(tag)) {
            return true;
        }
    }

    // Check effect ID as a tag
    if (IsImmuneTo(definition->GetId())) {
        return true;
    }

    return false;
}

// -------------------------------------------------------------------------
// Periodic Effects
// -------------------------------------------------------------------------

std::vector<std::pair<EffectInstance*, PeriodicEffect>> EffectContainer::ConsumePendingTicks() {
    std::vector<std::pair<EffectInstance*, PeriodicEffect>> result;

    for (const auto& effect : m_effects) {
        if (!effect->IsActive()) continue;

        auto ticks = effect->ConsumePendingTicks();
        for (const auto& tick : ticks) {
            result.emplace_back(effect.get(), tick);
        }
    }

    return result;
}

// -------------------------------------------------------------------------
// Serialization
// -------------------------------------------------------------------------

std::string EffectContainer::SerializeState() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"owner_id\":" << m_ownerId;
    ss << ",\"effects\":[";

    bool first = true;
    for (const auto& effect : m_effects) {
        if (effect->IsPersistent()) {
            if (!first) ss << ",";
            ss << effect->SerializeState();
            first = false;
        }
    }

    ss << "]";
    ss << "}";
    return ss.str();
}

bool EffectContainer::DeserializeState(const std::string& data) {
    // In production, use proper JSON parsing
    return true;
}

// -------------------------------------------------------------------------
// Private Helpers
// -------------------------------------------------------------------------

void EffectContainer::CleanupExpiredEffects() {
    auto it = m_effects.begin();
    while (it != m_effects.end()) {
        if ((*it)->IsExpired()) {
            // Update index
            auto range = m_effectsByDefinitionId.equal_range((*it)->GetEffectId());
            for (auto indexIt = range.first; indexIt != range.second; ++indexIt) {
                if (indexIt->second == it->get()) {
                    m_effectsByDefinitionId.erase(indexIt);
                    break;
                }
            }

            it = m_effects.erase(it);
        } else {
            ++it;
        }
    }

    m_statCacheDirty = true;
    UpdateImmunities();
}

void EffectContainer::UpdateImmunities() {
    m_immunityRefCounts.clear();

    for (const auto& effect : m_effects) {
        const EffectDefinition* def = effect->GetDefinition();
        if (!def) continue;

        for (const auto& tag : def->GetImmunityTags()) {
            m_immunityRefCounts[tag]++;
        }
    }

    // Remove immunities that are no longer granted
    std::unordered_set<std::string> toRemove;
    for (const auto& immunity : m_immunities) {
        if (m_immunityRefCounts.find(immunity) == m_immunityRefCounts.end()) {
            // Check if it's a manually added immunity (we keep those)
            // For simplicity, we remove it
            toRemove.insert(immunity);
        }
    }
}

EffectInstance* EffectContainer::FindExistingEffect(const std::string& effectId, uint32_t sourceId) {
    auto range = m_effectsByDefinitionId.equal_range(effectId);

    for (auto it = range.first; it != range.second; ++it) {
        EffectInstance* instance = it->second;

        // Check if stacking is separate per source
        const EffectDefinition* def = instance->GetDefinition();
        if (def && def->GetStacking().separatePerSource) {
            if (instance->GetSourceId() == sourceId) {
                return instance;
            }
        } else {
            return instance;  // Any instance matches
        }
    }

    return nullptr;
}

} // namespace Effects
} // namespace Vehement
