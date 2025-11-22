#include "Recruitment.hpp"
#include "../entities/EntityManager.hpp"
#include "../entities/Player.hpp"
#include <engine/math/Random.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// Name and hint generation data
// ============================================================================

static const char* s_personalityHints[] = {
    "Seems brave and reliable",
    "Appears cautious but dependable",
    "Looks hardworking",
    "Has a cheerful demeanor",
    "Seems reserved but competent",
    "Appears eager to help",
    "Looks experienced",
    "Has a determined expression",
    "Seems friendly and social",
    "Appears tough and resilient"
};

// ============================================================================
// Construction
// ============================================================================

Recruitment::Recruitment() = default;

Recruitment::Recruitment(const RecruitmentConfig& config)
    : m_config(config) {
}

Recruitment::~Recruitment() = default;

// ============================================================================
// Core Update
// ============================================================================

void Recruitment::Update(float deltaTime, EntityManager& entityManager, Population& population,
                         Player* player) {
    // Cleanup NPCs that no longer exist
    CleanupRemovedNPCs(entityManager);

    // Update discovery (check if player is near any NPCs)
    UpdateDiscovery(entityManager, player);

    // Update spawning of new potential recruits
    UpdateSpawning(deltaTime, entityManager, player);

    // Update refugee waves
    UpdateRefugees(deltaTime, entityManager);

    // Update recruitment interaction status
    for (auto& [id, info] : m_potentialRecruits) {
        // Check if can recruit
        info.canRecruit = CanRecruit(id, population);

        // If actively recruiting this NPC, update progress
        if (id == m_activeRecruitmentTarget && player) {
            NPC* npc = entityManager.GetEntityAs<NPC>(id);
            if (npc) {
                float dist = glm::distance(
                    glm::vec2(player->GetPosition().x, player->GetPosition().z),
                    glm::vec2(npc->GetPosition().x, npc->GetPosition().z)
                );

                if (dist <= m_config.interactionRange) {
                    // Continue interaction
                    info.interactionProgress += deltaTime / (m_config.interactionTime * info.recruitDifficulty);

                    if (info.interactionProgress >= 1.0f) {
                        // Recruitment complete!
                        if (RecruitWorker(id, entityManager, population)) {
                            m_activeRecruitmentTarget = Entity::INVALID_ID;
                        }
                    }
                } else {
                    // Too far - cancel interaction
                    CancelRecruitmentInteraction(id);
                }
            }
        }
    }
}

// ============================================================================
// Discovery
// ============================================================================

Entity::EntityId Recruitment::DiscoverSurvivor(const glm::vec3& position, EntityManager& entityManager) {
    // Check if we're at max unrecruited survivors
    if (GetUnrecruitedCount() >= m_config.maxUnrecruitedSurvivors) {
        return Entity::INVALID_ID;
    }

    // Create an NPC at the position
    NPC* npc = entityManager.CreateEntity<NPC>();
    if (!npc) return Entity::INVALID_ID;

    npc->SetPosition(position);

    // Generate recruit info
    RecruitInfo info;
    info.npcId = npc->GetId();
    info.type = DetermineRecruitType();
    info.discovered = true;
    GenerateRecruitInfo(info, npc);

    m_potentialRecruits[npc->GetId()] = info;
    m_discoveredNPCs.insert(npc->GetId());
    m_totalDiscovered++;

    if (m_onDiscovery) {
        m_onDiscovery(info);
    }

    return npc->GetId();
}

bool Recruitment::IsDiscovered(Entity::EntityId npcId) const {
    return m_discoveredNPCs.count(npcId) > 0;
}

std::vector<RecruitInfo> Recruitment::GetDiscoveredRecruits() const {
    std::vector<RecruitInfo> result;
    for (const auto& [id, info] : m_potentialRecruits) {
        if (info.discovered) {
            result.push_back(info);
        }
    }
    return result;
}

const RecruitInfo* Recruitment::GetRecruitInfo(Entity::EntityId npcId) const {
    auto it = m_potentialRecruits.find(npcId);
    if (it != m_potentialRecruits.end()) {
        return &it->second;
    }
    return nullptr;
}

