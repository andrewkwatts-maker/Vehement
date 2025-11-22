#include "OfflineSimulation.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

// Logging macros
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define SIM_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define SIM_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#else
#include <iostream>
#define SIM_LOG_INFO(...) std::cout << "[OfflineSim] " << __VA_ARGS__ << std::endl
#define SIM_LOG_WARN(...) std::cerr << "[OfflineSim WARN] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ============================================================================
// OfflineReport Implementation
// ============================================================================

std::string OfflineReport::GetSummary() const {
    std::stringstream ss;

    ss << "=== Offline Report ===\n";
    ss << "Time offline: " << ResourceFormatter::FormatDuration(hoursOffline) << "\n\n";

    // Resources
    if (!resourcesGained.empty()) {
        ss << "Resources gained:\n";
        for (const auto& [type, amount] : resourcesGained) {
            ss << "  " << ResourceFormatter::Format(type, amount) << "\n";
        }
        ss << "\n";
    }

    if (!resourcesLost.empty()) {
        ss << "Resources consumed:\n";
        for (const auto& [type, amount] : resourcesLost) {
            ss << "  " << ResourceFormatter::Format(type, amount) << "\n";
        }
        ss << "\n";
    }

    // Population
    if (workersLost > 0 || workersInjured > 0) {
        ss << "Casualties:\n";
        if (workersLost > 0) {
            ss << "  Workers lost: " << workersLost << "\n";
        }
        if (workersInjured > 0) {
            ss << "  Workers injured: " << workersInjured << "\n";
        }
        ss << "\n";
    }

    // Combat
    if (attacksReceived > 0) {
        ss << "Combat:\n";
        ss << "  Attacks: " << attacksReceived << "\n";
        ss << "  Attacks survived: " << attacksSurvived << "\n";
        ss << "  Zombies killed: " << zombiesKilled << "\n";
        if (buildingsDamaged > 0) {
            ss << "  Buildings damaged: " << buildingsDamaged << "\n";
        }
        if (buildingsDestroyed > 0) {
            ss << "  Buildings destroyed: " << buildingsDestroyed << "\n";
        }
        ss << "\n";
    }

    // Events
    if (!events.empty()) {
        ss << "Events:\n";
        for (const auto& event : events) {
            ss << "  - " << event << "\n";
        }
    }

    return ss.str();
}

void OfflineReport::AddEvent(const std::string& description) {
    events.push_back(description);
}

bool OfflineReport::HasSignificantEvents() const {
    return attacksReceived > 0 ||
           workersLost > 0 ||
           buildingsDestroyed > 0 ||
           !events.empty();
}

// ============================================================================
// ResourceFormatter Implementation
// ============================================================================

std::string ResourceFormatter::Format(ResourceType type, int amount) {
    std::stringstream ss;
    ss << ResourceTypeToString(type) << ": ";
    if (amount >= 0) {
        ss << "+" << amount;
    } else {
        ss << amount;
    }
    return ss.str();
}

std::string ResourceFormatter::FormatList(const std::map<ResourceType, int>& resources) {
    std::stringstream ss;
    bool first = true;
    for (const auto& [type, amount] : resources) {
        if (!first) ss << ", ";
        ss << Format(type, amount);
        first = false;
    }
    return ss.str();
}

std::string ResourceFormatter::FormatDuration(float hours) {
    std::stringstream ss;

    if (hours < 1.0f) {
        int minutes = static_cast<int>(hours * 60.0f);
        ss << minutes << " minute" << (minutes != 1 ? "s" : "");
    } else if (hours < 24.0f) {
        int h = static_cast<int>(hours);
        int m = static_cast<int>((hours - h) * 60.0f);
        ss << h << " hour" << (h != 1 ? "s" : "");
        if (m > 0) {
            ss << " " << m << " minute" << (m != 1 ? "s" : "");
        }
    } else {
        int days = static_cast<int>(hours / 24.0f);
        int h = static_cast<int>(hours - days * 24.0f);
        ss << days << " day" << (days != 1 ? "s" : "");
        if (h > 0) {
            ss << " " << h << " hour" << (h != 1 ? "s" : "");
        }
    }

    return ss.str();
}

// ============================================================================
// OfflineSimulation Implementation
// ============================================================================

OfflineSimulation::OfflineSimulation() {
    // Seed RNG with current time
    auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    m_rng.seed(static_cast<unsigned int>(seed));
}

OfflineSimulation& OfflineSimulation::Instance() {
    static OfflineSimulation instance;
    return instance;
}

