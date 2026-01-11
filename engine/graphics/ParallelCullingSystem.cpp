#include "ParallelCullingSystem.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace Engine {
namespace Graphics {

// ============================================================================
// Frustum Implementation
// ============================================================================

Frustum::Frustum(const Matrix4& viewProj) {
    // Extract frustum planes from view-projection matrix
    // Left plane
    planes[0] = Vector4(
        viewProj.m[3] + viewProj.m[0],
        viewProj.m[7] + viewProj.m[4],
        viewProj.m[11] + viewProj.m[8],
        viewProj.m[15] + viewProj.m[12]
    );

    // Right plane
    planes[1] = Vector4(
        viewProj.m[3] - viewProj.m[0],
        viewProj.m[7] - viewProj.m[4],
        viewProj.m[11] - viewProj.m[8],
        viewProj.m[15] - viewProj.m[12]
    );

    // Bottom plane
    planes[2] = Vector4(
        viewProj.m[3] + viewProj.m[1],
        viewProj.m[7] + viewProj.m[5],
        viewProj.m[11] + viewProj.m[9],
        viewProj.m[15] + viewProj.m[13]
    );

    // Top plane
    planes[3] = Vector4(
        viewProj.m[3] - viewProj.m[1],
        viewProj.m[7] - viewProj.m[5],
        viewProj.m[11] - viewProj.m[9],
        viewProj.m[15] - viewProj.m[13]
    );

    // Near plane
    planes[4] = Vector4(
        viewProj.m[3] + viewProj.m[2],
        viewProj.m[7] + viewProj.m[6],
        viewProj.m[11] + viewProj.m[10],
        viewProj.m[15] + viewProj.m[14]
    );

    // Far plane
    planes[5] = Vector4(
        viewProj.m[3] - viewProj.m[2],
        viewProj.m[7] - viewProj.m[6],
        viewProj.m[11] - viewProj.m[10],
        viewProj.m[15] - viewProj.m[14]
    );

    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = std::sqrt(planes[i].x * planes[i].x +
                                planes[i].y * planes[i].y +
                                planes[i].z * planes[i].z);
        if (length > 0.0f) {
            planes[i].x /= length;
            planes[i].y /= length;
            planes[i].z /= length;
            planes[i].w /= length;
        }
    }
}

bool Frustum::TestSphere(const Vector3& center, float radius) const {
    for (int i = 0; i < 6; i++) {
        float distance = planes[i].x * center.x +
                        planes[i].y * center.y +
                        planes[i].z * center.z +
                        planes[i].w;

        if (distance < -radius) {
            return false; // Outside frustum
        }
    }
    return true; // Inside or intersecting
}

bool Frustum::TestAABB(const Vector3& min, const Vector3& max) const {
    for (int i = 0; i < 6; i++) {
        // Get positive vertex
        Vector3 pVertex;
        pVertex.x = (planes[i].x >= 0) ? max.x : min.x;
        pVertex.y = (planes[i].y >= 0) ? max.y : min.y;
        pVertex.z = (planes[i].z >= 0) ? max.z : min.z;

        float distance = planes[i].x * pVertex.x +
                        planes[i].y * pVertex.y +
                        planes[i].z * pVertex.z +
                        planes[i].w;

        if (distance < 0) {
            return false; // Outside frustum
        }
    }
    return true; // Inside or intersecting
}

// ============================================================================
// JobQueue Implementation
// ============================================================================

void JobQueue::Push(std::function<void()> job) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_jobs.push(std::move(job));
}

bool JobQueue::TryPop(std::function<void()>& job) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_jobs.empty()) {
        return false;
    }
    job = std::move(m_jobs.front());
    m_jobs.pop();
    return true;
}

bool JobQueue::Empty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_jobs.empty();
}

size_t JobQueue::Size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_jobs.size();
}

// ============================================================================
// ThreadPool Implementation
// ============================================================================

ThreadPool::ThreadPool(size_t numThreads)
    : m_stop(false)
    , m_activeTasks(0) {

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 8; // Fallback
    }

    // Clamp to reasonable range
    numThreads = std::max<size_t>(1, std::min<size_t>(32, numThreads));

    m_workers.reserve(numThreads);
    for (size_t i = 0; i < numThreads; i++) {
        m_workers.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    m_stop = true;
    m_condition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::WorkerThread() {
    while (true) {
        std::function<void()> job;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this] {
                return m_stop || !m_jobQueue.Empty();
            });

            if (m_stop && m_jobQueue.Empty()) {
                break;
            }

            if (!m_jobQueue.TryPop(job)) {
                continue;
            }
        }

        if (job) {
            job();
            m_activeTasks--;
        }
    }
}

void ThreadPool::WaitAll() {
    while (m_activeTasks > 0 || !m_jobQueue.Empty()) {
        std::this_thread::yield();
    }
}

// ============================================================================
// ParallelCullingSystem Implementation
// ============================================================================