// ============================================================================
// Recruitment
// ============================================================================

bool Recruitment::RecruitWorker(Entity::EntityId npcId, EntityManager& entityManager,
                                 Population& population) {
    // Check if can recruit
    std::string reason;
    if (!CanRecruit(npcId, population, &reason)) {
        return false;
    }

    // Get the NPC
    NPC* npc = entityManager.GetEntityAs<NPC>(npcId);
    if (!npc) return false;

    auto infoIt = m_potentialRecruits.find(npcId);
    if (infoIt == m_potentialRecruits.end()) return false;

    const RecruitInfo& info = infoIt->second;

    // Create worker from NPC
    auto worker = std::make_unique<Worker>(*npc);

    // Apply type-specific bonuses
    GenerateSpecializedSkills(worker.get(), info.type, info.specialization);

    // Set name
    worker->SetWorkerName(info.name);

    // Add to population
    Worker* workerPtr = worker.get();
    if (!population.AddWorker(std::move(worker))) {
        return false;  // Failed to add (no housing?)
    }

    // Remove NPC
    entityManager.RemoveEntity(npcId);

    // Clean up tracking
    m_potentialRecruits.erase(npcId);
    m_discoveredNPCs.erase(npcId);

    // Remove from incoming refugees if applicable
    auto refugeeIt = std::find(m_incomingRefugees.begin(), m_incomingRefugees.end(), npcId);
    if (refugeeIt != m_incomingRefugees.end()) {
        m_incomingRefugees.erase(refugeeIt);
    }

    m_totalRecruited++;

    if (m_onRecruitment) {
        m_onRecruitment(*workerPtr);
    }

    return true;
}

bool Recruitment::StartRecruitmentInteraction(Entity::EntityId npcId, Player* player) {
    if (!player) return false;

    auto it = m_potentialRecruits.find(npcId);
    if (it == m_potentialRecruits.end()) return false;

    // Check range
    NPC* npc = nullptr;  // Would need entityManager here in real use
    // For now, just start the interaction
    m_activeRecruitmentTarget = npcId;
    it->second.interactionProgress = 0.0f;

    return true;
}

bool Recruitment::UpdateRecruitmentInteraction(Entity::EntityId npcId, float deltaTime, Player* player) {
    if (npcId != m_activeRecruitmentTarget) return false;
    if (!player) return false;

    auto it = m_potentialRecruits.find(npcId);
    if (it == m_potentialRecruits.end()) return false;

    // Progress is updated in main Update(), just check if complete
    return it->second.interactionProgress >= 1.0f;
}

void Recruitment::CancelRecruitmentInteraction(Entity::EntityId npcId) {
    if (npcId == m_activeRecruitmentTarget) {
        m_activeRecruitmentTarget = Entity::INVALID_ID;
    }

    auto it = m_potentialRecruits.find(npcId);
    if (it != m_potentialRecruits.end()) {
        it->second.interactionProgress = 0.0f;
    }
}

bool Recruitment::CanRecruit(Entity::EntityId npcId, const Population& population,
                              std::string* outReason) const {
    // Check if this is a potential recruit
    auto it = m_potentialRecruits.find(npcId);
    if (it == m_potentialRecruits.end()) {
        if (outReason) *outReason = "Not a potential recruit";
        return false;
    }

    // Check if discovered
    if (!it->second.discovered) {
        if (outReason) *outReason = "Survivor not yet discovered";
        return false;
    }

    // Check housing
    if (m_config.requireHousing && population.GetAvailableHousing() <= 0) {
        if (outReason) *outReason = "No available housing";
        return false;
    }

    return true;
}

// ============================================================================
// Refugee Waves
// ============================================================================

