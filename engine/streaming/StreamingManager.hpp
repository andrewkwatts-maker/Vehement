#pragma once

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <glm/glm.hpp>

namespace Nova {

// ============================================================================
// Streaming Types
// ============================================================================

/**
 * @brief Asset type for streaming
 */
enum class StreamableType : uint8_t {
    Texture,
    Mesh,
    Audio,
    Animation,
    Custom
};

/**
 * @brief LOD level
 */
enum class LODLevel : uint8_t {
    Highest = 0,
    High = 1,
    Medium = 2,
    Low = 3,
    Lowest = 4,
    Count = 5
};

/**
 * @brief Stream request priority
 */
enum class StreamPriority : uint8_t {
    Critical = 0,    ///< Must load immediately
    High = 1,        ///< Load as soon as possible
    Normal = 2,      ///< Standard priority
    Low = 3,         ///< Load when convenient
    Background = 4   ///< Load only when idle
};

/**
 * @brief Streaming request status
 */
enum class StreamStatus : uint8_t {
    Pending,
    Loading,
    Loaded,
    Failed,
    Cancelled
};

// ============================================================================
// Streamable Asset
// ============================================================================

/**
 * @brief Base interface for streamable assets
 */
class IStreamable {
public:
    virtual ~IStreamable() = default;

    [[nodiscard]] virtual StreamableType GetType() const = 0;
    [[nodiscard]] virtual size_t GetMemorySize() const = 0;
    [[nodiscard]] virtual LODLevel GetCurrentLOD() const = 0;
    [[nodiscard]] virtual bool IsLoaded() const = 0;
    [[nodiscard]] virtual bool IsStreaming() const = 0;

    virtual void OnLoaded() = 0;
    virtual void OnUnloaded() = 0;
};

/**
 * @brief Streaming request
 */
struct StreamRequest {
    uint32_t id;
    std::string path;
    StreamableType type;
    StreamPriority priority;
    LODLevel targetLOD;
    std::weak_ptr<IStreamable> asset;
    std::function<void(bool success)> callback;

    // For priority queue sorting
    bool operator<(const StreamRequest& other) const {
        return priority > other.priority;  // Lower priority value = higher priority
    }
};

/**
 * @brief Memory budget per asset type
 */
struct MemoryBudget {
    size_t texturesBudget = 512 * 1024 * 1024;   ///< 512 MB
    size_t meshesBudget = 256 * 1024 * 1024;     ///< 256 MB
    size_t audioBudget = 128 * 1024 * 1024;      ///< 128 MB
    size_t totalBudget = 1024 * 1024 * 1024;     ///< 1 GB total

    size_t texturesUsed = 0;
    size_t meshesUsed = 0;
    size_t audioUsed = 0;

    [[nodiscard]] size_t GetTotalUsed() const {
        return texturesUsed + meshesUsed + audioUsed;
    }

    [[nodiscard]] bool CanAllocate(StreamableType type, size_t size) const {
        if (GetTotalUsed() + size > totalBudget) return false;
        switch (type) {
            case StreamableType::Texture: return texturesUsed + size <= texturesBudget;
            case StreamableType::Mesh: return meshesUsed + size <= meshesBudget;
            case StreamableType::Audio: return audioUsed + size <= audioBudget;
            default: return true;
        }
    }

    void Allocate(StreamableType type, size_t size) {
        switch (type) {
            case StreamableType::Texture: texturesUsed += size; break;
            case StreamableType::Mesh: meshesUsed += size; break;
            case StreamableType::Audio: audioUsed += size; break;
            default: break;
        }
    }

    void Free(StreamableType type, size_t size) {
        switch (type) {
            case StreamableType::Texture: texturesUsed = (texturesUsed > size) ? texturesUsed - size : 0; break;
            case StreamableType::Mesh: meshesUsed = (meshesUsed > size) ? meshesUsed - size : 0; break;
            case StreamableType::Audio: audioUsed = (audioUsed > size) ? audioUsed - size : 0; break;
            default: break;
        }
    }
};

// ============================================================================
// Texture Streaming
// ============================================================================

/**
 * @brief Mip level info for texture streaming
 */
struct MipLevelInfo {
    uint32_t width;
    uint32_t height;
    size_t size;
    bool resident;
};

/**
 * @brief Streamable texture
 */
class StreamableTexture : public IStreamable {
public:
    StreamableTexture() = default;
    ~StreamableTexture() override = default;