ParallelCullingSystem::ParallelCullingSystem(const Config& config)
    : m_config(config) {

    m_threadPool = std::make_unique<ThreadPool>(config.numThreads);
    m_cullingTimeSamples.reserve(MAX_TIME_SAMPLES);
}

ParallelCullingSystem::~ParallelCullingSystem() = default;

CullingResult ParallelCullingSystem::CullObjects(
    const std::vector<SDFInstance>& instances,
    const CullingCamera& camera) {

    auto startTime = std::chrono::high_resolution_clock::now();

    CullingResult result;

    if (instances.empty()) {
        return result;
    }

    // Calculate number of jobs
    const int jobCount = (static_cast<int>(instances.size()) + m_config.jobGranularity - 1) / m_config.jobGranularity;

    // Create culling jobs
    std::vector<CullingJob> jobs(jobCount);
    std::vector<std::future<void>> futures;
    futures.reserve(jobCount);

    for (int i = 0; i < jobCount; i++) {
        jobs[i].startIndex = i * m_config.jobGranularity;
        jobs[i].count = std::min(m_config.jobGranularity,
                                static_cast<int>(instances.size()) - jobs[i].startIndex);
        jobs[i].frustum = &camera.frustum;
        jobs[i].camera = &camera;
        jobs[i].instances = &instances;

        // Reserve approximate capacity
        jobs[i].visibleIndices.reserve(jobs[i].count / 2);
        if (m_config.enableLOD) {
            jobs[i].lodLevels.reserve(jobs[i].count / 2);
        }
    }

    // Submit jobs to thread pool
    for (int i = 0; i < jobCount; i++) {
        futures.push_back(m_threadPool->Submit([this, &jobs, i]() {
            ProcessCullingJob(jobs[i]);
        }));
    }

    // Wait for all jobs to complete
    for (auto& future : futures) {
        future.get();
    }

    // Merge results
    MergeResults(jobs, result);

    auto endTime = std::chrono::high_resolution_clock::now();
    result.cullingTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    // Update statistics
    m_cullingTimeSamples.push_back(result.cullingTimeMs);
    if (m_cullingTimeSamples.size() > MAX_TIME_SAMPLES) {
        m_cullingTimeSamples.erase(m_cullingTimeSamples.begin());
    }

    m_stats.totalObjectsTested += static_cast<uint32_t>(instances.size());
    m_stats.totalObjectsVisible += result.totalVisible;
    m_stats.maxCullingTimeMs = std::max(m_stats.maxCullingTimeMs, result.cullingTimeMs);
    m_stats.minCullingTimeMs = std::min(m_stats.minCullingTimeMs, result.cullingTimeMs);

    // Calculate average
    float sum = 0.0f;
    for (float time : m_cullingTimeSamples) {
        sum += time;
    }
    m_stats.avgCullingTimeMs = sum / m_cullingTimeSamples.size();

    if (m_stats.totalObjectsTested > 0) {
        m_stats.visibilityRatio = static_cast<float>(m_stats.totalObjectsVisible) /
                                  static_cast<float>(m_stats.totalObjectsTested);
    }

    return result;
}

CullingResult ParallelCullingSystem::CullObjectsFast(
    const std::vector<SDFInstance>& instances,
    const Frustum& frustum) {

    auto startTime = std::chrono::high_resolution_clock::now();

    CullingResult result;

    if (instances.empty()) {
        return result;
    }

    // Calculate number of jobs
    const int jobCount = (static_cast<int>(instances.size()) + m_config.jobGranularity - 1) / m_config.jobGranularity;

    // Create culling jobs (no LOD)
    std::vector<CullingJob> jobs(jobCount);
    std::vector<std::future<void>> futures;
    futures.reserve(jobCount);

    CullingCamera dummyCamera;
    dummyCamera.frustum = frustum;

    for (int i = 0; i < jobCount; i++) {
        jobs[i].startIndex = i * m_config.jobGranularity;
        jobs[i].count = std::min(m_config.jobGranularity,
                                static_cast<int>(instances.size()) - jobs[i].startIndex);
        jobs[i].frustum = &frustum;
        jobs[i].camera = &dummyCamera;
        jobs[i].instances = &instances;
        jobs[i].visibleIndices.reserve(jobs[i].count / 2);
    }

    // Submit jobs
    for (int i = 0; i < jobCount; i++) {
        futures.push_back(m_threadPool->Submit([&jobs, i]() {
            CullingJob& job = jobs[i];
            for (int j = 0; j < job.count; j++) {
                int index = job.startIndex + j;
                const SDFInstance& instance = (*job.instances)[index];

                // Transform bounding sphere center
                Vector3 worldCenter = instance.transform.TransformPoint(instance.boundingSphereCenter);

                // Frustum test
                if (job.frustum->TestSphere(worldCenter, instance.boundingSphereRadius)) {
                    job.visibleIndices.push_back(instance.instanceID);
                }
            }
        }));
    }

    // Wait for completion
    for (auto& future : futures) {
        future.get();
    }

    // Merge results
    MergeResults(jobs, result);

    auto endTime = std::chrono::high_resolution_clock::now();
    result.cullingTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    return result;
}

