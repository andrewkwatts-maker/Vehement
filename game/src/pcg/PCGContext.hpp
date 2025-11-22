#pragma once

#include "../world/Tile.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace Vehement {

// Forward declarations
class TileMap;
class Entity;

/**
 * @brief Biome types for terrain classification
 */
enum class BiomeType : uint8_t {
    Unknown = 0,
    Urban,
    Suburban,
    Rural,
    Forest,
    Desert,
    Grassland,
    Wetland,
    Mountain,
    Water,
    Industrial,
    Commercial,
    Residential,
    Park,
    COUNT
};

/**
 * @brief Get biome type name
 */
inline const char* GetBiomeTypeName(BiomeType type) {
    switch (type) {
        case BiomeType::Urban: return "urban";
        case BiomeType::Suburban: return "suburban";
        case BiomeType::Rural: return "rural";
        case BiomeType::Forest: return "forest";
        case BiomeType::Desert: return "desert";
        case BiomeType::Grassland: return "grassland";
        case BiomeType::Wetland: return "wetland";
        case BiomeType::Mountain: return "mountain";
        case BiomeType::Water: return "water";
        case BiomeType::Industrial: return "industrial";
        case BiomeType::Commercial: return "commercial";
        case BiomeType::Residential: return "residential";
        case BiomeType::Park: return "park";
        default: return "unknown";
    }
}

/**
 * @brief Road type classification
 */
enum class RoadType : uint8_t {
    None = 0,
    Highway,
    MainRoad,
    SecondaryRoad,
    ResidentialStreet,
    Path,
    COUNT
};

/**
 * @brief Get road type name
 */
inline const char* GetRoadTypeName(RoadType type) {
    switch (type) {
        case RoadType::Highway: return "highway";
        case RoadType::MainRoad: return "main_road";
        case RoadType::SecondaryRoad: return "secondary_road";
        case RoadType::ResidentialStreet: return "residential";
        case RoadType::Path: return "path";
        default: return "none";
    }
}

/**
 * @brief Building type classification
 */
enum class BuildingType : uint8_t {
    None = 0,
    House,
    Apartment,
    Office,
    Shop,
    Factory,
    Warehouse,
    Hospital,
    School,
    Church,
    Government,
    Garage,
    Shed,
    COUNT
};

/**
 * @brief Geographic road data from real-world sources
 */
struct GeoRoad {
    std::vector<glm::vec2> points;      // Road path points
    RoadType type = RoadType::None;
    std::string name;
    float width = 1.0f;                 // Road width in meters
    int lanes = 1;
    bool oneWay = false;
    bool hasSidewalk = true;
};

/**
 * @brief Geographic building data from real-world sources
 */
struct GeoBuilding {
    std::vector<glm::vec2> footprint;   // Building polygon points
    BuildingType type = BuildingType::None;
    std::string name;
    std::string address;
    float height = 0.0f;                // Building height in meters
    int floors = 1;
    int yearBuilt = 0;
};

/**
 * @brief Geographic tile data containing real-world information
 */
struct GeoTileData {
    glm::dvec2 latLon{0.0, 0.0};        // GPS coordinates
    float elevation = 0.0f;             // Elevation in meters
    BiomeType biome = BiomeType::Unknown;
    float populationDensity = 0.0f;     // People per km^2

    // Raw OpenStreetMap-style tags
    std::unordered_map<std::string, std::string> tags;

    // Extracted features
    std::vector<GeoRoad> roads;
    std::vector<GeoBuilding> buildings;

    // Water features
    bool hasWater = false;
    float waterDepth = 0.0f;

    // Vegetation data
    float treeDensity = 0.0f;           // Trees per hectare
    std::string vegetationType;
};

/**
 * @brief Entity spawn definition for PCG output
 */
struct PCGEntitySpawn {
    std::string entityType;             // Entity type identifier
    glm::vec3 position{0.0f};
    float rotation = 0.0f;
    std::unordered_map<std::string, std::string> properties;
};

/**
 * @brief Foliage placement definition
 */
struct PCGFoliage {
    std::string foliageType;            // Tree/plant type
    glm::vec3 position{0.0f};
    float scale = 1.0f;
    float rotation = 0.0f;
};

/**
 * @brief Context passed to PCG generators and Python scripts
 *
 * Provides:
 * - Access to real-world geographic data
 * - Tile manipulation functions
 * - Random number generation (seeded)
 * - Noise functions for procedural generation
 * - Output buffers for tiles, entities, foliage
 */
class PCGContext {
public:
    /**
     * @brief Construct PCG context for a region
     * @param width Region width in tiles
     * @param height Region height in tiles
     * @param seed Random seed for reproducibility
     */
    PCGContext(int width, int height, uint64_t seed = 0);
    ~PCGContext();

