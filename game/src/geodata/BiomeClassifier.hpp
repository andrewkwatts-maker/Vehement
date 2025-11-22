#pragma once

#include "GeoTypes.hpp"
#include <memory>
#include <unordered_map>

namespace Vehement {
namespace Geo {

/**
 * @brief Configuration for biome classification
 */
struct BiomeConfig {
    // Climate data sources
    std::string climateDataPath;         // Path to climate data files
    bool useOnlineClimate = false;       // Fetch climate data online

    // Classification parameters
    float urbanDensityThreshold = 0.3f;  // Building density for urban classification
    float forestCoverThreshold = 0.4f;   // Tree cover for forest classification

    // Latitude-based defaults
    float tropicLatitude = 23.5f;        // Tropic of Cancer/Capricorn
    float arcticLatitude = 66.5f;        // Arctic/Antarctic circle

    // Seasonal settings
    int currentMonth = 6;                // Current month (1-12) for seasonal variation

    /**
     * @brief Load from file
     */
    static BiomeConfig LoadFromFile(const std::string& path);

    /**
     * @brief Save to file
     */
    void SaveToFile(const std::string& path) const;
};

/**
 * @brief Climate data for a location
 */
struct ClimateData {
    float meanTemperature = 15.0f;       // Annual mean temperature (Celsius)
    float minTemperature = 5.0f;         // Coldest month mean
    float maxTemperature = 25.0f;        // Warmest month mean
    float annualPrecipitation = 800.0f;  // Annual precipitation (mm)
    float humidity = 0.5f;               // Average relative humidity (0-1)

    // Monthly data (optional, for seasonal variation)
    float monthlyTemp[12] = {0};
    float monthlyPrecip[12] = {0};

    /**
     * @brief Calculate aridity index
     */
    float GetAridityIndex() const {
        if (meanTemperature < 0) return 0.0f;
        return annualPrecipitation / (meanTemperature + 10.0f);
    }

    /**
     * @brief Check if climate data is valid
     */
    bool IsValid() const {
        return annualPrecipitation >= 0 && meanTemperature > -100.0f;
    }
};

/**
 * @brief Biome classifier for geographic data
 *
 * Classifies biomes based on:
 * - Climate data (temperature, precipitation)
 * - Land use from OSM
 * - Latitude/elevation
 * - Building/road density
 */
class BiomeClassifier {
public:
    BiomeClassifier();
    ~BiomeClassifier();

    /**
     * @brief Initialize the classifier
     */
    bool Initialize(const std::string& configPath = "");

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Set configuration
     */
    void SetConfig(const BiomeConfig& config) { m_config = config; }

    /**
     * @brief Get configuration
     */
    const BiomeConfig& GetConfig() const { return m_config; }

    // ==========================================================================
    // Classification
    // ==========================================================================

    /**
     * @brief Classify biome at a coordinate
     * @param coord Geographic coordinate
     * @param landUse Optional land use data at location
     * @param elevation Optional elevation in meters
     * @return Biome data
     */
    BiomeData ClassifyBiome(
        const GeoCoordinate& coord,
        const GeoLandUse* landUse = nullptr,
        float elevation = 0.0f);

    /**
     * @brief Classify biome for a tile
     * @param tileData Tile data with land use, buildings, etc.
     * @return Biome data for the tile
     */
    BiomeData ClassifyTile(const GeoTileData& tileData);

    /**
     * @brief Classify biome from land use type
     */
    BiomeData ClassifyFromLandUse(LandUseType landUse);

    /**
     * @brief Get biome for coordinates using all available data
     */
    BiomeType GetBiomeType(
        const GeoCoordinate& coord,
        const std::vector<GeoLandUse>& landUse,
        const std::vector<GeoBuilding>& buildings,
        float elevation = 0.0f);

    // ==========================================================================
    // Climate Data
    // ==========================================================================

    /**
     * @brief Get climate data for a coordinate
     */
    ClimateData GetClimateData(const GeoCoordinate& coord);

    /**
     * @brief Estimate climate from latitude
     */
    ClimateData EstimateClimateFromLatitude(double latitude);

