#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <future>
#include <atomic>
#include <queue>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

class Mesh;
class Shader;
class Texture;

/**
 * @brief Level of Detail configuration for terrain chunks
 */
struct TerrainLOD {
    int resolution;      // Vertex resolution at this LOD level
    float maxDistance;   // Maximum distance for this LOD
};

/**
 * @brief State of a terrain chunk
 */
enum class ChunkState {
    Unloaded,
    Generating,
    Generated,
    MeshPending,
    Ready
};

/**
 * @brief Terrain chunk data with LOD support
 */
class TerrainChunk {
public:
    TerrainChunk(int x, int z, int size, float scale);
    ~TerrainChunk() = default;

    // Non-copyable, movable
    TerrainChunk(const TerrainChunk&) = delete;
    TerrainChunk& operator=(const TerrainChunk&) = delete;
    TerrainChunk(TerrainChunk&&) noexcept = default;
    TerrainChunk& operator=(TerrainChunk&&) noexcept = default;

    /**
     * @brief Generate heightmap data (thread-safe, can run async)
     */
    void Generate(float frequency, float amplitude, int octaves,
                  float persistence, float lacunarity);

    /**
     * @brief Generate heightmap with specific LOD resolution
     */
    void GenerateWithLOD(float frequency, float amplitude, int octaves,
                         float persistence, float lacunarity, int lodResolution);

    /**
     * @brief Create mesh from heightmap (must be called on main thread)
     */
    void CreateMesh();

    /**
     * @brief Create mesh with specific LOD level
     */
    void CreateMeshWithLOD(int lodLevel);

    /**
     * @brief Update LOD based on distance from viewer
     */
    void UpdateLOD(const glm::vec3& viewerPosition, const std::vector<TerrainLOD>& lodLevels);

    // Accessors
    [[nodiscard]] const glm::ivec2& GetCoord() const noexcept { return m_coord; }
    [[nodiscard]] std::shared_ptr<Mesh> GetMesh() const noexcept { return m_mesh; }
    [[nodiscard]] float GetHeight(float worldX, float worldZ) const;
    [[nodiscard]] glm::vec3 GetNormal(float worldX, float worldZ) const;
    [[nodiscard]] ChunkState GetState() const noexcept { return m_state.load(); }
    [[nodiscard]] int GetCurrentLOD() const noexcept { return m_currentLOD; }
    [[nodiscard]] glm::vec3 GetWorldCenter() const noexcept;
    [[nodiscard]] float GetDistanceToViewer(const glm::vec3& viewerPos) const noexcept;

    void SetState(ChunkState state) noexcept { m_state.store(state); }
    [[nodiscard]] bool IsReady() const noexcept { return m_state.load() == ChunkState::Ready; }

private:
    glm::ivec2 m_coord;
    int m_size;
    int m_effectiveResolution;  // Actual resolution (may be lower for LOD)
    float m_scale;
    std::vector<float> m_heights;
    std::shared_ptr<Mesh> m_mesh;
    std::atomic<ChunkState> m_state{ChunkState::Unloaded};
    int m_currentLOD{0};
    mutable std::shared_mutex m_heightsMutex;
};

/**
 * @brief Procedural terrain generator with async chunk loading and LOD
 */
class TerrainGenerator {
public:
    TerrainGenerator();
    ~TerrainGenerator();

    // Non-copyable
    TerrainGenerator(const TerrainGenerator&) = delete;
    TerrainGenerator& operator=(const TerrainGenerator&) = delete;

    /**
     * @brief Initialize the terrain generator
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Update visible chunks based on camera position
     * @param viewerPosition Current camera/viewer world position
     */
    void Update(const glm::vec3& viewerPosition);

    /**
     * @brief Process pending mesh creation on main thread
     * @param maxChunksPerFrame Limit mesh creation per frame for performance
     */
    void ProcessPendingMeshes(int maxChunksPerFrame = 2);

    /**
     * @brief Render visible terrain
     */
    void Render(Shader& shader);

    /**
     * @brief Get height at world position (thread-safe)
     */
    [[nodiscard]] float GetHeightAt(float x, float z) const;

    /**
     * @brief Get normal at world position (thread-safe)
     */
    [[nodiscard]] glm::vec3 GetNormalAt(float x, float z) const;

    /**
     * @brief Get statistics for debugging
     */
    struct Stats {
        size_t totalChunks;
        size_t visibleChunks;
        size_t pendingChunks;
        size_t generatingChunks;
    };
    [[nodiscard]] Stats GetStats() const;

    // Configuration
    void SetViewDistance(int chunks) { m_viewDistance = chunks; }
    void SetChunkSize(int size) { m_chunkSize = size; }
    void SetHeightScale(float scale) { m_heightScale = scale; }
    void SetNoiseParams(float frequency, int octaves, float persistence, float lacunarity);
    void SetLODLevels(const std::vector<TerrainLOD>& levels) { m_lodLevels = levels; }

    /**
     * @brief Force unload chunks beyond a certain distance
     */
    void UnloadDistantChunks(float maxDistance);

    /**
     * @brief Clear all chunks and reset
     */
    void ClearAllChunks();

private:
    struct ChunkKey {
        int x, z;

        bool operator==(const ChunkKey& other) const noexcept {
            return x == other.x && z == other.z;
        }
    };

    struct ChunkKeyHash {
        size_t operator()(const ChunkKey& key) const noexcept {
            // Improved hash combining
            size_t h1 = std::hash<int>{}(key.x);
            size_t h2 = std::hash<int>{}(key.z);
            return h1 ^ (h2 * 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    /**
     * @brief Get or create chunk, potentially queuing async generation
     */
    TerrainChunk* GetOrCreateChunk(int x, int z);

    /**
     * @brief Queue chunk for async generation
     */
    void QueueChunkGeneration(TerrainChunk* chunk);

    /**
     * @brief Worker function for async chunk generation
     */
    void GenerateChunkAsync(TerrainChunk* chunk);

    /**
     * @brief Calculate priority for chunk generation (closer = higher priority)
     */
    [[nodiscard]] float CalculateChunkPriority(int chunkX, int chunkZ) const;

    // Chunk storage with thread-safe access
    std::unordered_map<ChunkKey, std::unique_ptr<TerrainChunk>, ChunkKeyHash> m_chunks;
    mutable std::shared_mutex m_chunksMutex;

    // Visible chunks (updated each frame)
    std::vector<TerrainChunk*> m_visibleChunks;

    // Pending mesh creation queue (chunks generated but need mesh on main thread)
    std::queue<TerrainChunk*> m_pendingMeshQueue;
    mutable std::mutex m_pendingMeshMutex;

    // Async generation futures
    std::vector<std::future<void>> m_generationFutures;
    std::atomic<int> m_activeGenerations{0};
    static constexpr int MAX_CONCURRENT_GENERATIONS = 4;

    // Configuration
    int m_viewDistance = 4;
    int m_chunkSize = 64;
    float m_chunkScale = 1.0f;
    float m_heightScale = 50.0f;
    float m_frequency = 0.02f;
    int m_octaves = 6;
    float m_persistence = 0.5f;
    float m_lacunarity = 2.0f;

    // LOD configuration
    std::vector<TerrainLOD> m_lodLevels;

    // Viewer tracking
    glm::vec3 m_lastViewerPosition{0.0f};
    glm::ivec2 m_lastViewerChunk{INT_MAX, INT_MAX};

    // Chunk unloading
    float m_unloadDistance = 0.0f;  // 0 = auto (viewDistance * 1.5)

    std::atomic<bool> m_shutdown{false};
};

} // namespace Nova
