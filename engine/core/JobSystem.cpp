#include "core/JobSystem.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#endif

namespace Nova {

// Thread-local storage for worker identification
thread_local bool JobSystem::s_isWorkerThread = false;

JobSystem::~JobSystem() {
    if (m_initialized) {
        Shutdown();
    }
}

bool JobSystem::Initialize(const JobSystemConfig& config) {
    if (m_initialized) {
        spdlog::warn("JobSystem already initialized");
        return true;
    }

    // Determine worker count
    uint32_t hardwareThreads = std::thread::hardware_concurrency();
    m_workerCount = (config.workerThreads > 0)
        ? config.workerThreads
        : std::max(1u, hardwareThreads - 1);

    spdlog::info("Initializing JobSystem with {} worker threads", m_workerCount);

    m_running = true;
    m_workers.reserve(m_workerCount);

    // Create worker threads
    for (uint32_t i = 0; i < m_workerCount; ++i) {
        m_workers.emplace_back(&JobSystem::WorkerLoop, this, i);

        // Set thread name for debugging
#ifdef _WIN32
        std::wstring name = std::wstring(config.threadNamePrefix.begin(),
                                          config.threadNamePrefix.end()) +
                           std::to_wstring(i);
        SetThreadDescription(m_workers.back().native_handle(), name.c_str());
#elif defined(__linux__)
        std::string name = config.threadNamePrefix + std::to_string(i);
        pthread_setname_np(m_workers.back().native_handle(), name.c_str());
#elif defined(__APPLE__)
        // macOS requires setting thread name from the thread itself
#endif
    }

    m_initialized = true;
    spdlog::info("JobSystem initialized successfully");
    return true;
}

void JobSystem::Shutdown() {
    if (!m_initialized) {
        return;
    }

    spdlog::info("Shutting down JobSystem");

    // Signal workers to stop
    m_running = false;
    m_condition.notify_all();

    // Wait for all workers to finish
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    m_workers.clear();
    m_workerCount = 0;

    // Clear remaining jobs
    {
        std::lock_guard lock(m_queueMutex);
        while (!m_jobQueue.empty()) {
            m_jobQueue.pop();
        }
    }

    m_initialized = false;
    spdlog::info("JobSystem shutdown complete");
}

JobHandle JobSystem::Submit(Job job, JobPriority priority) {
    auto completed = std::make_shared<std::atomic<bool>>(false);

    {
        std::lock_guard lock(m_queueMutex);
        m_jobQueue.push(PrioritizedJob{
            std::move(job),
            completed,
            nullptr,
            priority
        });
    }

    m_condition.notify_one();
    return JobHandle(completed);
}

void JobSystem::Submit(Job job, JobCounter& counter, JobPriority priority) {
    counter.Increment();

    {
        std::lock_guard lock(m_queueMutex);
        m_jobQueue.push(PrioritizedJob{
            std::move(job),
            nullptr,
            &counter,
            priority
        });
    }

    m_condition.notify_one();
}

void JobSystem::SubmitAndWait(const std::vector<Job>& jobs, JobPriority priority) {
    if (jobs.empty()) {
        return;
    }

    JobCounter counter(0);

    for (const auto& job : jobs) {
        Submit(job, counter, priority);
    }

    counter.Wait();
}

void JobSystem::ParallelFor(size_t start, size_t end, size_t batchSize,
                             const std::function<void(size_t)>& func) {
    if (start >= end) {
        return;
    }

    size_t count = end - start;

    // For small ranges, just execute sequentially
    if (count <= batchSize || m_workerCount == 0) {
        for (size_t i = start; i < end; ++i) {
            func(i);
        }
        return;
    }

    JobCounter counter(0);

    for (size_t batchStart = start; batchStart < end; batchStart += batchSize) {
        size_t batchEnd = std::min(batchStart + batchSize, end);

        Submit([=, &func]() {
            for (size_t i = batchStart; i < batchEnd; ++i) {
                func(i);
            }
        }, counter);
    }

    counter.Wait();
}

void JobSystem::ParallelFor(size_t count, const std::function<void(size_t)>& func) {
    // Auto-determine batch size based on work and thread count
    size_t batchSize = std::max(size_t(1), count / (m_workerCount * 4));
    ParallelFor(0, count, batchSize, func);
}

void JobSystem::ParallelForRange(size_t start, size_t end,
                                  const std::function<void(size_t, size_t)>& func) {
    if (start >= end) {
        return;
    }

    size_t count = end - start;

    // For small ranges, execute sequentially
    if (count <= 1000 || m_workerCount == 0) {
        func(start, end);
        return;
    }

    uint32_t numBatches = m_workerCount * 2;
    size_t batchSize = (count + numBatches - 1) / numBatches;

    JobCounter counter(0);

    for (size_t batchStart = start; batchStart < end; batchStart += batchSize) {
        size_t batchEnd = std::min(batchStart + batchSize, end);

        Submit([=, &func]() {
            func(batchStart, batchEnd);
        }, counter);
    }

    counter.Wait();
}

size_t JobSystem::GetPendingJobCount() const {
    std::lock_guard lock(m_queueMutex);
    return m_jobQueue.size();
}

bool JobSystem::IsWorkerThread() const {
    return s_isWorkerThread;
}

bool JobSystem::YieldAndProcess() {
    auto job = TryGetJob();
    if (job) {
        ExecuteJob(*job);
        return true;
    }
    return false;
}

void JobSystem::WorkerLoop(uint32_t threadIndex) {
    s_isWorkerThread = true;

#ifdef __APPLE__
    // Set thread name on macOS (must be done from the thread itself)
    std::string name = "Nova_Worker_" + std::to_string(threadIndex);
    pthread_setname_np(name.c_str());
#endif

    while (m_running) {
        PrioritizedJob job;

        {
            std::unique_lock lock(m_queueMutex);

            // Wait for work or shutdown signal
            m_condition.wait(lock, [this] {
                return !m_jobQueue.empty() || !m_running;
            });

            if (!m_running && m_jobQueue.empty()) {
                break;
            }

            if (m_jobQueue.empty()) {
                continue;
            }

            job = m_jobQueue.top();
            m_jobQueue.pop();
        }

        ExecuteJob(job);
    }

    s_isWorkerThread = false;
}

std::optional<JobSystem::PrioritizedJob> JobSystem::TryGetJob() {
    std::lock_guard lock(m_queueMutex);

    if (m_jobQueue.empty()) {
        return std::nullopt;
    }

    auto job = m_jobQueue.top();
    m_jobQueue.pop();
    return job;
}

void JobSystem::ExecuteJob(PrioritizedJob& job) {
    try {
        job.job();
    } catch (const std::exception& e) {
        spdlog::error("Job threw exception: {}", e.what());
    } catch (...) {
        spdlog::error("Job threw unknown exception");
    }

    if (job.completed) {
        job.completed->store(true, std::memory_order_release);
    }

    if (job.counter) {
        job.counter->Decrement();
    }
}

} // namespace Nova
