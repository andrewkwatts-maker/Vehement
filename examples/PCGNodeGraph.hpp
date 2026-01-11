#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

/**
 * @brief Procedural Content Generation Node Graph System
 *
 * Visual scripting system for generating terrain and placing assets
 * based on noise functions, real-world data, and custom logic.
 * Similar to UE5's PCG framework.
 */

namespace PCG {

// Forward declarations
class PCGNode;
class PCGGraph;

/**
 * @brief Pin types for node connections
 */
enum class PinType {
    Float,      // Single float value
    Vec2,       // 2D vector
    Vec3,       // 3D position
    Color,      // RGBA color
    Noise,      // Noise field data
    Mask,       // Boolean mask
    Terrain,    // Terrain heightmap
    AssetList,  // List of assets to place
    Transform,  // Position/rotation/scale
    Custom      // Custom data type
};

/**
 * @brief Node categories
 */
enum class NodeCategory {
    Input,          // Inputs (position, lat/long, time, etc.)
    Noise,          // Noise generators (Perlin, Simplex, Voronoi, etc.)
    Math,           // Math operations (add, multiply, etc.)
    Blend,          // Blend operations (lerp, overlay, min, max)
    RealWorldData,  // Real-world data (elevation, roads, buildings, foliage)
    Terrain,        // Terrain operations (heightmap, splat maps)
    AssetPlacement, // Asset spawning and distribution
    Filter,         // Filtering and masking
    Output          // Final outputs
};

/**
 * @brief Input/Output pin on a node
 */
struct PCGPin {
    std::string name;
    PinType type;
    bool isInput;
    int nodeId;     // Parent node
    int pinIndex;   // Index within node

    // Connection info (for input pins)
    int connectedNodeId = -1;
    int connectedPinIndex = -1;
    bool isConnected = false;

    // Default value (when not connected)
    float defaultFloat = 0.0f;
    glm::vec2 defaultVec2{0.0f};
    glm::vec3 defaultVec3{0.0f};
};

/**
 * @brief Execution context for node evaluation
 */
struct PCGContext {
    // World position
    glm::vec3 position{0.0f};

    // Geographic coordinates
    double latitude = 0.0;
    double longitude = 0.0;

    // Real-world data
    float elevation = 0.0f;
    float roadDistance = 999.0f;  // Distance to nearest road
    float buildingDistance = 999.0f;
    std::string biome = "default";

    // Seed for deterministic randomness
    uint64_t seed = 0;

    // Custom parameters
    std::unordered_map<std::string, float> parameters;
};

/**
 * @brief Base class for all PCG nodes
 */
class PCGNode {
public:
    PCGNode(int id, const std::string& name, NodeCategory category)
        : m_id(id), m_name(name), m_category(category) {}

    virtual ~PCGNode() = default;

    // Node info
    int GetId() const { return m_id; }
    const std::string& GetName() const { return m_name; }
    NodeCategory GetCategory() const { return m_category; }

    // UI position
    glm::vec2 GetPosition() const { return m_position; }
    void SetPosition(const glm::vec2& pos) { m_position = pos; }

    // Pins
    const std::vector<PCGPin>& GetInputPins() const { return m_inputs; }
    const std::vector<PCGPin>& GetOutputPins() const { return m_outputs; }

    PCGPin* GetInputPin(int index) {
        if (index >= 0 && index < m_inputs.size()) return &m_inputs[index];
        return nullptr;
    }

    PCGPin* GetOutputPin(int index) {
        if (index >= 0 && index < m_outputs.size()) return &m_outputs[index];
        return nullptr;
    }

    // Execution
    virtual void Execute(const PCGContext& context) = 0;

    // Output retrieval
    virtual float GetFloatOutput(int pinIndex) const { return 0.0f; }
    virtual glm::vec3 GetVec3Output(int pinIndex) const { return glm::vec3(0.0f); }

protected:
    int m_id;
    std::string m_name;
    NodeCategory m_category;
    glm::vec2 m_position{0.0f};

    std::vector<PCGPin> m_inputs;
    std::vector<PCGPin> m_outputs;

    void AddInput(const std::string& name, PinType type) {
        PCGPin pin;
        pin.name = name;
        pin.type = type;
        pin.isInput = true;
        pin.nodeId = m_id;
        pin.pinIndex = static_cast<int>(m_inputs.size());
        m_inputs.push_back(pin);
    }

