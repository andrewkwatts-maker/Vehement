#pragma once

#include <vector>
#include <array>
#include <memory>
#include <atomic>
#include <mutex>
#include <type_traits>
#include <cassert>
#include <cstddef>
#include <new>
#include <functional>
#include <optional>

namespace Nova {

/**
 * @brief Configuration for object pools loaded from JSON
 */
struct PoolConfig {
    size_t initialCapacity = 64;
    size_t maxCapacity = 4096;
    bool growOnDemand = true;
    bool threadSafe = true;
};

/**
 * @brief Lock-free object pool for single-threaded hot paths
 *
 * Uses a free list with indices for O(1) allocation and deallocation.
 * No virtual calls, no allocations after initialization.
 *
 * @tparam T Object type to pool
 * @tparam Capacity Maximum number of objects (compile-time for best performance)
 */
template<typename T, size_t Capacity>
class FixedPool {
public:
    using value_type = T;
    using size_type = uint32_t;
    static constexpr size_type INVALID_INDEX = ~size_type{0};

    FixedPool() {
        // Initialize free list - each slot points to next free slot
        for (size_type i = 0; i < Capacity - 1; ++i) {
            m_freeList[i] = i + 1;
        }
        m_freeList[Capacity - 1] = INVALID_INDEX;
        m_freeHead = 0;
        m_activeCount = 0;
    }

    ~FixedPool() {
        // Destruct all active objects
        for (size_type i = 0; i < Capacity; ++i) {
            if (IsActive(i)) {
                GetObject(i)->~T();
            }
        }
    }

    // Non-copyable, movable
    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;
    FixedPool(FixedPool&&) noexcept = default;
    FixedPool& operator=(FixedPool&&) noexcept = default;

    /**
     * @brief Allocate an object from the pool
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pair of (pointer, index) or (nullptr, INVALID_INDEX) if full
     */
    template<typename... Args>
    [[nodiscard]] std::pair<T*, size_type> Allocate(Args&&... args) {
        if (m_freeHead == INVALID_INDEX) {
            return {nullptr, INVALID_INDEX};
        }

        size_type index = m_freeHead;
        m_freeHead = m_freeList[index];
        m_active[index] = true;
        ++m_activeCount;

        // Construct in-place
        T* obj = new (GetStorage(index)) T(std::forward<Args>(args)...);
        return {obj, index};
    }

    /**
     * @brief Deallocate an object back to the pool
     * @param index Index returned from Allocate
     */
    void Deallocate(size_type index) {
        assert(index < Capacity && "Invalid pool index");
        assert(m_active[index] && "Double-free detected");

        // Destruct
        GetObject(index)->~T();
        m_active[index] = false;
        --m_activeCount;

        // Add to free list
        m_freeList[index] = m_freeHead;
        m_freeHead = index;
    }

    /**
     * @brief Get object by index
     */
    [[nodiscard]] T* Get(size_type index) {
        assert(index < Capacity && m_active[index]);
        return GetObject(index);
    }

    [[nodiscard]] const T* Get(size_type index) const {
        assert(index < Capacity && m_active[index]);
        return GetObject(index);
    }

    /**
     * @brief Check if index is valid and active
     */
    [[nodiscard]] bool IsActive(size_type index) const {
        return index < Capacity && m_active[index];
    }

    /**
     * @brief Get number of active objects
     */
    [[nodiscard]] size_type GetActiveCount() const { return m_activeCount; }

    /**
     * @brief Get capacity
     */
    [[nodiscard]] static constexpr size_type GetCapacity() { return Capacity; }

    /**
     * @brief Iterate over all active objects
     */
    template<typename Func>
    void ForEach(Func&& func) {
        for (size_type i = 0; i < Capacity; ++i) {
            if (m_active[i]) {
                func(*GetObject(i), i);
            }
        }
    }

    template<typename Func>
    void ForEach(Func&& func) const {
        for (size_type i = 0; i < Capacity; ++i) {
            if (m_active[i]) {
                func(*GetObject(i), i);
            }
        }
    }

    /**
     * @brief Clear all objects
     */
    void Clear() {
        for (size_type i = 0; i < Capacity; ++i) {
            if (m_active[i]) {
                Deallocate(i);
            }
        }
    }

private:
    [[nodiscard]] void* GetStorage(size_type index) {
        return &m_storage[index * sizeof(T)];
    }

    [[nodiscard]] T* GetObject(size_type index) {
        return std::launder(reinterpret_cast<T*>(&m_storage[index * sizeof(T)]));
    }

    [[nodiscard]] const T* GetObject(size_type index) const {
        return std::launder(reinterpret_cast<const T*>(&m_storage[index * sizeof(T)]));
    }