void Recruitment::TriggerRefugeeWave(int count, EntityManager& entityManager) {
    if (count <= 0) return;

    // Spawn refugees heading toward base
    for (int i = 0; i < count; ++i) {
        // Spawn at edge of map, heading toward base
        glm::vec2 direction = Nova::Random::Direction2D();
        float spawnDist = m_config.maxSpawnDistance * 1.5f;

        glm::vec3 spawnPos = m_basePosition + glm::vec3(direction.x, 0.0f, direction.y) * spawnDist;

        // Clamp to spawn bounds
        spawnPos.x = std::clamp(spawnPos.x, m_spawnMin.x, m_spawnMax.x);
        spawnPos.z = std::clamp(spawnPos.y, m_spawnMin.y, m_spawnMax.y);

        // Create NPC
        NPC* npc = entityManager.CreateEntity<NPC>();
        if (!npc) continue;

        npc->SetPosition(spawnPos);

        // Set up routine to walk toward base
        NPCRoutine routine;
        routine.AddWaypoint(m_basePosition, 0.0f, "base");
        routine.loop = false;
        npc->SetRoutine(routine);
        npc->SetState(NPCState::Wander);

        // Generate refugee info
        RecruitInfo info;
        info.npcId = npc->GetId();
        info.type = RecruitType::Refugee;
        info.discovered = false;  // Will be discovered when they arrive
        info.recruitDifficulty = 0.5f;  // Refugees are easy to recruit
        GenerateRecruitInfo(info, npc);

        m_potentialRecruits[npc->GetId()] = info;
        m_incomingRefugees.push_back(npc->GetId());
    }

    if (m_onRefugeeWave) {
        m_onRefugeeWave(count);
    }
}

std::vector<Entity::EntityId> Recruitment::GetIncomingRefugees() const {
    return m_incomingRefugees;
}

// ============================================================================
// Spawning
// ============================================================================

Entity::EntityId Recruitment::SpawnPotentialRecruit(EntityManager& entityManager,
                                                     const glm::vec3& playerPosition) {
    // Check if at max
    if (GetUnrecruitedCount() >= m_config.maxUnrecruitedSurvivors) {
        return Entity::INVALID_ID;
    }

    // Get spawn position
    glm::vec3 spawnPos = GetRandomSpawnPosition(playerPosition);

    // Create NPC
    NPC* npc = entityManager.CreateEntity<NPC>();
    if (!npc) return Entity::INVALID_ID;

    npc->SetPosition(spawnPos);

    // Give NPC a simple routine (wander in area)
    NPCRoutine routine;
    routine.AddWaypoint(spawnPos + glm::vec3(Nova::Random::Range(-5.0f, 5.0f), 0.0f, Nova::Random::Range(-5.0f, 5.0f)), 3.0f);
    routine.AddWaypoint(spawnPos + glm::vec3(Nova::Random::Range(-5.0f, 5.0f), 0.0f, Nova::Random::Range(-5.0f, 5.0f)), 3.0f);
    routine.AddWaypoint(spawnPos, 5.0f);
    routine.loop = true;
    npc->SetRoutine(routine);
    npc->SetState(NPCState::Wander);

    // Generate recruit info (but not discovered yet)
    RecruitInfo info;
    info.npcId = npc->GetId();
    info.type = DetermineRecruitType();
    info.discovered = false;
    GenerateRecruitInfo(info, npc);

    m_potentialRecruits[npc->GetId()] = info;

    return npc->GetId();
}

void Recruitment::SetSpawnBounds(const glm::vec2& min, const glm::vec2& max) {
    m_spawnMin = min;
    m_spawnMax = max;
}

// ============================================================================
// Internal Helpers
// ============================================================================

