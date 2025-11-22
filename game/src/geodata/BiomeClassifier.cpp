#include "BiomeClassifier.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace Vehement {
namespace Geo {

// =============================================================================
// BiomeConfig Implementation
// =============================================================================

BiomeConfig BiomeConfig::LoadFromFile(const std::string& path) {
    BiomeConfig config;

    std::ifstream file(path);
    if (!file.is_open()) return config;

    try {
        nlohmann::json json;
        file >> json;

        if (json.contains("climateDataPath")) config.climateDataPath = json["climateDataPath"];
        if (json.contains("useOnlineClimate")) config.useOnlineClimate = json["useOnlineClimate"];
        if (json.contains("urbanDensityThreshold")) config.urbanDensityThreshold = json["urbanDensityThreshold"];
        if (json.contains("forestCoverThreshold")) config.forestCoverThreshold = json["forestCoverThreshold"];
        if (json.contains("currentMonth")) config.currentMonth = json["currentMonth"];

    } catch (...) {}

    return config;
}

void BiomeConfig::SaveToFile(const std::string& path) const {
    nlohmann::json json;

    json["climateDataPath"] = climateDataPath;
    json["useOnlineClimate"] = useOnlineClimate;
    json["urbanDensityThreshold"] = urbanDensityThreshold;
    json["forestCoverThreshold"] = forestCoverThreshold;
    json["currentMonth"] = currentMonth;

    std::ofstream file(path);
    if (file.is_open()) {
        file << json.dump(2);
    }
}

// =============================================================================
// BiomeClassifier Implementation
// =============================================================================

BiomeClassifier::BiomeClassifier() = default;
BiomeClassifier::~BiomeClassifier() = default;

bool BiomeClassifier::Initialize(const std::string& configPath) {
    if (!configPath.empty()) {
        m_config = BiomeConfig::LoadFromFile(configPath);
    }

    if (!m_config.climateDataPath.empty()) {
        LoadClimateData(m_config.climateDataPath);
    }

    return true;
}

void BiomeClassifier::Shutdown() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_climateCache.clear();
}

BiomeData BiomeClassifier::ClassifyBiome(
    const GeoCoordinate& coord,
    const GeoLandUse* landUse,
    float elevation) {

    BiomeData data;

    // If we have explicit land use, use it
    if (landUse) {
        data = ClassifyFromLandUse(landUse->type);
        data.elevation = elevation;
        return data;
    }

    // Otherwise use climate-based classification
    ClimateData climate = GetClimateData(coord);
    data.type = ClassifyFromClimate(climate, coord.latitude);

    data.temperature = climate.meanTemperature;
    data.precipitation = climate.annualPrecipitation;
    data.humidity = climate.humidity;
    data.elevation = elevation;

    // Get default properties for the biome type
    BiomeData defaults = GetDefaultBiomeData(data.type);
    data.foliageDensity = defaults.foliageDensity;
    data.grassDensity = defaults.grassDensity;
    data.groundColor = defaults.groundColor;
    data.primaryTexture = defaults.primaryTexture;
    data.foliageModels = defaults.foliageModels;

    return data;
}

BiomeData BiomeClassifier::ClassifyTile(const GeoTileData& tileData) {
    BiomeData data;

    // Calculate densities
    float buildingDensity = CalculateBuildingDensity(tileData.buildings, tileData.bounds);
    float roadDensity = CalculateRoadDensity(tileData.roads, tileData.bounds);

    // Check for urban areas first
    if (buildingDensity > m_config.urbanDensityThreshold) {
        data.type = ClassifyUrbanLevel(buildingDensity, roadDensity);
        data = GetDefaultBiomeData(data.type);
        return data;
    }

    // Check land use
    if (!tileData.landUse.empty()) {
        // Find dominant land use
        std::unordered_map<LandUseType, double> areaByType;
        for (const auto& lu : tileData.landUse) {
            areaByType[lu.type] += lu.GetArea();
        }

        LandUseType dominant = LandUseType::Unknown;
        double maxArea = 0;
        for (const auto& [type, area] : areaByType) {
            if (area > maxArea) {
                maxArea = area;
                dominant = type;
            }
        }

        if (dominant != LandUseType::Unknown) {
            return ClassifyFromLandUse(dominant);
        }
    }

    // Fall back to coordinate-based classification
    GeoCoordinate center = tileData.bounds.GetCenter();
    float avgElevation = 0.0f;
    if (tileData.elevation.width > 0) {
        auto [minE, maxE] = tileData.elevation.GetMinMax();
        avgElevation = (minE + maxE) / 2.0f;
    }

    return ClassifyBiome(center, nullptr, avgElevation);
}

