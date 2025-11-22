#include "GeoDataProvider.hpp"
#include "GeoTileCache.hpp"
#include <curl/curl.h>
#include <cmath>
#include <thread>
#include <sstream>

namespace Vehement {
namespace Geo {

// =============================================================================
// GeoCoordinate Implementation
// =============================================================================

double GeoCoordinate::DistanceTo(const GeoCoordinate& other) const {
    double lat1 = DegToRad(latitude);
    double lat2 = DegToRad(other.latitude);
    double dLat = DegToRad(other.latitude - latitude);
    double dLon = DegToRad(other.longitude - longitude);

    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1) * std::cos(lat2) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS_METERS * c;
}

double GeoCoordinate::BearingTo(const GeoCoordinate& other) const {
    double lat1 = DegToRad(latitude);
    double lat2 = DegToRad(other.latitude);
    double dLon = DegToRad(other.longitude - longitude);

    double y = std::sin(dLon) * std::cos(lat2);
    double x = std::cos(lat1) * std::sin(lat2) -
               std::sin(lat1) * std::cos(lat2) * std::cos(dLon);

    double bearing = RadToDeg(std::atan2(y, x));
    return std::fmod(bearing + 360.0, 360.0);
}

GeoCoordinate GeoCoordinate::Offset(double distance, double bearing) const {
    double lat1 = DegToRad(latitude);
    double lon1 = DegToRad(longitude);
    double brng = DegToRad(bearing);
    double d = distance / EARTH_RADIUS_METERS;

    double lat2 = std::asin(std::sin(lat1) * std::cos(d) +
                            std::cos(lat1) * std::sin(d) * std::cos(brng));
    double lon2 = lon1 + std::atan2(std::sin(brng) * std::sin(d) * std::cos(lat1),
                                     std::cos(d) - std::sin(lat1) * std::sin(lat2));

    return GeoCoordinate(RadToDeg(lat2), RadToDeg(lon2));
}

glm::ivec2 GeoCoordinate::ToTileXY(int zoom) const {
    int n = 1 << zoom;
    double latRad = DegToRad(latitude);

    int x = static_cast<int>((longitude + 180.0) / 360.0 * n);
    int y = static_cast<int>((1.0 - std::asinh(std::tan(latRad)) / M_PI) / 2.0 * n);

    x = std::clamp(x, 0, n - 1);
    y = std::clamp(y, 0, n - 1);

    return glm::ivec2(x, y);
}

GeoCoordinate GeoCoordinate::FromTileXY(int x, int y, int zoom) {
    int n = 1 << zoom;

    double lon = static_cast<double>(x) / n * 360.0 - 180.0;
    double latRad = std::atan(std::sinh(M_PI * (1 - 2 * static_cast<double>(y) / n)));
    double lat = RadToDeg(latRad);

    return GeoCoordinate(lat, lon);
}

// =============================================================================
// GeoBoundingBox Implementation
// =============================================================================

double GeoBoundingBox::GetWidthMeters() const {
    GeoCoordinate center = GetCenter();
    GeoCoordinate left(center.latitude, min.longitude);
    GeoCoordinate right(center.latitude, max.longitude);
    return left.DistanceTo(right);
}

double GeoBoundingBox::GetHeightMeters() const {
    GeoCoordinate center = GetCenter();
    GeoCoordinate bottom(min.latitude, center.longitude);
    GeoCoordinate top(max.latitude, center.longitude);
    return bottom.DistanceTo(top);
}

bool GeoBoundingBox::Intersects(const GeoBoundingBox& other) const {
    return !(max.longitude < other.min.longitude ||
             min.longitude > other.max.longitude ||
             max.latitude < other.min.latitude ||
             min.latitude > other.max.latitude);
}

void GeoBoundingBox::Expand(const GeoCoordinate& coord) {
    min.latitude = std::min(min.latitude, coord.latitude);
    min.longitude = std::min(min.longitude, coord.longitude);
    max.latitude = std::max(max.latitude, coord.latitude);
    max.longitude = std::max(max.longitude, coord.longitude);
}

GeoBoundingBox GeoBoundingBox::Padded(double margin) const {
    return GeoBoundingBox(
        min.latitude - margin, min.longitude - margin,
        max.latitude + margin, max.longitude + margin
    );
}

GeoBoundingBox GeoBoundingBox::FromCenterRadius(const GeoCoordinate& center, double radiusMeters) {
    GeoCoordinate north = center.Offset(radiusMeters, 0);
    GeoCoordinate south = center.Offset(radiusMeters, 180);
    GeoCoordinate east = center.Offset(radiusMeters, 90);
    GeoCoordinate west = center.Offset(radiusMeters, 270);

    return GeoBoundingBox(
        south.latitude, west.longitude,
        north.latitude, east.longitude
    );
}

GeoBoundingBox GeoBoundingBox::FromTile(int x, int y, int zoom) {
    GeoCoordinate nw = GeoCoordinate::FromTileXY(x, y, zoom);
    GeoCoordinate se = GeoCoordinate::FromTileXY(x + 1, y + 1, zoom);

    return GeoBoundingBox(
        se.latitude, nw.longitude,
        nw.latitude, se.longitude
    );
}

// =============================================================================
// TileID Implementation
// =============================================================================

GeoBoundingBox TileID::GetBounds() const {
    return GeoBoundingBox::FromTile(x, y, zoom);
}

std::string TileID::ToKey() const {
    std::ostringstream oss;
    oss << zoom << "/" << x << "/" << y;
    return oss.str();
}

TileID TileID::FromKey(const std::string& key) {
    TileID tile;
    std::istringstream iss(key);
    char sep;
    iss >> tile.zoom >> sep >> tile.x >> sep >> tile.y;
    return tile;
}

TileID TileID::FromCoordinate(const GeoCoordinate& coord, int zoom) {
    glm::ivec2 xy = coord.ToTileXY(zoom);
    return TileID(xy.x, xy.y, zoom);
}

// =============================================================================
// GeoTileData Implementation
// =============================================================================

bool GeoTileData::IsExpired() const {
    if (expiryTimestamp <= 0) return false;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    return timestamp > expiryTimestamp;
}

const GeoRoad* GeoTileData::GetRoadById(int64_t id) const {
    for (const auto& road : roads) {
        if (road.id == id) return &road;
    }
    return nullptr;
}

const GeoBuilding* GeoTileData::GetBuildingById(int64_t id) const {
    for (const auto& building : buildings) {
        if (building.id == id) return &building;
    }
    return nullptr;
}

const GeoPOI* GeoTileData::GetPOIById(int64_t id) const {
    for (const auto& poi : pois) {
        if (poi.id == id) return &poi;
    }
    return nullptr;
}

void GeoTileData::Clear() {
    roads.clear();
    buildings.clear();
    waterBodies.clear();
    pois.clear();
    landUse.clear();
    elevation = ElevationGrid();
    biome = BiomeData();
    status = DataStatus::None;
    errorMessage.clear();
}

// =============================================================================
// Type Conversion Functions
// =============================================================================

const char* BiomeTypeToString(BiomeType biome) {
    switch (biome) {
        case BiomeType::Desert: return "Desert";
        case BiomeType::Grassland: return "Grassland";
        case BiomeType::Savanna: return "Savanna";
        case BiomeType::Shrubland: return "Shrubland";
        case BiomeType::Forest: return "Forest";
        case BiomeType::TemperateForest: return "TemperateForest";
        case BiomeType::BorealForest: return "BorealForest";
        case BiomeType::TropicalForest: return "TropicalForest";
        case BiomeType::Jungle: return "Jungle";
        case BiomeType::Tundra: return "Tundra";
        case BiomeType::Arctic: return "Arctic";
        case BiomeType::Wetland: return "Wetland";
        case BiomeType::Swamp: return "Swamp";
        case BiomeType::Mangrove: return "Mangrove";
        case BiomeType::Ocean: return "Ocean";
        case BiomeType::Sea: return "Sea";
        case BiomeType::Lake: return "Lake";
        case BiomeType::River: return "River";
        case BiomeType::Coastal: return "Coastal";
        case BiomeType::Farmland: return "Farmland";
        case BiomeType::Orchard: return "Orchard";
        case BiomeType::Vineyard: return "Vineyard";
        case BiomeType::Urban: return "Urban";
        case BiomeType::Suburban: return "Suburban";
        case BiomeType::Industrial: return "Industrial";
        case BiomeType::Commercial: return "Commercial";
        case BiomeType::Residential: return "Residential";
        case BiomeType::Mountain: return "Mountain";
        case BiomeType::Beach: return "Beach";
        case BiomeType::Quarry: return "Quarry";
        case BiomeType::Landfill: return "Landfill";
        case BiomeType::Cemetery: return "Cemetery";
        case BiomeType::Park: return "Park";
        default: return "Unknown";
    }
}

BiomeType BiomeTypeFromString(const std::string& str) {
    if (str == "Desert") return BiomeType::Desert;
    if (str == "Grassland") return BiomeType::Grassland;
    if (str == "Savanna") return BiomeType::Savanna;
    if (str == "Forest") return BiomeType::Forest;
    if (str == "Jungle") return BiomeType::Jungle;
    if (str == "Tundra") return BiomeType::Tundra;
    if (str == "Urban") return BiomeType::Urban;
    if (str == "Suburban") return BiomeType::Suburban;
    // Add more mappings as needed
    return BiomeType::Unknown;
}

const char* RoadTypeToString(RoadType type) {
    switch (type) {
        case RoadType::Motorway: return "Motorway";
        case RoadType::Trunk: return "Trunk";
        case RoadType::Primary: return "Primary";
        case RoadType::Secondary: return "Secondary";
        case RoadType::Tertiary: return "Tertiary";
        case RoadType::Residential: return "Residential";
        case RoadType::Service: return "Service";
        case RoadType::Unclassified: return "Unclassified";
        case RoadType::LivingStreet: return "LivingStreet";
        case RoadType::Pedestrian: return "Pedestrian";
        case RoadType::Footway: return "Footway";
        case RoadType::Cycleway: return "Cycleway";
        case RoadType::Path: return "Path";
        case RoadType::Track: return "Track";
        case RoadType::Steps: return "Steps";
        case RoadType::Rail: return "Rail";
        case RoadType::LightRail: return "LightRail";
        case RoadType::Subway: return "Subway";
        default: return "Unknown";
    }
}

RoadType RoadTypeFromOSM(const std::string& highway) {
    if (highway == "motorway") return RoadType::Motorway;
    if (highway == "trunk") return RoadType::Trunk;
    if (highway == "primary") return RoadType::Primary;
    if (highway == "secondary") return RoadType::Secondary;
    if (highway == "tertiary") return RoadType::Tertiary;
    if (highway == "residential") return RoadType::Residential;
    if (highway == "service") return RoadType::Service;
    if (highway == "unclassified") return RoadType::Unclassified;
    if (highway == "living_street") return RoadType::LivingStreet;
    if (highway == "pedestrian") return RoadType::Pedestrian;
    if (highway == "footway" || highway == "footpath") return RoadType::Footway;
    if (highway == "cycleway") return RoadType::Cycleway;
    if (highway == "path") return RoadType::Path;
    if (highway == "track") return RoadType::Track;
    if (highway == "steps") return RoadType::Steps;
    if (highway == "motorway_link") return RoadType::MotorwayLink;
    if (highway == "trunk_link") return RoadType::TrunkLink;
    if (highway == "primary_link") return RoadType::PrimaryLink;
    if (highway == "secondary_link") return RoadType::SecondaryLink;
    if (highway == "tertiary_link") return RoadType::TertiaryLink;
    return RoadType::Unknown;
}

float GetDefaultRoadWidth(RoadType type) {
    switch (type) {
        case RoadType::Motorway: return 15.0f;
        case RoadType::Trunk: return 12.0f;
        case RoadType::Primary: return 10.0f;
        case RoadType::Secondary: return 8.0f;
        case RoadType::Tertiary: return 7.0f;
        case RoadType::Residential: return 6.0f;
        case RoadType::Service: return 4.0f;
        case RoadType::Unclassified: return 5.0f;
        case RoadType::LivingStreet: return 4.0f;
        case RoadType::Pedestrian: return 3.0f;
        case RoadType::Footway: return 2.0f;
        case RoadType::Cycleway: return 2.5f;
        case RoadType::Path: return 1.5f;
        case RoadType::Track: return 3.0f;
        case RoadType::Steps: return 2.0f;
        case RoadType::Rail: return 4.0f;
        case RoadType::LightRail: return 3.0f;
        case RoadType::Subway: return 4.0f;
        default: return 4.0f;
    }
}

int GetDefaultLaneCount(RoadType type) {
    switch (type) {
        case RoadType::Motorway: return 4;
        case RoadType::Trunk: return 4;
        case RoadType::Primary: return 2;
        case RoadType::Secondary: return 2;
        case RoadType::Tertiary: return 2;
        case RoadType::Residential: return 2;
        default: return 1;
    }
}

const char* BuildingTypeToString(BuildingType type) {
    switch (type) {
        case BuildingType::House: return "House";
        case BuildingType::Apartments: return "Apartments";
        case BuildingType::Commercial: return "Commercial";
        case BuildingType::Industrial: return "Industrial";
        case BuildingType::Office: return "Office";
        case BuildingType::Retail: return "Retail";
        case BuildingType::Warehouse: return "Warehouse";
        case BuildingType::Hospital: return "Hospital";
        case BuildingType::School: return "School";
        case BuildingType::Church: return "Church";
        default: return "Unknown";
    }
}

BuildingType BuildingTypeFromOSM(const std::string& building) {
    if (building == "house" || building == "detached") return BuildingType::House;
    if (building == "apartments" || building == "residential") return BuildingType::Apartments;
    if (building == "commercial") return BuildingType::Commercial;
    if (building == "industrial") return BuildingType::Industrial;
    if (building == "office") return BuildingType::Office;
    if (building == "retail") return BuildingType::Retail;
    if (building == "warehouse") return BuildingType::Warehouse;
    if (building == "hospital") return BuildingType::Hospital;
    if (building == "school") return BuildingType::School;
    if (building == "church" || building == "chapel") return BuildingType::Church;
    if (building == "yes" || building == "building") return BuildingType::Unknown;
    return BuildingType::Unknown;
}

const char* WaterTypeToString(WaterType type) {
    switch (type) {
        case WaterType::Ocean: return "Ocean";
        case WaterType::Sea: return "Sea";
        case WaterType::Lake: return "Lake";
        case WaterType::River: return "River";
        case WaterType::Stream: return "Stream";
        case WaterType::Pond: return "Pond";
        case WaterType::Reservoir: return "Reservoir";
        case WaterType::Canal: return "Canal";
        case WaterType::Wetland: return "Wetland";
        case WaterType::Coastline: return "Coastline";
        default: return "Unknown";
    }
}

WaterType WaterTypeFromOSM(const std::string& natural, const std::string& water, const std::string& waterway) {
    if (natural == "water") {
        if (water == "lake") return WaterType::Lake;
        if (water == "river") return WaterType::River;
        if (water == "pond") return WaterType::Pond;
        if (water == "reservoir") return WaterType::Reservoir;
        return WaterType::Lake;
    }
    if (natural == "coastline") return WaterType::Coastline;
    if (natural == "wetland") return WaterType::Wetland;

    if (!waterway.empty()) {
        if (waterway == "river") return WaterType::River;
        if (waterway == "stream") return WaterType::Stream;
        if (waterway == "canal") return WaterType::Canal;
        if (waterway == "drain") return WaterType::Drain;
    }

    return WaterType::Unknown;
}

const char* POICategoryToString(POICategory category) {
    switch (category) {
        case POICategory::Restaurant: return "Restaurant";
        case POICategory::Cafe: return "Cafe";
        case POICategory::Bar: return "Bar";
        case POICategory::Supermarket: return "Supermarket";
        case POICategory::Hospital: return "Hospital";
        case POICategory::School: return "School";
        case POICategory::Park: return "Park";
        case POICategory::Hotel: return "Hotel";
        case POICategory::BusStop: return "BusStop";
        case POICategory::TrainStation: return "TrainStation";
        case POICategory::FuelStation: return "FuelStation";
        default: return "Unknown";
    }
}

POICategory POICategoryFromOSM(const std::string& amenity, const std::string& shop,
                                const std::string& tourism, const std::string& natural) {
    // Amenity mapping
    if (amenity == "restaurant") return POICategory::Restaurant;
    if (amenity == "fast_food") return POICategory::FastFood;
    if (amenity == "cafe") return POICategory::Cafe;
    if (amenity == "bar") return POICategory::Bar;
    if (amenity == "pub") return POICategory::Pub;
    if (amenity == "bank") return POICategory::Bank;
    if (amenity == "atm") return POICategory::ATM;
    if (amenity == "hospital") return POICategory::Hospital;
    if (amenity == "clinic") return POICategory::Clinic;
    if (amenity == "pharmacy") return POICategory::Pharmacy;
    if (amenity == "school") return POICategory::School;
    if (amenity == "university") return POICategory::University;
    if (amenity == "library") return POICategory::Library;
    if (amenity == "police") return POICategory::Police;
    if (amenity == "fire_station") return POICategory::FireStation;
    if (amenity == "fuel") return POICategory::FuelStation;
    if (amenity == "parking") return POICategory::ParkingLot;
    if (amenity == "place_of_worship") return POICategory::PlaceOfWorship;
    if (amenity == "cinema") return POICategory::Cinema;
    if (amenity == "theatre") return POICategory::Theatre;

    // Shop mapping
    if (shop == "supermarket") return POICategory::Supermarket;
    if (shop == "convenience") return POICategory::Convenience;
    if (shop == "clothes") return POICategory::Clothes;
    if (shop == "electronics") return POICategory::Electronics;
    if (shop == "mall") return POICategory::Mall;

    // Tourism mapping
    if (tourism == "hotel") return POICategory::Hotel;
    if (tourism == "hostel") return POICategory::Hostel;
    if (tourism == "camp_site") return POICategory::Campsite;
    if (tourism == "museum") return POICategory::Museum;
    if (tourism == "viewpoint") return POICategory::Viewpoint;

    // Natural mapping
    if (natural == "peak") return POICategory::Peak;
    if (natural == "beach") return POICategory::Beach;
    if (natural == "spring") return POICategory::Spring;

    return POICategory::Unknown;
}

const char* LandUseTypeToString(LandUseType type) {
    switch (type) {
        case LandUseType::Residential: return "Residential";
        case LandUseType::Commercial: return "Commercial";
        case LandUseType::Industrial: return "Industrial";
        case LandUseType::Forest: return "Forest";
        case LandUseType::Park: return "Park";
        case LandUseType::Farmland: return "Farmland";
        case LandUseType::Grassland: return "Grassland";
        default: return "Unknown";
    }
}

LandUseType LandUseTypeFromOSM(const std::string& landuse, const std::string& natural, const std::string& leisure) {
    if (landuse == "residential") return LandUseType::Residential;
    if (landuse == "commercial" || landuse == "retail") return LandUseType::Commercial;
    if (landuse == "industrial") return LandUseType::Industrial;
    if (landuse == "forest") return LandUseType::Forest;
    if (landuse == "farmland" || landuse == "farm") return LandUseType::Farmland;
    if (landuse == "meadow" || landuse == "grass") return LandUseType::Meadow;
    if (landuse == "orchard") return LandUseType::Orchard;
    if (landuse == "vineyard") return LandUseType::Vineyard;
    if (landuse == "cemetery") return LandUseType::Cemetery;
    if (landuse == "military") return LandUseType::Military;
    if (landuse == "quarry") return LandUseType::Quarry;
    if (landuse == "construction") return LandUseType::Construction;
    if (landuse == "brownfield") return LandUseType::Brownfield;
    if (landuse == "landfill") return LandUseType::Landfill;
    if (landuse == "recreation_ground") return LandUseType::Recreation;
    if (landuse == "railway") return LandUseType::Railway;

    if (natural == "wood") return LandUseType::Wood;
    if (natural == "grassland") return LandUseType::Grassland;
    if (natural == "heath") return LandUseType::Heath;
    if (natural == "scrub") return LandUseType::Scrub;
    if (natural == "wetland") return LandUseType::Wetland;
    if (natural == "beach") return LandUseType::Beach;
    if (natural == "sand") return LandUseType::Sand;
    if (natural == "rock" || natural == "bare_rock") return LandUseType::Rock;

    if (leisure == "park") return LandUseType::Park;
    if (leisure == "playground") return LandUseType::Playground;
    if (leisure == "pitch") return LandUseType::SportsPitch;
    if (leisure == "golf_course") return LandUseType::Golf;

    return LandUseType::Unknown;
}

// =============================================================================
// Utility Functions
// =============================================================================

double CalculatePolygonArea(const std::vector<GeoCoordinate>& polygon) {
    if (polygon.size() < 3) return 0.0;

    double area = 0.0;
    int n = static_cast<int>(polygon.size());

    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += polygon[i].longitude * polygon[j].latitude;
        area -= polygon[j].longitude * polygon[i].latitude;
    }

