#include "ProcGenNodes.hpp"
#include <algorithm>
#include <cmath>
#include <execution>
#include <numeric>
#include <future>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace Nova {
namespace ProcGen {

// =============================================================================
// SOLID Principle Implementation
// =============================================================================
//
// This file implements procedural generation nodes following SOLID principles:
//
// S - Single Responsibility: Each node class handles one specific operation
// O - Open/Closed: New node types can be added without modifying existing code
// L - Liskov Substitution: All nodes derive from VisualScript::Node interface
// I - Interface Segregation: Nodes only implement required Execute() method
// D - Dependency Inversion: Nodes depend on abstract Port interface, not concrete types
//
// =============================================================================

// =============================================================================
// IProcGenNode Interface Extension
// =============================================================================

/**
 * @brief Extended interface for procedural generation nodes
 *
 * Provides additional capabilities beyond the base Node interface:
 * - Input/output metadata for graph validation
 * - Async execution support
 * - Serialization hooks
 */
class IProcGenNodeExtension {
public:
    virtual ~IProcGenNodeExtension() = default;

    /**
     * @brief Get list of required input port names
     */
    virtual std::vector<std::string> GetRequiredInputs() const { return {}; }

    /**
     * @brief Get list of output port names this node produces
     */
    virtual std::vector<std::string> GetProducedOutputs() const { return {}; }

    /**
     * @brief Whether this node supports parallel execution
     */
    virtual bool SupportsParallelExecution() const { return false; }

    /**
     * @brief Estimated execution cost (for scheduling)
     */
    virtual float GetExecutionCost() const { return 1.0f; }

    /**
     * @brief Custom serialization hook
     */
    virtual nlohmann::json SerializeCustomData() const { return {}; }

    /**
     * @brief Custom deserialization hook
     */
    virtual void DeserializeCustomData(const nlohmann::json& json) {}
};

// =============================================================================
// Node Executor - Handles Graph Evaluation with Async/Parallel Support
// =============================================================================

/**
 * @brief Thread pool for parallel node execution
 */
class NodeExecutorThreadPool {
public:
    static NodeExecutorThreadPool& Instance() {
        static NodeExecutorThreadPool instance;
        return instance;
    }

    void Initialize(size_t numThreads = 0) {
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
            if (numThreads == 0) numThreads = 4;
        }

        m_running = true;
        m_workers.reserve(numThreads);

        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_condition.wait(lock, [this] {
                            return !m_running || !m_tasks.empty();
                        });

                        if (!m_running && m_tasks.empty()) {
                            return;
                        }

                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    task();
                    --m_pendingTasks;
                }
            });
        }
    }

    void Shutdown() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_running = false;
        }
        m_condition.notify_all();

        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        m_workers.clear();
    }

    template<typename F>
    std::future<void> Submit(F&& task) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            ++m_pendingTasks;
            m_tasks.emplace([task = std::forward<F>(task), promise]() mutable {
                try {
                    task();
                    promise->set_value();
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        m_condition.notify_one();

        return future;
    }

    void WaitForAll() {
        while (m_pendingTasks > 0) {
            std::this_thread::yield();
        }
    }

    ~NodeExecutorThreadPool() {
        Shutdown();
    }

private:
    NodeExecutorThreadPool() = default;

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_running{false};
    std::atomic<size_t> m_pendingTasks{0};
};

/**
 * @brief Graph executor with topological sorting and parallel execution
 */
class NodeGraphExecutor {
public:
    struct ExecutionStats {
        size_t nodesExecuted = 0;
        size_t nodesParallel = 0;
        float totalTimeMs = 0.0f;
        std::unordered_map<std::string, float> nodeTimesMs;
    };

    /**
     * @brief Execute a graph with optional parallelization
     */
    static ExecutionStats Execute(VisualScript::Graph* graph,
                                  VisualScript::ExecutionContext& context,
                                  bool enableParallel = true) {
        ExecutionStats stats;
        auto startTime = std::chrono::high_resolution_clock::now();

        // Get topological order of nodes
        auto sortedNodes = TopologicalSort(graph);

        // Group nodes into execution levels (nodes in same level can run in parallel)
        auto levels = GroupIntoLevels(sortedNodes, graph);

        // Execute each level
        for (const auto& level : levels) {
            if (enableParallel && level.size() > 1) {
                // Execute level in parallel
                std::vector<std::future<void>> futures;
                futures.reserve(level.size());

                for (auto* node : level) {
                    futures.push_back(NodeExecutorThreadPool::Instance().Submit([node, &context, &stats]() {
                        auto nodeStart = std::chrono::high_resolution_clock::now();
                        node->Execute(context);
                        auto nodeEnd = std::chrono::high_resolution_clock::now();

                        float nodeTime = std::chrono::duration<float, std::milli>(nodeEnd - nodeStart).count();
                        stats.nodeTimesMs[node->GetId()] = nodeTime;
                    }));
                    ++stats.nodesParallel;
                }

                // Wait for all nodes in level to complete
                for (auto& future : futures) {
                    future.wait();
                }
            } else {
                // Execute sequentially
                for (auto* node : level) {
                    auto nodeStart = std::chrono::high_resolution_clock::now();
                    node->Execute(context);
                    auto nodeEnd = std::chrono::high_resolution_clock::now();

                    float nodeTime = std::chrono::duration<float, std::milli>(nodeEnd - nodeStart).count();
                    stats.nodeTimesMs[node->GetId()] = nodeTime;
                }
            }
            stats.nodesExecuted += level.size();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        stats.totalTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        return stats;
    }

private:
    static std::vector<VisualScript::Node*> TopologicalSort(VisualScript::Graph* graph) {
        std::vector<VisualScript::Node*> result;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> visiting;

        std::function<void(VisualScript::Node*)> visit = [&](VisualScript::Node* node) {
            if (visited.count(node->GetId())) return;
            if (visiting.count(node->GetId())) return; // Cycle detected

            visiting.insert(node->GetId());

            // Visit dependencies (nodes connected to input ports)
            for (const auto& port : node->GetInputPorts()) {
                for (const auto& conn : port->GetConnections()) {
                    if (auto* sourceNode = conn->GetSource()->GetOwner()) {
                        visit(sourceNode);
                    }
                }
            }

            visiting.erase(node->GetId());
            visited.insert(node->GetId());
            result.push_back(node);
        };

        for (const auto& node : graph->GetNodes()) {
            visit(node.get());
        }

        return result;
    }

    static std::vector<std::vector<VisualScript::Node*>> GroupIntoLevels(
            const std::vector<VisualScript::Node*>& sortedNodes,
            VisualScript::Graph* graph) {

        std::vector<std::vector<VisualScript::Node*>> levels;
        std::unordered_map<std::string, int> nodeLevel;

        for (auto* node : sortedNodes) {
            int level = 0;

            // Find max level of dependencies
            for (const auto& port : node->GetInputPorts()) {
                for (const auto& conn : port->GetConnections()) {
                    if (auto* sourceNode = conn->GetSource()->GetOwner()) {
                        auto it = nodeLevel.find(sourceNode->GetId());
                        if (it != nodeLevel.end()) {
                            level = std::max(level, it->second + 1);
                        }
                    }
                }
            }

            nodeLevel[node->GetId()] = level;

            // Ensure we have enough levels
            while (levels.size() <= static_cast<size_t>(level)) {
                levels.push_back({});
            }
            levels[level].push_back(node);
        }

        return levels;
    }
};

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

// 3D FBM for volumetric noise
float FBM3D(const glm::vec3& pos, float frequency, int octaves, float persistence, float lacunarity,
            std::function<float(const glm::vec3&)> noiseFunc) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    glm::vec3 p = pos * frequency;

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

uint32_t Hash2(uint32_t x, uint32_t y) {
    return Hash(x ^ (y * 0x45d9f3b));
}

uint32_t Hash3(uint32_t x, uint32_t y, uint32_t z) {
    return Hash(x ^ Hash2(y, z));
}

glm::vec2 Hash22(const glm::vec2& p) {
    uint32_t n = Hash(static_cast<uint32_t>(p.x * 127.1f + p.y * 311.7f));
    float x = static_cast<float>(n & 0xFFFF) / 65535.0f;
    float y = static_cast<float>((n >> 16) & 0xFFFF) / 65535.0f;
    return glm::vec2(x, y);
}

glm::vec3 Hash33(const glm::vec3& p) {
    uint32_t n = Hash(static_cast<uint32_t>(p.x * 127.1f + p.y * 311.7f + p.z * 74.7f));
    float x = static_cast<float>(n & 0x3FF) / 1023.0f;
    float y = static_cast<float>((n >> 10) & 0x3FF) / 1023.0f;
    float z = static_cast<float>((n >> 20) & 0x3FF) / 1023.0f;
    return glm::vec3(x, y, z);
}

// Type-safe port value extraction
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

// Poisson disk sampling for natural placement distributions
std::vector<glm::vec2> PoissonDiskSampling(int width, int height, float minDist, int maxAttempts, std::mt19937& rng) {
    std::vector<glm::vec2> points;
    std::vector<glm::vec2> active;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float cellSize = minDist / std::sqrt(2.0f);
    int gridWidth = static_cast<int>(std::ceil(width / cellSize));
    int gridHeight = static_cast<int>(std::ceil(height / cellSize));
    std::vector<int> grid(gridWidth * gridHeight, -1);

    auto toGrid = [cellSize](const glm::vec2& p) -> glm::ivec2 {
        return glm::ivec2(static_cast<int>(p.x / cellSize), static_cast<int>(p.y / cellSize));
    };

    auto isValid = [&](const glm::vec2& candidate) -> bool {
        if (candidate.x < 0 || candidate.x >= width || candidate.y < 0 || candidate.y >= height) {
            return false;
        }

        glm::ivec2 cell = toGrid(candidate);
        int searchRadius = 2;

        for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
            for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
                int nx = cell.x + dx;
                int ny = cell.y + dy;
                if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
                    int idx = grid[ny * gridWidth + nx];
                    if (idx >= 0) {
                        float d = glm::distance(candidate, points[idx]);
                        if (d < minDist) return false;
                    }
                }
            }
        }
        return true;
    };

    // Initial point
    glm::vec2 initial(dist(rng) * width, dist(rng) * height);
    points.push_back(initial);
    active.push_back(initial);
    glm::ivec2 initCell = toGrid(initial);
    grid[initCell.y * gridWidth + initCell.x] = 0;

    while (!active.empty()) {
        int randIdx = static_cast<int>(dist(rng) * active.size());
        glm::vec2 point = active[randIdx];
        bool found = false;

        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            float angle = dist(rng) * 2.0f * 3.14159265359f;
            float radius = minDist + dist(rng) * minDist;
            glm::vec2 candidate = point + glm::vec2(std::cos(angle), std::sin(angle)) * radius;

            if (isValid(candidate)) {
                points.push_back(candidate);
                active.push_back(candidate);
                glm::ivec2 cell = toGrid(candidate);
                grid[cell.y * gridWidth + cell.x] = static_cast<int>(points.size()) - 1;
                found = true;
                break;
            }
        }

        if (!found) {
            active.erase(active.begin() + randIdx);
        }
    }

    return points;
}

