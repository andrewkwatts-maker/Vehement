#include "OSMDataProvider.hpp"
#include "GeoTileCache.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

namespace Vehement {
namespace Geo {

// =============================================================================
// OSMConfig Implementation
// =============================================================================

OSMConfig OSMConfig::LoadFromFile(const std::string& path) {
    OSMConfig config;

    std::ifstream file(path);
    if (!file.is_open()) return config;

    try {
        nlohmann::json json;
        file >> json;

        if (json.contains("overpassEndpoint")) config.overpassEndpoint = json["overpassEndpoint"];
        if (json.contains("nominatimEndpoint")) config.nominatimEndpoint = json["nominatimEndpoint"];
        if (json.contains("requestsPerSecond")) config.requestsPerSecond = json["requestsPerSecond"];
        if (json.contains("burstSize")) config.burstSize = json["burstSize"];
        if (json.contains("queryTimeout")) config.queryTimeout = json["queryTimeout"];
        if (json.contains("httpTimeout")) config.httpTimeout = json["httpTimeout"];
        if (json.contains("defaultZoom")) config.defaultZoom = json["defaultZoom"];
        if (json.contains("fetchRelations")) config.fetchRelations = json["fetchRelations"];
        if (json.contains("fetchMetadata")) config.fetchMetadata = json["fetchMetadata"];
        if (json.contains("useJson")) config.useJson = json["useJson"];

    } catch (...) {
        // Return defaults on error
    }

    return config;
}

void OSMConfig::SaveToFile(const std::string& path) const {
    nlohmann::json json;

    json["overpassEndpoint"] = overpassEndpoint;
    json["nominatimEndpoint"] = nominatimEndpoint;
    json["requestsPerSecond"] = requestsPerSecond;
    json["burstSize"] = burstSize;
    json["queryTimeout"] = queryTimeout;
    json["httpTimeout"] = httpTimeout;
    json["defaultZoom"] = defaultZoom;
    json["fetchRelations"] = fetchRelations;
    json["fetchMetadata"] = fetchMetadata;
    json["useJson"] = useJson;

    std::ofstream file(path);
    if (file.is_open()) {
        file << json.dump(2);
    }
}

// =============================================================================
// OSMDataProvider Implementation
// =============================================================================

OSMDataProvider::OSMDataProvider() {
    m_httpClient = std::make_unique<CurlHttpClient>();
}

OSMDataProvider::~OSMDataProvider() {
    Shutdown();
}

bool OSMDataProvider::Initialize(const std::string& configPath) {
    if (!configPath.empty()) {
        m_config = OSMConfig::LoadFromFile(configPath);
    }

    m_httpClient->SetTimeout(m_config.httpTimeout);
    m_httpClient->SetUserAgent("Vehement2-GeoData/1.0 (game engine)");

    SetRateLimit(m_config.requestsPerSecond);

    // Start worker threads
    m_running = true;
    for (int i = 0; i < m_workerCount; ++i) {
        m_workers.emplace_back(&OSMDataProvider::WorkerThread, this);
    }

    return true;
}

void OSMDataProvider::Shutdown() {
    m_running = false;
    m_queueCondition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();

    m_rateLimiter.Shutdown();
}

bool OSMDataProvider::IsAvailable() const {
    if (m_offlineMode) return false;

    // Try a simple status check
    auto response = const_cast<CurlHttpClient*>(m_httpClient.get())->Get(
        m_config.overpassEndpoint + "/status");

    return response.IsSuccess();
}

void OSMDataProvider::SetConfig(const OSMConfig& config) {
    m_config = config;
    m_httpClient->SetTimeout(config.httpTimeout);
    SetRateLimit(config.requestsPerSecond);
}

GeoTileData OSMDataProvider::Query(const GeoBoundingBox& bounds,
                                    const GeoQueryOptions& options) {
    GeoTileData data;
    data.bounds = bounds;

    // Check cache first
    TileID tile = TileID::FromCoordinate(bounds.GetCenter(), m_config.defaultZoom);
    if (options.useCache && !options.forceRefresh) {
        if (CheckCache(tile, data)) {
            data.status = DataStatus::Cached;
            return data;
        }
    }

    // Check offline mode
    if (m_offlineMode) {
        data.status = DataStatus::Error;
        data.errorMessage = "Offline mode - data not in cache";
        return data;
    }

    // Rate limit
    m_rateLimiter.Acquire();

    // Build and execute query
    std::string query = BuildOverpassQuery(bounds, options);
    nlohmann::json response;

    try {
        response = ExecuteOverpassQuery(query);
    } catch (const std::exception& e) {
        data.status = DataStatus::Error;
        data.errorMessage = e.what();
        return data;
    }

    // Parse response
    ParseOverpassResponse(response, data, options);

    // Set metadata
    auto now = std::chrono::system_clock::now();
    data.fetchTimestamp = std::chrono::system_clock::to_time_t(now);
    data.expiryTimestamp = data.fetchTimestamp + options.cacheExpiryHours * 3600;
    data.sourceVersion = "OSM Overpass API";
    data.status = DataStatus::Loaded;

    // Store in cache
    if (options.useCache) {
        StoreInCache(tile, data);
    }

    return data;
}

GeoTileData OSMDataProvider::QueryTile(const TileID& tile,
                                         const GeoQueryOptions& options) {
    GeoTileData data;
    data.tileId = tile;
    data.bounds = tile.GetBounds();

    // Check cache
    if (options.useCache && !options.forceRefresh) {
        if (CheckCache(tile, data)) {
            data.status = DataStatus::Cached;
            return data;
        }
    }

    return Query(data.bounds, options);
}

GeoTileData OSMDataProvider::QueryRadius(const GeoCoordinate& center,
                                           double radiusMeters,
                                           const GeoQueryOptions& options) {
    GeoBoundingBox bounds = GeoBoundingBox::FromCenterRadius(center, radiusMeters);
    return Query(bounds, options);
}

void OSMDataProvider::QueryAsync(const GeoBoundingBox& bounds,
                                  GeoQueryCallback callback,
                                  const GeoQueryOptions& options) {
    AsyncTask task;
    task.bounds = bounds;
    task.options = options;
    task.callback = callback;
    task.isTileQuery = false;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_taskQueue.push(std::move(task));
    }
    m_queueCondition.notify_one();
}

void OSMDataProvider::QueryTileAsync(const TileID& tile,
                                       GeoQueryCallback callback,
                                       const GeoQueryOptions& options) {
    AsyncTask task;
    task.tile = tile;
    task.bounds = tile.GetBounds();
    task.options = options;
    task.callback = callback;
    task.isTileQuery = true;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_taskQueue.push(std::move(task));
    }
    m_queueCondition.notify_one();
}