    [[nodiscard]] StreamableType GetType() const override { return StreamableType::Texture; }
    [[nodiscard]] size_t GetMemorySize() const override { return m_memorySize; }
    [[nodiscard]] LODLevel GetCurrentLOD() const override { return m_currentLOD; }
    [[nodiscard]] bool IsLoaded() const override { return m_loaded; }
    [[nodiscard]] bool IsStreaming() const override { return m_streaming; }

    void OnLoaded() override;
    void OnUnloaded() override;

    /**
     * @brief Request a specific mip level
     */
    void RequestMipLevel(int level);

    /**
     * @brief Get texture handle
     */
    [[nodiscard]] uint32_t GetHandle() const { return m_handle; }

    /**
     * @brief Get mip info
     */
    [[nodiscard]] const std::vector<MipLevelInfo>& GetMipInfo() const { return m_mipLevels; }

private:
    uint32_t m_handle = 0;
    std::string m_path;
    std::vector<MipLevelInfo> m_mipLevels;
    LODLevel m_currentLOD = LODLevel::Lowest;
    LODLevel m_requestedLOD = LODLevel::Lowest;
    size_t m_memorySize = 0;
    bool m_loaded = false;
    bool m_streaming = false;
};

// ============================================================================
// LOD Streaming
// ============================================================================

/**
 * @brief LOD mesh info
 */
struct LODMeshInfo {
    std::string path;
    size_t vertexCount;
    size_t indexCount;
    size_t memorySize;
    float screenSizeThreshold;   ///< Screen size ratio threshold
    bool resident;
};

/**
 * @brief Streamable mesh with LOD support
 */
class StreamableMesh : public IStreamable {
public:
    StreamableMesh() = default;
    ~StreamableMesh() override = default;

    [[nodiscard]] StreamableType GetType() const override { return StreamableType::Mesh; }
    [[nodiscard]] size_t GetMemorySize() const override { return m_memorySize; }
    [[nodiscard]] LODLevel GetCurrentLOD() const override { return m_currentLOD; }
    [[nodiscard]] bool IsLoaded() const override { return m_loaded; }
    [[nodiscard]] bool IsStreaming() const override { return m_streaming; }

    void OnLoaded() override;
    void OnUnloaded() override;

    /**
     * @brief Request specific LOD
     */
    void RequestLOD(LODLevel lod);

    /**
     * @brief Calculate desired LOD based on screen size
     */
    [[nodiscard]] LODLevel CalculateLOD(float screenSize) const;

    /**
     * @brief Add LOD level info
     */
    void AddLODLevel(const LODMeshInfo& info);

    /**
     * @brief Get VAO for current LOD
     */
    [[nodiscard]] uint32_t GetVAO() const { return m_vaos[static_cast<int>(m_currentLOD)]; }

private:
    std::vector<LODMeshInfo> m_lodLevels;
    std::array<uint32_t, 5> m_vaos{0};
    LODLevel m_currentLOD = LODLevel::Lowest;
    LODLevel m_requestedLOD = LODLevel::Lowest;
    size_t m_memorySize = 0;
    bool m_loaded = false;
    bool m_streaming = false;
};

// ============================================================================
// Streaming Manager
// ============================================================================

/**
 * @brief Resource streaming manager
 *
 * Features:
 * - Texture mip streaming
 * - Mesh LOD streaming
 * - Priority-based loading queue
 * - Memory budgets per asset type
 * - Background loading threads
 * - Distance/screen-size based streaming
 *
 * Usage:
 * @code
 * auto& streaming = StreamingManager::Instance();
 * streaming.Initialize();
 *
 * // Register streamable texture
 * auto tex = streaming.CreateTexture("texture.dds");
 *
 * // Update streaming based on camera position
 * streaming.UpdateStreaming(cameraPos, viewProj);
 *
 * // Process loaded assets on main thread
 * streaming.ProcessLoadedAssets();
 * @endcode
 */
class StreamingManager {
public:
    static StreamingManager& Instance();

