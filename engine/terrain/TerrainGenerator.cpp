#include "terrain/TerrainGenerator.hpp"
#include "terrain/NoiseGenerator.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Shader.hpp"
#include "config/Config.hpp"

#include <spdlog/spdlog.h>
#include <cmath>

namespace Nova {

// TerrainChunk implementation
TerrainChunk::TerrainChunk(int x, int z, int size, float scale)
    : m_coord(x, z)
    , m_size(size)
    , m_scale(scale)
{
    m_heights.resize((size + 1) * (size + 1));
}

void TerrainChunk::Generate(float frequency, float amplitude, int octaves,
                             float persistence, float lacunarity) {
    float worldX = m_coord.x * m_size * m_scale;
    float worldZ = m_coord.y * m_size * m_scale;

    for (int z = 0; z <= m_size; ++z) {
        for (int x = 0; x <= m_size; ++x) {
            float sampleX = (worldX + x * m_scale) * frequency;
            float sampleZ = (worldZ + z * m_scale) * frequency;

            float height = NoiseGenerator::FractalNoise(sampleX, sampleZ,
                                                         octaves, persistence, lacunarity);

            m_heights[z * (m_size + 1) + x] = height * amplitude;
        }
    }
}

void TerrainChunk::CreateMesh() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float worldX = m_coord.x * m_size * m_scale;
    float worldZ = m_coord.y * m_size * m_scale;

    // Create vertices
    for (int z = 0; z <= m_size; ++z) {
        for (int x = 0; x <= m_size; ++x) {
            float height = m_heights[z * (m_size + 1) + x];

            Vertex vertex;
            vertex.position = glm::vec3(
                worldX + x * m_scale,
                height,
                worldZ + z * m_scale
            );

            // Calculate normal
            float hL = x > 0 ? m_heights[z * (m_size + 1) + x - 1] : height;
            float hR = x < m_size ? m_heights[z * (m_size + 1) + x + 1] : height;
            float hD = z > 0 ? m_heights[(z - 1) * (m_size + 1) + x] : height;
            float hU = z < m_size ? m_heights[(z + 1) * (m_size + 1) + x] : height;

            vertex.normal = glm::normalize(glm::vec3(hL - hR, 2.0f * m_scale, hD - hU));
            vertex.texCoords = glm::vec2(
                static_cast<float>(x) / m_size,
                static_cast<float>(z) / m_size
            );

            // Tangent
            vertex.tangent = glm::vec3(1, 0, 0);
            vertex.bitangent = glm::cross(vertex.normal, vertex.tangent);

            vertices.push_back(vertex);
        }
    }

