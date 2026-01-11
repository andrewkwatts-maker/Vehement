#include "ChunkStreamer.hpp"
#include <chrono>
#include <algorithm>

namespace Nova {

static uint64_t GetTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

ChunkStreamer::ChunkStreamer() = default;

ChunkStreamer::~ChunkStreamer() {
    Shutdown();
}

bool ChunkStreamer::Initialize(WorldDatabase* database, int ioThreadCount) {
    if (!database || !database->IsInitialized()) {
        return false;
    }

    m_database = database;
    m_ioRunning = true;

    // Start I/O threads
    for (int i = 0; i < ioThreadCount; i++) {
        m_ioThreads.emplace_back(&ChunkStreamer::IOThreadFunc, this);
    }

    return true;
}

void ChunkStreamer::Shutdown() {
    if (!m_ioRunning) return;

    // Save all dirty chunks
    SaveAllDirtyChunks(true);

    // Stop I/O threads
    m_ioRunning = false;
    m_ioCondition.notify_all();

    for (auto& thread : m_ioThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    m_ioThreads.clear();
    m_database = nullptr;
}

void ChunkStreamer::Update(float deltaTime) {
    if (!m_database) return;

    m_frameCounter++;

    // Process chunk loads/unloads based on view positions
    if (m_frameCounter % 10 == 0) { // Every 10 frames
        UpdateLoadedChunks();
    }

    // Auto-save
    if (m_autoSaveEnabled) {
        m_autoSaveTimer += deltaTime;
        if (m_autoSaveTimer >= m_autoSaveInterval) {
            SaveAllDirtyChunks(false);
            m_autoSaveTimer = 0.0f;
        }
    }

    // LRU cache management
    if (m_frameCounter % 100 == 0) { // Every 100 frames
        EvictLRUChunks();
    }
}

void ChunkStreamer::SetViewPosition(glm::vec3 position, int playerId) {
    std::lock_guard<std::mutex> lock(m_viewMutex);
    if (playerId < 0) playerId = 0;
    m_viewPositions[playerId] = position;
}

void ChunkStreamer::SetViewDistance(float distance) {
    m_viewDistance = std::max(distance, 32.0f);
}

void ChunkStreamer::AddViewPosition(int playerId, glm::vec3 position) {
    std::lock_guard<std::mutex> lock(m_viewMutex);
    m_viewPositions[playerId] = position;
}

void ChunkStreamer::RemoveViewPosition(int playerId) {
    std::lock_guard<std::mutex> lock(m_viewMutex);
    m_viewPositions.erase(playerId);
}

void ChunkStreamer::LoadChunk(glm::ivec3 chunkPos, int priority, std::function<void(bool)> callback) {
    {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        if (m_chunkStates[chunkPos] == ChunkLoadState::Loaded ||
            m_chunkStates[chunkPos] == ChunkLoadState::Loading) {
            if (callback) callback(true);
            return;
        }
        m_chunkStates[chunkPos] = ChunkLoadState::Queued;
    }

    ChunkIORequest request;
    request.type = ChunkIORequestType::Load;
    request.chunkPos = chunkPos;
    request.priority = priority;
    request.timestamp = GetTimestamp();
    request.callback = callback;

    {
        std::lock_guard<std::mutex> lock(m_ioMutex);
        m_ioQueue.push(request);
        m_stats.pendingLoads++;
    }
    m_ioCondition.notify_one();
}

void ChunkStreamer::SaveChunk(glm::ivec3 chunkPos, const ChunkData& data, std::function<void(bool)> callback) {
    {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        m_loadedChunks[chunkPos] = data;
        m_chunkStates[chunkPos] = ChunkLoadState::Dirty;
        m_dirtyChunks.insert(chunkPos);
    }

    ChunkIORequest request;
    request.type = ChunkIORequestType::Save;
    request.chunkPos = chunkPos;
    request.data = data;
    request.priority = 50; // Medium priority
    request.timestamp = GetTimestamp();
    request.callback = callback;

    {
        std::lock_guard<std::mutex> lock(m_ioMutex);
        m_ioQueue.push(request);
        m_stats.pendingSaves++;
    }
    m_ioCondition.notify_one();
}

void ChunkStreamer::UnloadChunk(glm::ivec3 chunkPos, bool saveIfDirty) {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    if (saveIfDirty && m_dirtyChunks.count(chunkPos)) {
        auto it = m_loadedChunks.find(chunkPos);
        if (it != m_loadedChunks.end()) {
            m_chunkMutex.unlock();
            SaveChunk(chunkPos, it->second);
            m_chunkMutex.lock();
        }
    }

    m_loadedChunks.erase(chunkPos);
    m_chunkStates.erase(chunkPos);
    m_dirtyChunks.erase(chunkPos);
    m_chunkAccessTime.erase(chunkPos);

    if (OnChunkUnloaded) {
        OnChunkUnloaded(chunkPos);
    }
}

void ChunkStreamer::MarkChunkDirty(glm::ivec3 chunkPos) {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    m_dirtyChunks.insert(chunkPos);
    m_chunkStates[chunkPos] = ChunkLoadState::Dirty;
}

bool ChunkStreamer::IsChunkLoaded(glm::ivec3 chunkPos) const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    return m_loadedChunks.count(chunkPos) > 0;
}

ChunkData* ChunkStreamer::GetChunk(glm::ivec3 chunkPos) {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_loadedChunks.find(chunkPos);
    if (it != m_loadedChunks.end()) {
        UpdateChunkAccess(chunkPos);
        return &it->second;
    }
    return nullptr;
}

const ChunkData* ChunkStreamer::GetChunk(glm::ivec3 chunkPos) const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_loadedChunks.find(chunkPos);
    return (it != m_loadedChunks.end()) ? &it->second : nullptr;
}

