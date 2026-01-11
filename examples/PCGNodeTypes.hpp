#pragma once

#include "PCGNodeGraph.hpp"
#include "DataSourceManager.hpp"
#include <memory>

/**
 * @brief Generalized PCG node types
 *
 * Inspired by:
 * - Houdini: Attribute-based, context-driven, flexible types
 * - Substance Designer: Clear input/output types, material-focused
 * - UE5 PCG: Spatial queries, point generation, filtering
 *
 * Core Concepts:
 * 1. Everything flows as typed data (Float, Vector, Texture, PointCloud, etc.)
 * 2. Nodes are context-aware (know position, can query world data)
 * 3. Strongly typed but with implicit conversions where sensible
 * 4. Chainable and composable
 */

namespace PCG {

// =============================================================================
// Data Types (Expanded)
// =============================================================================

/**
 * @brief Extended pin types for generalized system
 */
enum class DataType {
    // Primitives
    Boolean,        // True/false
    Integer,        // Whole numbers
    Float,          // Decimal numbers
    Vector2,        // 2D vector
    Vector3,        // 3D vector
    Vector4,        // 4D vector (or RGBA color)
    Color,          // RGB/RGBA color (alias for Vector4)
    String,         // Text string

    // Arrays
    FloatArray,     // Array of floats
    VectorArray,    // Array of vectors
    ColorArray,     // Array of colors

    // Textures/Fields
    Texture2D,      // 2D texture/image
    Texture3D,      // 3D volumetric texture
    NoiseField,     // Continuous noise field
    DistanceField,  // Signed distance field
    VectorField,    // Flow/direction field

    // Geometry/Spatial
    PointCloud,     // Collection of points with attributes
    Mesh,           // Triangle mesh
    Spline,         // Curve/path
    Volume,         // Voxel volume

    // Data
    Attribute,      // Generic attribute (key-value)
    Metadata,       // Metadata dictionary

    // Special
    Terrain,        // Terrain heightmap
    Biome,          // Biome data
    Mask,           // Boolean mask/selection
    Transform,      // Transform matrix

    Any             // Wildcard (for flexible nodes)
};

/**
 * @brief Data packet - holds actual data flowing through graph
 */
struct DataPacket {
    DataType type;

    // Primitive values
    bool boolValue = false;
    int intValue = 0;
    float floatValue = 0.0f;
    glm::vec2 vec2Value{0.0f};
    glm::vec3 vec3Value{0.0f};
    glm::vec4 vec4Value{0.0f};
    std::string stringValue;

    // Array values
    std::vector<float> floatArray;
    std::vector<glm::vec3> vectorArray;
    std::vector<glm::vec4> colorArray;

    // Texture data
    struct TextureData {
        int width = 0;
        int height = 0;
        int depth = 1;  // 1 for 2D, >1 for 3D
        int channels = 1;  // 1=grayscale, 3=RGB, 4=RGBA
        std::vector<float> data;
    };
    std::shared_ptr<TextureData> textureData;

    // Point cloud
    struct PointCloudData {
        std::vector<glm::vec3> positions;
        std::unordered_map<std::string, std::vector<float>> attributes;  // Per-point attributes
    };
    std::shared_ptr<PointCloudData> pointCloudData;

    // Metadata
    std::unordered_map<std::string, std::string> metadata;

    // Implicit conversions
    float AsFloat() const {
        if (type == DataType::Float) return floatValue;
        if (type == DataType::Integer) return static_cast<float>(intValue);
        if (type == DataType::Boolean) return boolValue ? 1.0f : 0.0f;
        return 0.0f;
    }

    glm::vec3 AsVec3() const {
        if (type == DataType::Vector3) return vec3Value;
        if (type == DataType::Vector2) return glm::vec3(vec2Value, 0.0f);
        if (type == DataType::Float) return glm::vec3(floatValue);
        return glm::vec3(0.0f);
    }
};

// =============================================================================
// Generalized Base Node
// =============================================================================

/**
 * @brief Enhanced base node with data packet support
 */
class GeneralizedNode : public PCGNode {
public:
    GeneralizedNode(int id, const std::string& name, NodeCategory category)
        : PCGNode(id, name, category) {}

    virtual ~GeneralizedNode() = default;

    /**
     * @brief Execute with data packets
     */
    virtual void ExecutePacket(const PCGContext& context,
                               const std::vector<DataPacket>& inputs,
                               std::vector<DataPacket>& outputs) = 0;

