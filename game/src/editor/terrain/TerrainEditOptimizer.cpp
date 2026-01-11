#include "TerrainEditOptimizer.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace Vehement {

// =============================================================================
// TerrainEditOptimizer
// =============================================================================

TerrainEditOptimizer::TerrainEditOptimizer() = default;

TerrainEditOptimizer::~TerrainEditOptimizer() {
    Shutdown();
}

void TerrainEditOptimizer::Initialize(Nova::VoxelTerrain* terrain, const Config& config) {
    m_terrain = terrain;
    m_config = config;

    // Start worker threads
    if (m_config.enableAsyncProcessing) {
        m_running = true;
        for (int i = 0; i < m_config.workerThreads; ++i) {
            m_workerThreads.emplace_back(&TerrainEditOptimizer::WorkerThreadFunc, this);
        }
    }

    m_initialized = true;
    spdlog::info("TerrainEditOptimizer initialized with {} worker threads", m_config.workerThreads);
}

void TerrainEditOptimizer::Shutdown() {
    if (!m_initialized) return;

    // Stop worker threads
    m_running = false;
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();

    // Clear queues
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_editQueue.empty()) {
            m_editQueue.pop();
        }
    }

    m_currentBatch.clear();
    m_affectedChunks.clear();

    m_initialized = false;
    spdlog::info("TerrainEditOptimizer shutdown");
}

void TerrainEditOptimizer::QueueEdit(const TerrainEditJob& job) {
    if (!m_initialized) return;

    std::lock_guard<std::mutex> lock(m_queueMutex);

    // Check queue size limit
    if (m_editQueue.size() >= static_cast<size_t>(m_config.maxJobQueueSize)) {
        spdlog::warn("Edit queue full, dropping oldest edit");
        m_editQueue.pop();
    }

    m_editQueue.push(job);
}

void TerrainEditOptimizer::ProcessEdits(float deltaTime, const glm::vec3& cameraPosition) {
    if (!m_initialized) return;

    m_batchTimer += deltaTime;

    // Process batches at intervals
    if (m_config.enableBatching && m_batchTimer >= m_config.batchInterval) {
        m_batchTimer = 0.0f;

        // Pull edits from queue into batch
        std::lock_guard<std::mutex> lock(m_queueMutex);

        int count = 0;
        while (!m_editQueue.empty() && count < m_config.maxEditsPerBatch) {
            m_currentBatch.push_back(m_editQueue.front());
            m_editQueue.pop();
            count++;
        }

        // Coalesce edits if enabled
        if (m_config.enableEditCoalescing && m_currentBatch.size() > 1) {
            CoalesceEdits();
        }

        // Process batch
        for (const auto& job : m_currentBatch) {
            ProcessEdit(job);
        }

        m_currentBatch.clear();
    }
}

void TerrainEditOptimizer::FlushEdits() {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    // Process all queued edits immediately
    while (!m_editQueue.empty()) {
        ProcessEdit(m_editQueue.front());
        m_editQueue.pop();
    }

    // Process current batch
    for (const auto& job : m_currentBatch) {
        ProcessEdit(job);
    }
    m_currentBatch.clear();
}

size_t TerrainEditOptimizer::GetPendingEditCount() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_editQueue.size() + m_currentBatch.size();
}

void TerrainEditOptimizer::ClearAffectedChunks() {
    m_affectedChunks.clear();
}

int TerrainEditOptimizer::CalculateLOD(float distance) const {
    if (!m_config.enableLOD) return 0;

    if (distance < m_config.lodDistance0) return 0;
    if (distance < m_config.lodDistance1) return 1;
    if (distance < m_config.lodDistance2) return 2;
    return 3;
}

bool TerrainEditOptimizer::ShouldUseLOD(const glm::vec3& position, const glm::vec3& cameraPosition) const {
    if (!m_config.enableLOD) return false;

    float distance = glm::length(position - cameraPosition);
    return distance > m_config.lodDistance0;
}

void TerrainEditOptimizer::ProcessEdit(const TerrainEditJob& job) {
    if (!m_terrain) return;

    // Execute the operation
    if (job.operation) {
        job.operation();
    }

    // Track affected chunks
    glm::ivec3 minChunk = WorldToChunk(job.minBounds);
    glm::ivec3 maxChunk = WorldToChunk(job.maxBounds);

    for (int z = minChunk.z; z <= maxChunk.z; ++z) {
        for (int y = minChunk.y; y <= maxChunk.y; ++y) {
            for (int x = minChunk.x; x <= maxChunk.x; ++x) {
                m_affectedChunks.insert(glm::ivec3(x, y, z));
            }
        }
    }
}

void TerrainEditOptimizer::CoalesceEdits() {
    // Simple coalescing: remove duplicate edits at same position
    std::unordered_map<glm::ivec3, TerrainEditJob> uniqueEdits;

    for (const auto& job : m_currentBatch) {
        glm::ivec3 pos(
            static_cast<int>(job.position.x / m_config.coalescingRadius),
            static_cast<int>(job.position.y / m_config.coalescingRadius),
            static_cast<int>(job.position.z / m_config.coalescingRadius)
        );

        auto it = uniqueEdits.find(pos);
        if (it == uniqueEdits.end()) {
            uniqueEdits[pos] = job;
        } else {
            // Keep the one with higher priority or newer timestamp
            if (job.priority > it->second.priority || job.timestamp > it->second.timestamp) {
                it->second = job;
            }
        }
    }

    // Replace batch with coalesced edits
    m_currentBatch.clear();
    for (const auto& pair : uniqueEdits) {
        m_currentBatch.push_back(pair.second);
    }
}

