#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <optional>
#include <glm/glm.hpp>

namespace Vehement {
namespace Geo {

// =============================================================================
// Core Geographic Coordinates
// =============================================================================

/**
 * @brief Geographic coordinate (latitude, longitude)
 */
struct GeoCoordinate {
    double latitude = 0.0;   // Degrees, -90 to 90
    double longitude = 0.0;  // Degrees, -180 to 180

    GeoCoordinate() = default;
    GeoCoordinate(double lat, double lon) : latitude(lat), longitude(lon) {}

    bool operator==(const GeoCoordinate& other) const {
        return latitude == other.latitude && longitude == other.longitude;
    }

    bool operator!=(const GeoCoordinate& other) const {
        return !(*this == other);
    }

    /**
     * @brief Calculate distance to another coordinate using Haversine formula
     * @return Distance in meters
     */
    double DistanceTo(const GeoCoordinate& other) const;

    /**
     * @brief Calculate bearing to another coordinate
     * @return Bearing in degrees (0-360, 0 = North)
     */
    double BearingTo(const GeoCoordinate& other) const;

    /**
     * @brief Move coordinate by distance and bearing
     * @param distance Distance in meters
     * @param bearing Bearing in degrees
     * @return New coordinate
     */
    GeoCoordinate Offset(double distance, double bearing) const;

    /**
     * @brief Convert to tile coordinates at given zoom level (Web Mercator)
     */
    glm::ivec2 ToTileXY(int zoom) const;

    /**
     * @brief Create coordinate from tile position
     */
    static GeoCoordinate FromTileXY(int x, int y, int zoom);

    /**
     * @brief Check if coordinate is valid
     */
    bool IsValid() const {
        return latitude >= -90.0 && latitude <= 90.0 &&
               longitude >= -180.0 && longitude <= 180.0;
    }
};

/**
 * @brief Geographic bounding box
 */
struct GeoBoundingBox {
    GeoCoordinate min;  // Southwest corner (min lat, min lon)
    GeoCoordinate max;  // Northeast corner (max lat, max lon)

    GeoBoundingBox() = default;
    GeoBoundingBox(const GeoCoordinate& sw, const GeoCoordinate& ne) : min(sw), max(ne) {}
    GeoBoundingBox(double minLat, double minLon, double maxLat, double maxLon)
        : min(minLat, minLon), max(maxLat, maxLon) {}

    /**
     * @brief Get center of bounding box
     */
    GeoCoordinate GetCenter() const {
        return GeoCoordinate(
            (min.latitude + max.latitude) / 2.0,
            (min.longitude + max.longitude) / 2.0
        );
    }

    /**
     * @brief Get width in degrees
     */
    double GetWidthDegrees() const { return max.longitude - min.longitude; }

    /**
     * @brief Get height in degrees
     */
    double GetHeightDegrees() const { return max.latitude - min.latitude; }

    /**
     * @brief Get approximate width in meters
     */
    double GetWidthMeters() const;

    /**
     * @brief Get approximate height in meters
     */
    double GetHeightMeters() const;

    /**
     * @brief Check if a coordinate is within this box
     */
    bool Contains(const GeoCoordinate& coord) const {
        return coord.latitude >= min.latitude && coord.latitude <= max.latitude &&
               coord.longitude >= min.longitude && coord.longitude <= max.longitude;
    }

    /**
     * @brief Check if this box intersects another
     */
    bool Intersects(const GeoBoundingBox& other) const;

    /**
     * @brief Expand box to include a coordinate
     */
    void Expand(const GeoCoordinate& coord);

    /**
     * @brief Expand box by a margin (in degrees)
     */
    GeoBoundingBox Padded(double margin) const;

    /**
     * @brief Check if box is valid
     */
    bool IsValid() const {
        return min.IsValid() && max.IsValid() &&
               min.latitude <= max.latitude && min.longitude <= max.longitude;
    }

    /**
     * @brief Create from center and radius
     */
    static GeoBoundingBox FromCenterRadius(const GeoCoordinate& center, double radiusMeters);

    /**
     * @brief Create from tile coordinates
     */
    static GeoBoundingBox FromTile(int x, int y, int zoom);
};

// =============================================================================
// Biome Classification
// =============================================================================

/**
 * @brief Biome type enumeration
 */
enum class BiomeType : uint8_t {
    Unknown = 0,

