#include "Upkeep.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Vehement {
namespace RTS {

// ============================================================================
// Status Helpers
// ============================================================================

const char* GetUpkeepStatusName(UpkeepStatus status) {
    switch (status) {
        case UpkeepStatus::Healthy:   return "Healthy";
        case UpkeepStatus::Adequate:  return "Adequate";
        case UpkeepStatus::Low:       return "Low";
        case UpkeepStatus::Critical:  return "Critical";
        case UpkeepStatus::Depleted:  return "Depleted";
        default:                      return "Unknown";
    }
}

uint32_t GetUpkeepStatusColor(UpkeepStatus status) {
    switch (status) {
        case UpkeepStatus::Healthy:   return 0x4CAF50FF; // Green
        case UpkeepStatus::Adequate:  return 0x8BC34AFF; // Light green
        case UpkeepStatus::Low:       return 0xFFC107FF; // Yellow
        case UpkeepStatus::Critical:  return 0xFF9800FF; // Orange
        case UpkeepStatus::Depleted:  return 0xF44336FF; // Red
        default:                      return 0xFFFFFFFF; // White
    }
}

// ============================================================================
// UpkeepSystem Implementation
// ============================================================================

UpkeepSystem::UpkeepSystem() = default;

UpkeepSystem::~UpkeepSystem() {
    Shutdown();
}

void UpkeepSystem::Initialize(const UpkeepConfig& config) {
    m_config = config;
    m_scarcitySettings = ScarcitySettings::Normal();

    // Initialize tracking for all resource types
    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        auto type = static_cast<ResourceType>(i);
        m_consumptionAccumulators[type] = 0.0f;
        m_totalConsumed[type] = 0;
        m_totalStarvationTime[type] = 0.0f;

        StarvationEffect effect;
        effect.resourceType = type;
        effect.active = false;
        effect.duration = 0.0f;
        effect.damagePerSecond = config.starvationDamageAmount / config.starvationDamageInterval;
        effect.speedMultiplier = config.starvationSpeedPenalty;
        effect.productionMultiplier = config.starvationProductionPenalty;
        effect.moraleMultiplier = 0.5f;
        m_starvationEffects[type] = effect;
    }

    m_initialized = true;
}

void UpkeepSystem::Shutdown() {
    m_sources.clear();
    m_warnings.clear();
    m_initialized = false;
}

void UpkeepSystem::Update(float deltaTime) {
    if (!m_initialized || !m_resourceStock) return;

    m_updateTimer += deltaTime;

    // Always process consumption
    UpdateConsumption(deltaTime);
    UpdateStarvation(deltaTime);

    // Less frequent warning updates
    if (m_updateTimer >= m_config.updateInterval) {
        UpdateWarnings();
        m_updateTimer = 0.0f;
    }
}

// -------------------------------------------------------------------------
// Upkeep Source Management
// -------------------------------------------------------------------------

uint32_t UpkeepSystem::RegisterWorker(const glm::vec2& position, float* healthRef) {
    UpkeepSource source;
    source.id = GenerateSourceId();
    source.type = UpkeepSourceType::Worker;
    source.resourceType = ResourceType::Food;
    source.consumptionRate = m_config.workerFoodConsumption * m_scarcitySettings.consumptionMultiplier;
    source.active = true;
    source.name = "Worker";
    source.position = position;
    source.healthRef = healthRef;

    m_sources.push_back(std::move(source));
    return m_sources.back().id;
}

uint32_t UpkeepSystem::RegisterBuilding(const std::string& name, const glm::vec2& position, float consumptionRate) {
    UpkeepSource source;
    source.id = GenerateSourceId();
    source.type = UpkeepSourceType::Building;
    source.resourceType = ResourceType::Fuel;
    source.consumptionRate = consumptionRate * m_scarcitySettings.consumptionMultiplier;
    source.active = true;
    source.name = name;
    source.position = position;

    m_sources.push_back(std::move(source));
    return m_sources.back().id;
}

uint32_t UpkeepSystem::RegisterDefense(const std::string& name, const glm::vec2& position) {
    UpkeepSource source;
    source.id = GenerateSourceId();
    source.type = UpkeepSourceType::DefenseStructure;
    source.resourceType = ResourceType::Ammunition;
    source.consumptionRate = 0.0f; // Defense consumes on-demand, not per-second
    source.active = true;
    source.name = name;
    source.position = position;

    m_sources.push_back(std::move(source));
    return m_sources.back().id;
}

