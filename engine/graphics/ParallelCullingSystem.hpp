#pragma once

#include <vector>
#include <future>
#include <atomic>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "../math/Vector3.hpp"
#include "../math/Matrix4.hpp"

namespace Engine {
namespace Graphics {

/**
 * @brief Frustum representation for culling
 */
struct Frustum {
    Vector4 planes[6]; // Left, right, bottom, top, near, far

    Frustum() = default;
    explicit Frustum(const Matrix4& viewProj);

    bool TestSphere(const Vector3& center, float radius) const;
    bool TestAABB(const Vector3& min, const Vector3& max) const;
};

/**
 * @brief Camera representation for LOD calculations
 */
struct CullingCamera {
    Vector3 position;
    Matrix4 viewProjection;
    Frustum frustum;
    float nearPlane;
    float farPlane;
    float fov;

    CullingCamera() : nearPlane(0.1f), farPlane(1000.0f), fov(60.0f) {}
};

/**
 * @brief SDF instance data for culling
 */
struct SDFInstance {
    Matrix4 transform;
    Vector3 boundingSphereCenter;
    float boundingSphereRadius;
    Vector3 aabbMin;
    Vector3 aabbMax;
    uint32_t materialID;
    uint32_t lodLevel;
    uint32_t instanceID;

    SDFInstance() : boundingSphereRadius(1.0f), materialID(0), lodLevel(0), instanceID(0) {}
};

/**
 * @brief Result of culling operation
 */
struct CullingResult {
    std::vector<uint32_t> visibleIndices;
    std::vector<uint32_t> lodLevels;
    uint32_t totalVisible;
    float cullingTimeMs;

    CullingResult() : totalVisible(0), cullingTimeMs(0.0f) {}

    void Clear() {
        visibleIndices.clear();
        lodLevels.clear();
        totalVisible = 0;
        cullingTimeMs = 0.0f;
    }
};

/**
 * @brief Culling job for parallel processing
 */
struct CullingJob {
    int startIndex;
    int count;
    const Frustum* frustum;
    const CullingCamera* camera;
    const std::vector<SDFInstance>* instances;
    std::vector<uint32_t> visibleIndices;
    std::vector<uint32_t> lodLevels;

    CullingJob() : startIndex(0), count(0), frustum(nullptr), camera(nullptr), instances(nullptr) {}
};

/**
 * @brief Lock-free job queue for worker threads
 */
class JobQueue {
public:
    JobQueue() = default;
    ~JobQueue() = default;

    void Push(std::function<void()> job);
    bool TryPop(std::function<void()>& job);
    bool Empty() const;
    size_t Size() const;

private:
    std::queue<std::function<void()>> m_jobs;
    mutable std::mutex m_mutex;
};

/**
 * @brief Thread pool for parallel task execution
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = 0);
    ~ThreadPool();

    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    void WaitAll();
    size_t GetThreadCount() const { return m_workers.size(); }

private:
    void WorkerThread();

    std::vector<std::thread> m_workers;
    JobQueue m_jobQueue;
    std::condition_variable m_condition;
    std::mutex m_queueMutex;
    std::atomic<bool> m_stop;
    std::atomic<int> m_activeTasks;
};

/**
 * @brief Multi-threaded parallel culling system
 * Performs frustum culling and LOD calculation for 10,000+ objects
 * Target: <1ms for 10K objects with 8-16 threads
 */
class ParallelCullingSystem {
public:
    /**
     * @brief Configuration for culling system
     */
    struct Config {
        size_t numThreads;           // 0 = auto-detect
        int jobGranularity;          // Objects per job (default: 256)
        bool enableLOD;              // Enable LOD calculation
        float lodBias;               // LOD bias factor
        int maxLODLevel;             // Maximum LOD level

        Config()
            : numThreads(0)
            , jobGranularity(256)
            , enableLOD(true)
            , lodBias(1.0f)
            , maxLODLevel(4) {}
    };

    explicit ParallelCullingSystem(const Config& config = Config());
    ~ParallelCullingSystem();

    /**
     * @brief Perform parallel frustum culling
     * @param instances Array of SDF instances to cull
     * @param camera Camera for culling and LOD
     * @return Culling result with visible indices
     */
    CullingResult CullObjects(const std::vector<SDFInstance>& instances, const CullingCamera& camera);

    /**
     * @brief Perform frustum culling only (no LOD)
     */
    CullingResult CullObjectsFast(const std::vector<SDFInstance>& instances, const Frustum& frustum);

    /**
     * @brief Calculate LOD for visible objects
     */
    void CalculateLOD(const std::vector<SDFInstance>& instances,
                     const std::vector<uint32_t>& visibleIndices,
                     const CullingCamera& camera,
                     std::vector<uint32_t>& outLODLevels);

    /**
     * @brief Get performance statistics
     */
    struct Stats {
        float avgCullingTimeMs;
        float maxCullingTimeMs;
        float minCullingTimeMs;
        uint32_t totalObjectsTested;
        uint32_t totalObjectsVisible;
        float visibilityRatio;

        Stats() : avgCullingTimeMs(0), maxCullingTimeMs(0), minCullingTimeMs(999999.0f),
                  totalObjectsTested(0), totalObjectsVisible(0), visibilityRatio(0) {}
    };

    Stats GetStats() const { return m_stats; }
    void ResetStats();

    /**
     * @brief Update configuration
     */
    void SetConfig(const Config& config);
    Config GetConfig() const { return m_config; }

private:
    /**
     * @brief Process a single culling job
     */
    void ProcessCullingJob(CullingJob& job);

    /**
     * @brief Calculate LOD level for an instance
     */
    uint32_t CalculateLODLevel(const SDFInstance& instance, const CullingCamera& camera) const;

    /**
     * @brief Merge job results into final output
     */
    void MergeResults(const std::vector<CullingJob>& jobs, CullingResult& outResult);

    Config m_config;
    std::unique_ptr<ThreadPool> m_threadPool;
    Stats m_stats;

    // Performance tracking
    std::vector<float> m_cullingTimeSamples;
    static constexpr size_t MAX_TIME_SAMPLES = 60;
};

// Template implementation for ThreadPool::Submit
template<typename F, typename... Args>
auto ThreadPool::Submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using ReturnType = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = task->get_future();

    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        if (m_stop) {
            throw std::runtime_error("Cannot submit to stopped ThreadPool");
        }

        m_activeTasks++;
        m_jobQueue.Push([task]() { (*task)(); });
    }

    m_condition.notify_one();
    return result;
}

} // namespace Graphics
} // namespace Engine