void Recruitment::UpdateDiscovery(EntityManager& entityManager, Player* player) {
    if (!player) return;

    glm::vec3 playerPos = player->GetPosition();
    float discoveryRange = m_config.interactionRange * 3.0f;  // Larger range for discovery

    for (auto& [id, info] : m_potentialRecruits) {
        if (info.discovered) continue;

        NPC* npc = entityManager.GetEntityAs<NPC>(id);
        if (!npc) continue;

        float dist = glm::distance(
            glm::vec2(playerPos.x, playerPos.z),
            glm::vec2(npc->GetPosition().x, npc->GetPosition().z)
        );

        if (dist <= discoveryRange) {
            // Discovered!
            info.discovered = true;
            m_discoveredNPCs.insert(id);
            m_totalDiscovered++;

            if (m_onDiscovery) {
                m_onDiscovery(info);
            }
        }
    }

    // Also check incoming refugees that reached the base
    for (auto it = m_incomingRefugees.begin(); it != m_incomingRefugees.end();) {
        Entity::EntityId refugeeId = *it;
        NPC* npc = entityManager.GetEntityAs<NPC>(refugeeId);

        if (!npc) {
            it = m_incomingRefugees.erase(it);
            continue;
        }

        float distToBase = glm::distance(
            glm::vec2(m_basePosition.x, m_basePosition.z),
            glm::vec2(npc->GetPosition().x, npc->GetPosition().z)
        );

        if (distToBase <= 10.0f) {
            // Refugee arrived at base - auto-discover
            auto infoIt = m_potentialRecruits.find(refugeeId);
            if (infoIt != m_potentialRecruits.end() && !infoIt->second.discovered) {
                infoIt->second.discovered = true;
                m_discoveredNPCs.insert(refugeeId);
                m_totalDiscovered++;

                if (m_onDiscovery) {
                    m_onDiscovery(infoIt->second);
                }
            }

            // Stop NPC movement
            npc->SetState(NPCState::Idle);
            npc->GetRoutine().waypoints.clear();

            it = m_incomingRefugees.erase(it);
        } else {
            ++it;
        }
    }
}

void Recruitment::UpdateSpawning(float deltaTime, EntityManager& entityManager, Player* player) {
    if (!player) return;

    m_spawnTimer += deltaTime;

    // Check for random spawn
    if (Nova::Random::Value() < m_config.baseSpawnChance * deltaTime) {
        SpawnPotentialRecruit(entityManager, player->GetPosition());
    }

    // Check for refugee wave
    if (Nova::Random::Value() < m_config.refugeeWaveChance * deltaTime) {
        int count = Nova::Random::Range(m_config.refugeeWaveMin, m_config.refugeeWaveMax);
        TriggerRefugeeWave(count, entityManager);
    }
}

void Recruitment::UpdateRefugees(float deltaTime, EntityManager& entityManager) {
    // Refugees are updated by NPC AI system, we just track them
    // Nothing special to do here beyond what UpdateDiscovery handles
}

void Recruitment::CleanupRemovedNPCs(EntityManager& entityManager) {
    // Remove entries for NPCs that no longer exist
    for (auto it = m_potentialRecruits.begin(); it != m_potentialRecruits.end();) {
        Entity* entity = entityManager.GetEntity(it->first);
        if (!entity || entity->IsMarkedForRemoval() || !entity->IsAlive()) {
            m_discoveredNPCs.erase(it->first);

            // Remove from refugees if applicable
            auto refugeeIt = std::find(m_incomingRefugees.begin(), m_incomingRefugees.end(), it->first);
            if (refugeeIt != m_incomingRefugees.end()) {
                m_incomingRefugees.erase(refugeeIt);
            }

            it = m_potentialRecruits.erase(it);
        } else {
            ++it;
        }
    }
}

RecruitType Recruitment::DetermineRecruitType() const {
    float roll = Nova::Random::Value();

    if (roll < m_config.leaderChance) {
        return RecruitType::Leader;
    }
    roll -= m_config.leaderChance;

    if (roll < m_config.specialistChance) {
        return RecruitType::Specialist;
    }
    roll -= m_config.specialistChance;

    if (roll < m_config.mercenaryChance) {
        return RecruitType::Mercenary;
    }
    roll -= m_config.mercenaryChance;

    if (roll < m_config.refugeeChance) {
        return RecruitType::Refugee;
    }
    roll -= m_config.refugeeChance;

    // Check for skilled (20% of remaining)
    if (Nova::Random::Value() < 0.2f) {
        return RecruitType::Skilled;
    }

    return RecruitType::Regular;
}

