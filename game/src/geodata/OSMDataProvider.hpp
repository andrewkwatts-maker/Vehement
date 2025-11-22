#pragma once

#include "GeoDataProvider.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include <condition_variable>
#include <queue>

namespace Vehement {
namespace Geo {

/**
 * @brief Configuration for OSM data provider
 */
struct OSMConfig {
    // API endpoints
    std::string overpassEndpoint = "https://overpass-api.de/api/interpreter";
    std::string nominatimEndpoint = "https://nominatim.openstreetmap.org";

    // Rate limiting
    double requestsPerSecond = 1.0;     // Max requests per second
    int burstSize = 2;                   // Burst allowance

    // Timeouts
    int queryTimeout = 180;              // Overpass query timeout (seconds)
    int httpTimeout = 60;                // HTTP request timeout (seconds)

    // Data options
    int defaultZoom = 16;                // Default zoom level for tiles
    bool fetchRelations = true;          // Fetch multipolygon relations
    bool fetchMetadata = false;          // Fetch OSM metadata (user, timestamp)

    // Output format
    bool useJson = true;                 // Use JSON output (vs XML)

    /**
     * @brief Load from JSON file
     */
    static OSMConfig LoadFromFile(const std::string& path);

    /**
     * @brief Save to JSON file
     */
    void SaveToFile(const std::string& path) const;
};

/**
 * @brief OpenStreetMap data provider using Overpass API
 *
 * Fetches and parses geographic data from OpenStreetMap via the Overpass API.
 * Supports roads, buildings, water bodies, POIs, and land use data.
 */
class OSMDataProvider : public GeoDataProviderBase {
public:
    OSMDataProvider();
    ~OSMDataProvider() override;

    // ==========================================================================
    // Provider Interface
    // ==========================================================================

    std::string GetName() const override { return "OpenStreetMap"; }
    std::string GetVersion() const override { return "1.0.0"; }
    bool IsAvailable() const override;
    std::string GetAttribution() const override {
        return "Data (c) OpenStreetMap contributors, ODbL";
    }

    bool Initialize(const std::string& configPath = "") override;
    void Shutdown() override;

    // Synchronous queries
    GeoTileData Query(const GeoBoundingBox& bounds,
                      const GeoQueryOptions& options = {}) override;

    GeoTileData QueryTile(const TileID& tile,
                           const GeoQueryOptions& options = {}) override;

    GeoTileData QueryRadius(const GeoCoordinate& center,
                             double radiusMeters,
                             const GeoQueryOptions& options = {}) override;

    // Asynchronous queries
    void QueryAsync(const GeoBoundingBox& bounds,
                    GeoQueryCallback callback,
                    const GeoQueryOptions& options = {}) override;

    void QueryTileAsync(const TileID& tile,
                         GeoQueryCallback callback,
                         const GeoQueryOptions& options = {}) override;

    void QueryTilesAsync(const std::vector<TileID>& tiles,
                          GeoQueryCallback callback,
                          GeoProgressCallback progress = nullptr,
                          const GeoQueryOptions& options = {}) override;

    std::future<GeoTileData> QueryFuture(const GeoBoundingBox& bounds,
                                          const GeoQueryOptions& options = {}) override;

    // Prefetch
    int PrefetchTiles(const std::vector<TileID>& tiles,
                       GeoProgressCallback progress = nullptr) override;

    // ==========================================================================
    // OSM-Specific Methods
    // ==========================================================================

    /**
     * @brief Get configuration
     */
    const OSMConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set configuration
     */
    void SetConfig(const OSMConfig& config);

    /**
     * @brief Execute raw Overpass query
     * @param query Overpass QL query string
     * @return JSON response
     */
    nlohmann::json ExecuteOverpassQuery(const std::string& query);

    /**
     * @brief Build Overpass query for bounding box
     */
    std::string BuildOverpassQuery(const GeoBoundingBox& bounds,
                                    const GeoQueryOptions& options) const;

    /**
     * @brief Search for location by name (geocoding)
     */
    std::vector<GeoCoordinate> SearchLocation(const std::string& query, int limit = 5);