    // Non-copyable
    PCGContext(const PCGContext&) = delete;
    PCGContext& operator=(const PCGContext&) = delete;
    PCGContext(PCGContext&&) noexcept = default;
    PCGContext& operator=(PCGContext&&) noexcept = default;

    // ========== Dimensions ==========

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // ========== World Coordinates ==========

    /**
     * @brief Set the world tile offset for this context
     */
    void SetWorldOffset(int worldX, int worldY);
    int GetWorldX() const { return m_worldX; }
    int GetWorldY() const { return m_worldY; }

    /**
     * @brief Convert local to world coordinates
     */
    glm::ivec2 LocalToWorld(int localX, int localY) const;

    /**
     * @brief Convert world to local coordinates
     */
    glm::ivec2 WorldToLocal(int worldX, int worldY) const;

    // ========== Random Number Generation ==========

    /**
     * @brief Get random float [0, 1)
     */
    float Random();

    /**
     * @brief Get random float [min, max)
     */
    float Random(float min, float max);

    /**
     * @brief Get random integer [min, max]
     */
    int RandomInt(int min, int max);

    /**
     * @brief Get random boolean with given probability of true
     */
    bool RandomBool(float probability = 0.5f);

    /**
     * @brief Get the random generator
     */
    std::mt19937& GetRNG() { return m_rng; }

    /**
     * @brief Reset random generator to initial seed
     */
    void ResetRNG();

    uint64_t GetSeed() const { return m_seed; }

    // ========== Noise Functions ==========

    /**
     * @brief Perlin noise at position
     * @param x X coordinate
     * @param y Y coordinate
     * @param frequency Noise frequency
     * @param octaves Number of octaves for fractal noise
     * @return Noise value [-1, 1]
     */
    float PerlinNoise(float x, float y, float frequency = 1.0f, int octaves = 1);

    /**
     * @brief Simplex noise at position
     */
    float SimplexNoise(float x, float y, float frequency = 1.0f, int octaves = 1);

    /**
     * @brief Worley (cellular) noise at position
     * @return Distance to nearest cell point [0, 1]
     */
    float WorleyNoise(float x, float y, float frequency = 1.0f);

    /**
     * @brief Ridged noise for terrain features
     */
    float RidgedNoise(float x, float y, float frequency = 1.0f, int octaves = 4);

    /**
     * @brief Billow noise (absolute value of perlin)
     */
    float BillowNoise(float x, float y, float frequency = 1.0f, int octaves = 4);

    // ========== Geographic Data Access ==========

    /**
     * @brief Set geographic data for this region
     */
    void SetGeoData(std::shared_ptr<GeoTileData> geoData);

    /**
     * @brief Get elevation at local position
     */
    float GetElevation(int x, int y) const;

    /**
     * @brief Get biome at local position
     */
    BiomeType GetBiome(int x, int y) const;

    /**
     * @brief Get biome name at position
     */
    std::string GetBiomeName(int x, int y) const;

    /**
     * @brief Check if position is water
     */
    bool IsWater(int x, int y) const;

    /**
     * @brief Check if position is on a road
     */
    bool IsRoad(int x, int y) const;

    /**
     * @brief Get road type at position
     */
    RoadType GetRoadType(int x, int y) const;

    /**
     * @brief Get road type name at position
     */
    std::string GetRoadTypeName(int x, int y) const;

    /**
     * @brief Get building at position (if any)
     */
    const GeoBuilding* GetBuilding(int x, int y) const;

    /**
     * @brief Query nearby roads
     */
    std::vector<const GeoRoad*> GetNearbyRoads(int x, int y, float radius) const;

    /**
     * @brief Query nearby buildings
     */
    std::vector<const GeoBuilding*> GetNearbyBuildings(int x, int y, float radius) const;

    /**
     * @brief Get population density at position
     */
    float GetPopulationDensity(int x, int y) const;

    /**
     * @brief Get tree density at position
     */
    float GetTreeDensity(int x, int y) const;

    // ========== Tile Output ==========

    /**
     * @brief Set tile at position
     */
    void SetTile(int x, int y, TileType type);

    /**
     * @brief Set tile by name (for Python scripts)
     */
    void SetTile(int x, int y, const std::string& typeName);

    /**
     * @brief Get tile at position
     */
    TileType GetTile(int x, int y) const;

    /**
     * @brief Set elevation at position (for height map)
     */
    void SetElevation(int x, int y, float elevation);

    /**
     * @brief Set wall at position
     */
    void SetWall(int x, int y, TileType type, float height);

    /**
     * @brief Check if position is walkable in current output
     */
    bool IsWalkable(int x, int y) const;

    /**
     * @brief Clear tile at position
     */
    void ClearTile(int x, int y);

