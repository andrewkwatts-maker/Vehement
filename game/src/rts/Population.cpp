#include "Population.hpp"
#include "../entities/EntityManager.hpp"
#include <engine/pathfinding/Graph.hpp>
#include <engine/math/Random.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// Construction
// ============================================================================

Population::Population() {
    // Initialize morale factors to neutral
    m_moraleFactors.foodQuality = 0.0f;
    m_moraleFactors.housingQuality = 0.0f;
    m_moraleFactors.safety = 5.0f;  // Start with some safety bonus
    m_moraleFactors.overwork = 0.0f;
    m_moraleFactors.leadership = 0.0f;
}

Population::~Population() = default;

// ============================================================================
// Core Update
// ============================================================================

void Population::Update(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    m_totalTime += deltaTime;

    // Update all workers
    UpdateWorkers(deltaTime, entityManager, navGraph);

    // Cleanup dead/deserted workers
    CleanupWorkers();

    // Distribute food
    DistributeFood(deltaTime);

    // Update morale factors periodically
    m_moraleUpdateTimer -= deltaTime;
    if (m_moraleUpdateTimer <= 0.0f) {
        UpdateMoraleFactors(MORALE_UPDATE_INTERVAL);
        m_moraleUpdateTimer = MORALE_UPDATE_INTERVAL;
    }

    // Check for population growth
    if (m_growthEnabled) {
        m_growthCheckTimer -= deltaTime;
        if (m_growthCheckTimer <= 0.0f) {
            UpdateGrowthConditions();
            CheckPopulationGrowth(GROWTH_CHECK_INTERVAL);
            m_growthCheckTimer = GROWTH_CHECK_INTERVAL;
        }
    }

    // Update statistics
    UpdateStatistics();
}

void Population::UpdateWorkers(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    for (auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval()) {
            // Set up callbacks if not already set
            if (!worker->GetDeathCallback()) {
                worker->SetDeathCallback([this](Worker& w) {
                    if (m_onWorkerDeath) {
                        m_onWorkerDeath(w);
                    }
                    m_stats.deathsToday++;

                    // Remove from housing
                    if (w.HasHome()) {
                        auto it = m_housing.find(w.GetHomeId());
                        if (it != m_housing.end()) {
                            auto& residents = it->second.residents;
                            residents.erase(
                                std::remove(residents.begin(), residents.end(), w.GetId()),
                                residents.end()
                            );
                            it->second.occupancy--;
                        }
                    }

                    // Remove from workplace
                    if (w.GetWorkplaceId() != 0) {
                        auto it = m_workplaces.find(w.GetWorkplaceId());
                        if (it != m_workplaces.end()) {
                            auto& workers = it->second.workers;
                            workers.erase(
                                std::remove(workers.begin(), workers.end(), w.GetId()),
                                workers.end()
                            );
                            it->second.currentWorkers--;
                        }
                    }
                });
            }

            if (!worker->GetDesertionCallback()) {
                worker->SetDesertionCallback([this](Worker& w) {
                    if (m_onWorkerDesertion) {
                        m_onWorkerDesertion(w);
                    }
                    m_stats.desertionsToday++;

                    // Apply morale penalty to other workers
                    ApplyMoraleEvent(-5.0f, "A worker deserted the settlement");
                });
            }

            // Update the worker
            worker->Update(deltaTime);
            worker->UpdateAI(deltaTime, entityManager, navGraph);
        }
    }
}

// ============================================================================
// Worker Management
// ============================================================================

bool Population::AddWorker(std::unique_ptr<Worker> worker) {
    if (!worker) return false;

    // Check housing capacity
    if (GetAvailableHousing() <= 0) {
        return false;  // No room
    }

    Entity::EntityId id = worker->GetId();

    // Try to find housing for the worker
    if (!FindAndAssignHousing(worker.get())) {
        return false;  // No housing available
    }

    // Add to workers list
    m_workerIndex[id] = m_workers.size();
    m_workers.push_back(std::move(worker));

    m_stats.recruitsToday++;

    // Boost morale for new recruit
    ApplyMoraleEvent(2.0f, "A new survivor joined the settlement");

    return true;
}

