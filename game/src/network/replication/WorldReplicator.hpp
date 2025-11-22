#pragma once

#include "NetworkedEntity.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace Network {
namespace Replication {

// World object types
enum class WorldObjectType {
    Building,
    Resource,
    Decoration,
    Terrain,
    Path,
    Trigger,
    SpawnPoint,
    Custom
};

// Territory state
enum class TerritoryState {
    Neutral,
    Contested,
    Owned
};

// Building data
struct BuildingData {
    uint64_t buildingId;
    std::string buildingType;
    uint64_t ownerId;
    int team;
    NetVec3 position;
    NetQuat rotation;
    float health;
    float maxHealth;
    int level;
    float constructionProgress;
    bool isConstructing;
    bool isDestroyed;
    std::unordered_map<std::string, std::string> properties;
};

// Resource node data
struct ResourceNodeData {
    uint64_t nodeId;
    std::string resourceType;
    NetVec3 position;
    float currentAmount;
    float maxAmount;
    float regenerationRate;
    bool isDepleted;
    std::chrono::steady_clock::time_point lastHarvest;
    std::unordered_set<uint64_t> harvesters;
};

// Territory data
struct TerritoryData {
    uint64_t territoryId;
    std::string name;
    std::vector<NetVec3> boundaries;
    NetVec3 center;
    TerritoryState state;
    uint64_t ownerId;
    int owningTeam;
    float captureProgress;
    float captureRate;
    std::unordered_set<uint64_t> unitsInTerritory;
    std::unordered_map<int, int> teamPresence;  // team -> unit count
};

// Fog of war cell
struct FogOfWarCell {
    int x, y;
    enum class State { Hidden, Explored, Visible } state;
    std::chrono::steady_clock::time_point lastSeen;
};

// Fog of war data for a player
struct FogOfWarData {
    uint64_t playerId;
    int gridWidth;
    int gridHeight;
    float cellSize;
    std::vector<FogOfWarCell::State> cells;
    std::unordered_set<uint64_t> visibleEntities;
};

// World sync chunk
struct WorldChunk {
    int chunkX, chunkY;
    std::vector<BuildingData> buildings;
    std::vector<ResourceNodeData> resources;
    std::vector<TerritoryData> territories;
    uint64_t version;
    std::chrono::steady_clock::time_point lastUpdate;
};

// Callbacks
using BuildingCallback = std::function<void(const BuildingData&)>;
using ResourceCallback = std::function<void(const ResourceNodeData&)>;
using TerritoryCallback = std::function<void(const TerritoryData&)>;
using FogCallback = std::function<void(const FogOfWarData&)>;

/**
 * WorldReplicator - World state replication
 *
 * Features:
 * - Building placement sync
 * - Resource node sync
 * - Fog of war sync
 * - Territory ownership sync
 */
class WorldReplicator {
public:
    static WorldReplicator& getInstance();

    // Initialization
    bool initialize();
    void shutdown();
    void update(float deltaTime);

    // Building replication
    void registerBuilding(const BuildingData& building);
    void unregisterBuilding(uint64_t buildingId);
    void replicateBuildingPlace(const BuildingData& building);
    void replicateBuildingDestroy(uint64_t buildingId, uint64_t destroyerId);
    void replicateBuildingDamage(uint64_t buildingId, float damage);
    void replicateBuildingRepair(uint64_t buildingId, float amount);
    void replicateBuildingUpgrade(uint64_t buildingId, int newLevel);
    void replicateConstructionProgress(uint64_t buildingId, float progress);
    const BuildingData* getBuilding(uint64_t buildingId) const;
    std::vector<uint64_t> getBuildingsByTeam(int team) const;
    std::vector<uint64_t> getBuildingsInRange(const NetVec3& center, float radius) const;

    // Resource replication
    void registerResource(const ResourceNodeData& resource);
    void unregisterResource(uint64_t nodeId);
    void replicateResourceHarvest(uint64_t nodeId, uint64_t harvesterId, float amount);
    void replicateResourceRegeneration(uint64_t nodeId, float amount);
    void replicateResourceDepletion(uint64_t nodeId);
    void replicateResourceRespawn(uint64_t nodeId, float amount);
    const ResourceNodeData* getResource(uint64_t nodeId) const;
    std::vector<uint64_t> getResourcesByType(const std::string& type) const;
    std::vector<uint64_t> getResourcesInRange(const NetVec3& center, float radius) const;