    /**
     * @brief Get output data packet
     */
    virtual DataPacket GetOutputPacket(int pinIndex) const {
        if (pinIndex >= 0 && pinIndex < m_outputPackets.size()) {
            return m_outputPackets[pinIndex];
        }
        return DataPacket();
    }

protected:
    std::vector<DataPacket> m_outputPackets;
};

// =============================================================================
// Geospatial Query Nodes (Free Data Sources)
// =============================================================================

/**
 * @brief Generic geospatial query node
 */
class GeospatialQueryNode : public GeneralizedNode {
public:
    GeospatialQueryNode(int id, const std::string& name, DataSource::SourceType sourceType)
        : GeneralizedNode(id, name, NodeCategory::RealWorldData)
        , m_sourceType(sourceType) {
        AddInput("Lat/Long", PinType::Vec2);
        AddInput("Zoom Level", PinType::Float);
        AddOutput("Data", PinType::Float);  // Type varies by source
        AddOutput("Texture", PinType::Custom);  // Full tile texture
    }

    void Execute(const PCGContext& context) override {
        // Query data source manager
        auto latLon = GetInputVec2(0);
        int zoom = static_cast<int>(GetInputFloat(1));

        // TODO: Query DataSourceManager
        m_value = 0.0f;
    }

    void ExecutePacket(const PCGContext& context,
                      const std::vector<DataPacket>& inputs,
                      std::vector<DataPacket>& outputs) override {
        // Extract inputs
        glm::vec2 latLon(context.latitude, context.longitude);
        int zoom = 10;

        if (!inputs.empty() && inputs[0].type == DataType::Vector2) {
            latLon = inputs[0].vec2Value;
        }
        if (inputs.size() > 1) {
            zoom = static_cast<int>(inputs[1].AsFloat());
        }

        // Query data source
        if (m_dataSourceManager) {
            auto result = m_dataSourceManager->Query(m_sourceType, latLon.x, latLon.y, zoom);

            DataPacket outPacket;
            outPacket.type = DataType::Float;
            outPacket.floatValue = !result.empty() ? result[0] : 0.0f;
            outputs.push_back(outPacket);
        }
    }

    void SetDataSourceManager(std::shared_ptr<DataSource::DataSourceManager> manager) {
        m_dataSourceManager = manager;
    }

protected:
    DataSource::SourceType m_sourceType;
    std::shared_ptr<DataSource::DataSourceManager> m_dataSourceManager;
    float m_value = 0.0f;

    // Helper methods
    glm::vec2 GetInputVec2(int index) const {
        // TODO: Get from connected input
        return glm::vec2(0.0f);
    }

