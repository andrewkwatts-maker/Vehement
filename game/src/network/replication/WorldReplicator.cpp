#include "WorldReplicator.hpp"
#include "ReplicationManager.hpp"
#include <algorithm>
#include <cmath>

namespace Network {
namespace Replication {

WorldReplicator& WorldReplicator::getInstance() {
    static WorldReplicator instance;
    return instance;
}

WorldReplicator::WorldReplicator() {}

WorldReplicator::~WorldReplicator() {
    shutdown();
}

bool WorldReplicator::initialize() {
    if (m_initialized) return true;
    m_initialized = true;
    return true;
}

void WorldReplicator::shutdown() {
    if (!m_initialized) return;

    m_buildings.clear();
    m_resources.clear();
    m_territories.clear();
    m_fogOfWar.clear();
    m_chunks.clear();

    m_initialized = false;
}

void WorldReplicator::update(float deltaTime) {
    if (!m_initialized) return;

    // Update fog of war
    m_fogTimer += deltaTime;
    if (m_fogTimer >= m_fogUpdateRate) {
        m_fogTimer = 0.0f;
        updateFog(deltaTime);
    }

    // Update territories
    m_territoryTimer += deltaTime;
    if (m_territoryTimer >= m_territoryUpdateRate) {
        m_territoryTimer = 0.0f;
        updateTerritories(deltaTime);
    }

    // Update resources
    updateResources(deltaTime);
}

void WorldReplicator::registerBuilding(const BuildingData& building) {
    m_buildings[building.buildingId] = building;

    // Add to chunk
    auto [cx, cy] = getChunkCoords(building.position);
    m_chunks[{cx, cy}].buildings.push_back(building);
}

void WorldReplicator::unregisterBuilding(uint64_t buildingId) {
    auto it = m_buildings.find(buildingId);
    if (it == m_buildings.end()) return;

    // Remove from chunk
    auto [cx, cy] = getChunkCoords(it->second.position);
    auto& chunk = m_chunks[{cx, cy}];
    chunk.buildings.erase(
        std::remove_if(chunk.buildings.begin(), chunk.buildings.end(),
            [buildingId](const BuildingData& b) { return b.buildingId == buildingId; }),
        chunk.buildings.end());

    m_buildings.erase(it);
}

void WorldReplicator::replicateBuildingPlace(const BuildingData& building) {
    registerBuilding(building);

    for (auto& callback : m_buildingPlaceCallbacks) {
        callback(building);
    }
}

void WorldReplicator::replicateBuildingDestroy(uint64_t buildingId, uint64_t destroyerId) {
    auto it = m_buildings.find(buildingId);
    if (it == m_buildings.end()) return;

    it->second.isDestroyed = true;
    it->second.health = 0.0f;

    for (auto& callback : m_buildingDestroyCallbacks) {
        callback(it->second);
    }

    unregisterBuilding(buildingId);
}

void WorldReplicator::replicateBuildingDamage(uint64_t buildingId, float damage) {
    auto it = m_buildings.find(buildingId);
    if (it == m_buildings.end()) return;

    it->second.health = std::max(0.0f, it->second.health - damage);

    if (it->second.health <= 0.0f) {
        replicateBuildingDestroy(buildingId, 0);
    }
}

void WorldReplicator::replicateBuildingRepair(uint64_t buildingId, float amount) {
    auto it = m_buildings.find(buildingId);
    if (it == m_buildings.end()) return;

    it->second.health = std::min(it->second.maxHealth, it->second.health + amount);
}

void WorldReplicator::replicateBuildingUpgrade(uint64_t buildingId, int newLevel) {
    auto it = m_buildings.find(buildingId);
    if (it == m_buildings.end()) return;

    it->second.level = newLevel;
}

void WorldReplicator::replicateConstructionProgress(uint64_t buildingId, float progress) {
    auto it = m_buildings.find(buildingId);
    if (it == m_buildings.end()) return;

    it->second.constructionProgress = progress;
    it->second.isConstructing = progress < 1.0f;

    if (progress >= 1.0f) {
        for (auto& callback : m_buildingPlaceCallbacks) {
            callback(it->second);
        }
    }
}

const BuildingData* WorldReplicator::getBuilding(uint64_t buildingId) const {
    auto it = m_buildings.find(buildingId);
    if (it != m_buildings.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<uint64_t> WorldReplicator::getBuildingsByTeam(int team) const {
    std::vector<uint64_t> result;
    for (const auto& [id, building] : m_buildings) {
        if (building.team == team && !building.isDestroyed) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<uint64_t> WorldReplicator::getBuildingsInRange(const NetVec3& center,
                                                            float radius) const {
    std::vector<uint64_t> result;
    float radiusSq = radius * radius;

    for (const auto& [id, building] : m_buildings) {
        if (building.isDestroyed) continue;

        float dx = building.position.x - center.x;
        float dy = building.position.y - center.y;
        float dz = building.position.z - center.z;
        float distSq = dx*dx + dy*dy + dz*dz;

        if (distSq <= radiusSq) {
            result.push_back(id);
        }
    }
    return result;
}

void WorldReplicator::registerResource(const ResourceNodeData& resource) {
    m_resources[resource.nodeId] = resource;

    auto [cx, cy] = getChunkCoords(resource.position);
    m_chunks[{cx, cy}].resources.push_back(resource);
}

void WorldReplicator::unregisterResource(uint64_t nodeId) {
    auto it = m_resources.find(nodeId);
    if (it == m_resources.end()) return;

    auto [cx, cy] = getChunkCoords(it->second.position);
    auto& chunk = m_chunks[{cx, cy}];
    chunk.resources.erase(
        std::remove_if(chunk.resources.begin(), chunk.resources.end(),
            [nodeId](const ResourceNodeData& r) { return r.nodeId == nodeId; }),
        chunk.resources.end());

    m_resources.erase(it);
}

void WorldReplicator::replicateResourceHarvest(uint64_t nodeId, uint64_t harvesterId,
                                                float amount) {
    auto it = m_resources.find(nodeId);
    if (it == m_resources.end()) return;

    it->second.currentAmount = std::max(0.0f, it->second.currentAmount - amount);
    it->second.harvesters.insert(harvesterId);
    it->second.lastHarvest = std::chrono::steady_clock::now();

    if (it->second.currentAmount <= 0.0f) {
        it->second.isDepleted = true;
    }

    for (auto& callback : m_resourceCallbacks) {
        callback(it->second);
    }
}

void WorldReplicator::replicateResourceRegeneration(uint64_t nodeId, float amount) {
    auto it = m_resources.find(nodeId);
    if (it == m_resources.end()) return;

    it->second.currentAmount = std::min(it->second.maxAmount,
                                         it->second.currentAmount + amount);

    if (it->second.currentAmount > 0.0f) {
        it->second.isDepleted = false;
    }

    for (auto& callback : m_resourceCallbacks) {
        callback(it->second);
    }
}

void WorldReplicator::replicateResourceDepletion(uint64_t nodeId) {
    auto it = m_resources.find(nodeId);
    if (it == m_resources.end()) return;

    it->second.isDepleted = true;
    it->second.currentAmount = 0.0f;

    for (auto& callback : m_resourceCallbacks) {
        callback(it->second);
    }
}

void WorldReplicator::replicateResourceRespawn(uint64_t nodeId, float amount) {
    auto it = m_resources.find(nodeId);
    if (it == m_resources.end()) return;

    it->second.currentAmount = amount;
    it->second.isDepleted = false;

    for (auto& callback : m_resourceCallbacks) {
        callback(it->second);
    }
}

const ResourceNodeData* WorldReplicator::getResource(uint64_t nodeId) const {
    auto it = m_resources.find(nodeId);
    if (it != m_resources.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<uint64_t> WorldReplicator::getResourcesByType(const std::string& type) const {
    std::vector<uint64_t> result;
    for (const auto& [id, resource] : m_resources) {
        if (resource.resourceType == type && !resource.isDepleted) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<uint64_t> WorldReplicator::getResourcesInRange(const NetVec3& center,
                                                            float radius) const {
    std::vector<uint64_t> result;
    float radiusSq = radius * radius;

    for (const auto& [id, resource] : m_resources) {
        float dx = resource.position.x - center.x;
        float dy = resource.position.y - center.y;
        float dz = resource.position.z - center.z;
        float distSq = dx*dx + dy*dy + dz*dz;

        if (distSq <= radiusSq) {
            result.push_back(id);
        }
    }
    return result;
}

void WorldReplicator::registerTerritory(const TerritoryData& territory) {
    m_territories[territory.territoryId] = territory;

    auto [cx, cy] = getChunkCoords(territory.center);
    m_chunks[{cx, cy}].territories.push_back(territory);
}

void WorldReplicator::unregisterTerritory(uint64_t territoryId) {
    m_territories.erase(territoryId);
}

void WorldReplicator::replicateTerritoryCapture(uint64_t territoryId, int newTeam) {
    auto it = m_territories.find(territoryId);
    if (it == m_territories.end()) return;

    it->second.owningTeam = newTeam;
    it->second.state = TerritoryState::Owned;
    it->second.captureProgress = 1.0f;

    for (auto& callback : m_territoryCallbacks) {
        callback(it->second);
    }
}

void WorldReplicator::replicateCaptureProgress(uint64_t territoryId, float progress) {
    auto it = m_territories.find(territoryId);
    if (it == m_territories.end()) return;

    it->second.captureProgress = progress;

    if (progress > 0.0f && progress < 1.0f) {
        it->second.state = TerritoryState::Contested;
    }

    for (auto& callback : m_territoryCallbacks) {
        callback(it->second);
    }
}

void WorldReplicator::replicateUnitEnterTerritory(uint64_t unitId, uint64_t territoryId) {
    auto it = m_territories.find(territoryId);
    if (it == m_territories.end()) return;

    it->second.unitsInTerritory.insert(unitId);
    updateTerritoryControl(territoryId);
}

void WorldReplicator::replicateUnitLeaveTerritory(uint64_t unitId, uint64_t territoryId) {
    auto it = m_territories.find(territoryId);
    if (it == m_territories.end()) return;

    it->second.unitsInTerritory.erase(unitId);
    updateTerritoryControl(territoryId);
}

const TerritoryData* WorldReplicator::getTerritory(uint64_t territoryId) const {
    auto it = m_territories.find(territoryId);
    if (it != m_territories.end()) {
        return &it->second;
    }
    return nullptr;
}

uint64_t WorldReplicator::getTerritoryAt(const NetVec3& position) const {
    for (const auto& [id, territory] : m_territories) {
        if (isPointInTerritory(position, territory)) {
            return id;
        }
    }
    return 0;
}

std::vector<uint64_t> WorldReplicator::getTerritoriesByTeam(int team) const {
    std::vector<uint64_t> result;
    for (const auto& [id, territory] : m_territories) {
        if (territory.owningTeam == team) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<uint64_t> WorldReplicator::getContestedTerritories() const {
    std::vector<uint64_t> result;
    for (const auto& [id, territory] : m_territories) {
        if (territory.state == TerritoryState::Contested) {
            result.push_back(id);
        }
    }
    return result;
}

void WorldReplicator::initializeFogOfWar(uint64_t playerId, int width, int height,
                                          float cellSize) {
    FogOfWarData fog;
    fog.playerId = playerId;
    fog.gridWidth = width;
    fog.gridHeight = height;
    fog.cellSize = cellSize;
    fog.cells.resize(width * height, FogOfWarCell::State::Hidden);

    m_fogOfWar[playerId] = fog;
}

void WorldReplicator::updateFogOfWar(uint64_t playerId, const std::vector<NetVec3>& visionSources,
                                      float visionRange) {
    auto it = m_fogOfWar.find(playerId);
    if (it == m_fogOfWar.end()) return;

    auto& fog = it->second;

    // Decay visible cells to explored
    for (auto& state : fog.cells) {
        if (state == FogOfWarCell::State::Visible) {
            state = FogOfWarCell::State::Explored;
        }
    }

    // Clear visible entities
    fog.visibleEntities.clear();

    // Apply vision from each source
    for (const auto& source : visionSources) {
        int centerCellX = static_cast<int>(source.x / fog.cellSize);
        int centerCellY = static_cast<int>(source.z / fog.cellSize);
        int radiusCells = static_cast<int>(visionRange / fog.cellSize);

        for (int dy = -radiusCells; dy <= radiusCells; dy++) {
            for (int dx = -radiusCells; dx <= radiusCells; dx++) {
                int cellX = centerCellX + dx;
                int cellY = centerCellY + dy;

                if (cellX < 0 || cellX >= fog.gridWidth ||
                    cellY < 0 || cellY >= fog.gridHeight) continue;

                float dist = std::sqrt(static_cast<float>(dx*dx + dy*dy)) * fog.cellSize;
                if (dist <= visionRange) {
                    int index = cellY * fog.gridWidth + cellX;
                    fog.cells[index] = FogOfWarCell::State::Visible;
                }
            }
        }
    }

    for (auto& callback : m_fogCallbacks) {
        callback(fog);
    }
}

void WorldReplicator::revealArea(uint64_t playerId, const NetVec3& center, float radius) {
    auto it = m_fogOfWar.find(playerId);
    if (it == m_fogOfWar.end()) return;

    auto& fog = it->second;
    int centerCellX = static_cast<int>(center.x / fog.cellSize);
    int centerCellY = static_cast<int>(center.z / fog.cellSize);
    int radiusCells = static_cast<int>(radius / fog.cellSize);

    for (int dy = -radiusCells; dy <= radiusCells; dy++) {
        for (int dx = -radiusCells; dx <= radiusCells; dx++) {
            int cellX = centerCellX + dx;
            int cellY = centerCellY + dy;

            if (cellX < 0 || cellX >= fog.gridWidth ||
                cellY < 0 || cellY >= fog.gridHeight) continue;

            float dist = std::sqrt(static_cast<float>(dx*dx + dy*dy)) * fog.cellSize;
            if (dist <= radius) {
                int index = cellY * fog.gridWidth + cellX;
                fog.cells[index] = FogOfWarCell::State::Visible;
            }
        }
    }
}

void WorldReplicator::hideArea(uint64_t playerId, const NetVec3& center, float radius) {
    auto it = m_fogOfWar.find(playerId);
    if (it == m_fogOfWar.end()) return;

    auto& fog = it->second;
    int centerCellX = static_cast<int>(center.x / fog.cellSize);
    int centerCellY = static_cast<int>(center.z / fog.cellSize);
    int radiusCells = static_cast<int>(radius / fog.cellSize);

    for (int dy = -radiusCells; dy <= radiusCells; dy++) {
        for (int dx = -radiusCells; dx <= radiusCells; dx++) {
            int cellX = centerCellX + dx;
            int cellY = centerCellY + dy;

            if (cellX < 0 || cellX >= fog.gridWidth ||
                cellY < 0 || cellY >= fog.gridHeight) continue;

            float dist = std::sqrt(static_cast<float>(dx*dx + dy*dy)) * fog.cellSize;
            if (dist <= radius) {
                int index = cellY * fog.gridWidth + cellX;
                if (fog.cells[index] == FogOfWarCell::State::Visible) {
                    fog.cells[index] = FogOfWarCell::State::Explored;
                }
            }
        }
    }
}

bool WorldReplicator::isVisible(uint64_t playerId, const NetVec3& position) const {
    auto it = m_fogOfWar.find(playerId);
    if (it == m_fogOfWar.end()) return true;  // No fog = all visible

    const auto& fog = it->second;
    int index = getCellIndex(playerId, position);
    if (index < 0 || index >= static_cast<int>(fog.cells.size())) return false;

    return fog.cells[index] == FogOfWarCell::State::Visible;
}

bool WorldReplicator::isExplored(uint64_t playerId, const NetVec3& position) const {
    auto it = m_fogOfWar.find(playerId);
    if (it == m_fogOfWar.end()) return true;

    const auto& fog = it->second;
    int index = getCellIndex(playerId, position);
    if (index < 0 || index >= static_cast<int>(fog.cells.size())) return false;

    return fog.cells[index] != FogOfWarCell::State::Hidden;
}

bool WorldReplicator::isEntityVisible(uint64_t playerId, uint64_t entityId) const {
    auto it = m_fogOfWar.find(playerId);
    if (it == m_fogOfWar.end()) return true;

    return it->second.visibleEntities.find(entityId) != it->second.visibleEntities.end();
}

const FogOfWarData* WorldReplicator::getFogOfWar(uint64_t playerId) const {
    auto it = m_fogOfWar.find(playerId);
    if (it != m_fogOfWar.end()) {
        return &it->second;
    }
    return nullptr;
}

void WorldReplicator::replicateFogOfWar(uint64_t playerId) {
    // Send fog state to player
}

void WorldReplicator::requestChunk(int chunkX, int chunkY) {
    // Request chunk from server
}

void WorldReplicator::sendChunk(int chunkX, int chunkY) {
    auto it = m_chunks.find({chunkX, chunkY});
    if (it == m_chunks.end()) return;

    // Serialize and send chunk
}

const WorldChunk* WorldReplicator::getChunk(int chunkX, int chunkY) const {
    auto it = m_chunks.find({chunkX, chunkY});
    if (it != m_chunks.end()) {
        return &it->second;
    }
    return nullptr;
}

std::pair<int, int> WorldReplicator::getChunkCoords(const NetVec3& position) const {
    int cx = static_cast<int>(std::floor(position.x / m_chunkSize));
    int cy = static_cast<int>(std::floor(position.z / m_chunkSize));
    return {cx, cy};
}

void WorldReplicator::onBuildingPlaced(BuildingCallback callback) {
    m_buildingPlaceCallbacks.push_back(callback);
}

void WorldReplicator::onBuildingDestroyed(BuildingCallback callback) {
    m_buildingDestroyCallbacks.push_back(callback);
}

void WorldReplicator::onResourceChanged(ResourceCallback callback) {
    m_resourceCallbacks.push_back(callback);
}

void WorldReplicator::onTerritoryChanged(TerritoryCallback callback) {
    m_territoryCallbacks.push_back(callback);
}

void WorldReplicator::onFogUpdate(FogCallback callback) {
    m_fogCallbacks.push_back(callback);
}

// Private methods

void WorldReplicator::updateTerritories(float deltaTime) {
    for (auto& [id, territory] : m_territories) {
        if (territory.state == TerritoryState::Contested) {
            updateTerritoryControl(id);
        }
    }
}

void WorldReplicator::updateResources(float deltaTime) {
    auto now = std::chrono::steady_clock::now();

    for (auto& [id, resource] : m_resources) {
        if (resource.isDepleted) continue;
        if (resource.regenerationRate <= 0.0f) continue;
        if (resource.currentAmount >= resource.maxAmount) continue;

        // Regenerate resources
        resource.currentAmount = std::min(resource.maxAmount,
            resource.currentAmount + resource.regenerationRate * deltaTime);
    }
}

void WorldReplicator::updateFog(float deltaTime) {
    // Fog updates are handled in updateFogOfWar
}

int WorldReplicator::getCellIndex(uint64_t playerId, const NetVec3& position) const {
    auto it = m_fogOfWar.find(playerId);
    if (it == m_fogOfWar.end()) return -1;

    const auto& fog = it->second;
    int cellX = static_cast<int>(position.x / fog.cellSize);
    int cellY = static_cast<int>(position.z / fog.cellSize);

    if (cellX < 0 || cellX >= fog.gridWidth ||
        cellY < 0 || cellY >= fog.gridHeight) {
        return -1;
    }

    return cellY * fog.gridWidth + cellX;
}

void WorldReplicator::setFogCell(uint64_t playerId, int x, int y, FogOfWarCell::State state) {
    auto it = m_fogOfWar.find(playerId);
    if (it == m_fogOfWar.end()) return;

    auto& fog = it->second;
    if (x < 0 || x >= fog.gridWidth || y < 0 || y >= fog.gridHeight) return;

    int index = y * fog.gridWidth + x;
    fog.cells[index] = state;
}

bool WorldReplicator::isPointInTerritory(const NetVec3& point,
                                          const TerritoryData& territory) const {
    // Simple polygon point-in-polygon test
    const auto& bounds = territory.boundaries;
    if (bounds.size() < 3) return false;

    int crossings = 0;
    for (size_t i = 0; i < bounds.size(); i++) {
        const NetVec3& v1 = bounds[i];
        const NetVec3& v2 = bounds[(i + 1) % bounds.size()];

        if ((v1.z <= point.z && v2.z > point.z) ||
            (v2.z <= point.z && v1.z > point.z)) {

            float t = (point.z - v1.z) / (v2.z - v1.z);
            float x = v1.x + t * (v2.x - v1.x);

            if (point.x < x) {
                crossings++;
            }
        }
    }

    return (crossings % 2) == 1;
}

void WorldReplicator::updateTerritoryControl(uint64_t territoryId) {
    auto it = m_territories.find(territoryId);
    if (it == m_territories.end()) return;

    auto& territory = it->second;

    // Count units by team
    territory.teamPresence.clear();

    for (uint64_t unitId : territory.unitsInTerritory) {
        // Would get unit team from UnitReplicator
        // For now, assume team 0
        territory.teamPresence[0]++;
    }

    // Determine control
    int dominantTeam = -1;
    int maxPresence = 0;

    for (const auto& [team, count] : territory.teamPresence) {
        if (count > maxPresence) {
            maxPresence = count;
            dominantTeam = team;
        }
    }

    if (dominantTeam >= 0 && maxPresence > 0) {
        if (dominantTeam != territory.owningTeam) {
            territory.state = TerritoryState::Contested;
            territory.captureProgress += territory.captureRate * m_territoryUpdateRate;

            if (territory.captureProgress >= 1.0f) {
                replicateTerritoryCapture(territoryId, dominantTeam);
            }
        }
    }
}

} // namespace Replication
} // namespace Network