    return std::abs(area) / 2.0;
}

double CalculatePolygonAreaMeters(const std::vector<GeoCoordinate>& polygon) {
    if (polygon.size() < 3) return 0.0;

    // Use spherical excess formula for more accurate area calculation
    double area = 0.0;
    int n = static_cast<int>(polygon.size());

    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        int k = (i + 2) % n;

        double lat1 = DegToRad(polygon[i].latitude);
        double lon1 = DegToRad(polygon[i].longitude);
        double lat2 = DegToRad(polygon[j].latitude);
        double lon2 = DegToRad(polygon[j].longitude);
        double lat3 = DegToRad(polygon[k].latitude);
        double lon3 = DegToRad(polygon[k].longitude);

        area += (lon2 - lon1) * (2 + std::sin(lat1) + std::sin(lat2));
    }

    area = std::abs(area) * EARTH_RADIUS_METERS * EARTH_RADIUS_METERS / 2.0;
    return area;
}

double CalculatePolylineLength(const std::vector<GeoCoordinate>& polyline) {
    if (polyline.size() < 2) return 0.0;

    double length = 0.0;
    for (size_t i = 1; i < polyline.size(); ++i) {
        length += polyline[i - 1].DistanceTo(polyline[i]);
    }

    return length;
}

GeoCoordinate CalculateCentroid(const std::vector<GeoCoordinate>& polygon) {
    if (polygon.empty()) return GeoCoordinate();
    if (polygon.size() == 1) return polygon[0];

    double latSum = 0.0;
    double lonSum = 0.0;

    for (const auto& coord : polygon) {
        latSum += coord.latitude;
        lonSum += coord.longitude;
    }

    return GeoCoordinate(latSum / polygon.size(), lonSum / polygon.size());
}

