#pragma once

#include "../scripting/visual/VisualScriptingCore.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <vector>
#include <cmath>
#include <random>

namespace Nova {
namespace ProcGen {

// Forward declarations
class HeightmapData;
struct BiomeInfo;

// =============================================================================
// Execution Context Extension for ProcGen
// =============================================================================

/**
 * @brief Extended execution context for procedural generation
 */
struct ProcGenContext {
    int seed = 0;
    glm::ivec2 chunkPos = glm::ivec2(0);
    int resolution = 64;
    float worldScale = 1.0f;
    std::mt19937 rng;
    std::unordered_map<std::string, std::any> sharedData;
};

// =============================================================================
// Noise Nodes
// =============================================================================

/**
 * @brief Perlin noise generator node
 */
class PerlinNoiseNode : public VisualScript::Node {
public:
    PerlinNoiseNode() : Node("PerlinNoise", "Perlin Noise") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Generates smooth Perlin noise");

        AddInputPort(std::make_shared<VisualScript::Port>("position", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec2"));
        AddInputPort(std::make_shared<VisualScript::Port>("frequency", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("octaves", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));
        AddInputPort(std::make_shared<VisualScript::Port>("persistence", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("lacunarity", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("value", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Simplex noise generator node (faster than Perlin)
 */
class SimplexNoiseNode : public VisualScript::Node {
public:
    SimplexNoiseNode() : Node("SimplexNoise", "Simplex Noise") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Generates smooth Simplex noise (faster than Perlin)");

        AddInputPort(std::make_shared<VisualScript::Port>("position", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec2"));
        AddInputPort(std::make_shared<VisualScript::Port>("frequency", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("octaves", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));
        AddInputPort(std::make_shared<VisualScript::Port>("persistence", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("lacunarity", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("value", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Worley (Cellular) noise generator node
 */
class WorleyNoiseNode : public VisualScript::Node {
public:
    WorleyNoiseNode() : Node("WorleyNoise", "Worley Noise") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Generates cellular/Worley noise patterns");

        AddInputPort(std::make_shared<VisualScript::Port>("position", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec2"));
        AddInputPort(std::make_shared<VisualScript::Port>("frequency", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("distanceType", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));

        AddOutputPort(std::make_shared<VisualScript::Port>("value", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
        AddOutputPort(std::make_shared<VisualScript::Port>("cellId", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "int"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Voronoi diagram generator node
 */
class VoronoiNode : public VisualScript::Node {
public:
    VoronoiNode() : Node("Voronoi", "Voronoi") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Generates Voronoi diagram patterns");

        AddInputPort(std::make_shared<VisualScript::Port>("position", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec2"));
        AddInputPort(std::make_shared<VisualScript::Port>("scale", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("randomness", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("value", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
        AddOutputPort(std::make_shared<VisualScript::Port>("cellId", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "int"));
        AddOutputPort(std::make_shared<VisualScript::Port>("cellCenter", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "vec2"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

// =============================================================================
// Erosion Nodes
// =============================================================================

/**
 * @brief Hydraulic erosion simulation node
 */
class HydraulicErosionNode : public VisualScript::Node {
public:
    HydraulicErosionNode() : Node("HydraulicErosion", "Hydraulic Erosion") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Simulates water-based erosion");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("iterations", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));
        AddInputPort(std::make_shared<VisualScript::Port>("rainAmount", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("evaporation", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("sedimentCapacity", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("erosionStrength", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("depositionStrength", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("erodedHeightmap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
        AddOutputPort(std::make_shared<VisualScript::Port>("sedimentMap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Thermal erosion simulation node
 */
class ThermalErosionNode : public VisualScript::Node {
public:
    ThermalErosionNode() : Node("ThermalErosion", "Thermal Erosion") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Simulates slope-based erosion (talus)");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("iterations", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));
        AddInputPort(std::make_shared<VisualScript::Port>("talusAngle", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("strength", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("erodedHeightmap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

// =============================================================================
// Terrain Shaping Nodes
// =============================================================================

/**
 * @brief Terrace/step function node
 */
class TerraceNode : public VisualScript::Node {
public:
    TerraceNode() : Node("Terrace", "Terrace") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Creates terraced/stepped terrain");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("steps", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));
        AddInputPort(std::make_shared<VisualScript::Port>("smoothness", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("terracedHeightmap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Ridge detection and enhancement node
 */
class RidgeNode : public VisualScript::Node {
public:
    RidgeNode() : Node("Ridge", "Ridge") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Creates sharp ridges in terrain");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("sharpness", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("offset", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("ridgedHeightmap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Slope mapping node
 */
class SlopeNode : public VisualScript::Node {
public:
    SlopeNode() : Node("Slope", "Slope") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Calculates slope angle from heightmap");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("scale", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("slopeMap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
        AddOutputPort(std::make_shared<VisualScript::Port>("normalMap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "vec3array"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

// =============================================================================
// Resource Placement Nodes
// =============================================================================

/**
 * @brief Ore/resource placement node
 */
class ResourcePlacementNode : public VisualScript::Node {
public:
    ResourcePlacementNode() : Node("ResourcePlacement", "Resource Placement") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Places resources (ores, minerals) based on rules");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("resourceType", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "string"));
        AddInputPort(std::make_shared<VisualScript::Port>("density", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("minHeight", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("maxHeight", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("minSlope", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("maxSlope", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("clusterSize", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("resourceMap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "resourcearray"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Vegetation placement node
 */
class VegetationPlacementNode : public VisualScript::Node {
public:
    VegetationPlacementNode() : Node("VegetationPlacement", "Vegetation Placement") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Places trees, plants, grass based on biome");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("biomeMap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "biomemap"));
        AddInputPort(std::make_shared<VisualScript::Port>("vegetationType", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "string"));
        AddInputPort(std::make_shared<VisualScript::Port>("density", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("minSlope", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("maxSlope", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("vegetationMap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "vegetationarray"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Water placement node
 */
class WaterPlacementNode : public VisualScript::Node {
public:
    WaterPlacementNode() : Node("WaterPlacement", "Water Placement") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Places water bodies (rivers, lakes, oceans)");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("waterLevel", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("flowMap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));

        AddOutputPort(std::make_shared<VisualScript::Port>("waterMask", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
        AddOutputPort(std::make_shared<VisualScript::Port>("depthMap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

// =============================================================================
// Structure Placement Nodes
// =============================================================================

/**
 * @brief Ruins placement node
 */
class RuinsPlacementNode : public VisualScript::Node {
public:
    RuinsPlacementNode() : Node("RuinsPlacement", "Ruins Placement") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Places ancient ruins and structures");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("biomeMap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "biomemap"));
        AddInputPort(std::make_shared<VisualScript::Port>("density", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("minDistance", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("ruinTypes", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "stringarray"));

        AddOutputPort(std::make_shared<VisualScript::Port>("ruinsList", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "structurearray"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Ancient structures placement node
 */
class AncientStructuresNode : public VisualScript::Node {
public:
    AncientStructuresNode() : Node("AncientStructures", "Ancient Structures") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Places ancient monuments, temples, dungeons");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("density", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("structureTypes", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "stringarray"));
        AddInputPort(std::make_shared<VisualScript::Port>("minSize", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("maxSize", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("structuresList", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "structurearray"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Building placement node
 */
class BuildingPlacementNode : public VisualScript::Node {
public:
    BuildingPlacementNode() : Node("BuildingPlacement", "Building Placement") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Places buildings, villages, cities");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("biomeMap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "biomemap"));
        AddInputPort(std::make_shared<VisualScript::Port>("buildingType", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "string"));
        AddInputPort(std::make_shared<VisualScript::Port>("density", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("maxSlope", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("buildingsList", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "buildingarray"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

// =============================================================================
// Biome and Climate Nodes
// =============================================================================

/**
 * @brief Biome calculation node
 */
class BiomeNode : public VisualScript::Node {
public:
    BiomeNode() : Node("Biome", "Biome") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Calculates biome based on temperature and precipitation");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("temperature", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("precipitation", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("latitude", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("biomeMap", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "biomemap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Climate simulation node
 */
class ClimateNode : public VisualScript::Node {
public:
    ClimateNode() : Node("Climate", "Climate") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Simulates temperature and precipitation patterns");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("latitude", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("oceanDistance", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("windPattern", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "string"));

        AddOutputPort(std::make_shared<VisualScript::Port>("temperature", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
        AddOutputPort(std::make_shared<VisualScript::Port>("precipitation", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
        AddOutputPort(std::make_shared<VisualScript::Port>("humidity", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

// =============================================================================
// Utility Nodes
// =============================================================================

/**
 * @brief Blend/mix node for heightmaps
 */
class BlendNode : public VisualScript::Node {
public:
    BlendNode() : Node("Blend", "Blend") {
        SetCategory(VisualScript::NodeCategory::Math);
        SetDescription("Blends two heightmaps together");

        AddInputPort(std::make_shared<VisualScript::Port>("inputA", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("inputB", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("blend", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("blendMode", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "string"));

        AddOutputPort(std::make_shared<VisualScript::Port>("result", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Remap value range node
 */
class RemapNode : public VisualScript::Node {
public:
    RemapNode() : Node("Remap", "Remap") {
        SetCategory(VisualScript::NodeCategory::Math);
        SetDescription("Remaps value range");

        AddInputPort(std::make_shared<VisualScript::Port>("input", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("inputMin", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("inputMax", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("outputMin", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("outputMax", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("result", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Curve/gradient adjustment node
 */
class CurveNode : public VisualScript::Node {
public:
    CurveNode() : Node("Curve", "Curve") {
        SetCategory(VisualScript::NodeCategory::Math);
        SetDescription("Applies curve transformation to values");

        AddInputPort(std::make_shared<VisualScript::Port>("input", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("curveType", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "string"));
        AddInputPort(std::make_shared<VisualScript::Port>("strength", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("result", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

/**
 * @brief Clamp node
 */
class ClampNode : public VisualScript::Node {
public:
    ClampNode() : Node("Clamp", "Clamp") {
        SetCategory(VisualScript::NodeCategory::Math);
        SetDescription("Clamps values to range");

        AddInputPort(std::make_shared<VisualScript::Port>("input", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("min", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("max", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("result", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override;
};

// =============================================================================
// Data Structures
// =============================================================================

/**
 * @brief Heightmap data structure
 */
class HeightmapData {
public:
    HeightmapData(int width, int height)
        : m_width(width), m_height(height), m_data(width * height, 0.0f) {}

    float Get(int x, int y) const {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) return 0.0f;
        return m_data[y * m_width + x];
    }

    void Set(int x, int y, float value) {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
        m_data[y * m_width + x] = value;
    }

    float GetBilinear(float x, float y) const;
    glm::vec3 GetNormal(int x, int y, float scale = 1.0f) const;

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    std::vector<float>& GetData() { return m_data; }
    const std::vector<float>& GetData() const { return m_data; }

private:
    int m_width;
    int m_height;
    std::vector<float> m_data;
};

/**
 * @brief Biome information
 */
struct BiomeInfo {
    int biomeId = 0;
    std::string biomeName;
    glm::vec3 color = glm::vec3(0.5f);
    float minTemperature = -10.0f;
    float maxTemperature = 30.0f;
    float minPrecipitation = 0.0f;
    float maxPrecipitation = 2000.0f;
    float minElevation = 0.0f;
    float maxElevation = 3000.0f;
};

} // namespace ProcGen
} // namespace Nova