    float GetInputFloat(int index) const {
        // TODO: Get from connected input
        return 0.0f;
    }
};

/**
 * @brief SRTM Elevation node
 */
class SRTMElevationNode : public GeospatialQueryNode {
public:
    SRTMElevationNode(int id)
        : GeospatialQueryNode(id, "SRTM Elevation", DataSource::SourceType::SRTM_30m) {
        // Override outputs to be more specific
        m_outputs.clear();
        AddOutput("Elevation (m)", PinType::Float);
        AddOutput("Heightmap", PinType::Custom);
    }
};

/**
 * @brief Copernicus DEM node
 */
class CopernicusDEMNode : public GeospatialQueryNode {
public:
    CopernicusDEMNode(int id)
        : GeospatialQueryNode(id, "Copernicus DEM", DataSource::SourceType::COPERNICUS_DEM) {
        m_outputs.clear();
        AddOutput("Elevation (m)", PinType::Float);
        AddOutput("Heightmap", PinType::Custom);
    }
};

/**
 * @brief Sentinel-2 Satellite Imagery node
 */
class Sentinel2Node : public GeospatialQueryNode {
public:
    Sentinel2Node(int id)
        : GeospatialQueryNode(id, "Sentinel-2 RGB", DataSource::SourceType::SENTINEL2_RGB) {
        m_outputs.clear();
        AddOutput("RGB Color", PinType::Color);
        AddOutput("Red", PinType::Float);
        AddOutput("Green", PinType::Float);
        AddOutput("Blue", PinType::Float);
        AddOutput("Image Texture", PinType::Custom);
    }
};

/**
 * @brief Sentinel-2 NDVI (Vegetation Index) node
 */
class Sentinel2NDVINode : public GeospatialQueryNode {
public:
    Sentinel2NDVINode(int id)
        : GeospatialQueryNode(id, "Sentinel-2 NDVI", DataSource::SourceType::SENTINEL2_NDVI) {
        m_outputs.clear();
        AddOutput("NDVI", PinType::Float);  // -1 to 1, higher = more vegetation
        AddOutput("Is Vegetated", PinType::Float);  // Boolean-like (0 or 1)
    }
};

/**
 * @brief OSM Roads node
 */
class OSMRoadsNode : public GeospatialQueryNode {
public:
    OSMRoadsNode(int id)
        : GeospatialQueryNode(id, "OSM Roads", DataSource::SourceType::OSM_ROADS) {
        m_outputs.clear();
        AddOutput("Distance to Road", PinType::Float);
        AddOutput("Road Type", PinType::Float);  // Encoded: 0=none, 1=path, 2=residential, 3=highway
        AddOutput("On Road", PinType::Float);  // Boolean
    }
};

/**
 * @brief OSM Buildings node
 */
class OSMBuildingsNode : public GeospatialQueryNode {
public:
    OSMBuildingsNode(int id)
        : GeospatialQueryNode(id, "OSM Buildings", DataSource::SourceType::OSM_BUILDINGS) {
        m_outputs.clear();
        AddOutput("Distance to Building", PinType::Float);
        AddOutput("Building Density", PinType::Float);  // Buildings per km²
        AddOutput("In Building", PinType::Float);  // Boolean
    }
};

/**
 * @brief ESA WorldCover Land Cover node
 */
class ESAWorldCoverNode : public GeospatialQueryNode {
public:
    ESAWorldCoverNode(int id)
        : GeospatialQueryNode(id, "ESA WorldCover", DataSource::SourceType::ESA_WORLDCOVER) {
        m_outputs.clear();
        AddOutput("Land Cover Type", PinType::Float);
        // Types: 10=Tree, 20=Shrub, 30=Grass, 40=Crop, 50=Built, 60=Bare, 70=Snow, 80=Water, 90=Wetland, 95=Mangrove, 100=Moss
        AddOutput("Is Forest", PinType::Float);
        AddOutput("Is Urban", PinType::Float);
        AddOutput("Is Water", PinType::Float);
    }
};

/**
 * @brief OpenWeather Temperature node
 */
class OpenWeatherTempNode : public GeospatialQueryNode {
public:
    OpenWeatherTempNode(int id)
        : GeospatialQueryNode(id, "OpenWeather Temp", DataSource::SourceType::OPENWEATHER_TEMP) {
        m_outputs.clear();
        AddOutput("Temperature (°C)", PinType::Float);
        AddOutput("Temperature (K)", PinType::Float);
    }
};

/**
 * @brief WorldClim Precipitation node
 */
class WorldClimPrecipNode : public GeospatialQueryNode {
public:
    WorldClimPrecipNode(int id)
        : GeospatialQueryNode(id, "WorldClim Precip", DataSource::SourceType::WORLDCLIM_PRECIP) {
        AddInput("Month", PinType::Float);  // 1-12
        m_outputs.clear();
        AddOutput("Precipitation (mm)", PinType::Float);
    }
};

/**
 * @brief WorldPop Population Density node
 */
class WorldPopDensityNode : public GeospatialQueryNode {
public:
    WorldPopDensityNode(int id)
        : GeospatialQueryNode(id, "WorldPop Density", DataSource::SourceType::WORLDPOP_DENSITY) {
        m_outputs.clear();
        AddOutput("People per km²", PinType::Float);
        AddOutput("Is Urban", PinType::Float);  // Density > 1000
        AddOutput("Is Rural", PinType::Float);  // Density < 150
    }
};

// =============================================================================
// Advanced Generator Nodes (Houdini-inspired)
// =============================================================================

/**
 * @brief Point Scatter node - generates point cloud
 */
class PointScatterNode : public GeneralizedNode {
public:
    PointScatterNode(int id)
        : GeneralizedNode(id, "Point Scatter", NodeCategory::AssetPlacement) {
        AddInput("Density", PinType::Float);
        AddInput("Mask", PinType::Float);  // 0-1, where to scatter
        AddInput("Seed", PinType::Float);
        AddOutput("Points", PinType::Custom);  // PointCloud
    }