// SDF primitives for geometry generation
float SDFSphere(const glm::vec3& p, float radius) {
    return glm::length(p) - radius;
}

float SDFBox(const glm::vec3& p, const glm::vec3& halfExtents) {
    glm::vec3 q = glm::abs(p) - halfExtents;
    return glm::length(glm::max(q, glm::vec3(0.0f))) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
}

float SDFCylinder(const glm::vec3& p, float radius, float height) {
    glm::vec2 d = glm::abs(glm::vec2(glm::length(glm::vec2(p.x, p.z)), p.y)) - glm::vec2(radius, height);
    return std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, glm::vec2(0.0f)));
}

float SDFCone(const glm::vec3& p, float angle, float height) {
    glm::vec2 c = glm::vec2(std::sin(angle), std::cos(angle));
    glm::vec2 q = glm::vec2(glm::length(glm::vec2(p.x, p.z)), p.y);
    float d1 = -q.y - height;
    float d2 = std::max(glm::dot(q, c), q.y);
    return glm::length(glm::max(glm::vec2(d1, d2), glm::vec2(0.0f))) + std::min(std::max(d1, d2), 0.0f);
}

float SDFTorus(const glm::vec3& p, float majorRadius, float minorRadius) {
    glm::vec2 q = glm::vec2(glm::length(glm::vec2(p.x, p.z)) - majorRadius, p.y);
    return glm::length(q) - minorRadius;
}

// CSG operations
float SDFUnion(float d1, float d2) { return std::min(d1, d2); }
float SDFIntersection(float d1, float d2) { return std::max(d1, d2); }
float SDFDifference(float d1, float d2) { return std::max(d1, -d2); }

float SDFSmoothUnion(float d1, float d2, float k) {
    float h = std::max(k - std::abs(d1 - d2), 0.0f) / k;
    return std::min(d1, d2) - h * h * k * 0.25f;
}

float SDFSmoothIntersection(float d1, float d2, float k) {
    float h = std::max(k - std::abs(d1 - d2), 0.0f) / k;
    return std::max(d1, d2) + h * h * k * 0.25f;
}

