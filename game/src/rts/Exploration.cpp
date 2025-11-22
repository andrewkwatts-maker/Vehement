#include "Exploration.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace Vehement2 {
namespace RTS {

// ============================================================================
// Helper Functions
// ============================================================================

static uint64_t TileHash(int x, int y) {
    return (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(y);
}

static std::string GetDiscoveryTypeName(DiscoveryType type) {
    switch (type) {
        case DiscoveryType::ResourceNode: return "Resource Node";
        case DiscoveryType::Survivor: return "Survivor";
        case DiscoveryType::LootCache: return "Loot Cache";
        case DiscoveryType::EnemyBase: return "Enemy Base";
        case DiscoveryType::PointOfInterest: return "Point of Interest";
        case DiscoveryType::AncientRuin: return "Ancient Ruin";
        case DiscoveryType::HiddenPath: return "Hidden Path";
        case DiscoveryType::DangerZone: return "Danger Zone";
        case DiscoveryType::SafeZone: return "Safe Zone";
        case DiscoveryType::WaterSource: return "Water Source";
        case DiscoveryType::Artifact: return "Artifact";
        default: return "Unknown";
    }
}

static std::string GetRarityName(DiscoveryRarity rarity) {
    switch (rarity) {
        case DiscoveryRarity::Common: return "Common";
        case DiscoveryRarity::Uncommon: return "Uncommon";
        case DiscoveryRarity::Rare: return "Rare";
        case DiscoveryRarity::Epic: return "Epic";
        case DiscoveryRarity::Legendary: return "Legendary";
        default: return "Unknown";
    }
}

// ============================================================================
// Exploration Implementation
// ============================================================================

Exploration::Exploration() {
    // Initialize RNG with random seed
    std::random_device rd;
    m_rng.seed(rd());
}

Exploration::~Exploration() {
    Shutdown();
}

bool Exploration::Initialize(SessionFogOfWar* fogOfWar, int mapWidth, int mapHeight, float tileSize) {
    if (m_initialized) {
        spdlog::warn("Exploration already initialized");
        return true;
    }

    if (!fogOfWar) {
        spdlog::error("Exploration requires a valid SessionFogOfWar pointer");
        return false;
    }

    m_fogOfWar = fogOfWar;
    m_mapWidth = mapWidth;
    m_mapHeight = mapHeight;
    m_tileSize = tileSize;

    // Set up fog of war callback for tile reveals
    m_fogOfWar->SetTileRevealedCallback([this](const glm::ivec2& tile, FogState) {
        OnTileRevealed(tile);
    });

    m_initialized = true;
    spdlog::info("Exploration system initialized for {}x{} map", mapWidth, mapHeight);
    return true;
}

void Exploration::Shutdown() {
    if (!m_initialized) return;

    m_discoveries.clear();
    m_tileToDiscovery.clear();
    m_activeMissions.clear();
    m_completedMissions.clear();

    m_initialized = false;
    spdlog::info("Exploration system shutdown");
}

void Exploration::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update scout missions
    UpdateScoutMissions(deltaTime);

    // Update discovery respawn timers
    for (auto& discovery : m_discoveries) {
        if (discovery.claimed && discovery.respawnTime > 0.0f) {
            discovery.currentRespawnTimer += deltaTime;
            if (discovery.currentRespawnTimer >= discovery.respawnTime) {
                discovery.claimed = false;
                discovery.currentRespawnTimer = 0.0f;
                spdlog::debug("Discovery {} respawned", discovery.id);
            }
        }
    }

    // Check for milestones
    ProcessMilestones();
}

// ============================================================================
// Exploration Progress
// ============================================================================

float Exploration::GetExplorationPercent() const {
    if (!m_fogOfWar) return 0.0f;
    return m_fogOfWar->GetExplorationPercent();
}

int Exploration::GetTilesExplored() const {
    if (!m_fogOfWar) return 0;
    return m_fogOfWar->GetTilesExplored();
}

int Exploration::GetExplorationLevel() const {
    return m_explorationLevel;
}

bool Exploration::HasReachedMilestone(float percent) const {
    return std::find(m_reachedMilestones.begin(), m_reachedMilestones.end(), percent)
           != m_reachedMilestones.end();
}

// ============================================================================
// Discovery System
// ============================================================================

void Exploration::OnTileRevealed(const glm::ivec2& tile) {
    if (!m_initialized) return;

    // Grant XP for exploration
    GrantExplorationXP(m_config.xpPerTileExplored);

    // Check for discoveries at this tile
    CheckForDiscoveries(tile);
}

void Exploration::CheckForDiscoveries(const glm::ivec2& tile) {
    uint64_t hash = TileHash(tile.x, tile.y);

    // Check if there's a pre-placed discovery here
    auto it = m_tileToDiscovery.find(hash);
    if (it != m_tileToDiscovery.end()) {
        uint32_t idx = it->second;
        if (idx < m_discoveries.size() && !m_discoveries[idx].discovered) {
            Discovery& discovery = m_discoveries[idx];
            discovery.discovered = true;

            spdlog::info("Discovered {} ({}) at ({}, {})",
                         discovery.name, GetDiscoveryTypeName(discovery.type),
                         tile.x, tile.y);

            // Grant discovery XP
            float xp = discovery.GetXPReward();
            GrantExplorationXP(xp);

            // Update statistics
            switch (discovery.type) {
                case DiscoveryType::ResourceNode:
                    m_totalResourcesFound += discovery.resourceAmount;
                    break;
                case DiscoveryType::Survivor:
                    m_totalSurvivorsFound += discovery.survivorCount;
                    break;
                default:
                    break;
            }

            // Notify callback
            if (m_onDiscovery) {
                m_onDiscovery(discovery);
            }
        }
    }
}

void Exploration::PlaceDiscovery(const Discovery& discovery) {
    Discovery newDiscovery = discovery;
    newDiscovery.id = m_nextDiscoveryId++;

    uint64_t hash = TileHash(discovery.tile.x, discovery.tile.y);
    m_tileToDiscovery[hash] = static_cast<uint32_t>(m_discoveries.size());
    m_discoveries.push_back(newDiscovery);

    spdlog::debug("Placed discovery {} at ({}, {})",
                  newDiscovery.name, discovery.tile.x, discovery.tile.y);
}

void Exploration::GenerateDiscoveries(uint32_t seed) {
    if (seed != 0) {
        m_rng.seed(seed);
    }

    spdlog::info("Generating discoveries for map with seed {}", seed);

    // Clear existing discoveries
    m_discoveries.clear();
    m_tileToDiscovery.clear();
    m_nextDiscoveryId = 1;

    int totalTiles = m_mapWidth * m_mapHeight;

    // Calculate expected counts based on density
    int resourceCount = static_cast<int>((totalTiles / 100.0f) * m_config.resourceNodeDensity);
    int survivorCount = static_cast<int>((totalTiles / 100.0f) * m_config.survivorDensity);
    int lootCount = static_cast<int>((totalTiles / 100.0f) * m_config.lootCacheDensity);
    int enemyBaseCount = static_cast<int>((totalTiles / 100.0f) * m_config.enemyBaseDensity);
    int poiCount = static_cast<int>((totalTiles / 100.0f) * m_config.poiDensity);

    std::uniform_int_distribution<int> xDist(0, m_mapWidth - 1);
    std::uniform_int_distribution<int> yDist(0, m_mapHeight - 1);

    // Helper to generate a discovery at a random tile
    auto generateAtRandomTile = [&](DiscoveryType type) {
        // Try to find an unoccupied tile
        for (int attempts = 0; attempts < 100; attempts++) {
            int x = xDist(m_rng);
            int y = yDist(m_rng);
            uint64_t hash = TileHash(x, y);

            if (m_tileToDiscovery.find(hash) == m_tileToDiscovery.end()) {
                Discovery discovery = GenerateRandomDiscovery(glm::ivec2(x, y));
                discovery.type = type;
                discovery.name = GetDiscoveryTypeName(type);
                discovery.worldPosition = glm::vec2(
                    (x + 0.5f) * m_tileSize,
                    (y + 0.5f) * m_tileSize
                );
                PlaceDiscovery(discovery);
                return true;
            }
        }
        return false;
    };

    // Generate resource nodes
    for (int i = 0; i < resourceCount; i++) {
        generateAtRandomTile(DiscoveryType::ResourceNode);
    }

    // Generate survivors
    for (int i = 0; i < survivorCount; i++) {
        generateAtRandomTile(DiscoveryType::Survivor);
    }

    // Generate loot caches
    for (int i = 0; i < lootCount; i++) {
        generateAtRandomTile(DiscoveryType::LootCache);
    }

    // Generate enemy bases
    for (int i = 0; i < enemyBaseCount; i++) {
        generateAtRandomTile(DiscoveryType::EnemyBase);
    }

    // Generate POIs
    for (int i = 0; i < poiCount; i++) {
        generateAtRandomTile(DiscoveryType::PointOfInterest);
    }

    // Add a few rare discoveries
    std::uniform_int_distribution<int> rareDist(0, 3);
    int rareCount = std::max(1, totalTiles / 1000);
    for (int i = 0; i < rareCount; i++) {
        if (generateAtRandomTile(DiscoveryType::AncientRuin)) {
            // Mark as rare or higher
            if (!m_discoveries.empty()) {
                auto& last = m_discoveries.back();
                last.rarity = static_cast<DiscoveryRarity>(
                    std::min(4, static_cast<int>(DiscoveryRarity::Rare) + rareDist(m_rng))
                );
            }
        }
    }

    // Add artifacts (very rare)
    int artifactCount = std::max(1, totalTiles / 5000);
    for (int i = 0; i < artifactCount; i++) {
        if (generateAtRandomTile(DiscoveryType::Artifact)) {
            if (!m_discoveries.empty()) {
                m_discoveries.back().rarity = DiscoveryRarity::Legendary;
            }
        }
    }

    spdlog::info("Generated {} discoveries", m_discoveries.size());
}

Discovery Exploration::GenerateRandomDiscovery(const glm::ivec2& tile) {
    Discovery discovery;
    discovery.tile = tile;
    discovery.worldPosition = glm::vec2(
        (tile.x + 0.5f) * m_tileSize,
        (tile.y + 0.5f) * m_tileSize
    );

    // Random rarity
    std::uniform_int_distribution<int> rarityDist(0, 100);
    int roll = rarityDist(m_rng);
    if (roll < 60) {
        discovery.rarity = DiscoveryRarity::Common;
    } else if (roll < 85) {
        discovery.rarity = DiscoveryRarity::Uncommon;
    } else if (roll < 95) {
        discovery.rarity = DiscoveryRarity::Rare;
    } else if (roll < 99) {
        discovery.rarity = DiscoveryRarity::Epic;
    } else {
        discovery.rarity = DiscoveryRarity::Legendary;
    }

    // Generate type-specific data
    std::uniform_int_distribution<int> resourceDist(10, 100);
    std::uniform_int_distribution<int> survivorDist(1, 5);
    std::uniform_int_distribution<int> tierDist(1, 5);

    discovery.resourceAmount = resourceDist(m_rng) * (1 + static_cast<int>(discovery.rarity));
    discovery.survivorCount = survivorDist(m_rng);
    discovery.lootTier = std::min(5, tierDist(m_rng) + static_cast<int>(discovery.rarity));

    // Resource types
    std::vector<std::string> resourceTypes = { "Wood", "Stone", "Iron", "Gold", "Crystal" };
    std::uniform_int_distribution<size_t> resourceTypeDist(0, resourceTypes.size() - 1);
    discovery.resourceType = resourceTypes[resourceTypeDist(m_rng)];

    return discovery;
}

std::vector<const Discovery*> Exploration::GetDiscoveriesOfType(DiscoveryType type) const {
    std::vector<const Discovery*> result;
    for (const auto& discovery : m_discoveries) {
        if (discovery.type == type) {
            result.push_back(&discovery);
        }
    }
    return result;
}

std::vector<const Discovery*> Exploration::GetUndiscoveredInRange(const glm::vec2& center,
                                                                    float radius) const {
    std::vector<const Discovery*> result;
    float radiusSq = radius * radius;

    for (const auto& discovery : m_discoveries) {
        if (!discovery.discovered) {
            float distSq = glm::dot(discovery.worldPosition - center,
                                    discovery.worldPosition - center);
            if (distSq <= radiusSq) {
                result.push_back(&discovery);
            }
        }
    }
    return result;
}

std::vector<const Discovery*> Exploration::GetUnclaimedDiscoveries() const {
    std::vector<const Discovery*> result;
    for (const auto& discovery : m_discoveries) {
        if (discovery.discovered && !discovery.claimed) {
            result.push_back(&discovery);
        }
    }
    return result;
}

bool Exploration::ClaimDiscovery(uint32_t discoveryId) {
    for (auto& discovery : m_discoveries) {
        if (discovery.id == discoveryId) {
            if (discovery.discovered && !discovery.claimed) {
                discovery.claimed = true;
                spdlog::info("Claimed discovery {} ({})",
                             discovery.name, GetDiscoveryTypeName(discovery.type));
                return true;
            }
            return false;
        }
    }
    return false;
}

// ============================================================================
// Scout Missions
// ============================================================================

uint32_t Exploration::SendScout(Worker* scout, const glm::vec2& destination) {
    if (!scout || !scout->IsAvailable()) {
        spdlog::warn("Cannot send scout: invalid or unavailable worker");
        return 0;
    }

    ScoutMission mission;
    mission.missionId = m_nextMissionId++;
    mission.scoutUnitId = scout->GetId();
    mission.startPosition = scout->GetPosition();
    mission.destination = destination;
    mission.status = ScoutMission::Status::InProgress;

    // Calculate path
    mission.waypoints = CalculateScoutPath(mission.startPosition, destination);

    // Estimate duration
    float totalDistance = 0.0f;
    glm::vec2 prev = mission.startPosition;
    for (const auto& wp : mission.waypoints) {
        totalDistance += glm::length(wp - prev);
        prev = wp;
    }
    mission.estimatedDuration = totalDistance / (m_config.scoutSpeed * m_tileSize);

    // Mark worker as busy
    scout->SetAvailable(false);

    m_activeMissions.push_back(mission);
    spdlog::info("Scout mission {} started to ({}, {})",
                 mission.missionId, destination.x, destination.y);

    return mission.missionId;
}

void Exploration::CancelScoutMission(uint32_t missionId) {
    for (auto it = m_activeMissions.begin(); it != m_activeMissions.end(); ++it) {
        if (it->missionId == missionId) {
            it->status = ScoutMission::Status::Aborted;
            m_completedMissions.push_back(*it);
            m_activeMissions.erase(it);
            spdlog::info("Scout mission {} cancelled", missionId);
            return;
        }
    }
}

void Exploration::UpdateScoutMissions(float deltaTime) {
    for (auto it = m_activeMissions.begin(); it != m_activeMissions.end();) {
        ScoutMission& mission = *it;

        if (mission.status == ScoutMission::Status::InProgress) {
            // Update progress
            float progressIncrement = (m_config.scoutSpeed * m_tileSize * deltaTime) /
                                       (mission.estimatedDuration * m_config.scoutSpeed * m_tileSize);
            mission.progress = std::min(1.0f, mission.progress + progressIncrement);

            // Reveal tiles along the path as scout moves
            if (!mission.waypoints.empty()) {
                size_t wpIndex = static_cast<size_t>(mission.progress * mission.waypoints.size());
                wpIndex = std::min(wpIndex, mission.waypoints.size() - 1);

                glm::vec2 currentPos = mission.waypoints[wpIndex];

                // Reveal area around scout
                if (m_fogOfWar) {
                    m_fogOfWar->RevealArea(currentPos,
                                           m_config.scoutVisionBonus * m_tileSize);
                }
            }

            // Check for completion
            if (mission.progress >= 1.0f) {
                mission.status = ScoutMission::Status::Completed;

                // Final reveal at destination
                if (m_fogOfWar) {
                    m_fogOfWar->RevealArea(mission.destination,
                                           m_config.scoutVisionBonus * 2.0f * m_tileSize);
                }

                // Calculate XP
                mission.explorationXPEarned = glm::length(mission.destination - mission.startPosition)
                                               / m_tileSize * m_config.xpPerTileExplored;
                GrantExplorationXP(mission.explorationXPEarned);

                spdlog::info("Scout mission {} completed", mission.missionId);

                // Notify callback
                if (m_onScoutComplete) {
                    m_onScoutComplete(mission);
                }

                // Move to completed
                m_completedMissions.push_back(mission);
                it = m_activeMissions.erase(it);
                continue;
            }
        }

        ++it;
    }
}

std::vector<glm::vec2> Exploration::CalculateScoutPath(const glm::vec2& from, const glm::vec2& to) {
    // Simple direct path with intermediate waypoints
    // In a real implementation, this would use A* pathfinding
    std::vector<glm::vec2> path;

    glm::vec2 delta = to - from;
    float distance = glm::length(delta);
    int numWaypoints = std::max(2, static_cast<int>(distance / (m_tileSize * 2.0f)));

    for (int i = 0; i <= numWaypoints; i++) {
        float t = static_cast<float>(i) / static_cast<float>(numWaypoints);
        path.push_back(from + delta * t);
    }

    return path;
}

// ============================================================================
// XP and Rewards
// ============================================================================

void Exploration::GrantExplorationXP(float amount) {
    m_totalXP += amount;

    // Check for level up
    float xpForNextLevel = GetXPForNextLevel();
    while (m_totalXP >= xpForNextLevel && m_explorationLevel < 100) {
        m_explorationLevel++;
        spdlog::info("Exploration level up! Now level {}", m_explorationLevel);
        xpForNextLevel = GetXPForNextLevel();
    }
}

float Exploration::GetXPForNextLevel() const {
    return CalculateXPForLevel(m_explorationLevel + 1);
}

float Exploration::GetLevelProgress() const {
    float currentLevelXP = CalculateXPForLevel(m_explorationLevel);
    float nextLevelXP = CalculateXPForLevel(m_explorationLevel + 1);
    float xpIntoLevel = m_totalXP - currentLevelXP;
    float xpNeeded = nextLevelXP - currentLevelXP;
    return xpNeeded > 0 ? xpIntoLevel / xpNeeded : 1.0f;
}

float Exploration::CalculateXPForLevel(int level) const {
    // Exponential XP curve
    return 100.0f * std::pow(1.5f, level - 1);
}

// ============================================================================
// Milestones
// ============================================================================

void Exploration::ProcessMilestones() {
    float currentPercent = GetExplorationPercent();

    for (float milestone : m_config.milestones) {
        if (currentPercent >= milestone && !HasReachedMilestone(milestone)) {
            m_reachedMilestones.push_back(milestone);
            spdlog::info("Exploration milestone reached: {}%", milestone);

            // Bonus XP for milestone
            float bonusXP = milestone * 2.0f;
            GrantExplorationXP(bonusXP);

            if (m_onMilestone) {
                m_onMilestone(milestone);
            }
        }
    }

    m_lastExplorationPercent = currentPercent;
}

// ============================================================================
// Statistics
// ============================================================================

int Exploration::GetDiscoveryCount() const {
    int count = 0;
    for (const auto& discovery : m_discoveries) {
        if (discovery.discovered) count++;
    }
    return count;
}

int Exploration::GetDiscoveryCount(DiscoveryType type) const {
    int count = 0;
    for (const auto& discovery : m_discoveries) {
        if (discovery.discovered && discovery.type == type) count++;
    }
    return count;
}

int Exploration::GetTotalResourcesFound() const {
    return m_totalResourcesFound;
}

int Exploration::GetTotalSurvivorsFound() const {
    return m_totalSurvivorsFound;
}

} // namespace RTS
} // namespace Vehement2