bool PointInPolygon(const GeoCoordinate& point, const std::vector<GeoCoordinate>& polygon) {
    if (polygon.size() < 3) return false;

    bool inside = false;
    int n = static_cast<int>(polygon.size());

    for (int i = 0, j = n - 1; i < n; j = i++) {
        const auto& pi = polygon[i];
        const auto& pj = polygon[j];

        if (((pi.latitude > point.latitude) != (pj.latitude > point.latitude)) &&
            (point.longitude < (pj.longitude - pi.longitude) * (point.latitude - pi.latitude) /
             (pj.latitude - pi.latitude) + pi.longitude)) {
            inside = !inside;
        }
    }

    return inside;
}

// =============================================================================
// GeoRoad Implementation
// =============================================================================

double GeoRoad::GetLength() const {
    return CalculatePolylineLength(points);
}

GeoBoundingBox GeoRoad::GetBounds() const {
    if (points.empty()) return GeoBoundingBox();

    GeoBoundingBox bounds(points[0], points[0]);
    for (const auto& p : points) {
        bounds.Expand(p);
    }
    return bounds;
}

bool GeoRoad::IsDrivable() const {
    switch (type) {
        case RoadType::Motorway:
        case RoadType::Trunk:
        case RoadType::Primary:
        case RoadType::Secondary:
        case RoadType::Tertiary:
        case RoadType::Residential:
        case RoadType::Service:
        case RoadType::Unclassified:
        case RoadType::LivingStreet:
        case RoadType::MotorwayLink:
        case RoadType::TrunkLink:
        case RoadType::PrimaryLink:
        case RoadType::SecondaryLink:
        case RoadType::TertiaryLink:
            return true;
        default:
            return false;
    }
}

