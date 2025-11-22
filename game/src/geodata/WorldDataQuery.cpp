#include "WorldDataQuery.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>

namespace Vehement {
namespace Geo {

// =============================================================================
// WorldDataConfig Implementation
// =============================================================================

WorldDataConfig WorldDataConfig::LoadFromFile(const std::string& path) {
    WorldDataConfig config;

    std::ifstream file(path);
    if (!file.is_open()) return config;

    try {
        nlohmann::json json;
        file >> json;

        if (json.contains("osmConfigPath")) config.osmConfigPath = json["osmConfigPath"];
        if (json.contains("elevationConfigPath")) config.elevationConfigPath = json["elevationConfigPath"];
        if (json.contains("biomeConfigPath")) config.biomeConfigPath = json["biomeConfigPath"];
        if (json.contains("cacheConfigPath")) config.cacheConfigPath = json["cacheConfigPath"];

        if (json.contains("worldOrigin")) {
            config.worldOrigin = GeoCoordinate(
                json["worldOrigin"]["latitude"],
                json["worldOrigin"]["longitude"]
            );
        }

        if (json.contains("worldScale")) config.worldScale = json["worldScale"];
        if (json.contains("defaultZoom")) config.defaultZoom = json["defaultZoom"];

        if (json.contains("processRoads")) config.processRoads = json["processRoads"];
        if (json.contains("processBuildings")) config.processBuildings = json["processBuildings"];
        if (json.contains("processElevation")) config.processElevation = json["processElevation"];
        if (json.contains("processBiomes")) config.processBiomes = json["processBiomes"];

        if (json.contains("elevationResolution")) config.elevationResolution = json["elevationResolution"];
        if (json.contains("roadSimplifyTolerance")) config.roadSimplifyTolerance = json["roadSimplifyTolerance"];
        if (json.contains("buildingSimplifyTolerance")) config.buildingSimplifyTolerance = json["buildingSimplifyTolerance"];

    } catch (...) {}

    return config;
}

void WorldDataConfig::SaveToFile(const std::string& path) const {
    nlohmann::json json;

    json["osmConfigPath"] = osmConfigPath;
    json["elevationConfigPath"] = elevationConfigPath;
    json["biomeConfigPath"] = biomeConfigPath;
    json["cacheConfigPath"] = cacheConfigPath;

    json["worldOrigin"] = {
        {"latitude", worldOrigin.latitude},
        {"longitude", worldOrigin.longitude}
    };

    json["worldScale"] = worldScale;
    json["defaultZoom"] = defaultZoom;

    json["processRoads"] = processRoads;
    json["processBuildings"] = processBuildings;
    json["processElevation"] = processElevation;
    json["processBiomes"] = processBiomes;

    json["elevationResolution"] = elevationResolution;
    json["roadSimplifyTolerance"] = roadSimplifyTolerance;
    json["buildingSimplifyTolerance"] = buildingSimplifyTolerance;

    std::ofstream file(path);
    if (file.is_open()) {
        file << json.dump(2);
    }
}

// =============================================================================
// WorldDataQuery Implementation
// =============================================================================

WorldDataQuery::WorldDataQuery() = default;

WorldDataQuery::~WorldDataQuery() {
    Shutdown();
}

bool WorldDataQuery::Initialize(const std::string& configPath) {
    if (!configPath.empty()) {
        m_config = WorldDataConfig::LoadFromFile(configPath);
    }
    return Initialize(m_config);
}

bool WorldDataQuery::Initialize(const WorldDataConfig& config) {
    m_config = config;

    // Initialize cache
    m_cache = std::make_shared<GeoTileCache>();
    if (!config.cacheConfigPath.empty()) {
        m_cache->Initialize(config.cacheConfigPath);
    } else {
        m_cache->Initialize();
    }

    // Initialize OSM provider
    m_osmProvider = std::make_unique<OSMDataProvider>();
    m_osmProvider->Initialize(config.osmConfigPath);
    m_osmProvider->SetCache(m_cache);

    // Initialize elevation provider
    m_elevationProvider = std::make_unique<ElevationProvider>();
    m_elevationProvider->Initialize(config.elevationConfigPath);
    m_elevationProvider->SetCache(m_cache);

    // Initialize biome classifier
    m_biomeClassifier = std::make_unique<BiomeClassifier>();
    m_biomeClassifier->Initialize(config.biomeConfigPath);

    // Set up coordinate transforms
    m_roadNetwork.SetDefaultTransform(m_config.worldOrigin, m_config.worldScale);
    m_buildingFootprints.SetDefaultTransform(m_config.worldOrigin, m_config.worldScale);

    m_initialized = true;
    return true;
}

void WorldDataQuery::Shutdown() {
    if (m_osmProvider) {
        m_osmProvider->Shutdown();
    }
    if (m_elevationProvider) {
        m_elevationProvider->Shutdown();
    }
    if (m_biomeClassifier) {
        m_biomeClassifier->Shutdown();
    }
    if (m_cache) {
        m_cache->Shutdown();
    }

    m_roadNetwork.Clear();
    m_buildingFootprints.Clear();

    m_initialized = false;
}

void WorldDataQuery::SetConfig(const WorldDataConfig& config) {
    m_config = config;
    m_roadNetwork.SetDefaultTransform(m_config.worldOrigin, m_config.worldScale);
    m_buildingFootprints.SetDefaultTransform(m_config.worldOrigin, m_config.worldScale);
}

void WorldDataQuery::SetWorldOrigin(const GeoCoordinate& origin) {
    m_config.worldOrigin = origin;
    m_roadNetwork.SetDefaultTransform(origin, m_config.worldScale);
    m_buildingFootprints.SetDefaultTransform(origin, m_config.worldScale);
}

WorldTileData WorldDataQuery::QueryArea(const GeoBoundingBox& bounds) {
    GeoQueryOptions options;
    options.fetchRoads = m_config.processRoads;
    options.fetchBuildings = m_config.processBuildings;
    options.fetchLandUse = true;
    options.fetchPOIs = true;
    options.fetchWater = true;
    options.fetchElevation = m_config.processElevation;

    GeoTileData geoData = m_osmProvider->Query(bounds, options);

    // Fetch elevation separately if needed
    if (m_config.processElevation && geoData.elevation.width == 0) {
        geoData.elevation = m_elevationProvider->GetElevationGrid(
            bounds, m_config.elevationResolution);
    }

    return ProcessGeoData(geoData);
}

WorldTileData WorldDataQuery::QueryTile(const TileID& tile) {
    GeoTileData geoData = FetchAllData(tile);
    return ProcessGeoData(geoData);
}

WorldTileData WorldDataQuery::QueryRadius(const GeoCoordinate& center, double radiusMeters) {
    GeoBoundingBox bounds = GeoBoundingBox::FromCenterRadius(center, radiusMeters);
    return QueryArea(bounds);
}

WorldTileData WorldDataQuery::QueryByGamePosition(float gameX, float gameY, float radius) {
    GeoCoordinate center = GameToGeo(glm::vec2(gameX, gameY));
    double radiusMeters = radius / m_config.worldScale;
    return QueryRadius(center, radiusMeters);
}

void WorldDataQuery::QueryAreaAsync(const GeoBoundingBox& bounds, WorldDataCallback callback) {
    m_osmProvider->QueryAsync(bounds,
        [this, callback](const GeoTileData& data, bool success, const std::string& error) {
            if (success) {
                WorldTileData worldData = ProcessGeoData(data);
                callback(worldData, true);
            } else {
                WorldTileData worldData;
                worldData.errorMessage = error;
                callback(worldData, false);
            }
        });
}

void WorldDataQuery::QueryTileAsync(const TileID& tile, WorldDataCallback callback) {
    m_osmProvider->QueryTileAsync(tile,
        [this, callback](const GeoTileData& data, bool success, const std::string& error) {
            if (success) {
                WorldTileData worldData = ProcessGeoData(data);
                callback(worldData, true);
            } else {
                WorldTileData worldData;
                worldData.errorMessage = error;
                callback(worldData, false);
            }
        });
}

void WorldDataQuery::QueryTilesAsync(const std::vector<TileID>& tiles,
                                       WorldDataCallback callback,
                                       GeoProgressCallback progress) {
    m_osmProvider->QueryTilesAsync(tiles,
        [this, callback](const GeoTileData& data, bool success, const std::string& error) {
            if (success) {
                WorldTileData worldData = ProcessGeoData(data);
                callback(worldData, true);
            } else {
                WorldTileData worldData;
                worldData.errorMessage = error;
                callback(worldData, false);
            }
        }, progress);
}

std::future<WorldTileData> WorldDataQuery::QueryTileFuture(const TileID& tile) {
    auto promise = std::make_shared<std::promise<WorldTileData>>();
    auto future = promise->get_future();

    QueryTileAsync(tile, [promise](const WorldTileData& data, bool success) {
        promise->set_value(data);
    });

    return future;
}

glm::vec2 WorldDataQuery::GeoToGame(const GeoCoordinate& coord) const {
    double dx = (coord.longitude - m_config.worldOrigin.longitude) *
                std::cos(DegToRad(m_config.worldOrigin.latitude)) * 111320.0;
    double dy = (coord.latitude - m_config.worldOrigin.latitude) * 110540.0;

    return glm::vec2(
        static_cast<float>(dx * m_config.worldScale),
        static_cast<float>(dy * m_config.worldScale)
    );
}

GeoCoordinate WorldDataQuery::GameToGeo(const glm::vec2& gamePos) const {
    double metersX = gamePos.x / m_config.worldScale;
    double metersY = gamePos.y / m_config.worldScale;

    double lon = m_config.worldOrigin.longitude +
                 metersX / (111320.0 * std::cos(DegToRad(m_config.worldOrigin.latitude)));
    double lat = m_config.worldOrigin.latitude + metersY / 110540.0;

    return GeoCoordinate(lat, lon);
}

TileID WorldDataQuery::GetTileAtGamePosition(float gameX, float gameY) const {
    GeoCoordinate coord = GameToGeo(glm::vec2(gameX, gameY));
    return TileID::FromCoordinate(coord, m_config.defaultZoom);
}

std::vector<TileID> WorldDataQuery::GetTilesInGameArea(
    const glm::vec2& min, const glm::vec2& max) const {

    std::vector<TileID> tiles;

    GeoCoordinate minCoord = GameToGeo(min);
    GeoCoordinate maxCoord = GameToGeo(max);

    glm::ivec2 minTile = minCoord.ToTileXY(m_config.defaultZoom);
    glm::ivec2 maxTile = maxCoord.ToTileXY(m_config.defaultZoom);

    // Ensure proper ordering
    int minX = std::min(minTile.x, maxTile.x);
    int maxX = std::max(minTile.x, maxTile.x);
    int minY = std::min(minTile.y, maxTile.y);
    int maxY = std::max(minTile.y, maxTile.y);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            tiles.emplace_back(x, y, m_config.defaultZoom);
        }
    }

