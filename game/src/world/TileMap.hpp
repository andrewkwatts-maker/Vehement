#pragma once

#include "Tile.hpp"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {
    class Graph;
}

namespace Vehement {

/**
 * @brief Chunk of tiles for large map support
 *
 * Maps are divided into chunks for efficient loading/unloading
 * and reduced memory usage for large worlds.
 */
struct TileChunk {
    static constexpr int CHUNK_SIZE = 32;  // 32x32 tiles per chunk

    int chunkX = 0;
    int chunkY = 0;
    std::vector<Tile> tiles;  // CHUNK_SIZE * CHUNK_SIZE tiles
    bool dirty = false;       // Needs re-rendering
    bool loaded = false;

    TileChunk() : tiles(CHUNK_SIZE * CHUNK_SIZE) {}

    /**
     * @brief Get tile at local chunk coordinates
     */
    Tile& GetTile(int localX, int localY) {
        return tiles[localY * CHUNK_SIZE + localX];
    }

    const Tile& GetTile(int localX, int localY) const {
        return tiles[localY * CHUNK_SIZE + localX];
    }

    /**
     * @brief Get chunk key for hashmap storage
     */
    static int64_t GetChunkKey(int chunkX, int chunkY) {
        return (static_cast<int64_t>(chunkX) << 32) | (static_cast<uint32_t>(chunkY));
    }
};

/**
 * @brief Configuration for tile map creation
 */
struct TileMapConfig {
    int width = 64;                  // Map width in tiles
    int height = 64;                 // Map height in tiles
    float tileSize = 1.0f;           // World units per tile
    bool useChunks = false;          // Enable chunk-based loading
    TileType defaultTile = TileType::GroundGrass1;
};

/**
 * @brief Grid-based tile map system
 *
 * Manages a 2D grid of tiles with support for:
 * - Load/save to JSON format (Firebase compatible)
 * - Coordinate conversion between world and tile space
 * - Pathfinding integration via walkability grid
 * - Optional chunk-based loading for large maps
 */
class TileMap {
public:
    TileMap();
    explicit TileMap(const TileMapConfig& config);
    ~TileMap() = default;

    // Prevent copying (maps can be large)
    TileMap(const TileMap&) = delete;
    TileMap& operator=(const TileMap&) = delete;
    TileMap(TileMap&&) noexcept = default;
    TileMap& operator=(TileMap&&) noexcept = default;

    /**
     * @brief Initialize the map with given dimensions
     */
    void Initialize(int width, int height, float tileSize = 1.0f);

    /**
     * @brief Clear and reset the map
     */
    void Clear();

    /**
     * @brief Fill entire map with a tile type
     */
    void Fill(const Tile& tile);

    // ========== Tile Access ==========

    /**
     * @brief Get tile at grid coordinates
     * @param x Grid X coordinate
     * @param y Grid Y coordinate
     * @return Pointer to tile or nullptr if out of bounds
     */
    Tile* GetTile(int x, int y);
    const Tile* GetTile(int x, int y) const;

    /**
     * @brief Set tile at grid coordinates
     * @param x Grid X coordinate
     * @param y Grid Y coordinate
     * @param tile The tile to set
     * @return true if successful, false if out of bounds
     */
    bool SetTile(int x, int y, const Tile& tile);

    /**
     * @brief Get tile at world position
     */
    Tile* GetTileAtWorld(float worldX, float worldZ);
    const Tile* GetTileAtWorld(float worldX, float worldZ) const;

    /**
     * @brief Set tile at world position
     */
    bool SetTileAtWorld(float worldX, float worldZ, const Tile& tile);

    // ========== Coordinate Conversion ==========

    /**
     * @brief Convert world coordinates to tile coordinates
     */
    glm::ivec2 WorldToTile(float worldX, float worldZ) const;
    glm::ivec2 WorldToTile(const glm::vec3& worldPos) const;

    /**
     * @brief Convert tile coordinates to world coordinates (center of tile)
     */
    glm::vec3 TileToWorld(int tileX, int tileY) const;

    /**
     * @brief Convert tile coordinates to world coordinates (corner of tile)
     */
    glm::vec3 TileToWorldCorner(int tileX, int tileY) const;

    /**
     * @brief Check if tile coordinates are within bounds
     */
    bool IsInBounds(int x, int y) const;

    /**
     * @brief Check if world position is within map bounds
     */
    bool IsInBoundsWorld(float worldX, float worldZ) const;

    // ========== Map Properties ==========

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    float GetTileSize() const { return m_tileSize; }

    /**
     * @brief Get world-space bounds of the map
     */
    glm::vec2 GetWorldMin() const { return glm::vec2(0.0f); }
    glm::vec2 GetWorldMax() const {
        return glm::vec2(m_width * m_tileSize, m_height * m_tileSize);
    }

    // ========== Serialization (JSON for Firebase) ==========

