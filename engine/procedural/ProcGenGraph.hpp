#pragma once

#include "../scripting/visual/VisualScriptingCore.hpp"
#include "ProcGenNodes.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <future>

namespace Nova {
namespace ProcGen {

// Forward declarations
class ChunkGenerator;
struct GenerationTask;

/**
 * @brief Result of chunk generation
 */
struct ChunkGenerationResult {
    glm::ivec2 chunkPos;
    std::shared_ptr<HeightmapData> heightmap;
    std::vector<uint8_t> biomeData;
    std::vector<uint8_t> resourceData;
    std::vector<uint8_t> structureData;
    bool success = false;
    std::string errorMessage;
    float generationTime = 0.0f;
};

/**
 * @brief Configuration for procedural generation
 */
struct ProcGenConfig {
    int seed = 12345;
    int chunkSize = 64;
    float worldScale = 1.0f;
    int maxConcurrentTasks = 4;
    bool enableCaching = true;
    size_t maxCacheSize = 1024; // chunks
    std::string cachePath = "cache/procgen/";
};

/**
 * @brief Procedural generation graph executor
 *
 * Executes a visual script graph to generate terrain chunks.
 * Supports multi-threaded generation and caching.
 */
class ProcGenGraph {
public:
    ProcGenGraph();
    ~ProcGenGraph();

    /**
     * @brief Load graph from visual script
     */
    bool LoadFromGraph(VisualScript::GraphPtr graph);

    /**
     * @brief Load graph from JSON
     */
    bool LoadFromJson(const nlohmann::json& json);

    /**
     * @brief Save graph to JSON
     */
    nlohmann::json SaveToJson() const;

    /**
     * @brief Set generation configuration
     */
    void SetConfig(const ProcGenConfig& config) { m_config = config; }
    const ProcGenConfig& GetConfig() const { return m_config; }

    /**
     * @brief Generate a single chunk (synchronous)
     */
    ChunkGenerationResult GenerateChunk(const glm::ivec2& chunkPos);

    /**
     * @brief Generate chunk asynchronously
     */
    std::future<ChunkGenerationResult> GenerateChunkAsync(const glm::ivec2& chunkPos);

    /**
     * @brief Generate multiple chunks asynchronously
     */
    std::vector<std::future<ChunkGenerationResult>> GenerateChunks(const std::vector<glm::ivec2>& chunkPositions);

    /**
     * @brief Check if chunk is in cache
     */
    bool IsChunkCached(const glm::ivec2& chunkPos) const;

    /**
     * @brief Get cached chunk (returns null if not cached)
     */
    ChunkGenerationResult GetCachedChunk(const glm::ivec2& chunkPos) const;

    /**
     * @brief Clear generation cache
     */
    void ClearCache();

    /**
     * @brief Save cache to disk
     */
    bool SaveCache();

    /**
     * @brief Load cache from disk
     */
    bool LoadCache();

    /**
     * @brief Validate graph structure
     */
    bool Validate(std::vector<std::string>& errors) const;

    /**
     * @brief Get generation statistics
     */
    struct Stats {
        size_t chunksGenerated = 0;
        size_t chunksCached = 0;
        float avgGenerationTime = 0.0f;
        float totalGenerationTime = 0.0f;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
    };
    Stats GetStats() const { return m_stats; }

private:
    VisualScript::GraphPtr m_graph;
    ProcGenConfig m_config;
    Stats m_stats;

    // Cache management
    mutable std::unordered_map<uint64_t, ChunkGenerationResult> m_cache;
    mutable std::mutex m_cacheMutex;

    uint64_t ChunkPosToKey(const glm::ivec2& pos) const {
        return (static_cast<uint64_t>(pos.x) << 32) | static_cast<uint64_t>(pos.y);
    }

    glm::ivec2 KeyToChunkPos(uint64_t key) const {
        return glm::ivec2(key >> 32, key & 0xFFFFFFFF);
    }
};

/**
 * @brief Integration with ChunkStreamer for seamless world streaming
 */
class ProcGenChunkProvider {
public:
    ProcGenChunkProvider(std::shared_ptr<ProcGenGraph> graph);

    /**
     * @brief Request chunk generation
     * Called by ChunkStreamer when chunk needs to be loaded
     */
    void RequestChunk(const glm::ivec3& chunkPos, int priority);

    /**
     * @brief Check if chunk is ready
     */
    bool IsChunkReady(const glm::ivec3& chunkPos) const;

    /**
     * @brief Get generated chunk data
     */
    bool GetChunkData(const glm::ivec3& chunkPos, std::vector<uint8_t>& terrainData,
                      std::vector<uint8_t>& biomeData);

    /**
     * @brief Cancel pending chunk generation
     */
    void CancelChunk(const glm::ivec3& chunkPos);

    /**
     * @brief Update - processes completed generation tasks
     */
    void Update();

private:
    std::shared_ptr<ProcGenGraph> m_graph;

    struct PendingChunk {
        glm::ivec3 pos;
        int priority;
        std::future<ChunkGenerationResult> future;
    };
    std::vector<PendingChunk> m_pendingChunks;
    std::unordered_map<uint64_t, ChunkGenerationResult> m_completedChunks;
    mutable std::mutex m_mutex;
};

/**
 * @brief Factory for creating procedural generation nodes
 */
class ProcGenNodeFactory {
public:
    static ProcGenNodeFactory& Instance();

    /**
     * @brief Register a node type
     */
    using NodeCreator = std::function<VisualScript::NodePtr()>;
    void RegisterNode(const std::string& typeId, NodeCreator creator);

    /**
     * @brief Create node by type ID
     */
    VisualScript::NodePtr CreateNode(const std::string& typeId) const;

    /**
     * @brief Get all registered node types
     */
    std::vector<std::string> GetNodeTypes() const;

    /**
     * @brief Register all built-in proc gen nodes
     */
    void RegisterBuiltInNodes();

private:
    ProcGenNodeFactory() = default;
    std::unordered_map<std::string, NodeCreator> m_creators;
};

} // namespace ProcGen
} // namespace Nova