    /**
     * @brief Load climate data file
     */
    bool LoadClimateData(const std::string& path);

    // ==========================================================================
    // Density Calculations
    // ==========================================================================

    /**
     * @brief Calculate building density for an area
     * @param buildings Building list
     * @param bounds Area bounds
     * @return Density (0-1)
     */
    float CalculateBuildingDensity(
        const std::vector<GeoBuilding>& buildings,
        const GeoBoundingBox& bounds);

    /**
     * @brief Calculate road density for an area
     */
    float CalculateRoadDensity(
        const std::vector<GeoRoad>& roads,
        const GeoBoundingBox& bounds);

    /**
     * @brief Estimate vegetation density from land use
     */
    float EstimateVegetationDensity(const std::vector<GeoLandUse>& landUse);

    // ==========================================================================
    // Biome Properties
    // ==========================================================================

    /**
     * @brief Get default properties for a biome type
     */
    BiomeData GetDefaultBiomeData(BiomeType biome);

    /**
     * @brief Get foliage density for biome type
     */
    float GetFoliageDensity(BiomeType biome);

    /**
     * @brief Get grass density for biome type
     */
    float GetGrassDensity(BiomeType biome);

    /**
     * @brief Get ground texture for biome type
     */
    std::string GetGroundTexture(BiomeType biome);

    /**
     * @brief Get foliage models for biome type
     */
    std::vector<std::string> GetFoliageModels(BiomeType biome);

    /**
     * @brief Get ground color for biome type
     */
    glm::vec3 GetGroundColor(BiomeType biome);

    // ==========================================================================
    // Seasonal Variation
    // ==========================================================================

    /**
     * @brief Set current month for seasonal calculations
     */
    void SetCurrentMonth(int month) { m_config.currentMonth = std::clamp(month, 1, 12); }

    /**
     * @brief Get seasonal multiplier for vegetation
     * @param biome Biome type
     * @param month Month (1-12)
     * @return Multiplier (0-1)
     */
    float GetSeasonalVegetationMultiplier(BiomeType biome, int month);

    /**
     * @brief Get seasonal foliage color
     */
    glm::vec3 GetSeasonalFoliageColor(BiomeType biome, int month);

    /**
     * @brief Check if it's winter in the given hemisphere
     */
    bool IsWinter(double latitude, int month);

private:
    /**
     * @brief Classify from climate using Whittaker diagram
     */
    BiomeType ClassifyFromClimate(const ClimateData& climate, double latitude);

    /**
     * @brief Classify urban density level
     */
    BiomeType ClassifyUrbanLevel(float buildingDensity, float roadDensity);

    /**
     * @brief Blend multiple biome types
     */
    BiomeData BlendBiomes(const std::vector<std::pair<BiomeType, float>>& biomes);

    BiomeConfig m_config;

    // Climate data lookup (cached by tile)
    std::unordered_map<std::string, ClimateData> m_climateCache;
    mutable std::mutex m_cacheMutex;
};

/**
 * @brief Biome transition calculator for smooth blending
 */
class BiomeTransition {
public:
    /**
     * @brief Calculate blend weights between biomes at a point
     * @param coord Point coordinate
     * @param biomeGrid Grid of biome types
     * @param bounds Biome grid bounds
     * @return Map of biome type to weight
     */
    static std::unordered_map<BiomeType, float> CalculateBlendWeights(
        const GeoCoordinate& coord,
        const std::vector<std::vector<BiomeType>>& biomeGrid,
        const GeoBoundingBox& bounds);

    /**
     * @brief Interpolate biome data
     */
    static BiomeData InterpolateBiomeData(
        const std::unordered_map<BiomeType, float>& weights,
        const std::function<BiomeData(BiomeType)>& getBiomeData);

    /**
     * @brief Generate biome grid for area
     */
    static std::vector<std::vector<BiomeType>> GenerateBiomeGrid(
        const GeoBoundingBox& bounds,
        int resolution,
        BiomeClassifier& classifier);
};

} // namespace Geo
} // namespace Vehement
