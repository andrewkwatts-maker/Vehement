#pragma once

#include "PCGPipeline.hpp"
#include <vector>

namespace Vehement {

/**
 * @brief Building generation parameters
 */
struct BuildingParams {
    // Placement
    bool useRealData = true;            // Use GeoBuilding footprints
    bool generateProcedural = true;     // Fill gaps procedurally
    float minRoadDistance = 2.0f;       // Minimum distance from road edge
    float maxRoadDistance = 20.0f;      // Maximum distance from road
    int minBuildingSize = 3;            // Minimum building dimension
    int maxBuildingSize = 15;           // Maximum building dimension

    // Density
    float urbanDensity = 0.7f;          // Building density in urban areas
    float suburbanDensity = 0.4f;
    float ruralDensity = 0.1f;

    // Building styles
    TileType residentialFloor = TileType::WoodFlooring1;
    TileType residentialWall = TileType::BricksRock;
    TileType commercialFloor = TileType::ConcreteTiles1;
    TileType commercialWall = TileType::BricksGrey;
    TileType industrialFloor = TileType::Metal1;
    TileType industrialWall = TileType::MetalTile1;

    // Wall settings
    float defaultWallHeight = 2.5f;
    float minWallHeight = 2.0f;
    float maxWallHeight = 5.0f;

    // Features
    bool addEntrances = true;
    bool addWindows = false;            // Visual only
    bool addRoofs = false;              // For 3D rendering
};

/**
 * @brief Building footprint for generation
 */
struct BuildingFootprint {
    glm::ivec2 position;                // Top-left corner
    int width = 0;
    int height = 0;
    BuildingType type = BuildingType::None;
    float wallHeight = 2.5f;
    int entranceSide = 0;               // 0=south, 1=east, 2=north, 3=west
    bool fromRealData = false;
};

/**
 * @brief Building placement
 *
 * Generates:
 * - Place buildings from GeoBuilding footprints
 * - Procedural building generation where no data
 * - Building density based on land use
 * - Respects road setbacks
 *
 * Python script hook: building_*.py
 */
class BuildingGenerator : public PCGStageGenerator {
public:
    BuildingGenerator();
    ~BuildingGenerator() override = default;

    // PCGStageGenerator interface
    PCGStageResult Generate(PCGContext& context, PCGMode mode) override;
    PCGStage GetStage() const override { return PCGStage::Buildings; }
    const char* GetName() const override { return "BuildingGenerator"; }

    // Configuration
    void SetParams(const BuildingParams& params) { m_params = params; }
    BuildingParams& GetParams() { return m_params; }
    const BuildingParams& GetParams() const { return m_params; }

    // Individual generation steps

    /**
     * @brief Convert GeoBuilding data to footprints
     */
    void ConvertGeoBuildings(PCGContext& context, std::vector<BuildingFootprint>& buildings);

    /**
     * @brief Generate procedural buildings in empty areas
     */
    void GenerateProceduralBuildings(PCGContext& context,
                                      std::vector<BuildingFootprint>& buildings);

    /**
     * @brief Rasterize buildings to tiles
     */
    void RasterizeBuildings(PCGContext& context,
                            const std::vector<BuildingFootprint>& buildings);

    /**
     * @brief Add building entrances
     */
    void AddEntrances(PCGContext& context, std::vector<BuildingFootprint>& buildings);

    /**
     * @brief Find valid building location
     * @return true if location found
     */
    bool FindBuildingLocation(PCGContext& context, int width, int height,
                               glm::ivec2& outPosition);

    /**
     * @brief Check if area is valid for building
     */
    bool IsValidBuildingArea(PCGContext& context, int x, int y, int width, int height);

    /**
     * @brief Get building density for biome
     */
    float GetDensityForBiome(BiomeType biome) const;

private:
    BuildingParams m_params;

    // Helpers
    TileType GetFloorTile(BuildingType type) const;
    TileType GetWallTile(BuildingType type) const;
    BuildingType InferBuildingType(BiomeType biome, PCGContext& context);
    void RasterizeBuilding(PCGContext& context, const BuildingFootprint& building);
    int FindBestEntranceSide(PCGContext& context, const BuildingFootprint& building);
};

} // namespace Vehement