    alignas(T) std::array<std::byte, Capacity * sizeof(T)> m_storage;
    std::array<size_type, Capacity> m_freeList;
    std::array<bool, Capacity> m_active{};
    size_type m_freeHead = 0;
    size_type m_activeCount = 0;
};

/**
 * @brief Thread-safe object pool with dynamic growth
 *
 * Uses blocks to grow without invalidating pointers.
 * Supports concurrent allocation/deallocation.
 *
 * @tparam T Object type to pool
 * @tparam BlockSize Objects per block (power of 2 recommended)
 */
template<typename T, size_t BlockSize = 256>
class ThreadSafePool {
public:
    using value_type = T;
    using size_type = uint32_t;
    static constexpr size_type INVALID_HANDLE = ~size_type{0};

    /**
     * @brief Handle to a pooled object (stable across reallocations)
     */
    struct Handle {
        size_type blockIndex = 0;
        size_type localIndex = 0;

        [[nodiscard]] bool IsValid() const {
            return blockIndex != INVALID_HANDLE && localIndex != INVALID_HANDLE;
        }

        [[nodiscard]] size_type ToIndex() const {
            return blockIndex * BlockSize + localIndex;
        }

        static Handle FromIndex(size_type index) {
            return {index / BlockSize, index % BlockSize};
        }

        bool operator==(const Handle& other) const = default;
    };

    static constexpr Handle INVALID{INVALID_HANDLE, INVALID_HANDLE};

    explicit ThreadSafePool(const PoolConfig& config = {})
        : m_config(config) {
        // Pre-allocate initial blocks
        size_t initialBlocks = (config.initialCapacity + BlockSize - 1) / BlockSize;
        for (size_t i = 0; i < initialBlocks; ++i) {
            AllocateBlock();
        }
    }

    ~ThreadSafePool() {
        Clear();
    }

    // Non-copyable, movable
    ThreadSafePool(const ThreadSafePool&) = delete;
    ThreadSafePool& operator=(const ThreadSafePool&) = delete;
    ThreadSafePool(ThreadSafePool&&) noexcept = default;
    ThreadSafePool& operator=(ThreadSafePool&&) noexcept = default;

    /**
     * @brief Allocate an object from the pool (thread-safe)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Handle to allocated object
     */
    template<typename... Args>
    [[nodiscard]] Handle Allocate(Args&&... args) {
        std::lock_guard lock(m_mutex);
        return AllocateInternal(std::forward<Args>(args)...);
    }

    /**
     * @brief Deallocate an object (thread-safe)
     * @param handle Handle to deallocate
     */
    void Deallocate(Handle handle) {
        std::lock_guard lock(m_mutex);
        DeallocateInternal(handle);
    }

    /**
     * @brief Get object by handle (thread-safe read)
     */
    [[nodiscard]] T* Get(Handle handle) {
        if (!handle.IsValid() || handle.blockIndex >= m_blocks.size()) {
            return nullptr;
        }
        return m_blocks[handle.blockIndex].Get(handle.localIndex);
    }

    [[nodiscard]] const T* Get(Handle handle) const {
        if (!handle.IsValid() || handle.blockIndex >= m_blocks.size()) {
            return nullptr;
        }
        return m_blocks[handle.blockIndex].Get(handle.localIndex);
    }

    /**
     * @brief Check if handle is valid
     */
    [[nodiscard]] bool IsValid(Handle handle) const {
        return handle.IsValid() &&
               handle.blockIndex < m_blocks.size() &&
               m_blocks[handle.blockIndex].IsActive(handle.localIndex);
    }

    /**
     * @brief Get active object count
     */
    [[nodiscard]] size_type GetActiveCount() const {
        std::lock_guard lock(m_mutex);
        size_type count = 0;
        for (const auto& block : m_blocks) {
            count += block.GetActiveCount();
        }
        return count;
    }

    /**
     * @brief Get total capacity
     */
    [[nodiscard]] size_t GetCapacity() const {
        return m_blocks.size() * BlockSize;
    }

    /**
     * @brief Iterate over all active objects (locks pool)
     */
    template<typename Func>
    void ForEach(Func&& func) {
        std::lock_guard lock(m_mutex);
        for (size_type blockIdx = 0; blockIdx < m_blocks.size(); ++blockIdx) {
            m_blocks[blockIdx].ForEach([&](T& obj, size_type localIdx) {
                Handle handle{blockIdx, localIdx};
                func(obj, handle);
            });
        }
    }