bool GeoRoad::IsWalkable() const {
    switch (type) {
        case RoadType::Motorway:
        case RoadType::MotorwayLink:
            return false;
        default:
            return true;
    }
}

// =============================================================================
// GeoBuilding Implementation
// =============================================================================

float GeoBuilding::GetEstimatedHeight() const {
    if (height > 0) return height;

    // Estimate from levels
    if (levels > 0) return levels * 3.0f;  // ~3m per floor

    // Default heights by type
    switch (type) {
        case BuildingType::House:
        case BuildingType::Detached:
        case BuildingType::Semidetached:
            return 8.0f;
        case BuildingType::Apartments:
            return 15.0f;
        case BuildingType::Commercial:
        case BuildingType::Office:
            return 20.0f;
        case BuildingType::Industrial:
        case BuildingType::Warehouse:
            return 10.0f;
        case BuildingType::Shed:
        case BuildingType::Garage:
            return 3.0f;
        default:
            return 10.0f;
    }
}

int GeoBuilding::GetEstimatedLevels() const {
    if (levels > 0) return levels;
    return static_cast<int>(GetEstimatedHeight() / 3.0f);
}

double GeoBuilding::GetArea() const {
    return CalculatePolygonAreaMeters(outline);
}

GeoCoordinate GeoBuilding::GetCentroid() const {
    return CalculateCentroid(outline);
}