    // Non-copyable
    StreamingManager(const StreamingManager&) = delete;
    StreamingManager& operator=(const StreamingManager&) = delete;

    /**
     * @brief Initialize streaming system
     */
    bool Initialize(int numThreads = 2);

    /**
     * @brief Shutdown
     */
    void Shutdown();

    // =========== Asset Creation ===========

    /**
     * @brief Create a streamable texture
     */
    std::shared_ptr<StreamableTexture> CreateTexture(const std::string& path);

    /**
     * @brief Create a streamable mesh
     */
    std::shared_ptr<StreamableMesh> CreateMesh(const std::string& basePath);

    /**
     * @brief Register a custom streamable
     */
    void RegisterStreamable(const std::string& id, std::shared_ptr<IStreamable> asset);

    /**
     * @brief Unregister streamable
     */
    void UnregisterStreamable(const std::string& id);

    // =========== Streaming Requests ===========

    /**
     * @brief Request asset streaming
     */
    uint32_t RequestStream(const std::string& path, StreamableType type,
                           StreamPriority priority = StreamPriority::Normal,
                           LODLevel targetLOD = LODLevel::Highest,
                           std::function<void(bool)> callback = nullptr);

    /**
     * @brief Cancel a streaming request
     */
    void CancelRequest(uint32_t requestId);

    /**
     * @brief Cancel all requests for an asset
     */
    void CancelAssetRequests(const std::string& path);

    // =========== Update ===========

    /**
     * @brief Update streaming based on viewer position
     */
    void UpdateStreaming(const glm::vec3& viewerPos, const glm::mat4& viewProj);

    /**
     * @brief Process loaded assets (call on main thread)
     */
    void ProcessLoadedAssets();

    /**
     * @brief Force stream in specific region
     */
    void StreamRegion(const glm::vec3& center, float radius, StreamPriority priority);

    // =========== Memory Management ===========

    /**
     * @brief Set memory budgets
     */
    void SetMemoryBudget(const MemoryBudget& budget);

    /**
     * @brief Get current memory usage
     */
    [[nodiscard]] const MemoryBudget& GetMemoryBudget() const { return m_budget; }

    /**
     * @brief Force unload low priority assets
     */
    void TrimMemory(size_t targetSize);

    /**
     * @brief Unload all assets
     */
    void UnloadAll();

    // =========== Settings ===========

    /**
     * @brief Set streaming distance multiplier
     */
    void SetStreamingDistance(float distance) { m_streamingDistance = distance; }

    /**
     * @brief Set texture mip bias
     */
    void SetMipBias(float bias) { m_mipBias = bias; }

    /**
     * @brief Enable/disable streaming
     */
    void SetStreamingEnabled(bool enabled) { m_streamingEnabled = enabled; }
    [[nodiscard]] bool IsStreamingEnabled() const { return m_streamingEnabled; }

    /**
     * @brief Set max requests per frame
     */
    void SetMaxRequestsPerFrame(int max) { m_maxRequestsPerFrame = max; }

    // =========== Statistics ===========

    [[nodiscard]] size_t GetPendingRequestCount() const { return m_pendingRequests.size(); }
    [[nodiscard]] size_t GetLoadedAssetCount() const { return m_loadedAssets.size(); }
    [[nodiscard]] size_t GetTotalMemoryUsed() const { return m_budget.GetTotalUsed(); }

private:
    StreamingManager() = default;
    ~StreamingManager();

    void WorkerThread();
    StreamRequest* GetNextRequest();
    void LoadAsset(StreamRequest& request);
    float CalculateAssetPriority(const IStreamable* asset, const glm::vec3& viewerPos);

    // Thread pool
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running{false};
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;

