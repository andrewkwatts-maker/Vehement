#include "ProcGenNodes.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {
namespace ProcGen {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

// FBM (Fractional Brownian Motion) implementation
float FBM(const glm::vec2& pos, float frequency, int octaves, float persistence, float lacunarity,
          std::function<float(const glm::vec2&)> noiseFunc) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    glm::vec2 p = pos * frequency;

    for (int i = 0; i < octaves; i++) {
        value += noiseFunc(p) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        p *= lacunarity;
    }

    return value / maxValue;
}

// Hash function for procedural generation
uint32_t Hash(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

glm::vec2 Hash22(const glm::vec2& p) {
    uint32_t n = Hash(static_cast<uint32_t>(p.x * 127.1f + p.y * 311.7f));
    float x = static_cast<float>(n & 0xFFFF) / 65535.0f;
    float y = static_cast<float>((n >> 16) & 0xFFFF) / 65535.0f;
    return glm::vec2(x, y);
}

template<typename T>
T GetPortValue(const std::shared_ptr<VisualScript::Port>& port, T defaultValue) {
    try {
        if (port && port->IsConnected()) {
            return std::any_cast<T>(port->GetValue());
        }
        if (port) {
            return std::any_cast<T>(port->GetDefaultValue());
        }
    } catch (...) {}
    return defaultValue;
}

} // anonymous namespace

// =============================================================================
// Heightmap Data Implementation
// =============================================================================

float HeightmapData::GetBilinear(float x, float y) const {
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float fx = x - x0;
    float fy = y - y0;

    float v00 = Get(x0, y0);
    float v10 = Get(x1, y0);
    float v01 = Get(x0, y1);
    float v11 = Get(x1, y1);

    float v0 = v00 * (1.0f - fx) + v10 * fx;
    float v1 = v01 * (1.0f - fx) + v11 * fx;

    return v0 * (1.0f - fy) + v1 * fy;
}

glm::vec3 HeightmapData::GetNormal(int x, int y, float scale) const {
    float hL = Get(x - 1, y);
    float hR = Get(x + 1, y);
    float hD = Get(x, y - 1);
    float hU = Get(x, y + 1);

    glm::vec3 normal;
    normal.x = (hL - hR) * scale;
    normal.y = 2.0f;
    normal.z = (hD - hU) * scale;

    return glm::normalize(normal);
}

// =============================================================================
// Noise Node Implementations
// =============================================================================

void PerlinNoiseNode::Execute(VisualScript::ExecutionContext& context) {
    auto posPort = GetInputPort("position");
    auto freqPort = GetInputPort("frequency");
    auto octavesPort = GetInputPort("octaves");
    auto persistencePort = GetInputPort("persistence");
    auto lacunarityPort = GetInputPort("lacunarity");

    glm::vec2 position = GetPortValue(posPort, glm::vec2(0.0f));
    float frequency = GetPortValue(freqPort, 1.0f);
    int octaves = GetPortValue(octavesPort, 4);
    float persistence = GetPortValue(persistencePort, 0.5f);
    float lacunarity = GetPortValue(lacunarityPort, 2.0f);

    float value = FBM(position, frequency, octaves, persistence, lacunarity,
        [](const glm::vec2& p) {
            return glm::perlin(p) * 0.5f + 0.5f;
        });

    auto outPort = GetOutputPort("value");
    if (outPort) {
        outPort->SetValue(value);
    }
}

void SimplexNoiseNode::Execute(VisualScript::ExecutionContext& context) {
    auto posPort = GetInputPort("position");
    auto freqPort = GetInputPort("frequency");
    auto octavesPort = GetInputPort("octaves");
    auto persistencePort = GetInputPort("persistence");
    auto lacunarityPort = GetInputPort("lacunarity");

    glm::vec2 position = GetPortValue(posPort, glm::vec2(0.0f));
    float frequency = GetPortValue(freqPort, 1.0f);
    int octaves = GetPortValue(octavesPort, 4);
    float persistence = GetPortValue(persistencePort, 0.5f);
    float lacunarity = GetPortValue(lacunarityPort, 2.0f);

    float value = FBM(position, frequency, octaves, persistence, lacunarity,
        [](const glm::vec2& p) {
            return glm::simplex(p) * 0.5f + 0.5f;
        });

    auto outPort = GetOutputPort("value");
    if (outPort) {
        outPort->SetValue(value);
    }
}