    return tiles;
}

std::vector<ProcessedRoad> WorldDataQuery::GetRoads(const GeoBoundingBox& bounds) {
    GeoQueryOptions options;
    options.fetchRoads = true;
    options.fetchBuildings = false;
    options.fetchElevation = false;

    GeoTileData data = m_osmProvider->Query(bounds, options);

    RoadNetwork network;
    network.SetDefaultTransform(m_config.worldOrigin, m_config.worldScale);
    network.ProcessAll(data.roads);

    return network.GetRoads();
}

std::vector<ProcessedBuilding> WorldDataQuery::GetBuildings(const GeoBoundingBox& bounds) {
    GeoQueryOptions options;
    options.fetchRoads = false;
    options.fetchBuildings = true;
    options.fetchElevation = false;

    GeoTileData data = m_osmProvider->Query(bounds, options);

    BuildingFootprints footprints;
    footprints.SetDefaultTransform(m_config.worldOrigin, m_config.worldScale);
    footprints.ProcessAll(data.buildings);

    return footprints.GetBuildings();
}

float WorldDataQuery::GetElevation(const GeoCoordinate& coord) {
    return m_elevationProvider->GetElevation(coord);
}

float WorldDataQuery::GetElevationAtGamePos(float gameX, float gameY) {
    GeoCoordinate coord = GameToGeo(glm::vec2(gameX, gameY));
    return GetElevation(coord) * m_config.worldScale;
}