BiomeData BiomeClassifier::ClassifyFromLandUse(LandUseType landUse) {
    BiomeType biome = BiomeType::Unknown;

    switch (landUse) {
        case LandUseType::Residential:
            biome = BiomeType::Residential;
            break;
        case LandUseType::Commercial:
        case LandUseType::Retail:
            biome = BiomeType::Commercial;
            break;
        case LandUseType::Industrial:
            biome = BiomeType::Industrial;
            break;
        case LandUseType::Forest:
        case LandUseType::Wood:
            biome = BiomeType::Forest;
            break;
        case LandUseType::Grassland:
        case LandUseType::Meadow:
            biome = BiomeType::Grassland;
            break;
        case LandUseType::Farmland:
            biome = BiomeType::Farmland;
            break;
        case LandUseType::Orchard:
            biome = BiomeType::Orchard;
            break;
        case LandUseType::Park:
        case LandUseType::Recreation:
            biome = BiomeType::Park;
            break;
        case LandUseType::Wetland:
        case LandUseType::Marsh:
            biome = BiomeType::Wetland;
            break;
        case LandUseType::Beach:
        case LandUseType::Sand:
            biome = BiomeType::Beach;
            break;
        case LandUseType::Cemetery:
            biome = BiomeType::Cemetery;
            break;
        case LandUseType::Quarry:
            biome = BiomeType::Quarry;
            break;
        case LandUseType::Landfill:
            biome = BiomeType::Landfill;
            break;
        default:
            biome = BiomeType::Grassland;
            break;
    }

    return GetDefaultBiomeData(biome);
}

BiomeType BiomeClassifier::GetBiomeType(
    const GeoCoordinate& coord,
    const std::vector<GeoLandUse>& landUse,
    const std::vector<GeoBuilding>& buildings,
    float elevation) {

    // Check if point is in any land use area
    for (const auto& lu : landUse) {
        if (lu.Contains(coord)) {
            return ClassifyFromLandUse(lu.type).type;
        }
    }

    // Check building density around point
    GeoBoundingBox localBounds = GeoBoundingBox::FromCenterRadius(coord, 100.0);
    float density = CalculateBuildingDensity(buildings, localBounds);

    if (density > m_config.urbanDensityThreshold) {
        return density > 0.6f ? BiomeType::Urban : BiomeType::Suburban;
    }

    // Use climate-based
    ClimateData climate = GetClimateData(coord);
    return ClassifyFromClimate(climate, coord.latitude);
}

ClimateData BiomeClassifier::GetClimateData(const GeoCoordinate& coord) {
    // Check cache
    std::string key = std::to_string(static_cast<int>(coord.latitude)) + "_" +
                      std::to_string(static_cast<int>(coord.longitude));

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_climateCache.find(key);
        if (it != m_climateCache.end()) {
            return it->second;
        }
    }

    // Estimate from latitude
    ClimateData climate = EstimateClimateFromLatitude(coord.latitude);

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_climateCache[key] = climate;
    }

    return climate;
}

