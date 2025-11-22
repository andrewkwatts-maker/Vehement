#pragma once

#include "GeoDataProvider.hpp"
#include <string>
#include <vector>
#include <memory>

namespace Vehement {
namespace Geo {

/**
 * @brief Configuration for elevation data provider
 */
struct ElevationConfig {
    // API endpoints
    std::string openElevationEndpoint = "https://api.open-elevation.com/api/v1/lookup";
    std::string mapzenEndpoint = "https://elevation.nationalmap.gov/arcgis/rest/services";
    std::string srtmEndpoint = "";  // Local SRTM data path

    // Rate limiting
    double requestsPerSecond = 1.0;
    int batchSize = 100;             // Max points per request

    // Resolution
    int defaultResolution = 30;      // Default meters per sample
    int minResolution = 10;
    int maxResolution = 90;

    // Data source priority
    enum class DataSource { OpenElevation, Mapzen, SRTM, Local };
    std::vector<DataSource> sourcePriority = { DataSource::OpenElevation, DataSource::SRTM };

    // Local DEM files
    std::string localDEMPath;        // Path to local GeoTIFF files

    /**
     * @brief Load from JSON file
     */
    static ElevationConfig LoadFromFile(const std::string& path);

    /**
     * @brief Save to JSON file
     */
    void SaveToFile(const std::string& path) const;
};

/**
 * @brief Elevation data provider
 *
 * Fetches elevation data from various sources:
 * - Open-Elevation API
 * - Mapzen/Amazon terrain tiles
 * - SRTM data (local or remote)
 * - Local GeoTIFF DEM files
 */
class ElevationProvider {
public:
    ElevationProvider();
    ~ElevationProvider();

    /**
     * @brief Initialize the provider
     */
    bool Initialize(const std::string& configPath = "");

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Set configuration
     */
    void SetConfig(const ElevationConfig& config) { m_config = config; }

    /**
     * @brief Get configuration
     */
    const ElevationConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set cache
     */
    void SetCache(std::shared_ptr<GeoTileCache> cache) { m_cache = cache; }

    // ==========================================================================
    // Elevation Queries
    // ==========================================================================

    /**
     * @brief Get elevation at a single point
     * @param coord Geographic coordinate
     * @return Elevation in meters (NaN if unavailable)
     */
    float GetElevation(const GeoCoordinate& coord);

    /**
     * @brief Get elevation for multiple points
     * @param coords List of coordinates
     * @return List of elevations (NaN for unavailable points)
     */
    std::vector<float> GetElevations(const std::vector<GeoCoordinate>& coords);

    /**
     * @brief Get elevation grid for a bounding box
     * @param bounds Geographic bounding box
     * @param resolution Meters per sample (0 = use default)
     * @return Elevation grid
     */
    ElevationGrid GetElevationGrid(const GeoBoundingBox& bounds, int resolution = 0);

    /**
     * @brief Get elevation grid for a tile
     * @param tile Tile identifier
     * @param resolution Meters per sample (0 = use default)
     * @return Elevation grid
     */
    ElevationGrid GetElevationGridForTile(const TileID& tile, int resolution = 0);

    // ==========================================================================
    // Terrain Analysis
    // ==========================================================================

    /**
     * @brief Calculate slope at a coordinate
     * @return Slope in degrees (0-90)
     */
    float GetSlope(const GeoCoordinate& coord);

    /**
     * @brief Calculate aspect (compass direction of slope) at a coordinate
     * @return Aspect in degrees (0-360, 0 = North)
     */
    float GetAspect(const GeoCoordinate& coord);

    /**
     * @brief Get terrain roughness at a coordinate
     * @return Roughness factor (0-1)
     */
    float GetRoughness(const GeoCoordinate& coord, float radius = 100.0f);

    /**
     * @brief Calculate viewshed from a point
     * @param observer Observer coordinate
     * @param height Observer height above ground
     * @param radius Maximum view distance in meters
     * @param resolution Grid resolution
     * @return Grid of visibility (1 = visible, 0 = hidden)
     */
    std::vector<std::vector<bool>> CalculateViewshed(
        const GeoCoordinate& observer,
        float height,
        float radius,
        int resolution = 30);

    // ==========================================================================
    // Texture Generation
    // ==========================================================================

    /**
     * @brief Generate heightmap texture
     * @param grid Elevation grid
     * @param minElev Minimum elevation for normalization (auto if < 0)
     * @param maxElev Maximum elevation for normalization (auto if < 0)
     * @return 8-bit grayscale heightmap
     */
    std::vector<uint8_t> GenerateHeightmap(
        const ElevationGrid& grid,
        float minElev = -1.0f,
        float maxElev = -1.0f);

