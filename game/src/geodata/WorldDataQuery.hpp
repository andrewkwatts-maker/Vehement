#pragma once

#include "GeoTypes.hpp"
#include "GeoDataProvider.hpp"
#include "OSMDataProvider.hpp"
#include "ElevationProvider.hpp"
#include "BiomeClassifier.hpp"
#include "RoadNetwork.hpp"
#include "BuildingFootprints.hpp"
#include "GeoTileCache.hpp"
#include <memory>
#include <functional>
#include <future>

namespace Vehement {
namespace Geo {

/**
 * @brief World data query configuration
 */
struct WorldDataConfig {
    // Data providers
    std::string osmConfigPath;
    std::string elevationConfigPath;
    std::string biomeConfigPath;
    std::string cacheConfigPath;

    // Coordinate system
    GeoCoordinate worldOrigin;              // Origin for coordinate transforms
    float worldScale = 1.0f;                // Scale factor (world units per meter)

    // Default tile zoom level
    int defaultZoom = 16;

    // Processing options
    bool processRoads = true;
    bool processBuildings = true;
    bool processElevation = true;
    bool processBiomes = true;

    // Quality settings
    int elevationResolution = 30;           // Meters per elevation sample
    float roadSimplifyTolerance = 1.0f;     // Road simplification tolerance
    float buildingSimplifyTolerance = 0.5f; // Building simplification tolerance

    /**
     * @brief Load from file
     */
    static WorldDataConfig LoadFromFile(const std::string& path);

    /**
     * @brief Save to file
     */
    void SaveToFile(const std::string& path) const;
};

/**
 * @brief Processed world tile data ready for game use
 */
struct WorldTileData {
    TileID tileId;
    GeoBoundingBox bounds;

    // Transformed game data
    std::vector<ProcessedRoad> roads;
    std::vector<ProcessedBuilding> buildings;
    BiomeData biome;

    // Terrain data
    ElevationGrid elevation;
    std::vector<uint8_t> heightmap;         // 8-bit heightmap texture
    std::vector<uint8_t> normalMap;         // RGB normal map

    // Road network graph (for pathfinding)
    RoadGraph roadGraph;

    // Metadata
    bool isLoaded = false;
    std::string errorMessage;
    int64_t loadTimestamp = 0;
};

/**
 * @brief Query completion callback
 */
using WorldDataCallback = std::function<void(const WorldTileData& data, bool success)>;

/**
 * @brief High-level world data query interface
 *
 * Combines all geo data providers into a unified interface:
 * - Fetches OSM data for roads, buildings, POIs
 * - Fetches elevation data
 * - Classifies biomes
 * - Transforms to game coordinates
 * - Generates meshes and textures
 * - Caches results
 */
class WorldDataQuery {
public:
    WorldDataQuery();
    ~WorldDataQuery();

    /**
     * @brief Initialize the query system
     */
    bool Initialize(const std::string& configPath = "");

    /**
     * @brief Initialize with config
     */
    bool Initialize(const WorldDataConfig& config);

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Set configuration
     */
    void SetConfig(const WorldDataConfig& config);

    /**
     * @brief Get configuration
     */
    const WorldDataConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set world origin for coordinate transforms
     */
    void SetWorldOrigin(const GeoCoordinate& origin);

    /**
     * @brief Get world origin
     */
    const GeoCoordinate& GetWorldOrigin() const { return m_config.worldOrigin; }

    // ==========================================================================
    // Synchronous Queries
    // ==========================================================================

    /**
     * @brief Query all data for a geographic area
     * @param bounds Geographic bounding box
     * @return Processed world data
     */
    WorldTileData QueryArea(const GeoBoundingBox& bounds);

    /**
     * @brief Query all data for a tile
     * @param tile Tile identifier
     * @return Processed world data
     */
    WorldTileData QueryTile(const TileID& tile);

    /**
     * @brief Query all data around a coordinate
     * @param center Center coordinate
     * @param radiusMeters Radius in meters
     * @return Processed world data
     */
    WorldTileData QueryRadius(const GeoCoordinate& center, double radiusMeters);