void WorleyNoiseNode::Execute(VisualScript::ExecutionContext& context) {
    auto posPort = GetInputPort("position");
    auto freqPort = GetInputPort("frequency");
    auto distTypePort = GetInputPort("distanceType");

    glm::vec2 position = GetPortValue(posPort, glm::vec2(0.0f));
    float frequency = GetPortValue(freqPort, 1.0f);
    int distanceType = GetPortValue(distTypePort, 0); // 0=Euclidean, 1=Manhattan, 2=Chebyshev

    glm::vec2 p = position * frequency;
    glm::ivec2 cell = glm::ivec2(std::floor(p.x), std::floor(p.y));

    float minDist = 1000.0f;
    int closestCellId = 0;

    // Check 3x3 neighborhood
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            glm::ivec2 neighbor = cell + glm::ivec2(x, y);
            glm::vec2 cellPoint = glm::vec2(neighbor) + Hash22(glm::vec2(neighbor));
            glm::vec2 diff = cellPoint - p;

            float dist;
            if (distanceType == 0) {
                dist = glm::length(diff); // Euclidean
            } else if (distanceType == 1) {
                dist = std::abs(diff.x) + std::abs(diff.y); // Manhattan
            } else {
                dist = std::max(std::abs(diff.x), std::abs(diff.y)); // Chebyshev
            }

            if (dist < minDist) {
                minDist = dist;
                closestCellId = Hash(neighbor.x + neighbor.y * 127);
            }
        }
    }

    auto valuePort = GetOutputPort("value");
    auto cellIdPort = GetOutputPort("cellId");

    if (valuePort) valuePort->SetValue(minDist);
    if (cellIdPort) cellIdPort->SetValue(closestCellId);
}

void VoronoiNode::Execute(VisualScript::ExecutionContext& context) {
    auto posPort = GetInputPort("position");
    auto scalePort = GetInputPort("scale");
    auto randomnessPort = GetInputPort("randomness");

    glm::vec2 position = GetPortValue(posPort, glm::vec2(0.0f));
    float scale = GetPortValue(scalePort, 1.0f);
    float randomness = GetPortValue(randomnessPort, 1.0f);

    glm::vec2 p = position * scale;
    glm::ivec2 cell = glm::ivec2(std::floor(p.x), std::floor(p.y));

    float minDist = 1000.0f;
    int closestCellId = 0;
    glm::vec2 closestCenter;

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            glm::ivec2 neighbor = cell + glm::ivec2(x, y);
            glm::vec2 offset = Hash22(glm::vec2(neighbor)) * randomness;
            glm::vec2 cellPoint = glm::vec2(neighbor) + offset;
            float dist = glm::distance(cellPoint, p);

            if (dist < minDist) {
                minDist = dist;
                closestCellId = Hash(neighbor.x + neighbor.y * 127);
                closestCenter = cellPoint;
            }
        }
    }

    auto valuePort = GetOutputPort("value");
    auto cellIdPort = GetOutputPort("cellId");
    auto centerPort = GetOutputPort("cellCenter");

    if (valuePort) valuePort->SetValue(minDist);
    if (cellIdPort) cellIdPort->SetValue(closestCellId);
    if (centerPort) centerPort->SetValue(closestCenter);
}

// =============================================================================
// Erosion Node Implementations
// =============================================================================

void HydraulicErosionNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        int iterations = GetPortValue(GetInputPort("iterations"), 1000);
        float rainAmount = GetPortValue(GetInputPort("rainAmount"), 0.01f);
        float evaporation = GetPortValue(GetInputPort("evaporation"), 0.01f);
        float sedimentCapacity = GetPortValue(GetInputPort("sedimentCapacity"), 4.0f);
        float erosionStrength = GetPortValue(GetInputPort("erosionStrength"), 0.3f);
        float depositionStrength = GetPortValue(GetInputPort("depositionStrength"), 0.3f);

        auto erodedMap = std::make_shared<HeightmapData>(*heightmap);
        auto sedimentMap = std::make_shared<HeightmapData>(heightmap->GetWidth(), heightmap->GetHeight());

        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int iter = 0; iter < iterations; iter++) {
            // Random starting position
            int x = static_cast<int>(dist(rng) * heightmap->GetWidth());
            int y = static_cast<int>(dist(rng) * heightmap->GetHeight());

            float water = rainAmount;
            float sediment = 0.0f;
            float velocity = 1.0f;

            for (int step = 0; step < 30; step++) {
                float height = erodedMap->Get(x, y);

                // Find steepest descent
                float minHeight = height;
                int nextX = x, nextY = y;

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        float nh = erodedMap->Get(x + dx, y + dy);
                        if (nh < minHeight) {
                            minHeight = nh;
                            nextX = x + dx;
                            nextY = y + dy;
                        }
                    }
                }

                float heightDiff = height - minHeight;
                if (heightDiff <= 0.0f) break; // Flat area

                // Update sediment capacity
                float capacity = std::max(heightDiff, 0.01f) * velocity * water * sedimentCapacity;

                if (sediment > capacity) {
                    // Deposit sediment
                    float deposit = (sediment - capacity) * depositionStrength;
                    erodedMap->Set(x, y, height + deposit);
                    sediment -= deposit;
                } else {
                    // Erode terrain
                    float erosion = std::min((capacity - sediment) * erosionStrength, heightDiff);
                    erodedMap->Set(x, y, height - erosion);
                    sediment += erosion;
                }

                water *= (1.0f - evaporation);
                velocity = std::sqrt(velocity * velocity + heightDiff * 9.8f); // Simple physics

                x = nextX;
                y = nextY;
            }

            sedimentMap->Set(x, y, sedimentMap->Get(x, y) + sediment);
        }

        auto outHeightmapPort = GetOutputPort("erodedHeightmap");
        auto outSedimentPort = GetOutputPort("sedimentMap");

        if (outHeightmapPort) outHeightmapPort->SetValue(erodedMap);
        if (outSedimentPort) outSedimentPort->SetValue(sedimentMap);

    } catch (...) {}
}

void ThermalErosionNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        int iterations = GetPortValue(GetInputPort("iterations"), 100);
        float talusAngle = GetPortValue(GetInputPort("talusAngle"), 0.7f); // radians
        float strength = GetPortValue(GetInputPort("strength"), 0.5f);

        auto erodedMap = std::make_shared<HeightmapData>(*heightmap);
        float threshold = std::tan(talusAngle);

        for (int iter = 0; iter < iterations; iter++) {
            auto tempMap = std::make_shared<HeightmapData>(*erodedMap);

            for (int y = 1; y < heightmap->GetHeight() - 1; y++) {
                for (int x = 1; x < heightmap->GetWidth() - 1; x++) {
                    float height = erodedMap->Get(x, y);
                    float maxDiff = 0.0f;

                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            float neighborHeight = erodedMap->Get(x + dx, y + dy);
                            float diff = height - neighborHeight;
                            if (diff > threshold) {
                                float transfer = strength * (diff - threshold) * 0.5f;
                                tempMap->Set(x, y, tempMap->Get(x, y) - transfer);
                                tempMap->Set(x + dx, y + dy, tempMap->Get(x + dx, y + dy) + transfer);
                            }
                        }
                    }
                }
            }

            erodedMap = tempMap;
        }

        auto outPort = GetOutputPort("erodedHeightmap");
        if (outPort) outPort->SetValue(erodedMap);

    } catch (...) {}
}

// =============================================================================
// Terrain Shaping Node Implementations
// =============================================================================

void TerraceNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        int steps = GetPortValue(GetInputPort("steps"), 5);
        float smoothness = GetPortValue(GetInputPort("smoothness"), 0.1f);

        auto terracedMap = std::make_shared<HeightmapData>(*heightmap);

        for (int y = 0; y < heightmap->GetHeight(); y++) {
            for (int x = 0; x < heightmap->GetWidth(); x++) {
                float h = heightmap->Get(x, y);
                float stepped = std::floor(h * steps) / steps;
                float smooth = glm::mix(h, stepped, 1.0f - smoothness);
                terracedMap->Set(x, y, smooth);
            }
        }

        auto outPort = GetOutputPort("terracedHeightmap");
        if (outPort) outPort->SetValue(terracedMap);

    } catch (...) {}
}

void RidgeNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        float sharpness = GetPortValue(GetInputPort("sharpness"), 1.0f);
        float offset = GetPortValue(GetInputPort("offset"), 0.5f);

        auto ridgedMap = std::make_shared<HeightmapData>(*heightmap);

        for (int y = 0; y < heightmap->GetHeight(); y++) {
            for (int x = 0; x < heightmap->GetWidth(); x++) {
                float h = heightmap->Get(x, y);
                float ridged = 1.0f - std::abs(h - offset) * sharpness;
                ridgedMap->Set(x, y, std::max(0.0f, std::min(1.0f, ridged)));
            }
        }

        auto outPort = GetOutputPort("ridgedHeightmap");
        if (outPort) outPort->SetValue(ridgedMap);

    } catch (...) {}
}

void SlopeNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        float scale = GetPortValue(GetInputPort("scale"), 1.0f);

        auto slopeMap = std::make_shared<HeightmapData>(heightmap->GetWidth(), heightmap->GetHeight());

        for (int y = 0; y < heightmap->GetHeight(); y++) {
            for (int x = 0; x < heightmap->GetWidth(); x++) {
                glm::vec3 normal = heightmap->GetNormal(x, y, scale);
                float slope = 1.0f - normal.y; // 0 = flat, 1 = vertical
                slopeMap->Set(x, y, slope);
            }
        }

        auto outSlopePort = GetOutputPort("slopeMap");
        if (outSlopePort) outSlopePort->SetValue(slopeMap);

    } catch (...) {}
}

// =============================================================================
// Resource/Structure Placement Implementations (Stubs for now)
// =============================================================================

void ResourcePlacementNode::Execute(VisualScript::ExecutionContext& context) {
    // TODO: Implement resource placement logic
}

void VegetationPlacementNode::Execute(VisualScript::ExecutionContext& context) {
    // TODO: Implement vegetation placement logic
}

void WaterPlacementNode::Execute(VisualScript::ExecutionContext& context) {
    // TODO: Implement water placement logic
}

void RuinsPlacementNode::Execute(VisualScript::ExecutionContext& context) {
    // TODO: Implement ruins placement logic
}

void AncientStructuresNode::Execute(VisualScript::ExecutionContext& context) {
    // TODO: Implement ancient structures placement logic
}

void BuildingPlacementNode::Execute(VisualScript::ExecutionContext& context) {
    // TODO: Implement building placement logic
}

// =============================================================================
// Biome and Climate Node Implementations (Stubs for now)
// =============================================================================

void BiomeNode::Execute(VisualScript::ExecutionContext& context) {
    // TODO: Implement biome calculation
}

void ClimateNode::Execute(VisualScript::ExecutionContext& context) {
    // TODO: Implement climate simulation
}

// =============================================================================
// Utility Node Implementations
// =============================================================================