void OSMDataProvider::QueryTilesAsync(const std::vector<TileID>& tiles,
                                        GeoQueryCallback callback,
                                        GeoProgressCallback progress,
                                        const GeoQueryOptions& options) {
    std::atomic<int> completed{0};
    int total = static_cast<int>(tiles.size());

    for (const auto& tile : tiles) {
        QueryTileAsync(tile, [callback, progress, &completed, total, tile](
            const GeoTileData& data, bool success, const std::string& error) {

            callback(data, success, error);

            int c = ++completed;
            if (progress) {
                progress(c, total, tile);
            }
        }, options);
    }
}

std::future<GeoTileData> OSMDataProvider::QueryFuture(const GeoBoundingBox& bounds,
                                                        const GeoQueryOptions& options) {
    auto promise = std::make_shared<std::promise<GeoTileData>>();
    auto future = promise->get_future();

    QueryAsync(bounds, [promise](const GeoTileData& data, bool success, const std::string& error) {
        if (success) {
            promise->set_value(data);
        } else {
            GeoTileData errorData = data;
            errorData.status = DataStatus::Error;
            errorData.errorMessage = error;
            promise->set_value(errorData);
        }
    }, options);

    return future;
}

int OSMDataProvider::PrefetchTiles(const std::vector<TileID>& tiles,
                                     GeoProgressCallback progress) {
    std::atomic<int> successCount{0};
    std::atomic<int> completed{0};
    int total = static_cast<int>(tiles.size());

    GeoQueryOptions options;
    options.useCache = true;
    options.forceRefresh = false;

    for (const auto& tile : tiles) {
        auto data = QueryTile(tile, options);
        if (data.status == DataStatus::Loaded || data.status == DataStatus::Cached) {
            ++successCount;
        }

        int c = ++completed;
        if (progress) {
            progress(c, total, tile);
        }
    }

    return successCount;
}