    // Natural biomes
    Desert,
    Grassland,
    Savanna,
    Shrubland,
    Forest,
    TemperateForest,
    BorealForest,
    TropicalForest,
    Jungle,
    Tundra,
    Arctic,
    Wetland,
    Swamp,
    Mangrove,

    // Water biomes
    Ocean,
    Sea,
    Lake,
    River,
    Coastal,

    // Agricultural
    Farmland,
    Orchard,
    Vineyard,

    // Urban biomes
    Urban,
    Suburban,
    Industrial,
    Commercial,
    Residential,

    // Special
    Mountain,
    Beach,
    Quarry,
    Landfill,
    Cemetery,
    Park,

    Count
};

/**
 * @brief Get string name for biome type
 */
const char* BiomeTypeToString(BiomeType biome);

/**
 * @brief Parse biome type from string
 */
BiomeType BiomeTypeFromString(const std::string& str);

/**
 * @brief Biome data with properties
 */
struct BiomeData {
    BiomeType type = BiomeType::Unknown;
    float temperature = 15.0f;       // Average temperature in Celsius
    float precipitation = 500.0f;    // Annual precipitation in mm
    float humidity = 0.5f;           // Relative humidity 0-1
    float foliageDensity = 0.5f;     // Tree/vegetation density 0-1
    float grassDensity = 0.5f;       // Grass coverage 0-1
    float elevation = 0.0f;          // Average elevation in meters
    float slope = 0.0f;              // Average slope in degrees

    // Seasonal variations (multipliers for different seasons)
    float springMultiplier = 1.0f;
    float summerMultiplier = 1.0f;
    float autumnMultiplier = 1.0f;
    float winterMultiplier = 1.0f;

    // Visual properties
    glm::vec3 groundColor = glm::vec3(0.3f, 0.5f, 0.2f);
    std::string primaryTexture;
    std::vector<std::string> foliageModels;
};

// =============================================================================
// Road Data Types
// =============================================================================

/**
 * @brief Road classification
 */
enum class RoadType : uint8_t {
    Unknown = 0,

    // Major roads
    Motorway,           // Highway/Interstate
    Trunk,              // Major arterial
    Primary,            // Primary road
    Secondary,          // Secondary road
    Tertiary,           // Tertiary road

    // Minor roads
    Residential,        // Residential street
    Service,            // Service road
    Unclassified,       // Minor road

    // Special
    LivingStreet,       // Pedestrian priority
    Pedestrian,         // Pedestrian only
    Footway,            // Footpath
    Cycleway,           // Bike path
    Path,               // General path
    Track,              // Unpaved track
    Steps,              // Stairs

    // Links
    MotorwayLink,
    TrunkLink,
    PrimaryLink,
    SecondaryLink,
    TertiaryLink,

    // Rail
    Rail,               // Standard rail
    LightRail,          // Light rail/tram
    Subway,             // Underground rail

    Count
};

/**
 * @brief Get string name for road type
 */
const char* RoadTypeToString(RoadType type);

/**
 * @brief Parse road type from OSM highway tag
 */
RoadType RoadTypeFromOSM(const std::string& highway);

/**
 * @brief Get default width for road type (meters)
 */
float GetDefaultRoadWidth(RoadType type);

/**
 * @brief Get default lane count for road type
 */
int GetDefaultLaneCount(RoadType type);

/**
 * @brief Road surface type
 */
enum class RoadSurface : uint8_t {
    Unknown = 0,
    Asphalt,
    Concrete,
    Paved,
    Gravel,
    Dirt,
    Sand,
    Cobblestone,
    Wood,
    Metal,
    Count
};

/**
 * @brief Geographic road representation
 */
struct GeoRoad {
    int64_t id = 0;                              // OSM way ID
    std::string name;                            // Road name
    std::string ref;                             // Road reference (e.g., "US-101")
    RoadType type = RoadType::Unknown;
    RoadSurface surface = RoadSurface::Unknown;

    std::vector<GeoCoordinate> points;           // Road centerline points

    float width = 0.0f;                          // Road width in meters (0 = use default)
    int lanes = 0;                               // Lane count (0 = use default)
    bool oneway = false;                         // One-way street
    int maxSpeed = 0;                            // Max speed in km/h (0 = unknown)
    bool bridge = false;                         // Is a bridge
    bool tunnel = false;                         // Is a tunnel
    int layer = 0;                               // Layer for bridges/tunnels