bool Population::RemoveWorker(Entity::EntityId workerId) {
    auto indexIt = m_workerIndex.find(workerId);
    if (indexIt == m_workerIndex.end()) {
        return false;
    }

    size_t index = indexIt->second;
    Worker* worker = m_workers[index].get();

    // Remove from housing
    if (worker->HasHome()) {
        auto housingIt = m_housing.find(worker->GetHomeId());
        if (housingIt != m_housing.end()) {
            auto& residents = housingIt->second.residents;
            residents.erase(
                std::remove(residents.begin(), residents.end(), workerId),
                residents.end()
            );
            housingIt->second.occupancy--;
        }
    }

    // Remove from workplace
    if (worker->GetWorkplaceId() != 0) {
        auto workplaceIt = m_workplaces.find(worker->GetWorkplaceId());
        if (workplaceIt != m_workplaces.end()) {
            auto& workers = workplaceIt->second.workers;
            workers.erase(
                std::remove(workers.begin(), workers.end(), workerId),
                workers.end()
            );
            workplaceIt->second.currentWorkers--;
        }
    }

    // Swap with last and pop
    if (index != m_workers.size() - 1) {
        m_workers[index] = std::move(m_workers.back());
        m_workerIndex[m_workers[index]->GetId()] = index;
    }
    m_workers.pop_back();
    m_workerIndex.erase(workerId);

    return true;
}

Worker* Population::GetWorker(Entity::EntityId workerId) {
    auto it = m_workerIndex.find(workerId);
    if (it == m_workerIndex.end()) return nullptr;
    return m_workers[it->second].get();
}

const Worker* Population::GetWorker(Entity::EntityId workerId) const {
    auto it = m_workerIndex.find(workerId);
    if (it == m_workerIndex.end()) return nullptr;
    return m_workers[it->second].get();
}