    void AddOutput(const std::string& name, PinType type) {
        PCGPin pin;
        pin.name = name;
        pin.type = type;
        pin.isInput = false;
        pin.nodeId = m_id;
        pin.pinIndex = static_cast<int>(m_outputs.size());
        m_outputs.push_back(pin);
    }
};

// =============================================================================
// Input Nodes
// =============================================================================

class PositionInputNode : public PCGNode {
public:
    PositionInputNode(int id) : PCGNode(id, "Position", NodeCategory::Input) {
        AddOutput("XYZ", PinType::Vec3);
        AddOutput("X", PinType::Float);
        AddOutput("Y", PinType::Float);
        AddOutput("Z", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        m_position = context.position;
    }

    float GetFloatOutput(int pinIndex) const override {
        if (pinIndex == 1) return m_position.x;
        if (pinIndex == 2) return m_position.y;
        if (pinIndex == 3) return m_position.z;
        return 0.0f;
    }

    glm::vec3 GetVec3Output(int pinIndex) const override {
        if (pinIndex == 0) return m_position;
        return glm::vec3(0.0f);
    }

private:
    glm::vec3 m_position{0.0f};
};

class LatLongInputNode : public PCGNode {
public:
    LatLongInputNode(int id) : PCGNode(id, "Lat/Long", NodeCategory::Input) {
        AddOutput("Latitude", PinType::Float);
        AddOutput("Longitude", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        m_latitude = static_cast<float>(context.latitude);
        m_longitude = static_cast<float>(context.longitude);
    }

    float GetFloatOutput(int pinIndex) const override {
        if (pinIndex == 0) return m_latitude;
        if (pinIndex == 1) return m_longitude;
        return 0.0f;
    }

private:
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
};

// =============================================================================
// Noise Nodes
// =============================================================================

class PerlinNoiseNode : public PCGNode {
public:
    PerlinNoiseNode(int id) : PCGNode(id, "Perlin Noise", NodeCategory::Noise) {
        AddInput("Position", PinType::Vec3);
        AddInput("Scale", PinType::Float);
        AddInput("Octaves", PinType::Float);
        AddInput("Persistence", PinType::Float);
        AddOutput("Value", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        // TODO: Implement Perlin noise
        m_output = 0.5f; // Placeholder
    }

    float GetFloatOutput(int pinIndex) const override {
        return m_output;
    }

private:
    float m_output = 0.0f;
    float m_scale = 1.0f;
    int m_octaves = 4;
    float m_persistence = 0.5f;
};

class SimplexNoiseNode : public PCGNode {
public:
    SimplexNoiseNode(int id) : PCGNode(id, "Simplex Noise", NodeCategory::Noise) {
        AddInput("Position", PinType::Vec3);
        AddInput("Scale", PinType::Float);
        AddOutput("Value", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        // TODO: Implement Simplex noise
        m_output = 0.5f; // Placeholder
    }

    float GetFloatOutput(int pinIndex) const override {
        return m_output;
    }

private:
    float m_output = 0.0f;
};

class VoronoiNoiseNode : public PCGNode {
public:
    VoronoiNoiseNode(int id) : PCGNode(id, "Voronoi", NodeCategory::Noise) {
        AddInput("Position", PinType::Vec3);
        AddInput("Scale", PinType::Float);
        AddInput("Randomness", PinType::Float);
        AddOutput("Distance", PinType::Float);
        AddOutput("Cell ID", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        // TODO: Implement Voronoi
        m_distance = 0.5f; // Placeholder
        m_cellId = 0.0f;
    }

    float GetFloatOutput(int pinIndex) const override {
        if (pinIndex == 0) return m_distance;
        if (pinIndex == 1) return m_cellId;
        return 0.0f;
    }

private:
    float m_distance = 0.0f;
    float m_cellId = 0.0f;
};

// =============================================================================
// Real-World Data Nodes
// =============================================================================

class ElevationDataNode : public PCGNode {
public:
    ElevationDataNode(int id) : PCGNode(id, "Elevation Data", NodeCategory::RealWorldData) {
        AddInput("Lat/Long", PinType::Vec2);
        AddOutput("Elevation", PinType::Float);
        AddOutput("Slope", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        // TODO: Query real elevation data
        m_elevation = context.elevation;
        m_slope = 0.0f; // Placeholder
    }

    float GetFloatOutput(int pinIndex) const override {
        if (pinIndex == 0) return m_elevation;
        if (pinIndex == 1) return m_slope;
        return 0.0f;
    }

private:
    float m_elevation = 0.0f;
    float m_slope = 0.0f;
};

class RoadDistanceNode : public PCGNode {
public:
    RoadDistanceNode(int id) : PCGNode(id, "Road Distance", NodeCategory::RealWorldData) {
        AddInput("Lat/Long", PinType::Vec2);
        AddOutput("Distance", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        m_distance = context.roadDistance;
    }

    float GetFloatOutput(int pinIndex) const override {
        return m_distance;
    }

private:
    float m_distance = 999.0f;
};

class BuildingDataNode : public PCGNode {
public:
    BuildingDataNode(int id) : PCGNode(id, "Building Data", NodeCategory::RealWorldData) {
        AddInput("Lat/Long", PinType::Vec2);
        AddOutput("Distance", PinType::Float);
        AddOutput("Density", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        m_distance = context.buildingDistance;
        m_density = 0.0f; // Placeholder
    }

    float GetFloatOutput(int pinIndex) const override {
        if (pinIndex == 0) return m_distance;
        if (pinIndex == 1) return m_density;
        return 0.0f;
    }

private:
    float m_distance = 999.0f;
    float m_density = 0.0f;
};

class BiomeDataNode : public PCGNode {
public:
    BiomeDataNode(int id) : PCGNode(id, "Biome Data", NodeCategory::RealWorldData) {
        AddInput("Lat/Long", PinType::Vec2);
        AddOutput("Temperature", PinType::Float);
        AddOutput("Rainfall", PinType::Float);
        AddOutput("Foliage Density", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        // TODO: Query biome data based on lat/long
        m_temperature = 20.0f;
        m_rainfall = 0.5f;
        m_foliageDensity = 0.5f;
    }

    float GetFloatOutput(int pinIndex) const override {
        if (pinIndex == 0) return m_temperature;
        if (pinIndex == 1) return m_rainfall;
        if (pinIndex == 2) return m_foliageDensity;
        return 0.0f;
    }

private:
    float m_temperature = 20.0f;
    float m_rainfall = 0.5f;
    float m_foliageDensity = 0.5f;
};

// =============================================================================
// Math Nodes
// =============================================================================

class MathNode : public PCGNode {
public:
    enum class Operation {
        Add, Subtract, Multiply, Divide,
        Min, Max, Clamp, Lerp,
        Power, Abs, Sin, Cos
    };