    /**
     * @brief Query by game world position
     * @param gameX Game world X coordinate
     * @param gameY Game world Y coordinate
     * @param radius Radius in game units
     * @return Processed world data
     */
    WorldTileData QueryByGamePosition(float gameX, float gameY, float radius);

    // ==========================================================================
    // Asynchronous Queries
    // ==========================================================================

    /**
     * @brief Query area asynchronously
     */
    void QueryAreaAsync(const GeoBoundingBox& bounds, WorldDataCallback callback);

    /**
     * @brief Query tile asynchronously
     */
    void QueryTileAsync(const TileID& tile, WorldDataCallback callback);

    /**
     * @brief Query multiple tiles asynchronously
     */
    void QueryTilesAsync(const std::vector<TileID>& tiles,
                          WorldDataCallback callback,
                          GeoProgressCallback progress = nullptr);

    /**
     * @brief Get future for async query
     */
    std::future<WorldTileData> QueryTileFuture(const TileID& tile);

    // ==========================================================================
    // Coordinate Conversion
    // ==========================================================================

    /**
     * @brief Convert geographic coordinate to game position
     */
    glm::vec2 GeoToGame(const GeoCoordinate& coord) const;

    /**
     * @brief Convert game position to geographic coordinate
     */
    GeoCoordinate GameToGeo(const glm::vec2& gamePos) const;

    /**
     * @brief Get tile ID for game position
     */
    TileID GetTileAtGamePosition(float gameX, float gameY) const;

    /**
     * @brief Get tiles covering game area
     */
    std::vector<TileID> GetTilesInGameArea(
        const glm::vec2& min, const glm::vec2& max) const;

    // ==========================================================================
    // Individual Data Access
    // ==========================================================================

    /**
     * @brief Get only roads for area
     */
    std::vector<ProcessedRoad> GetRoads(const GeoBoundingBox& bounds);

    /**
     * @brief Get only buildings for area
     */
    std::vector<ProcessedBuilding> GetBuildings(const GeoBoundingBox& bounds);

    /**
     * @brief Get elevation at coordinate
     */
    float GetElevation(const GeoCoordinate& coord);

    /**
     * @brief Get elevation at game position
     */
    float GetElevationAtGamePos(float gameX, float gameY);

    /**
     * @brief Get biome at coordinate
     */
    BiomeData GetBiome(const GeoCoordinate& coord);

    // ==========================================================================
    // Mesh Generation
    // ==========================================================================

    /**
     * @brief Generate terrain mesh for tile
     */
    TerrainMeshGenerator::TerrainMesh GenerateTerrainMesh(const TileID& tile);

    /**
     * @brief Generate road mesh for tile
     */
    std::pair<std::vector<RoadNetwork::RoadVertex>, std::vector<uint32_t>>
    GenerateRoadMesh(const TileID& tile);

    /**
     * @brief Generate building meshes for tile
     */
    std::vector<BuildingFootprints::BuildingMesh> GenerateBuildingMeshes(const TileID& tile);

    // ==========================================================================
    // Cache Management
    // ==========================================================================

    /**
     * @brief Get cache
     */
    std::shared_ptr<GeoTileCache> GetCache() { return m_cache; }

    /**
     * @brief Prefetch tiles for offline use
     */
    int PrefetchTiles(const std::vector<TileID>& tiles,
                       GeoProgressCallback progress = nullptr);

    /**
     * @brief Prefetch area for offline use
     */
    int PrefetchArea(const GeoBoundingBox& bounds, int minZoom, int maxZoom,
                      GeoProgressCallback progress = nullptr);

    /**
     * @brief Clear cache
     */
    void ClearCache();

    /**
     * @brief Set offline mode
     */
    void SetOfflineMode(bool offline);

    /**
     * @brief Check if offline mode
     */
    bool IsOfflineMode() const;

    // ==========================================================================
    // Providers Access
    // ==========================================================================

    /**
     * @brief Get OSM provider
     */
    OSMDataProvider& GetOSMProvider() { return *m_osmProvider; }