void OfflineSimulation::SetConfig(const OfflineSimulationConfig& config) {
    m_config = config;
}

OfflineReport OfflineSimulation::Simulate(WorldState& state, float hoursOffline) {
    // Initialize report
    m_currentReport = OfflineReport();
    m_currentReport.hoursOffline = hoursOffline;

    // Clamp simulation time
    float simulatedHours = std::min(hoursOffline, m_config.maxSimulatedHours);

    if (simulatedHours < m_config.simulationTimeStep) {
        m_currentReport.AddEvent("Brief absence - no significant changes.");
        return m_currentReport;
    }

    SIM_LOG_INFO("Simulating " << simulatedHours << " hours of offline time");

    // Calculate number of steps
    int steps = static_cast<int>(std::ceil(simulatedHours / m_config.simulationTimeStep));
    float stepHours = simulatedHours / steps;

    // Run simulation steps
    for (int step = 0; step < steps; ++step) {
        // Production first
        SimulateProduction(state, stepHours);

        // Then consumption
        SimulateConsumption(state, stepHours);

        // Construction progress
        SimulateConstruction(state, stepHours);

        // Worker morale/efficiency
        SimulateWorkers(state, stepHours);

        // Resource regeneration
        if (m_config.enableResourceRegeneration) {
            SimulateResourceRegeneration(state, stepHours);
        }
    }

    // Simulate threats (attacks can happen any time)
    auto attacks = SimulateThreats(state, simulatedHours);
    m_currentReport.attacksReceived = static_cast<int>(attacks.size());

    for (const auto& attack : attacks) {
        if (attack.wasRepelled) {
            m_currentReport.attacksSurvived++;
        }
        m_currentReport.zombiesKilled += attack.zombiesKilled;
        m_currentReport.buildingsDamaged += static_cast<int>(attack.damagedBuildings.size());
        m_currentReport.buildingsDestroyed += static_cast<int>(attack.destroyedBuildings.size());
        m_currentReport.workersLost += static_cast<int>(attack.killedWorkers.size());
    }

    // Update state timestamps
    state.attacksSurvived += m_currentReport.attacksSurvived;

    // Add summary events
    if (hoursOffline > m_config.maxSimulatedHours) {
        m_currentReport.AddEvent(
            "Simulation capped at " + ResourceFormatter::FormatDuration(m_config.maxSimulatedHours) +
            ". You were away for " + ResourceFormatter::FormatDuration(hoursOffline) + "."
        );
    }

    return m_currentReport;
}

void OfflineSimulation::SimulateProduction(WorldState& state, float hours) {
    for (auto& building : state.buildings) {
        // Skip incomplete or inactive buildings
        if (!building.IsConstructed() || !building.isActive || building.IsDestroyed()) {
            continue;
        }

        // Calculate production
        ResourceType resType = building.producesResource;
        float baseRate = building.productionPerHour;

        // Worker bonus (more workers = more production)
        float workerBonus = 1.0f + (building.assignedWorkers * 0.5f);

        // Offline penalty
        float offlineMultiplier = m_config.offlineProductionMultiplier;

        // Calculate actual production
        float produced = baseRate * workerBonus * offlineMultiplier * hours;
        int producedInt = static_cast<int>(produced);

        if (producedInt > 0) {
            state.resources.Add(resType, producedInt);
            m_currentReport.resourcesGained[resType] += producedInt;
            m_currentReport.totalResourcesProduced += producedInt;

            if (m_productionCallback) {
                m_productionCallback(resType, producedInt);
            }
        }
    }
}

