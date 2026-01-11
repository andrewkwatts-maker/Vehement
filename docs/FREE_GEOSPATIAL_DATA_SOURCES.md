# Free Geospatial Data Sources for PCG System

## Overview

This document catalogs free, publicly available geospatial data sources that can be queried programmatically for procedural content generation.

## Elevation Data

### 1. SRTM (Shuttle Radar Topography Mission)
- **Resolution**: 30m (1 arc-second) and 90m (3 arc-second)
- **Coverage**: Global (60°N to 56°S)
- **Format**: HGT (height) files
- **API**: AWS Open Data
  ```
  https://elevation-tiles-prod.s3.amazonaws.com/skadi/{z}/{x}/{y}.hgt
  ```
- **License**: Public Domain
- **Use Cases**: Terrain height, slope calculation, valley/ridge detection
- **Node**: `SRTMElevationNode`

### 2. Copernicus DEM
- **Resolution**: 30m and 90m
- **Coverage**: Global
- **Format**: GeoTIFF
- **API**: AWS Open Data, OpenTopography
  ```
  https://copernicus-dem-30m.s3.amazonaws.com/
  ```
- **License**: CC BY 4.0
- **Use Cases**: High-quality elevation, better than SRTM in some regions
- **Node**: `CopernicusDEMNode`

### 3. ASTER GDEM
- **Resolution**: 30m
- **Coverage**: Global (83°N to 83°S)
- **Format**: GeoTIFF
- **API**: NASA Earthdata
- **License**: Public Domain
- **Use Cases**: Alternative to SRTM, good for areas with no SRTM coverage

### 4. ALOS World 3D
- **Resolution**: 30m
- **Coverage**: Global
- **Format**: GeoTIFF
- **API**: OpenTopography
- **License**: CC BY 4.0
- **Use Cases**: High-quality DEM, especially good for Asia-Pacific

## Satellite Imagery

### 5. Sentinel-2
- **Resolution**: 10m (RGB), 20m (RedEdge), 60m (other bands)
- **Coverage**: Global, every 5 days
- **Format**: JPEG2000, GeoTIFF
- **API**:
  - Sentinel Hub (requires free account): `https://services.sentinel-hub.com/`
  - AWS Open Data: `https://registry.opendata.aws/sentinel-2/`
  - Google Earth Engine
- **License**: CC BY-SA 3.0 IGO
- **Bands**:
  - RGB: True color imagery
  - NDVI: Vegetation index (0.0-1.0, higher = more vegetation)
  - NDWI: Water index
  - NDBI: Built-up index
- **Use Cases**:
  - Texture mapping (RGB)
  - Vegetation detection (NDVI)
  - Water detection (NDWI)
  - Urban area detection (NDBI)
- **Nodes**: `Sentinel2Node`, `Sentinel2NDVINode`

### 6. Landsat 8/9
- **Resolution**: 30m (multispectral), 15m (panchromatic)
- **Coverage**: Global, every 16 days
- **Format**: GeoTIFF
- **API**: USGS EarthExplorer, AWS Open Data
  ```
  https://landsatlook.usgs.gov/tile-services/
  ```
- **License**: Public Domain
- **Use Cases**: Long-term change detection, RGB imagery
- **Node**: `Landsat8Node`

### 7. MODIS
- **Resolution**: 250m, 500m, 1km
- **Coverage**: Global, daily
- **Format**: HDF, GeoTIFF
- **API**: NASA MODIS, Google Earth Engine
- **License**: Public Domain
- **Use Cases**:
  - NDVI for large-scale vegetation
  - Land cover classification
  - Temperature
- **Node**: `MODISNDVINode`, `MODISLandCoverNode`

## Vector Data (Roads, Buildings, Land Use)

### 8. OpenStreetMap (OSM)
- **Resolution**: Vector (various detail levels)
- **Coverage**: Global, crowd-sourced
- **Format**: XML, PBF, GeoJSON
- **API**:
  - Overpass API: `https://overpass-api.de/api/interpreter`
  - Overpass Turbo (web interface): `https://overpass-turbo.eu/`
  - OSM tiles: `https://tile.openstreetmap.org/{z}/{x}/{y}.png`