nlohmann::json OSMDataProvider::ExecuteOverpassQuery(const std::string& query) {
    IncrementRequestCount();

    std::string encodedQuery = query;
    // URL encode if needed

    HttpResponse response = m_httpClient->Post(
        m_config.overpassEndpoint,
        "data=" + encodedQuery,
        "application/x-www-form-urlencoded"
    );

    AddBytesDownloaded(response.downloadSize);

    if (!response.IsSuccess()) {
        throw std::runtime_error("Overpass API error: " + response.error +
                                 " (HTTP " + std::to_string(response.statusCode) + ")");
    }

    try {
        return nlohmann::json::parse(response.body);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Failed to parse Overpass response: " + std::string(e.what()));
    }
}

std::string OSMDataProvider::BuildOverpassQuery(const GeoBoundingBox& bounds,
                                                  const GeoQueryOptions& options) const {
    OverpassQueryBuilder builder;
    builder.SetTimeout(m_config.queryTimeout)
           .SetBounds(bounds);

    if (options.fetchRoads) {
        builder.AddHighways();
        builder.AddRailways();
    }

    if (options.fetchBuildings) {
        builder.AddBuildings();
    }

    if (options.fetchWater) {
        builder.AddWater();
    }

    if (options.fetchPOIs) {
        builder.AddPOIs();
    }

    if (options.fetchLandUse) {
        builder.AddLandUse();
    }

    return builder.Build();
}

std::vector<GeoCoordinate> OSMDataProvider::SearchLocation(const std::string& query, int limit) {
    std::vector<GeoCoordinate> results;

    std::string url = m_config.nominatimEndpoint + "/search?format=json&q=" +
                      query + "&limit=" + std::to_string(limit);

    m_rateLimiter.Acquire();
    IncrementRequestCount();

    HttpResponse response = m_httpClient->Get(url);
    AddBytesDownloaded(response.downloadSize);

    if (!response.IsSuccess()) return results;

    try {
        auto json = nlohmann::json::parse(response.body);
        for (const auto& item : json) {
            double lat = std::stod(item["lat"].get<std::string>());
            double lon = std::stod(item["lon"].get<std::string>());
            results.emplace_back(lat, lon);
        }
    } catch (...) {
        // Return empty on error
    }

    return results;
}

std::string OSMDataProvider::ReverseGeocode(const GeoCoordinate& coord) {
    std::string url = m_config.nominatimEndpoint + "/reverse?format=json&lat=" +
                      std::to_string(coord.latitude) + "&lon=" +
                      std::to_string(coord.longitude);

    m_rateLimiter.Acquire();
    IncrementRequestCount();

    HttpResponse response = m_httpClient->Get(url);
    AddBytesDownloaded(response.downloadSize);

    if (!response.IsSuccess()) return "";

    try {
        auto json = nlohmann::json::parse(response.body);
        if (json.contains("display_name")) {
            return json["display_name"];
        }
    } catch (...) {
        // Return empty on error
    }

    return "";
}

void OSMDataProvider::ParseOverpassResponse(const nlohmann::json& response,
                                              GeoTileData& outData,
                                              const GeoQueryOptions& options) {
    if (!response.contains("elements")) return;

    const auto& elements = response["elements"];

    // Extract all nodes first for coordinate lookup
    auto nodes = ExtractNodes(elements);

    // Parse each feature type
    if (options.fetchRoads) {
        ParseRoads(elements, nodes, outData.roads, options);
    }

    if (options.fetchBuildings) {
        ParseBuildings(elements, nodes, outData.buildings, options);
    }

    if (options.fetchWater) {
        ParseWaterBodies(elements, nodes, outData.waterBodies, options);
    }

    if (options.fetchPOIs) {
        ParsePOIs(elements, nodes, outData.pois, options);
    }

    if (options.fetchLandUse) {
        ParseLandUse(elements, nodes, outData.landUse, options);
    }
}