    /**
     * @brief Generate 16-bit heightmap for more precision
     */
    std::vector<uint16_t> GenerateHeightmap16(
        const ElevationGrid& grid,
        float minElev = -1.0f,
        float maxElev = -1.0f);

    /**
     * @brief Generate normal map texture
     * @param grid Elevation grid
     * @param strength Normal strength multiplier
     * @return RGB normal map
     */
    std::vector<uint8_t> GenerateNormalMap(
        const ElevationGrid& grid,
        float strength = 1.0f);

    /**
     * @brief Generate slope map texture
     * @param grid Elevation grid
     * @return 8-bit slope map (0 = flat, 255 = 90 degrees)
     */
    std::vector<uint8_t> GenerateSlopeMap(const ElevationGrid& grid);

    /**
     * @brief Generate aspect map texture
     * @param grid Elevation grid
     * @return 8-bit aspect map (0-255 = 0-360 degrees)
     */
    std::vector<uint8_t> GenerateAspectMap(const ElevationGrid& grid);

    // ==========================================================================
    // Local DEM Support
    // ==========================================================================

    /**
     * @brief Load local DEM file (GeoTIFF)
     * @param path Path to GeoTIFF file
     * @return true on success
     */
    bool LoadLocalDEM(const std::string& path);

    /**
     * @brief Check if a coordinate is covered by local DEM
     */
    bool HasLocalCoverage(const GeoCoordinate& coord) const;

    /**
     * @brief Get list of loaded local DEM files
     */
    std::vector<std::string> GetLoadedDEMFiles() const;

    // ==========================================================================
    // Statistics
    // ==========================================================================

    /**
     * @brief Get request count
     */
    size_t GetRequestCount() const { return m_requestCount; }

    /**
     * @brief Get cache hit count
     */
    size_t GetCacheHits() const { return m_cacheHits; }

private:
    /**
     * @brief Fetch elevation from Open-Elevation API
     */
    std::vector<float> FetchFromOpenElevation(const std::vector<GeoCoordinate>& coords);

    /**
     * @brief Fetch elevation from local DEM
     */
    std::vector<float> FetchFromLocalDEM(const std::vector<GeoCoordinate>& coords);

    /**
     * @brief Interpolate elevation at fractional grid position
     */
    float InterpolateElevation(const ElevationGrid& grid, double x, double y);

    /**
     * @brief Create grid sample points for bounding box
     */
    std::vector<GeoCoordinate> CreateGridSamplePoints(
        const GeoBoundingBox& bounds, int width, int height);

    ElevationConfig m_config;
    std::unique_ptr<CurlHttpClient> m_httpClient;
    std::shared_ptr<GeoTileCache> m_cache;
    RateLimiter m_rateLimiter;

    // Local DEM data
    struct LocalDEM {
        std::string path;
        GeoBoundingBox bounds;
        std::vector<float> data;
        int width = 0;
        int height = 0;
        float noDataValue = -9999.0f;
    };
    std::vector<LocalDEM> m_localDEMs;
    mutable std::mutex m_demMutex;

    // Statistics
    std::atomic<size_t> m_requestCount{0};
    std::atomic<size_t> m_cacheHits{0};
};

/**
 * @brief Terrain mesh generator from elevation data
 */
class TerrainMeshGenerator {
public:
    struct MeshVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
    };

    struct TerrainMesh {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        GeoBoundingBox bounds;
        float minElevation = 0.0f;
        float maxElevation = 0.0f;
    };

    /**
     * @brief Generate terrain mesh from elevation grid
     * @param grid Elevation grid
     * @param scale World scale (meters per unit)
     * @param uvScale Texture coordinate scale
     * @return Generated mesh
     */
    static TerrainMesh GenerateMesh(
        const ElevationGrid& grid,
        float scale = 1.0f,
        float uvScale = 1.0f);

    /**
     * @brief Generate LOD mesh with reduced detail
     * @param grid Elevation grid
     * @param lodLevel LOD level (0 = full detail, higher = less detail)
     * @param scale World scale
     * @return Generated LOD mesh
     */
    static TerrainMesh GenerateLODMesh(
        const ElevationGrid& grid,
        int lodLevel,
        float scale = 1.0f);

    /**
     * @brief Generate mesh with adaptive tessellation based on slope
     */
    static TerrainMesh GenerateAdaptiveMesh(
        const ElevationGrid& grid,
        float slopeThreshold = 15.0f,
        float scale = 1.0f);
};

} // namespace Geo
} // namespace Vehement
