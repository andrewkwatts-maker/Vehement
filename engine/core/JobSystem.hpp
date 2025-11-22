#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <memory>
#include <optional>
#include <array>
#include <string>
#include <cassert>

namespace Nova {

/**
 * @brief Job priority levels
 */
enum class JobPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief Configuration for job system from JSON
 */
struct JobSystemConfig {
    uint32_t workerThreads = 0;  // 0 = auto (hardware_concurrency - 1)
    bool enablePriorities = true;
    size_t queueCapacity = 4096;
    std::string threadNamePrefix = "Nova_Worker_";
};

/**
 * @brief Lightweight job handle for tracking completion
 */
class JobHandle {
public:
    JobHandle() = default;
    explicit JobHandle(std::shared_ptr<std::atomic<bool>> completed)
        : m_completed(std::move(completed)) {}

    /**
     * @brief Check if job is complete (non-blocking)
     */
    [[nodiscard]] bool IsComplete() const {
        return !m_completed || m_completed->load(std::memory_order_acquire);
    }

    /**
     * @brief Wait for job completion (blocking)
     */
    void Wait() const {
        if (m_completed) {
            while (!m_completed->load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    }

    /**
     * @brief Check if handle is valid
     */
    [[nodiscard]] bool IsValid() const { return m_completed != nullptr; }

private:
    std::shared_ptr<std::atomic<bool>> m_completed;
};

/**
 * @brief Counter for batch job synchronization
 */
class JobCounter {
public:
    explicit JobCounter(uint32_t count = 0)
        : m_count(count) {}

    void Increment(uint32_t n = 1) {
        m_count.fetch_add(n, std::memory_order_release);
    }

    void Decrement() {
        m_count.fetch_sub(1, std::memory_order_release);
        m_cv.notify_all();
    }

    [[nodiscard]] bool IsComplete() const {
        return m_count.load(std::memory_order_acquire) == 0;
    }

    void Wait() {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [this] {
            return m_count.load(std::memory_order_acquire) == 0;
        });
    }

    [[nodiscard]] uint32_t GetCount() const {
        return m_count.load(std::memory_order_acquire);
    }

private:
    std::atomic<uint32_t> m_count;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

/**
 * @brief Simple, efficient job system for parallel work distribution
 *
 * Features:
 * - Work-stealing for load balancing
 * - Priority queues for critical work
 * - Batch job support with counters
 * - Parallel-for helper
 */
class JobSystem {
public:
    using Job = std::function<void()>;
    using JobWithData = std::function<void(void*)>;

    /**
     * @brief Get singleton instance
     */
    static JobSystem& Instance() {
        static JobSystem instance;
        return instance;
    }

    // Delete copy/move
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    JobSystem(JobSystem&&) = delete;
    JobSystem& operator=(JobSystem&&) = delete;

    /**
     * @brief Initialize the job system
     * @param config Configuration options
     * @return true on success
     */
    [[nodiscard]] bool Initialize(const JobSystemConfig& config = {});

    /**
     * @brief Shutdown the job system
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Submit a job to the queue
     * @param job Function to execute
     * @param priority Job priority
     * @return Handle to track completion
     */
    [[nodiscard]] JobHandle Submit(Job job, JobPriority priority = JobPriority::Normal);

    /**
     * @brief Submit a job with associated counter (for batch operations)
     * @param job Function to execute
     * @param counter Counter to decrement on completion
     * @param priority Job priority
     */
    void Submit(Job job, JobCounter& counter, JobPriority priority = JobPriority::Normal);

    /**
     * @brief Submit multiple jobs and return when all complete
     * @param jobs Vector of jobs to execute
     * @param priority Job priority
     */
    void SubmitAndWait(const std::vector<Job>& jobs, JobPriority priority = JobPriority::Normal);

    /**
     * @brief Parallel for loop
     * @param start Start index (inclusive)
     * @param end End index (exclusive)
     * @param batchSize Number of iterations per job
     * @param func Function called with (index)
     */
    void ParallelFor(size_t start, size_t end, size_t batchSize,
                     const std::function<void(size_t)>& func);

    /**
     * @brief Parallel for loop with automatic batching
     */
    void ParallelFor(size_t count, const std::function<void(size_t)>& func);

    /**
     * @brief Parallel for loop over a range with data
     * @param start Start index
     * @param end End index
     * @param func Function called with (startIndex, endIndex)
     */
    void ParallelForRange(size_t start, size_t end,
                          const std::function<void(size_t, size_t)>& func);

    /**
     * @brief Get number of worker threads
     */
    [[nodiscard]] uint32_t GetWorkerCount() const { return m_workerCount; }

    /**
     * @brief Get approximate number of pending jobs
     */
    [[nodiscard]] size_t GetPendingJobCount() const;

    /**
     * @brief Check if current thread is a worker thread
     */
    [[nodiscard]] bool IsWorkerThread() const;

    /**
     * @brief Yield current thread to process jobs (useful for main thread)
     * @return true if a job was processed
     */
    bool YieldAndProcess();

private:
    JobSystem() = default;
    ~JobSystem();

    struct PrioritizedJob {
        Job job;
        std::shared_ptr<std::atomic<bool>> completed;
        JobCounter* counter = nullptr;
        JobPriority priority = JobPriority::Normal;

        bool operator<(const PrioritizedJob& other) const {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
    };

    void WorkerLoop(uint32_t threadIndex);
    std::optional<PrioritizedJob> TryGetJob();
    void ExecuteJob(PrioritizedJob& job);

    // Worker threads
    std::vector<std::thread> m_workers;
    uint32_t m_workerCount = 0;

    // Job queue with priority support
    std::priority_queue<PrioritizedJob> m_jobQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_condition;

    // State
    std::atomic<bool> m_running{false};
    bool m_initialized = false;

    // Thread identification
    thread_local static bool s_isWorkerThread;
};

/**
 * @brief RAII helper for scoped parallel work
 */
class ScopedParallelWork {
public:
    explicit ScopedParallelWork(const std::string& name = "") : m_name(name) {}

    ~ScopedParallelWork() {
        Wait();
    }

    void AddJob(JobSystem::Job job, JobPriority priority = JobPriority::Normal) {
        JobSystem::Instance().Submit(std::move(job), m_counter, priority);
    }

    void Wait() {
        m_counter.Wait();
    }

    [[nodiscard]] bool IsComplete() const {
        return m_counter.IsComplete();
    }

private:
    std::string m_name;
    JobCounter m_counter{0};
};

/**
 * @brief Parallel algorithm helpers
 */
namespace Parallel {

/**
 * @brief Parallel transform (map)
 */
template<typename InputIt, typename OutputIt, typename UnaryOp>
void Transform(InputIt first, InputIt last, OutputIt result, UnaryOp op) {
    size_t count = std::distance(first, last);
    if (count == 0) return;

    JobSystem::Instance().ParallelFor(0, count, [&](size_t i) {
        result[i] = op(first[i]);
    });
}

/**
 * @brief Parallel reduce
 */
template<typename InputIt, typename T, typename BinaryOp>
T Reduce(InputIt first, InputIt last, T init, BinaryOp op) {
    size_t count = std::distance(first, last);
    if (count == 0) return init;

    uint32_t numThreads = JobSystem::Instance().GetWorkerCount() + 1;
    std::vector<T> partialResults(numThreads, init);

    size_t chunkSize = (count + numThreads - 1) / numThreads;

    JobCounter counter(numThreads);
    for (uint32_t t = 0; t < numThreads; ++t) {
        size_t start = t * chunkSize;
        size_t end = std::min(start + chunkSize, count);

        JobSystem::Instance().Submit([&, t, start, end]() {
            T local = init;
            for (size_t i = start; i < end; ++i) {
                local = op(local, first[i]);
            }
            partialResults[t] = local;
        }, counter);
    }

    counter.Wait();

    T result = init;
    for (const auto& partial : partialResults) {
        result = op(result, partial);
    }
    return result;
}

/**
 * @brief Parallel sort (uses std::sort internally with parallel partition)
 */
template<typename RandomIt, typename Compare>
void Sort(RandomIt first, RandomIt last, Compare comp, size_t threshold = 10000) {
    size_t count = std::distance(first, last);

    if (count <= threshold) {
        std::sort(first, last, comp);
        return;
    }

    // Simple parallel quicksort with limited depth
    auto mid = first + count / 2;
    std::nth_element(first, mid, last, comp);

    JobCounter counter(2);

    JobSystem::Instance().Submit([&]() {
        Sort(first, mid, comp, threshold);
    }, counter);

    JobSystem::Instance().Submit([&]() {
        Sort(mid, last, comp, threshold);
    }, counter);

    counter.Wait();
}

template<typename RandomIt>
void Sort(RandomIt first, RandomIt last) {
    Sort(first, last, std::less<>{});
}

} // namespace Parallel

} // namespace Nova