uint32_t UpkeepSystem::RegisterSource(const UpkeepSource& source) {
    UpkeepSource newSource = source;
    newSource.id = GenerateSourceId();
    newSource.consumptionRate *= m_scarcitySettings.consumptionMultiplier;

    m_sources.push_back(std::move(newSource));
    return m_sources.back().id;
}

void UpkeepSystem::UnregisterSource(uint32_t sourceId) {
    m_sources.erase(
        std::remove_if(m_sources.begin(), m_sources.end(),
            [sourceId](const UpkeepSource& s) { return s.id == sourceId; }),
        m_sources.end()
    );
}

void UpkeepSystem::SetSourceActive(uint32_t sourceId, bool active) {
    for (auto& source : m_sources) {
        if (source.id == sourceId) {
            source.active = active;
            return;
        }
    }
}

const UpkeepSource* UpkeepSystem::GetSource(uint32_t sourceId) const {
    for (const auto& source : m_sources) {
        if (source.id == sourceId) return &source;
    }
    return nullptr;
}

// -------------------------------------------------------------------------
// Upkeep Calculation
// -------------------------------------------------------------------------

float UpkeepSystem::GetTotalConsumption(ResourceType type) const {
    float total = 0.0f;
    for (const auto& source : m_sources) {
        if (source.resourceType == type && source.active) {
            total += source.consumptionRate;
        }
    }
    return total;
}

float UpkeepSystem::GetConsumptionByType(UpkeepSourceType sourceType) const {
    float total = 0.0f;
    for (const auto& source : m_sources) {
        if (source.type == sourceType && source.active) {
            total += source.consumptionRate;
        }
    }
    return total;
}

int UpkeepSystem::GetActiveSourceCount(UpkeepSourceType type) const {
    return static_cast<int>(std::count_if(m_sources.begin(), m_sources.end(),
        [type](const UpkeepSource& s) { return s.type == type && s.active; }));
}

float UpkeepSystem::GetTimeUntilDepletion(ResourceType type) const {
    if (!m_resourceStock) return -1.0f;

    float consumption = GetTotalConsumption(type);
    float income = m_resourceStock->GetNetRate(type);

    float netRate = income - consumption;
    if (netRate >= 0) return -1.0f; // Not depleting

    int current = m_resourceStock->GetAmount(type);
    return current / (-netRate);
}

// -------------------------------------------------------------------------
// Upkeep Status
// -------------------------------------------------------------------------

UpkeepStatus UpkeepSystem::GetStatus(ResourceType type) const {
    if (!m_resourceStock) return UpkeepStatus::Healthy;

    int amount = m_resourceStock->GetAmount(type);
    int capacity = m_resourceStock->GetCapacity(type);

    if (capacity <= 0) return UpkeepStatus::Healthy;

    float percentage = static_cast<float>(amount) / capacity;

    // Check if we're actively consuming this resource
    float consumption = GetTotalConsumption(type);
    if (consumption <= 0) return UpkeepStatus::Healthy;

    if (amount <= 0) return UpkeepStatus::Depleted;
    if (percentage < m_config.criticalThreshold) return UpkeepStatus::Critical;
    if (percentage < m_config.lowThreshold) return UpkeepStatus::Low;
    if (percentage < m_config.adequateThreshold) return UpkeepStatus::Adequate;
    return UpkeepStatus::Healthy;
}

void UpkeepSystem::AcknowledgeWarning(ResourceType type) {
    for (auto& warning : m_warnings) {
        if (warning.resourceType == type) {
            warning.acknowledged = true;
        }
    }
}

bool UpkeepSystem::IsStarving(ResourceType type) const {
    auto it = m_starvationEffects.find(type);
    return it != m_starvationEffects.end() && it->second.active;
}

const StarvationEffect& UpkeepSystem::GetStarvationEffect(ResourceType type) const {
    static StarvationEffect defaultEffect;
    auto it = m_starvationEffects.find(type);
    if (it == m_starvationEffects.end()) return defaultEffect;
    return it->second;
}