    // Territory replication
    void registerTerritory(const TerritoryData& territory);
    void unregisterTerritory(uint64_t territoryId);
    void replicateTerritoryCapture(uint64_t territoryId, int newTeam);
    void replicateCaptureProgress(uint64_t territoryId, float progress);
    void replicateUnitEnterTerritory(uint64_t unitId, uint64_t territoryId);
    void replicateUnitLeaveTerritory(uint64_t unitId, uint64_t territoryId);
    const TerritoryData* getTerritory(uint64_t territoryId) const;
    uint64_t getTerritoryAt(const NetVec3& position) const;
    std::vector<uint64_t> getTerritoriesByTeam(int team) const;
    std::vector<uint64_t> getContestedTerritories() const;

    // Fog of war
    void initializeFogOfWar(uint64_t playerId, int width, int height, float cellSize);
    void updateFogOfWar(uint64_t playerId, const std::vector<NetVec3>& visionSources, float visionRange);
    void revealArea(uint64_t playerId, const NetVec3& center, float radius);
    void hideArea(uint64_t playerId, const NetVec3& center, float radius);
    bool isVisible(uint64_t playerId, const NetVec3& position) const;
    bool isExplored(uint64_t playerId, const NetVec3& position) const;
    bool isEntityVisible(uint64_t playerId, uint64_t entityId) const;
    const FogOfWarData* getFogOfWar(uint64_t playerId) const;
    void replicateFogOfWar(uint64_t playerId);

    // Chunk-based sync
    void requestChunk(int chunkX, int chunkY);
    void sendChunk(int chunkX, int chunkY);
    const WorldChunk* getChunk(int chunkX, int chunkY) const;
    void setChunkSize(float size) { m_chunkSize = size; }
    std::pair<int, int> getChunkCoords(const NetVec3& position) const;

    // Callbacks
    void onBuildingPlaced(BuildingCallback callback);
    void onBuildingDestroyed(BuildingCallback callback);
    void onResourceChanged(ResourceCallback callback);
    void onTerritoryChanged(TerritoryCallback callback);
    void onFogUpdate(FogCallback callback);

    // Settings
    void setFogUpdateRate(float rate) { m_fogUpdateRate = rate; }
    void setTerritoryUpdateRate(float rate) { m_territoryUpdateRate = rate; }

private:
    WorldReplicator();
    ~WorldReplicator();
    WorldReplicator(const WorldReplicator&) = delete;
    WorldReplicator& operator=(const WorldReplicator&) = delete;

    // Internal updates
    void updateTerritories(float deltaTime);
    void updateResources(float deltaTime);
    void updateFog(float deltaTime);

    // Fog helpers
    int getCellIndex(uint64_t playerId, const NetVec3& position) const;
    void setFogCell(uint64_t playerId, int x, int y, FogOfWarCell::State state);

    // Territory helpers
    bool isPointInTerritory(const NetVec3& point, const TerritoryData& territory) const;
    void updateTerritoryControl(uint64_t territoryId);

private:
    bool m_initialized = false;

    // World data
    std::unordered_map<uint64_t, BuildingData> m_buildings;
    std::unordered_map<uint64_t, ResourceNodeData> m_resources;
    std::unordered_map<uint64_t, TerritoryData> m_territories;
    std::unordered_map<uint64_t, FogOfWarData> m_fogOfWar;

    // Chunks
    std::unordered_map<std::pair<int,int>, WorldChunk, PairHash> m_chunks;
    float m_chunkSize = 100.0f;

    // Callbacks
    std::vector<BuildingCallback> m_buildingPlaceCallbacks;
    std::vector<BuildingCallback> m_buildingDestroyCallbacks;
    std::vector<ResourceCallback> m_resourceCallbacks;
    std::vector<TerritoryCallback> m_territoryCallbacks;
    std::vector<FogCallback> m_fogCallbacks;

    // Update rates
    float m_fogUpdateRate = 0.1f;
    float m_territoryUpdateRate = 0.5f;
    float m_fogTimer = 0.0f;
    float m_territoryTimer = 0.0f;

    // Hash for pair keys
    struct PairHash {
        size_t operator()(const std::pair<int,int>& p) const {
            return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 16);
        }
    };
};

} // namespace Replication
} // namespace Network