void Population::CleanupWorkers() {
    // Remove workers marked for removal
    for (auto it = m_workers.begin(); it != m_workers.end();) {
        if ((*it)->IsMarkedForRemoval()) {
            Entity::EntityId id = (*it)->GetId();

            // Update index map
            m_workerIndex.erase(id);

            // If not last element, swap with last
            if (it != m_workers.end() - 1) {
                *it = std::move(m_workers.back());
                m_workerIndex[(*it)->GetId()] = std::distance(m_workers.begin(), it);
            }
            m_workers.pop_back();
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Housing
// ============================================================================

void Population::RegisterHousing(uint32_t buildingId, int capacity, const glm::vec3& position) {
    HousingInfo info;
    info.buildingId = buildingId;
    info.capacity = capacity;
    info.occupancy = 0;
    info.position = position;

    m_housing[buildingId] = info;
    m_housingCapacity += capacity;
}

void Population::UnregisterHousing(uint32_t buildingId) {
    auto it = m_housing.find(buildingId);
    if (it == m_housing.end()) return;

    // Evict residents
    for (Entity::EntityId residentId : it->second.residents) {
        Worker* worker = GetWorker(residentId);
        if (worker) {
            worker->SetHome(0);
            worker->SetHomePosition(glm::vec3(0.0f));

            // Try to find new housing
            FindAndAssignHousing(worker);
        }
    }

    m_housingCapacity -= it->second.capacity;
    m_housing.erase(it);
}

int Population::GetAvailableHousing() const noexcept {
    int available = 0;
    for (const auto& [id, info] : m_housing) {
        available += info.capacity - info.occupancy;
    }
    return available;
}

bool Population::AssignHousing(Worker* worker, uint32_t buildingId) {
    if (!worker) return false;

    auto it = m_housing.find(buildingId);
    if (it == m_housing.end()) return false;

    HousingInfo& housing = it->second;

    // Check capacity
    if (housing.occupancy >= housing.capacity) {
        return false;
    }

    // Remove from old housing if any
    if (worker->HasHome()) {
        auto oldHousing = m_housing.find(worker->GetHomeId());
        if (oldHousing != m_housing.end()) {
            auto& residents = oldHousing->second.residents;
            residents.erase(
                std::remove(residents.begin(), residents.end(), worker->GetId()),
                residents.end()
            );
            oldHousing->second.occupancy--;
        }
    }

    // Assign new housing
    housing.residents.push_back(worker->GetId());
    housing.occupancy++;
    worker->SetHome(buildingId);
    worker->SetHomePosition(housing.position);

    return true;
}

bool Population::FindAndAssignHousing(Worker* worker) {
    if (!worker) return false;

    // Find housing with available space
    for (auto& [id, housing] : m_housing) {
        if (housing.occupancy < housing.capacity) {
            return AssignHousing(worker, id);
        }
    }

    return false;  // No housing available
}

// ============================================================================
// Workplace Assignment
// ============================================================================

void Population::RegisterWorkplace(uint32_t buildingId, WorkerJob jobType, int maxWorkers,
                                   const glm::vec3& position) {
    WorkplaceInfo info;
    info.buildingId = buildingId;
    info.jobType = jobType;
    info.maxWorkers = maxWorkers;
    info.currentWorkers = 0;
    info.position = position;

    m_workplaces[buildingId] = info;
}

void Population::UnregisterWorkplace(uint32_t buildingId) {
    auto it = m_workplaces.find(buildingId);
    if (it == m_workplaces.end()) return;

    // Unassign workers
    for (Entity::EntityId workerId : it->second.workers) {
        Worker* worker = GetWorker(workerId);
        if (worker) {
            worker->ClearJobAssignment();
        }
    }

    m_workplaces.erase(it);
}

bool Population::AssignJob(Worker* worker, WorkerJob job, uint32_t buildingId) {
    if (!worker) return false;

    auto it = m_workplaces.find(buildingId);
    if (it == m_workplaces.end()) return false;

    WorkplaceInfo& workplace = it->second;

    // Check job type matches
    if (workplace.jobType != job) {
        return false;
    }

    // Check capacity
    if (workplace.currentWorkers >= workplace.maxWorkers) {
        return false;
    }

    // Unassign from old job if any
    UnassignWorker(worker);

    // Assign new job
    workplace.workers.push_back(worker->GetId());
    workplace.currentWorkers++;

    worker->SetJob(job);
    worker->SetWorkplace(buildingId);
    worker->SetWorkplacePosition(workplace.position);

    return true;
}

void Population::UnassignWorker(Worker* worker) {
    if (!worker) return;

    uint32_t oldWorkplace = worker->GetWorkplaceId();
    if (oldWorkplace != 0) {
        auto it = m_workplaces.find(oldWorkplace);
        if (it != m_workplaces.end()) {
            auto& workers = it->second.workers;
            workers.erase(
                std::remove(workers.begin(), workers.end(), worker->GetId()),
                workers.end()
            );
            it->second.currentWorkers--;
        }
    }

    worker->ClearJobAssignment();
}

std::vector<Worker*> Population::GetWorkersAtBuilding(uint32_t buildingId) const {
    std::vector<Worker*> result;

    // Check workplaces
    auto workplaceIt = m_workplaces.find(buildingId);
    if (workplaceIt != m_workplaces.end()) {
        for (Entity::EntityId id : workplaceIt->second.workers) {
            auto indexIt = m_workerIndex.find(id);
            if (indexIt != m_workerIndex.end()) {
                result.push_back(m_workers[indexIt->second].get());
            }
        }
    }

    // Check housing
    auto housingIt = m_housing.find(buildingId);
    if (housingIt != m_housing.end()) {
        for (Entity::EntityId id : housingIt->second.residents) {
            auto indexIt = m_workerIndex.find(id);
            if (indexIt != m_workerIndex.end()) {
                result.push_back(m_workers[indexIt->second].get());
            }
        }
    }

    return result;
}

// ============================================================================
// Food System
// ============================================================================

float Population::GetDailyFoodConsumption() const noexcept {
    return static_cast<float>(m_workers.size()) * FOOD_PER_WORKER_PER_DAY;
}

float Population::GetDaysOfFoodRemaining() const noexcept {
    float dailyConsumption = GetDailyFoodConsumption();
    if (dailyConsumption <= 0.0f) return 999.0f;  // No workers = infinite food
    return m_foodStorage / dailyConsumption;
}

void Population::DistributeFood(float deltaTime) {
    m_foodDistributionTimer += deltaTime;

    // Distribute food every in-game day
    if (m_foodDistributionTimer >= DAY_DURATION) {
        m_foodDistributionTimer = 0.0f;

        float dailyConsumption = GetDailyFoodConsumption();

        if (m_foodStorage >= dailyConsumption) {
            // Enough food for everyone
            m_foodStorage -= dailyConsumption;

            for (auto& worker : m_workers) {
                if (worker && !worker->IsMarkedForRemoval()) {
                    // Feed worker
                    worker->Feed(30.0f);  // Restore hunger

                    // Good food boosts morale
                    if (m_moraleFactors.foodQuality > 0.0f) {
                        worker->GetNeeds().ModifyMorale(m_moraleFactors.foodQuality);
                    }
                }
            }

            // Update food quality morale
            m_moraleFactors.foodQuality = std::min(5.0f, m_moraleFactors.foodQuality + 0.5f);
        } else {
            // Not enough food - ration it
            float ratio = m_foodStorage / dailyConsumption;
            m_foodStorage = 0.0f;

            for (auto& worker : m_workers) {
                if (worker && !worker->IsMarkedForRemoval()) {
                    // Partial feeding based on available food
                    worker->Feed(30.0f * ratio);

                    // Hunger reduces morale
                    if (ratio < 0.5f) {
                        worker->GetNeeds().ModifyMorale(-10.0f);
                    }
                }
            }

            // Food shortage reduces morale factor
            m_moraleFactors.foodQuality = std::max(-10.0f, m_moraleFactors.foodQuality - 2.0f);

            // Apply global morale event for food shortage
            ApplyMoraleEvent(-5.0f, "Food shortage - workers are hungry");
        }

        // Reset daily stats
        m_stats.deathsToday = 0;
        m_stats.desertionsToday = 0;
        m_stats.recruitsToday = 0;
    }
}

// ============================================================================
// Morale System
// ============================================================================

void Population::SetMoraleFactor(const std::string& factor, float value) {
    if (factor == "food" || factor == "foodQuality") {
        m_moraleFactors.foodQuality = value;
    } else if (factor == "housing" || factor == "housingQuality") {
        m_moraleFactors.housingQuality = value;
    } else if (factor == "safety") {
        m_moraleFactors.safety = value;
    } else if (factor == "overwork") {
        m_moraleFactors.overwork = value;
    } else if (factor == "leadership") {
        m_moraleFactors.leadership = value;
    }
}

void Population::ApplyMoraleEvent(float amount, const std::string& /*reason*/) {
    for (auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval()) {
            // Apply event with personality modifier
            float modifier = worker->GetPersonality().GetMoraleRecoveryModifier();
            float effectiveAmount = amount;

            // Optimists are less affected by negative events, more by positive
            if (amount < 0.0f) {
                effectiveAmount *= (2.0f - modifier);  // Optimists less affected
            } else {
                effectiveAmount *= modifier;  // Optimists more affected
            }

            worker->GetNeeds().ModifyMorale(effectiveAmount);
        }
    }
}

void Population::RecordAttack() {
    m_lastAttackTime = m_totalTime;

    // Reduce safety morale
    m_moraleFactors.safety = std::max(-10.0f, m_moraleFactors.safety - 3.0f);

    // Apply immediate morale impact
    ApplyMoraleEvent(-3.0f, "Settlement under attack!");
}

void Population::UpdateMoraleFactors(float deltaTime) {
    // Safety recovers over time (30 seconds = full recovery of 1 point)
    float timeSinceAttack = m_totalTime - m_lastAttackTime;
    if (timeSinceAttack > 30.0f) {
        m_moraleFactors.safety = std::min(10.0f, m_moraleFactors.safety + 0.5f * (deltaTime / 30.0f));
    }

    // Housing quality based on crowding
    int totalCapacity = m_housingCapacity;
    int totalOccupancy = static_cast<int>(m_workers.size());

    if (totalCapacity > 0) {
        float crowdingRatio = static_cast<float>(totalOccupancy) / static_cast<float>(totalCapacity);
        if (crowdingRatio > 0.9f) {
            m_moraleFactors.housingQuality = -5.0f;  // Overcrowded
        } else if (crowdingRatio > 0.75f) {
            m_moraleFactors.housingQuality = -2.0f;  // Cramped
        } else if (crowdingRatio < 0.5f) {
            m_moraleFactors.housingQuality = 3.0f;   // Spacious
        } else {
            m_moraleFactors.housingQuality = 0.0f;   // Normal
        }
    }

    // Overwork penalty based on how many workers are working vs resting
    int workingCount = 0;
    int restingCount = 0;
    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval()) {
            if (worker->GetWorkerState() == WorkerState::Working) {
                workingCount++;
            } else if (worker->GetWorkerState() == WorkerState::Resting) {
                restingCount++;
            }
        }
    }

    if (m_workers.size() > 0) {
        float workRatio = static_cast<float>(workingCount) / static_cast<float>(m_workers.size());
        if (workRatio > 0.8f && restingCount < static_cast<int>(m_workers.size()) * 0.1f) {
            m_moraleFactors.overwork = -5.0f;  // Everyone working, no one resting
        } else if (workRatio > 0.6f) {
            m_moraleFactors.overwork = -2.0f;
        } else {
            m_moraleFactors.overwork = 0.0f;
        }
    }

    // Apply morale factors to workers
    float totalModifier = m_moraleFactors.GetTotal();
    if (std::abs(totalModifier) > 0.1f) {
        for (auto& worker : m_workers) {
            if (worker && !worker->IsMarkedForRemoval()) {
                // Apply small incremental change based on factors
                worker->GetNeeds().ModifyMorale(totalModifier * 0.01f * deltaTime);
            }
        }
    }
}