    MathNode(int id, Operation op)
        : PCGNode(id, GetOperationName(op), NodeCategory::Math)
        , m_operation(op) {
        AddInput("A", PinType::Float);
        if (op == Operation::Clamp || op == Operation::Lerp) {
            AddInput("B", PinType::Float);
            AddInput("C", PinType::Float);
        } else if (op != Operation::Abs && op != Operation::Sin && op != Operation::Cos) {
            AddInput("B", PinType::Float);
        }
        AddOutput("Result", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        // TODO: Implement math operations
        m_result = 0.0f;
    }

    float GetFloatOutput(int pinIndex) const override {
        return m_result;
    }

private:
    Operation m_operation;
    float m_result = 0.0f;

    static std::string GetOperationName(Operation op) {
        switch (op) {
            case Operation::Add: return "Add";
            case Operation::Subtract: return "Subtract";
            case Operation::Multiply: return "Multiply";
            case Operation::Divide: return "Divide";
            case Operation::Min: return "Min";
            case Operation::Max: return "Max";
            case Operation::Clamp: return "Clamp";
            case Operation::Lerp: return "Lerp";
            case Operation::Power: return "Power";
            case Operation::Abs: return "Abs";
            case Operation::Sin: return "Sin";
            case Operation::Cos: return "Cos";
            default: return "Math";
        }
    }
};

// =============================================================================
// PCG Graph
// =============================================================================

/**
 * @brief Graph containing connected PCG nodes
 */
class PCGGraph {
public:
    PCGGraph() = default;
    ~PCGGraph() = default;

    // Node management
    int AddNode(std::unique_ptr<PCGNode> node) {
        int id = node->GetId();
        m_nodes[id] = std::move(node);
        return id;
    }

    void RemoveNode(int nodeId) {
        m_nodes.erase(nodeId);
        // TODO: Remove connections
    }

    PCGNode* GetNode(int nodeId) {
        auto it = m_nodes.find(nodeId);
        return it != m_nodes.end() ? it->second.get() : nullptr;
    }

    const std::unordered_map<int, std::unique_ptr<PCGNode>>& GetNodes() const {
        return m_nodes;
    }

    // Connection management
    bool ConnectPins(int sourceNodeId, int sourcePin, int targetNodeId, int targetPin) {
        auto* sourceNode = GetNode(sourceNodeId);
        auto* targetNode = GetNode(targetNodeId);
        if (!sourceNode || !targetNode) return false;

        auto* output = sourceNode->GetOutputPin(sourcePin);
        auto* input = targetNode->GetInputPin(targetPin);
        if (!output || !input) return false;

        input->connectedNodeId = sourceNodeId;
        input->connectedPinIndex = sourcePin;
        input->isConnected = true;

        return true;
    }

    void DisconnectPin(int nodeId, int pinIndex) {
        auto* node = GetNode(nodeId);
        if (!node) return;

        auto* pin = node->GetInputPin(pinIndex);
        if (pin) {
            pin->connectedNodeId = -1;
            pin->connectedPinIndex = -1;
            pin->isConnected = false;
        }
    }

    // Execution
    void Execute(const PCGContext& context) {
        // TODO: Implement topological sort and execute in order
        for (auto& [id, node] : m_nodes) {
            node->Execute(context);
        }
    }

    // Serialization
    void SaveToFile(const std::string& path);
    bool LoadFromFile(const std::string& path);

private:
    std::unordered_map<int, std::unique_ptr<PCGNode>> m_nodes;
    int m_nextNodeId = 1;
};

} // namespace PCG