    /**
     * @brief Clear all objects
     */
    void Clear() {
        std::lock_guard lock(m_mutex);
        for (auto& block : m_blocks) {
            block.Clear();
        }
    }

private:
    template<typename... Args>
    Handle AllocateInternal(Args&&... args) {
        // Find a block with free space
        for (size_type blockIdx = 0; blockIdx < m_blocks.size(); ++blockIdx) {
            auto [ptr, localIdx] = m_blocks[blockIdx].Allocate(std::forward<Args>(args)...);
            if (ptr) {
                return {blockIdx, localIdx};
            }
        }

        // Need to grow
        if (!m_config.growOnDemand || GetCapacity() >= m_config.maxCapacity) {
            return INVALID;
        }

        size_type newBlockIdx = AllocateBlock();
        auto [ptr, localIdx] = m_blocks[newBlockIdx].Allocate(std::forward<Args>(args)...);
        return ptr ? Handle{newBlockIdx, localIdx} : INVALID;
    }

    void DeallocateInternal(Handle handle) {
        if (handle.IsValid() && handle.blockIndex < m_blocks.size()) {
            m_blocks[handle.blockIndex].Deallocate(handle.localIndex);
        }
    }

    size_type AllocateBlock() {
        m_blocks.emplace_back();
        return static_cast<size_type>(m_blocks.size() - 1);
    }

    PoolConfig m_config;
    std::vector<FixedPool<T, BlockSize>> m_blocks;
    mutable std::mutex m_mutex;
};

/**
 * @brief Lock-free concurrent pool using atomic free list
 *
 * Best for high-contention scenarios where mutex overhead is unacceptable.
 * Uses tagged pointers to prevent ABA problem.
 *
 * @tparam T Object type to pool
 * @tparam Capacity Maximum number of objects
 */
template<typename T, size_t Capacity>
class LockFreePool {
public:
    using value_type = T;
    using size_type = uint32_t;
    static constexpr size_type INVALID_INDEX = ~size_type{0};

    LockFreePool() {
        // Initialize free list
        for (size_type i = 0; i < Capacity - 1; ++i) {
            m_nodes[i].next.store(i + 1, std::memory_order_relaxed);
        }
        m_nodes[Capacity - 1].next.store(INVALID_INDEX, std::memory_order_relaxed);
        m_head.store({0, 0}, std::memory_order_relaxed);
    }

    ~LockFreePool() {
        for (size_type i = 0; i < Capacity; ++i) {
            if (m_active[i].load(std::memory_order_relaxed)) {
                GetObject(i)->~T();
            }
        }
    }

    // Non-copyable
    LockFreePool(const LockFreePool&) = delete;
    LockFreePool& operator=(const LockFreePool&) = delete;

    /**
     * @brief Allocate an object (lock-free)
     */
    template<typename... Args>
    [[nodiscard]] std::pair<T*, size_type> Allocate(Args&&... args) {
        TaggedIndex oldHead = m_head.load(std::memory_order_acquire);

        while (true) {
            if (oldHead.index == INVALID_INDEX) {
                return {nullptr, INVALID_INDEX};
            }

            size_type nextIndex = m_nodes[oldHead.index].next.load(std::memory_order_relaxed);
            TaggedIndex newHead{nextIndex, oldHead.tag + 1};

            if (m_head.compare_exchange_weak(oldHead, newHead,
                    std::memory_order_release, std::memory_order_acquire)) {
                m_active[oldHead.index].store(true, std::memory_order_release);
                m_activeCount.fetch_add(1, std::memory_order_relaxed);

                T* obj = new (GetStorage(oldHead.index)) T(std::forward<Args>(args)...);
                return {obj, oldHead.index};
            }
        }
    }

    /**
     * @brief Deallocate an object (lock-free)
     */
    void Deallocate(size_type index) {
        assert(index < Capacity);

        // Destruct
        GetObject(index)->~T();
        m_active[index].store(false, std::memory_order_release);
        m_activeCount.fetch_sub(1, std::memory_order_relaxed);

        // Push to free list
        TaggedIndex oldHead = m_head.load(std::memory_order_acquire);
        while (true) {
            m_nodes[index].next.store(oldHead.index, std::memory_order_relaxed);
            TaggedIndex newHead{index, oldHead.tag + 1};

            if (m_head.compare_exchange_weak(oldHead, newHead,
                    std::memory_order_release, std::memory_order_acquire)) {
                break;
            }
        }
    }

    [[nodiscard]] T* Get(size_type index) {
        assert(index < Capacity && m_active[index].load(std::memory_order_acquire));
        return GetObject(index);
    }

    [[nodiscard]] const T* Get(size_type index) const {
        assert(index < Capacity && m_active[index].load(std::memory_order_acquire));
        return GetObject(index);
    }

    [[nodiscard]] bool IsActive(size_type index) const {
        return index < Capacity && m_active[index].load(std::memory_order_acquire);
    }

    [[nodiscard]] size_type GetActiveCount() const {
        return m_activeCount.load(std::memory_order_relaxed);
    }