// ============================================================================
// Population Growth
// ============================================================================

void Population::UpdateGrowthConditions() {
    // Check food excess
    float daysOfFood = GetDaysOfFoodRemaining();
    m_growthConditions.hasExcessFood = daysOfFood > 3.0f;

    // Check housing excess
    m_growthConditions.hasExcessHousing = GetAvailableHousing() > 0;

    // Check morale
    float totalMorale = 0.0f;
    int count = 0;
    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval()) {
            totalMorale += worker->GetNeeds().morale;
            count++;
        }
    }
    m_growthConditions.isHighMorale = (count > 0 && totalMorale / count > 70.0f);

    // Check safety (no recent attacks)
    m_growthConditions.isSafe = (m_totalTime - m_lastAttackTime > 60.0f);
}

void Population::CheckPopulationGrowth(float /*deltaTime*/) {
    if (!m_growthConditions.CanGrow()) return;

    // Random chance for a new survivor to appear
    if (Nova::Random::Value() < BASE_GROWTH_CHANCE) {
        // Find a housing building to spawn near
        glm::vec3 spawnPosition{0.0f};
        bool foundSpawn = false;

        for (const auto& [id, housing] : m_housing) {
            if (housing.occupancy < housing.capacity) {
                spawnPosition = housing.position;
                // Offset slightly
                spawnPosition.x += Nova::Random::Range(-3.0f, 3.0f);
                spawnPosition.z += Nova::Random::Range(-3.0f, 3.0f);
                foundSpawn = true;
                break;
            }
        }

        if (foundSpawn && m_onPopulationGrowth) {
            m_onPopulationGrowth(spawnPosition);
        }
    }
}