- **License**: ODbL (Open Database License)
- **Queries**:
  ```
  // Get roads in bbox
  [out:json];
  way["highway"](bbox: south, west, north, east);
  out geom;

  // Get buildings
  [out:json];
  way["building"](bbox: south, west, north, east);
  out geom;

  // Get landuse
  [out:json];
  (
    way["landuse"](bbox: south, west, north, east);
    relation["landuse"](bbox: south, west, north, east);
  );
  out geom;
  ```
- **Use Cases**:
  - Avoid placing trees on roads
  - Avoid placing objects in buildings
  - Urban vs rural classification
  - Path generation following real roads
- **Nodes**: `OSMRoadsNode`, `OSMBuildingsNode`, `OSMLandUseNode`, `OSMWaterNode`

## Land Cover / Land Use

### 9. ESA WorldCover
- **Resolution**: 10m
- **Coverage**: Global
- **Format**: GeoTIFF
- **API**: ESA WorldCover WMS
  ```
  https://services.terrascope.be/wms/v2
  ```
- **License**: CC BY 4.0
- **Classes**:
  - 10: Tree cover
  - 20: Shrubland
  - 30: Grassland
  - 40: Cropland
  - 50: Built-up
  - 60: Bare / sparse vegetation
  - 70: Snow and ice
  - 80: Permanent water bodies
  - 90: Herbaceous wetland
  - 95: Mangroves
  - 100: Moss and lichen
- **Use Cases**:
  - Biome detection
  - Vegetation type selection
  - Urban area detection
- **Node**: `ESAWorldCoverNode`

### 10. MODIS Land Cover (MCD12Q1)
- **Resolution**: 500m
- **Coverage**: Global, annual
- **Format**: HDF, GeoTIFF
- **API**: NASA MODIS, Google Earth Engine
- **License**: Public Domain
- **Use Cases**: Biome classification for large areas

### 11. CORINE Land Cover (Europe only)
- **Resolution**: 100m
- **Coverage**: Europe
- **Format**: GeoTIFF
- **API**: Copernicus Land Monitoring Service
- **License**: Free for non-commercial
- **Use Cases**: Detailed European land cover

## Climate / Weather

### 12. OpenWeatherMap
- **Resolution**: Variable (city-level to grid)
- **Coverage**: Global
- **Format**: JSON
- **API**:
  ```
  https://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={API_KEY}
  https://tile.openweathermap.org/map/temp_new/{z}/{x}/{y}.png?appid={API_KEY}
  ```
- **License**: CC BY-SA 4.0 (free tier: 60 calls/min)
- **Data**: Current temperature, precipitation, wind, clouds
- **Use Cases**: Dynamic weather, seasonal variation
- **Node**: `OpenWeatherTempNode`

### 13. WorldClim
- **Resolution**: 30 arc-seconds (~1km) to 10 arc-minutes (~20km)
- **Coverage**: Global
- **Format**: GeoTIFF
- **API**: Direct download from worldclim.org
  ```
  https://biogeo.ucdavis.edu/data/worldclim/v2.1/base/wc2.1_30s_bio_{variable}.tif
  ```
- **License**: CC BY-SA 4.0
- **Variables**:
  - Temperature (monthly averages)
  - Precipitation (monthly totals)
  - 19 bioclimatic variables
- **Use Cases**:
  - Long-term climate for biome selection
  - Seasonal vegetation changes
- **Node**: `WorldClimPrecipNode`, `WorldClimBioClimNode`

### 14. ERA5 (European Reanalysis)
- **Resolution**: 30km
- **Coverage**: Global, hourly since 1950
- **Format**: NetCDF, GRIB
- **API**: Copernicus Climate Data Store
- **License**: Free (requires account)
- **Use Cases**: Historical weather, climate analysis

## Population / Urban

### 15. WorldPop
- **Resolution**: 100m
- **Coverage**: Global
- **Format**: GeoTIFF
- **API**: WorldPop FTP, GEE
  ```
  https://data.worldpop.org/GIS/Population/Global_2000_2020/
  ```
- **License**: CC BY 4.0
- **Data**: Population density (people per 100m grid cell)
- **Use Cases**:
  - Urban vs rural areas
  - Population-based resource distribution
  - City placement
- **Node**: `WorldPopDensityNode`