ClimateData BiomeClassifier::EstimateClimateFromLatitude(double latitude) {
    ClimateData climate;

    double absLat = std::abs(latitude);

    // Temperature estimation (very rough approximation)
    // Equator ~27C, poles ~-10C
    climate.meanTemperature = static_cast<float>(27.0 - absLat * 0.41);
    climate.maxTemperature = climate.meanTemperature + 10.0f;
    climate.minTemperature = climate.meanTemperature - 10.0f;

    // Precipitation estimation
    // Highest near equator, low at poles and ~30 degrees (deserts)
    if (absLat < 10.0) {
        climate.annualPrecipitation = 2000.0f;  // Tropical
    } else if (absLat < 30.0) {
        climate.annualPrecipitation = 500.0f;   // Subtropical desert belt
    } else if (absLat < 60.0) {
        climate.annualPrecipitation = 800.0f;   // Temperate
    } else {
        climate.annualPrecipitation = 300.0f;   // Polar
    }

    // Humidity correlates with precipitation
    climate.humidity = std::min(1.0f, climate.annualPrecipitation / 2000.0f);

    return climate;
}

bool BiomeClassifier::LoadClimateData(const std::string& path) {
    // Placeholder for loading climate data from file
    // In production, this would load from WorldClim or similar datasets
    return true;
}

float BiomeClassifier::CalculateBuildingDensity(
    const std::vector<GeoBuilding>& buildings,
    const GeoBoundingBox& bounds) {

    double totalArea = bounds.GetWidthMeters() * bounds.GetHeightMeters();
    if (totalArea <= 0) return 0.0f;

    double buildingArea = 0.0;
    for (const auto& building : buildings) {
        if (bounds.Intersects(building.GetBounds())) {
            buildingArea += building.GetArea();
        }
    }

    return static_cast<float>(std::min(1.0, buildingArea / totalArea));
}

float BiomeClassifier::CalculateRoadDensity(
    const std::vector<GeoRoad>& roads,
    const GeoBoundingBox& bounds) {

    double totalArea = bounds.GetWidthMeters() * bounds.GetHeightMeters();
    if (totalArea <= 0) return 0.0f;

    double roadArea = 0.0;
    for (const auto& road : roads) {
        if (bounds.Intersects(road.GetBounds())) {
            roadArea += road.GetLength() * road.GetEffectiveWidth();
        }
    }

    return static_cast<float>(std::min(1.0, roadArea / totalArea));
}

float BiomeClassifier::EstimateVegetationDensity(const std::vector<GeoLandUse>& landUse) {
    if (landUse.empty()) return 0.5f;

    double totalArea = 0.0;
    double vegetationArea = 0.0;

    for (const auto& lu : landUse) {
        double area = lu.GetArea();
        totalArea += area;

        switch (lu.type) {
            case LandUseType::Forest:
            case LandUseType::Wood:
                vegetationArea += area;
                break;
            case LandUseType::Grassland:
            case LandUseType::Meadow:
            case LandUseType::Park:
                vegetationArea += area * 0.7;
                break;
            case LandUseType::Farmland:
            case LandUseType::Orchard:
                vegetationArea += area * 0.5;
                break;
            case LandUseType::Residential:
                vegetationArea += area * 0.3;
                break;
            default:
                break;
        }
    }

    return totalArea > 0 ? static_cast<float>(vegetationArea / totalArea) : 0.5f;
}