    std::unordered_map<std::string, std::string> tags;  // Additional OSM tags

    /**
     * @brief Get effective width (actual or default)
     */
    float GetEffectiveWidth() const {
        return width > 0 ? width : GetDefaultRoadWidth(type);
    }

    /**
     * @brief Get effective lane count
     */
    int GetEffectiveLanes() const {
        return lanes > 0 ? lanes : GetDefaultLaneCount(type);
    }

    /**
     * @brief Get road length in meters
     */
    double GetLength() const;

    /**
     * @brief Get bounding box of road
     */
    GeoBoundingBox GetBounds() const;

    /**
     * @brief Check if road is drivable
     */
    bool IsDrivable() const;

    /**
     * @brief Check if road is walkable
     */
    bool IsWalkable() const;
};

// =============================================================================
// Building Data Types
// =============================================================================

/**
 * @brief Building type classification
 */
enum class BuildingType : uint8_t {
    Unknown = 0,

    // Residential
    House,
    Apartments,
    Detached,
    Semidetached,
    Terrace,
    Dormitory,

    // Commercial
    Commercial,
    Retail,
    Office,
    Supermarket,
    Mall,
    Hotel,

    // Industrial
    Industrial,
    Warehouse,
    Factory,

    // Public/Civic
    Public,
    Civic,
    Government,
    Hospital,
    School,
    University,
    Church,
    Mosque,
    Temple,

    // Transportation
    TrainStation,
    TransportTerminal,
    Hangar,
    Garage,
    Parking,

    // Recreation
    Stadium,
    SportsHall,

    // Utilities
    Service,
    Shed,
    Cabin,
    Farm,
    Barn,

    Count
};

/**
 * @brief Get string name for building type
 */
const char* BuildingTypeToString(BuildingType type);

/**
 * @brief Parse building type from OSM building tag
 */
BuildingType BuildingTypeFromOSM(const std::string& building);

/**
 * @brief Building material/facade type
 */
enum class BuildingMaterial : uint8_t {
    Unknown = 0,
    Brick,
    Stone,
    Concrete,
    Glass,
    Metal,
    Wood,
    Plaster,
    Stucco,
    Vinyl,
    Count
};

/**
 * @brief Building roof type
 */
enum class RoofType : uint8_t {
    Unknown = 0,
    Flat,
    Gabled,
    Hipped,
    Pyramidal,
    Dome,
    Skillion,
    Gambrel,
    Mansard,
    Round,
    Count
};

/**
 * @brief Geographic building representation
 */
struct GeoBuilding {
    int64_t id = 0;                              // OSM way/relation ID
    std::string name;                            // Building name
    BuildingType type = BuildingType::Unknown;

    std::vector<GeoCoordinate> outline;          // Building footprint (closed polygon)
    std::vector<std::vector<GeoCoordinate>> holes;  // Interior holes

    float height = 0.0f;                         // Building height in meters (0 = estimate)
    float minHeight = 0.0f;                      // Minimum height (for building parts)
    int levels = 0;                              // Number of floors (0 = estimate)
    int minLevel = 0;                            // Minimum level (for building parts)

    BuildingMaterial material = BuildingMaterial::Unknown;
    RoofType roofType = RoofType::Unknown;
    float roofHeight = 0.0f;                     // Roof height in meters
    glm::vec3 roofColor = glm::vec3(0.5f);       // Roof color (RGB)
    glm::vec3 wallColor = glm::vec3(0.8f);       // Wall color (RGB)

    std::string address;                         // Street address
    std::unordered_map<std::string, std::string> tags;  // Additional OSM tags

    /**
     * @brief Get estimated height based on levels or type
     */
    float GetEstimatedHeight() const;

    /**
     * @brief Get estimated level count
     */
    int GetEstimatedLevels() const;

    /**
     * @brief Calculate footprint area in square meters
     */
    double GetArea() const;

    /**
     * @brief Get centroid of building
     */
    GeoCoordinate GetCentroid() const;