void Recruitment::GenerateRecruitInfo(RecruitInfo& info, NPC* npc) {
    // Generate name based on appearance
    static const char* firstNames[] = {
        "Alex", "Jordan", "Casey", "Riley", "Morgan", "Taylor", "Quinn", "Avery",
        "Sam", "Charlie", "Jamie", "Drew", "Pat", "Jesse", "Robin", "Kerry"
    };
    static const char* lastNames[] = {
        "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller", "Davis"
    };

    int firstIdx = Nova::Random::Range(0, static_cast<int>(sizeof(firstNames) / sizeof(char*)) - 1);
    int lastIdx = Nova::Random::Range(0, static_cast<int>(sizeof(lastNames) / sizeof(char*)) - 1);
    info.name = std::string(firstNames[firstIdx]) + " " + lastNames[lastIdx];

    // Generate preview skills (for UI display before recruiting)
    WorkerSkills previewSkills;
    previewSkills.gathering = Nova::Random::Range(5.0f, 25.0f);
    previewSkills.building = Nova::Random::Range(5.0f, 25.0f);
    previewSkills.farming = Nova::Random::Range(5.0f, 25.0f);
    previewSkills.combat = Nova::Random::Range(5.0f, 25.0f);
    previewSkills.crafting = Nova::Random::Range(5.0f, 25.0f);
    previewSkills.medical = Nova::Random::Range(5.0f, 25.0f);
    previewSkills.scouting = Nova::Random::Range(5.0f, 25.0f);
    previewSkills.trading = Nova::Random::Range(5.0f, 25.0f);

    // Modify based on type
    switch (info.type) {
        case RecruitType::Skilled:
            // One skill is notably higher
            {
                int skillIdx = Nova::Random::Range(0, 7);
                float* skills[] = {
                    &previewSkills.gathering, &previewSkills.building,
                    &previewSkills.farming, &previewSkills.combat,
                    &previewSkills.crafting, &previewSkills.medical,
                    &previewSkills.scouting, &previewSkills.trading
                };
                *skills[skillIdx] = Nova::Random::Range(40.0f, 60.0f);
            }
            info.recruitDifficulty = 1.2f;
            break;

        case RecruitType::Refugee:
            // Lower skills, easier to recruit
            previewSkills.gathering *= 0.6f;
            previewSkills.building *= 0.6f;
            previewSkills.farming *= 0.6f;
            previewSkills.combat *= 0.4f;
            previewSkills.crafting *= 0.6f;
            previewSkills.medical *= 0.6f;
            previewSkills.scouting *= 0.6f;
            previewSkills.trading *= 0.6f;
            info.recruitDifficulty = 0.5f;
            break;

        case RecruitType::Mercenary:
            // High combat, expensive
            previewSkills.combat = Nova::Random::Range(50.0f, 70.0f);
            info.recruitDifficulty = 2.0f;
            break;

        case RecruitType::Specialist:
            // Very high in one skill
            {
                int skillIdx = Nova::Random::Range(0, 7);
                float* skills[] = {
                    &previewSkills.gathering, &previewSkills.building,
                    &previewSkills.farming, &previewSkills.combat,
                    &previewSkills.crafting, &previewSkills.medical,
                    &previewSkills.scouting, &previewSkills.trading
                };
                *skills[skillIdx] = Nova::Random::Range(70.0f, 90.0f);
            }
            info.recruitDifficulty = 2.5f;
            break;

        case RecruitType::Leader:
            // Balanced but with leadership bonus
            previewSkills.gathering = Nova::Random::Range(30.0f, 50.0f);
            previewSkills.building = Nova::Random::Range(30.0f, 50.0f);
            previewSkills.farming = Nova::Random::Range(30.0f, 50.0f);
            previewSkills.combat = Nova::Random::Range(30.0f, 50.0f);
            previewSkills.crafting = Nova::Random::Range(30.0f, 50.0f);
            previewSkills.medical = Nova::Random::Range(30.0f, 50.0f);
            previewSkills.scouting = Nova::Random::Range(30.0f, 50.0f);
            previewSkills.trading = Nova::Random::Range(30.0f, 50.0f);
            info.recruitDifficulty = 3.0f;
            break;

        default:
            info.recruitDifficulty = 1.0f;
            break;
    }

    // Find best skill
    info.bestSkillName = GetBestSkillName(previewSkills, info.specialization);
    info.bestSkillLevel = [&]() {
        switch (info.specialization) {
            case WorkerJob::Gatherer:  return previewSkills.gathering;
            case WorkerJob::Builder:   return previewSkills.building;
            case WorkerJob::Farmer:    return previewSkills.farming;
            case WorkerJob::Guard:     return previewSkills.combat;
            case WorkerJob::Crafter:   return previewSkills.crafting;
            case WorkerJob::Medic:     return previewSkills.medical;
            case WorkerJob::Scout:     return previewSkills.scouting;
            case WorkerJob::Trader:    return previewSkills.trading;
            default:                   return 10.0f;
        }
    }();

    // Health preview
    info.healthPreview = npc ? npc->GetHealth() : 100.0f;

    // Generate personality hint
    WorkerPersonality personality = WorkerPersonality::GenerateRandom();
    info.personalityHint = GeneratePersonalityHint(personality);
}