ChunkLoadState ChunkStreamer::GetChunkState(glm::ivec3 chunkPos) const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_chunkStates.find(chunkPos);
    return (it != m_chunkStates.end()) ? it->second : ChunkLoadState::Unloaded;
}

void ChunkStreamer::SaveAllDirtyChunks(bool blocking) {
    std::vector<glm::ivec3> dirtyChunks;
    {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        dirtyChunks.assign(m_dirtyChunks.begin(), m_dirtyChunks.end());
    }

    for (const auto& chunkPos : dirtyChunks) {
        auto* chunk = GetChunk(chunkPos);
        if (chunk) {
            SaveChunk(chunkPos, *chunk);
        }
    }

    if (blocking) {
        // Wait for all saves to complete
        while (GetPendingRequestCount() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void ChunkStreamer::UnloadAllChunks(bool saveFirst) {
    if (saveFirst) {
        SaveAllDirtyChunks(true);
    }

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    m_loadedChunks.clear();
    m_chunkStates.clear();
    m_dirtyChunks.clear();
    m_chunkAccessTime.clear();
}

void ChunkStreamer::PreloadChunksInRadius(glm::vec3 center, float radius) {
    glm::ivec3 centerChunk = WorldToChunkPos(center);
    int chunkRadius = static_cast<int>(std::ceil(radius / m_chunkSize));

    for (int x = -chunkRadius; x <= chunkRadius; x++) {
        for (int z = -chunkRadius; z <= chunkRadius; z++) {
            for (int y = -2; y <= 2; y++) { // Load a few Y levels
                glm::ivec3 chunkPos = centerChunk + glm::ivec3(x, y, z);
                float dist = GetChunkDistance(chunkPos, center);
                if (dist <= radius) {
                    int priority = static_cast<int>((1.0f - dist / radius) * 100);
                    LoadChunk(chunkPos, priority);
                }
            }
        }
    }
}

void ChunkStreamer::SetAutoSaveEnabled(bool enabled) {
    m_autoSaveEnabled = enabled;
}

void ChunkStreamer::SetAutoSaveInterval(float seconds) {
    m_autoSaveInterval = std::max(seconds, 10.0f);
}

void ChunkStreamer::SetMaxCachedChunks(size_t maxChunks) {
    m_maxCachedChunks = std::max(maxChunks, size_t(100));
}

void ChunkStreamer::ClearCache(bool saveFirst) {
    if (saveFirst) {
        SaveAllDirtyChunks(true);
    }

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    m_loadedChunks.clear();
    m_chunkStates.clear();
    m_dirtyChunks.clear();
    m_chunkAccessTime.clear();
}

ChunkStreamStats ChunkStreamer::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    ChunkStreamStats stats = m_stats;
    stats.loadedChunks = GetLoadedChunkCount();
    stats.dirtyChunks = GetDirtyChunkCount();

    if (m_totalLoads > 0) {
        stats.avgLoadTime = m_totalLoadTime / static_cast<float>(m_totalLoads);
    }
    if (m_totalSaves > 0) {
        stats.avgSaveTime = m_totalSaveTime / static_cast<float>(m_totalSaves);
    }

    return stats;
}

size_t ChunkStreamer::GetLoadedChunkCount() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    return m_loadedChunks.size();
}

size_t ChunkStreamer::GetDirtyChunkCount() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    return m_dirtyChunks.size();
}