    /**
     * @brief Get bounding box
     */
    GeoBoundingBox GetBounds() const;
};

// =============================================================================
// Water Body Data Types
// =============================================================================

/**
 * @brief Water body type
 */
enum class WaterType : uint8_t {
    Unknown = 0,
    Ocean,
    Sea,
    Lake,
    Reservoir,
    Pond,
    River,
    Stream,
    Canal,
    Drain,
    Wetland,
    Marsh,
    Swamp,
    Bay,
    Strait,
    Coastline,
    Count
};

/**
 * @brief Get string name for water type
 */
const char* WaterTypeToString(WaterType type);

/**
 * @brief Parse water type from OSM tags
 */
WaterType WaterTypeFromOSM(const std::string& natural, const std::string& water, const std::string& waterway);

/**
 * @brief Geographic water body representation
 */
struct GeoWaterBody {
    int64_t id = 0;                              // OSM way/relation ID
    std::string name;                            // Water body name
    WaterType type = WaterType::Unknown;

    // For area water bodies (lakes, etc.)
    std::vector<GeoCoordinate> outline;          // Shoreline polygon
    std::vector<std::vector<GeoCoordinate>> islands;  // Islands within

    // For linear water bodies (rivers, streams)
    std::vector<GeoCoordinate> centerline;       // River centerline
    float width = 0.0f;                          // Average width in meters

    bool isArea = true;                          // Area (true) or linear (false)
    bool intermittent = false;                   // Seasonal/intermittent
    bool tidal = false;                          // Tidal influence

    std::unordered_map<std::string, std::string> tags;  // Additional OSM tags

    /**
     * @brief Get bounding box
     */
    GeoBoundingBox GetBounds() const;

    /**
     * @brief Calculate area in square meters (for area water bodies)
     */
    double GetArea() const;

    /**
     * @brief Calculate length in meters (for linear water bodies)
     */
    double GetLength() const;
};

// =============================================================================
// Point of Interest (POI) Data Types
// =============================================================================

/**
 * @brief POI category
 */
enum class POICategory : uint8_t {
    Unknown = 0,

    // Food & Drink
    Restaurant,
    FastFood,
    Cafe,
    Bar,
    Pub,

    // Shopping
    Supermarket,
    Convenience,
    Clothes,
    Electronics,
    Pharmacy,
    Mall,

    // Services
    Bank,
    ATM,
    PostOffice,
    Hospital,
    Clinic,
    Police,
    FireStation,

    // Transport
    BusStop,
    TrainStation,
    SubwayStation,
    ParkingLot,
    FuelStation,
    ChargingStation,

    // Recreation
    Park,
    Playground,
    SportsCenter,
    Stadium,
    Cinema,
    Theatre,
    Museum,

    // Accommodation
    Hotel,
    Hostel,
    Campsite,

    // Education
    School,
    University,
    Library,

    // Religious
    PlaceOfWorship,
    Cemetery,

    // Natural
    Peak,
    Viewpoint,
    Beach,
    Spring,

    // Other
    Landmark,
    Monument,
    Memorial,
    Fountain,

    Count
};

/**
 * @brief Get string name for POI category
 */
const char* POICategoryToString(POICategory category);

/**
 * @brief Parse POI category from OSM amenity/shop/tourism tags
 */
POICategory POICategoryFromOSM(const std::string& amenity, const std::string& shop,
                                const std::string& tourism, const std::string& natural);

/**
 * @brief Geographic point of interest
 */
struct GeoPOI {
    int64_t id = 0;                              // OSM node/way ID
    std::string name;                            // POI name
    POICategory category = POICategory::Unknown;
    GeoCoordinate location;                      // POI location

    std::string address;                         // Street address
    std::string phone;                           // Phone number
    std::string website;                         // Website URL
    std::string openingHours;                    // Opening hours

    // For area POIs
    std::vector<GeoCoordinate> outline;          // Area boundary (if applicable)

    std::unordered_map<std::string, std::string> tags;  // Additional OSM tags

    /**
     * @brief Check if POI has area data
     */
    bool HasArea() const { return !outline.empty(); }

    /**
     * @brief Get bounding box (if area POI)
     */
    GeoBoundingBox GetBounds() const;
};

// =============================================================================
// Land Use Data Types
// =============================================================================

/**
 * @brief Land use classification
 */
enum class LandUseType : uint8_t {
    Unknown = 0,

