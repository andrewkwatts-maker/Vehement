#include "InfectionSystem.hpp"
#include "EntityManager.hpp"
#include "NPC.hpp"
#include "Zombie.hpp"
#include <engine/math/Random.hpp>
#include <algorithm>
#include <numeric>

namespace Vehement {

// ============================================================================
// InfectionConfig Implementation
// ============================================================================

float InfectionConfig::GetRandomDuration() const {
    if (infectionDurationVariance <= 0.0f) {
        return infectionDuration;
    }

    float variance = Nova::Random::Range(-infectionDurationVariance, infectionDurationVariance);
    return std::max(1.0f, infectionDuration + variance);
}

// ============================================================================
// InfectionSystem Implementation
// ============================================================================

InfectionSystem::InfectionSystem() {
    // Default configuration
    m_config.baseInfectionChance = 0.3f;
    m_config.infectionDuration = 30.0f;
    m_config.infectionDurationVariance = 5.0f;
    m_config.allowCure = true;
    m_config.cureWindow = 0.1f;
    m_config.proximityInfectionRadius = 0.0f;
    m_config.proximityInfectionChance = 0.0f;
}

InfectionSystem::InfectionSystem(const InfectionConfig& config)
    : m_config(config) {
}

void InfectionSystem::Update(float deltaTime, EntityManager& entityManager) {
    // Process proximity infections if enabled
    if (m_config.proximityInfectionRadius > 0.0f && m_config.proximityInfectionChance > 0.0f) {
        ProcessProximityInfection(deltaTime, entityManager);
    }

    // Check for NPCs that need to be removed from tracking (dead or removed)
    std::vector<Entity::EntityId> toRemove;

    for (Entity::EntityId npcId : m_infectedNPCs) {
        Entity* entity = entityManager.GetEntity(npcId);

        if (!entity || entity->IsMarkedForRemoval()) {
            toRemove.push_back(npcId);
            continue;
        }

        // Verify it's still an NPC
        if (entity->GetType() != EntityType::NPC) {
            toRemove.push_back(npcId);
            continue;
        }

        NPC* npc = static_cast<NPC*>(entity);

        // Check if NPC died before turning
        if (!npc->IsAlive()) {
            toRemove.push_back(npcId);
            continue;
        }

        // Handle conversion if NPC is in turning state and timer expired
        if (npc->IsTurning() && npc->GetInfectionTimer() <= 0.0f) {
            HandleConversion(*npc, entityManager);
            toRemove.push_back(npcId);
        }
    }

    // Remove NPCs no longer being tracked
    for (Entity::EntityId id : toRemove) {
        StopTracking(id);
    }

    // Update current count statistic
    m_stats.currentlyInfected = static_cast<uint32_t>(m_infectedNPCs.size());
}

bool InfectionSystem::InfectNPC(NPC& npc, Entity::EntityId source) {
    // Already infected?
    if (npc.IsInfected()) {
        return false;
    }

    // Already dead?
    if (!npc.IsAlive()) {
        return false;
    }

    // Calculate infection duration
    float duration = m_config.GetRandomDuration();
    npc.SetInfectionDuration(duration);

    // Infect the NPC
    npc.Infect();

    // Track the NPC
    m_infectedNPCs.insert(npc.GetId());

    // Update statistics
    m_stats.totalInfected++;
    m_stats.sessionInfected++;
    m_stats.currentlyInfected = static_cast<uint32_t>(m_infectedNPCs.size());

    // Set up turn callback to notify this system
    npc.SetTurnCallback([this, &npc](NPC& turningNPC) {
        // Conversion is handled in Update
    });

    // Fire callback
    if (m_onInfection) {
        m_onInfection(npc, source);
    }

    return true;
}

bool InfectionSystem::CureNPC(NPC& npc) {
    // Check if cures are allowed
    if (!m_config.allowCure) {
        return false;
    }

    // Must be infected but not in final phase
    if (!npc.IsInfected()) {
        return false;
    }

    // Check cure window - can't cure in final % of duration
    float progress = npc.GetInfectionProgress();
    if (progress >= (1.0f - m_config.cureWindow)) {
        return false;  // Too late to cure
    }

    // Cure the NPC
    if (npc.Cure()) {
        // Stop tracking
        StopTracking(npc.GetId());

        // Update statistics
        m_stats.totalCured++;
        m_stats.sessionCured++;
        m_stats.currentlyInfected = static_cast<uint32_t>(m_infectedNPCs.size());

        // Fire callback
        if (m_onCure) {
            m_onCure(npc);
        }

        return true;
    }

    return false;
}

bool InfectionSystem::RollInfection(float bonusChance) const {
    float chance = std::clamp(m_config.baseInfectionChance + bonusChance, 0.0f, 1.0f);
    return Nova::Random::Value() < chance;
}

bool InfectionSystem::IsTracked(Entity::EntityId npcId) const {
    return m_infectedNPCs.find(npcId) != m_infectedNPCs.end();
}

Zombie* InfectionSystem::SpawnZombieFromInfection(EntityManager& entityManager,
                                                  const glm::vec3& position,
                                                  ZombieType zombieType) {
    // Create zombie at NPC's position
    Zombie* zombie = entityManager.CreateEntity<Zombie>(zombieType);

    if (zombie) {
        zombie->SetPosition(position);
        zombie->SetHomePosition(position);

        // Start in chase state if there are targets nearby
        zombie->SetState(ZombieState::Idle);
    }

    return zombie;
}

void InfectionSystem::HandleConversion(NPC& npc, EntityManager& entityManager) {
    glm::vec3 position = npc.GetPosition();
    float conversionTime = npc.GetInfectionDuration() - npc.GetInfectionTimer();

    // Update statistics
    m_stats.totalConverted++;
    m_stats.sessionConverted++;

    // Track conversion time for average
    m_conversionTimes.push_back(conversionTime);
    if (m_conversionTimes.size() > 100) {
        m_conversionTimes.erase(m_conversionTimes.begin());
    }

    // Calculate running average
    if (!m_conversionTimes.empty()) {
        float sum = std::accumulate(m_conversionTimes.begin(), m_conversionTimes.end(), 0.0f);
        m_stats.averageConversionTime = sum / static_cast<float>(m_conversionTimes.size());
    }

    // Fire callback for custom handling
    if (m_onConversion) {
        m_onConversion(npc, position);
    } else {
        // Default behavior: spawn a standard zombie
        SpawnZombieFromInfection(entityManager, position, ZombieType::Standard);
    }

    // NPC will be marked for removal by its own Update
}

void InfectionSystem::ProcessProximityInfection(float deltaTime, EntityManager& entityManager) {
    // Get all zombies
    auto zombies = entityManager.GetEntitiesByType(EntityType::Zombie);

    for (Entity* zombieEntity : zombies) {
        if (!zombieEntity->IsAlive()) continue;

        // Find NPCs in proximity range
        auto nearbyNPCs = entityManager.FindEntitiesInRadius(
            zombieEntity->GetPosition(),
            m_config.proximityInfectionRadius,
            EntityType::NPC
        );

        for (Entity* npcEntity : nearbyNPCs) {
            NPC* npc = static_cast<NPC*>(npcEntity);

            // Skip already infected
            if (npc->IsInfected()) continue;

            // Roll for proximity infection (scaled by deltaTime for per-second chance)
            float chanceThisFrame = m_config.proximityInfectionChance * deltaTime;
            if (Nova::Random::Value() < chanceThisFrame) {
                InfectNPC(*npc, zombieEntity->GetId());
            }
        }
    }
}

void InfectionSystem::StopTracking(Entity::EntityId npcId) {
    m_infectedNPCs.erase(npcId);
    m_stats.currentlyInfected = static_cast<uint32_t>(m_infectedNPCs.size());
}

} // namespace Vehement