void ParallelCullingSystem::ProcessCullingJob(CullingJob& job) {
    for (int i = 0; i < job.count; i++) {
        int index = job.startIndex + i;
        const SDFInstance& instance = (*job.instances)[index];

        // Transform bounding sphere center
        Vector3 worldCenter = instance.transform.TransformPoint(instance.boundingSphereCenter);

        // Frustum culling
        if (job.frustum->TestSphere(worldCenter, instance.boundingSphereRadius)) {
            job.visibleIndices.push_back(instance.instanceID);

            // Calculate LOD if enabled
            if (m_config.enableLOD && job.camera) {
                uint32_t lod = CalculateLODLevel(instance, *job.camera);
                job.lodLevels.push_back(lod);
            }
        }
    }
}

uint32_t ParallelCullingSystem::CalculateLODLevel(
    const SDFInstance& instance,
    const CullingCamera& camera) const {

    // Calculate distance from camera to instance
    Vector3 worldCenter = instance.transform.TransformPoint(instance.boundingSphereCenter);
    Vector3 toInstance = worldCenter - camera.position;
    float distance = toInstance.Length();

    // Calculate screen-space size (projected radius)
    float fovRadians = camera.fov * 3.14159265f / 180.0f;
    float screenHeight = 2.0f * std::tan(fovRadians * 0.5f) * distance;
    float projectedSize = (instance.boundingSphereRadius * 2.0f) / screenHeight;

    // Apply LOD bias
    projectedSize *= m_config.lodBias;

    // Determine LOD level based on screen-space size
    uint32_t lodLevel = 0;

    if (projectedSize > 0.3f) {
        lodLevel = 0; // Highest detail
    } else if (projectedSize > 0.15f) {
        lodLevel = 1;
    } else if (projectedSize > 0.075f) {
        lodLevel = 2;
    } else if (projectedSize > 0.0375f) {
        lodLevel = 3;
    } else {
        lodLevel = 4; // Lowest detail
    }

    // Clamp to max LOD level
    lodLevel = std::min(lodLevel, static_cast<uint32_t>(m_config.maxLODLevel));

    return lodLevel;
}

void ParallelCullingSystem::MergeResults(
    const std::vector<CullingJob>& jobs,
    CullingResult& outResult) {

    // Calculate total visible count
    size_t totalVisible = 0;
    for (const auto& job : jobs) {
        totalVisible += job.visibleIndices.size();
    }

    // Reserve space
    outResult.visibleIndices.reserve(totalVisible);
    if (m_config.enableLOD) {
        outResult.lodLevels.reserve(totalVisible);
    }

    // Merge results from all jobs
    for (const auto& job : jobs) {
        outResult.visibleIndices.insert(
            outResult.visibleIndices.end(),
            job.visibleIndices.begin(),
            job.visibleIndices.end()
        );

        if (m_config.enableLOD) {
            outResult.lodLevels.insert(
                outResult.lodLevels.end(),
                job.lodLevels.begin(),
                job.lodLevels.end()
            );
        }
    }

    outResult.totalVisible = static_cast<uint32_t>(totalVisible);
}

void ParallelCullingSystem::CalculateLOD(
    const std::vector<SDFInstance>& instances,
    const std::vector<uint32_t>& visibleIndices,
    const CullingCamera& camera,
    std::vector<uint32_t>& outLODLevels) {

    outLODLevels.resize(visibleIndices.size());

    // Parallel LOD calculation
    const size_t batchSize = 256;
    const size_t numBatches = (visibleIndices.size() + batchSize - 1) / batchSize;

    std::vector<std::future<void>> futures;
    futures.reserve(numBatches);

    for (size_t i = 0; i < numBatches; i++) {
        futures.push_back(m_threadPool->Submit([this, &instances, &visibleIndices, &camera, &outLODLevels, i, batchSize]() {
            size_t start = i * batchSize;
            size_t end = std::min(start + batchSize, visibleIndices.size());

            for (size_t j = start; j < end; j++) {
                uint32_t instanceIndex = visibleIndices[j];
                if (instanceIndex < instances.size()) {
                    outLODLevels[j] = CalculateLODLevel(instances[instanceIndex], camera);
                }
            }
        }));
    }

    for (auto& future : futures) {
        future.get();
    }
}

void ParallelCullingSystem::ResetStats() {
    m_stats = Stats();
    m_cullingTimeSamples.clear();
}

void ParallelCullingSystem::SetConfig(const Config& config) {
    m_config = config;

    // Recreate thread pool if thread count changed
    if (m_threadPool->GetThreadCount() != config.numThreads && config.numThreads > 0) {
        m_threadPool = std::make_unique<ThreadPool>(config.numThreads);
    }
}

} // namespace Graphics
} // namespace Engine
