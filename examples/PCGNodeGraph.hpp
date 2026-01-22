#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <queue>
#include <map>
#include <glm/glm.hpp>
#include <FastNoise/FastNoise.h>

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

    // Type identifier for serialization (override in derived classes)
    virtual std::string GetTypeId() const { return m_name; }

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

        // Set default values for inputs
        m_inputs[1].defaultFloat = 1.0f;    // Scale
        m_inputs[2].defaultFloat = 4.0f;    // Octaves
        m_inputs[3].defaultFloat = 0.5f;    // Persistence
    }

    void Execute(const PCGContext& context) override {
        // Get position from context (input pin 0 could override but uses context position by default)
        glm::vec3 pos = context.position;

        // Get scale (frequency) - use default value
        float scale = m_inputs[1].defaultFloat;
        if (scale <= 0.0f) scale = 1.0f;

        // Get octaves count
        int octaves = static_cast<int>(m_inputs[2].defaultFloat);
        octaves = std::max(1, std::min(octaves, 8)); // Clamp to reasonable range

        // Get persistence (gain)
        float persistence = m_inputs[3].defaultFloat;
        persistence = std::max(0.0f, std::min(persistence, 1.0f));

        // Create FastNoise Perlin generator
        auto fnPerlin = FastNoise::New<FastNoise::Perlin>();

        // For multi-octave (FBm) noise
        if (octaves > 1) {
            auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();
            fnFractal->SetSource(fnPerlin);
            fnFractal->SetOctaveCount(octaves);
            fnFractal->SetGain(persistence);
            fnFractal->SetLacunarity(2.0f);

            // Generate single noise value at position
            // FastNoise frequency is applied to coordinates
            float noiseValue = fnFractal->GenSingle3D(
                pos.x * scale,
                pos.y * scale,
                pos.z * scale,
                static_cast<int>(context.seed)
            );

            // Normalize from [-1, 1] to [0, 1]
            m_output = (noiseValue + 1.0f) * 0.5f;
        } else {
            // Single octave Perlin noise
            float noiseValue = fnPerlin->GenSingle3D(
                pos.x * scale,
                pos.y * scale,
                pos.z * scale,
                static_cast<int>(context.seed)
            );

            // Normalize from [-1, 1] to [0, 1]
            m_output = (noiseValue + 1.0f) * 0.5f;
        }
    }

    float GetFloatOutput(int pinIndex) const override {
        return m_output;
    }

    // Setters for node parameters (used by editor)
    void SetScale(float scale) { m_inputs[1].defaultFloat = scale; }
    void SetOctaves(int octaves) { m_inputs[2].defaultFloat = static_cast<float>(octaves); }
    void SetPersistence(float persistence) { m_inputs[3].defaultFloat = persistence; }

private:
    float m_output = 0.0f;
};

class SimplexNoiseNode : public PCGNode {
public:
    SimplexNoiseNode(int id) : PCGNode(id, "Simplex Noise", NodeCategory::Noise) {
        AddInput("Position", PinType::Vec3);
        AddInput("Scale", PinType::Float);
        AddOutput("Value", PinType::Float);

        // Set default values for inputs
        m_inputs[1].defaultFloat = 1.0f;    // Scale
    }

    void Execute(const PCGContext& context) override {
        // Get position from context
        glm::vec3 pos = context.position;

        // Get scale (frequency) - use default value
        float scale = m_inputs[1].defaultFloat;
        if (scale <= 0.0f) scale = 1.0f;

        // Create FastNoise Simplex generator
        auto fnSimplex = FastNoise::New<FastNoise::Simplex>();

        // Generate single noise value at position
        float noiseValue = fnSimplex->GenSingle3D(
            pos.x * scale,
            pos.y * scale,
            pos.z * scale,
            static_cast<int>(context.seed)
        );

        // Normalize from [-1, 1] to [0, 1]
        m_output = (noiseValue + 1.0f) * 0.5f;
    }

    float GetFloatOutput(int pinIndex) const override {
        return m_output;
    }

    // Setter for node parameters (used by editor)
    void SetScale(float scale) { m_inputs[1].defaultFloat = scale; }

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

