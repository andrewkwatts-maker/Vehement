#pragma once

#include "WorldDatabase.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <set>
#include <map>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Chunk I/O request types
 */
enum class ChunkIORequestType {
    Load,
    Save,
    Unload,
    Generate
};

/**
 * @brief Chunk I/O request
 */
struct ChunkIORequest {
    ChunkIORequestType type;
    glm::ivec3 chunkPos;
    ChunkData data;
    int priority = 0;
    uint64_t timestamp = 0;
    std::function<void(bool)> callback;

    bool operator<(const ChunkIORequest& other) const {
        return priority < other.priority; // Higher priority = higher value
    }
};

/**
 * @brief Chunk load state
 */
enum class ChunkLoadState {
    Unloaded,
    Queued,
    Loading,
    Loaded,
    Saving,
    Dirty
};

/**
 * @brief Chunk streaming statistics
 */
struct ChunkStreamStats {
    size_t loadedChunks = 0;
    size_t dirtyChunks = 0;
    size_t pendingLoads = 0;
    size_t pendingSaves = 0;
    float avgLoadTime = 0.0f;    // Milliseconds
    float avgSaveTime = 0.0f;    // Milliseconds
    size_t totalBytesLoaded = 0;
    size_t totalBytesSaved = 0;
    size_t cacheHits = 0;
    size_t cacheMisses = 0;
};

/**
 * @brief Chunk streaming system with background I/O
 *
 * Features:
 * - Load/unload chunks based on player proximity
 * - Background thread for I/O operations
 * - Priority-based loading (near > far)
 * - Automatic dirty chunk tracking
 * - Periodic auto-save
 * - LRU cache for chunk data
 * - Load/save statistics
 */
class ChunkStreamer {
public:
    ChunkStreamer();
    ~ChunkStreamer();