    /**
     * @brief Fill rectangle with tile type
     */
    void FillRect(int x, int y, int width, int height, TileType type);

    /**
     * @brief Draw line of tiles
     */
    void DrawLine(int x1, int y1, int x2, int y2, TileType type);

    /**
     * @brief Get the output tile buffer
     */
    const std::vector<Tile>& GetTiles() const { return m_tiles; }
    std::vector<Tile>& GetTiles() { return m_tiles; }

    /**
     * @brief Get elevation buffer
     */
    const std::vector<float>& GetElevations() const { return m_elevations; }
    std::vector<float>& GetElevations() { return m_elevations; }

    // ========== Foliage Output ==========

    /**
     * @brief Spawn foliage at position
     */
    void SpawnFoliage(int x, int y, const std::string& foliageType, float scale = 1.0f);

    /**
     * @brief Spawn foliage at world position
     */
    void SpawnFoliageWorld(float worldX, float worldY, const std::string& foliageType, float scale = 1.0f);

    /**
     * @brief Get foliage spawns
     */
    const std::vector<PCGFoliage>& GetFoliageSpawns() const { return m_foliageSpawns; }

    /**
     * @brief Clear foliage spawns
     */
    void ClearFoliageSpawns();

    // ========== Entity Output ==========

    /**
     * @brief Spawn entity at position
     */
    void SpawnEntity(int x, int y, const std::string& entityType);

    /**
     * @brief Spawn entity with properties
     */
    void SpawnEntity(int x, int y, const std::string& entityType,
                     const std::unordered_map<std::string, std::string>& properties);

    /**
     * @brief Spawn entity at world position
     */
    void SpawnEntityWorld(float worldX, float worldY, const std::string& entityType);

    /**
     * @brief Get entity spawns
     */
    const std::vector<PCGEntitySpawn>& GetEntitySpawns() const { return m_entitySpawns; }

    /**
     * @brief Clear entity spawns
     */
    void ClearEntitySpawns();

    // ========== Utility Functions ==========

    /**
     * @brief Check if position is in bounds
     */
    bool InBounds(int x, int y) const;

    /**
     * @brief Clamp position to bounds
     */
    glm::ivec2 Clamp(int x, int y) const;

    /**
     * @brief Get distance between two points
     */
    float Distance(int x1, int y1, int x2, int y2) const;

    /**
     * @brief Calculate path between two points (A* pathfinding)
     */
    std::vector<glm::ivec2> FindPath(int startX, int startY, int endX, int endY) const;

    /**
     * @brief Check line of sight between two points
     */
    bool HasLineOfSight(int x1, int y1, int x2, int y2) const;

    // ========== Context Metadata ==========

    /**
     * @brief Set custom data by key
     */
    void SetData(const std::string& key, const std::string& value);

    /**
     * @brief Get custom data by key
     */
    std::string GetData(const std::string& key) const;

    /**
     * @brief Check if custom data exists
     */
    bool HasData(const std::string& key) const;

    /**
     * @brief Get all custom data
     */
    const std::unordered_map<std::string, std::string>& GetAllData() const { return m_customData; }

    // ========== Stage Communication ==========

    /**
     * @brief Mark a position as occupied (for later stages)
     */
    void MarkOccupied(int x, int y);

    /**
     * @brief Check if position is occupied
     */
    bool IsOccupied(int x, int y) const;

    /**
     * @brief Clear occupied markers
     */
    void ClearOccupied();

    /**
     * @brief Mark a region with a zone type
     */
    void MarkZone(int x, int y, int width, int height, const std::string& zoneType);

    /**
     * @brief Get zone type at position
     */
    std::string GetZone(int x, int y) const;

private:
    int m_width;
    int m_height;
    int m_worldX = 0;
    int m_worldY = 0;

    uint64_t m_seed;
    std::mt19937 m_rng;

    // Geographic data
    std::shared_ptr<GeoTileData> m_geoData;

    // Output buffers
    std::vector<Tile> m_tiles;
    std::vector<float> m_elevations;
    std::vector<PCGFoliage> m_foliageSpawns;
    std::vector<PCGEntitySpawn> m_entitySpawns;

    // Occupancy grid for stage communication
    std::vector<bool> m_occupied;
    std::vector<std::string> m_zones;

    // Custom data
    std::unordered_map<std::string, std::string> m_customData;

    // Road/building lookup cache
    mutable std::vector<std::vector<RoadType>> m_roadCache;
    mutable std::vector<std::vector<int>> m_buildingCache;
    mutable bool m_cacheValid = false;

    void BuildCache() const;
    int GetIndex(int x, int y) const { return y * m_width + x; }

    // Tile name lookup
    static TileType TileTypeFromName(const std::string& name);
};

} // namespace Vehement