        // Set default values for inputs
        m_inputs[1].defaultFloat = 1.0f;    // Scale
        m_inputs[2].defaultFloat = 1.0f;    // Randomness (jitter)
    }

    void Execute(const PCGContext& context) override {
        // Get position from context
        glm::vec3 pos = context.position;

        // Get scale (frequency) - use default value
        float scale = m_inputs[1].defaultFloat;
        if (scale <= 0.0f) scale = 1.0f;

        // Get randomness (jitter modifier) - clamp to [0, 1]
        float randomness = m_inputs[2].defaultFloat;
        randomness = std::max(0.0f, std::min(randomness, 1.0f));

        // Create FastNoise CellularDistance generator for Voronoi distance
        auto fnCellularDist = FastNoise::New<FastNoise::CellularDistance>();
        fnCellularDist->SetJitterModifier(randomness);
        fnCellularDist->SetDistanceFunction(FastNoise::DistanceFunction::Euclidean);

        // Generate distance to nearest cell
        float distValue = fnCellularDist->GenSingle3D(
            pos.x * scale,
            pos.y * scale,
            pos.z * scale,
            static_cast<int>(context.seed)
        );

        // Distance output is typically in range [0, ~1] but can vary
        // Normalize to [0, 1] range
        m_distance = std::max(0.0f, std::min(distValue, 1.0f));

        // Create FastNoise CellularValue generator for cell ID
        auto fnCellularVal = FastNoise::New<FastNoise::CellularValue>();
        fnCellularVal->SetJitterModifier(randomness);

        // Generate cell value (essentially random per-cell ID)
        float cellValue = fnCellularVal->GenSingle3D(
            pos.x * scale,
            pos.y * scale,
            pos.z * scale,
            static_cast<int>(context.seed)
        );

        // Normalize from [-1, 1] to [0, 1]
        m_cellId = (cellValue + 1.0f) * 0.5f;
    }

    float GetFloatOutput(int pinIndex) const override {
        if (pinIndex == 0) return m_distance;
        if (pinIndex == 1) return m_cellId;
        return 0.0f;
    }

    // Setters for node parameters (used by editor)
    void SetScale(float scale) { m_inputs[1].defaultFloat = scale; }
    void SetRandomness(float randomness) { m_inputs[2].defaultFloat = randomness; }

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
        // Get lat/long from input or context
        float lat = static_cast<float>(context.latitude);
        float lon = static_cast<float>(context.longitude);

        // Check if input pin is connected
        if (m_inputs[0].isConnected) {
            // Input would provide lat/long - use context as fallback
        }

        // Procedural elevation generation using multi-octave noise-like function
        // Since we don't have real elevation data, generate realistic terrain patterns

        // Convert lat/long to radians for trig functions
        float latRad = glm::radians(lat);
        float lonRad = glm::radians(lon);

        // Multi-frequency terrain generation (pseudo-noise using sin/cos)
        // Large scale continental features
        float continental = std::sin(latRad * 2.0f) * std::cos(lonRad * 3.0f) * 500.0f;

        // Medium scale mountain ranges
        float mountains = std::sin(latRad * 15.0f + lonRad * 20.0f) *
                         std::cos(latRad * 25.0f - lonRad * 18.0f) * 800.0f;
        mountains = std::max(0.0f, mountains); // Mountains are positive

        // Small scale hills and valleys
        float hills = std::sin(latRad * 50.0f) * std::cos(lonRad * 45.0f) * 100.0f;
        hills += std::sin(latRad * 80.0f + lonRad * 70.0f) * 50.0f;

        // Fine detail (local variation)
        float detail = std::sin(latRad * 200.0f) * std::cos(lonRad * 180.0f) * 20.0f;
        detail += std::cos(latRad * 300.0f + lonRad * 250.0f) * 10.0f;

        // Combine all scales with appropriate weights
        m_elevation = context.elevation; // Start with context if provided
        if (context.elevation == 0.0f) {
            // Generate procedural elevation if no real data
            m_elevation = 200.0f + continental + mountains + hills + detail;

            // Ensure reasonable elevation range (sea level to high mountains)
            m_elevation = std::max(-50.0f, std::min(4500.0f, m_elevation));
        }

        // Calculate slope from elevation gradient
        // Approximate by sampling nearby points
        float dx = 0.001f; // Small offset in degrees
        float elevNorth = 200.0f + std::sin(glm::radians(lat + dx) * 15.0f + lonRad * 20.0f) *
                         std::cos(glm::radians(lat + dx) * 25.0f - lonRad * 18.0f) * 800.0f;
        float elevEast = 200.0f + std::sin(latRad * 15.0f + glm::radians(lon + dx) * 20.0f) *
                        std::cos(latRad * 25.0f - glm::radians(lon + dx) * 18.0f) * 800.0f;

        // Slope as angle in degrees (0 = flat, 90 = vertical)
        float dElevX = elevEast - m_elevation;
        float dElevY = elevNorth - m_elevation;
        float gradient = std::sqrt(dElevX * dElevX + dElevY * dElevY);
        m_slope = std::atan(gradient / (dx * 111000.0f)) * 180.0f / 3.14159f; // Convert to degrees
        m_slope = std::min(90.0f, m_slope);
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
        // Get lat/long from context
        float lat = static_cast<float>(context.latitude);
        float lon = static_cast<float>(context.longitude);

        // Temperature model based on latitude and elevation
        // Base temperature decreases with latitude (equator is warmest)
        float absLat = std::abs(lat);
        float baseTemp = 30.0f - (absLat / 90.0f) * 50.0f; // 30C at equator, -20C at poles

        // Add seasonal variation based on longitude (simplified proxy for seasonal offset)
        float seasonalVariation = std::sin(glm::radians(lon) * 2.0f) * 5.0f;

        // Temperature lapse rate: ~6.5C per 1000m elevation
        float elevationEffect = context.elevation * 0.0065f;

        // Local variation using trig functions (simulates microclimates)
        float localVariation = std::sin(glm::radians(lat * 50.0f)) *
                              std::cos(glm::radians(lon * 45.0f)) * 3.0f;

        m_temperature = baseTemp + seasonalVariation - elevationEffect + localVariation;
        m_temperature = std::max(-40.0f, std::min(50.0f, m_temperature)); // Clamp to realistic range

        // Rainfall/moisture model
        // Higher near equator (ITCZ), mid-latitudes, and certain longitude bands
        // Lower near 30 degrees (subtropical highs) and poles

        // Latitude-based precipitation pattern
        float latEffect = 1.0f;
        if (absLat < 10.0f) {
            // Equatorial high rainfall (ITCZ)
            latEffect = 1.0f;
        } else if (absLat < 35.0f) {
            // Subtropical dry zone
            latEffect = 0.3f + (std::abs(absLat - 25.0f) / 25.0f) * 0.4f;
        } else if (absLat < 60.0f) {
            // Mid-latitude moderate rainfall
            latEffect = 0.6f + std::sin(glm::radians((absLat - 35.0f) * 4.0f)) * 0.3f;
        } else {
            // Polar low precipitation
            latEffect = 0.2f;
        }

        // Elevation increases precipitation (orographic effect) up to a point
        float elevEffect = 1.0f + std::min(context.elevation / 3000.0f, 0.5f);

        // Local variation (simulates rain shadows, etc.)
        float rainVariation = std::sin(glm::radians(lat * 30.0f + lon * 25.0f)) * 0.2f;

        m_rainfall = latEffect * elevEffect + rainVariation;
        m_rainfall = std::max(0.0f, std::min(1.0f, m_rainfall)); // Normalize to 0-1

        // Foliage density based on temperature and rainfall (Whittaker biome model)
        // Optimal conditions: moderate temperature (15-25C) and high rainfall
        float tempFactor = 1.0f - std::abs(m_temperature - 20.0f) / 40.0f;
        tempFactor = std::max(0.0f, tempFactor);

        // Need both warmth and water for vegetation
        m_foliageDensity = tempFactor * m_rainfall;

        // Extreme cold limits vegetation regardless of moisture
        if (m_temperature < -10.0f) {
            m_foliageDensity *= 0.2f;
        } else if (m_temperature < 0.0f) {
            m_foliageDensity *= 0.5f;
        }

        m_foliageDensity = std::max(0.0f, std::min(1.0f, m_foliageDensity));
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
        if (op == Operation::Clamp) {
            AddInput("Min", PinType::Float);
            AddInput("Max", PinType::Float);
            // Set defaults for clamp: min=0, max=1
            m_inputs[1].defaultFloat = 0.0f;
            m_inputs[2].defaultFloat = 1.0f;
        } else if (op == Operation::Lerp) {
            AddInput("B", PinType::Float);
            AddInput("T", PinType::Float);
            // Set defaults for lerp: B=1, T=0.5
            m_inputs[1].defaultFloat = 1.0f;
            m_inputs[2].defaultFloat = 0.5f;
        } else if (op != Operation::Abs && op != Operation::Sin && op != Operation::Cos) {
            AddInput("B", PinType::Float);
            // Set default B value based on operation
            if (op == Operation::Multiply || op == Operation::Divide || op == Operation::Power) {
                m_inputs[1].defaultFloat = 1.0f;
            } else {
                m_inputs[1].defaultFloat = 0.0f;
            }
        }
        AddOutput("Result", PinType::Float);
    }

    void Execute(const PCGContext& context) override {
        // Get input A (always present)
        float a = m_inputs[0].defaultFloat;

        // Get input B if present (for binary operations)
        float b = 0.0f;
        if (m_inputs.size() > 1) {
            b = m_inputs[1].defaultFloat;
        }

        // Get input C if present (for ternary operations like clamp/lerp)
        float c = 0.0f;
        if (m_inputs.size() > 2) {
            c = m_inputs[2].defaultFloat;
        }

        // Execute the operation
        switch (m_operation) {
            case Operation::Add:
                m_result = a + b;
                break;

            case Operation::Subtract:
                m_result = a - b;
                break;

            case Operation::Multiply:
                m_result = a * b;
                break;

            case Operation::Divide:
                // Protect against division by zero
                m_result = (std::abs(b) > 1e-6f) ? (a / b) : 0.0f;
                break;

            case Operation::Min:
                m_result = std::min(a, b);
                break;

            case Operation::Max:
                m_result = std::max(a, b);
                break;

            case Operation::Clamp:
                // b = min, c = max
                m_result = std::max(b, std::min(c, a));
                break;

            case Operation::Lerp:
                // a = start, b = end, c = t (interpolation factor)
                m_result = a + (b - a) * c;
                break;

            case Operation::Power:
                // Handle negative base with non-integer exponent
                if (a < 0.0f && std::floor(b) != b) {
                    m_result = 0.0f;
                } else {
                    m_result = std::pow(a, b);
                }
                break;

            case Operation::Abs:
                m_result = std::abs(a);
                break;

            case Operation::Sin:
                m_result = std::sin(a);
                break;

            case Operation::Cos:
                m_result = std::cos(a);
                break;

            default:
                m_result = 0.0f;
                break;
        }
    }

    float GetFloatOutput(int pinIndex) const override {
        return m_result;
    }

    // Setters for node parameters (used by editor)
    void SetInputA(float value) { m_inputs[0].defaultFloat = value; }
    void SetInputB(float value) { if (m_inputs.size() > 1) m_inputs[1].defaultFloat = value; }
    void SetInputC(float value) { if (m_inputs.size() > 2) m_inputs[2].defaultFloat = value; }

    Operation GetOperation() const { return m_operation; }

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
        // Disconnect all connections to/from this node before removal
        // Find any input pins on other nodes that reference this node as source
        for (auto& [id, node] : m_nodes) {
            if (id == nodeId) continue;  // Skip the node being removed

            // Check each input pin for connections to the removed node
            for (size_t i = 0; i < node->GetInputPins().size(); ++i) {
                PCGPin* pin = node->GetInputPin(static_cast<int>(i));
                if (pin && pin->isConnected && pin->connectedNodeId == nodeId) {
                    pin->connectedNodeId = -1;
                    pin->connectedPinIndex = -1;
                    pin->isConnected = false;
                }
            }
        }

        // Now remove the node itself (its own input connections are removed with it)
        m_nodes.erase(nodeId);
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
        // Get topologically sorted execution order
        std::vector<int> executionOrder = GetTopologicalOrder();

        // Execute nodes in dependency order (inputs before outputs)
        for (int nodeId : executionOrder) {
            auto* node = GetNode(nodeId);
            if (node) {
                node->Execute(context);
            }
        }
    }

    /**
     * @brief Get nodes in topological order using Kahn's algorithm
     *
     * Returns nodes sorted so that dependencies (inputs) execute before
     * the nodes that depend on them. Uses in-degree counting approach.
     */
    std::vector<int> GetTopologicalOrder() const {
        std::vector<int> order;
        std::map<int, int> inDegree;

        // Initialize in-degree for each node to 0
        for (const auto& [id, node] : m_nodes) {
            inDegree[id] = 0;
        }

        // Calculate in-degree by counting incoming connections
        // For each node, check its input pins for connections
        for (const auto& [id, node] : m_nodes) {
            for (const auto& inputPin : node->GetInputPins()) {
                if (inputPin.isConnected && inputPin.connectedNodeId >= 0) {
                    // This node has an incoming edge from connectedNodeId
                    inDegree[id]++;
                }
            }
        }

        // Queue all nodes with in-degree 0 (no dependencies)
        std::queue<int> readyQueue;
        for (const auto& [id, degree] : inDegree) {
            if (degree == 0) {
                readyQueue.push(id);
            }
        }

        // Process nodes in dependency order
        while (!readyQueue.empty()) {
            int nodeId = readyQueue.front();
            readyQueue.pop();
            order.push_back(nodeId);

            // For each node that depends on this one, reduce its in-degree
            // Find nodes whose input pins are connected to this node's outputs
            for (auto& [id, otherNode] : m_nodes) {
                if (id == nodeId) continue;

                for (const auto& inputPin : otherNode->GetInputPins()) {
                    if (inputPin.isConnected && inputPin.connectedNodeId == nodeId) {
                        inDegree[id]--;
                        if (inDegree[id] == 0) {
                            readyQueue.push(id);
                        }
                    }
                }
            }
        }

        // If order size doesn't match node count, there's a cycle
        // Return what we have (partial order) - could also throw or log error
        return order;
    }

    // Serialization
    void SaveToFile(const std::string& path);
    bool LoadFromFile(const std::string& path);

private:
    std::unordered_map<int, std::unique_ptr<PCGNode>> m_nodes;
    int m_nextNodeId = 1;
};

} // namespace PCG