GeoBoundingBox GeoBuilding::GetBounds() const {
    if (outline.empty()) return GeoBoundingBox();

    GeoBoundingBox bounds(outline[0], outline[0]);
    for (const auto& p : outline) {
        bounds.Expand(p);
    }
    return bounds;
}

// =============================================================================
// GeoWaterBody Implementation
// =============================================================================

GeoBoundingBox GeoWaterBody::GetBounds() const {
    const auto& pts = isArea ? outline : centerline;
    if (pts.empty()) return GeoBoundingBox();

    GeoBoundingBox bounds(pts[0], pts[0]);
    for (const auto& p : pts) {
        bounds.Expand(p);
    }
    return bounds;
}

double GeoWaterBody::GetArea() const {
    if (!isArea) return 0.0;
    return CalculatePolygonAreaMeters(outline);
}

double GeoWaterBody::GetLength() const {
    if (isArea) return 0.0;
    return CalculatePolylineLength(centerline);
}

// =============================================================================
// GeoPOI Implementation
// =============================================================================

GeoBoundingBox GeoPOI::GetBounds() const {
    if (!HasArea()) {
        // Point POI - create small bounding box
        return GeoBoundingBox::FromCenterRadius(location, 10.0);
    }

    GeoBoundingBox bounds(outline[0], outline[0]);
    for (const auto& p : outline) {
        bounds.Expand(p);
    }
    return bounds;
}