// -------------------------------------------------------------------------
// Defense Ammunition
// -------------------------------------------------------------------------

bool UpkeepSystem::ConsumeDefenseAmmo(uint32_t defenseId) {
    if (!m_resourceStock) return false;

    // Verify defense exists and is active
    bool validDefense = false;
    for (const auto& source : m_sources) {
        if (source.id == defenseId && source.type == UpkeepSourceType::DefenseStructure && source.active) {
            validDefense = true;
            break;
        }
    }

    if (!validDefense) return false;

    // Try to consume ammunition
    if (m_resourceStock->CanAfford(ResourceType::Ammunition, m_config.defenseAmmoPerShot)) {
        m_resourceStock->Remove(ResourceType::Ammunition, m_config.defenseAmmoPerShot);
        m_totalConsumed[ResourceType::Ammunition] += m_config.defenseAmmoPerShot;
        return true;
    }

    return false;
}

bool UpkeepSystem::HasDefenseAmmo() const {
    if (!m_resourceStock) return false;
    return m_resourceStock->GetAmount(ResourceType::Ammunition) >= m_config.defenseAmmoPerShot;
}

// -------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------

void UpkeepSystem::ApplyScarcitySettings(const ScarcitySettings& settings) {
    // Update existing sources
    float oldMultiplier = m_scarcitySettings.consumptionMultiplier;
    float newMultiplier = settings.consumptionMultiplier;

    for (auto& source : m_sources) {
        // Adjust consumption rate
        source.consumptionRate = (source.consumptionRate / oldMultiplier) * newMultiplier;
    }

    m_scarcitySettings = settings;
}

// -------------------------------------------------------------------------
// Statistics
// -------------------------------------------------------------------------

int UpkeepSystem::GetTotalConsumed(ResourceType type) const {
    auto it = m_totalConsumed.find(type);
    return it != m_totalConsumed.end() ? it->second : 0;
}

float UpkeepSystem::GetTotalStarvationTime(ResourceType type) const {
    auto it = m_totalStarvationTime.find(type);
    return it != m_totalStarvationTime.end() ? it->second : 0.0f;
}

// -------------------------------------------------------------------------
// Private Methods
// -------------------------------------------------------------------------

void UpkeepSystem::UpdateConsumption(float deltaTime) {
    if (!m_resourceStock) return;

    // Accumulate consumption for each resource type
    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        auto type = static_cast<ResourceType>(i);
        float consumption = GetTotalConsumption(type) * deltaTime;

        m_consumptionAccumulators[type] += consumption;

        // Apply whole units of consumption
        int wholeUnits = static_cast<int>(m_consumptionAccumulators[type]);
        if (wholeUnits > 0) {
            m_consumptionAccumulators[type] -= wholeUnits;

            int available = m_resourceStock->GetAmount(type);
            int consumed = std::min(wholeUnits, available);

            if (consumed > 0) {
                m_resourceStock->Remove(type, consumed);
                m_totalConsumed[type] += consumed;
            }

            // Check if we couldn't consume enough (starvation starts)
            if (consumed < wholeUnits && GetTotalConsumption(type) > 0) {
                auto& effect = m_starvationEffects[type];
                if (!effect.active) {
                    effect.active = true;
                    effect.duration = 0.0f;
                    if (m_onStarvation) {
                        m_onStarvation(type, true);
                    }
                }
            }
        }
    }
}

void UpkeepSystem::UpdateStarvation(float deltaTime) {
    m_starvationDamageTimer += deltaTime;

    for (auto& [type, effect] : m_starvationEffects) {
        if (effect.active) {
            effect.duration += deltaTime;
            m_totalStarvationTime[type] += deltaTime;

            // Check if resources have recovered
            if (m_resourceStock && m_resourceStock->GetAmount(type) > 0) {
                effect.active = false;
                effect.duration = 0.0f;
                if (m_onStarvation) {
                    m_onStarvation(type, false);
                }
            }
        }
    }

    // Apply starvation damage at intervals
    if (m_starvationDamageTimer >= m_config.starvationDamageInterval) {
        m_starvationDamageTimer = 0.0f;
        ApplyStarvationDamage(m_config.starvationDamageInterval);
    }
}