void Recruitment::GenerateSpecializedSkills(Worker* worker, RecruitType type,
                                             WorkerJob specialization) {
    if (!worker) return;

    WorkerSkills& skills = worker->GetSkills();

    switch (type) {
        case RecruitType::Skilled:
            // Boost specialized skill
            switch (specialization) {
                case WorkerJob::Gatherer:  skills.gathering = Nova::Random::Range(40.0f, 60.0f); break;
                case WorkerJob::Builder:   skills.building = Nova::Random::Range(40.0f, 60.0f); break;
                case WorkerJob::Farmer:    skills.farming = Nova::Random::Range(40.0f, 60.0f); break;
                case WorkerJob::Guard:     skills.combat = Nova::Random::Range(40.0f, 60.0f); break;
                case WorkerJob::Crafter:   skills.crafting = Nova::Random::Range(40.0f, 60.0f); break;
                case WorkerJob::Medic:     skills.medical = Nova::Random::Range(40.0f, 60.0f); break;
                case WorkerJob::Scout:     skills.scouting = Nova::Random::Range(40.0f, 60.0f); break;
                case WorkerJob::Trader:    skills.trading = Nova::Random::Range(40.0f, 60.0f); break;
                default: break;
            }
            break;

        case RecruitType::Refugee:
            // Lower all skills
            skills.gathering *= 0.6f;
            skills.building *= 0.6f;
            skills.farming *= 0.6f;
            skills.combat *= 0.4f;
            skills.crafting *= 0.6f;
            skills.medical *= 0.6f;
            skills.scouting *= 0.6f;
            skills.trading *= 0.6f;
            // Higher loyalty (grateful)
            worker->SetLoyalty(Nova::Random::Range(60.0f, 80.0f));
            break;

        case RecruitType::Mercenary:
            skills.combat = Nova::Random::Range(50.0f, 70.0f);
            // Lower loyalty
            worker->SetLoyalty(Nova::Random::Range(20.0f, 40.0f));
            break;

        case RecruitType::Specialist:
            // Very high specialized skill
            switch (specialization) {
                case WorkerJob::Gatherer:  skills.gathering = Nova::Random::Range(70.0f, 90.0f); break;
                case WorkerJob::Builder:   skills.building = Nova::Random::Range(70.0f, 90.0f); break;
                case WorkerJob::Farmer:    skills.farming = Nova::Random::Range(70.0f, 90.0f); break;
                case WorkerJob::Guard:     skills.combat = Nova::Random::Range(70.0f, 90.0f); break;
                case WorkerJob::Crafter:   skills.crafting = Nova::Random::Range(70.0f, 90.0f); break;
                case WorkerJob::Medic:     skills.medical = Nova::Random::Range(70.0f, 90.0f); break;
                case WorkerJob::Scout:     skills.scouting = Nova::Random::Range(70.0f, 90.0f); break;
                case WorkerJob::Trader:    skills.trading = Nova::Random::Range(70.0f, 90.0f); break;
                default: break;
            }
            break;

        case RecruitType::Leader:
            // Balanced skills
            skills.gathering = Nova::Random::Range(30.0f, 50.0f);
            skills.building = Nova::Random::Range(30.0f, 50.0f);
            skills.farming = Nova::Random::Range(30.0f, 50.0f);
            skills.combat = Nova::Random::Range(30.0f, 50.0f);
            skills.crafting = Nova::Random::Range(30.0f, 50.0f);
            skills.medical = Nova::Random::Range(30.0f, 50.0f);
            skills.scouting = Nova::Random::Range(30.0f, 50.0f);
            skills.trading = Nova::Random::Range(30.0f, 50.0f);
            // High loyalty
            worker->SetLoyalty(Nova::Random::Range(70.0f, 90.0f));
            // Positive personality traits
            WorkerPersonality p = worker->GetPersonality();
            p.optimism = Nova::Random::Range(0.3f, 1.0f);
            p.sociability = Nova::Random::Range(0.3f, 1.0f);
            worker->SetPersonality(p);
            break;
    }
}

