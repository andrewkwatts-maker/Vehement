#include "terrain/TerrainGenerator.hpp"
#include "terrain/NoiseGenerator.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Shader.hpp"
#include "config/Config.hpp"

#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <execution>
#include <thread>

namespace Nova {

// ============================================================================
// TerrainChunk Implementation
// ============================================================================

TerrainChunk::TerrainChunk(int x, int z, int size, float scale)
    : m_coord(x, z)
    , m_size(size)
    , m_effectiveResolution(size)
    , m_scale(scale)
{
    m_heights.resize(static_cast<size_t>((size + 1)) * (size + 1));
}

void TerrainChunk::Generate(float frequency, float amplitude, int octaves,
                             float persistence, float lacunarity) {
    GenerateWithLOD(frequency, amplitude, octaves, persistence, lacunarity, m_size);
}

void TerrainChunk::GenerateWithLOD(float frequency, float amplitude, int octaves,
                                    float persistence, float lacunarity, int lodResolution) {
    m_state.store(ChunkState::Generating);

    const int resolution = std::max(1, lodResolution);
    const int step = m_size / resolution;
    const int gridSize = resolution + 1;

    std::vector<float> newHeights(static_cast<size_t>(gridSize) * gridSize);

    const float worldX = static_cast<float>(m_coord.x * m_size) * m_scale;
    const float worldZ = static_cast<float>(m_coord.y * m_size) * m_scale;

    // Generate heights - can be parallelized for large chunks
    if (resolution >= 32) {
        // Parallel generation for larger chunks
        std::vector<int> rows(gridSize);
        std::iota(rows.begin(), rows.end(), 0);

        std::for_each(std::execution::par_unseq, rows.begin(), rows.end(),
            [&](int z) {
                for (int x = 0; x <= resolution; ++x) {
                    const float sampleX = (worldX + x * step * m_scale) * frequency;
                    const float sampleZ = (worldZ + z * step * m_scale) * frequency;

                    const float height = NoiseGenerator::FractalNoise(
                        sampleX, sampleZ, octaves, persistence, lacunarity);

                    newHeights[static_cast<size_t>(z) * gridSize + x] = height * amplitude;
                }
            });
    } else {
        // Sequential for smaller chunks (overhead not worth it)
        for (int z = 0; z <= resolution; ++z) {
            for (int x = 0; x <= resolution; ++x) {
                const float sampleX = (worldX + x * step * m_scale) * frequency;
                const float sampleZ = (worldZ + z * step * m_scale) * frequency;

                const float height = NoiseGenerator::FractalNoise(
                    sampleX, sampleZ, octaves, persistence, lacunarity);

                newHeights[static_cast<size_t>(z) * gridSize + x] = height * amplitude;
            }
        }
    }

    // Update heights with write lock
    {
        std::unique_lock lock(m_heightsMutex);
        m_heights = std::move(newHeights);
        m_effectiveResolution = resolution;
    }

    m_state.store(ChunkState::Generated);
}

void TerrainChunk::CreateMesh() {
    CreateMeshWithLOD(0);
}

void TerrainChunk::CreateMeshWithLOD(int lodLevel) {
    std::shared_lock lock(m_heightsMutex);

    const int resolution = m_effectiveResolution;
    const int gridSize = resolution + 1;
    const int step = m_size / resolution;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Reserve memory to avoid reallocations
    vertices.reserve(static_cast<size_t>(gridSize) * gridSize);
    indices.reserve(static_cast<size_t>(resolution) * resolution * 6);

    const float worldX = static_cast<float>(m_coord.x * m_size) * m_scale;
    const float worldZ = static_cast<float>(m_coord.y * m_size) * m_scale;

    // Create vertices
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            const size_t idx = static_cast<size_t>(z) * gridSize + x;
            const float height = m_heights[idx];

            Vertex vertex{};
            vertex.position = glm::vec3(
                worldX + x * step * m_scale,
                height,
                worldZ + z * step * m_scale
            );

            // Calculate normal using central differences
            float hL = (x > 0) ? m_heights[idx - 1] : height;
            float hR = (x < resolution) ? m_heights[idx + 1] : height;
            float hD = (z > 0) ? m_heights[idx - gridSize] : height;
            float hU = (z < resolution) ? m_heights[idx + gridSize] : height;

            vertex.normal = glm::normalize(glm::vec3(
                hL - hR,
                2.0f * step * m_scale,
                hD - hU
            ));

            vertex.texCoords = glm::vec2(
                static_cast<float>(x) / resolution,
                static_cast<float>(z) / resolution
            );

            // Compute tangent basis
            vertex.tangent = glm::normalize(glm::vec3(1.0f, (hR - hL) / (2.0f * step * m_scale), 0.0f));
            vertex.bitangent = glm::cross(vertex.normal, vertex.tangent);

            vertices.push_back(vertex);
        }
    }

    // Create indices with correct winding order
    for (int z = 0; z < resolution; ++z) {
        for (int x = 0; x < resolution; ++x) {
            const uint32_t topLeft = static_cast<uint32_t>(z * gridSize + x);
            const uint32_t topRight = topLeft + 1;
            const uint32_t bottomLeft = static_cast<uint32_t>((z + 1) * gridSize + x);
            const uint32_t bottomRight = bottomLeft + 1;

            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    lock.unlock();

    m_mesh = std::make_shared<Mesh>();
    m_mesh->Create(vertices, indices);
    m_currentLOD = lodLevel;
    m_state.store(ChunkState::Ready);
}

void TerrainChunk::UpdateLOD(const glm::vec3& viewerPosition,
                              const std::vector<TerrainLOD>& lodLevels) {
    if (lodLevels.empty()) return;

    const float distance = GetDistanceToViewer(viewerPosition);

    int targetLOD = static_cast<int>(lodLevels.size()) - 1;
    for (size_t i = 0; i < lodLevels.size(); ++i) {
        if (distance <= lodLevels[i].maxDistance) {
            targetLOD = static_cast<int>(i);
            break;
        }
    }

    if (targetLOD != m_currentLOD && m_state.load() == ChunkState::Ready) {
        // LOD change needed - would require regeneration
        m_currentLOD = targetLOD;
    }
}

float TerrainChunk::GetHeight(float worldX, float worldZ) const {
    std::shared_lock lock(m_heightsMutex);

    const int resolution = m_effectiveResolution;
    const int gridSize = resolution + 1;
    const int step = m_size / resolution;

    const float chunkX = static_cast<float>(m_coord.x * m_size) * m_scale;
    const float chunkZ = static_cast<float>(m_coord.y * m_size) * m_scale;

    float localX = (worldX - chunkX) / (step * m_scale);
    float localZ = (worldZ - chunkZ) / (step * m_scale);

    int x0 = static_cast<int>(std::floor(localX));
    int z0 = static_cast<int>(std::floor(localZ));

    x0 = std::clamp(x0, 0, resolution - 1);
    z0 = std::clamp(z0, 0, resolution - 1);

    const float tx = localX - static_cast<float>(x0);
    const float tz = localZ - static_cast<float>(z0);

    const size_t baseIdx = static_cast<size_t>(z0) * gridSize + x0;
    const float h00 = m_heights[baseIdx];
    const float h10 = m_heights[baseIdx + 1];
    const float h01 = m_heights[baseIdx + gridSize];
    const float h11 = m_heights[baseIdx + gridSize + 1];

    // Bilinear interpolation
    const float h0 = h00 + (h10 - h00) * tx;
    const float h1 = h01 + (h11 - h01) * tx;

    return h0 + (h1 - h0) * tz;
}

glm::vec3 TerrainChunk::GetNormal(float worldX, float worldZ) const {
    constexpr float delta = 0.5f;

    const float hL = GetHeight(worldX - delta, worldZ);
    const float hR = GetHeight(worldX + delta, worldZ);
    const float hD = GetHeight(worldX, worldZ - delta);
    const float hU = GetHeight(worldX, worldZ + delta);

    return glm::normalize(glm::vec3(hL - hR, 2.0f * delta, hD - hU));
}

glm::vec3 TerrainChunk::GetWorldCenter() const noexcept {
    const float halfSize = static_cast<float>(m_size) * m_scale * 0.5f;
    return glm::vec3(
        static_cast<float>(m_coord.x * m_size) * m_scale + halfSize,
        0.0f,
        static_cast<float>(m_coord.y * m_size) * m_scale + halfSize
    );
}

float TerrainChunk::GetDistanceToViewer(const glm::vec3& viewerPos) const noexcept {
    const glm::vec3 center = GetWorldCenter();
    const glm::vec2 diff(viewerPos.x - center.x, viewerPos.z - center.z);
    return std::sqrt(diff.x * diff.x + diff.y * diff.y);
}

// ============================================================================
// TerrainGenerator Implementation
// ============================================================================

TerrainGenerator::TerrainGenerator() {
    // Setup default LOD levels
    m_lodLevels = {
        {64, 100.0f},   // LOD 0: Full resolution up to 100 units
        {32, 200.0f},   // LOD 1: Half resolution up to 200 units
        {16, 400.0f},   // LOD 2: Quarter resolution up to 400 units
        {8, 800.0f}     // LOD 3: Eighth resolution beyond
    };
}

TerrainGenerator::~TerrainGenerator() {
    Shutdown();
}

bool TerrainGenerator::Initialize() {
    auto& config = Config::Instance();

    m_chunkSize = config.Get("terrain.chunk_size", 64);
    m_viewDistance = config.Get("terrain.view_distance", 4);
    m_heightScale = config.Get("terrain.height_scale", 50.0f);
    m_frequency = config.Get("terrain.noise_frequency", 0.02f);
    m_octaves = config.Get("terrain.octaves", 6);
    m_persistence = config.Get("terrain.persistence", 0.5f);
    m_lacunarity = config.Get("terrain.lacunarity", 2.0f);

    // Calculate auto unload distance
    m_unloadDistance = static_cast<float>(m_viewDistance + 2) * m_chunkSize * m_chunkScale;

    spdlog::info("TerrainGenerator initialized: chunk_size={}, view_distance={}, height_scale={}",
                 m_chunkSize, m_viewDistance, m_heightScale);

    return true;
}

void TerrainGenerator::Shutdown() {
    m_shutdown.store(true);

    // Wait for all pending generations to complete
    for (auto& future : m_generationFutures) {
        if (future.valid()) {
            future.wait();
        }
    }
    m_generationFutures.clear();

    // Clear pending mesh queue
    {
        std::lock_guard lock(m_pendingMeshMutex);
        std::queue<TerrainChunk*> empty;
        std::swap(m_pendingMeshQueue, empty);
    }

    // Clear chunks
    {
        std::unique_lock lock(m_chunksMutex);
        m_chunks.clear();
    }

    m_visibleChunks.clear();
    m_shutdown.store(false);
}

void TerrainGenerator::Update(const glm::vec3& viewerPosition) {
    const int chunkX = static_cast<int>(std::floor(viewerPosition.x / (m_chunkSize * m_chunkScale)));
    const int chunkZ = static_cast<int>(std::floor(viewerPosition.z / (m_chunkSize * m_chunkScale)));

    m_lastViewerPosition = viewerPosition;

    // Check if viewer moved to different chunk
    if (chunkX == m_lastViewerChunk.x && chunkZ == m_lastViewerChunk.y) {
        // Still update LOD for visible chunks
        for (auto* chunk : m_visibleChunks) {
            if (chunk) {
                chunk->UpdateLOD(viewerPosition, m_lodLevels);
            }
        }
        return;
    }

    m_lastViewerChunk = glm::ivec2(chunkX, chunkZ);
    m_visibleChunks.clear();

    // Collect chunks in view distance, prioritize by distance
    struct ChunkRequest {
        int x, z;
        float priority;
    };
    std::vector<ChunkRequest> requests;

    for (int z = chunkZ - m_viewDistance; z <= chunkZ + m_viewDistance; ++z) {
        for (int x = chunkX - m_viewDistance; x <= chunkX + m_viewDistance; ++x) {
            const float dx = static_cast<float>(x - chunkX);
            const float dz = static_cast<float>(z - chunkZ);
            const float dist = std::sqrt(dx * dx + dz * dz);

            // Skip corners beyond circular view distance
            if (dist > static_cast<float>(m_viewDistance) + 0.5f) {
                continue;
            }

            requests.push_back({x, z, dist});
        }
    }

    // Sort by priority (closer chunks first)
    std::sort(requests.begin(), requests.end(),
              [](const ChunkRequest& a, const ChunkRequest& b) {
                  return a.priority < b.priority;
              });

    // Process chunks
    for (const auto& req : requests) {
        TerrainChunk* chunk = GetOrCreateChunk(req.x, req.z);
        if (chunk) {
            m_visibleChunks.push_back(chunk);
            chunk->UpdateLOD(viewerPosition, m_lodLevels);
        }
    }

    // Cleanup completed futures
    m_generationFutures.erase(
        std::remove_if(m_generationFutures.begin(), m_generationFutures.end(),
            [](std::future<void>& f) {
                return f.valid() &&
                       f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }),
        m_generationFutures.end()
    );

    // Unload distant chunks periodically
    UnloadDistantChunks(m_unloadDistance);
}

void TerrainGenerator::ProcessPendingMeshes(int maxChunksPerFrame) {
    int processed = 0;

    while (processed < maxChunksPerFrame) {
        TerrainChunk* chunk = nullptr;

        {
            std::lock_guard lock(m_pendingMeshMutex);
            if (m_pendingMeshQueue.empty()) {
                break;
            }
            chunk = m_pendingMeshQueue.front();
            m_pendingMeshQueue.pop();
        }

        if (chunk && chunk->GetState() == ChunkState::MeshPending) {
            chunk->CreateMesh();
            ++processed;
        }
    }
}

void TerrainGenerator::Render(Shader& shader) {
    for (TerrainChunk* chunk : m_visibleChunks) {
        if (chunk && chunk->IsReady() && chunk->GetMesh()) {
            shader.SetMat4("u_Model", glm::mat4(1.0f));
            chunk->GetMesh()->Draw();
        }
    }
}

float TerrainGenerator::GetHeightAt(float x, float z) const {
    const int chunkX = static_cast<int>(std::floor(x / (m_chunkSize * m_chunkScale)));
    const int chunkZ = static_cast<int>(std::floor(z / (m_chunkSize * m_chunkScale)));

    ChunkKey key{chunkX, chunkZ};

    std::shared_lock lock(m_chunksMutex);
    auto it = m_chunks.find(key);
    if (it != m_chunks.end() && it->second->GetState() != ChunkState::Unloaded) {
        return it->second->GetHeight(x, z);
    }

    return 0.0f;
}

glm::vec3 TerrainGenerator::GetNormalAt(float x, float z) const {
    constexpr float delta = 0.5f;

    const float hL = GetHeightAt(x - delta, z);
    const float hR = GetHeightAt(x + delta, z);
    const float hD = GetHeightAt(x, z - delta);
    const float hU = GetHeightAt(x, z + delta);

    return glm::normalize(glm::vec3(hL - hR, 2.0f * delta, hD - hU));
}

TerrainGenerator::Stats TerrainGenerator::GetStats() const {
    Stats stats{};

    std::shared_lock lock(m_chunksMutex);
    stats.totalChunks = m_chunks.size();
    stats.visibleChunks = m_visibleChunks.size();
    stats.generatingChunks = static_cast<size_t>(m_activeGenerations.load());

    {
        std::lock_guard meshLock(m_pendingMeshMutex);
        stats.pendingChunks = m_pendingMeshQueue.size();
    }

    return stats;
}

TerrainChunk* TerrainGenerator::GetOrCreateChunk(int x, int z) {
    ChunkKey key{x, z};

    // Try to find existing chunk with read lock
    {
        std::shared_lock lock(m_chunksMutex);
        auto it = m_chunks.find(key);
        if (it != m_chunks.end()) {
            return it->second.get();
        }
    }

    // Need to create new chunk with write lock
    {
        std::unique_lock lock(m_chunksMutex);

        // Double-check after acquiring write lock
        auto it = m_chunks.find(key);
        if (it != m_chunks.end()) {
            return it->second.get();
        }

        auto chunk = std::make_unique<TerrainChunk>(x, z, m_chunkSize, m_chunkScale);
        TerrainChunk* ptr = chunk.get();
        m_chunks[key] = std::move(chunk);

        // Queue for async generation
        QueueChunkGeneration(ptr);

        return ptr;
    }
}

void TerrainGenerator::QueueChunkGeneration(TerrainChunk* chunk) {
    if (m_shutdown.load()) return;

    // Limit concurrent generations
    if (m_activeGenerations.load() >= MAX_CONCURRENT_GENERATIONS) {
        // Generate synchronously if too many pending
        GenerateChunkAsync(chunk);
        return;
    }

    m_activeGenerations.fetch_add(1);

    auto future = std::async(std::launch::async, [this, chunk]() {
        GenerateChunkAsync(chunk);
        m_activeGenerations.fetch_sub(1);
    });

    m_generationFutures.push_back(std::move(future));
}

void TerrainGenerator::GenerateChunkAsync(TerrainChunk* chunk) {
    if (m_shutdown.load()) return;

    chunk->Generate(m_frequency, m_heightScale, m_octaves, m_persistence, m_lacunarity);

    // Queue for mesh creation on main thread
    chunk->SetState(ChunkState::MeshPending);

    {
        std::lock_guard lock(m_pendingMeshMutex);
        m_pendingMeshQueue.push(chunk);
    }
}

float TerrainGenerator::CalculateChunkPriority(int chunkX, int chunkZ) const {
    const float dx = static_cast<float>(chunkX) - m_lastViewerChunk.x;
    const float dz = static_cast<float>(chunkZ) - m_lastViewerChunk.y;
    return std::sqrt(dx * dx + dz * dz);
}

void TerrainGenerator::UnloadDistantChunks(float maxDistance) {
    std::unique_lock lock(m_chunksMutex);

    const float maxDistSq = maxDistance * maxDistance;

    for (auto it = m_chunks.begin(); it != m_chunks.end(); ) {
        const auto& chunk = it->second;
        const float dist = chunk->GetDistanceToViewer(m_lastViewerPosition);

        if (dist * dist > maxDistSq && chunk->GetState() == ChunkState::Ready) {
            it = m_chunks.erase(it);
        } else {
            ++it;
        }
    }
}

void TerrainGenerator::ClearAllChunks() {
    Shutdown();

    {
        std::unique_lock lock(m_chunksMutex);
        m_chunks.clear();
    }

    m_lastViewerChunk = glm::ivec2(INT_MAX, INT_MAX);
}

void TerrainGenerator::SetNoiseParams(float frequency, int octaves,
                                       float persistence, float lacunarity) {
    m_frequency = frequency;
    m_octaves = octaves;
    m_persistence = persistence;
    m_lacunarity = lacunarity;

    // Clear existing chunks to regenerate with new params
    ClearAllChunks();
}

} // namespace Nova