    // Prevent copying
    ChunkStreamer(const ChunkStreamer&) = delete;
    ChunkStreamer& operator=(const ChunkStreamer&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize chunk streamer
     * @param database Database to use for I/O
     * @param ioThreadCount Number of background I/O threads
     * @return True if initialization succeeded
     */
    bool Initialize(WorldDatabase* database, int ioThreadCount = 1);

    /**
     * @brief Shutdown chunk streamer and flush all changes
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return m_database != nullptr; }

    // =========================================================================
    // UPDATE
    // =========================================================================

    /**
     * @brief Update chunk streaming (call each frame)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // VIEW CONFIGURATION
    // =========================================================================

    /**
     * @brief Set view position to determine which chunks to load
     * @param position View position
     * @param playerId Optional player ID for tracking
     */
    void SetViewPosition(glm::vec3 position, int playerId = -1);

    /**
     * @brief Set view distance (radius)
     * @param distance View distance
     */
    void SetViewDistance(float distance);

    /**
     * @brief Get current view distance
     */
    float GetViewDistance() const { return m_viewDistance; }

    /**
     * @brief Add view position (for multiple players)
     * @param playerId Player ID
     * @param position View position
     */
    void AddViewPosition(int playerId, glm::vec3 position);

    /**
     * @brief Remove view position
     * @param playerId Player ID
     */
    void RemoveViewPosition(int playerId);

    // =========================================================================
    // CHUNK OPERATIONS
    // =========================================================================

    /**
     * @brief Load chunk (async)
     * @param chunkPos Chunk position
     * @param priority Load priority (higher = load first)
     * @param callback Optional completion callback
     */
    void LoadChunk(glm::ivec3 chunkPos, int priority = 0, std::function<void(bool)> callback = nullptr);

    /**
     * @brief Save chunk (async)
     * @param chunkPos Chunk position
     * @param data Chunk data
     * @param callback Optional completion callback
     */
    void SaveChunk(glm::ivec3 chunkPos, const ChunkData& data, std::function<void(bool)> callback = nullptr);

    /**
     * @brief Unload chunk
     * @param chunkPos Chunk position
     * @param saveIfDirty Save before unloading if dirty
     */
    void UnloadChunk(glm::ivec3 chunkPos, bool saveIfDirty = true);

    /**
     * @brief Mark chunk as dirty (needs saving)
     * @param chunkPos Chunk position
     */
    void MarkChunkDirty(glm::ivec3 chunkPos);

    /**
     * @brief Check if chunk is loaded
     */
    bool IsChunkLoaded(glm::ivec3 chunkPos) const;

    /**
     * @brief Get chunk data (if loaded)
     * @param chunkPos Chunk position
     * @return Pointer to chunk data, or nullptr if not loaded
     */
    ChunkData* GetChunk(glm::ivec3 chunkPos);

    /**
     * @brief Get chunk data (const)
     */
    const ChunkData* GetChunk(glm::ivec3 chunkPos) const;

    /**
     * @brief Get chunk load state
     */
    ChunkLoadState GetChunkState(glm::ivec3 chunkPos) const;

    // =========================================================================
    // BATCH OPERATIONS
    // =========================================================================

    /**
     * @brief Save all dirty chunks
     * @param blocking Wait for saves to complete
     */
    void SaveAllDirtyChunks(bool blocking = false);

    /**
     * @brief Unload all chunks
     * @param saveFirst Save dirty chunks first
     */
    void UnloadAllChunks(bool saveFirst = true);

    /**
     * @brief Preload chunks in radius around position
     * @param center Center position
     * @param radius Load radius
     */
    void PreloadChunksInRadius(glm::vec3 center, float radius);

    // =========================================================================
    // AUTO-SAVE
    // =========================================================================

    /**
     * @brief Enable/disable auto-save
     */
    void SetAutoSaveEnabled(bool enabled);

    /**
     * @brief Set auto-save interval
     * @param seconds Interval in seconds
     */
    void SetAutoSaveInterval(float seconds);

    /**
     * @brief Get auto-save enabled
     */
    bool IsAutoSaveEnabled() const { return m_autoSaveEnabled; }

    /**
     * @brief Get auto-save interval
     */
    float GetAutoSaveInterval() const { return m_autoSaveInterval; }

    // =========================================================================
    // CACHE MANAGEMENT
    // =========================================================================

    /**
     * @brief Set max cached chunks (LRU eviction)
     * @param maxChunks Maximum number of chunks to cache
     */
    void SetMaxCachedChunks(size_t maxChunks);

    /**
     * @brief Get max cached chunks
     */
    size_t GetMaxCachedChunks() const { return m_maxCachedChunks; }

    /**
     * @brief Clear chunk cache
     * @param saveFirst Save dirty chunks first
     */
    void ClearCache(bool saveFirst = true);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get streaming statistics
     */
    ChunkStreamStats GetStatistics() const;

    /**
     * @brief Get loaded chunk count
     */
    size_t GetLoadedChunkCount() const;

    /**
     * @brief Get dirty chunk count
     */
    size_t GetDirtyChunkCount() const;

    /**
     * @brief Get pending I/O request count
     */
    size_t GetPendingRequestCount() const;

    // =========================================================================
    // CALLBACKS
    // =========================================================================

    std::function<void(glm::ivec3 chunkPos, const ChunkData& chunk)> OnChunkLoaded;
    std::function<void(glm::ivec3 chunkPos)> OnChunkUnloaded;
    std::function<void(glm::ivec3 chunkPos, bool success)> OnChunkSaved;
    std::function<void(const std::string& error)> OnError;

private:
    // I/O thread function
    void IOThreadFunc();

    // Process chunk loads
    void ProcessChunkLoads();

    // Process chunk saves
    void ProcessChunkSaves();

    // Process chunk unloads
    void ProcessChunkUnloads();

    // Determine which chunks should be loaded
    void UpdateLoadedChunks();

    // Calculate chunk priority based on distance
    int CalculateChunkPriority(glm::ivec3 chunkPos) const;

    // Get closest view position to chunk
    glm::vec3 GetClosestViewPosition(glm::ivec3 chunkPos) const;

    // LRU cache management
    void EvictLRUChunks();
    void UpdateChunkAccess(glm::ivec3 chunkPos);

    // Helper functions
    glm::ivec3 WorldToChunkPos(glm::vec3 worldPos) const;
    float GetChunkDistance(glm::ivec3 chunkPos, glm::vec3 worldPos) const;
    void LogError(const std::string& message);

    WorldDatabase* m_database = nullptr;

    // Loaded chunks (in memory)
    std::map<glm::ivec3, ChunkData> m_loadedChunks;
    std::map<glm::ivec3, ChunkLoadState> m_chunkStates;
    std::set<glm::ivec3> m_dirtyChunks;
    mutable std::mutex m_chunkMutex;

    // LRU cache
    std::map<glm::ivec3, uint64_t> m_chunkAccessTime;
    size_t m_maxCachedChunks = 1000;

    // View positions (for multiple players)
    std::map<int, glm::vec3> m_viewPositions;
    float m_viewDistance = 200.0f;
    int m_chunkSize = 16; // Chunk size in world units
    mutable std::mutex m_viewMutex;

    // I/O threads
    std::vector<std::thread> m_ioThreads;
    std::priority_queue<ChunkIORequest> m_ioQueue;
    std::mutex m_ioMutex;
    std::condition_variable m_ioCondition;
    std::atomic<bool> m_ioRunning{false};

    // Auto-save
    bool m_autoSaveEnabled = true;
    float m_autoSaveInterval = 300.0f; // 5 minutes
    float m_autoSaveTimer = 0.0f;

    // Statistics
    mutable std::mutex m_statsMutex;
    ChunkStreamStats m_stats;
    float m_totalLoadTime = 0.0f;
    size_t m_totalLoads = 0;
    float m_totalSaveTime = 0.0f;
    size_t m_totalSaves = 0;

    // Timing
    uint64_t m_frameCounter = 0;
};

// Helper for glm::ivec3 comparison in maps
inline bool operator<(const glm::ivec3& a, const glm::ivec3& b) {
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    return a.z < b.z;
}

} // namespace Nova
