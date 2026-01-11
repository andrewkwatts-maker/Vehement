#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <queue>
#include <chrono>
#include <glm/glm.hpp>

/**
 * @brief Manages geospatial data sources with tile-based caching
 *
 * Supports multiple free data sources:
 * - Elevation: SRTM, ASTER GDEM, OpenTopography
 * - Satellite: Sentinel-2, Landsat, MODIS
 * - Vector: OpenStreetMap (roads, buildings, landuse)
 * - Climate: OpenWeatherMap, WorldClim
 * - Population: WorldPop, GHS
 * - Land Cover: ESA WorldCover, USGS NLCD
 */

namespace DataSource {

/**
 * @brief Data source types
 */
enum class SourceType {
    // Elevation/Terrain
    SRTM_30m,           // Shuttle Radar Topography Mission - 30m resolution
    SRTM_90m,           // SRTM - 90m resolution
    ASTER_GDEM,         // ASTER Global DEM - 30m
    ALOS_World3D,       // ALOS World 3D - 30m
    NASADEM,            // NASA DEM - 30m
    COPERNICUS_DEM,     // Copernicus DEM - 30m/90m

    // Satellite Imagery
    SENTINEL2_RGB,      // Sentinel-2 RGB - 10m
    SENTINEL2_NDVI,     // Sentinel-2 NDVI (vegetation index)
    LANDSAT8_RGB,       // Landsat 8 RGB - 30m
    MODIS_NDVI,         // MODIS NDVI - 250m

    // Vector Data
    OSM_ROADS,          // OpenStreetMap roads
    OSM_BUILDINGS,      // OpenStreetMap buildings
    OSM_LANDUSE,        // OpenStreetMap land use
    OSM_WATER,          // OpenStreetMap water bodies
    OSM_NATURAL,        // OpenStreetMap natural features

    // Climate/Weather
    OPENWEATHER_TEMP,   // OpenWeatherMap temperature
    WORLDCLIM_PRECIP,   // WorldClim precipitation
    WORLDCLIM_BIOCLIM,  // WorldClim bioclimatic variables

    // Population/Urban
    WORLDPOP_DENSITY,   // WorldPop population density
    GHS_BUILT,          // Global Human Settlement built-up

    // Land Cover
    ESA_WORLDCOVER,     // ESA WorldCover 10m
    MODIS_LANDCOVER,    // MODIS Land Cover Type
    CORINE,             // CORINE Land Cover (Europe)

    // Custom/User
    CUSTOM_RASTER,      // User-provided raster data
    CUSTOM_VECTOR       // User-provided vector data
};

/**
 * @brief Tile key for caching (Web Mercator tiling scheme)
 */
struct TileKey {
    int zoom;
    int x;
    int y;
    SourceType source;

    bool operator==(const TileKey& other) const {
        return zoom == other.zoom && x == other.x && y == other.y && source == other.source;
    }
};

} // namespace DataSource

// Hash function for TileKey
namespace std {
    template<>
    struct hash<DataSource::TileKey> {
        size_t operator()(const DataSource::TileKey& k) const {
            return ((hash<int>()(k.zoom) ^ (hash<int>()(k.x) << 1)) >> 1)
                 ^ (hash<int>()(k.y) << 1)
                 ^ (hash<int>()(static_cast<int>(k.source)) << 2);
        }
    };
}

namespace DataSource {

/**
 * @brief Cached tile data
 */
struct CachedTile {
    TileKey key;
    std::vector<float> data;  // Raster data as flat array
    int width = 256;
    int height = 256;
    int channels = 1;  // 1=grayscale, 3=RGB, 4=RGBA

    // Cache metadata
    std::chrono::system_clock::time_point lastAccess;
    std::chrono::system_clock::time_point downloadTime;
    size_t accessCount = 0;
    size_t sizeBytes = 0;

    // Status
    bool isLoaded = false;
    bool hasError = false;
    std::string errorMessage;
};

/**
 * @brief Cache configuration
 */
struct CacheConfig {
    // Size limits
    size_t maxCacheSizeMB = 1024;  // 1GB default
    size_t maxTilesInMemory = 1000;

    // Eviction policy
    enum class EvictionPolicy {
        LRU,        // Least Recently Used
        LFU,        // Least Frequently Used
        FIFO,       // First In First Out
        Size        // Evict largest tiles first
    };
    EvictionPolicy policy = EvictionPolicy::LRU;