    /**
     * @brief Reverse geocode coordinate to address
     */
    std::string ReverseGeocode(const GeoCoordinate& coord);

private:
    /**
     * @brief Parse Overpass JSON response
     */
    void ParseOverpassResponse(const nlohmann::json& response,
                                GeoTileData& outData,
                                const GeoQueryOptions& options);

    /**
     * @brief Parse roads from OSM elements
     */
    void ParseRoads(const nlohmann::json& elements,
                    const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                    std::vector<GeoRoad>& outRoads,
                    const GeoQueryOptions& options);

    /**
     * @brief Parse buildings from OSM elements
     */
    void ParseBuildings(const nlohmann::json& elements,
                        const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                        std::vector<GeoBuilding>& outBuildings,
                        const GeoQueryOptions& options);

    /**
     * @brief Parse water bodies from OSM elements
     */
    void ParseWaterBodies(const nlohmann::json& elements,
                          const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                          std::vector<GeoWaterBody>& outWater,
                          const GeoQueryOptions& options);

    /**
     * @brief Parse POIs from OSM elements
     */
    void ParsePOIs(const nlohmann::json& elements,
                   const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                   std::vector<GeoPOI>& outPOIs,
                   const GeoQueryOptions& options);

    /**
     * @brief Parse land use from OSM elements
     */
    void ParseLandUse(const nlohmann::json& elements,
                      const std::unordered_map<int64_t, GeoCoordinate>& nodes,
                      std::vector<GeoLandUse>& outLandUse,
                      const GeoQueryOptions& options);

    /**
     * @brief Extract node coordinates from response
     */
    std::unordered_map<int64_t, GeoCoordinate> ExtractNodes(const nlohmann::json& elements);

    /**
     * @brief Convert OSM way nodes to coordinates
     */
    std::vector<GeoCoordinate> WayToCoordinates(
        const nlohmann::json& way,
        const std::unordered_map<int64_t, GeoCoordinate>& nodes);

    /**
     * @brief Parse color from OSM tag
     */
    glm::vec3 ParseColor(const std::string& colorStr);

    /**
     * @brief Worker thread function
     */
    void WorkerThread();

    OSMConfig m_config;
    std::unique_ptr<CurlHttpClient> m_httpClient;

    // Async processing
    struct AsyncTask {
        GeoBoundingBox bounds;
        GeoQueryOptions options;
        GeoQueryCallback callback;
        TileID tile;
        bool isTileQuery = false;
    };

    std::queue<AsyncTask> m_taskQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running{false};
    int m_workerCount = 2;
};

/**
 * @brief Overpass query builder helper
 */
class OverpassQueryBuilder {
public:
    OverpassQueryBuilder();

    /**
     * @brief Set output format
     */
    OverpassQueryBuilder& SetFormat(const std::string& format);

    /**
     * @brief Set timeout
     */
    OverpassQueryBuilder& SetTimeout(int seconds);

    /**
     * @brief Set bounding box
     */
    OverpassQueryBuilder& SetBounds(const GeoBoundingBox& bounds);

    /**
     * @brief Add node query
     */
    OverpassQueryBuilder& AddNodeQuery(const std::string& filter);

    /**
     * @brief Add way query
     */
    OverpassQueryBuilder& AddWayQuery(const std::string& filter);

    /**
     * @brief Add relation query
     */
    OverpassQueryBuilder& AddRelationQuery(const std::string& filter);

    /**
     * @brief Add highway query
     */
    OverpassQueryBuilder& AddHighways(const std::vector<std::string>& types = {});

    /**
     * @brief Add building query
     */
    OverpassQueryBuilder& AddBuildings();

    /**
     * @brief Add water query
     */
    OverpassQueryBuilder& AddWater();

    /**
     * @brief Add POI query
     */
    OverpassQueryBuilder& AddPOIs(const std::vector<std::string>& amenities = {});

    /**
     * @brief Add land use query
     */
    OverpassQueryBuilder& AddLandUse();

    /**
     * @brief Add railway query
     */
    OverpassQueryBuilder& AddRailways();

    /**
     * @brief Build the query string
     */
    std::string Build() const;

private:
    std::string m_format = "json";
    int m_timeout = 180;
    GeoBoundingBox m_bounds;
    std::vector<std::string> m_queries;
    bool m_includeGeometry = true;
};

} // namespace Geo
} // namespace Vehement