void OfflineSimulation::SimulateConsumption(WorldState& state, float hours) {
    // Workers consume food
    int workerCount = static_cast<int>(state.workers.size());
    float foodConsumed = workerCount * 2.0f * m_config.offlineConsumptionMultiplier * hours;
    int foodConsumedInt = static_cast<int>(foodConsumed);

    if (foodConsumedInt > 0) {
        int actualConsumed = std::min(foodConsumedInt, state.resources.Get(ResourceType::Food));
        state.resources.Consume(ResourceType::Food, actualConsumed);
        m_currentReport.resourcesLost[ResourceType::Food] += actualConsumed;
        m_currentReport.totalResourcesConsumed += actualConsumed;

        // Check for starvation
        if (state.resources.Get(ResourceType::Food) == 0 && foodConsumedInt > actualConsumed) {
            // Workers lose morale and health when starving
            int shortage = foodConsumedInt - actualConsumed;
            for (auto& worker : state.workers) {
                worker.morale = std::max(0.0f, worker.morale - shortage * 5.0f);
                if (worker.morale < 10.0f) {
                    // Severe starvation - worker might die
                    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                    if (dist(m_rng) < 0.1f) {  // 10% chance per starving period
                        worker.health = 0;
                        m_currentReport.AddEvent(worker.name + " died of starvation.");
                    }
                }
            }

            if (shortage > workerCount * 2) {
                m_currentReport.AddEvent("Your base is suffering from food shortage!");
            }
        }
    }

    // Defense buildings consume fuel
    int towerCount = 0;
    for (const auto& b : state.buildings) {
        if (b.type == BuildingType::Tower && b.IsConstructed() && b.isActive) {
            towerCount++;
        }
    }

    if (towerCount > 0) {
        float fuelConsumed = towerCount * 0.5f * m_config.offlineConsumptionMultiplier * hours;
        int fuelConsumedInt = static_cast<int>(fuelConsumed);
        if (fuelConsumedInt > 0) {
            state.resources.Consume(ResourceType::Fuel, fuelConsumedInt);
            m_currentReport.resourcesLost[ResourceType::Fuel] += fuelConsumedInt;
        }
    }
}

std::vector<OfflineAttack> OfflineSimulation::SimulateThreats(WorldState& state, float hours) {
    std::vector<OfflineAttack> attacks;

    // Calculate how many hours to check
    int totalHours = static_cast<int>(hours);
    if (totalHours < 1) {
        return attacks;
    }

    // Track attacks to enforce daily limit
    int attacksToday = 0;
    int dayOfSimulation = 0;

    for (int hour = 0; hour < totalHours; ++hour) {
        // Reset attack count at midnight (hour 0, 24, 48, etc.)
        if (hour % 24 == 0) {
            attacksToday = 0;
            dayOfSimulation++;
        }

        // Skip if we've hit daily limit
        if (attacksToday >= static_cast<int>(m_config.maxAttacksPerDay)) {
            continue;
        }

        // Check if night time
        bool isNight = IsNightHour(hour % 24);

        // Generate potential attack
        OfflineAttack attack = GenerateAttack(state, hour, isNight);

        if (attack.zombieCount > 0) {
            // Resolve the attack
            ResolveAttack(state, attack);
            attacks.push_back(attack);
            attacksToday++;

            // Add event to report
            std::stringstream ss;
            ss << "Hour " << hour << ": ";
            if (isNight) {
                ss << "Night attack! ";
            } else {
                ss << "Zombie attack! ";
            }
            ss << attack.zombieCount << " zombies. ";
            if (attack.wasRepelled) {
                ss << "Defenses held. ";
            } else {
                ss << "Some damage taken. ";
            }
            ss << attack.zombiesKilled << " zombies killed.";
            m_currentReport.AddEvent(ss.str());

            // Notify callback
            if (m_attackCallback) {
                m_attackCallback(attack);
            }
        }
    }

    return attacks;
}

void OfflineSimulation::SimulateWorkers(WorldState& state, float hours) {
    for (auto& worker : state.workers) {
        if (!worker.IsAlive()) {
            continue;
        }

        // Morale decreases while offline (player isn't there to manage)
        worker.morale = std::max(
            m_config.minMorale,
            worker.morale - m_config.moraleDrainPerHour * hours
        );

        // Efficiency affected by morale
        worker.efficiency = 0.5f + (worker.morale / 200.0f);  // 0.5 to 1.0

        // Workers can slowly recover health
        if (worker.health < worker.maxHealth) {
            // Check if hospital exists
            bool hasHospital = false;
            for (const auto& b : state.buildings) {
                if (b.type == BuildingType::Hospital && b.IsConstructed()) {
                    hasHospital = true;
                    break;
                }
            }

            float healRate = hasHospital ? 5.0f : 1.0f;  // Per hour
            worker.health = std::min(
                worker.maxHealth,
                worker.health + static_cast<int>(healRate * hours)
            );
        }
    }

    // Remove dead workers
    auto it = std::remove_if(state.workers.begin(), state.workers.end(),
        [](const Worker& w) { return !w.IsAlive(); });

    int removed = static_cast<int>(std::distance(it, state.workers.end()));
    if (removed > 0) {
        state.workers.erase(it, state.workers.end());
        m_currentReport.workersLost += removed;
    }
}