size_t ChunkStreamer::GetPendingRequestCount() const {
    std::lock_guard<std::mutex> lock(m_ioMutex);
    return m_ioQueue.size();
}

void ChunkStreamer::IOThreadFunc() {
    while (m_ioRunning) {
        ChunkIORequest request;

        {
            std::unique_lock<std::mutex> lock(m_ioMutex);
            m_ioCondition.wait(lock, [this] { return !m_ioQueue.empty() || !m_ioRunning; });

            if (!m_ioRunning) break;
            if (m_ioQueue.empty()) continue;

            request = m_ioQueue.top();
            m_ioQueue.pop();
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        bool success = false;

        switch (request.type) {
            case ChunkIORequestType::Load: {
                ChunkData chunk = m_database->LoadChunk(request.chunkPos.x, request.chunkPos.y, request.chunkPos.z);
                success = chunk.isGenerated;

                if (success) {
                    std::lock_guard<std::mutex> lock(m_chunkMutex);
                    m_loadedChunks[request.chunkPos] = chunk;
                    m_chunkStates[request.chunkPos] = ChunkLoadState::Loaded;
                    UpdateChunkAccess(request.chunkPos);

                    if (OnChunkLoaded) {
                        OnChunkLoaded(request.chunkPos, chunk);
                    }
                }

                auto endTime = std::chrono::high_resolution_clock::now();
                float loadTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
                {
                    std::lock_guard<std::mutex> lock(m_statsMutex);
                    m_totalLoadTime += loadTime;
                    m_totalLoads++;
                    m_stats.pendingLoads--;
                }
                break;
            }

            case ChunkIORequestType::Save: {
                success = m_database->SaveChunk(request.chunkPos.x, request.chunkPos.y, request.chunkPos.z, request.data);

                if (success) {
                    std::lock_guard<std::mutex> lock(m_chunkMutex);
                    m_dirtyChunks.erase(request.chunkPos);
                    m_chunkStates[request.chunkPos] = ChunkLoadState::Loaded;

                    if (OnChunkSaved) {
                        OnChunkSaved(request.chunkPos, true);
                    }
                }

                auto endTime = std::chrono::high_resolution_clock::now();
                float saveTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
                {
                    std::lock_guard<std::mutex> lock(m_statsMutex);
                    m_totalSaveTime += saveTime;
                    m_totalSaves++;
                    m_stats.pendingSaves--;
                }
                break;
            }

            default:
                break;
        }

        if (request.callback) {
            request.callback(success);
        }
    }
}

void ChunkStreamer::UpdateLoadedChunks() {
    std::vector<glm::ivec3> viewPositions;
    {
        std::lock_guard<std::mutex> lock(m_viewMutex);
        for (const auto& [playerId, pos] : m_viewPositions) {
            viewPositions.push_back(WorldToChunkPos(pos));
        }
    }

    if (viewPositions.empty()) return;

    // Determine which chunks should be loaded
    std::set<glm::ivec3> desiredChunks;
    int chunkRadius = static_cast<int>(std::ceil(m_viewDistance / m_chunkSize));

    for (const auto& centerChunk : viewPositions) {
        for (int x = -chunkRadius; x <= chunkRadius; x++) {
            for (int z = -chunkRadius; z <= chunkRadius; z++) {
                for (int y = -2; y <= 2; y++) {
                    glm::ivec3 chunkPos = centerChunk + glm::ivec3(x, y, z);
                    desiredChunks.insert(chunkPos);
                }
            }
        }
    }

    // Load new chunks
    for (const auto& chunkPos : desiredChunks) {
        if (!IsChunkLoaded(chunkPos)) {
            int priority = CalculateChunkPriority(chunkPos);
            LoadChunk(chunkPos, priority);
        }
    }

    // Unload distant chunks
    std::vector<glm::ivec3> chunksToUnload;
    {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        for (const auto& [chunkPos, chunk] : m_loadedChunks) {
            if (desiredChunks.count(chunkPos) == 0) {
                chunksToUnload.push_back(chunkPos);
            }
        }
    }

    for (const auto& chunkPos : chunksToUnload) {
        UnloadChunk(chunkPos, true);
    }
}

int ChunkStreamer::CalculateChunkPriority(glm::ivec3 chunkPos) const {
    glm::vec3 closestView = GetClosestViewPosition(chunkPos);
    float distance = GetChunkDistance(chunkPos, closestView);
    float normalizedDist = std::clamp(distance / m_viewDistance, 0.0f, 1.0f);
    return static_cast<int>((1.0f - normalizedDist) * 100);
}

glm::vec3 ChunkStreamer::GetClosestViewPosition(glm::ivec3 chunkPos) const {
    std::lock_guard<std::mutex> lock(m_viewMutex);
    if (m_viewPositions.empty()) {
        return glm::vec3(0.0f);
    }

    glm::vec3 chunkWorldPos = glm::vec3(chunkPos) * static_cast<float>(m_chunkSize);
    float minDist = FLT_MAX;
    glm::vec3 closest;

    for (const auto& [playerId, pos] : m_viewPositions) {
        float dist = glm::length(pos - chunkWorldPos);
        if (dist < minDist) {
            minDist = dist;
            closest = pos;
        }
    }

    return closest;
}

void ChunkStreamer::EvictLRUChunks() {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    if (m_loadedChunks.size() <= m_maxCachedChunks) {
        return;
    }

    // Sort by access time
    std::vector<std::pair<glm::ivec3, uint64_t>> accessTimes;
    for (const auto& [chunkPos, accessTime] : m_chunkAccessTime) {
        accessTimes.emplace_back(chunkPos, accessTime);
    }

    std::sort(accessTimes.begin(), accessTimes.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    // Evict oldest chunks
    size_t toEvict = m_loadedChunks.size() - m_maxCachedChunks;
    for (size_t i = 0; i < toEvict && i < accessTimes.size(); i++) {
        m_chunkMutex.unlock();
        UnloadChunk(accessTimes[i].first, true);
        m_chunkMutex.lock();
    }
}

void ChunkStreamer::UpdateChunkAccess(glm::ivec3 chunkPos) {
    m_chunkAccessTime[chunkPos] = GetTimestamp();
}

glm::ivec3 ChunkStreamer::WorldToChunkPos(glm::vec3 worldPos) const {
    return glm::ivec3(
        static_cast<int>(std::floor(worldPos.x / m_chunkSize)),
        static_cast<int>(std::floor(worldPos.y / m_chunkSize)),
        static_cast<int>(std::floor(worldPos.z / m_chunkSize))
    );
}

float ChunkStreamer::GetChunkDistance(glm::ivec3 chunkPos, glm::vec3 worldPos) const {
    glm::vec3 chunkWorldPos = glm::vec3(chunkPos) * static_cast<float>(m_chunkSize);
    return glm::length(chunkWorldPos - worldPos);
}

void ChunkStreamer::LogError(const std::string& message) {
    if (OnError) {
        OnError(message);
    }
}

} // namespace Nova