BiomeData WorldDataQuery::GetBiome(const GeoCoordinate& coord) {
    return m_biomeClassifier->ClassifyBiome(coord);
}

TerrainMeshGenerator::TerrainMesh WorldDataQuery::GenerateTerrainMesh(const TileID& tile) {
    ElevationGrid grid = m_elevationProvider->GetElevationGridForTile(
        tile, m_config.elevationResolution);

    return TerrainMeshGenerator::GenerateMesh(grid, m_config.worldScale);
}

std::pair<std::vector<RoadNetwork::RoadVertex>, std::vector<uint32_t>>
WorldDataQuery::GenerateRoadMesh(const TileID& tile) {

    WorldTileData data = QueryTile(tile);

    glm::vec2 minBounds(std::numeric_limits<float>::max());
    glm::vec2 maxBounds(std::numeric_limits<float>::lowest());

    for (const auto& road : data.roads) {
        auto [rMin, rMax] = road.GetBounds();
        minBounds = glm::min(minBounds, rMin);
        maxBounds = glm::max(maxBounds, rMax);
    }

    RoadNetwork network;
    network.SetDefaultTransform(m_config.worldOrigin, m_config.worldScale);

    // Reconstruct GeoRoads from processed roads (simplified)
    // In practice, we'd cache the original data
    return network.GenerateRoadMesh(minBounds, maxBounds);
}