// =============================================================================
// GeoLandUse Implementation
// =============================================================================

GeoBoundingBox GeoLandUse::GetBounds() const {
    if (outline.empty()) return GeoBoundingBox();

    GeoBoundingBox bounds(outline[0], outline[0]);
    for (const auto& p : outline) {
        bounds.Expand(p);
    }
    return bounds;
}

double GeoLandUse::GetArea() const {
    return CalculatePolygonAreaMeters(outline);
}

bool GeoLandUse::Contains(const GeoCoordinate& coord) const {
    return PointInPolygon(coord, outline);
}

// =============================================================================
// ElevationGrid Implementation
// =============================================================================

float ElevationGrid::SampleElevation(const GeoCoordinate& coord) const {
    if (width <= 0 || height <= 0 || data.empty()) return noDataValue;
    if (!bounds.Contains(coord)) return noDataValue;

    // Calculate fractional position
    double fx = (coord.longitude - bounds.min.longitude) / bounds.GetWidthDegrees() * (width - 1);
    double fy = (bounds.max.latitude - coord.latitude) / bounds.GetHeightDegrees() * (height - 1);

    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, width - 1);
    int y1 = std::min(y0 + 1, height - 1);

    float fracX = static_cast<float>(fx - x0);
    float fracY = static_cast<float>(fy - y0);

    // Bilinear interpolation
    float e00 = GetElevation(x0, y0);
    float e10 = GetElevation(x1, y0);
    float e01 = GetElevation(x0, y1);
    float e11 = GetElevation(x1, y1);

    // Check for no data values
    if (e00 == noDataValue || e10 == noDataValue || e01 == noDataValue || e11 == noDataValue) {
        return noDataValue;
    }

    float e0 = e00 * (1 - fracX) + e10 * fracX;
    float e1 = e01 * (1 - fracX) + e11 * fracX;

    return e0 * (1 - fracY) + e1 * fracY;
}