    // Urban
    Residential,
    Commercial,
    Industrial,
    Retail,
    Institutional,

    // Agricultural
    Farmland,
    Meadow,
    Orchard,
    Vineyard,
    Allotments,

    // Natural
    Forest,
    Wood,
    Grassland,
    Heath,
    Scrub,
    Wetland,
    Marsh,
    Beach,
    Sand,
    Rock,

    // Recreation
    Recreation,
    Park,
    Playground,
    SportsPitch,
    Golf,

    // Infrastructure
    Railway,
    Highway,
    Parking,
    Construction,
    Brownfield,
    Landfill,

    // Special
    Cemetery,
    Military,
    Quarry,
    Basin,
    Reservoir,

    Count
};

/**
 * @brief Get string name for land use type
 */
const char* LandUseTypeToString(LandUseType type);

/**
 * @brief Parse land use type from OSM landuse/natural tags
 */
LandUseType LandUseTypeFromOSM(const std::string& landuse, const std::string& natural, const std::string& leisure);

/**
 * @brief Geographic land use area
 */
struct GeoLandUse {
    int64_t id = 0;                              // OSM way/relation ID
    std::string name;                            // Area name
    LandUseType type = LandUseType::Unknown;

    std::vector<GeoCoordinate> outline;          // Area boundary
    std::vector<std::vector<GeoCoordinate>> holes;  // Interior holes

    std::unordered_map<std::string, std::string> tags;  // Additional OSM tags

    /**
     * @brief Get bounding box
     */
    GeoBoundingBox GetBounds() const;

    /**
     * @brief Calculate area in square meters
     */
    double GetArea() const;

    /**
     * @brief Check if point is inside land use area
     */
    bool Contains(const GeoCoordinate& coord) const;
};

// =============================================================================
// Elevation Data Types
// =============================================================================

/**
 * @brief Elevation data point
 */
struct ElevationPoint {
    GeoCoordinate coord;
    float elevation = 0.0f;                      // Elevation in meters
    float slope = 0.0f;                          // Slope in degrees
    float aspect = 0.0f;                         // Aspect in degrees (0 = North)
};

/**
 * @brief Elevation grid data
 */
struct ElevationGrid {
    GeoBoundingBox bounds;
    int width = 0;                               // Grid width in samples
    int height = 0;                              // Grid height in samples
    std::vector<float> data;                     // Elevation values (row-major)
    float noDataValue = -9999.0f;                // Value indicating no data

    /**
     * @brief Get elevation at grid position
     */
    float GetElevation(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return noDataValue;
        return data[y * width + x];
    }

    /**
     * @brief Set elevation at grid position
     */
    void SetElevation(int x, int y, float elevation) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            data[y * width + x] = elevation;
        }
    }

    /**
     * @brief Sample elevation at coordinate (bilinear interpolation)
     */
    float SampleElevation(const GeoCoordinate& coord) const;

    /**
     * @brief Calculate slope at coordinate
     */
    float CalculateSlope(const GeoCoordinate& coord) const;

    /**
     * @brief Calculate aspect at coordinate
     */
    float CalculateAspect(const GeoCoordinate& coord) const;

    /**
     * @brief Get min/max elevation values
     */
    std::pair<float, float> GetMinMax() const;

    /**
     * @brief Generate heightmap texture data (normalized 0-1)
     */
    std::vector<uint8_t> GenerateHeightmap() const;

    /**
     * @brief Generate normal map texture data
     */
    std::vector<uint8_t> GenerateNormalMap() const;
};

// =============================================================================
// Combined Tile Data
// =============================================================================

/**
 * @brief Tile identifier
 */
struct TileID {
    int x = 0;
    int y = 0;
    int zoom = 0;

    TileID() = default;
    TileID(int x_, int y_, int z_) : x(x_), y(y_), zoom(z_) {}

    bool operator==(const TileID& other) const {
        return x == other.x && y == other.y && zoom == other.zoom;
    }

    /**
     * @brief Get bounding box for this tile
     */
    GeoBoundingBox GetBounds() const;

    /**
     * @brief Get string key for caching
     */
    std::string ToKey() const;

    /**
     * @brief Parse from key string
     */
    static TileID FromKey(const std::string& key);

    /**
     * @brief Get tile ID from coordinate at zoom level
     */
    static TileID FromCoordinate(const GeoCoordinate& coord, int zoom);
};