void OfflineSimulation::SimulateConstruction(WorldState& state, float hours) {
    for (auto& building : state.buildings) {
        if (building.IsConstructed() || building.IsDestroyed()) {
            continue;
        }

        // Count builders assigned to this building
        int builders = 0;
        for (const auto& worker : state.workers) {
            if (worker.job == WorkerJob::Building &&
                worker.assignedBuildingId == building.id) {
                builders++;
            }
        }

        // No progress without builders
        if (builders == 0) {
            continue;
        }

        // Construction progress: base rate * builders * hours
        float baseRate = 0.1f;  // 10% per hour per builder
        float progress = baseRate * builders * hours * m_config.offlineProductionMultiplier;

        building.constructionProgress = std::min(1.0f, building.constructionProgress + progress);

        if (building.constructionProgress >= 1.0f) {
            building.constructionProgress = 1.0f;
            building.isActive = true;

            m_currentReport.AddEvent(
                std::string(BuildingTypeToString(building.type)) + " construction completed!"
            );

            // Free up builders
            for (auto& worker : state.workers) {
                if (worker.job == WorkerJob::Building &&
                    worker.assignedBuildingId == building.id) {
                    worker.job = WorkerJob::Idle;
                    worker.assignedBuildingId = -1;
                }
            }
        }
    }
}

void OfflineSimulation::SimulateResourceRegeneration(WorldState& state, float hours) {
    for (auto& node : state.resourceNodes) {
        if (node.regenerationRate <= 0.0f || node.remaining >= node.maxAmount) {
            continue;
        }

        float regenerated = node.regenerationRate * hours;
        node.remaining = std::min(node.maxAmount, node.remaining + static_cast<int>(regenerated));

        if (node.depleted && node.remaining > 0) {
            node.depleted = false;
        }
    }
}

void OfflineSimulation::ApplyWorldEvents(WorldState& state, const std::vector<WorldEvent>& events) {
    for (const auto& event : events) {
        // Apply based on event type
        switch (event.type) {
            case WorldEvent::Type::ZombieAttack:
                // Already handled in SimulateThreats
                break;

            case WorldEvent::Type::ResourceDepleted:
                if (event.data.contains("nodeId")) {
                    int nodeId = event.data["nodeId"].get<int>();
                    for (auto& node : state.resourceNodes) {
                        if (node.id == nodeId) {
                            node.depleted = true;
                            node.remaining = 0;
                            break;
                        }
                    }
                }
                break;

            case WorldEvent::Type::SeasonChanged:
                // Season changes could affect production rates
                m_currentReport.AddEvent("Season changed: " + event.description);
                break;

            case WorldEvent::Type::WorldBossSpawned:
                // Major threat event
                m_currentReport.AddEvent("WARNING: " + event.description);
                break;

            default:
                // Generic event
                if (!event.description.empty()) {
                    m_currentReport.AddEvent(event.description);
                }
                break;
        }

        m_currentReport.worldEvents.push_back(event);
    }
}

float OfflineSimulation::CalculateDefenseStrength(const WorldState& state) const {
    float defense = 10.0f;  // Base defense

    for (const auto& building : state.buildings) {
        if (!building.IsConstructed() || building.IsDestroyed()) {
            continue;
        }

        switch (building.type) {
            case BuildingType::Wall:
                defense += 20.0f * building.level;
                break;
            case BuildingType::Tower:
                defense += 50.0f * building.level;
                break;
            case BuildingType::Gate:
                defense += 15.0f * building.level;
                break;
            case BuildingType::Bunker:
                defense += 100.0f * building.level;
                break;
            case BuildingType::CommandCenter:
                defense += 30.0f;  // Command center provides some defense
                break;
            default:
                break;
        }
    }

    // Worker defenders
    int defenders = 0;
    for (const auto& worker : state.workers) {
        if (worker.job == WorkerJob::Defending && worker.IsAlive()) {
            defenders++;
        }
    }
    defense += defenders * m_config.workerDefenseBonus * 50.0f;

    // Apply offline effectiveness
    defense *= m_config.defenseEffectiveness;

    return defense;
}

float OfflineSimulation::CalculateAttackStrength(int zombieCount, int zombieStrength, bool isNight) const {
    float attack = zombieCount * zombieStrength;

    if (isNight) {
        attack *= m_config.nightAttackMultiplier;
    }

    return attack;
}