float ElevationGrid::CalculateSlope(const GeoCoordinate& coord) const {
    if (width <= 2 || height <= 2) return 0.0f;

    // Calculate grid spacing in meters
    double cellSizeX = bounds.GetWidthMeters() / (width - 1);
    double cellSizeY = bounds.GetHeightMeters() / (height - 1);

    // Get neighboring elevations
    double offset = 0.0001;  // Small offset for neighboring samples
    float eN = SampleElevation(GeoCoordinate(coord.latitude + offset, coord.longitude));
    float eS = SampleElevation(GeoCoordinate(coord.latitude - offset, coord.longitude));
    float eE = SampleElevation(GeoCoordinate(coord.latitude, coord.longitude + offset));
    float eW = SampleElevation(GeoCoordinate(coord.latitude, coord.longitude - offset));

    if (eN == noDataValue || eS == noDataValue || eE == noDataValue || eW == noDataValue) {
        return 0.0f;
    }

    // Calculate slope using finite differences
    float dzdx = (eE - eW) / (2.0f * static_cast<float>(cellSizeX));
    float dzdy = (eN - eS) / (2.0f * static_cast<float>(cellSizeY));

    float slope = std::atan(std::sqrt(dzdx * dzdx + dzdy * dzdy));
    return static_cast<float>(RadToDeg(slope));
}

float ElevationGrid::CalculateAspect(const GeoCoordinate& coord) const {
    if (width <= 2 || height <= 2) return 0.0f;

    double offset = 0.0001;
    float eN = SampleElevation(GeoCoordinate(coord.latitude + offset, coord.longitude));
    float eS = SampleElevation(GeoCoordinate(coord.latitude - offset, coord.longitude));
    float eE = SampleElevation(GeoCoordinate(coord.latitude, coord.longitude + offset));
    float eW = SampleElevation(GeoCoordinate(coord.latitude, coord.longitude - offset));

    if (eN == noDataValue || eS == noDataValue || eE == noDataValue || eW == noDataValue) {
        return 0.0f;
    }

    float dzdx = eE - eW;
    float dzdy = eN - eS;

    float aspect = static_cast<float>(RadToDeg(std::atan2(-dzdy, dzdx)));
    return std::fmod(aspect + 360.0f, 360.0f);
}

std::pair<float, float> ElevationGrid::GetMinMax() const {
    float minVal = std::numeric_limits<float>::max();
    float maxVal = std::numeric_limits<float>::lowest();

    for (float v : data) {
        if (v != noDataValue) {
            minVal = std::min(minVal, v);
            maxVal = std::max(maxVal, v);
        }
    }

    return {minVal, maxVal};
}

std::vector<uint8_t> ElevationGrid::GenerateHeightmap() const {
    std::vector<uint8_t> heightmap(width * height);

    auto [minElev, maxElev] = GetMinMax();
    float range = maxElev - minElev;
    if (range < 0.001f) range = 1.0f;

    for (int i = 0; i < width * height; ++i) {
        float v = data[i];
        if (v == noDataValue) {
            heightmap[i] = 0;
        } else {
            float normalized = (v - minElev) / range;
            heightmap[i] = static_cast<uint8_t>(std::clamp(normalized * 255.0f, 0.0f, 255.0f));
        }
    }

    return heightmap;
}

std::vector<uint8_t> ElevationGrid::GenerateNormalMap() const {
    std::vector<uint8_t> normalMap(width * height * 3);

    float cellSizeX = static_cast<float>(bounds.GetWidthMeters()) / (width - 1);
    float cellSizeY = static_cast<float>(bounds.GetHeightMeters()) / (height - 1);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float eL = GetElevation(std::max(0, x - 1), y);
            float eR = GetElevation(std::min(width - 1, x + 1), y);
            float eU = GetElevation(x, std::max(0, y - 1));
            float eD = GetElevation(x, std::min(height - 1, y + 1));

            glm::vec3 normal(0, 0, 1);

            if (eL != noDataValue && eR != noDataValue && eU != noDataValue && eD != noDataValue) {
                float dzdx = (eR - eL) / (2.0f * cellSizeX);
                float dzdy = (eD - eU) / (2.0f * cellSizeY);

                normal = glm::normalize(glm::vec3(-dzdx, -dzdy, 1.0f));
            }

            int idx = (y * width + x) * 3;
            normalMap[idx + 0] = static_cast<uint8_t>((normal.x * 0.5f + 0.5f) * 255.0f);
            normalMap[idx + 1] = static_cast<uint8_t>((normal.y * 0.5f + 0.5f) * 255.0f);
            normalMap[idx + 2] = static_cast<uint8_t>((normal.z * 0.5f + 0.5f) * 255.0f);
        }
    }

    return normalMap;
}

// =============================================================================
// RateLimiter Implementation
// =============================================================================

RateLimiter::RateLimiter(double requestsPerSecond, int burstSize)
    : m_lastRequest(std::chrono::steady_clock::now())
    , m_minInterval(1.0 / requestsPerSecond)
    , m_burstSize(burstSize)
    , m_availableTokens(burstSize) {
}

