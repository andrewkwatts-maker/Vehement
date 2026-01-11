#include "ProcGenGraph.hpp"
#include <chrono>
#include <fstream>

namespace Nova {
namespace ProcGen {

// =============================================================================
// ProcGenGraph Implementation
// =============================================================================

ProcGenGraph::ProcGenGraph() {
}

ProcGenGraph::~ProcGenGraph() {
}

bool ProcGenGraph::LoadFromGraph(VisualScript::GraphPtr graph) {
    if (!graph) return false;

    std::vector<std::string> errors;
    if (!graph->Validate(errors)) {
        return false;
    }

    m_graph = graph;
    return true;
}

bool ProcGenGraph::LoadFromJson(const nlohmann::json& json) {
    try {
        m_graph = VisualScript::Graph::Deserialize(json);
        return m_graph != nullptr;
    } catch (const std::exception& e) {
        return false;
    }
}

nlohmann::json ProcGenGraph::SaveToJson() const {
    if (!m_graph) return nlohmann::json{};
    return m_graph->Serialize();
}

ChunkGenerationResult ProcGenGraph::GenerateChunk(const glm::ivec2& chunkPos) {
    auto startTime = std::chrono::high_resolution_clock::now();

    ChunkGenerationResult result;
    result.chunkPos = chunkPos;

    // Check cache first
    if (m_config.enableCaching) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        uint64_t key = ChunkPosToKey(chunkPos);
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            m_stats.cacheHits++;
            return it->second;
        }
        m_stats.cacheMisses++;
    }

    if (!m_graph) {
        result.errorMessage = "No generation graph loaded";
        return result;
    }

    try {
        // Create execution context
        VisualScript::ExecutionContext context(m_graph.get());

        // Set up chunk-specific variables
        m_graph->SetVariable("chunkX", chunkPos.x);
        m_graph->SetVariable("chunkY", chunkPos.y);
        m_graph->SetVariable("seed", m_config.seed);
        m_graph->SetVariable("chunkSize", m_config.chunkSize);
        m_graph->SetVariable("worldScale", m_config.worldScale);

        // Execute the graph
        // Note: This is simplified - actual implementation would walk the execution graph
        // For now, just create a simple heightmap
        auto heightmap = std::make_shared<HeightmapData>(m_config.chunkSize, m_config.chunkSize);

        // Simple Perlin noise generation as fallback
        for (int y = 0; y < m_config.chunkSize; y++) {
            for (int x = 0; x < m_config.chunkSize; x++) {
                glm::vec2 worldPos = glm::vec2(
                    (chunkPos.x * m_config.chunkSize + x) * m_config.worldScale,
                    (chunkPos.y * m_config.chunkSize + y) * m_config.worldScale
                );

                float noise = glm::perlin(worldPos * 0.01f) * 0.5f + 0.5f;
                heightmap->Set(x, y, noise);
            }
        }

        result.heightmap = heightmap;
        result.success = true;

        auto endTime = std::chrono::high_resolution_clock::now();
        result.generationTime = std::chrono::duration<float>(endTime - startTime).count();

        // Update stats
        m_stats.chunksGenerated++;
        m_stats.totalGenerationTime += result.generationTime;
        m_stats.avgGenerationTime = m_stats.totalGenerationTime / m_stats.chunksGenerated;

        // Cache result
        if (m_config.enableCaching) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            uint64_t key = ChunkPosToKey(chunkPos);
            m_cache[key] = result;
            m_stats.chunksCached = m_cache.size();

            // Evict oldest if cache is full
            if (m_cache.size() > m_config.maxCacheSize) {
                m_cache.erase(m_cache.begin());
            }
        }

    } catch (const std::exception& e) {
        result.errorMessage = std::string("Generation error: ") + e.what();
        result.success = false;
    }

    return result;
}

std::future<ChunkGenerationResult> ProcGenGraph::GenerateChunkAsync(const glm::ivec2& chunkPos) {
    return std::async(std::launch::async, [this, chunkPos]() {
        return GenerateChunk(chunkPos);
    });
}

std::vector<std::future<ChunkGenerationResult>> ProcGenGraph::GenerateChunks(
    const std::vector<glm::ivec2>& chunkPositions) {

    std::vector<std::future<ChunkGenerationResult>> futures;
    futures.reserve(chunkPositions.size());

    for (const auto& pos : chunkPositions) {
        futures.push_back(GenerateChunkAsync(pos));
    }

    return futures;
}

bool ProcGenGraph::IsChunkCached(const glm::ivec2& chunkPos) const {
    if (!m_config.enableCaching) return false;

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    uint64_t key = ChunkPosToKey(chunkPos);
    return m_cache.find(key) != m_cache.end();
}

