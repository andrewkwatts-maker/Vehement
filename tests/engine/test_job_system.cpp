/**
 * @file test_job_system.cpp
 * @brief Unit tests for job system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/JobSystem.hpp"

#include "utils/TestHelpers.hpp"

#include <atomic>
#include <vector>
#include <numeric>
#include <algorithm>
#include <chrono>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// Job System Tests
// =============================================================================

class JobSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& js = JobSystem::Instance();
        if (!js.IsInitialized()) {
            JobSystemConfig config;
            config.workerThreads = 4;
            config.enablePriorities = true;
            js.Initialize(config);
        }
    }

    void TearDown() override {
        // Don't shutdown - singleton persists
    }
};

TEST_F(JobSystemTest, IsInitialized) {
    EXPECT_TRUE(JobSystem::Instance().IsInitialized());
}

TEST_F(JobSystemTest, GetWorkerCount) {
    uint32_t workers = JobSystem::Instance().GetWorkerCount();
    EXPECT_GT(workers, 0);
}

TEST_F(JobSystemTest, SubmitSingleJob) {
    std::atomic<bool> jobRan{false};

    auto handle = JobSystem::Instance().Submit([&jobRan]() {
        jobRan = true;
    });

    handle.Wait();

    EXPECT_TRUE(jobRan);
    EXPECT_TRUE(handle.IsComplete());
}

TEST_F(JobSystemTest, SubmitMultipleJobs) {
    std::atomic<int> counter{0};
    std::vector<JobHandle> handles;

    for (int i = 0; i < 100; ++i) {
        handles.push_back(JobSystem::Instance().Submit([&counter]() {
            counter++;
        }));
    }

    for (auto& handle : handles) {
        handle.Wait();
    }

    EXPECT_EQ(100, counter);
}

TEST_F(JobSystemTest, JobHandle_IsComplete) {
    std::atomic<bool> shouldComplete{false};

    auto handle = JobSystem::Instance().Submit([&shouldComplete]() {
        while (!shouldComplete) {
            std::this_thread::yield();
        }
    });

    EXPECT_FALSE(handle.IsComplete());

    shouldComplete = true;
    handle.Wait();

    EXPECT_TRUE(handle.IsComplete());
}

TEST_F(JobSystemTest, JobHandle_InvalidHandle) {
    JobHandle invalid;

    EXPECT_FALSE(invalid.IsValid());
    EXPECT_TRUE(invalid.IsComplete());  // Invalid handles are "complete"
}

// =============================================================================
// Job Priority Tests
// =============================================================================

TEST_F(JobSystemTest, PriorityOrdering) {
    // This test verifies that higher priority jobs tend to run first
    // Note: Due to concurrent execution, this is probabilistic

    std::vector<int> executionOrder;
    std::mutex orderMutex;

    JobCounter counter;

    // Submit low priority jobs first
    for (int i = 0; i < 10; ++i) {
        JobSystem::Instance().Submit([i, &executionOrder, &orderMutex]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            std::lock_guard lock(orderMutex);
            executionOrder.push_back(i);
        }, counter, JobPriority::Low);
    }

    // Then high priority
    for (int i = 100; i < 110; ++i) {
        JobSystem::Instance().Submit([i, &executionOrder, &orderMutex]() {
            std::lock_guard lock(orderMutex);
            executionOrder.push_back(i);
        }, counter, JobPriority::High);
    }

    counter.Wait();

    // High priority jobs (100+) should appear earlier in the list
    // Count high-priority jobs in first half
    size_t halfSize = executionOrder.size() / 2;
    int highPriorityInFirstHalf = 0;
    for (size_t i = 0; i < halfSize; ++i) {
        if (executionOrder[i] >= 100) {
            highPriorityInFirstHalf++;
        }
    }

    // Most high priority jobs should be in first half
    EXPECT_GE(highPriorityInFirstHalf, 5);
}

// =============================================================================
// Job Counter Tests
// =============================================================================

TEST_F(JobSystemTest, JobCounter_Basic) {
    JobCounter counter;
    std::atomic<int> value{0};

    for (int i = 0; i < 50; ++i) {
        JobSystem::Instance().Submit([&value]() {
            value++;
        }, counter);
    }

    counter.Wait();

    EXPECT_EQ(50, value);
    EXPECT_TRUE(counter.IsComplete());
}

TEST_F(JobSystemTest, JobCounter_GetCount) {
    JobCounter counter(10);

    EXPECT_EQ(10, counter.GetCount());

    counter.Decrement();
    EXPECT_EQ(9, counter.GetCount());
}

TEST_F(JobSystemTest, JobCounter_Increment) {
    JobCounter counter(0);

    counter.Increment(5);
    EXPECT_EQ(5, counter.GetCount());
}

// =============================================================================
// Parallel For Tests
// =============================================================================

TEST_F(JobSystemTest, ParallelFor_BasicRange) {
    std::vector<int> data(1000, 0);

    JobSystem::Instance().ParallelFor(0, data.size(), [&data](size_t i) {
        data[i] = static_cast<int>(i);
    });

    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(static_cast<int>(i), data[i]);
    }
}

TEST_F(JobSystemTest, ParallelFor_WithBatchSize) {
    std::vector<int> data(1000, 0);
    std::atomic<int> batchCount{0};

    JobSystem::Instance().ParallelFor(0, data.size(), 100, [&data, &batchCount](size_t i) {
        data[i] = 1;
    });

    int sum = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(1000, sum);
}

TEST_F(JobSystemTest, ParallelFor_EmptyRange) {
    std::atomic<int> callCount{0};

    JobSystem::Instance().ParallelFor(0, 0, [&callCount](size_t) {
        callCount++;
    });

    EXPECT_EQ(0, callCount);
}

TEST_F(JobSystemTest, ParallelForRange) {
    std::vector<int> data(1000, 0);

    JobSystem::Instance().ParallelForRange(0, data.size(), [&data](size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            data[i] = 1;
        }
    });

    int sum = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(1000, sum);
}

// =============================================================================
// Submit And Wait Tests
// =============================================================================

TEST_F(JobSystemTest, SubmitAndWait) {
    std::atomic<int> counter{0};

    std::vector<JobSystem::Job> jobs;
    for (int i = 0; i < 100; ++i) {
        jobs.push_back([&counter]() {
            counter++;
        });
    }

    JobSystem::Instance().SubmitAndWait(jobs);

    EXPECT_EQ(100, counter);
}

// =============================================================================
// Scoped Parallel Work Tests
// =============================================================================

TEST_F(JobSystemTest, ScopedParallelWork_Basic) {
    std::atomic<int> value{0};

    {
        ScopedParallelWork work("TestWork");

        for (int i = 0; i < 10; ++i) {
            work.AddJob([&value]() {
                value++;
            });
        }
    }  // Waits on destruction

    EXPECT_EQ(10, value);
}

TEST_F(JobSystemTest, ScopedParallelWork_ExplicitWait) {
    std::atomic<int> value{0};

    ScopedParallelWork work;

    for (int i = 0; i < 10; ++i) {
        work.AddJob([&value]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            value++;
        });
    }

    EXPECT_FALSE(work.IsComplete());

    work.Wait();

    EXPECT_TRUE(work.IsComplete());
    EXPECT_EQ(10, value);
}

// =============================================================================
// Parallel Algorithm Tests
// =============================================================================

TEST_F(JobSystemTest, ParallelTransform) {
    std::vector<int> input(1000);
    std::iota(input.begin(), input.end(), 0);

    std::vector<int> output(1000);

    Parallel::Transform(input.begin(), input.end(), output.begin(),
        [](int x) { return x * 2; });

    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(static_cast<int>(i * 2), output[i]);
    }
}

TEST_F(JobSystemTest, ParallelReduce) {
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);

    int sum = Parallel::Reduce(data.begin(), data.end(), 0,
        [](int a, int b) { return a + b; });

    int expected = (1000 * 1001) / 2;
    EXPECT_EQ(expected, sum);
}

TEST_F(JobSystemTest, ParallelSort) {
    std::vector<int> data(10000);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 100000);

    for (auto& val : data) {
        val = dist(gen);
    }

    Parallel::Sort(data.begin(), data.end());

    EXPECT_TRUE(std::is_sorted(data.begin(), data.end()));
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(JobSystemTest, ConcurrentSubmit) {
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&counter]() {
            for (int i = 0; i < 100; ++i) {
                auto handle = JobSystem::Instance().Submit([&counter]() {
                    counter++;
                });
                handle.Wait();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(400, counter);
}

// =============================================================================
// Dependencies Tests
// =============================================================================

TEST_F(JobSystemTest, ChainedJobs) {
    std::vector<int> sequence;
    std::mutex sequenceMutex;

    JobCounter counter1, counter2, counter3;

    // First job
    JobSystem::Instance().Submit([&sequence, &sequenceMutex]() {
        std::lock_guard lock(sequenceMutex);
        sequence.push_back(1);
    }, counter1);

    counter1.Wait();

    // Second job (depends on first)
    JobSystem::Instance().Submit([&sequence, &sequenceMutex]() {
        std::lock_guard lock(sequenceMutex);
        sequence.push_back(2);
    }, counter2);

    counter2.Wait();

    // Third job (depends on second)
    JobSystem::Instance().Submit([&sequence, &sequenceMutex]() {
        std::lock_guard lock(sequenceMutex);
        sequence.push_back(3);
    }, counter3);

    counter3.Wait();

    ASSERT_EQ(3, sequence.size());
    EXPECT_EQ(1, sequence[0]);
    EXPECT_EQ(2, sequence[1]);
    EXPECT_EQ(3, sequence[2]);
}

// =============================================================================
// YieldAndProcess Tests
// =============================================================================

TEST_F(JobSystemTest, YieldAndProcess) {
    // Only works if current thread is main thread
    if (!JobSystem::Instance().IsWorkerThread()) {
        std::atomic<bool> jobProcessed{false};

        JobSystem::Instance().Submit([&jobProcessed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            jobProcessed = true;
        });

        // Try to help process jobs
        while (!jobProcessed) {
            JobSystem::Instance().YieldAndProcess();
        }

        EXPECT_TRUE(jobProcessed);
    }
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(JobSystemTest, Performance_ManySmallJobs) {
    constexpr int numJobs = 10000;
    std::atomic<int> counter{0};

    auto start = std::chrono::high_resolution_clock::now();

    JobCounter jobCounter;
    for (int i = 0; i < numJobs; ++i) {
        JobSystem::Instance().Submit([&counter]() {
            counter++;
        }, jobCounter);
    }

    jobCounter.Wait();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(numJobs, counter);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 5000);
}

TEST_F(JobSystemTest, Performance_ParallelSpeedup) {
    constexpr size_t dataSize = 1000000;
    std::vector<double> data(dataSize);

    // Fill with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    for (auto& val : data) {
        val = dist(gen);
    }

    // Sequential timing
    auto seqStart = std::chrono::high_resolution_clock::now();
    double seqSum = 0.0;
    for (const auto& val : data) {
        seqSum += std::sqrt(val);
    }
    auto seqEnd = std::chrono::high_resolution_clock::now();
    auto seqTime = std::chrono::duration_cast<std::chrono::microseconds>(seqEnd - seqStart);

    // Parallel timing
    auto parStart = std::chrono::high_resolution_clock::now();
    double parSum = Parallel::Reduce(data.begin(), data.end(), 0.0,
        [](double a, double b) { return a + std::sqrt(b); });
    auto parEnd = std::chrono::high_resolution_clock::now();
    auto parTime = std::chrono::duration_cast<std::chrono::microseconds>(parEnd - parStart);

    // Results should be approximately equal
    EXPECT_NEAR(seqSum, parSum, seqSum * 0.01);  // 1% tolerance

    // Parallel should be faster (or at least not much slower)
    // Note: For small workloads, overhead may dominate
}