void BlendNode::Execute(VisualScript::ExecutionContext& context) {
    auto inputAPort = GetInputPort("inputA");
    auto inputBPort = GetInputPort("inputB");
    if (!inputAPort || !inputBPort) return;

    try {
        auto heightmapA = std::any_cast<std::shared_ptr<HeightmapData>>(inputAPort->GetValue());
        auto heightmapB = std::any_cast<std::shared_ptr<HeightmapData>>(inputBPort->GetValue());
        if (!heightmapA || !heightmapB) return;

        float blend = GetPortValue(GetInputPort("blend"), 0.5f);
        std::string blendMode = GetPortValue(GetInputPort("blendMode"), std::string("lerp"));

        auto result = std::make_shared<HeightmapData>(heightmapA->GetWidth(), heightmapA->GetHeight());

        for (int y = 0; y < result->GetHeight(); y++) {
            for (int x = 0; x < result->GetWidth(); x++) {
                float a = heightmapA->Get(x, y);
                float b = heightmapB->Get(x, y);
                float value;

                if (blendMode == "add") {
                    value = a + b * blend;
                } else if (blendMode == "multiply") {
                    value = a * glm::mix(1.0f, b, blend);
                } else if (blendMode == "min") {
                    value = std::min(a, b);
                } else if (blendMode == "max") {
                    value = std::max(a, b);
                } else { // lerp
                    value = glm::mix(a, b, blend);
                }

                result->Set(x, y, value);
            }
        }

        auto outPort = GetOutputPort("result");
        if (outPort) outPort->SetValue(result);

    } catch (...) {}
}

void RemapNode::Execute(VisualScript::ExecutionContext& context) {
    auto inputPort = GetInputPort("input");
    if (!inputPort) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(inputPort->GetValue());
        if (!heightmap) return;

        float inputMin = GetPortValue(GetInputPort("inputMin"), 0.0f);
        float inputMax = GetPortValue(GetInputPort("inputMax"), 1.0f);
        float outputMin = GetPortValue(GetInputPort("outputMin"), 0.0f);
        float outputMax = GetPortValue(GetInputPort("outputMax"), 1.0f);

        auto result = std::make_shared<HeightmapData>(heightmap->GetWidth(), heightmap->GetHeight());

        for (int y = 0; y < result->GetHeight(); y++) {
            for (int x = 0; x < result->GetWidth(); x++) {
                float value = heightmap->Get(x, y);
                float normalized = (value - inputMin) / (inputMax - inputMin);
                float remapped = outputMin + normalized * (outputMax - outputMin);
                result->Set(x, y, remapped);
            }
        }

        auto outPort = GetOutputPort("result");
        if (outPort) outPort->SetValue(result);

    } catch (...) {}
}

void CurveNode::Execute(VisualScript::ExecutionContext& context) {
    auto inputPort = GetInputPort("input");
    if (!inputPort) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(inputPort->GetValue());
        if (!heightmap) return;

        std::string curveType = GetPortValue(GetInputPort("curveType"), std::string("smoothstep"));
        float strength = GetPortValue(GetInputPort("strength"), 1.0f);

        auto result = std::make_shared<HeightmapData>(heightmap->GetWidth(), heightmap->GetHeight());

        for (int y = 0; y < result->GetHeight(); y++) {
            for (int x = 0; x < result->GetWidth(); x++) {
                float value = heightmap->Get(x, y);
                float curved;

                if (curveType == "smoothstep") {
                    curved = glm::smoothstep(0.0f, 1.0f, value);
                } else if (curveType == "pow2") {
                    curved = value * value;
                } else if (curveType == "pow3") {
                    curved = value * value * value;
                } else if (curveType == "sqrt") {
                    curved = std::sqrt(value);
                } else {
                    curved = value;
                }

                result->Set(x, y, glm::mix(value, curved, strength));
            }
        }

        auto outPort = GetOutputPort("result");
        if (outPort) outPort->SetValue(result);

    } catch (...) {}
}

void ClampNode::Execute(VisualScript::ExecutionContext& context) {
    auto inputPort = GetInputPort("input");
    if (!inputPort) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(inputPort->GetValue());
        if (!heightmap) return;

        float minVal = GetPortValue(GetInputPort("min"), 0.0f);
        float maxVal = GetPortValue(GetInputPort("max"), 1.0f);

        auto result = std::make_shared<HeightmapData>(heightmap->GetWidth(), heightmap->GetHeight());

        for (int y = 0; y < result->GetHeight(); y++) {
            for (int x = 0; x < result->GetWidth(); x++) {
                float value = heightmap->Get(x, y);
                result->Set(x, y, glm::clamp(value, minVal, maxVal));
            }
        }

        auto outPort = GetOutputPort("result");
        if (outPort) outPort->SetValue(result);

    } catch (...) {}
}

} // namespace ProcGen
} // namespace Nova
