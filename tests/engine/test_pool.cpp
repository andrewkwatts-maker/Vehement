/**
 * @file test_pool.cpp
 * @brief Unit tests for memory pool systems
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/Pool.hpp"

#include "utils/TestHelpers.hpp"

#include <thread>
#include <atomic>
#include <vector>
#include <set>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// Test Object Types
// =============================================================================

struct SimpleObject {
    int id = 0;
    float value = 0.0f;

    SimpleObject() = default;
    SimpleObject(int i, float v) : id(i), value(v) {}

    bool operator==(const SimpleObject& other) const {
        return id == other.id && FloatEqual(value, other.value);
    }
};

struct ComplexObject {
    std::string name;
    std::vector<int> data;
    int constructorCalls = 0;
    int destructorCalls = 0;

    ComplexObject() : constructorCalls(1) {}
    ComplexObject(const std::string& n) : name(n), constructorCalls(1) {}

    ~ComplexObject() {
        // Note: Can't track destructor calls in the object itself
    }
};

// =============================================================================
// FixedPool Tests
// =============================================================================

class FixedPoolTest : public ::testing::Test {
protected:
    static constexpr size_t kPoolCapacity = 16;
    using TestPool = FixedPool<SimpleObject, kPoolCapacity>;
};

TEST_F(FixedPoolTest, Construction) {
    TestPool pool;

    EXPECT_EQ(0, pool.GetActiveCount());
    EXPECT_EQ(kPoolCapacity, pool.GetCapacity());
}

TEST_F(FixedPoolTest, AllocateSingle) {
    TestPool pool;

    auto [obj, index] = pool.Allocate(42, 3.14f);

    ASSERT_NE(nullptr, obj);
    EXPECT_NE(TestPool::INVALID_INDEX, index);
    EXPECT_EQ(42, obj->id);
    EXPECT_FLOAT_EQ(3.14f, obj->value);
    EXPECT_EQ(1, pool.GetActiveCount());
}

TEST_F(FixedPoolTest, AllocateMultiple) {
    TestPool pool;

    std::vector<std::pair<SimpleObject*, TestPool::size_type>> allocations;

    for (int i = 0; i < 10; ++i) {
        allocations.push_back(pool.Allocate(i, static_cast<float>(i)));
    }

    EXPECT_EQ(10, pool.GetActiveCount());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(i, allocations[i].first->id);
    }
}

TEST_F(FixedPoolTest, AllocateUntilFull) {
    TestPool pool;

    // Fill the pool
    for (size_t i = 0; i < kPoolCapacity; ++i) {
        auto [obj, index] = pool.Allocate();
        EXPECT_NE(nullptr, obj);
    }

    EXPECT_EQ(kPoolCapacity, pool.GetActiveCount());

    // Try to allocate one more
    auto [obj, index] = pool.Allocate();
    EXPECT_EQ(nullptr, obj);
    EXPECT_EQ(TestPool::INVALID_INDEX, index);
}

TEST_F(FixedPoolTest, Deallocate) {
    TestPool pool;

    auto [obj1, idx1] = pool.Allocate(1, 1.0f);
    auto [obj2, idx2] = pool.Allocate(2, 2.0f);
    auto [obj3, idx3] = pool.Allocate(3, 3.0f);

    EXPECT_EQ(3, pool.GetActiveCount());

    pool.Deallocate(idx2);

    EXPECT_EQ(2, pool.GetActiveCount());
    EXPECT_FALSE(pool.IsActive(idx2));
    EXPECT_TRUE(pool.IsActive(idx1));
    EXPECT_TRUE(pool.IsActive(idx3));
}

TEST_F(FixedPoolTest, ReallocateAfterDeallocate) {
    TestPool pool;

    auto [obj1, idx1] = pool.Allocate(1, 1.0f);
    pool.Deallocate(idx1);

    auto [obj2, idx2] = pool.Allocate(2, 2.0f);

    // Should reuse the same index
    EXPECT_EQ(idx1, idx2);
    EXPECT_EQ(2, obj2->id);
}

TEST_F(FixedPoolTest, Get) {
    TestPool pool;

    auto [obj, index] = pool.Allocate(42, 3.14f);

    SimpleObject* retrieved = pool.Get(index);
    EXPECT_EQ(obj, retrieved);
    EXPECT_EQ(42, retrieved->id);
}

TEST_F(FixedPoolTest, IsActive) {
    TestPool pool;

    auto [obj, index] = pool.Allocate();

    EXPECT_TRUE(pool.IsActive(index));
    EXPECT_FALSE(pool.IsActive(100));  // Invalid index

    pool.Deallocate(index);
    EXPECT_FALSE(pool.IsActive(index));
}

TEST_F(FixedPoolTest, ForEach) {
    TestPool pool;

    for (int i = 0; i < 5; ++i) {
        pool.Allocate(i, 0.0f);
    }

    int sum = 0;
    pool.ForEach([&sum](SimpleObject& obj, TestPool::size_type) {
        sum += obj.id;
    });

    EXPECT_EQ(0 + 1 + 2 + 3 + 4, sum);
}

TEST_F(FixedPoolTest, Clear) {
    TestPool pool;

    for (int i = 0; i < 10; ++i) {
        pool.Allocate(i, 0.0f);
    }

    pool.Clear();

    EXPECT_EQ(0, pool.GetActiveCount());
}

// =============================================================================
// ThreadSafePool Tests
// =============================================================================

class ThreadSafePoolTest : public ::testing::Test {
protected:
    using TestPool = ThreadSafePool<SimpleObject, 64>;
};

TEST_F(ThreadSafePoolTest, Construction) {
    PoolConfig config;
    config.initialCapacity = 64;
    config.maxCapacity = 1024;

    TestPool pool(config);

    EXPECT_EQ(0, pool.GetActiveCount());
    EXPECT_GE(pool.GetCapacity(), 64);
}

TEST_F(ThreadSafePoolTest, AllocateDeallocate) {
    TestPool pool;

    auto handle = pool.Allocate(42, 3.14f);
    EXPECT_TRUE(handle.IsValid());

    auto* obj = pool.Get(handle);
    ASSERT_NE(nullptr, obj);
    EXPECT_EQ(42, obj->id);

    pool.Deallocate(handle);
    EXPECT_FALSE(pool.IsValid(handle));
}

TEST_F(ThreadSafePoolTest, HandleToIndex) {
    TestPool pool;

    auto handle1 = pool.Allocate();
    auto handle2 = pool.Allocate();
    auto handle3 = pool.Allocate();

    // Each handle should have a unique index
    EXPECT_NE(handle1.ToIndex(), handle2.ToIndex());
    EXPECT_NE(handle2.ToIndex(), handle3.ToIndex());
}

TEST_F(ThreadSafePoolTest, GrowOnDemand) {
    PoolConfig config;
    config.initialCapacity = 16;
    config.maxCapacity = 256;
    config.growOnDemand = true;

    TestPool pool(config);

    // Allocate more than initial capacity
    std::vector<TestPool::Handle> handles;
    for (int i = 0; i < 100; ++i) {
        auto handle = pool.Allocate(i, 0.0f);
        EXPECT_TRUE(handle.IsValid());
        handles.push_back(handle);
    }

    EXPECT_EQ(100, pool.GetActiveCount());
    EXPECT_GE(pool.GetCapacity(), 100);
}

TEST_F(ThreadSafePoolTest, ConcurrentAllocation) {
    TestPool pool;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    const int allocsPerThread = 50;
    const int numThreads = 4;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&pool, &successCount, t]() {
            for (int i = 0; i < allocsPerThread; ++i) {
                auto handle = pool.Allocate(t * 1000 + i, 0.0f);
                if (handle.IsValid()) {
                    successCount++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(numThreads * allocsPerThread, successCount);
    EXPECT_EQ(numThreads * allocsPerThread, static_cast<int>(pool.GetActiveCount()));
}

TEST_F(ThreadSafePoolTest, ConcurrentAllocateDeallocate) {
    TestPool pool;
    std::vector<std::thread> threads;
    std::atomic<int> operationCount{0};

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&pool, &operationCount]() {
            for (int i = 0; i < 100; ++i) {
                auto handle = pool.Allocate();
                if (handle.IsValid()) {
                    std::this_thread::yield();
                    pool.Deallocate(handle);
                    operationCount++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(400, operationCount);
    EXPECT_EQ(0, pool.GetActiveCount());
}

// =============================================================================
// LockFreePool Tests
// =============================================================================

class LockFreePoolTest : public ::testing::Test {
protected:
    static constexpr size_t kPoolCapacity = 128;
    using TestPool = LockFreePool<SimpleObject, kPoolCapacity>;
};

TEST_F(LockFreePoolTest, Construction) {
    TestPool pool;
    EXPECT_EQ(0, pool.GetActiveCount());
}

TEST_F(LockFreePoolTest, AllocateDeallocate) {
    TestPool pool;

    auto [obj, index] = pool.Allocate(42, 3.14f);
    ASSERT_NE(nullptr, obj);
    EXPECT_EQ(42, obj->id);
    EXPECT_EQ(1, pool.GetActiveCount());

    pool.Deallocate(index);
    EXPECT_EQ(0, pool.GetActiveCount());
}

TEST_F(LockFreePoolTest, ConcurrentOperations) {
    TestPool pool;
    std::vector<std::thread> threads;
    std::atomic<int> allocSuccess{0};
    std::atomic<int> deallocSuccess{0};

    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&pool, &allocSuccess, &deallocSuccess, t]() {
            std::vector<LockFreePoolTest::TestPool::size_type> myIndices;

            // Allocate
            for (int i = 0; i < 10; ++i) {
                auto [obj, index] = pool.Allocate(t * 100 + i, 0.0f);
                if (obj) {
                    myIndices.push_back(index);
                    allocSuccess++;
                }
            }

            std::this_thread::yield();

            // Deallocate
            for (auto index : myIndices) {
                pool.Deallocate(index);
                deallocSuccess++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(allocSuccess.load(), deallocSuccess.load());
    EXPECT_EQ(0, pool.GetActiveCount());
}

TEST_F(LockFreePoolTest, NoABAProblem) {
    // Test that ABA problem is handled by tagged pointers
    TestPool pool;

    // Allocate, deallocate, reallocate pattern
    auto [obj1, idx1] = pool.Allocate(1, 1.0f);
    pool.Deallocate(idx1);

    auto [obj2, idx2] = pool.Allocate(2, 2.0f);
    pool.Deallocate(idx2);

    auto [obj3, idx3] = pool.Allocate(3, 3.0f);

    // Object should have correct value despite reuse
    EXPECT_EQ(3, pool.Get(idx3)->id);
}

// =============================================================================
// PooledHandle Tests
// =============================================================================

TEST(PooledHandleTest, RAIIDeallocation) {
    ThreadSafePool<SimpleObject> pool;

    {
        auto handle = pool.Allocate(42, 3.14f);
        PooledHandle<ThreadSafePool<SimpleObject>> scoped(pool, handle);

        EXPECT_TRUE(scoped.IsValid());
        EXPECT_EQ(42, scoped->id);
        EXPECT_EQ(1, pool.GetActiveCount());
    }

    // Should be deallocated when scoped goes out of scope
    EXPECT_EQ(0, pool.GetActiveCount());
}

TEST(PooledHandleTest, MoveSemantics) {
    ThreadSafePool<SimpleObject> pool;

    auto handle = pool.Allocate(42, 3.14f);
    PooledHandle<ThreadSafePool<SimpleObject>> h1(pool, handle);

    auto h2 = std::move(h1);

    EXPECT_FALSE(h1.IsValid());
    EXPECT_TRUE(h2.IsValid());
    EXPECT_EQ(42, h2->id);
}

TEST(PooledHandleTest, Reset) {
    ThreadSafePool<SimpleObject> pool;

    auto handle = pool.Allocate(42, 3.14f);
    PooledHandle<ThreadSafePool<SimpleObject>> scoped(pool, handle);

    scoped.Reset();

    EXPECT_FALSE(scoped.IsValid());
    EXPECT_EQ(0, pool.GetActiveCount());
}

// =============================================================================
// FrameAllocator Tests
// =============================================================================

class FrameAllocatorTest : public ::testing::Test {
protected:
    static constexpr size_t kCapacity = 4096;
};

TEST_F(FrameAllocatorTest, Construction) {
    FrameAllocator allocator(kCapacity);

    EXPECT_EQ(0, allocator.GetUsed());
    EXPECT_EQ(kCapacity, allocator.GetCapacity());
}

TEST_F(FrameAllocatorTest, AllocatePrimitive) {
    FrameAllocator allocator(kCapacity);

    int* intPtr = allocator.Allocate<int>();
    ASSERT_NE(nullptr, intPtr);
    *intPtr = 42;
    EXPECT_EQ(42, *intPtr);

    EXPECT_GE(allocator.GetUsed(), sizeof(int));
}

TEST_F(FrameAllocatorTest, AllocateArray) {
    FrameAllocator allocator(kCapacity);

    float* floats = allocator.Allocate<float>(10);
    ASSERT_NE(nullptr, floats);

    for (int i = 0; i < 10; ++i) {
        floats[i] = static_cast<float>(i);
    }

    EXPECT_FLOAT_EQ(5.0f, floats[5]);
}

TEST_F(FrameAllocatorTest, Create) {
    FrameAllocator allocator(kCapacity);

    SimpleObject* obj = allocator.Create<SimpleObject>(100, 2.5f);
    ASSERT_NE(nullptr, obj);
    EXPECT_EQ(100, obj->id);
    EXPECT_FLOAT_EQ(2.5f, obj->value);
}

TEST_F(FrameAllocatorTest, Reset) {
    FrameAllocator allocator(kCapacity);

    for (int i = 0; i < 100; ++i) {
        allocator.Allocate<int>();
    }

    size_t usedBefore = allocator.GetUsed();
    EXPECT_GT(usedBefore, 0);

    allocator.Reset();

    EXPECT_EQ(0, allocator.GetUsed());
    EXPECT_EQ(0, allocator.GetAllocationCount());
}

TEST_F(FrameAllocatorTest, Alignment) {
    FrameAllocator allocator(kCapacity);

    // Allocate byte to potentially misalign
    allocator.Allocate<char>();

    // Allocate aligned type
    alignas(16) struct AlignedType {
        char data[16];
    };

    AlignedType* aligned = allocator.Allocate<AlignedType>();
    ASSERT_NE(nullptr, aligned);

    // Check alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(aligned);
    EXPECT_EQ(0, addr % alignof(AlignedType));
}

TEST_F(FrameAllocatorTest, OutOfMemory) {
    FrameAllocator allocator(64);  // Small capacity

    // Allocate until full
    while (allocator.Allocate<int>() != nullptr) {
        // Keep allocating
    }

    // Next allocation should return nullptr
    int* ptr = allocator.Allocate<int>();
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(FrameAllocatorTest, AllocationCount) {
    FrameAllocator allocator(kCapacity);

    for (int i = 0; i < 50; ++i) {
        allocator.Allocate<int>();
    }

    EXPECT_EQ(50, allocator.GetAllocationCount());
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST(PoolPerformanceTest, FixedPoolThroughput) {
    constexpr size_t iterations = 10000;
    FixedPool<SimpleObject, 1024> pool;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        auto [obj, idx] = pool.Allocate(static_cast<int>(i), 0.0f);
        pool.Deallocate(idx);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double opsPerSecond = (iterations * 2.0 * 1000000.0) / duration.count();

    // Should achieve at least 1M ops/sec
    EXPECT_GT(opsPerSecond, 1000000.0);
}

TEST(PoolPerformanceTest, FrameAllocatorReset) {
    constexpr size_t iterations = 1000;
    FrameAllocator allocator(1024 * 1024);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        // Simulate frame allocations
        for (int j = 0; j < 100; ++j) {
            allocator.Allocate<float>(10);
        }
        allocator.Reset();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Reset should be O(1) - very fast
    double avgResetTime = duration.count() / static_cast<double>(iterations);
    EXPECT_LT(avgResetTime, 10.0);  // Less than 10 microseconds per reset
}