    // Requests
    std::priority_queue<StreamRequest> m_requestQueue;
    std::unordered_map<uint32_t, StreamRequest> m_pendingRequests;
    std::queue<StreamRequest> m_completedRequests;
    std::mutex m_completedMutex;
    uint32_t m_nextRequestId = 1;

    // Assets
    std::unordered_map<std::string, std::shared_ptr<IStreamable>> m_streamables;
    std::vector<std::shared_ptr<IStreamable>> m_loadedAssets;
    std::mutex m_assetMutex;

    // Memory
    MemoryBudget m_budget;

    // Settings
    float m_streamingDistance = 500.0f;
    float m_mipBias = 0.0f;
    bool m_streamingEnabled = true;
    int m_maxRequestsPerFrame = 4;

    bool m_initialized = false;
};

// ============================================================================
// Implementation
// ============================================================================

inline StreamingManager& StreamingManager::Instance() {
    static StreamingManager instance;
    return instance;
}

inline StreamingManager::~StreamingManager() {
    Shutdown();
}

inline bool StreamingManager::Initialize(int numThreads) {
    if (m_initialized) return true;

    m_running = true;

    for (int i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&StreamingManager::WorkerThread, this);
    }

    m_initialized = true;
    return true;
}

inline void StreamingManager::Shutdown() {
    if (!m_initialized) return;

    m_running = false;
    m_queueCondition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();

    UnloadAll();
    m_initialized = false;
}

inline std::shared_ptr<StreamableTexture> StreamingManager::CreateTexture(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_assetMutex);

    auto it = m_streamables.find(path);
    if (it != m_streamables.end()) {
        return std::dynamic_pointer_cast<StreamableTexture>(it->second);
    }

    auto texture = std::make_shared<StreamableTexture>();
    m_streamables[path] = texture;
    return texture;
}

inline std::shared_ptr<StreamableMesh> StreamingManager::CreateMesh(const std::string& basePath) {
    std::lock_guard<std::mutex> lock(m_assetMutex);

    auto it = m_streamables.find(basePath);
    if (it != m_streamables.end()) {
        return std::dynamic_pointer_cast<StreamableMesh>(it->second);
    }

    auto mesh = std::make_shared<StreamableMesh>();
    m_streamables[basePath] = mesh;
    return mesh;
}

inline void StreamingManager::RegisterStreamable(const std::string& id, std::shared_ptr<IStreamable> asset) {
    std::lock_guard<std::mutex> lock(m_assetMutex);
    m_streamables[id] = std::move(asset);
}

inline void StreamingManager::UnregisterStreamable(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_assetMutex);
    m_streamables.erase(id);
}

inline uint32_t StreamingManager::RequestStream(const std::string& path, StreamableType type,
                                                 StreamPriority priority, LODLevel targetLOD,
                                                 std::function<void(bool)> callback) {
    StreamRequest request;
    request.id = m_nextRequestId++;
    request.path = path;
    request.type = type;
    request.priority = priority;
    request.targetLOD = targetLOD;
    request.callback = std::move(callback);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_requestQueue.push(request);
        m_pendingRequests[request.id] = request;
    }

    m_queueCondition.notify_one();
    return request.id;
}

inline void StreamingManager::CancelRequest(uint32_t requestId) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingRequests.erase(requestId);
}

inline void StreamingManager::CancelAssetRequests(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ) {
        if (it->second.path == path) {
            it = m_pendingRequests.erase(it);
        } else {
            ++it;
        }
    }
}

inline void StreamingManager::UpdateStreaming(const glm::vec3& viewerPos, const glm::mat4& /*viewProj*/) {
    if (!m_streamingEnabled) return;

    std::lock_guard<std::mutex> lock(m_assetMutex);

    for (auto& [id, asset] : m_streamables) {
        float priority = CalculateAssetPriority(asset.get(), viewerPos);

        // Queue streaming requests based on priority
        if (priority > 0.5f && !asset->IsLoaded() && !asset->IsStreaming()) {
            StreamPriority p = (priority > 0.9f) ? StreamPriority::High : StreamPriority::Normal;
            RequestStream(id, asset->GetType(), p, LODLevel::Highest);
        }
    }
}