ChunkGenerationResult ProcGenGraph::GetCachedChunk(const glm::ivec2& chunkPos) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    uint64_t key = ChunkPosToKey(chunkPos);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return it->second;
    }
    return ChunkGenerationResult{};
}

void ProcGenGraph::ClearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
    m_stats.chunksCached = 0;
}

bool ProcGenGraph::SaveCache() {
    // TODO: Implement cache serialization
    return false;
}

bool ProcGenGraph::LoadCache() {
    // TODO: Implement cache deserialization
    return false;
}

bool ProcGenGraph::Validate(std::vector<std::string>& errors) const {
    if (!m_graph) {
        errors.push_back("No generation graph loaded");
        return false;
    }

    return m_graph->Validate(errors);
}

// =============================================================================
// ProcGenChunkProvider Implementation
// =============================================================================

ProcGenChunkProvider::ProcGenChunkProvider(std::shared_ptr<ProcGenGraph> graph)
    : m_graph(graph) {
}

void ProcGenChunkProvider::RequestChunk(const glm::ivec3& chunkPos, int priority) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Convert 3D to 2D for now (ignore Z)
    glm::ivec2 chunkPos2D(chunkPos.x, chunkPos.y);

    // Check if already pending or completed
    uint64_t key = (static_cast<uint64_t>(chunkPos.x) << 32) | static_cast<uint64_t>(chunkPos.y);
    if (m_completedChunks.find(key) != m_completedChunks.end()) {
        return; // Already generated
    }

    for (const auto& pending : m_pendingChunks) {
        if (pending.pos == chunkPos) {
            return; // Already pending
        }
    }

    // Request generation
    PendingChunk pending;
    pending.pos = chunkPos;
    pending.priority = priority;
    pending.future = m_graph->GenerateChunkAsync(chunkPos2D);
    m_pendingChunks.push_back(std::move(pending));
}

bool ProcGenChunkProvider::IsChunkReady(const glm::ivec3& chunkPos) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = (static_cast<uint64_t>(chunkPos.x) << 32) | static_cast<uint64_t>(chunkPos.y);
    return m_completedChunks.find(key) != m_completedChunks.end();
}

bool ProcGenChunkProvider::GetChunkData(const glm::ivec3& chunkPos,
                                        std::vector<uint8_t>& terrainData,
                                        std::vector<uint8_t>& biomeData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = (static_cast<uint64_t>(chunkPos.x) << 32) | static_cast<uint64_t>(chunkPos.y);

    auto it = m_completedChunks.find(key);
    if (it == m_completedChunks.end()) {
        return false;
    }

    const auto& result = it->second;
    if (!result.success || !result.heightmap) {
        return false;
    }

    // Convert heightmap to terrain data
    terrainData.clear();
    const auto& heightData = result.heightmap->GetData();
    terrainData.reserve(heightData.size() * sizeof(float));

    for (float h : heightData) {
        // Convert float height to uint8_t (0-255)
        uint8_t value = static_cast<uint8_t>(std::clamp(h * 255.0f, 0.0f, 255.0f));
        terrainData.push_back(value);
    }

    biomeData = result.biomeData;
    return true;
}

void ProcGenChunkProvider::CancelChunk(const glm::ivec3& chunkPos) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_pendingChunks.erase(
        std::remove_if(m_pendingChunks.begin(), m_pendingChunks.end(),
            [&chunkPos](const PendingChunk& pending) {
                return pending.pos == chunkPos;
            }),
        m_pendingChunks.end());
}

void ProcGenChunkProvider::Update() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check completed futures
    for (auto it = m_pendingChunks.begin(); it != m_pendingChunks.end();) {
        if (it->future.wait_for(std::chrono::microseconds(0)) == std::future_status::ready) {
            auto result = it->future.get();
            uint64_t key = (static_cast<uint64_t>(it->pos.x) << 32) | static_cast<uint64_t>(it->pos.y);
            m_completedChunks[key] = std::move(result);
            it = m_pendingChunks.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// ProcGenNodeFactory Implementation
// =============================================================================

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
    for (const auto& pair : m_creators) {
        types.push_back(pair.first);
    }
    return types;
}

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

    // Resource placement nodes
    RegisterNode("ResourcePlacement", []() { return std::make_shared<ResourcePlacementNode>(); });
    RegisterNode("VegetationPlacement", []() { return std::make_shared<VegetationPlacementNode>(); });
    RegisterNode("WaterPlacement", []() { return std::make_shared<WaterPlacementNode>(); });

    // Structure placement nodes
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
}

} // namespace ProcGen
} // namespace Nova