float SDFSmoothDifference(float d1, float d2, float k) {
    float h = std::max(k - std::abs(-d1 - d2), 0.0f) / k;
    return std::max(d1, -d2) + h * h * k * 0.25f;
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
    float secondMinDist = 1000.0f;
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
                secondMinDist = minDist;
                minDist = dist;
                closestCellId = Hash(neighbor.x + neighbor.y * 127);
            } else if (dist < secondMinDist) {
                secondMinDist = dist;
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

        const int maxSteps = 64;
        const float inertia = 0.05f;
        const float gravity = 4.0f;
        const float minSlope = 0.01f;
        const int erosionRadius = 3;

        for (int iter = 0; iter < iterations; iter++) {
            // Random starting position
            float posX = dist(rng) * (heightmap->GetWidth() - 1);
            float posY = dist(rng) * (heightmap->GetHeight() - 1);
            float dirX = 0.0f, dirY = 0.0f;
            float water = rainAmount;
            float sediment = 0.0f;
            float velocity = 1.0f;

            for (int step = 0; step < maxSteps; step++) {
                int cellX = static_cast<int>(posX);
                int cellY = static_cast<int>(posY);

                if (cellX < 0 || cellX >= heightmap->GetWidth() - 1 ||
                    cellY < 0 || cellY >= heightmap->GetHeight() - 1) break;

                // Calculate droplet offset within cell
                float cellOffsetX = posX - cellX;
                float cellOffsetY = posY - cellY;

                // Calculate height and gradient using bilinear interpolation
                float heightNW = erodedMap->Get(cellX, cellY);
                float heightNE = erodedMap->Get(cellX + 1, cellY);
                float heightSW = erodedMap->Get(cellX, cellY + 1);
                float heightSE = erodedMap->Get(cellX + 1, cellY + 1);

                float gradientX = (heightNE - heightNW) * (1 - cellOffsetY) + (heightSE - heightSW) * cellOffsetY;
                float gradientY = (heightSW - heightNW) * (1 - cellOffsetX) + (heightSE - heightNE) * cellOffsetX;

                // Update direction with inertia
                dirX = dirX * inertia - gradientX * (1 - inertia);
                dirY = dirY * inertia - gradientY * (1 - inertia);

                // Normalize direction
                float len = std::sqrt(dirX * dirX + dirY * dirY);
                if (len > 0.0001f) {
                    dirX /= len;
                    dirY /= len;
                }

                // Update position
                float newPosX = posX + dirX;
                float newPosY = posY + dirY;

                // Stop if moving outside bounds
                if (newPosX < 0 || newPosX >= heightmap->GetWidth() - 1 ||
                    newPosY < 0 || newPosY >= heightmap->GetHeight() - 1) break;

                // Calculate height difference
                float newHeight = erodedMap->GetBilinear(newPosX, newPosY);
                float currentHeight = heightNW * (1 - cellOffsetX) * (1 - cellOffsetY) +
                                      heightNE * cellOffsetX * (1 - cellOffsetY) +
                                      heightSW * (1 - cellOffsetX) * cellOffsetY +
                                      heightSE * cellOffsetX * cellOffsetY;
                float heightDiff = currentHeight - newHeight;

                // Calculate sediment capacity
                float capacity = std::max(heightDiff, minSlope) * velocity * water * sedimentCapacity;

                if (sediment > capacity || heightDiff < 0) {
                    // Deposit sediment
                    float depositAmount = (heightDiff < 0) ?
                        std::min(sediment, -heightDiff) :
                        (sediment - capacity) * depositionStrength;

                    sediment -= depositAmount;

                    // Distribute deposit to surrounding cells
                    erodedMap->Set(cellX, cellY, erodedMap->Get(cellX, cellY) + depositAmount * (1 - cellOffsetX) * (1 - cellOffsetY));
                    erodedMap->Set(cellX + 1, cellY, erodedMap->Get(cellX + 1, cellY) + depositAmount * cellOffsetX * (1 - cellOffsetY));
                    erodedMap->Set(cellX, cellY + 1, erodedMap->Get(cellX, cellY + 1) + depositAmount * (1 - cellOffsetX) * cellOffsetY);
                    erodedMap->Set(cellX + 1, cellY + 1, erodedMap->Get(cellX + 1, cellY + 1) + depositAmount * cellOffsetX * cellOffsetY);
                } else {
                    // Erode terrain with brush
                    float erosionAmount = std::min((capacity - sediment) * erosionStrength, -heightDiff);

                    for (int ey = -erosionRadius; ey <= erosionRadius; ey++) {
                        for (int ex = -erosionRadius; ex <= erosionRadius; ex++) {
                            int erodeX = cellX + ex;
                            int erodeY = cellY + ey;

                            if (erodeX >= 0 && erodeX < heightmap->GetWidth() &&
                                erodeY >= 0 && erodeY < heightmap->GetHeight()) {
                                float weight = std::max(0.0f, 1.0f - std::sqrt(ex*ex + ey*ey) / erosionRadius);
                                float erode = erosionAmount * weight * 0.5f;
                                erodedMap->Set(erodeX, erodeY, erodedMap->Get(erodeX, erodeY) - erode);
                                sediment += erode;
                            }
                        }
                    }
                }

                // Update velocity and water
                velocity = std::sqrt(velocity * velocity + std::abs(heightDiff) * gravity);
                water *= (1.0f - evaporation);

                posX = newPosX;
                posY = newPosY;

                if (water < 0.001f) break;
            }

            // Track final sediment deposition
            int finalX = static_cast<int>(posX);
            int finalY = static_cast<int>(posY);
            if (finalX >= 0 && finalX < heightmap->GetWidth() &&
                finalY >= 0 && finalY < heightmap->GetHeight()) {
                sedimentMap->Set(finalX, finalY, sedimentMap->Get(finalX, finalY) + sediment);
            }
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

        // Direction offsets for 8-connectivity
        const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        const float dw[] = {0.707f, 1.0f, 0.707f, 1.0f, 1.0f, 0.707f, 1.0f, 0.707f}; // Distance weights

        for (int iter = 0; iter < iterations; iter++) {
            auto tempMap = std::make_shared<HeightmapData>(*erodedMap);

            for (int y = 1; y < heightmap->GetHeight() - 1; y++) {
                for (int x = 1; x < heightmap->GetWidth() - 1; x++) {
                    float height = erodedMap->Get(x, y);

                    // Calculate material to redistribute
                    float totalTransfer = 0.0f;
                    float transfers[8] = {0};

                    for (int i = 0; i < 8; i++) {
                        float neighborHeight = erodedMap->Get(x + dx[i], y + dy[i]);
                        float diff = (height - neighborHeight) / dw[i];

                        if (diff > threshold) {
                            transfers[i] = (diff - threshold) * strength;
                            totalTransfer += transfers[i];
                        }
                    }

                    // Apply transfers
                    if (totalTransfer > 0.0f) {
                        float available = std::min(height * 0.5f, totalTransfer);
                        float scale = available / totalTransfer;

                        tempMap->Set(x, y, tempMap->Get(x, y) - available);

                        for (int i = 0; i < 8; i++) {
                            if (transfers[i] > 0) {
                                float transfer = transfers[i] * scale;
                                tempMap->Set(x + dx[i], y + dy[i],
                                    tempMap->Get(x + dx[i], y + dy[i]) + transfer);
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

                // Smooth blend between stepped and original
                float blend = 1.0f - smoothness;
                float smooth = h * smoothness + stepped * blend;
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

                // Ridge function: creates sharp peaks at the offset value
                float ridged = 1.0f - std::abs(h - offset) * sharpness * 2.0f;
                ridged = std::pow(std::max(0.0f, ridged), 2.0f); // Sharpen peaks

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
        auto normalData = std::make_shared<std::vector<glm::vec3>>();
        normalData->reserve(heightmap->GetWidth() * heightmap->GetHeight());

        for (int y = 0; y < heightmap->GetHeight(); y++) {
            for (int x = 0; x < heightmap->GetWidth(); x++) {
                glm::vec3 normal = heightmap->GetNormal(x, y, scale);
                float slope = 1.0f - normal.y; // 0 = flat, 1 = vertical
                slopeMap->Set(x, y, slope);
                normalData->push_back(normal);
            }
        }

        auto outSlopePort = GetOutputPort("slopeMap");
        auto outNormalPort = GetOutputPort("normalMap");

        if (outSlopePort) outSlopePort->SetValue(slopeMap);
        if (outNormalPort) outNormalPort->SetValue(normalData);

    } catch (...) {}
}

// =============================================================================
// Resource/Structure Placement Implementations
// =============================================================================

/**
 * @brief Resource placement data structure
 */
struct ResourcePlacement {
    glm::vec2 position;
    std::string resourceType;
    float amount;
    int clusterId;
};

void ResourcePlacementNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        std::string resourceType = GetPortValue(GetInputPort("resourceType"), std::string("ore"));
        float density = GetPortValue(GetInputPort("density"), 0.1f);
        float minHeight = GetPortValue(GetInputPort("minHeight"), 0.2f);
        float maxHeight = GetPortValue(GetInputPort("maxHeight"), 0.8f);
        float minSlope = GetPortValue(GetInputPort("minSlope"), 0.0f);
        float maxSlope = GetPortValue(GetInputPort("maxSlope"), 0.5f);
        float clusterSize = GetPortValue(GetInputPort("clusterSize"), 10.0f);

        auto resources = std::make_shared<std::vector<ResourcePlacement>>();

        // Generate cluster centers using Poisson disk sampling
        std::mt19937 rng(42);
        auto clusterCenters = PoissonDiskSampling(
            heightmap->GetWidth(), heightmap->GetHeight(),
            clusterSize * 2.0f, 30, rng);

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        int clusterId = 0;

        for (const auto& center : clusterCenters) {
            float centerHeight = heightmap->GetBilinear(center.x, center.y);

            // Check if cluster center is in valid height range
            if (centerHeight < minHeight || centerHeight > maxHeight) continue;

            // Generate resources within cluster
            int numResources = static_cast<int>(clusterSize * clusterSize * density);

            for (int i = 0; i < numResources; i++) {
                float angle = dist(rng) * 2.0f * 3.14159265359f;
                float radius = dist(rng) * clusterSize;

                glm::vec2 pos = center + glm::vec2(std::cos(angle), std::sin(angle)) * radius;

                // Check bounds
                if (pos.x < 0 || pos.x >= heightmap->GetWidth() - 1 ||
                    pos.y < 0 || pos.y >= heightmap->GetHeight() - 1) continue;

                float height = heightmap->GetBilinear(pos.x, pos.y);

                // Check height constraints
                if (height < minHeight || height > maxHeight) continue;

                // Check slope constraints
                int ix = static_cast<int>(pos.x);
                int iy = static_cast<int>(pos.y);
                glm::vec3 normal = heightmap->GetNormal(ix, iy, 1.0f);
                float slope = 1.0f - normal.y;

                if (slope < minSlope || slope > maxSlope) continue;

                // Add resource
                ResourcePlacement resource;
                resource.position = pos;
                resource.resourceType = resourceType;
                resource.amount = 50.0f + dist(rng) * 100.0f; // 50-150 units
                resource.clusterId = clusterId;
                resources->push_back(resource);
            }

            clusterId++;
        }

        auto outPort = GetOutputPort("resourceMap");
        if (outPort) outPort->SetValue(resources);

    } catch (...) {}
}

/**
 * @brief Vegetation placement data structure
 */
struct VegetationPlacement {
    glm::vec2 position;
    std::string vegetationType;
    float scale;
    float rotation;
    int biomeId;
};

void VegetationPlacementNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        std::string vegetationType = GetPortValue(GetInputPort("vegetationType"), std::string("tree"));
        float density = GetPortValue(GetInputPort("density"), 0.5f);
        float minSlope = GetPortValue(GetInputPort("minSlope"), 0.0f);
        float maxSlope = GetPortValue(GetInputPort("maxSlope"), 0.3f);

        auto vegetation = std::make_shared<std::vector<VegetationPlacement>>();

        // Calculate minimum distance based on vegetation type
        float minDist = 3.0f;
        if (vegetationType == "tree") minDist = 5.0f;
        else if (vegetationType == "bush") minDist = 2.0f;
        else if (vegetationType == "grass") minDist = 0.5f;

        minDist /= std::sqrt(density);

        std::mt19937 rng(12345);
        auto positions = PoissonDiskSampling(
            heightmap->GetWidth(), heightmap->GetHeight(),
            minDist, 30, rng);

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (const auto& pos : positions) {
            if (pos.x < 1 || pos.x >= heightmap->GetWidth() - 1 ||
                pos.y < 1 || pos.y >= heightmap->GetHeight() - 1) continue;

            int ix = static_cast<int>(pos.x);
            int iy = static_cast<int>(pos.y);

            // Check slope
            glm::vec3 normal = heightmap->GetNormal(ix, iy, 1.0f);
            float slope = 1.0f - normal.y;

            if (slope < minSlope || slope > maxSlope) continue;

            // Add vegetation instance
            VegetationPlacement veg;
            veg.position = pos;
            veg.vegetationType = vegetationType;
            veg.scale = 0.8f + dist(rng) * 0.4f; // 0.8-1.2 scale variation
            veg.rotation = dist(rng) * 2.0f * 3.14159265359f;
            veg.biomeId = 0; // Default biome

            vegetation->push_back(veg);
        }

        auto outPort = GetOutputPort("vegetationMap");
        if (outPort) outPort->SetValue(vegetation);

    } catch (...) {}
}

void WaterPlacementNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        float waterLevel = GetPortValue(GetInputPort("waterLevel"), 0.3f);

        int width = heightmap->GetWidth();
        int height = heightmap->GetHeight();

        auto waterMask = std::make_shared<HeightmapData>(width, height);
        auto depthMap = std::make_shared<HeightmapData>(width, height);

        // Simple water placement based on height threshold
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float h = heightmap->Get(x, y);
                if (h <= waterLevel) {
                    waterMask->Set(x, y, 1.0f);
                    depthMap->Set(x, y, waterLevel - h);
                } else {
                    waterMask->Set(x, y, 0.0f);
                    depthMap->Set(x, y, 0.0f);
                }
            }
        }

        // Flow simulation for rivers (optional enhancement)
        auto flowMapPort = GetInputPort("flowMap");
        if (flowMapPort && flowMapPort->IsConnected()) {
            try {
                auto flowMap = std::any_cast<std::shared_ptr<HeightmapData>>(flowMapPort->GetValue());
                if (flowMap) {
                    // Add river channels based on flow accumulation
                    float flowThreshold = 0.5f;
                    for (int y = 0; y < height; y++) {
                        for (int x = 0; x < width; x++) {
                            float flow = flowMap->Get(x, y);
                            if (flow > flowThreshold) {
                                waterMask->Set(x, y, 1.0f);
                                depthMap->Set(x, y, std::max(depthMap->Get(x, y), flow * 0.1f));
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        auto outWaterMaskPort = GetOutputPort("waterMask");
        auto outDepthMapPort = GetOutputPort("depthMap");

        if (outWaterMaskPort) outWaterMaskPort->SetValue(waterMask);
        if (outDepthMapPort) outDepthMapPort->SetValue(depthMap);

    } catch (...) {}
}

/**
 * @brief Structure placement data
 */
struct StructurePlacement {
    glm::vec2 position;
    std::string structureType;
    float rotation;
    float scale;
    int priority;
};

void RuinsPlacementNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        float density = GetPortValue(GetInputPort("density"), 0.01f);
        float minDistance = GetPortValue(GetInputPort("minDistance"), 50.0f);

        auto ruins = std::make_shared<std::vector<StructurePlacement>>();

        // Get ruin types (default if not provided)
        std::vector<std::string> ruinTypes = {"small_ruin", "medium_ruin", "large_ruin", "tower_ruin"};

        std::mt19937 rng(54321);
        auto positions = PoissonDiskSampling(
            heightmap->GetWidth(), heightmap->GetHeight(),
            minDistance, 30, rng);

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> typeDist(0, static_cast<int>(ruinTypes.size()) - 1);

        // Only keep a fraction based on density
        int maxRuins = static_cast<int>(positions.size() * density * 10.0f);
        int ruinCount = 0;

        for (const auto& pos : positions) {
            if (ruinCount >= maxRuins) break;

            if (pos.x < 5 || pos.x >= heightmap->GetWidth() - 5 ||
                pos.y < 5 || pos.y >= heightmap->GetHeight() - 5) continue;

            int ix = static_cast<int>(pos.x);
            int iy = static_cast<int>(pos.y);

            // Check for relatively flat area
            glm::vec3 normal = heightmap->GetNormal(ix, iy, 1.0f);
            float slope = 1.0f - normal.y;

            if (slope > 0.2f) continue; // Too steep for ruins

            // Random chance based on density
            if (dist(rng) > density) continue;

            StructurePlacement ruin;
            ruin.position = pos;
            ruin.structureType = ruinTypes[typeDist(rng)];
            ruin.rotation = dist(rng) * 2.0f * 3.14159265359f;
            ruin.scale = 0.8f + dist(rng) * 0.4f;
            ruin.priority = static_cast<int>(dist(rng) * 100);

            ruins->push_back(ruin);
            ruinCount++;
        }

        auto outPort = GetOutputPort("ruinsList");
        if (outPort) outPort->SetValue(ruins);

    } catch (...) {}
}

void AncientStructuresNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        float density = GetPortValue(GetInputPort("density"), 0.005f);
        float minSize = GetPortValue(GetInputPort("minSize"), 10.0f);
        float maxSize = GetPortValue(GetInputPort("maxSize"), 50.0f);

        auto structures = std::make_shared<std::vector<StructurePlacement>>();

        // Ancient structure types
        std::vector<std::string> structureTypes = {"temple", "monument", "dungeon_entrance", "altar", "obelisk"};

        std::mt19937 rng(99999);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> typeDist(0, static_cast<int>(structureTypes.size()) - 1);

        // Find suitable high points or special locations
        std::vector<glm::vec2> candidates;

        // Sample the heightmap for high points
        int step = 10;
        for (int y = step; y < heightmap->GetHeight() - step; y += step) {
            for (int x = step; x < heightmap->GetWidth() - step; x += step) {
                float h = heightmap->Get(x, y);

                // Check if this is a local maximum
                bool isLocalMax = true;
                for (int dy = -step; dy <= step && isLocalMax; dy += step) {
                    for (int dx = -step; dx <= step && isLocalMax; dx += step) {
                        if (dx == 0 && dy == 0) continue;
                        if (heightmap->Get(x + dx, y + dy) > h) {
                            isLocalMax = false;
                        }
                    }
                }

                if (isLocalMax && h > 0.6f) { // On elevated terrain
                    candidates.push_back(glm::vec2(x, y));
                }
            }
        }

        // Also check for flat areas (for dungeons)
        for (int y = step; y < heightmap->GetHeight() - step; y += step) {
            for (int x = step; x < heightmap->GetWidth() - step; x += step) {
                glm::vec3 normal = heightmap->GetNormal(x, y, 1.0f);
                float slope = 1.0f - normal.y;

                if (slope < 0.05f && heightmap->Get(x, y) > 0.3f) {
                    candidates.push_back(glm::vec2(x, y));
                }
            }
        }

        // Select structures from candidates
        float minDistSq = minSize * minSize * 4.0f;

        for (const auto& pos : candidates) {
            if (dist(rng) > density * 10.0f) continue;

            // Check distance to existing structures
            bool tooClose = false;
            for (const auto& existing : *structures) {
                float dx = pos.x - existing.position.x;
                float dy = pos.y - existing.position.y;
                if (dx*dx + dy*dy < minDistSq) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;

            StructurePlacement structure;
            structure.position = pos;
            structure.structureType = structureTypes[typeDist(rng)];
            structure.rotation = dist(rng) * 2.0f * 3.14159265359f;
            structure.scale = minSize + dist(rng) * (maxSize - minSize);
            structure.priority = 100 + static_cast<int>(dist(rng) * 100);

            structures->push_back(structure);
        }

        auto outPort = GetOutputPort("structuresList");
        if (outPort) outPort->SetValue(structures);

    } catch (...) {}
}

/**
 * @brief Building placement data
 */
struct BuildingPlacement {
    glm::vec2 position;
    std::string buildingType;
    float rotation;
    glm::vec2 size;
    int villageId;
};

void BuildingPlacementNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        std::string buildingType = GetPortValue(GetInputPort("buildingType"), std::string("house"));
        float density = GetPortValue(GetInputPort("density"), 0.1f);
        float maxSlope = GetPortValue(GetInputPort("maxSlope"), 0.15f);

        auto buildings = std::make_shared<std::vector<BuildingPlacement>>();

        std::mt19937 rng(77777);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        // Find village centers (relatively flat, medium elevation areas)
        std::vector<glm::vec2> villageCenters;
        int step = 30;

        for (int y = step; y < heightmap->GetHeight() - step; y += step) {
            for (int x = step; x < heightmap->GetWidth() - step; x += step) {
                float h = heightmap->Get(x, y);

                // Check if area is relatively flat
                float maxLocalSlope = 0.0f;
                for (int dy = -5; dy <= 5; dy++) {
                    for (int dx = -5; dx <= 5; dx++) {
                        glm::vec3 normal = heightmap->GetNormal(x + dx, y + dy, 1.0f);
                        maxLocalSlope = std::max(maxLocalSlope, 1.0f - normal.y);
                    }
                }

                if (maxLocalSlope < maxSlope && h > 0.2f && h < 0.6f) {
                    if (dist(rng) < 0.1f) { // 10% chance to be a village center
                        villageCenters.push_back(glm::vec2(x, y));
                    }
                }
            }
        }

        // Generate buildings around village centers
        int villageId = 0;
        for (const auto& center : villageCenters) {
            // Number of buildings based on density
            int numBuildings = static_cast<int>(20 * density + dist(rng) * 10);

            for (int i = 0; i < numBuildings; i++) {
                float angle = dist(rng) * 2.0f * 3.14159265359f;
                float radius = 5.0f + dist(rng) * 25.0f;

                glm::vec2 pos = center + glm::vec2(std::cos(angle), std::sin(angle)) * radius;

                if (pos.x < 2 || pos.x >= heightmap->GetWidth() - 2 ||
                    pos.y < 2 || pos.y >= heightmap->GetHeight() - 2) continue;

                int ix = static_cast<int>(pos.x);
                int iy = static_cast<int>(pos.y);

                // Check slope
                glm::vec3 normal = heightmap->GetNormal(ix, iy, 1.0f);
                float slope = 1.0f - normal.y;
                if (slope > maxSlope) continue;

                BuildingPlacement building;
                building.position = pos;
                building.buildingType = buildingType;
                building.rotation = dist(rng) * 2.0f * 3.14159265359f;
                building.size = glm::vec2(4.0f + dist(rng) * 4.0f, 4.0f + dist(rng) * 4.0f);
                building.villageId = villageId;

                buildings->push_back(building);
            }
            villageId++;
        }

        auto outPort = GetOutputPort("buildingsList");
        if (outPort) outPort->SetValue(buildings);

    } catch (...) {}
}