OfflineAttack OfflineSimulation::GenerateAttack(const WorldState& state, int hour, bool isNight) {
    OfflineAttack attack;
    attack.hour = hour;
    attack.isNight = isNight;
    attack.wasRepelled = false;
    attack.damageDealt = 0;
    attack.zombiesKilled = 0;

    // Calculate attack chance
    float attackChance = m_config.baseAttackChancePerHour;
    if (isNight) {
        attackChance *= m_config.nightAttackMultiplier;
    }

    // Territory strength reduces attack chance
    attackChance *= (1.0f - state.territoryStrength * 0.005f);

    // Random check
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    if (dist(m_rng) > attackChance) {
        attack.zombieCount = 0;
        return attack;
    }

    // Generate zombie count based on time and territory
    std::uniform_int_distribution<int> countDist(5, 15 + static_cast<int>(hour / 24) * 5);
    attack.zombieCount = countDist(m_rng);

    // Zombie strength increases over time
    attack.zombieStrength = 10 + (hour / 48);  // Gets stronger every 2 days

    return attack;
}

void OfflineSimulation::ResolveAttack(WorldState& state, OfflineAttack& attack) {
    float defenseStrength = CalculateDefenseStrength(state);
    float attackStrength = CalculateAttackStrength(
        attack.zombieCount, attack.zombieStrength, attack.isNight);

    // Calculate zombie casualties
    float defenseRatio = defenseStrength / attackStrength;
    float zombieKillRate = std::min(1.0f, defenseRatio);
    attack.zombiesKilled = static_cast<int>(attack.zombieCount * zombieKillRate);

    // Check if attack was repelled
    attack.wasRepelled = defenseStrength >= attackStrength;
    state.totalZombiesKilled += attack.zombiesKilled;

    if (attack.wasRepelled) {
        // Minor damage even on success
        attack.damageDealt = static_cast<int>((1.0f - defenseRatio) * attackStrength * 0.1f);
    } else {
        // Attack breaks through - significant damage
        float excessAttack = attackStrength - defenseStrength;
        attack.damageDealt = static_cast<int>(excessAttack * 0.5f);

        // Damage buildings
        int remainingDamage = attack.damageDealt;
        while (remainingDamage > 0) {
            int buildingId = SelectBuildingToDamage(state);
            if (buildingId < 0) break;

            Building* building = state.GetBuilding(buildingId);
            if (!building) break;

            int damageTaken = std::min(remainingDamage, building->health);
            building->health -= damageTaken;
            remainingDamage -= damageTaken;

            attack.damagedBuildings.push_back(buildingId);

            if (building->health <= 0) {
                attack.destroyedBuildings.push_back(buildingId);
                m_currentReport.AddEvent(
                    std::string(BuildingTypeToString(building->type)) + " was destroyed!"
                );
            }
        }

        // Workers might die in strong attacks
        if (excessAttack > 50.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            float deathChance = std::min(0.3f, excessAttack / 500.0f);

            for (auto& worker : state.workers) {
                if (worker.IsAlive() && dist(m_rng) < deathChance) {
                    worker.health = 0;
                    attack.killedWorkers.push_back(worker.id);
                    m_currentReport.AddEvent(worker.name + " was killed in the attack!");
                }
            }
        }
    }

    m_currentReport.totalDamageReceived += attack.damageDealt;
}

bool OfflineSimulation::IsNightHour(int hour) const {
    // Night is from 20:00 (8 PM) to 6:00 (6 AM)
    return hour >= 20 || hour < 6;
}

int OfflineSimulation::SelectBuildingToDamage(const WorldState& state) {
    std::vector<int> candidates;

    for (const auto& building : state.buildings) {
        // Prioritize non-defense buildings
        if (building.IsConstructed() && !building.IsDestroyed()) {
            // Walls and towers take damage first
            if (building.type == BuildingType::Wall ||
                building.type == BuildingType::Tower ||
                building.type == BuildingType::Gate) {
                candidates.insert(candidates.begin(), building.id);
            } else {
                candidates.push_back(building.id);
            }
        }
    }

    if (candidates.empty()) {
        return -1;
    }

    // Return first candidate (prioritized)
    return candidates.front();
}

int OfflineSimulation::SelectWorkerAtRisk(const WorldState& state) {
    std::vector<int> atRisk;

    for (const auto& worker : state.workers) {
        if (worker.IsAlive()) {
            // Defenders and gatherers are more at risk
            if (worker.job == WorkerJob::Defending ||
                worker.job == WorkerJob::Gathering) {
                atRisk.push_back(worker.id);
            }
        }
    }

    if (atRisk.empty()) {
        // Any alive worker
        for (const auto& worker : state.workers) {
            if (worker.IsAlive()) {
                return worker.id;
            }
        }
        return -1;
    }

    std::uniform_int_distribution<size_t> dist(0, atRisk.size() - 1);
    return atRisk[dist(m_rng)];
}

} // namespace Vehement