    // Persistence
    bool enableDiskCache = true;
    std::string diskCachePath = "cache/geodata/";
    size_t maxDiskCacheSizeMB = 10240;  // 10GB default

    // Network
    int maxConcurrentDownloads = 4;
    int downloadTimeoutSeconds = 30;
    bool useCompression = true;

    // Retry policy
    int maxRetries = 3;
    int retryDelayMs = 1000;
};

/**
 * @brief Data source configuration
 */
struct SourceConfig {
    SourceType type;
    std::string apiUrl;
    std::string apiKey;  // For sources that require API key
    bool requiresAuth = false;
    int maxZoomLevel = 18;
    int minZoomLevel = 0;

    // Tile size
    int tileWidth = 256;
    int tileHeight = 256;

    // Data format
    std::string format = "png";  // png, jpg, tif, geojson, etc.

    // Attribution
    std::string attribution;
    std::string license;
};

/**
 * @brief Free data source APIs
 */
class FreeDataSources {
public:
    /**
     * @brief Get configuration for free data sources
     */
    static std::vector<SourceConfig> GetFreeSources() {
        return {
            // ===== Elevation Data =====
            {
                SourceType::SRTM_30m,
                "https://elevation-tiles-prod.s3.amazonaws.com/skadi/{z}/{x}/{y}.hgt",
                "",
                false,
                14,
                0,
                256, 256,
                "hgt",
                "NASA SRTM",
                "Public Domain"
            },
            {
                SourceType::COPERNICUS_DEM,
                "https://copernicus-dem-30m.s3.amazonaws.com/tiles/{z}/{x}/{y}.tif",
                "",
                false,
                14,
                0,
                256, 256,
                "tif",
                "Copernicus DEM",
                "CC BY 4.0"
            },
            {
                SourceType::NASADEM,
                "https://e4ftl01.cr.usgs.gov/MEASURES/NASADEM_HGT.001/{tile}.zip",
                "",
                false,
                14,
                0,
                3601, 3601,
                "hgt",
                "NASA DEM",
                "Public Domain"
            },

            // ===== Satellite Imagery =====
            {
                SourceType::SENTINEL2_RGB,
                "https://services.sentinel-hub.com/ogc/wms/{instance_id}",
                "",  // Requires free Sentinel Hub account
                true,
                18,
                0,
                256, 256,
                "png",
                "Sentinel-2 (ESA)",
                "CC BY-SA 3.0 IGO"
            },
            {
                SourceType::LANDSAT8_RGB,
                "https://landsatlook.usgs.gov/tile-services/landsat/EPSG:3857/{z}/{x}/{y}",
                "",
                false,
                14,
                0,
                256, 256,
                "jpg",
                "Landsat 8 (USGS)",
                "Public Domain"
            },

            // ===== OpenStreetMap =====
            {
                SourceType::OSM_ROADS,
                "https://overpass-api.de/api/interpreter",
                "",
                false,
                18,
                0,
                256, 256,
                "geojson",
                "OpenStreetMap contributors",
                "ODbL"
            },
            {
                SourceType::OSM_BUILDINGS,
                "https://overpass-api.de/api/interpreter",
                "",
                false,
                18,
                0,
                256, 256,
                "geojson",
                "OpenStreetMap contributors",
                "ODbL"
            },

            // ===== Climate Data =====
            {
                SourceType::OPENWEATHER_TEMP,
                "https://tile.openweathermap.org/map/temp_new/{z}/{x}/{y}.png",
                "",  // Requires free API key
                true,
                10,
                0,
                256, 256,
                "png",
                "OpenWeatherMap",
                "CC BY-SA 4.0"
            },
            {
                SourceType::WORLDCLIM_PRECIP,
                "https://biogeo.ucdavis.edu/data/worldclim/v2.1/base/wc2.1_30s_prec_{month}.tif",
                "",
                false,
                10,
                0,
                256, 256,
                "tif",
                "WorldClim",
                "CC BY-SA 4.0"
            },

            // ===== Land Cover =====
            {
                SourceType::ESA_WORLDCOVER,
                "https://services.terrascope.be/wms/v2",
                "",
                false,
                14,
                0,
                256, 256,
                "png",
                "ESA WorldCover",
                "CC BY 4.0"
            },
            {
                SourceType::MODIS_LANDCOVER,
                "https://appeears.earthdatacloud.nasa.gov/api/bundle/{request_id}/download/{file}",
                "",  // Requires NASA Earthdata account
                true,
                10,
                0,
                256, 256,
                "tif",
                "NASA MODIS",
                "Public Domain"
            }
        };
    }
};

/**
 * @brief Data source manager with caching
 */
class DataSourceManager {
public:
    DataSourceManager();
    ~DataSourceManager();