    // Create indices
    for (int z = 0; z < m_size; ++z) {
        for (int x = 0; x < m_size; ++x) {
            uint32_t topLeft = z * (m_size + 1) + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * (m_size + 1) + x;
            uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    m_mesh = std::make_shared<Mesh>();
    m_mesh->Create(vertices, indices);
}

float TerrainChunk::GetHeight(float worldX, float worldZ) const {
    float chunkX = m_coord.x * m_size * m_scale;
    float chunkZ = m_coord.y * m_size * m_scale;

    float localX = (worldX - chunkX) / m_scale;
    float localZ = (worldZ - chunkZ) / m_scale;

    int x0 = static_cast<int>(localX);
    int z0 = static_cast<int>(localZ);

    x0 = std::clamp(x0, 0, m_size - 1);
    z0 = std::clamp(z0, 0, m_size - 1);

    float tx = localX - x0;
    float tz = localZ - z0;

    float h00 = m_heights[z0 * (m_size + 1) + x0];
    float h10 = m_heights[z0 * (m_size + 1) + x0 + 1];
    float h01 = m_heights[(z0 + 1) * (m_size + 1) + x0];
    float h11 = m_heights[(z0 + 1) * (m_size + 1) + x0 + 1];

    float h0 = h00 + (h10 - h00) * tx;
    float h1 = h01 + (h11 - h01) * tx;

    return h0 + (h1 - h0) * tz;
}

// TerrainGenerator implementation
TerrainGenerator::TerrainGenerator() = default;

TerrainGenerator::~TerrainGenerator() = default;

bool TerrainGenerator::Initialize() {
    auto& config = Config::Instance();
    m_chunkSize = config.Get("terrain.chunk_size", 64);
    m_viewDistance = config.Get("terrain.view_distance", 4);
    m_heightScale = config.Get("terrain.height_scale", 50.0f);
    m_frequency = config.Get("terrain.noise_frequency", 0.02f);
    m_octaves = config.Get("terrain.octaves", 6);
    m_persistence = config.Get("terrain.persistence", 0.5f);
    m_lacunarity = config.Get("terrain.lacunarity", 2.0f);

    return true;
}

void TerrainGenerator::Update(const glm::vec3& viewerPosition) {
    int chunkX = static_cast<int>(std::floor(viewerPosition.x / (m_chunkSize * m_chunkScale)));
    int chunkZ = static_cast<int>(std::floor(viewerPosition.z / (m_chunkSize * m_chunkScale)));

    if (chunkX == m_lastViewerChunk.x && chunkZ == m_lastViewerChunk.y) {
        return;  // No change
    }

    m_lastViewerChunk = glm::ivec2(chunkX, chunkZ);
    m_visibleChunks.clear();

    for (int z = chunkZ - m_viewDistance; z <= chunkZ + m_viewDistance; ++z) {
        for (int x = chunkX - m_viewDistance; x <= chunkX + m_viewDistance; ++x) {
            TerrainChunk* chunk = GetOrCreateChunk(x, z);
            m_visibleChunks.push_back(chunk);
        }
    }
}

void TerrainGenerator::Render(Shader& shader) {
    for (TerrainChunk* chunk : m_visibleChunks) {
        if (chunk && chunk->GetMesh()) {
            shader.SetMat4("u_Model", glm::mat4(1.0f));
            chunk->GetMesh()->Draw();
        }
    }
}

float TerrainGenerator::GetHeightAt(float x, float z) const {
    int chunkX = static_cast<int>(std::floor(x / (m_chunkSize * m_chunkScale)));
    int chunkZ = static_cast<int>(std::floor(z / (m_chunkSize * m_chunkScale)));

    ChunkKey key{chunkX, chunkZ};
    auto it = m_chunks.find(key);
    if (it != m_chunks.end()) {
        return it->second->GetHeight(x, z);
    }

    return 0.0f;
}

glm::vec3 TerrainGenerator::GetNormalAt(float x, float z) const {
    float delta = 0.5f;
    float hL = GetHeightAt(x - delta, z);
    float hR = GetHeightAt(x + delta, z);
    float hD = GetHeightAt(x, z - delta);
    float hU = GetHeightAt(x, z + delta);

    return glm::normalize(glm::vec3(hL - hR, 2.0f * delta, hD - hU));
}

TerrainChunk* TerrainGenerator::GetOrCreateChunk(int x, int z) {
    ChunkKey key{x, z};
    auto it = m_chunks.find(key);

    if (it != m_chunks.end()) {
        return it->second.get();
    }

    auto chunk = std::make_unique<TerrainChunk>(x, z, m_chunkSize, m_chunkScale);
    chunk->Generate(m_frequency, m_heightScale, m_octaves, m_persistence, m_lacunarity);
    chunk->CreateMesh();

    TerrainChunk* ptr = chunk.get();
    m_chunks[key] = std::move(chunk);

    return ptr;
}

void TerrainGenerator::SetNoiseParams(float frequency, int octaves,
                                       float persistence, float lacunarity) {
    m_frequency = frequency;
    m_octaves = octaves;
    m_persistence = persistence;
    m_lacunarity = lacunarity;

    // Clear existing chunks to regenerate
    m_chunks.clear();
    m_lastViewerChunk = glm::ivec2(INT_MAX, INT_MAX);
}

} // namespace Nova
