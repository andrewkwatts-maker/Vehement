#pragma once

#include "PCGPipeline.hpp"

namespace Vehement {

/**
 * @brief Terrain generation parameters
 */
struct TerrainParams {
    // Height map
    float heightScale = 10.0f;          // Maximum height variation
    float noiseFrequency = 0.02f;       // Base noise frequency
    int noiseOctaves = 4;               // Fractal noise octaves
    float noisePersistence = 0.5f;      // Octave amplitude reduction

    // Erosion
    bool applyErosion = true;
    int erosionIterations = 5;
    float erosionStrength = 0.3f;

    // Smoothing
    bool applySmoothing = true;
    int smoothingPasses = 2;

    // Water
    float waterLevel = 0.3f;            // Height threshold for water
    bool generateWaterBodies = true;

    // Biome mapping
    bool useBiomeData = true;           // Use real-world biome data
    bool generateBiomes = false;        // Procedurally generate biomes

    // Tile selection
    TileType grassTile = TileType::GroundGrass1;
    TileType dirtTile = TileType::GroundDirt;
    TileType rockTile = TileType::GroundRocks;
    TileType forestTile = TileType::GroundForest1;
    TileType waterTile = TileType::Water1;
};

/**
 * @brief Base terrain generation
 *
 * Generates:
 * - Height map from real elevation data or procedural
 * - Tile type assignment based on biome/land use
 * - Erosion and smoothing passes
 * - Water body placement
 *
 * Python script hook: terrain_*.py
 */
class TerrainGenerator : public PCGStageGenerator {
public:
    TerrainGenerator();
    ~TerrainGenerator() override = default;

    // PCGStageGenerator interface
    PCGStageResult Generate(PCGContext& context, PCGMode mode) override;
    PCGStage GetStage() const override { return PCGStage::Terrain; }
    const char* GetName() const override { return "TerrainGenerator"; }

    // Configuration
    void SetParams(const TerrainParams& params) { m_params = params; }
    TerrainParams& GetParams() { return m_params; }
    const TerrainParams& GetParams() const { return m_params; }

    // Individual generation steps (for custom pipelines)

    /**
     * @brief Generate height map
     */
    void GenerateHeightMap(PCGContext& context);

    /**
     * @brief Apply erosion simulation
     */
    void ApplyErosion(PCGContext& context);

    /**
     * @brief Apply smoothing filter
     */
    void ApplySmoothing(PCGContext& context);

    /**
     * @brief Place water bodies
     */
    void GenerateWaterBodies(PCGContext& context);

    /**
     * @brief Assign tile types based on elevation and biome
     */
    void AssignTileTypes(PCGContext& context);

    /**
     * @brief Generate biomes procedurally
     */
    void GenerateBiomes(PCGContext& context);

private:
    TerrainParams m_params;

    // Height map helpers
    float SampleTerrainNoise(PCGContext& context, float x, float y);
    TileType SelectTileForBiome(BiomeType biome, float elevation, PCGContext& context);
};

} // namespace Vehement