std::unordered_map<int64_t, GeoCoordinate> OSMDataProvider::ExtractNodes(
    const nlohmann::json& elements) {

    std::unordered_map<int64_t, GeoCoordinate> nodes;

    for (const auto& elem : elements) {
        if (elem["type"] == "node") {
            int64_t id = elem["id"];
            double lat = elem["lat"];
            double lon = elem["lon"];
            nodes[id] = GeoCoordinate(lat, lon);
        }
    }

    return nodes;
}

std::vector<GeoCoordinate> OSMDataProvider::WayToCoordinates(
    const nlohmann::json& way,
    const std::unordered_map<int64_t, GeoCoordinate>& nodes) {

    std::vector<GeoCoordinate> coords;

    // Check for geometry in the way itself (from out:geom)
    if (way.contains("geometry")) {
        for (const auto& pt : way["geometry"]) {
            coords.emplace_back(pt["lat"], pt["lon"]);
        }
        return coords;
    }

    // Fall back to node references
    if (way.contains("nodes")) {
        for (const auto& nodeId : way["nodes"]) {
            auto it = nodes.find(nodeId.get<int64_t>());
            if (it != nodes.end()) {
                coords.push_back(it->second);
            }
        }
    }

    return coords;
}

void OSMDataProvider::ParseRoads(const nlohmann::json& elements,
                                   const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                                   std::vector<GeoRoad>& outRoads,
                                   const GeoQueryOptions& options) {

    for (const auto& elem : elements) {
        if (elem["type"] != "way") continue;
        if (!elem.contains("tags")) continue;

        const auto& tags = elem["tags"];

        // Check for highway or railway tag
        if (!tags.contains("highway") && !tags.contains("railway")) continue;

        GeoRoad road;
        road.id = elem["id"];
        road.points = WayToCoordinates(elem, nodes);

        if (road.points.size() < 2) continue;

        // Check minimum length
        if (road.GetLength() < options.minRoadLength) continue;

        // Parse highway type
        if (tags.contains("highway")) {
            road.type = RoadTypeFromOSM(tags["highway"]);
        } else if (tags.contains("railway")) {
            std::string railway = tags["railway"];
            if (railway == "rail") road.type = RoadType::Rail;
            else if (railway == "light_rail" || railway == "tram") road.type = RoadType::LightRail;
            else if (railway == "subway") road.type = RoadType::Subway;
        }

        // Filter by type if specified
        if (!options.roadTypes.empty()) {
            if (std::find(options.roadTypes.begin(), options.roadTypes.end(), road.type)
                == options.roadTypes.end()) {
                continue;
            }
        }

        // Parse other attributes
        if (tags.contains("name")) road.name = tags["name"];
        if (tags.contains("ref")) road.ref = tags["ref"];
        if (tags.contains("oneway")) road.oneway = (tags["oneway"] == "yes");
        if (tags.contains("bridge")) road.bridge = (tags["bridge"] == "yes");
        if (tags.contains("tunnel")) road.tunnel = (tags["tunnel"] == "yes");
        if (tags.contains("layer")) road.layer = std::stoi(tags["layer"].get<std::string>());

        if (tags.contains("lanes")) {
            try { road.lanes = std::stoi(tags["lanes"].get<std::string>()); }
            catch (...) {}
        }
        if (tags.contains("width")) {
            try { road.width = std::stof(tags["width"].get<std::string>()); }
            catch (...) {}
        }
        if (tags.contains("maxspeed")) {
            try {
                std::string speed = tags["maxspeed"];
                // Remove "mph" or "km/h" suffix
                road.maxSpeed = std::stoi(speed);
            } catch (...) {}
        }

        if (tags.contains("surface")) {
            std::string surf = tags["surface"];
            if (surf == "asphalt") road.surface = RoadSurface::Asphalt;
            else if (surf == "concrete") road.surface = RoadSurface::Concrete;
            else if (surf == "paved") road.surface = RoadSurface::Paved;
            else if (surf == "gravel") road.surface = RoadSurface::Gravel;
            else if (surf == "dirt" || surf == "earth") road.surface = RoadSurface::Dirt;
            else if (surf == "sand") road.surface = RoadSurface::Sand;
            else if (surf == "cobblestone" || surf == "sett") road.surface = RoadSurface::Cobblestone;
            else if (surf == "wood") road.surface = RoadSurface::Wood;
            else if (surf == "metal") road.surface = RoadSurface::Metal;
        }

        // Store all tags
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            road.tags[it.key()] = it.value();
        }

        outRoads.push_back(std::move(road));

        // Check max features limit
        if (static_cast<int>(outRoads.size()) >= options.maxFeatures) break;
    }
}