### 16. Global Human Settlement (GHS)
- **Resolution**: 10m (built-up), 1km (population)
- **Coverage**: Global
- **Format**: GeoTIFF
- **API**: EU Joint Research Centre
- **License**: CC BY 4.0
- **Use Cases**: Urban extent, building footprints

### 17. LandScan (Oak Ridge National Lab)
- **Resolution**: 1km
- **Coverage**: Global, annual
- **Format**: GeoTIFF
- **API**: landscan.ornl.gov
- **License**: Free for research/education
- **Use Cases**: High-quality population density

## Additional Data Sources

### 18. Natural Earth
- **Resolution**: Vector (1:10m, 1:50m, 1:110m scales)
- **Coverage**: Global
- **Format**: Shapefile, GeoJSON
- **API**: naturalearthdata.com
- **License**: Public Domain
- **Data**: Countries, rivers, lakes, roads, railroads, airports
- **Use Cases**: Large-scale features, borders

### 19. GEBCO (Bathymetry)
- **Resolution**: 15 arc-seconds (~450m)
- **Coverage**: Global ocean floors
- **Format**: NetCDF, GeoTIFF
- **API**: gebco.net
- **License**: Free
- **Use Cases**: Ocean depth, underwater terrain

### 20. Global Forest Watch
- **Resolution**: 30m
- **Coverage**: Global forests
- **Format**: Raster tiles
- **API**: globalforestwatch.org API
- **License**: CC BY 4.0
- **Data**: Forest cover, deforestation, tree height
- **Use Cases**: Forest density, tree placement

## API Access Patterns

### Tile-Based Access (Most Common)
```
https://server.com/tiles/{z}/{x}/{y}.{format}
```
Where:
- `z` = Zoom level (0-18 typically, higher = more detail)
- `x` = Tile X coordinate
- `y` = Tile Y coordinate
- `format` = png, jpg, tif, geojson, etc.

### Point Query
```
https://server.com/api/query?lat={latitude}&lon={longitude}
```

### Bounding Box Query
```
https://server.com/api/query?bbox={west},{south},{east},{north}
```

### Overpass QL (OSM)
```
[out:json];
node["amenity"="school"](around:1000, 40.7128, -74.0060);
out;
```

## Caching Strategy

### Memory Cache
```cpp
struct CacheConfig {
    size_t maxCacheSizeMB = 1024;  // 1GB RAM
    EvictionPolicy policy = LRU;
    int maxConcurrentDownloads = 4;
};
```

### Disk Cache
```cpp
struct DiskCacheConfig {
    std::string cachePath = "cache/geodata/";
    size_t maxDiskCacheSizeMB = 10240;  // 10GB disk
    bool compressOnDisk = true;  // Use zlib compression
    int cacheDurationDays = 30;  // Delete after 30 days
};
```

### Cache Key Format
```
{source_type}/{z}/{x}/{y}.{format}

Examples:
SRTM_30m/10/512/384.hgt
SENTINEL2_RGB/14/8234/5678.jpg
OSM_ROADS/16/32942/22653.geojson
```

### LRU Eviction
```cpp
void EvictLRU(size_t targetSizeBytes) {
    // Sort tiles by last access time
    std::sort(tiles.begin(), tiles.end(),
        [](auto& a, auto& b) { return a.lastAccess < b.lastAccess; });

    // Remove oldest until under target
    while (GetCacheSize() > targetSizeBytes && !tiles.empty()) {
        RemoveTile(tiles.front());
        tiles.erase(tiles.begin());
    }
}
```

## Example PCG Graphs Using Free Data

### Example 1: Realistic Forest Placement
```
[Lat/Long] -> [Sentinel-2 NDVI] -> [Threshold > 0.5] -> [AND] -> [Tree Scatter]
                                                          |
[Lat/Long] -> [OSM Roads] -> [Distance] -> [Threshold > 50m] -|
                                                          |
[Lat/Long] -> [ESA WorldCover] -> [Is Forest?] ----------|
```

### Example 2: Elevation-Based Biome Selection
```
[Lat/Long] -> [SRTM Elevation] -> [Remap] -> [Biome Selector]
                                   (0-500m = Jungle)
                                   (500-2000m = Forest)
                                   (2000-3500m = Alpine)
                                   (3500m+ = Snow)
```