bool RateLimiter::Acquire() {
    while (!m_shutdown) {
        if (TryAcquire()) return true;

        int waitMs = GetWaitTime();
        if (waitMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
        }
    }
    return false;
}

bool RateLimiter::TryAcquire() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - m_lastRequest).count();

    // Replenish tokens
    int newTokens = static_cast<int>(elapsed / m_minInterval);
    m_availableTokens = std::min(m_burstSize, m_availableTokens + newTokens);

    if (newTokens > 0) {
        m_lastRequest = now;
    }

    if (m_availableTokens > 0) {
        --m_availableTokens;
        return true;
    }

    return false;
}

int RateLimiter::GetWaitTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_availableTokens > 0) return 0;

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - m_lastRequest).count();
    double remaining = m_minInterval - elapsed;

    return static_cast<int>(std::max(0.0, remaining * 1000.0));
}

void RateLimiter::SetRate(double requestsPerSecond, int burstSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minInterval = 1.0 / requestsPerSecond;
    m_burstSize = burstSize;
}

void RateLimiter::Shutdown() {
    m_shutdown = true;
}

// =============================================================================
// GeoDataProviderBase Implementation
// =============================================================================

GeoDataProviderBase::GeoDataProviderBase()
    : m_rateLimiter(1.0, 3) {
}

GeoDataProviderBase::~GeoDataProviderBase() {
    m_rateLimiter.Shutdown();
}

void GeoDataProviderBase::SetRateLimit(double requestsPerSecond) {
    m_rateLimit = requestsPerSecond;
    m_rateLimiter.SetRate(requestsPerSecond);
}

void GeoDataProviderBase::ResetStatistics() {
    m_requestCount = 0;
    m_cacheHits = 0;
    m_cacheMisses = 0;
    m_bytesDownloaded = 0;
}

bool GeoDataProviderBase::CheckCache(const TileID& tile, GeoTileData& outData) {
    if (!m_cache) return false;

    if (m_cache->Get(tile, outData)) {
        if (!outData.IsExpired()) {
            IncrementCacheHits();
            return true;
        }
    }

    IncrementCacheMisses();
    return false;
}

void GeoDataProviderBase::StoreInCache(const TileID& tile, const GeoTileData& data) {
    if (m_cache) {
        m_cache->Put(tile, data);
    }
}

// =============================================================================
// CurlHttpClient Implementation
// =============================================================================

namespace {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userp) {
    size_t totalSize = size * nitems;
    auto* headers = static_cast<std::unordered_map<std::string, std::string>*>(userp);

    std::string line(buffer, totalSize);
    size_t colonPos = line.find(':');
    if (colonPos != std::string::npos) {
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        // Trim whitespace
        while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
            value = value.substr(1);
        }
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
            value.pop_back();
        }

        (*headers)[key] = value;
    }

    return totalSize;
}

}  // namespace

CurlHttpClient::CurlHttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

CurlHttpClient::~CurlHttpClient() {
    curl_global_cleanup();
}

HttpResponse CurlHttpClient::Get(const std::string& url,
                                   const std::unordered_map<std::string, std::string>& headers) {
    HttpResponse response;
    CURL* curl = curl_easy_init();

    if (!curl) {
        response.error = "Failed to initialize CURL";
        return response;
    }

    auto startTime = std::chrono::steady_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, m_userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Set custom headers
    struct curl_slist* headerList = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        headerList = curl_slist_append(headerList, header.c_str());
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    CURLcode res = curl_easy_perform(curl);

    auto endTime = std::chrono::steady_clock::now();
    response.downloadTime = std::chrono::duration<double>(endTime - startTime).count();

    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        response.statusCode = static_cast<int>(httpCode);

        double downloadSize = 0;
        curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &downloadSize);
        response.downloadSize = static_cast<size_t>(downloadSize);
    } else {
        response.error = curl_easy_strerror(res);
    }

    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    return response;
}

HttpResponse CurlHttpClient::Post(const std::string& url,
                                    const std::string& body,
                                    const std::string& contentType,
                                    const std::unordered_map<std::string, std::string>& headers) {
    HttpResponse response;
    CURL* curl = curl_easy_init();

    if (!curl) {
        response.error = "Failed to initialize CURL";
        return response;
    }

    auto startTime = std::chrono::steady_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, m_userAgent.c_str());

    // Set headers
    struct curl_slist* headerList = nullptr;
    std::string contentTypeHeader = "Content-Type: " + contentType;
    headerList = curl_slist_append(headerList, contentTypeHeader.c_str());

    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        headerList = curl_slist_append(headerList, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    CURLcode res = curl_easy_perform(curl);

    auto endTime = std::chrono::steady_clock::now();
    response.downloadTime = std::chrono::duration<double>(endTime - startTime).count();

    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        response.statusCode = static_cast<int>(httpCode);

        double downloadSize = 0;
        curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &downloadSize);
        response.downloadSize = static_cast<size_t>(downloadSize);
    } else {
        response.error = curl_easy_strerror(res);
    }

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    return response;
}

} // namespace Geo
} // namespace Vehement