// ============================================================================
// Queries
// ============================================================================

std::vector<Worker*> Population::GetIdleWorkers() const {
    std::vector<Worker*> result;
    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval() && worker->IsAvailable()) {
            result.push_back(worker.get());
        }
    }
    return result;
}

std::vector<Worker*> Population::GetWorkersByJob(WorkerJob job) const {
    std::vector<Worker*> result;
    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval() && worker->GetJob() == job) {
            result.push_back(worker.get());
        }
    }
    return result;
}

std::vector<Worker*> Population::GetWorkersByState(WorkerState state) const {
    std::vector<Worker*> result;
    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval() && worker->GetWorkerState() == state) {
            result.push_back(worker.get());
        }
    }
    return result;
}

std::vector<Worker*> Population::GetWorkersInRadius(const glm::vec3& position, float radius) const {
    std::vector<Worker*> result;
    float radiusSq = radius * radius;

    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval()) {
            glm::vec3 diff = worker->GetPosition() - position;
            if (glm::dot(diff, diff) <= radiusSq) {
                result.push_back(worker.get());
            }
        }
    }
    return result;
}

Worker* Population::GetNearestIdleWorker(const glm::vec3& position) const {
    Worker* nearest = nullptr;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval() && worker->IsAvailable()) {
            glm::vec3 diff = worker->GetPosition() - position;
            float distSq = glm::dot(diff, diff);
            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearest = worker.get();
            }
        }
    }
    return nearest;
}

Worker* Population::GetBestWorkerForJob(WorkerJob job) const {
    Worker* best = nullptr;
    float bestSkill = -1.0f;

    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval() && worker->IsAvailable()) {
            float skillLevel = 0.0f;
            switch (job) {
                case WorkerJob::Gatherer:  skillLevel = worker->GetSkills().gathering; break;
                case WorkerJob::Builder:   skillLevel = worker->GetSkills().building; break;
                case WorkerJob::Farmer:    skillLevel = worker->GetSkills().farming; break;
                case WorkerJob::Guard:     skillLevel = worker->GetSkills().combat; break;
                case WorkerJob::Crafter:   skillLevel = worker->GetSkills().crafting; break;
                case WorkerJob::Medic:     skillLevel = worker->GetSkills().medical; break;
                case WorkerJob::Scout:     skillLevel = worker->GetSkills().scouting; break;
                case WorkerJob::Trader:    skillLevel = worker->GetSkills().trading; break;
                default: break;
            }

            if (skillLevel > bestSkill) {
                bestSkill = skillLevel;
                best = worker.get();
            }
        }
    }
    return best;
}