    /**
     * @brief Save map to JSON string
     */
    std::string SaveToJson() const;

    /**
     * @brief Load map from JSON string
     */
    bool LoadFromJson(const std::string& json);

    /**
     * @brief Save map to file
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * @brief Load map from file
     */
    bool LoadFromFile(const std::string& filepath);

    // ========== Pathfinding Integration ==========

    /**
     * @brief Check if a tile is walkable
     */
    bool IsWalkable(int x, int y) const;

    /**
     * @brief Check if a world position is walkable
     */
    bool IsWalkableWorld(float worldX, float worldZ) const;

    /**
     * @brief Get movement cost at tile
     */
    float GetMovementCost(int x, int y) const;

    /**
     * @brief Build a navigation graph from the tile map
     * @param graph The graph to populate
     * @param includeDiagonals Whether to include diagonal connections
     */
    void BuildNavigationGraph(Nova::Graph& graph, bool includeDiagonals = true) const;

    /**
     * @brief Update navigation graph for a region (after tile changes)
     */
    void UpdateNavigationGraph(Nova::Graph& graph, int minX, int minY,
                                int maxX, int maxY, bool includeDiagonals = true) const;

    /**
     * @brief Get node ID for a tile coordinate (for graph lookups)
     */
    int GetNodeId(int x, int y) const { return y * m_width + x; }

    /**
     * @brief Get tile coordinates from node ID
     */
    glm::ivec2 GetTileFromNodeId(int nodeId) const {
        return glm::ivec2(nodeId % m_width, nodeId / m_width);
    }

    // ========== Chunk-based Loading ==========

    /**
     * @brief Enable chunk-based storage for large maps
     */
    void EnableChunks(bool enabled);

    /**
     * @brief Load chunk at specified chunk coordinates
     */
    bool LoadChunk(int chunkX, int chunkY);

    /**
     * @brief Unload chunk at specified chunk coordinates
     */
    void UnloadChunk(int chunkX, int chunkY);

    /**
     * @brief Check if chunk is loaded
     */
    bool IsChunkLoaded(int chunkX, int chunkY) const;

    /**
     * @brief Get all loaded chunk coordinates
     */
    std::vector<glm::ivec2> GetLoadedChunks() const;

    /**
     * @brief Set callback for loading chunk data
     */
    using ChunkLoadCallback = std::function<bool(int chunkX, int chunkY, TileChunk& chunk)>;
    void SetChunkLoadCallback(ChunkLoadCallback callback) { m_chunkLoadCallback = callback; }

    /**
     * @brief Set callback for saving chunk data
     */
    using ChunkSaveCallback = std::function<bool(int chunkX, int chunkY, const TileChunk& chunk)>;
    void SetChunkSaveCallback(ChunkSaveCallback callback) { m_chunkSaveCallback = callback; }

    // ========== Iteration ==========

    /**
     * @brief Iterate over all tiles
     */
    template<typename Func>
    void ForEachTile(Func&& func) {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                func(x, y, m_tiles[y * m_width + x]);
            }
        }
    }

    template<typename Func>
    void ForEachTile(Func&& func) const {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                func(x, y, m_tiles[y * m_width + x]);
            }
        }
    }

    /**
     * @brief Iterate over tiles in a region
     */
    template<typename Func>
    void ForEachTileInRegion(int minX, int minY, int maxX, int maxY, Func&& func) {
        minX = std::max(0, minX);
        minY = std::max(0, minY);
        maxX = std::min(m_width - 1, maxX);
        maxY = std::min(m_height - 1, maxY);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                func(x, y, m_tiles[y * m_width + x]);
            }
        }
    }

    /**
     * @brief Get all wall tiles (for rendering optimization)
     */
    std::vector<std::pair<glm::ivec2, const Tile*>> GetWallTiles() const;

    /**
     * @brief Mark region as dirty (needs re-rendering)
     */
    void MarkDirty(int x, int y, int width = 1, int height = 1);

    /**
     * @brief Check if any tiles are dirty
     */
    bool IsDirty() const { return m_dirty; }

    /**
     * @brief Clear dirty flag
     */
    void ClearDirty() { m_dirty = false; }

private:
    int m_width = 0;
    int m_height = 0;
    float m_tileSize = 1.0f;
    bool m_dirty = false;
    bool m_useChunks = false;

    // Flat storage for non-chunked maps
    std::vector<Tile> m_tiles;

    // Chunk storage for large maps
    std::unordered_map<int64_t, std::unique_ptr<TileChunk>> m_chunks;
    ChunkLoadCallback m_chunkLoadCallback;
    ChunkSaveCallback m_chunkSaveCallback;

    /**
     * @brief Get or create chunk for tile coordinates
     */
    TileChunk* GetChunkForTile(int x, int y);
    const TileChunk* GetChunkForTile(int x, int y) const;
};

} // namespace Vehement
