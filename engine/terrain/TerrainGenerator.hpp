#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {

class Mesh;
class Shader;
class Texture;

/**
 * @brief Terrain chunk data
 */
class TerrainChunk {
public:
    TerrainChunk(int x, int z, int size, float scale);

    void Generate(float frequency, float amplitude, int octaves,
                  float persistence, float lacunarity);

    void CreateMesh();

    const glm::ivec2& GetCoord() const { return m_coord; }
    std::shared_ptr<Mesh> GetMesh() const { return m_mesh; }
    float GetHeight(float worldX, float worldZ) const;

    glm::vec3 GetNormal(float worldX, float worldZ) const;

private:
    glm::ivec2 m_coord;
    int m_size;
    float m_scale;
    std::vector<float> m_heights;
    std::shared_ptr<Mesh> m_mesh;
};

/**
 * @brief Procedural terrain generator using noise
 */
class TerrainGenerator {
public:
    TerrainGenerator();
    ~TerrainGenerator();

    /**
     * @brief Initialize the terrain generator
     */
    bool Initialize();

    /**
     * @brief Update visible chunks based on camera position
     */
    void Update(const glm::vec3& viewerPosition);

    /**
     * @brief Render visible terrain
     */
    void Render(Shader& shader);

    /**
     * @brief Get height at world position
     */
    float GetHeightAt(float x, float z) const;

    /**
     * @brief Get normal at world position
     */
    glm::vec3 GetNormalAt(float x, float z) const;

    // Configuration
    void SetViewDistance(int chunks) { m_viewDistance = chunks; }
    void SetChunkSize(int size) { m_chunkSize = size; }
    void SetHeightScale(float scale) { m_heightScale = scale; }
    void SetNoiseParams(float frequency, int octaves, float persistence, float lacunarity);

private:
    struct ChunkKey {
        int x, z;
        bool operator==(const ChunkKey& other) const {
            return x == other.x && z == other.z;
        }
    };

    struct ChunkKeyHash {
        size_t operator()(const ChunkKey& key) const {
            return std::hash<int>()(key.x) ^ (std::hash<int>()(key.z) << 1);
        }
    };

    TerrainChunk* GetOrCreateChunk(int x, int z);

    std::unordered_map<ChunkKey, std::unique_ptr<TerrainChunk>, ChunkKeyHash> m_chunks;
    std::vector<TerrainChunk*> m_visibleChunks;

    int m_viewDistance = 4;
    int m_chunkSize = 64;
    float m_chunkScale = 1.0f;
    float m_heightScale = 50.0f;
    float m_frequency = 0.02f;
    int m_octaves = 6;
    float m_persistence = 0.5f;
    float m_lacunarity = 2.0f;

    glm::ivec2 m_lastViewerChunk{INT_MAX, INT_MAX};
};

} // namespace Nova