// =============================================================================
// Biome and Climate Node Implementations
// =============================================================================

/**
 * @brief Biome map data structure
 */
struct BiomeMapData {
    int width;
    int height;
    std::vector<int> biomeIds;
    std::vector<BiomeInfo> biomeTypes;

    BiomeMapData(int w, int h) : width(w), height(h), biomeIds(w * h, 0) {}

    int Get(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0;
        return biomeIds[y * width + x];
    }

    void Set(int x, int y, int biomeId) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        biomeIds[y * width + x] = biomeId;
    }
};

void BiomeNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    auto temperaturePort = GetInputPort("temperature");
    auto precipitationPort = GetInputPort("precipitation");

    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        float latitude = GetPortValue(GetInputPort("latitude"), 45.0f);

        int width = heightmap->GetWidth();
        int height = heightmap->GetHeight();

        auto biomeMap = std::make_shared<BiomeMapData>(width, height);

        // Define biome types using Whittaker diagram
        biomeMap->biomeTypes = {
            {0, "ocean", glm::vec3(0.0f, 0.2f, 0.5f), -50, 50, 0, 5000, -1.0f, 0.3f},
            {1, "beach", glm::vec3(0.9f, 0.85f, 0.6f), 10, 40, 0, 2000, 0.3f, 0.35f},
            {2, "tropical_rainforest", glm::vec3(0.0f, 0.4f, 0.0f), 20, 35, 2000, 5000, 0.35f, 0.7f},
            {3, "tropical_seasonal_forest", glm::vec3(0.2f, 0.5f, 0.1f), 20, 35, 1000, 2000, 0.35f, 0.7f},
            {4, "temperate_rainforest", glm::vec3(0.1f, 0.35f, 0.15f), 5, 20, 1500, 3000, 0.35f, 0.7f},
            {5, "temperate_deciduous", glm::vec3(0.2f, 0.45f, 0.1f), 5, 20, 750, 1500, 0.35f, 0.7f},
            {6, "grassland", glm::vec3(0.5f, 0.6f, 0.2f), 0, 25, 250, 750, 0.35f, 0.6f},
            {7, "desert", glm::vec3(0.8f, 0.7f, 0.4f), 10, 50, 0, 250, 0.35f, 0.6f},
            {8, "taiga", glm::vec3(0.15f, 0.3f, 0.2f), -10, 5, 250, 750, 0.35f, 0.8f},
            {9, "tundra", glm::vec3(0.6f, 0.7f, 0.65f), -50, -5, 0, 500, 0.35f, 0.8f},
            {10, "mountain", glm::vec3(0.5f, 0.5f, 0.5f), -20, 20, 0, 2000, 0.8f, 1.0f},
            {11, "snow_peak", glm::vec3(0.95f, 0.95f, 0.95f), -50, 0, 0, 3000, 0.9f, 1.0f}
        };

        // Get temperature and precipitation maps if provided
        std::shared_ptr<HeightmapData> tempMap, precipMap;

        if (temperaturePort && temperaturePort->IsConnected()) {
            try {
                tempMap = std::any_cast<std::shared_ptr<HeightmapData>>(temperaturePort->GetValue());
            } catch (...) {}
        }

        if (precipitationPort && precipitationPort->IsConnected()) {
            try {
                precipMap = std::any_cast<std::shared_ptr<HeightmapData>>(precipitationPort->GetValue());
            } catch (...) {}
        }

        // Calculate biomes
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float elevation = heightmap->Get(x, y);

                // Calculate temperature (base + latitude + elevation effects)
                float temperature;
                if (tempMap) {
                    temperature = tempMap->Get(x, y) * 60.0f - 20.0f; // Scale to -20 to 40
                } else {
                    temperature = 30.0f - std::abs(latitude) * 0.5f - elevation * 30.0f;
                }

                // Calculate precipitation
                float precipitation;
                if (precipMap) {
                    precipitation = precipMap->Get(x, y) * 3000.0f; // Scale to 0-3000mm
                } else {
                    precipitation = 1000.0f + (elevation > 0.5f ? -500.0f : 500.0f);
                }

                // Determine biome based on elevation, temperature, precipitation
                int biomeId = 7; // Default: desert

                if (elevation < 0.3f) {
                    biomeId = 0; // Ocean
                } else if (elevation < 0.35f) {
                    biomeId = 1; // Beach
                } else if (elevation > 0.9f) {
                    biomeId = 11; // Snow peak
                } else if (elevation > 0.8f) {
                    biomeId = 10; // Mountain
                } else {
                    // Use Whittaker diagram logic
                    if (temperature > 20.0f) {
                        if (precipitation > 2000.0f) biomeId = 2; // Tropical rainforest
                        else if (precipitation > 1000.0f) biomeId = 3; // Tropical seasonal
                        else if (precipitation > 250.0f) biomeId = 6; // Grassland
                        else biomeId = 7; // Desert
                    } else if (temperature > 5.0f) {
                        if (precipitation > 1500.0f) biomeId = 4; // Temperate rainforest
                        else if (precipitation > 750.0f) biomeId = 5; // Temperate deciduous
                        else if (precipitation > 250.0f) biomeId = 6; // Grassland
                        else biomeId = 7; // Desert
                    } else if (temperature > -10.0f) {
                        if (precipitation > 250.0f) biomeId = 8; // Taiga
                        else biomeId = 9; // Tundra
                    } else {
                        biomeId = 9; // Tundra
                    }
                }

                biomeMap->Set(x, y, biomeId);
            }
        }

        auto outPort = GetOutputPort("biomeMap");
        if (outPort) outPort->SetValue(biomeMap);

    } catch (...) {}
}