BiomeData BiomeClassifier::GetDefaultBiomeData(BiomeType biome) {
    BiomeData data;
    data.type = biome;

    switch (biome) {
        case BiomeType::Desert:
            data.temperature = 30.0f;
            data.precipitation = 100.0f;
            data.humidity = 0.1f;
            data.foliageDensity = 0.05f;
            data.grassDensity = 0.1f;
            data.groundColor = glm::vec3(0.85f, 0.75f, 0.55f);
            data.primaryTexture = "terrain/sand";
            break;

        case BiomeType::Grassland:
            data.temperature = 18.0f;
            data.precipitation = 500.0f;
            data.humidity = 0.5f;
            data.foliageDensity = 0.1f;
            data.grassDensity = 0.9f;
            data.groundColor = glm::vec3(0.4f, 0.6f, 0.2f);
            data.primaryTexture = "terrain/grass";
            break;

        case BiomeType::Forest:
        case BiomeType::TemperateForest:
            data.temperature = 15.0f;
            data.precipitation = 1000.0f;
            data.humidity = 0.7f;
            data.foliageDensity = 0.8f;
            data.grassDensity = 0.4f;
            data.groundColor = glm::vec3(0.3f, 0.45f, 0.2f);
            data.primaryTexture = "terrain/forest_floor";
            data.foliageModels = {"tree_oak", "tree_birch", "bush_small"};
            break;

        case BiomeType::TropicalForest:
        case BiomeType::Jungle:
            data.temperature = 27.0f;
            data.precipitation = 2500.0f;
            data.humidity = 0.9f;
            data.foliageDensity = 0.95f;
            data.grassDensity = 0.3f;
            data.groundColor = glm::vec3(0.25f, 0.4f, 0.15f);
            data.primaryTexture = "terrain/jungle_floor";
            data.foliageModels = {"tree_palm", "tree_tropical", "fern_large"};
            break;

        case BiomeType::Tundra:
            data.temperature = -5.0f;
            data.precipitation = 200.0f;
            data.humidity = 0.6f;
            data.foliageDensity = 0.05f;
            data.grassDensity = 0.3f;
            data.groundColor = glm::vec3(0.5f, 0.55f, 0.45f);
            data.primaryTexture = "terrain/tundra";
            break;

        case BiomeType::Arctic:
            data.temperature = -20.0f;
            data.precipitation = 150.0f;
            data.humidity = 0.4f;
            data.foliageDensity = 0.0f;
            data.grassDensity = 0.0f;
            data.groundColor = glm::vec3(0.95f, 0.95f, 0.98f);
            data.primaryTexture = "terrain/snow";
            break;

        case BiomeType::Urban:
            data.temperature = 20.0f;
            data.foliageDensity = 0.05f;
            data.grassDensity = 0.05f;
            data.groundColor = glm::vec3(0.4f, 0.4f, 0.4f);
            data.primaryTexture = "terrain/concrete";
            break;

        case BiomeType::Suburban:
            data.temperature = 18.0f;
            data.foliageDensity = 0.3f;
            data.grassDensity = 0.5f;
            data.groundColor = glm::vec3(0.35f, 0.5f, 0.25f);
            data.primaryTexture = "terrain/grass";
            data.foliageModels = {"tree_suburban", "bush_hedge"};
            break;

        case BiomeType::Wetland:
        case BiomeType::Swamp:
            data.temperature = 20.0f;
            data.precipitation = 1500.0f;
            data.humidity = 0.95f;
            data.foliageDensity = 0.6f;
            data.grassDensity = 0.4f;
            data.groundColor = glm::vec3(0.3f, 0.35f, 0.2f);
            data.primaryTexture = "terrain/swamp";
            data.foliageModels = {"tree_willow", "reed", "lily_pad"};
            break;

        case BiomeType::Beach:
            data.foliageDensity = 0.02f;
            data.grassDensity = 0.02f;
            data.groundColor = glm::vec3(0.9f, 0.85f, 0.7f);
            data.primaryTexture = "terrain/beach_sand";
            break;

        case BiomeType::Farmland:
            data.foliageDensity = 0.1f;
            data.grassDensity = 0.7f;
            data.groundColor = glm::vec3(0.5f, 0.45f, 0.3f);
            data.primaryTexture = "terrain/farmland";
            break;

        case BiomeType::Park:
            data.foliageDensity = 0.4f;
            data.grassDensity = 0.8f;
            data.groundColor = glm::vec3(0.35f, 0.55f, 0.25f);
            data.primaryTexture = "terrain/park_grass";
            data.foliageModels = {"tree_park", "bush_ornamental"};
            break;

        default:
            data.foliageDensity = 0.3f;
            data.grassDensity = 0.5f;
            data.groundColor = glm::vec3(0.4f, 0.5f, 0.3f);
            data.primaryTexture = "terrain/default";
            break;
    }

    // Seasonal multipliers (default)
    data.springMultiplier = 1.0f;
    data.summerMultiplier = 1.0f;
    data.autumnMultiplier = 0.8f;
    data.winterMultiplier = 0.3f;

    return data;
}