void TerrainEditOptimizer::WorkerThreadFunc() {
    while (m_running) {
        // Process jobs from queue in background
        std::unique_lock<std::mutex> lock(m_queueMutex);

        if (!m_editQueue.empty()) {
            TerrainEditJob job = m_editQueue.front();
            m_editQueue.pop();
            lock.unlock();

            ProcessEdit(job);
        } else {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

glm::ivec3 TerrainEditOptimizer::WorldToChunk(const glm::vec3& worldPos) const {
    if (!m_terrain) return glm::ivec3(0);

    int chunkSize = Nova::VoxelChunk::SIZE;
    return glm::ivec3(
        static_cast<int>(std::floor(worldPos.x / chunkSize)),
        static_cast<int>(std::floor(worldPos.y / chunkSize)),
        static_cast<int>(std::floor(worldPos.z / chunkSize))
    );
}

// =============================================================================
// EditSpatialHash
// =============================================================================

EditSpatialHash::EditSpatialHash(float cellSize)
    : m_cellSize(cellSize) {
}

void EditSpatialHash::AddEdit(const glm::vec3& position, const TerrainEditJob& job) {
    glm::ivec3 cell = WorldToCell(position);
    uint64_t hash = HashCell(cell);
    m_cells[hash].edits.push_back(job);
}

std::vector<TerrainEditJob*> EditSpatialHash::GetNearbyEdits(const glm::vec3& position, float radius) {
    std::vector<TerrainEditJob*> nearby;

    glm::ivec3 center = WorldToCell(position);
    int cellRadius = static_cast<int>(std::ceil(radius / m_cellSize));

    for (int z = -cellRadius; z <= cellRadius; ++z) {
        for (int y = -cellRadius; y <= cellRadius; ++y) {
            for (int x = -cellRadius; x <= cellRadius; ++x) {
                glm::ivec3 cell = center + glm::ivec3(x, y, z);
                uint64_t hash = HashCell(cell);

                auto it = m_cells.find(hash);
                if (it != m_cells.end()) {
                    for (auto& edit : it->second.edits) {
                        nearby.push_back(&edit);
                    }
                }
            }
        }
    }

    return nearby;
}

void EditSpatialHash::Clear() {
    m_cells.clear();
}

glm::ivec3 EditSpatialHash::WorldToCell(const glm::vec3& position) const {
    return glm::ivec3(
        static_cast<int>(std::floor(position.x / m_cellSize)),
        static_cast<int>(std::floor(position.y / m_cellSize)),
        static_cast<int>(std::floor(position.z / m_cellSize))
    );
}

uint64_t EditSpatialHash::HashCell(const glm::ivec3& cell) const {
    return ((static_cast<uint64_t>(cell.x) * 73856093) ^
            (static_cast<uint64_t>(cell.y) * 19349663) ^
            (static_cast<uint64_t>(cell.z) * 83492791));
}

// =============================================================================
// ChunkDirtyTracker
// =============================================================================

void ChunkDirtyTracker::MarkDirty(const glm::ivec3& chunkPos) {
    m_dirtyChunks.insert(chunkPos);
}

void ChunkDirtyTracker::MarkRegionDirty(const glm::vec3& minBounds, const glm::vec3& maxBounds) {
    int chunkSize = Nova::VoxelChunk::SIZE;

    glm::ivec3 minChunk(
        static_cast<int>(std::floor(minBounds.x / chunkSize)),
        static_cast<int>(std::floor(minBounds.y / chunkSize)),
        static_cast<int>(std::floor(minBounds.z / chunkSize))
    );

    glm::ivec3 maxChunk(
        static_cast<int>(std::floor(maxBounds.x / chunkSize)),
        static_cast<int>(std::floor(maxBounds.y / chunkSize)),
        static_cast<int>(std::floor(maxBounds.z / chunkSize))
    );

    for (int z = minChunk.z; z <= maxChunk.z; ++z) {
        for (int y = minChunk.y; y <= maxChunk.y; ++y) {
            for (int x = minChunk.x; x <= maxChunk.x; ++x) {
                m_dirtyChunks.insert(glm::ivec3(x, y, z));
            }
        }
    }
}

void ChunkDirtyTracker::Clear() {
    m_dirtyChunks.clear();
}

std::vector<glm::ivec3> ChunkDirtyTracker::GetSortedDirtyChunks(const glm::vec3& cameraPosition) const {
    std::vector<glm::ivec3> sorted(m_dirtyChunks.begin(), m_dirtyChunks.end());

    int chunkSize = Nova::VoxelChunk::SIZE;

    std::sort(sorted.begin(), sorted.end(),
        [&cameraPosition, chunkSize](const glm::ivec3& a, const glm::ivec3& b) {
            glm::vec3 aCenter = glm::vec3(a) * static_cast<float>(chunkSize) + glm::vec3(chunkSize / 2);
            glm::vec3 bCenter = glm::vec3(b) * static_cast<float>(chunkSize) + glm::vec3(chunkSize / 2);

            float distA = glm::distance(cameraPosition, aCenter);
            float distB = glm::distance(cameraPosition, bCenter);

            return distA < distB;  // Closer chunks first
        });

    return sorted;
}

} // namespace Vehement