// ============================================================================
// Selection
// ============================================================================

void Population::SelectWorkersInArea(const glm::vec2& min, const glm::vec2& max) {
    for (auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval()) {
            glm::vec2 pos = worker->GetPosition2D();
            bool inArea = pos.x >= min.x && pos.x <= max.x &&
                         pos.y >= min.y && pos.y <= max.y;
            worker->SetSelected(inArea);
        }
    }
}

std::vector<Worker*> Population::GetSelectedWorkers() const {
    std::vector<Worker*> result;
    for (const auto& worker : m_workers) {
        if (worker && !worker->IsMarkedForRemoval() && worker->IsSelected()) {
            result.push_back(worker.get());
        }
    }
    return result;
}

void Population::ClearSelection() {
    for (auto& worker : m_workers) {
        if (worker) {
            worker->SetSelected(false);
        }
    }
}

void Population::CommandSelectedMoveTo(const glm::vec3& position, Nova::Graph* navGraph) {
    // Get selected workers
    std::vector<Worker*> selected = GetSelectedWorkers();
    if (selected.empty()) return;

    // Formation: spread workers around target position
    float spacing = 1.5f;
    int gridSize = static_cast<int>(std::ceil(std::sqrt(selected.size())));

    for (size_t i = 0; i < selected.size(); ++i) {
        Worker* worker = selected[i];

        // Calculate offset in grid
        int row = static_cast<int>(i) / gridSize;
        int col = static_cast<int>(i) % gridSize;

        float offsetX = (col - gridSize / 2.0f) * spacing;
        float offsetZ = (row - gridSize / 2.0f) * spacing;

        glm::vec3 targetPos = position + glm::vec3(offsetX, 0.0f, offsetZ);

        // Stop following hero
        worker->SetFollowingHero(false);

        // Move to position
        worker->MoveTo(targetPos, navGraph);
    }
}

void Population::CommandSelectedFollowHero(bool follow) {
    std::vector<Worker*> selected = GetSelectedWorkers();
    for (Worker* worker : selected) {
        worker->SetFollowingHero(follow);
        if (!follow) {
            // Stop and go idle
            worker->ClearTask();
            worker->ClearPath();
            worker->SetWorkerState(WorkerState::Idle);
        }
    }
}

// ============================================================================
// Statistics
// ============================================================================

void Population::UpdateStatistics() {
    m_stats.totalWorkers = static_cast<int>(m_workers.size());
    m_stats.idleWorkers = 0;
    m_stats.workingWorkers = 0;
    m_stats.restingWorkers = 0;
    m_stats.injuredWorkers = 0;
    m_stats.fleeingWorkers = 0;
    m_stats.housingCapacity = m_housingCapacity;
    m_stats.availableHousing = GetAvailableHousing();
    m_stats.totalFoodConsumption = GetDailyFoodConsumption();

    float totalProductivity = 0.0f;
    float totalMorale = 0.0f;
    float totalHealth = 0.0f;
    float totalLoyalty = 0.0f;
    int activeCount = 0;

    m_stats.workersByJob.clear();

    for (const auto& worker : m_workers) {
        if (!worker || worker->IsMarkedForRemoval()) continue;

        // Count by state
        switch (worker->GetWorkerState()) {
            case WorkerState::Idle:    m_stats.idleWorkers++; break;
            case WorkerState::Working: m_stats.workingWorkers++; break;
            case WorkerState::Resting: m_stats.restingWorkers++; break;
            case WorkerState::Injured: m_stats.injuredWorkers++; break;
            case WorkerState::Fleeing: m_stats.fleeingWorkers++; break;
            default: break;
        }

        // Count by job
        m_stats.workersByJob[worker->GetJob()]++;

        // Accumulate averages
        totalProductivity += worker->GetProductivity();
        totalMorale += worker->GetNeeds().morale;
        totalHealth += worker->GetNeeds().health;
        totalLoyalty += worker->GetLoyalty();
        activeCount++;
    }

    // Calculate averages
    if (activeCount > 0) {
        m_stats.averageProductivity = totalProductivity / activeCount;
        m_stats.averageMorale = totalMorale / activeCount;
        m_stats.averageHealth = totalHealth / activeCount;
        m_stats.averageLoyalty = totalLoyalty / activeCount;
    } else {
        m_stats.averageProductivity = 0.0f;
        m_stats.averageMorale = 0.0f;
        m_stats.averageHealth = 0.0f;
        m_stats.averageLoyalty = 0.0f;
    }
}

} // namespace Vehement