    /**
     * @brief Get elevation provider
     */
    ElevationProvider& GetElevationProvider() { return *m_elevationProvider; }

    /**
     * @brief Get biome classifier
     */
    BiomeClassifier& GetBiomeClassifier() { return *m_biomeClassifier; }

    /**
     * @brief Get road network processor
     */
    RoadNetwork& GetRoadNetwork() { return m_roadNetwork; }

    /**
     * @brief Get building footprints processor
     */
    BuildingFootprints& GetBuildingFootprints() { return m_buildingFootprints; }

    // ==========================================================================
    // Statistics
    // ==========================================================================

    /**
     * @brief Get total API request count
     */
    size_t GetTotalRequestCount() const;

    /**
     * @brief Get cache hit rate
     */
    float GetCacheHitRate() const;

    /**
     * @brief Get total bytes downloaded
     */
    size_t GetBytesDownloaded() const;

private:
    /**
     * @brief Process raw geo data into world tile data
     */
    WorldTileData ProcessGeoData(const GeoTileData& geoData);

    /**
     * @brief Fetch and combine all data for a tile
     */
    GeoTileData FetchAllData(const TileID& tile);

    WorldDataConfig m_config;

    std::unique_ptr<OSMDataProvider> m_osmProvider;
    std::unique_ptr<ElevationProvider> m_elevationProvider;
    std::unique_ptr<BiomeClassifier> m_biomeClassifier;
    std::shared_ptr<GeoTileCache> m_cache;

    RoadNetwork m_roadNetwork;
    BuildingFootprints m_buildingFootprints;

    bool m_initialized = false;
};

/**
 * @brief Streaming world data manager
 *
 * Manages loading/unloading of world tiles based on camera position.
 */
class WorldDataStreamer {
public:
    /**
     * @brief Streaming configuration
     */
    struct Config {
        int loadRadius = 3;                  // Tiles to load around camera
        int unloadRadius = 5;                // Tiles to unload beyond
        int maxConcurrentLoads = 4;          // Max simultaneous loads
        float updateInterval = 0.5f;         // Seconds between update checks
    };

    WorldDataStreamer(WorldDataQuery& query);
    ~WorldDataStreamer();

    /**
     * @brief Set configuration
     */
    void SetConfig(const Config& config) { m_config = config; }

    /**
     * @brief Update streaming based on camera position
     * @param cameraX Camera X in game coordinates
     * @param cameraY Camera Y in game coordinates
     */
    void Update(float cameraX, float cameraY, float deltaTime);

    /**
     * @brief Get loaded tile data
     */
    const WorldTileData* GetTileData(const TileID& tile) const;

    /**
     * @brief Check if tile is loaded
     */
    bool IsTileLoaded(const TileID& tile) const;

    /**
     * @brief Get all loaded tiles
     */
    std::vector<TileID> GetLoadedTiles() const;

    /**
     * @brief Set tile loaded callback
     */
    using TileLoadedCallback = std::function<void(const TileID& tile, const WorldTileData& data)>;
    void SetTileLoadedCallback(TileLoadedCallback callback) { m_onTileLoaded = callback; }

    /**
     * @brief Set tile unloaded callback
     */
    using TileUnloadedCallback = std::function<void(const TileID& tile)>;
    void SetTileUnloadedCallback(TileUnloadedCallback callback) { m_onTileUnloaded = callback; }

private:
    /**
     * @brief Load a tile
     */
    void LoadTile(const TileID& tile);

    /**
     * @brief Unload a tile
     */
    void UnloadTile(const TileID& tile);

    /**
     * @brief Process completed loads
     */
    void ProcessCompletedLoads();

    WorldDataQuery& m_query;
    Config m_config;
    float m_timeSinceUpdate = 0.0f;

    TileID m_currentTile;
    std::unordered_map<std::string, WorldTileData> m_loadedTiles;
    std::unordered_map<std::string, std::future<WorldTileData>> m_pendingLoads;

    TileLoadedCallback m_onTileLoaded;
    TileUnloadedCallback m_onTileUnloaded;

    mutable std::mutex m_mutex;
};

} // namespace Geo
} // namespace Vehement