void OSMDataProvider::ParseBuildings(const nlohmann::json& elements,
                                       const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                                       std::vector<GeoBuilding>& outBuildings,
                                       const GeoQueryOptions& options) {

    for (const auto& elem : elements) {
        if (elem["type"] != "way") continue;
        if (!elem.contains("tags")) continue;

        const auto& tags = elem["tags"];
        if (!tags.contains("building")) continue;

        GeoBuilding building;
        building.id = elem["id"];
        building.outline = WayToCoordinates(elem, nodes);

        if (building.outline.size() < 3) continue;

        // Check minimum area
        if (building.GetArea() < options.minBuildingArea) continue;

        // Parse building type
        building.type = BuildingTypeFromOSM(tags["building"]);

        // Filter by type if specified
        if (!options.buildingTypes.empty()) {
            if (std::find(options.buildingTypes.begin(), options.buildingTypes.end(), building.type)
                == options.buildingTypes.end()) {
                continue;
            }
        }

        // Parse attributes
        if (tags.contains("name")) building.name = tags["name"];
        if (tags.contains("addr:full")) building.address = tags["addr:full"];
        else if (tags.contains("addr:housenumber") && tags.contains("addr:street")) {
            building.address = tags["addr:housenumber"].get<std::string>() + " " +
                              tags["addr:street"].get<std::string>();
        }

        // Height
        if (tags.contains("height")) {
            try {
                std::string h = tags["height"];
                building.height = std::stof(h);
            } catch (...) {}
        }
        if (tags.contains("building:levels")) {
            try {
                building.levels = std::stoi(tags["building:levels"].get<std::string>());
            } catch (...) {}
        }
        if (tags.contains("min_height")) {
            try { building.minHeight = std::stof(tags["min_height"].get<std::string>()); }
            catch (...) {}
        }
        if (tags.contains("building:min_level")) {
            try { building.minLevel = std::stoi(tags["building:min_level"].get<std::string>()); }
            catch (...) {}
        }

        // Material/facade
        if (tags.contains("building:material")) {
            std::string mat = tags["building:material"];
            if (mat == "brick") building.material = BuildingMaterial::Brick;
            else if (mat == "stone") building.material = BuildingMaterial::Stone;
            else if (mat == "concrete") building.material = BuildingMaterial::Concrete;
            else if (mat == "glass") building.material = BuildingMaterial::Glass;
            else if (mat == "metal") building.material = BuildingMaterial::Metal;
            else if (mat == "wood") building.material = BuildingMaterial::Wood;
            else if (mat == "plaster") building.material = BuildingMaterial::Plaster;
        }

        // Roof
        if (tags.contains("roof:shape")) {
            std::string roof = tags["roof:shape"];
            if (roof == "flat") building.roofType = RoofType::Flat;
            else if (roof == "gabled") building.roofType = RoofType::Gabled;
            else if (roof == "hipped") building.roofType = RoofType::Hipped;
            else if (roof == "pyramidal") building.roofType = RoofType::Pyramidal;
            else if (roof == "dome") building.roofType = RoofType::Dome;
            else if (roof == "skillion") building.roofType = RoofType::Skillion;
            else if (roof == "gambrel") building.roofType = RoofType::Gambrel;
            else if (roof == "mansard") building.roofType = RoofType::Mansard;
            else if (roof == "round") building.roofType = RoofType::Round;
        }
        if (tags.contains("roof:height")) {
            try { building.roofHeight = std::stof(tags["roof:height"].get<std::string>()); }
            catch (...) {}
        }

        // Colors
        if (tags.contains("building:colour")) {
            building.wallColor = ParseColor(tags["building:colour"]);
        }
        if (tags.contains("roof:colour")) {
            building.roofColor = ParseColor(tags["roof:colour"]);
        }

        // Store all tags
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            building.tags[it.key()] = it.value();
        }

        outBuildings.push_back(std::move(building));

        if (static_cast<int>(outBuildings.size()) >= options.maxFeatures) break;
    }
}