/**
 * @brief Hash function for TileID
 */
struct TileIDHash {
    size_t operator()(const TileID& id) const {
        return std::hash<int>()(id.x) ^ (std::hash<int>()(id.y) << 1) ^ (std::hash<int>()(id.zoom) << 2);
    }
};

/**
 * @brief Data status flags
 */
enum class DataStatus : uint8_t {
    None = 0,
    Pending = 1,
    Loading = 2,
    Loaded = 3,
    Error = 4,
    Cached = 5
};

/**
 * @brief Combined geographic tile data for PCG
 */
struct GeoTileData {
    TileID tileId;
    GeoBoundingBox bounds;
    DataStatus status = DataStatus::None;
    std::string errorMessage;

    // Feature data
    std::vector<GeoRoad> roads;
    std::vector<GeoBuilding> buildings;
    std::vector<GeoWaterBody> waterBodies;
    std::vector<GeoPOI> pois;
    std::vector<GeoLandUse> landUse;

    // Terrain data
    ElevationGrid elevation;
    BiomeData biome;

    // Metadata
    int64_t fetchTimestamp = 0;                  // Unix timestamp when fetched
    int64_t expiryTimestamp = 0;                 // Unix timestamp when data expires
    std::string sourceVersion;                   // Data source version

    /**
     * @brief Check if tile has any data
     */
    bool HasData() const {
        return !roads.empty() || !buildings.empty() || !waterBodies.empty() ||
               !pois.empty() || !landUse.empty() || elevation.width > 0;
    }

    /**
     * @brief Check if data is expired
     */
    bool IsExpired() const;

    /**
     * @brief Get road by ID
     */
    const GeoRoad* GetRoadById(int64_t id) const;

    /**
     * @brief Get building by ID
     */
    const GeoBuilding* GetBuildingById(int64_t id) const;

    /**
     * @brief Get POI by ID
     */
    const GeoPOI* GetPOIById(int64_t id) const;

    /**
     * @brief Clear all data
     */
    void Clear();
};

// =============================================================================
// Query Options
// =============================================================================

/**
 * @brief Options for geographic data queries
 */
struct GeoQueryOptions {
    bool fetchRoads = true;
    bool fetchBuildings = true;
    bool fetchWater = true;
    bool fetchPOIs = true;
    bool fetchLandUse = true;
    bool fetchElevation = true;
    bool fetchBiome = true;

    // Filter options
    std::vector<RoadType> roadTypes;             // Empty = all types
    std::vector<BuildingType> buildingTypes;     // Empty = all types
    std::vector<POICategory> poiCategories;      // Empty = all categories

    // Caching
    bool useCache = true;
    bool forceRefresh = false;
    int cacheExpiryHours = 24 * 7;               // 1 week default

    // Performance
    int maxFeatures = 10000;                     // Max features per category
    float minBuildingArea = 10.0f;               // Min building area (sq meters)
    float minRoadLength = 5.0f;                  // Min road segment length (meters)
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Calculate polygon area using Shoelace formula (in square degrees)
 */
double CalculatePolygonArea(const std::vector<GeoCoordinate>& polygon);

/**
 * @brief Calculate polygon area in square meters
 */
double CalculatePolygonAreaMeters(const std::vector<GeoCoordinate>& polygon);

/**
 * @brief Calculate polyline length in meters
 */
double CalculatePolylineLength(const std::vector<GeoCoordinate>& polyline);

/**
 * @brief Calculate centroid of polygon
 */
GeoCoordinate CalculateCentroid(const std::vector<GeoCoordinate>& polygon);

/**
 * @brief Check if point is inside polygon (ray casting)
 */
bool PointInPolygon(const GeoCoordinate& point, const std::vector<GeoCoordinate>& polygon);

/**
 * @brief Convert degrees to radians
 */
constexpr double DegToRad(double deg) { return deg * 3.14159265358979323846 / 180.0; }

/**
 * @brief Convert radians to degrees
 */
constexpr double RadToDeg(double rad) { return rad * 180.0 / 3.14159265358979323846; }

/**
 * @brief Earth's mean radius in meters
 */
constexpr double EARTH_RADIUS_METERS = 6371000.0;

} // namespace Geo
} // namespace Vehement