float BiomeClassifier::GetFoliageDensity(BiomeType biome) {
    return GetDefaultBiomeData(biome).foliageDensity;
}

float BiomeClassifier::GetGrassDensity(BiomeType biome) {
    return GetDefaultBiomeData(biome).grassDensity;
}

std::string BiomeClassifier::GetGroundTexture(BiomeType biome) {
    return GetDefaultBiomeData(biome).primaryTexture;
}

std::vector<std::string> BiomeClassifier::GetFoliageModels(BiomeType biome) {
    return GetDefaultBiomeData(biome).foliageModels;
}

glm::vec3 BiomeClassifier::GetGroundColor(BiomeType biome) {
    return GetDefaultBiomeData(biome).groundColor;
}

float BiomeClassifier::GetSeasonalVegetationMultiplier(BiomeType biome, int month) {
    BiomeData data = GetDefaultBiomeData(biome);

    // Northern hemisphere seasons (adjust for southern)
    if (month >= 3 && month <= 5) return data.springMultiplier;
    if (month >= 6 && month <= 8) return data.summerMultiplier;
    if (month >= 9 && month <= 11) return data.autumnMultiplier;
    return data.winterMultiplier;
}

glm::vec3 BiomeClassifier::GetSeasonalFoliageColor(BiomeType biome, int month) {
    // Base green color
    glm::vec3 spring(0.3f, 0.7f, 0.2f);
    glm::vec3 summer(0.2f, 0.6f, 0.15f);
    glm::vec3 autumn(0.7f, 0.4f, 0.1f);
    glm::vec3 winter(0.4f, 0.3f, 0.2f);

    // Evergreen biomes don't change much
    if (biome == BiomeType::BorealForest || biome == BiomeType::TropicalForest ||
        biome == BiomeType::Jungle) {
        return summer;
    }

    if (month >= 3 && month <= 5) return spring;
    if (month >= 6 && month <= 8) return summer;
    if (month >= 9 && month <= 11) return autumn;
    return winter;
}

bool BiomeClassifier::IsWinter(double latitude, int month) {
    bool northernHemisphere = latitude >= 0;

    if (northernHemisphere) {
        return month == 12 || month <= 2;
    } else {
        return month >= 6 && month <= 8;
    }
}

BiomeType BiomeClassifier::ClassifyFromClimate(const ClimateData& climate, double latitude) {
    float temp = climate.meanTemperature;
    float precip = climate.annualPrecipitation;
    double absLat = std::abs(latitude);

    // Arctic/Antarctic
    if (absLat > m_config.arcticLatitude || temp < -10.0f) {
        return BiomeType::Arctic;
    }

    // Tundra
    if (temp < 0.0f || absLat > 60.0f) {
        return BiomeType::Tundra;
    }

    // Desert
    if (precip < 250.0f) {
        return BiomeType::Desert;
    }

    // Tropical regions
    if (absLat < m_config.tropicLatitude) {
        if (precip > 2000.0f) {
            return BiomeType::TropicalForest;
        }
        if (precip > 1000.0f) {
            return BiomeType::Savanna;
        }
        return BiomeType::Grassland;
    }

    // Temperate regions
    if (precip > 1500.0f) {
        return BiomeType::TemperateForest;
    }
    if (precip > 750.0f) {
        if (temp > 10.0f) {
            return BiomeType::Forest;
        }
        return BiomeType::BorealForest;
    }
    if (precip > 400.0f) {
        return BiomeType::Grassland;
    }

    return BiomeType::Shrubland;
}

BiomeType BiomeClassifier::ClassifyUrbanLevel(float buildingDensity, float roadDensity) {
    float urbanIndex = buildingDensity * 0.7f + roadDensity * 0.3f;

    if (urbanIndex > 0.6f) return BiomeType::Urban;
    if (urbanIndex > 0.4f) return BiomeType::Commercial;
    if (urbanIndex > 0.2f) return BiomeType::Suburban;
    return BiomeType::Residential;
}