void OSMDataProvider::ParseWaterBodies(const nlohmann::json& elements,
                                          const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                                          std::vector<GeoWaterBody>& outWater,
                                          const GeoQueryOptions& options) {

    for (const auto& elem : elements) {
        if (!elem.contains("tags")) continue;
        const auto& tags = elem["tags"];

        // Check for water-related tags
        std::string natural = tags.contains("natural") ? tags["natural"].get<std::string>() : "";
        std::string water = tags.contains("water") ? tags["water"].get<std::string>() : "";
        std::string waterway = tags.contains("waterway") ? tags["waterway"].get<std::string>() : "";

        WaterType type = WaterTypeFromOSM(natural, water, waterway);
        if (type == WaterType::Unknown) continue;

        GeoWaterBody body;
        body.id = elem["id"];
        body.type = type;

        if (tags.contains("name")) body.name = tags["name"];
        if (tags.contains("intermittent")) body.intermittent = (tags["intermittent"] == "yes");
        if (tags.contains("tidal")) body.tidal = (tags["tidal"] == "yes");

        if (elem["type"] == "way") {
            auto coords = WayToCoordinates(elem, nodes);

            // Linear water body (river, stream)
            if (!waterway.empty() && waterway != "riverbank") {
                body.isArea = false;
                body.centerline = coords;

                if (tags.contains("width")) {
                    try { body.width = std::stof(tags["width"].get<std::string>()); }
                    catch (...) {}
                }
            } else {
                // Area water body
                body.isArea = true;
                body.outline = coords;
            }
        }

        // Store tags
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            body.tags[it.key()] = it.value();
        }

        outWater.push_back(std::move(body));

        if (static_cast<int>(outWater.size()) >= options.maxFeatures) break;
    }
}

void OSMDataProvider::ParsePOIs(const nlohmann::json& elements,
                                  const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                                  std::vector<GeoPOI>& outPOIs,
                                  const GeoQueryOptions& options) {

    for (const auto& elem : elements) {
        if (!elem.contains("tags")) continue;
        const auto& tags = elem["tags"];

        // Get POI category from various tags
        std::string amenity = tags.contains("amenity") ? tags["amenity"].get<std::string>() : "";
        std::string shop = tags.contains("shop") ? tags["shop"].get<std::string>() : "";
        std::string tourism = tags.contains("tourism") ? tags["tourism"].get<std::string>() : "";
        std::string natural = tags.contains("natural") ? tags["natural"].get<std::string>() : "";

        POICategory category = POICategoryFromOSM(amenity, shop, tourism, natural);
        if (category == POICategory::Unknown) continue;

        // Filter by category if specified
        if (!options.poiCategories.empty()) {
            if (std::find(options.poiCategories.begin(), options.poiCategories.end(), category)
                == options.poiCategories.end()) {
                continue;
            }
        }

        GeoPOI poi;
        poi.id = elem["id"];
        poi.category = category;

        // Get location
        if (elem["type"] == "node") {
            poi.location = GeoCoordinate(elem["lat"], elem["lon"]);
        } else if (elem["type"] == "way") {
            poi.outline = WayToCoordinates(elem, nodes);
            if (!poi.outline.empty()) {
                poi.location = CalculateCentroid(poi.outline);
            }
        }

        // Parse attributes
        if (tags.contains("name")) poi.name = tags["name"];
        if (tags.contains("phone")) poi.phone = tags["phone"];
        if (tags.contains("website")) poi.website = tags["website"];
        if (tags.contains("opening_hours")) poi.openingHours = tags["opening_hours"];

        // Address
        if (tags.contains("addr:full")) poi.address = tags["addr:full"];
        else if (tags.contains("addr:housenumber") && tags.contains("addr:street")) {
            poi.address = tags["addr:housenumber"].get<std::string>() + " " +
                         tags["addr:street"].get<std::string>();
        }

        // Store tags
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            poi.tags[it.key()] = it.value();
        }

        outPOIs.push_back(std::move(poi));

        if (static_cast<int>(outPOIs.size()) >= options.maxFeatures) break;
    }
}