void UpkeepSystem::ApplyStarvationDamage(float deltaTime) {
    // Food starvation damages workers
    if (IsStarving(ResourceType::Food)) {
        for (auto& source : m_sources) {
            if (source.type == UpkeepSourceType::Worker && source.active && source.healthRef) {
                *source.healthRef -= m_config.starvationDamageAmount;
                CheckSourceDeath(source);
            }
        }
    }

    // Fuel depletion could disable buildings (handled elsewhere)
    // Ammo depletion just prevents firing (handled in ConsumeDefenseAmmo)
}

void UpkeepSystem::CheckSourceDeath(UpkeepSource& source) {
    if (!source.healthRef || *source.healthRef > 0) return;

    source.active = false;

    if (source.type == UpkeepSourceType::Worker) {
        m_workersLostToStarvation++;
    }

    if (m_onSourceDied) {
        m_onSourceDied(source);
    }
}

void UpkeepSystem::UpdateWarnings() {
    m_warnings.clear();

    // Check each resource type that has active consumption
    std::vector<ResourceType> consumedTypes = {
        ResourceType::Food,
        ResourceType::Fuel,
        ResourceType::Ammunition
    };

    for (auto type : consumedTypes) {
        float consumption = GetTotalConsumption(type);
        if (consumption <= 0) continue;

        UpkeepWarning warning;
        warning.resourceType = type;
        warning.status = GetStatus(type);
        warning.timeUntilDepletion = GetTimeUntilDepletion(type);

        if (m_resourceStock) {
            float income = m_resourceStock->GetNetRate(type);
            warning.netRate = income - consumption;
        }

        // Generate message
        std::stringstream ss;
        switch (warning.status) {
            case UpkeepStatus::Critical:
                ss << GetResourceName(type) << " critically low!";
                if (warning.timeUntilDepletion > 0) {
                    ss << " Depletes in " << static_cast<int>(warning.timeUntilDepletion) << "s";
                }
                break;

            case UpkeepStatus::Low:
                ss << GetResourceName(type) << " running low.";
                break;

            case UpkeepStatus::Depleted:
                ss << GetResourceName(type) << " depleted! ";
                if (type == ResourceType::Food) {
                    ss << "Workers starving!";
                } else if (type == ResourceType::Fuel) {
                    ss << "Buildings shutting down!";
                } else if (type == ResourceType::Ammunition) {
                    ss << "Defenses cannot fire!";
                }
                break;

            default:
                continue; // No warning needed
        }

        warning.message = ss.str();
        warning.acknowledged = false;

        m_warnings.push_back(warning);

        if (m_onWarning && warning.status >= UpkeepStatus::Low) {
            m_onWarning(warning);
        }
    }
}

uint32_t UpkeepSystem::GenerateSourceId() {
    return m_nextSourceId++;
}

// ============================================================================
// UpkeepCalculator Implementation
// ============================================================================

int UpkeepCalculator::CalculateDailyFoodNeed(int workerCount, const UpkeepConfig& config) {
    float perSecond = workerCount * config.workerFoodConsumption;
    float perDay = perSecond * 86400.0f; // 24 * 60 * 60
    return static_cast<int>(std::ceil(perDay));
}

int UpkeepCalculator::CalculateFuelNeed(float buildingConsumption, float hours) {
    float total = buildingConsumption * hours * 3600.0f;
    return static_cast<int>(std::ceil(total));
}

int UpkeepCalculator::EstimateAmmoNeed(
    int defenseCount,
    float expectedAttacksPerHour,
    int shotsPerAttack,
    const UpkeepConfig& config
) {
    float attacksTotal = expectedAttacksPerHour;
    float totalShots = attacksTotal * defenseCount * shotsPerAttack;
    int ammoNeeded = static_cast<int>(totalShots) * config.defenseAmmoPerShot;
    return ammoNeeded;
}

bool UpkeepCalculator::CanSustainWorkers(
    const ResourceStock& stock,
    int workerCount,
    float hours,
    const UpkeepConfig& config
) {
    float seconds = hours * 3600.0f;
    float totalConsumption = workerCount * config.workerFoodConsumption * seconds;
    int needed = static_cast<int>(std::ceil(totalConsumption));

    return stock.CanAfford(ResourceType::Food, needed);
}

} // namespace RTS
} // namespace Vehement