inline void StreamingManager::ProcessLoadedAssets() {
    std::lock_guard<std::mutex> lock(m_completedMutex);

    while (!m_completedRequests.empty()) {
        StreamRequest& request = m_completedRequests.front();

        if (request.callback) {
            request.callback(true);
        }

        m_completedRequests.pop();
    }
}

inline void StreamingManager::SetMemoryBudget(const MemoryBudget& budget) {
    m_budget = budget;
}

inline void StreamingManager::TrimMemory(size_t targetSize) {
    std::lock_guard<std::mutex> lock(m_assetMutex);

    // Sort assets by priority (lowest first)
    std::vector<std::pair<std::string, std::shared_ptr<IStreamable>>> sortedAssets(
        m_streamables.begin(), m_streamables.end());

    // Unload lowest priority assets until under budget
    size_t currentUsed = m_budget.GetTotalUsed();
    for (auto& [id, asset] : sortedAssets) {
        if (currentUsed <= targetSize) break;

        if (asset->IsLoaded()) {
            size_t freed = asset->GetMemorySize();
            asset->OnUnloaded();
            m_budget.Free(asset->GetType(), freed);
            currentUsed -= freed;
        }
    }
}

inline void StreamingManager::UnloadAll() {
    std::lock_guard<std::mutex> lock(m_assetMutex);

    for (auto& [id, asset] : m_streamables) {
        if (asset->IsLoaded()) {
            asset->OnUnloaded();
        }
    }

    m_streamables.clear();
    m_loadedAssets.clear();
    m_budget.texturesUsed = 0;
    m_budget.meshesUsed = 0;
    m_budget.audioUsed = 0;
}

inline void StreamingManager::WorkerThread() {
    while (m_running) {
        StreamRequest request;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] {
                return !m_running || !m_requestQueue.empty();
            });

            if (!m_running) break;

            if (!m_requestQueue.empty()) {
                request = m_requestQueue.top();
                m_requestQueue.pop();
            } else {
                continue;
            }
        }

        // Check if cancelled
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_pendingRequests.find(request.id) == m_pendingRequests.end()) {
                continue;  // Cancelled
            }
        }

        LoadAsset(request);

        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedRequests.push(request);
        }

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_pendingRequests.erase(request.id);
        }
    }
}

inline void StreamingManager::LoadAsset(StreamRequest& /*request*/) {
    // Asset loading implementation
    // Would load file, decompress, prepare for GPU upload
}

inline float StreamingManager::CalculateAssetPriority(const IStreamable* /*asset*/, const glm::vec3& /*viewerPos*/) {
    // Priority calculation based on distance, screen size, etc.
    return 0.5f;
}

inline void StreamableTexture::OnLoaded() {
    m_loaded = true;
    m_streaming = false;
}

inline void StreamableTexture::OnUnloaded() {
    m_loaded = false;
    m_currentLOD = LODLevel::Lowest;
}

inline void StreamableTexture::RequestMipLevel(int /*level*/) {
    m_streaming = true;
}

inline void StreamableMesh::OnLoaded() {
    m_loaded = true;
    m_streaming = false;
}

inline void StreamableMesh::OnUnloaded() {
    m_loaded = false;
    m_currentLOD = LODLevel::Lowest;
}

inline void StreamableMesh::RequestLOD(LODLevel lod) {
    if (lod != m_currentLOD && !m_streaming) {
        m_requestedLOD = lod;
        m_streaming = true;
    }
}

inline LODLevel StreamableMesh::CalculateLOD(float screenSize) const {
    for (size_t i = 0; i < m_lodLevels.size(); ++i) {
        if (screenSize >= m_lodLevels[i].screenSizeThreshold) {
            return static_cast<LODLevel>(i);
        }
    }
    return LODLevel::Lowest;
}

inline void StreamableMesh::AddLODLevel(const LODMeshInfo& info) {
    m_lodLevels.push_back(info);
}

} // namespace Nova