void OSMDataProvider::ParseLandUse(const nlohmann::json& elements,
                                     const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                                     std::vector<GeoLandUse>& outLandUse,
                                     const GeoQueryOptions& options) {

    for (const auto& elem : elements) {
        if (elem["type"] != "way") continue;
        if (!elem.contains("tags")) continue;
        const auto& tags = elem["tags"];

        std::string landuse = tags.contains("landuse") ? tags["landuse"].get<std::string>() : "";
        std::string natural = tags.contains("natural") ? tags["natural"].get<std::string>() : "";
        std::string leisure = tags.contains("leisure") ? tags["leisure"].get<std::string>() : "";

        LandUseType type = LandUseTypeFromOSM(landuse, natural, leisure);
        if (type == LandUseType::Unknown) continue;

        GeoLandUse lu;
        lu.id = elem["id"];
        lu.type = type;
        lu.outline = WayToCoordinates(elem, nodes);

        if (lu.outline.size() < 3) continue;

        if (tags.contains("name")) lu.name = tags["name"];

        // Store tags
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            lu.tags[it.key()] = it.value();
        }

        outLandUse.push_back(std::move(lu));

        if (static_cast<int>(outLandUse.size()) >= options.maxFeatures) break;
    }
}

glm::vec3 OSMDataProvider::ParseColor(const std::string& colorStr) {
    // Handle named colors
    static const std::unordered_map<std::string, glm::vec3> namedColors = {
        {"white", {1.0f, 1.0f, 1.0f}},
        {"black", {0.0f, 0.0f, 0.0f}},
        {"red", {1.0f, 0.0f, 0.0f}},
        {"green", {0.0f, 1.0f, 0.0f}},
        {"blue", {0.0f, 0.0f, 1.0f}},
        {"yellow", {1.0f, 1.0f, 0.0f}},
        {"brown", {0.6f, 0.3f, 0.0f}},
        {"grey", {0.5f, 0.5f, 0.5f}},
        {"gray", {0.5f, 0.5f, 0.5f}},
        {"orange", {1.0f, 0.5f, 0.0f}},
        {"pink", {1.0f, 0.75f, 0.8f}},
        {"beige", {0.96f, 0.96f, 0.86f}},
    };

    auto it = namedColors.find(colorStr);
    if (it != namedColors.end()) {
        return it->second;
    }

    // Handle hex colors
    if (colorStr.size() >= 6) {
        std::string hex = colorStr;
        if (hex[0] == '#') hex = hex.substr(1);

        try {
            int r = std::stoi(hex.substr(0, 2), nullptr, 16);
            int g = std::stoi(hex.substr(2, 2), nullptr, 16);
            int b = std::stoi(hex.substr(4, 2), nullptr, 16);
            return glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f);
        } catch (...) {}
    }

    return glm::vec3(0.8f);  // Default gray
}

void OSMDataProvider::WorkerThread() {
    while (m_running) {
        AsyncTask task;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] {
                return !m_taskQueue.empty() || !m_running;
            });

            if (!m_running && m_taskQueue.empty()) return;
            if (m_taskQueue.empty()) continue;

            task = std::move(m_taskQueue.front());
            m_taskQueue.pop();
        }

        // Execute query
        GeoTileData data;
        bool success = true;
        std::string error;

        try {
            if (task.isTileQuery) {
                data = QueryTile(task.tile, task.options);
            } else {
                data = Query(task.bounds, task.options);
            }

            if (data.status == DataStatus::Error) {
                success = false;
                error = data.errorMessage;
            }
        } catch (const std::exception& e) {
            success = false;
            error = e.what();
            data.status = DataStatus::Error;
            data.errorMessage = error;
        }

        // Call callback
        if (task.callback) {
            task.callback(data, success, error);
        }
    }
}