std::vector<BuildingFootprints::BuildingMesh>
WorldDataQuery::GenerateBuildingMeshes(const TileID& tile) {

    WorldTileData data = QueryTile(tile);

    glm::vec2 minBounds(std::numeric_limits<float>::max());
    glm::vec2 maxBounds(std::numeric_limits<float>::lowest());

    for (const auto& building : data.buildings) {
        minBounds = glm::min(minBounds, building.boundsMin);
        maxBounds = glm::max(maxBounds, building.boundsMax);
    }

    BuildingFootprints footprints;
    footprints.SetDefaultTransform(m_config.worldOrigin, m_config.worldScale);

    return footprints.GenerateMeshesInBounds(minBounds, maxBounds);
}

int WorldDataQuery::PrefetchTiles(const std::vector<TileID>& tiles,
                                    GeoProgressCallback progress) {
    return m_osmProvider->PrefetchTiles(tiles, progress);
}

int WorldDataQuery::PrefetchArea(const GeoBoundingBox& bounds, int minZoom, int maxZoom,
                                   GeoProgressCallback progress) {
    std::vector<TileID> tiles;

    for (int zoom = minZoom; zoom <= maxZoom; ++zoom) {
        glm::ivec2 minTile = bounds.min.ToTileXY(zoom);
        glm::ivec2 maxTile = bounds.max.ToTileXY(zoom);

        int minX = std::min(minTile.x, maxTile.x);
        int maxX = std::max(minTile.x, maxTile.x);
        int minY = std::min(minTile.y, maxTile.y);
        int maxY = std::max(minTile.y, maxTile.y);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                tiles.emplace_back(x, y, zoom);
            }
        }
    }

    return PrefetchTiles(tiles, progress);
}

void WorldDataQuery::ClearCache() {
    if (m_cache) {
        m_cache->Clear();
    }
}

void WorldDataQuery::SetOfflineMode(bool offline) {
    if (m_osmProvider) {
        m_osmProvider->SetOfflineMode(offline);
    }
}

bool WorldDataQuery::IsOfflineMode() const {
    return m_osmProvider ? m_osmProvider->IsOfflineMode() : false;
}

size_t WorldDataQuery::GetTotalRequestCount() const {
    size_t count = 0;
    if (m_osmProvider) count += m_osmProvider->GetRequestCount();
    if (m_elevationProvider) count += m_elevationProvider->GetRequestCount();
    return count;
}

float WorldDataQuery::GetCacheHitRate() const {
    return m_cache ? m_cache->GetHitRate() : 0.0f;
}

size_t WorldDataQuery::GetBytesDownloaded() const {
    return m_osmProvider ? m_osmProvider->GetBytesDownloaded() : 0;
}

WorldTileData WorldDataQuery::ProcessGeoData(const GeoTileData& geoData) {
    WorldTileData worldData;
    worldData.tileId = geoData.tileId;
    worldData.bounds = geoData.bounds;

    auto now = std::chrono::system_clock::now();
    worldData.loadTimestamp = std::chrono::system_clock::to_time_t(now);

    // Process roads
    if (m_config.processRoads && !geoData.roads.empty()) {
        m_roadNetwork.Clear();
        m_roadNetwork.ProcessAll(geoData.roads);
        worldData.roads = m_roadNetwork.GetRoads();
        worldData.roadGraph = m_roadNetwork.GetGraph();
    }

    // Process buildings
    if (m_config.processBuildings && !geoData.buildings.empty()) {
        m_buildingFootprints.Clear();
        m_buildingFootprints.ProcessAll(geoData.buildings);
        worldData.buildings = m_buildingFootprints.GetBuildings();
    }

    // Copy elevation data
    if (m_config.processElevation && geoData.elevation.width > 0) {
        worldData.elevation = geoData.elevation;
        worldData.heightmap = m_elevationProvider->GenerateHeightmap(geoData.elevation);
        worldData.normalMap = m_elevationProvider->GenerateNormalMap(geoData.elevation);
    }

    // Classify biome
    if (m_config.processBiomes) {
        worldData.biome = m_biomeClassifier->ClassifyTile(geoData);
    }

    worldData.isLoaded = true;
    return worldData;
}