    /**
     * @brief Initialize the manager
     */
    bool Initialize(const CacheConfig& config = CacheConfig());

    /**
     * @brief Shutdown and save cache
     */
    void Shutdown();

    /**
     * @brief Query data at geographic location
     * @param source Data source type
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @param zoom Zoom level (higher = more detail)
     * @return Data value (or vector of values for RGB)
     */
    std::vector<float> Query(SourceType source, double latitude, double longitude, int zoom = 10);

    /**
     * @brief Query area and return as texture
     * @param source Data source type
     * @param minLat Minimum latitude
     * @param maxLat Maximum latitude
     * @param minLon Minimum longitude
     * @param maxLon Maximum longitude
     * @param zoom Zoom level
     * @return Raster data as flat array
     */
    std::vector<float> QueryArea(SourceType source,
                                  double minLat, double maxLat,
                                  double minLon, double maxLon,
                                  int zoom = 10,
                                  int& outWidth, int& outHeight);

    /**
     * @brief Prefetch tiles for an area
     */
    void PrefetchArea(SourceType source,
                      double minLat, double maxLat,
                      double minLon, double maxLon,
                      int zoom = 10);

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t tilesInMemory;
        size_t totalMemoryMB;
        size_t tilesOnDisk;
        size_t totalDiskMB;
        size_t cacheHits;
        size_t cacheMisses;
        size_t downloadsInProgress;
        size_t downloadsFailed;
    };
    CacheStats GetCacheStats() const;

    /**
     * @brief Clear cache
     */
    void ClearCache(bool diskOnly = false);

    /**
     * @brief Set API key for a source
     */
    void SetAPIKey(SourceType source, const std::string& apiKey);

    /**
     * @brief Configure data source
     */
    void ConfigureSource(const SourceConfig& config);

    /**
     * @brief Save cache to disk
     */
    void SaveCacheToDisk();

    /**
     * @brief Load cache from disk
     */
    void LoadCacheFromDisk();

private:
    // Tile management
    TileKey LatLonToTile(double latitude, double longitude, int zoom) const;
    void LatLonToTileXY(double latitude, double longitude, int zoom, int& x, int& y) const;
    std::vector<TileKey> GetTilesForArea(double minLat, double maxLat,
                                         double minLon, double maxLon,
                                         int zoom, SourceType source) const;

    // Cache management
    CachedTile* GetTile(const TileKey& key);
    void AddToCache(const TileKey& key, const CachedTile& tile);
    void EvictTiles(size_t targetSizeBytes);
    void UpdateAccessTime(CachedTile& tile);
    size_t GetCacheSizeBytes() const;

    // Download
    void DownloadTile(const TileKey& key);
    std::string BuildTileURL(const TileKey& key) const;
    bool DownloadFromURL(const std::string& url, std::vector<uint8_t>& outData);

    // Disk cache
    std::string GetTileCachePath(const TileKey& key) const;
    bool LoadTileFromDisk(const TileKey& key, CachedTile& outTile);
    void SaveTileToDisk(const TileKey& key, const CachedTile& tile);

    // Data parsing
    bool ParseRasterData(const std::vector<uint8_t>& rawData,
                        const std::string& format,
                        CachedTile& outTile);
    bool ParseVectorData(const std::vector<uint8_t>& rawData,
                        const std::string& format,
                        CachedTile& outTile);

    // State
    CacheConfig m_config;
    std::unordered_map<TileKey, CachedTile> m_cache;
    std::unordered_map<SourceType, SourceConfig> m_sourceConfigs;

    // Statistics
    mutable size_t m_cacheHits = 0;
    mutable size_t m_cacheMisses = 0;
    mutable size_t m_downloadsInProgress = 0;
    mutable size_t m_downloadsFailed = 0;

    // Download queue
    struct DownloadRequest {
        TileKey key;
        std::chrono::system_clock::time_point requestTime;
    };
    std::queue<DownloadRequest> m_downloadQueue;
};

} // namespace DataSource