BiomeData BiomeClassifier::BlendBiomes(const std::vector<std::pair<BiomeType, float>>& biomes) {
    BiomeData result;

    if (biomes.empty()) return result;

    // Find dominant biome
    float maxWeight = 0.0f;
    for (const auto& [type, weight] : biomes) {
        if (weight > maxWeight) {
            maxWeight = weight;
            result.type = type;
        }
    }

    // Blend properties
    float totalWeight = 0.0f;
    for (const auto& [type, weight] : biomes) {
        BiomeData data = GetDefaultBiomeData(type);
        result.temperature += data.temperature * weight;
        result.precipitation += data.precipitation * weight;
        result.humidity += data.humidity * weight;
        result.foliageDensity += data.foliageDensity * weight;
        result.grassDensity += data.grassDensity * weight;
        result.groundColor += data.groundColor * weight;
        totalWeight += weight;
    }

    if (totalWeight > 0.0f) {
        result.temperature /= totalWeight;
        result.precipitation /= totalWeight;
        result.humidity /= totalWeight;
        result.foliageDensity /= totalWeight;
        result.grassDensity /= totalWeight;
        result.groundColor /= totalWeight;
    }

    // Use primary texture from dominant biome
    result.primaryTexture = GetDefaultBiomeData(result.type).primaryTexture;

    return result;
}

// =============================================================================
// BiomeTransition Implementation
// =============================================================================

std::unordered_map<BiomeType, float> BiomeTransition::CalculateBlendWeights(
    const GeoCoordinate& coord,
    const std::vector<std::vector<BiomeType>>& biomeGrid,
    const GeoBoundingBox& bounds) {

    std::unordered_map<BiomeType, float> weights;

    if (biomeGrid.empty() || biomeGrid[0].empty()) {
        return weights;
    }

    int gridHeight = static_cast<int>(biomeGrid.size());
    int gridWidth = static_cast<int>(biomeGrid[0].size());

    // Calculate grid position
    double fx = (coord.longitude - bounds.min.longitude) / bounds.GetWidthDegrees() * (gridWidth - 1);
    double fy = (bounds.max.latitude - coord.latitude) / bounds.GetHeightDegrees() * (gridHeight - 1);

    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, gridWidth - 1);
    int y1 = std::min(y0 + 1, gridHeight - 1);

    float fracX = static_cast<float>(fx - x0);
    float fracY = static_cast<float>(fy - y0);

    // Bilinear interpolation weights
    float w00 = (1 - fracX) * (1 - fracY);
    float w10 = fracX * (1 - fracY);
    float w01 = (1 - fracX) * fracY;
    float w11 = fracX * fracY;

    weights[biomeGrid[y0][x0]] += w00;
    weights[biomeGrid[y0][x1]] += w10;
    weights[biomeGrid[y1][x0]] += w01;
    weights[biomeGrid[y1][x1]] += w11;

    return weights;
}

BiomeData BiomeTransition::InterpolateBiomeData(
    const std::unordered_map<BiomeType, float>& weights,
    const std::function<BiomeData(BiomeType)>& getBiomeData) {

    std::vector<std::pair<BiomeType, float>> biomeWeights;
    for (const auto& [type, weight] : weights) {
        biomeWeights.emplace_back(type, weight);
    }

    BiomeClassifier classifier;
    return classifier.GetDefaultBiomeData(BiomeType::Grassland);  // Simplified
}

std::vector<std::vector<BiomeType>> BiomeTransition::GenerateBiomeGrid(
    const GeoBoundingBox& bounds,
    int resolution,
    BiomeClassifier& classifier) {

    std::vector<std::vector<BiomeType>> grid(resolution, std::vector<BiomeType>(resolution));

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            double fx = static_cast<double>(x) / (resolution - 1);
            double fy = static_cast<double>(y) / (resolution - 1);

            GeoCoordinate coord(
                bounds.max.latitude - fy * bounds.GetHeightDegrees(),
                bounds.min.longitude + fx * bounds.GetWidthDegrees()
            );

            BiomeData data = classifier.ClassifyBiome(coord);
            grid[y][x] = data.type;
        }
    }

    return grid;
}

} // namespace Geo
} // namespace Vehement