void ClimateNode::Execute(VisualScript::ExecutionContext& context) {
    auto heightmapPort = GetInputPort("heightmap");
    if (!heightmapPort || !heightmapPort->IsConnected()) return;

    try {
        auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
        if (!heightmap) return;

        float latitude = GetPortValue(GetInputPort("latitude"), 45.0f);
        std::string windPattern = GetPortValue(GetInputPort("windPattern"), std::string("westerly"));

        int width = heightmap->GetWidth();
        int height = heightmap->GetHeight();

        auto temperatureMap = std::make_shared<HeightmapData>(width, height);
        auto precipitationMap = std::make_shared<HeightmapData>(width, height);
        auto humidityMap = std::make_shared<HeightmapData>(width, height);

        // Get ocean distance if provided
        std::shared_ptr<HeightmapData> oceanDistance;
        auto oceanPort = GetInputPort("oceanDistance");
        if (oceanPort && oceanPort->IsConnected()) {
            try {
                oceanDistance = std::any_cast<std::shared_ptr<HeightmapData>>(oceanPort->GetValue());
            } catch (...) {}
        }

        // Calculate climate variables
        float baseTemp = 30.0f - std::abs(latitude) * 0.7f;

        // Wind direction based on pattern
        glm::vec2 windDir(1.0f, 0.0f);
        if (windPattern == "easterly") windDir = glm::vec2(-1.0f, 0.0f);
        else if (windPattern == "northerly") windDir = glm::vec2(0.0f, -1.0f);
        else if (windPattern == "southerly") windDir = glm::vec2(0.0f, 1.0f);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float elevation = heightmap->Get(x, y);

                // Temperature: decreases with elevation and distance from equator
                float temp = baseTemp;
                temp -= elevation * 40.0f; // Lapse rate

                // Ocean moderation
                if (oceanDistance) {
                    float dist = oceanDistance->Get(x, y);
                    temp = temp * (1.0f - dist * 0.1f) + 15.0f * (dist * 0.1f);
                }

                // Normalize to 0-1
                float normalizedTemp = (temp + 20.0f) / 60.0f;
                temperatureMap->Set(x, y, glm::clamp(normalizedTemp, 0.0f, 1.0f));

                // Humidity: starts high near ocean, decreases inland
                float humidity = 0.8f;
                if (oceanDistance) {
                    humidity -= oceanDistance->Get(x, y) * 0.4f;
                }
                humidity = std::max(0.2f, humidity);
                humidityMap->Set(x, y, humidity);

                // Precipitation: based on humidity and orographic effects
                float precip = humidity;

                // Orographic precipitation (rain shadow effect)
                if (x > 0 && y > 0 && x < width - 1 && y < height - 1) {
                    float upwindElevation = heightmap->GetBilinear(
                        x - windDir.x * 5.0f, y - windDir.y * 5.0f);
                    float elevDiff = elevation - upwindElevation;

                    if (elevDiff > 0) {
                        // Windward side: more precipitation
                        precip += elevDiff * 2.0f;
                    } else {
                        // Leeward side: rain shadow
                        precip += elevDiff * 1.5f;
                    }
                }

                precipitationMap->Set(x, y, glm::clamp(precip, 0.0f, 1.0f));
            }
        }

        auto outTempPort = GetOutputPort("temperature");
        auto outPrecipPort = GetOutputPort("precipitation");
        auto outHumidityPort = GetOutputPort("humidity");

        if (outTempPort) outTempPort->SetValue(temperatureMap);
        if (outPrecipPort) outPrecipPort->SetValue(precipitationMap);
        if (outHumidityPort) outHumidityPort->SetValue(humidityMap);

    } catch (...) {}
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

        int width = std::min(heightmapA->GetWidth(), heightmapB->GetWidth());
        int height = std::min(heightmapA->GetHeight(), heightmapB->GetHeight());

        auto result = std::make_shared<HeightmapData>(width, height);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float a = heightmapA->Get(x, y);
                float b = heightmapB->Get(x, y);
                float value;

                if (blendMode == "add") {
                    value = a + b * blend;
                } else if (blendMode == "subtract") {
                    value = a - b * blend;
                } else if (blendMode == "multiply") {
                    value = a * glm::mix(1.0f, b, blend);
                } else if (blendMode == "screen") {
                    value = 1.0f - (1.0f - a) * (1.0f - b * blend);
                } else if (blendMode == "overlay") {
                    if (a < 0.5f) {
                        value = 2.0f * a * glm::mix(a, b, blend);
                    } else {
                        value = 1.0f - 2.0f * (1.0f - a) * (1.0f - glm::mix(a, b, blend));
                    }
                } else if (blendMode == "min") {
                    value = std::min(a, b);
                } else if (blendMode == "max") {
                    value = std::max(a, b);
                } else if (blendMode == "difference") {
                    value = std::abs(a - b);
                } else { // lerp (default)
                    value = glm::mix(a, b, blend);
                }

                result->Set(x, y, glm::clamp(value, 0.0f, 1.0f));
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

        float inputRange = inputMax - inputMin;
        float outputRange = outputMax - outputMin;

        if (std::abs(inputRange) < 0.0001f) inputRange = 1.0f; // Avoid division by zero

        for (int y = 0; y < result->GetHeight(); y++) {
            for (int x = 0; x < result->GetWidth(); x++) {
                float value = heightmap->Get(x, y);
                float normalized = (value - inputMin) / inputRange;
                float remapped = outputMin + normalized * outputRange;
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
                } else if (curveType == "smootherstep") {
                    // Perlin's improved smoothstep
                    curved = value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
                } else if (curveType == "pow2") {
                    curved = value * value;
                } else if (curveType == "pow3") {
                    curved = value * value * value;
                } else if (curveType == "pow4") {
                    curved = value * value * value * value;
                } else if (curveType == "sqrt") {
                    curved = std::sqrt(std::max(0.0f, value));
                } else if (curveType == "cbrt") {
                    curved = std::cbrt(value);
                } else if (curveType == "sin") {
                    curved = std::sin(value * 3.14159265359f * 0.5f);
                } else if (curveType == "cos") {
                    curved = 1.0f - std::cos(value * 3.14159265359f * 0.5f);
                } else if (curveType == "exp") {
                    curved = (std::exp(value) - 1.0f) / (std::exp(1.0f) - 1.0f);
                } else if (curveType == "log") {
                    curved = std::log(value * (std::exp(1.0f) - 1.0f) + 1.0f);
                } else if (curveType == "step") {
                    curved = value > 0.5f ? 1.0f : 0.0f;
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

// =============================================================================
// Additional Math Nodes
// =============================================================================

/**
 * @brief Add node for heightmaps and scalars
 */
class AddNode : public VisualScript::Node {
public:
    AddNode() : Node("Add", "Add") {
        SetCategory(VisualScript::NodeCategory::Math);
        SetDescription("Adds two values");

        AddInputPort(std::make_shared<VisualScript::Port>("a", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "any"));
        AddInputPort(std::make_shared<VisualScript::Port>("b", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "any"));

        AddOutputPort(std::make_shared<VisualScript::Port>("result", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "any"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        auto aPort = GetInputPort("a");
        auto bPort = GetInputPort("b");

        try {
            // Try heightmap addition
            auto hmA = std::any_cast<std::shared_ptr<HeightmapData>>(aPort->GetValue());
            auto hmB = std::any_cast<std::shared_ptr<HeightmapData>>(bPort->GetValue());

            if (hmA && hmB) {
                auto result = std::make_shared<HeightmapData>(hmA->GetWidth(), hmA->GetHeight());
                for (int y = 0; y < result->GetHeight(); y++) {
                    for (int x = 0; x < result->GetWidth(); x++) {
                        result->Set(x, y, hmA->Get(x, y) + hmB->Get(x, y));
                    }
                }
                GetOutputPort("result")->SetValue(result);
                return;
            }
        } catch (...) {}

        try {
            // Try scalar addition
            float a = GetPortValue(aPort, 0.0f);
            float b = GetPortValue(bPort, 0.0f);
            GetOutputPort("result")->SetValue(a + b);
        } catch (...) {}
    }
};

/**
 * @brief Multiply node for heightmaps and scalars
 */
class MultiplyNode : public VisualScript::Node {
public:
    MultiplyNode() : Node("Multiply", "Multiply") {
        SetCategory(VisualScript::NodeCategory::Math);
        SetDescription("Multiplies two values");

        AddInputPort(std::make_shared<VisualScript::Port>("a", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "any"));
        AddInputPort(std::make_shared<VisualScript::Port>("b", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "any"));

        AddOutputPort(std::make_shared<VisualScript::Port>("result", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "any"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        auto aPort = GetInputPort("a");
        auto bPort = GetInputPort("b");

        try {
            // Try heightmap * heightmap
            auto hmA = std::any_cast<std::shared_ptr<HeightmapData>>(aPort->GetValue());
            auto hmB = std::any_cast<std::shared_ptr<HeightmapData>>(bPort->GetValue());

            if (hmA && hmB) {
                auto result = std::make_shared<HeightmapData>(hmA->GetWidth(), hmA->GetHeight());
                for (int y = 0; y < result->GetHeight(); y++) {
                    for (int x = 0; x < result->GetWidth(); x++) {
                        result->Set(x, y, hmA->Get(x, y) * hmB->Get(x, y));
                    }
                }
                GetOutputPort("result")->SetValue(result);
                return;
            }
        } catch (...) {}

        try {
            // Try heightmap * scalar
            auto hm = std::any_cast<std::shared_ptr<HeightmapData>>(aPort->GetValue());
            float scalar = GetPortValue(bPort, 1.0f);

            if (hm) {
                auto result = std::make_shared<HeightmapData>(hm->GetWidth(), hm->GetHeight());
                for (int y = 0; y < result->GetHeight(); y++) {
                    for (int x = 0; x < result->GetWidth(); x++) {
                        result->Set(x, y, hm->Get(x, y) * scalar);
                    }
                }
                GetOutputPort("result")->SetValue(result);
                return;
            }
        } catch (...) {}

        try {
            // Scalar multiplication
            float a = GetPortValue(aPort, 0.0f);
            float b = GetPortValue(bPort, 1.0f);
            GetOutputPort("result")->SetValue(a * b);
        } catch (...) {}
    }
};

// =============================================================================
// Geometry Nodes (SDF Primitives and CSG)
// =============================================================================

/**
 * @brief SDF Sphere primitive node
 */
class SDFSphereNode : public VisualScript::Node {
public:
    SDFSphereNode() : Node("SDFSphere", "SDF Sphere") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("SDF sphere primitive");

        AddInputPort(std::make_shared<VisualScript::Port>("position", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec3"));
        AddInputPort(std::make_shared<VisualScript::Port>("center", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec3"));
        AddInputPort(std::make_shared<VisualScript::Port>("radius", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("distance", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        glm::vec3 pos = GetPortValue(GetInputPort("position"), glm::vec3(0.0f));
        glm::vec3 center = GetPortValue(GetInputPort("center"), glm::vec3(0.0f));
        float radius = GetPortValue(GetInputPort("radius"), 1.0f);

        float dist = SDFSphere(pos - center, radius);
        GetOutputPort("distance")->SetValue(dist);
    }
};

/**
 * @brief SDF Box primitive node
 */
class SDFBoxNode : public VisualScript::Node {
public:
    SDFBoxNode() : Node("SDFBox", "SDF Box") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("SDF box primitive");

        AddInputPort(std::make_shared<VisualScript::Port>("position", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec3"));
        AddInputPort(std::make_shared<VisualScript::Port>("center", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec3"));
        AddInputPort(std::make_shared<VisualScript::Port>("size", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec3"));

        AddOutputPort(std::make_shared<VisualScript::Port>("distance", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        glm::vec3 pos = GetPortValue(GetInputPort("position"), glm::vec3(0.0f));
        glm::vec3 center = GetPortValue(GetInputPort("center"), glm::vec3(0.0f));
        glm::vec3 size = GetPortValue(GetInputPort("size"), glm::vec3(1.0f));

        float dist = SDFBox(pos - center, size * 0.5f);
        GetOutputPort("distance")->SetValue(dist);
    }
};

/**
 * @brief CSG Union node
 */
class CSGUnionNode : public VisualScript::Node {
public:
    CSGUnionNode() : Node("CSGUnion", "CSG Union") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("CSG union of two SDF shapes");

        AddInputPort(std::make_shared<VisualScript::Port>("distanceA", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("distanceB", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("smoothness", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("distance", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        float dA = GetPortValue(GetInputPort("distanceA"), 0.0f);
        float dB = GetPortValue(GetInputPort("distanceB"), 0.0f);
        float k = GetPortValue(GetInputPort("smoothness"), 0.0f);

        float dist = k > 0.0f ? SDFSmoothUnion(dA, dB, k) : SDFUnion(dA, dB);
        GetOutputPort("distance")->SetValue(dist);
    }
};

/**
 * @brief CSG Intersection node
 */
class CSGIntersectionNode : public VisualScript::Node {
public:
    CSGIntersectionNode() : Node("CSGIntersection", "CSG Intersection") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("CSG intersection of two SDF shapes");

        AddInputPort(std::make_shared<VisualScript::Port>("distanceA", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("distanceB", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("smoothness", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("distance", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        float dA = GetPortValue(GetInputPort("distanceA"), 0.0f);
        float dB = GetPortValue(GetInputPort("distanceB"), 0.0f);
        float k = GetPortValue(GetInputPort("smoothness"), 0.0f);

        float dist = k > 0.0f ? SDFSmoothIntersection(dA, dB, k) : SDFIntersection(dA, dB);
        GetOutputPort("distance")->SetValue(dist);
    }
};

/**
 * @brief CSG Difference node
 */
class CSGDifferenceNode : public VisualScript::Node {
public:
    CSGDifferenceNode() : Node("CSGDifference", "CSG Difference") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("CSG difference of two SDF shapes (A minus B)");

        AddInputPort(std::make_shared<VisualScript::Port>("distanceA", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("distanceB", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));
        AddInputPort(std::make_shared<VisualScript::Port>("smoothness", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "float"));

        AddOutputPort(std::make_shared<VisualScript::Port>("distance", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        float dA = GetPortValue(GetInputPort("distanceA"), 0.0f);
        float dB = GetPortValue(GetInputPort("distanceB"), 0.0f);
        float k = GetPortValue(GetInputPort("smoothness"), 0.0f);

        float dist = k > 0.0f ? SDFSmoothDifference(dA, dB, k) : SDFDifference(dA, dB);
        GetOutputPort("distance")->SetValue(dist);
    }
};

// =============================================================================
// Texture Nodes
// =============================================================================

/**
 * @brief Texture sample node
 */
class TextureSampleNode : public VisualScript::Node {
public:
    TextureSampleNode() : Node("TextureSample", "Texture Sample") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Samples a texture at UV coordinates");

        AddInputPort(std::make_shared<VisualScript::Port>("texture", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "texture"));
        AddInputPort(std::make_shared<VisualScript::Port>("uv", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec2"));

        AddOutputPort(std::make_shared<VisualScript::Port>("color", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "vec4"));
        AddOutputPort(std::make_shared<VisualScript::Port>("value", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "float"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        // Sample heightmap as texture
        auto texPort = GetInputPort("texture");
        glm::vec2 uv = GetPortValue(GetInputPort("uv"), glm::vec2(0.0f));

        try {
            auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(texPort->GetValue());
            if (heightmap) {
                float x = uv.x * (heightmap->GetWidth() - 1);
                float y = uv.y * (heightmap->GetHeight() - 1);
                float value = heightmap->GetBilinear(x, y);

                GetOutputPort("color")->SetValue(glm::vec4(value, value, value, 1.0f));
                GetOutputPort("value")->SetValue(value);
            }
        } catch (...) {}
    }
};

/**
 * @brief Gradient texture generator node
 */
class GradientTextureNode : public VisualScript::Node {
public:
    GradientTextureNode() : Node("GradientTexture", "Gradient Texture") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Generates a gradient texture");

        AddInputPort(std::make_shared<VisualScript::Port>("width", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));
        AddInputPort(std::make_shared<VisualScript::Port>("height", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));
        AddInputPort(std::make_shared<VisualScript::Port>("direction", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec2"));

        AddOutputPort(std::make_shared<VisualScript::Port>("texture", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        int width = GetPortValue(GetInputPort("width"), 64);
        int height = GetPortValue(GetInputPort("height"), 64);
        glm::vec2 dir = glm::normalize(GetPortValue(GetInputPort("direction"), glm::vec2(1.0f, 0.0f)));

        auto result = std::make_shared<HeightmapData>(width, height);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                glm::vec2 uv(static_cast<float>(x) / (width - 1), static_cast<float>(y) / (height - 1));
                float value = glm::dot(uv, dir);
                result->Set(x, y, glm::clamp(value, 0.0f, 1.0f));
            }
        }

        GetOutputPort("texture")->SetValue(result);
    }
};

// =============================================================================
// Output Nodes
// =============================================================================

/**
 * @brief Mesh output node - converts heightmap to mesh
 */
class MeshOutputNode : public VisualScript::Node {
public:
    struct MeshData {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<uint32_t> indices;
    };

    MeshOutputNode() : Node("MeshOutput", "Mesh Output") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Converts heightmap to mesh data");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("scale", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec3"));
        AddInputPort(std::make_shared<VisualScript::Port>("lodLevel", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "int"));

        AddOutputPort(std::make_shared<VisualScript::Port>("mesh", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "mesh"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        auto heightmapPort = GetInputPort("heightmap");
        if (!heightmapPort || !heightmapPort->IsConnected()) return;

        try {
            auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(heightmapPort->GetValue());
            if (!heightmap) return;

            glm::vec3 scale = GetPortValue(GetInputPort("scale"), glm::vec3(1.0f));
            int lodLevel = GetPortValue(GetInputPort("lodLevel"), 0);

            int step = 1 << lodLevel; // LOD step
            int width = heightmap->GetWidth();
            int height = heightmap->GetHeight();

            auto mesh = std::make_shared<MeshData>();

            // Generate vertices
            for (int z = 0; z < height; z += step) {
                for (int x = 0; x < width; x += step) {
                    float h = heightmap->Get(x, z);
                    mesh->vertices.push_back(glm::vec3(
                        x * scale.x / (width - 1),
                        h * scale.y,
                        z * scale.z / (height - 1)));

                    glm::vec3 normal = heightmap->GetNormal(x, z, scale.y);
                    mesh->normals.push_back(normal);

                    mesh->uvs.push_back(glm::vec2(
                        static_cast<float>(x) / (width - 1),
                        static_cast<float>(z) / (height - 1)));
                }
            }

            // Generate indices
            int gridWidth = (width - 1) / step + 1;
            int gridHeight = (height - 1) / step + 1;

            for (int z = 0; z < gridHeight - 1; z++) {
                for (int x = 0; x < gridWidth - 1; x++) {
                    uint32_t topLeft = z * gridWidth + x;
                    uint32_t topRight = topLeft + 1;
                    uint32_t bottomLeft = (z + 1) * gridWidth + x;
                    uint32_t bottomRight = bottomLeft + 1;

                    // First triangle
                    mesh->indices.push_back(topLeft);
                    mesh->indices.push_back(bottomLeft);
                    mesh->indices.push_back(topRight);

                    // Second triangle
                    mesh->indices.push_back(topRight);
                    mesh->indices.push_back(bottomLeft);
                    mesh->indices.push_back(bottomRight);
                }
            }

            GetOutputPort("mesh")->SetValue(mesh);

        } catch (...) {}
    }
};

/**
 * @brief SDF output node - outputs SDF field data
 */
class SDFOutputNode : public VisualScript::Node {
public:
    struct SDFFieldData {
        int width, height, depth;
        std::vector<float> distances;
        glm::vec3 minBounds, maxBounds;
    };

    SDFOutputNode() : Node("SDFOutput", "SDF Output") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Outputs SDF field data for rendering");

        AddInputPort(std::make_shared<VisualScript::Port>("evaluator", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "function"));
        AddInputPort(std::make_shared<VisualScript::Port>("resolution", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "ivec3"));
        AddInputPort(std::make_shared<VisualScript::Port>("bounds", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "vec3"));

        AddOutputPort(std::make_shared<VisualScript::Port>("sdfField", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "sdf"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        // This would typically connect to an SDF evaluator graph
        // For now, create empty field
        auto field = std::make_shared<SDFFieldData>();
        field->width = 64;
        field->height = 64;
        field->depth = 64;
        field->distances.resize(64 * 64 * 64, 0.0f);
        field->minBounds = glm::vec3(-1.0f);
        field->maxBounds = glm::vec3(1.0f);

        GetOutputPort("sdfField")->SetValue(field);
    }
};

/**
 * @brief Heightmap output node - outputs heightmap as final result
 */
class HeightmapOutputNode : public VisualScript::Node {
public:
    HeightmapOutputNode() : Node("HeightmapOutput", "Heightmap Output") {
        SetCategory(VisualScript::NodeCategory::Custom);
        SetDescription("Final heightmap output");

        AddInputPort(std::make_shared<VisualScript::Port>("heightmap", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "heightmap"));
        AddInputPort(std::make_shared<VisualScript::Port>("normalize", VisualScript::PortDirection::Input, VisualScript::PortType::Data, "bool"));

        AddOutputPort(std::make_shared<VisualScript::Port>("result", VisualScript::PortDirection::Output, VisualScript::PortType::Data, "heightmap"));
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        auto inputPort = GetInputPort("heightmap");
        if (!inputPort || !inputPort->IsConnected()) return;

        try {
            auto heightmap = std::any_cast<std::shared_ptr<HeightmapData>>(inputPort->GetValue());
            if (!heightmap) return;

            bool normalize = GetPortValue(GetInputPort("normalize"), true);

            if (normalize) {
                auto result = std::make_shared<HeightmapData>(*heightmap);

                // Find min/max
                float minVal = std::numeric_limits<float>::max();
                float maxVal = std::numeric_limits<float>::lowest();

                for (int y = 0; y < result->GetHeight(); y++) {
                    for (int x = 0; x < result->GetWidth(); x++) {
                        float v = result->Get(x, y);
                        minVal = std::min(minVal, v);
                        maxVal = std::max(maxVal, v);
                    }
                }

                // Normalize
                float range = maxVal - minVal;
                if (range > 0.0001f) {
                    for (int y = 0; y < result->GetHeight(); y++) {
                        for (int x = 0; x < result->GetWidth(); x++) {
                            float v = (result->Get(x, y) - minVal) / range;
                            result->Set(x, y, v);
                        }
                    }
                }

                GetOutputPort("result")->SetValue(result);
            } else {
                GetOutputPort("result")->SetValue(heightmap);
            }

        } catch (...) {}
    }
};

// =============================================================================
// Node Factory Registration
// =============================================================================

namespace {

/**
 * @brief Registers all procedural generation nodes with the factory
 */
struct ProcGenNodeRegistrar {
    ProcGenNodeRegistrar() {
        auto& factory = ProcGenNodeFactory::Instance();
        factory.RegisterBuiltInNodes();
    }
};

// Auto-register on startup
static ProcGenNodeRegistrar s_registrar;

} // anonymous namespace

void ProcGenNodeFactory::RegisterBuiltInNodes() {
    // Noise nodes
    RegisterNode("PerlinNoise", []() { return std::make_shared<PerlinNoiseNode>(); });
    RegisterNode("SimplexNoise", []() { return std::make_shared<SimplexNoiseNode>(); });
    RegisterNode("WorleyNoise", []() { return std::make_shared<WorleyNoiseNode>(); });
    RegisterNode("Voronoi", []() { return std::make_shared<VoronoiNode>(); });

    // Erosion nodes
    RegisterNode("HydraulicErosion", []() { return std::make_shared<HydraulicErosionNode>(); });
    RegisterNode("ThermalErosion", []() { return std::make_shared<ThermalErosionNode>(); });

    // Terrain shaping nodes
    RegisterNode("Terrace", []() { return std::make_shared<TerraceNode>(); });
    RegisterNode("Ridge", []() { return std::make_shared<RidgeNode>(); });
    RegisterNode("Slope", []() { return std::make_shared<SlopeNode>(); });

    // Placement nodes
    RegisterNode("ResourcePlacement", []() { return std::make_shared<ResourcePlacementNode>(); });
    RegisterNode("VegetationPlacement", []() { return std::make_shared<VegetationPlacementNode>(); });
    RegisterNode("WaterPlacement", []() { return std::make_shared<WaterPlacementNode>(); });
    RegisterNode("RuinsPlacement", []() { return std::make_shared<RuinsPlacementNode>(); });
    RegisterNode("AncientStructures", []() { return std::make_shared<AncientStructuresNode>(); });
    RegisterNode("BuildingPlacement", []() { return std::make_shared<BuildingPlacementNode>(); });

    // Biome/Climate nodes
    RegisterNode("Biome", []() { return std::make_shared<BiomeNode>(); });
    RegisterNode("Climate", []() { return std::make_shared<ClimateNode>(); });

    // Utility nodes
    RegisterNode("Blend", []() { return std::make_shared<BlendNode>(); });
    RegisterNode("Remap", []() { return std::make_shared<RemapNode>(); });
    RegisterNode("Curve", []() { return std::make_shared<CurveNode>(); });
    RegisterNode("Clamp", []() { return std::make_shared<ClampNode>(); });

    // Math nodes
    RegisterNode("Add", []() { return std::make_shared<AddNode>(); });
    RegisterNode("Multiply", []() { return std::make_shared<MultiplyNode>(); });

    // Geometry nodes
    RegisterNode("SDFSphere", []() { return std::make_shared<SDFSphereNode>(); });
    RegisterNode("SDFBox", []() { return std::make_shared<SDFBoxNode>(); });
    RegisterNode("CSGUnion", []() { return std::make_shared<CSGUnionNode>(); });
    RegisterNode("CSGIntersection", []() { return std::make_shared<CSGIntersectionNode>(); });
    RegisterNode("CSGDifference", []() { return std::make_shared<CSGDifferenceNode>(); });

    // Texture nodes
    RegisterNode("TextureSample", []() { return std::make_shared<TextureSampleNode>(); });
    RegisterNode("GradientTexture", []() { return std::make_shared<GradientTextureNode>(); });

    // Output nodes
    RegisterNode("MeshOutput", []() { return std::make_shared<MeshOutputNode>(); });
    RegisterNode("SDFOutput", []() { return std::make_shared<SDFOutputNode>(); });
    RegisterNode("HeightmapOutput", []() { return std::make_shared<HeightmapOutputNode>(); });
}

ProcGenNodeFactory& ProcGenNodeFactory::Instance() {
    static ProcGenNodeFactory instance;
    return instance;
}

void ProcGenNodeFactory::RegisterNode(const std::string& typeId, NodeCreator creator) {
    m_creators[typeId] = creator;
}

VisualScript::NodePtr ProcGenNodeFactory::CreateNode(const std::string& typeId) const {
    auto it = m_creators.find(typeId);
    if (it != m_creators.end()) {
        return it->second();
    }
    return nullptr;
}

std::vector<std::string> ProcGenNodeFactory::GetNodeTypes() const {
    std::vector<std::string> types;
    types.reserve(m_creators.size());
    for (const auto& [key, value] : m_creators) {
        types.push_back(key);
    }
    return types;
}

// =============================================================================
// Serialization Support
// =============================================================================

/**
 * @brief Serializes a node graph to JSON
 */
nlohmann::json SerializeNodeGraph(const VisualScript::Graph* graph) {
    return graph->Serialize();
}

/**
 * @brief Deserializes a node graph from JSON
 */
VisualScript::GraphPtr DeserializeNodeGraph(const nlohmann::json& json) {
    return VisualScript::Graph::Deserialize(json);
}

} // namespace ProcGen
} // namespace Nova