    void Execute(const PCGContext& context) override {}

    void ExecutePacket(const PCGContext& context,
                      const std::vector<DataPacket>& inputs,
                      std::vector<DataPacket>& outputs) override {
        float density = inputs.empty() ? 1.0f : inputs[0].AsFloat();
        float mask = inputs.size() > 1 ? inputs[1].AsFloat() : 1.0f;

        DataPacket outPacket;
        outPacket.type = DataType::PointCloud;
        outPacket.pointCloudData = std::make_shared<DataPacket::PointCloudData>();

        // Generate points based on density and mask
        if (mask > 0.5f) {
            int numPoints = static_cast<int>(density * mask * 10);
            for (int i = 0; i < numPoints; ++i) {
                // TODO: Random distribution
                outPacket.pointCloudData->positions.push_back(context.position);
            }
        }

        outputs.push_back(outPacket);
    }
};

/**
 * @brief Attribute Wrangle node - script arbitrary operations
 */
class AttributeWrangleNode : public GeneralizedNode {
public:
    AttributeWrangleNode(int id)
        : GeneralizedNode(id, "Attribute Wrangle", NodeCategory::Math) {
        AddInput("Input", PinType::Custom);
        AddOutput("Output", PinType::Custom);
    }

    void Execute(const PCGContext& context) override {}

    void ExecutePacket(const PCGContext& context,
                      const std::vector<DataPacket>& inputs,
                      std::vector<DataPacket>& outputs) override {
        // TODO: Execute user-defined script
        // Could use Lua, Python, or custom expression language
        if (!inputs.empty()) {
            outputs.push_back(inputs[0]);
        }
    }

    void SetScript(const std::string& script) {
        m_script = script;
    }

private:
    std::string m_script;
};

/**
 * @brief Blend node - lerp between two inputs
 */
class BlendNode : public GeneralizedNode {
public:
    BlendNode(int id)
        : GeneralizedNode(id, "Blend", NodeCategory::Math) {
        AddInput("A", PinType::Float);
        AddInput("B", PinType::Float);
        AddInput("Factor", PinType::Float);  // 0=A, 1=B
        AddOutput("Result", PinType::Float);
    }

    void Execute(const PCGContext& context) override {}

    void ExecutePacket(const PCGContext& context,
                      const std::vector<DataPacket>& inputs,
                      std::vector<DataPacket>& outputs) override {
        if (inputs.size() < 3) return;

        float a = inputs[0].AsFloat();
        float b = inputs[1].AsFloat();
        float factor = glm::clamp(inputs[2].AsFloat(), 0.0f, 1.0f);

        DataPacket outPacket;
        outPacket.type = DataType::Float;
        outPacket.floatValue = glm::mix(a, b, factor);
        outputs.push_back(outPacket);
    }
};

/**
 * @brief Remap Range node - remap value from one range to another
 */
class RemapRangeNode : public GeneralizedNode {
public:
    RemapRangeNode(int id)
        : GeneralizedNode(id, "Remap Range", NodeCategory::Math) {
        AddInput("Value", PinType::Float);
        AddInput("Input Min", PinType::Float);
        AddInput("Input Max", PinType::Float);
        AddInput("Output Min", PinType::Float);
        AddInput("Output Max", PinType::Float);
        AddInput("Clamp", PinType::Float);  // Boolean
        AddOutput("Result", PinType::Float);
    }

    void Execute(const PCGContext& context) override {}

    void ExecutePacket(const PCGContext& context,
                      const std::vector<DataPacket>& inputs,
                      std::vector<DataPacket>& outputs) override {
        if (inputs.size() < 5) return;

        float value = inputs[0].AsFloat();
        float inMin = inputs[1].AsFloat();
        float inMax = inputs[2].AsFloat();
        float outMin = inputs[3].AsFloat();
        float outMax = inputs[4].AsFloat();
        bool clamp = inputs.size() > 5 ? (inputs[5].AsFloat() > 0.5f) : true;

        // Remap
        float t = (value - inMin) / (inMax - inMin);
        if (clamp) t = glm::clamp(t, 0.0f, 1.0f);
        float result = outMin + t * (outMax - outMin);

        DataPacket outPacket;
        outPacket.type = DataType::Float;
        outPacket.floatValue = result;
        outputs.push_back(outPacket);
    }
};

} // namespace PCG