// =============================================================================
// OverpassQueryBuilder Implementation
// =============================================================================

OverpassQueryBuilder::OverpassQueryBuilder() = default;

OverpassQueryBuilder& OverpassQueryBuilder::SetFormat(const std::string& format) {
    m_format = format;
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::SetTimeout(int seconds) {
    m_timeout = seconds;
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::SetBounds(const GeoBoundingBox& bounds) {
    m_bounds = bounds;
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddNodeQuery(const std::string& filter) {
    std::ostringstream oss;
    oss << "node" << filter << "(" << m_bounds.min.latitude << ","
        << m_bounds.min.longitude << "," << m_bounds.max.latitude << ","
        << m_bounds.max.longitude << ");";
    m_queries.push_back(oss.str());
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddWayQuery(const std::string& filter) {
    std::ostringstream oss;
    oss << "way" << filter << "(" << m_bounds.min.latitude << ","
        << m_bounds.min.longitude << "," << m_bounds.max.latitude << ","
        << m_bounds.max.longitude << ");";
    m_queries.push_back(oss.str());
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddRelationQuery(const std::string& filter) {
    std::ostringstream oss;
    oss << "relation" << filter << "(" << m_bounds.min.latitude << ","
        << m_bounds.min.longitude << "," << m_bounds.max.latitude << ","
        << m_bounds.max.longitude << ");";
    m_queries.push_back(oss.str());
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddHighways(const std::vector<std::string>& types) {
    if (types.empty()) {
        AddWayQuery("[highway]");
    } else {
        std::string filter = "[highway~\"" + types[0];
        for (size_t i = 1; i < types.size(); ++i) {
            filter += "|" + types[i];
        }
        filter += "\"]";
        AddWayQuery(filter);
    }
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddBuildings() {
    AddWayQuery("[building]");
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddWater() {
    AddWayQuery("[natural=water]");
    AddWayQuery("[waterway]");
    AddWayQuery("[natural=coastline]");
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddPOIs(const std::vector<std::string>& amenities) {
    if (amenities.empty()) {
        AddNodeQuery("[amenity]");
        AddNodeQuery("[shop]");
        AddNodeQuery("[tourism]");
        AddWayQuery("[amenity]");
        AddWayQuery("[shop]");
    } else {
        std::string filter = "[amenity~\"" + amenities[0];
        for (size_t i = 1; i < amenities.size(); ++i) {
            filter += "|" + amenities[i];
        }
        filter += "\"]";
        AddNodeQuery(filter);
        AddWayQuery(filter);
    }
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddLandUse() {
    AddWayQuery("[landuse]");
    AddWayQuery("[natural~\"wood|grassland|heath|scrub|wetland|beach|sand\"]");
    AddWayQuery("[leisure~\"park|playground|pitch|golf_course\"]");
    return *this;
}

OverpassQueryBuilder& OverpassQueryBuilder::AddRailways() {
    AddWayQuery("[railway~\"rail|light_rail|subway|tram\"]");
    return *this;
}

std::string OverpassQueryBuilder::Build() const {
    std::ostringstream oss;

    // Header
    oss << "[out:" << m_format << "]";
    oss << "[timeout:" << m_timeout << "]";
    oss << ";\n";

    // Combine all queries with union
    oss << "(\n";
    for (const auto& q : m_queries) {
        oss << "  " << q << "\n";
    }
    oss << ");\n";

    // Output with geometry
    if (m_includeGeometry) {
        oss << "out body geom;\n";
    } else {
        oss << "out body;\n";
        oss << ">;\n";
        oss << "out skel qt;\n";
    }

    return oss.str();
}

} // namespace Geo
} // namespace Vehement
