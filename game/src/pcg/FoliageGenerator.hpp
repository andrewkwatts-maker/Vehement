#pragma once

#include "PCGPipeline.hpp"
#include <vector>

namespace Vehement {

/**
 * @brief Foliage type definition
 */
struct FoliageType {
    std::string id;
    std::string name;
    std::vector<BiomeType> validBiomes;
    float minScale = 0.8f;
    float maxScale = 1.2f;
    float spacing = 2.0f;               // Minimum spacing between same type
    bool clustered = false;             // Tends to grow in clusters
    float clusterRadius = 5.0f;
    int clusterCount = 3;
};

/**
 * @brief Foliage generation parameters
 */
struct FoliageParams {
    // Density
    float baseDensity = 0.3f;           // Base tree/plant density
    float forestDensity = 0.6f;         // Density in forest biomes
    float urbanDensity = 0.05f;         // Density in urban areas
    float parkDensity = 0.4f;           // Density in parks

    // Placement rules
    float minRoadDistance = 1.0f;       // Minimum distance from roads
    float minBuildingDistance = 1.0f;   // Minimum distance from buildings
    float minWaterDistance = 0.5f;      // Minimum distance from water
    float minSpacing = 1.5f;            // Minimum spacing between foliage

    // Distribution
    bool useClustering = true;          // Natural clustering
    float clusterChance = 0.3f;         // Chance to start a cluster
    int maxClusterSize = 5;             // Max items in cluster

    // Variety
    bool useNoiseVariation = true;      // Vary density with noise
    float noiseFrequency = 0.1f;
    float noiseInfluence = 0.5f;        // How much noise affects density

    // Types
    std::vector<FoliageType> foliageTypes;
};

/**
 * @brief Vegetation placement
 *
 * Generates:
 * - Tree/plant density from biome data
 * - Species selection based on biome
 * - Clustering and natural distribution
 * - Avoids roads and buildings
 *
 * Python script hook: foliage_*.py
 */
class FoliageGenerator : public PCGStageGenerator {
public:
    FoliageGenerator();
    ~FoliageGenerator() override = default;

    // PCGStageGenerator interface
    PCGStageResult Generate(PCGContext& context, PCGMode mode) override;
    PCGStage GetStage() const override { return PCGStage::Foliage; }
    const char* GetName() const override { return "FoliageGenerator"; }

    // Configuration
    void SetParams(const FoliageParams& params) { m_params = params; }
    FoliageParams& GetParams() { return m_params; }
    const FoliageParams& GetParams() const { return m_params; }

    // Foliage type registration
    void RegisterFoliageType(const FoliageType& type);
    void ClearFoliageTypes();
    const std::vector<FoliageType>& GetFoliageTypes() const { return m_params.foliageTypes; }

    // Individual generation steps

    /**
     * @brief Calculate density map
     */
    void CalculateDensityMap(PCGContext& context, std::vector<float>& densityMap);

    /**
     * @brief Generate foliage placements
     */
    void GeneratePlacements(PCGContext& context, const std::vector<float>& densityMap);

    /**
     * @brief Apply clustering to placements
     */
    void ApplyClustering(PCGContext& context);

    /**
     * @brief Select foliage type for position
     */
    const FoliageType* SelectFoliageType(PCGContext& context, int x, int y);

    /**
     * @brief Check if position is valid for foliage
     */
    bool IsValidPosition(PCGContext& context, int x, int y) const;

    /**
     * @brief Get density for position
     */
    float GetDensityAt(PCGContext& context, int x, int y) const;

private:
    FoliageParams m_params;

    // Default foliage types
    void InitializeDefaultTypes();

    // Helpers
    float CalculateLocalDensity(PCGContext& context, int x, int y, BiomeType biome);
    bool CheckSpacing(PCGContext& context, int x, int y, float minDist);
};

} // namespace Vehement