glm::vec3 Recruitment::GetRandomSpawnPosition(const glm::vec3& playerPosition) const {
    // Spawn at random distance from player
    float distance = Nova::Random::Range(m_config.minSpawnDistance, m_config.maxSpawnDistance);
    glm::vec2 direction = Nova::Random::Direction2D();

    glm::vec3 pos = playerPosition + glm::vec3(direction.x, 0.0f, direction.y) * distance;

    // Clamp to bounds
    pos.x = std::clamp(pos.x, m_spawnMin.x, m_spawnMax.x);
    pos.z = std::clamp(pos.z, m_spawnMin.y, m_spawnMax.y);

    return pos;
}

std::string Recruitment::GeneratePersonalityHint(const WorkerPersonality& personality) const {
    // Generate a hint based on dominant traits
    std::string hint;

    if (personality.bravery > 0.5f) {
        hint = "Seems brave and fearless";
    } else if (personality.bravery < -0.5f) {
        hint = "Appears cautious and careful";
    } else if (personality.diligence > 0.5f) {
        hint = "Looks hardworking and dedicated";
    } else if (personality.diligence < -0.5f) {
        hint = "Seems to prefer taking it easy";
    } else if (personality.sociability > 0.5f) {
        hint = "Has a friendly, outgoing demeanor";
    } else if (personality.sociability < -0.5f) {
        hint = "Appears to prefer solitude";
    } else if (personality.optimism > 0.5f) {
        hint = "Has a cheerful outlook";
    } else if (personality.optimism < -0.5f) {
        hint = "Seems pessimistic about things";
    } else if (personality.loyalty > 0.5f) {
        hint = "Appears loyal and trustworthy";
    } else if (personality.loyalty < -0.5f) {
        hint = "Looks out for themselves first";
    } else {
        // Default to random hint
        int idx = Nova::Random::Range(0, static_cast<int>(sizeof(s_personalityHints) / sizeof(char*)) - 1);
        hint = s_personalityHints[idx];
    }

    return hint;
}

std::string Recruitment::GetBestSkillName(const WorkerSkills& skills, WorkerJob& outJob) const {
    struct SkillInfo {
        float value;
        WorkerJob job;
        const char* name;
    };

    SkillInfo skillInfos[] = {
        {skills.gathering, WorkerJob::Gatherer, "Gathering"},
        {skills.building, WorkerJob::Builder, "Building"},
        {skills.farming, WorkerJob::Farmer, "Farming"},
        {skills.combat, WorkerJob::Guard, "Combat"},
        {skills.crafting, WorkerJob::Crafter, "Crafting"},
        {skills.medical, WorkerJob::Medic, "Medical"},
        {skills.scouting, WorkerJob::Scout, "Scouting"},
        {skills.trading, WorkerJob::Trader, "Trading"}
    };

    // Find highest skill
    SkillInfo* best = &skillInfos[0];
    for (int i = 1; i < 8; ++i) {
        if (skillInfos[i].value > best->value) {
            best = &skillInfos[i];
        }
    }

    outJob = best->job;
    return best->name;
}

int Recruitment::GetUnrecruitedCount() const noexcept {
    return static_cast<int>(m_potentialRecruits.size());
}

} // namespace Vehement