### Example 3: Urban Area Detection
```
[Lat/Long] -> [WorldPop Density] -> [Threshold > 1000] -> [OR] -> [Is Urban]
                                                            |
[Lat/Long] -> [OSM Buildings] -> [Density > 100/km²] ------|
                                                            |
[Lat/Long] -> [ESA WorldCover] -> [Type == Built-up] ------|
```

### Example 4: Climate-Appropriate Vegetation
```
[Lat/Long] -> [WorldClim Temp] -> [Remap] -> [Tree Type]
              [WorldClim Precip]    |         (Temp + Precip determines species)
                    |               |
                    |---------------|
```

### Example 5: Avoid All Obstacles
```
[Lat/Long] -> [OSM Roads] -> [Distance < 10m] -> [OR] -> [NOT] -> [Tree Scatter]
[Lat/Long] -> [OSM Buildings] -> [Distance < 50m] -|
[Lat/Long] -> [OSM Water] -> [Is Water?] ----------|
[Lat/Long] -> [SRTM Elevation] -> [Slope > 30°] ---|
```

## API Rate Limits

| Source | Rate Limit | Free Tier | Notes |
|--------|------------|-----------|-------|
| OpenWeatherMap | 60 calls/min | 1000 calls/day | Requires API key |
| Sentinel Hub | 1500 requests/month | Free account required | Pay for more |
| Overpass API | No hard limit | Use responsibly | Self-hosted option available |
| AWS Open Data | No limit | Free | SRTM, Copernicus, Sentinel-2, Landsat |
| Google Earth Engine | ~50 requests/sec | Free for research | Requires account |

## Best Practices

### 1. Cache Aggressively
- Elevation data rarely changes → Cache indefinitely
- Satellite imagery changes → Cache for 1-7 days
- Weather data changes → Cache for hours
- Vector data (OSM) changes → Cache for weeks

### 2. Prefetch Intelligently
```cpp
// Prefetch surrounding tiles when user views an area
void PrefetchSurroundingTiles(TileKey center, int radius = 1) {
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            TileKey neighbor = {center.zoom, center.x + dx, center.y + dy, center.source};
            if (!IsInCache(neighbor)) {
                QueueDownload(neighbor);
            }
        }
    }
}
```

### 3. Use Appropriate Zoom Levels
- World-scale (continents): z=0-5
- Country-scale: z=6-9
- City-scale: z=10-13
- Neighborhood-scale: z=14-16
- Building-scale: z=17-18

### 4. Handle Missing Data Gracefully
```cpp
float QueryElevation(double lat, double lon) {
    auto result = dataSource->Query(SRTM_30m, lat, lon, 10);
    if (result.empty() || result.hasError) {
        // Fallback to lower resolution
        result = dataSource->Query(SRTM_90m, lat, lon, 9);
    }
    if (result.empty()) {
        // Fallback to procedural generation
        return GenerateProceduralElevation(lat, lon);
    }
    return result.value;
}
```

### 5. Respect Licenses and Attribution
Always display attribution in your application:
```cpp
std::string GetAttribution() {
    return "Elevation: NASA SRTM | "
           "Imagery: ESA Sentinel-2 | "
           "Roads: © OpenStreetMap contributors | "
           "Weather: OpenWeatherMap";
}
```

## Implementation Checklist

- [x] DataSourceManager.hpp created
- [x] PCGNodeTypes.hpp with query nodes created
- [ ] Implement HTTP download (use libcurl or httplib)
- [ ] Implement GeoTIFF parser (use GDAL or tinygeotiff)
- [ ] Implement tile coordinate conversion (lat/lon <-> tile XY)
- [ ] Implement LRU cache eviction
- [ ] Implement disk cache with compression
- [ ] Implement async download queue
- [ ] Add progress bars for downloads in UI
- [ ] Add cache statistics panel in editor
- [ ] Add data source configuration UI
- [ ] Test with each data source
- [ ] Add API key management

## Next Steps

1. Implement basic HTTP downloader
2. Implement tile coordinate math (Web Mercator projection)
3. Parse GeoTIFF files for elevation data
4. Parse PNG/JPG for satellite imagery
5. Parse GeoJSON for vector data
6. Create cache database (SQLite)
7. Integrate with PCG node graph
8. Add UI for configuring sources and cache