    [[nodiscard]] static constexpr size_type GetCapacity() { return Capacity; }

private:
    struct TaggedIndex {
        size_type index;
        size_type tag;  // Prevents ABA problem
    };

    struct Node {
        std::atomic<size_type> next;
    };

    [[nodiscard]] void* GetStorage(size_type index) {
        return &m_storage[index * sizeof(T)];
    }

    [[nodiscard]] T* GetObject(size_type index) {
        return std::launder(reinterpret_cast<T*>(&m_storage[index * sizeof(T)]));
    }

    [[nodiscard]] const T* GetObject(size_type index) const {
        return std::launder(reinterpret_cast<const T*>(&m_storage[index * sizeof(T)]));
    }

    alignas(T) std::array<std::byte, Capacity * sizeof(T)> m_storage;
    std::array<Node, Capacity> m_nodes;
    std::array<std::atomic<bool>, Capacity> m_active{};
    std::atomic<TaggedIndex> m_head;
    std::atomic<size_type> m_activeCount{0};
};

/**
 * @brief Scoped handle for automatic pool deallocation
 */
template<typename Pool>
class PooledHandle {
public:
    using Handle = typename Pool::Handle;

    PooledHandle() = default;
    PooledHandle(Pool& pool, Handle handle) : m_pool(&pool), m_handle(handle) {}

    ~PooledHandle() {
        Reset();
    }

    // Non-copyable, movable
    PooledHandle(const PooledHandle&) = delete;
    PooledHandle& operator=(const PooledHandle&) = delete;

    PooledHandle(PooledHandle&& other) noexcept
        : m_pool(other.m_pool), m_handle(other.m_handle) {
        other.m_pool = nullptr;
        other.m_handle = {};
    }

    PooledHandle& operator=(PooledHandle&& other) noexcept {
        if (this != &other) {
            Reset();
            m_pool = other.m_pool;
            m_handle = other.m_handle;
            other.m_pool = nullptr;
            other.m_handle = {};
        }
        return *this;
    }

    void Reset() {
        if (m_pool && m_handle.IsValid()) {
            m_pool->Deallocate(m_handle);
            m_pool = nullptr;
            m_handle = {};
        }
    }

    [[nodiscard]] auto* Get() { return m_pool ? m_pool->Get(m_handle) : nullptr; }
    [[nodiscard]] const auto* Get() const { return m_pool ? m_pool->Get(m_handle) : nullptr; }

    [[nodiscard]] Handle GetHandle() const { return m_handle; }
    [[nodiscard]] bool IsValid() const { return m_pool && m_handle.IsValid(); }

    auto* operator->() { return Get(); }
    const auto* operator->() const { return Get(); }
    auto& operator*() { return *Get(); }
    const auto& operator*() const { return *Get(); }

    explicit operator bool() const { return IsValid(); }

private:
    Pool* m_pool = nullptr;
    Handle m_handle{};
};

/**
 * @brief Frame allocator for temporary per-frame allocations
 *
 * Ultra-fast bump allocator that resets each frame.
 * Zero fragmentation, no deallocation overhead.
 */
class FrameAllocator {
public:
    explicit FrameAllocator(size_t capacity = 1024 * 1024)
        : m_capacity(capacity)
        , m_buffer(std::make_unique<std::byte[]>(capacity)) {
    }

    /**
     * @brief Allocate memory with alignment
     */
    template<typename T>
    [[nodiscard]] T* Allocate(size_t count = 1) {
        size_t size = sizeof(T) * count;
        size_t alignment = alignof(T);

        // Align offset
        size_t alignedOffset = (m_offset + alignment - 1) & ~(alignment - 1);

        if (alignedOffset + size > m_capacity) {
            return nullptr;  // Out of memory
        }

        T* ptr = reinterpret_cast<T*>(m_buffer.get() + alignedOffset);
        m_offset = alignedOffset + size;
        m_allocationCount++;

        return ptr;
    }

    /**
     * @brief Allocate and construct an object
     */
    template<typename T, typename... Args>
    [[nodiscard]] T* Create(Args&&... args) {
        T* ptr = Allocate<T>();
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }

    /**
     * @brief Reset for next frame (O(1) operation)
     */
    void Reset() {
        m_offset = 0;
        m_allocationCount = 0;
    }

    [[nodiscard]] size_t GetUsed() const { return m_offset; }
    [[nodiscard]] size_t GetCapacity() const { return m_capacity; }
    [[nodiscard]] size_t GetAllocationCount() const { return m_allocationCount; }

private:
    size_t m_capacity;
    std::unique_ptr<std::byte[]> m_buffer;
    size_t m_offset = 0;
    size_t m_allocationCount = 0;
};

} // namespace Nova