GeoTileData WorldDataQuery::FetchAllData(const TileID& tile) {
    GeoQueryOptions options;
    options.fetchRoads = m_config.processRoads;
    options.fetchBuildings = m_config.processBuildings;
    options.fetchLandUse = true;
    options.fetchPOIs = true;
    options.fetchWater = true;

    GeoTileData data = m_osmProvider->QueryTile(tile, options);

    // Fetch elevation
    if (m_config.processElevation) {
        data.elevation = m_elevationProvider->GetElevationGridForTile(
            tile, m_config.elevationResolution);
    }

    return data;
}

// =============================================================================
// WorldDataStreamer Implementation
// =============================================================================

WorldDataStreamer::WorldDataStreamer(WorldDataQuery& query)
    : m_query(query) {
}

WorldDataStreamer::~WorldDataStreamer() = default;

void WorldDataStreamer::Update(float cameraX, float cameraY, float deltaTime) {
    m_timeSinceUpdate += deltaTime;

    if (m_timeSinceUpdate < m_config.updateInterval) {
        ProcessCompletedLoads();
        return;
    }

    m_timeSinceUpdate = 0.0f;

    // Get current tile
    TileID currentTile = m_query.GetTileAtGamePosition(cameraX, cameraY);

    // Determine tiles to load
    std::vector<TileID> tilesToLoad;
    for (int dy = -m_config.loadRadius; dy <= m_config.loadRadius; ++dy) {
        for (int dx = -m_config.loadRadius; dx <= m_config.loadRadius; ++dx) {
            TileID tile(currentTile.x + dx, currentTile.y + dy, currentTile.zoom);
            std::string key = tile.ToKey();

            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_loadedTiles.find(key) == m_loadedTiles.end() &&
                m_pendingLoads.find(key) == m_pendingLoads.end()) {
                tilesToLoad.push_back(tile);
            }
        }
    }

    // Limit concurrent loads
    int loadCount = 0;
    for (const auto& tile : tilesToLoad) {
        if (loadCount >= m_config.maxConcurrentLoads) break;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (static_cast<int>(m_pendingLoads.size()) >= m_config.maxConcurrentLoads) break;

        LoadTile(tile);
        ++loadCount;
    }

    // Unload distant tiles
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<std::string> toUnload;
        for (const auto& [key, data] : m_loadedTiles) {
            int dx = std::abs(data.tileId.x - currentTile.x);
            int dy = std::abs(data.tileId.y - currentTile.y);

            if (dx > m_config.unloadRadius || dy > m_config.unloadRadius) {
                toUnload.push_back(key);
            }
        }

        for (const auto& key : toUnload) {
            TileID tile = TileID::FromKey(key);
            UnloadTile(tile);
        }
    }

    ProcessCompletedLoads();
    m_currentTile = currentTile;
}

const WorldTileData* WorldDataStreamer::GetTileData(const TileID& tile) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_loadedTiles.find(tile.ToKey());
    return it != m_loadedTiles.end() ? &it->second : nullptr;
}

bool WorldDataStreamer::IsTileLoaded(const TileID& tile) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loadedTiles.find(tile.ToKey()) != m_loadedTiles.end();
}

std::vector<TileID> WorldDataStreamer::GetLoadedTiles() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TileID> tiles;
    for (const auto& [key, data] : m_loadedTiles) {
        tiles.push_back(data.tileId);
    }
    return tiles;
}

void WorldDataStreamer::LoadTile(const TileID& tile) {
    std::string key = tile.ToKey();

    m_pendingLoads[key] = m_query.QueryTileFuture(tile);
}

void WorldDataStreamer::UnloadTile(const TileID& tile) {
    std::string key = tile.ToKey();

    m_loadedTiles.erase(key);

    if (m_onTileUnloaded) {
        m_onTileUnloaded(tile);
    }
}

void WorldDataStreamer::ProcessCompletedLoads() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> completed;

    for (auto& [key, future] : m_pendingLoads) {
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                WorldTileData data = future.get();
                m_loadedTiles[key] = std::move(data);

                if (m_onTileLoaded) {
                    m_onTileLoaded(m_loadedTiles[key].tileId, m_loadedTiles[key]);
                }
            } catch (...) {
                // Load failed
            }

            completed.push_back(key);
        }
    }

    for (const auto& key : completed) {
        m_pendingLoads.erase(key);
    }
}

} // namespace Geo
} // namespace Vehement
