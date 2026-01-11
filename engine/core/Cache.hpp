#pragma once

#include <vector>
#include <array>
#include <unordered_map>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <cassert>

namespace Nova {

/**
 * @brief Cache line size (typical x86/ARM)
 */
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @brief Helper to pad a type to cache line boundary
 */
template<typename T>
struct alignas(CACHE_LINE_SIZE) CacheAligned {
    T value;

    CacheAligned() = default;
    CacheAligned(const T& v) : value(v) {}
    CacheAligned(T&& v) : value(std::move(v)) {}

    operator T&() { return value; }
    operator const T&() const { return value; }
    T& operator*() { return value; }
    const T& operator*() const { return value; }
    T* operator->() { return &value; }
    const T* operator->() const { return &value; }
};

/**
 * @brief Flat hash map optimized for cache efficiency
 *
 * Uses open addressing with linear probing for cache-friendly access.
 * Best for small to medium sized maps with simple keys.
 *
 * @tparam Key Key type
 * @tparam Value Value type
 * @tparam Hash Hash function
 */
template<typename Key, typename Value,
         typename Hash = std::hash<Key>,
         typename KeyEqual = std::equal_to<Key>>
class FlatHashMap {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<Key, Value>;
    using size_type = size_t;

    static constexpr float MAX_LOAD_FACTOR = 0.75f;
    static constexpr size_t MIN_CAPACITY = 16;

    FlatHashMap() {
        Resize(MIN_CAPACITY);
    }

    explicit FlatHashMap(size_t initialCapacity) {
        Resize(std::max(MIN_CAPACITY, RoundUpPow2(initialCapacity)));
    }

    /**
     * @brief Insert or update a key-value pair
     */
    Value& Insert(const Key& key, Value value) {
        if (ShouldGrow()) {
            Resize(m_capacity * 2);
        }

        size_t idx = FindOrInsertSlot(key);

        if (m_states[idx] != State::Occupied) {
            m_keys[idx] = key;
            m_states[idx] = State::Occupied;
            ++m_size;
        }

        m_values[idx] = std::move(value);
        return m_values[idx];
    }

    /**
     * @brief Get value by key
     */
    [[nodiscard]] Value* Find(const Key& key) {
        size_t idx = FindSlot(key);
        return (idx != INVALID_SLOT) ? &m_values[idx] : nullptr;
    }

    [[nodiscard]] const Value* Find(const Key& key) const {
        size_t idx = FindSlot(key);
        return (idx != INVALID_SLOT) ? &m_values[idx] : nullptr;
    }

    /**
     * @brief Get value by key with default
     */
    [[nodiscard]] Value Get(const Key& key, const Value& defaultValue = Value{}) const {
        const Value* ptr = Find(key);
        return ptr ? *ptr : defaultValue;
    }

    /**
     * @brief Access operator (inserts default if not found)
     */
    Value& operator[](const Key& key) {
        Value* ptr = Find(key);
        if (ptr) return *ptr;
        return Insert(key, Value{});
    }

    /**
     * @brief Check if key exists
     */
    [[nodiscard]] bool Contains(const Key& key) const {
        return FindSlot(key) != INVALID_SLOT;
    }

    /**
     * @brief Remove a key
     */
    bool Remove(const Key& key) {
        size_t idx = FindSlot(key);
        if (idx == INVALID_SLOT) return false;

        m_states[idx] = State::Tombstone;
        --m_size;
        return true;
    }

    /**
     * @brief Clear all entries
     */
    void Clear() {
        std::fill(m_states.begin(), m_states.end(), State::Empty);
        m_size = 0;
    }

    [[nodiscard]] size_t Size() const { return m_size; }
    [[nodiscard]] size_t Capacity() const { return m_capacity; }
    [[nodiscard]] bool Empty() const { return m_size == 0; }

    /**
     * @brief Iterate over all entries
     */
    template<typename Func>
    void ForEach(Func&& func) {
        for (size_t i = 0; i < m_capacity; ++i) {
            if (m_states[i] == State::Occupied) {
                func(m_keys[i], m_values[i]);
            }
        }
    }

    template<typename Func>
    void ForEach(Func&& func) const {
        for (size_t i = 0; i < m_capacity; ++i) {
            if (m_states[i] == State::Occupied) {
                func(m_keys[i], m_values[i]);
            }
        }
    }

private:
    enum class State : uint8_t { Empty, Occupied, Tombstone };
    static constexpr size_t INVALID_SLOT = ~size_t{0};

    [[nodiscard]] bool ShouldGrow() const {
        return m_size >= static_cast<size_t>(m_capacity * MAX_LOAD_FACTOR);
    }

    void Resize(size_t newCapacity) {
        auto oldKeys = std::move(m_keys);
        auto oldValues = std::move(m_values);
        auto oldStates = std::move(m_states);
        size_t oldCapacity = m_capacity;

        m_capacity = newCapacity;
        m_mask = newCapacity - 1;
        m_keys.resize(newCapacity);
        m_values.resize(newCapacity);
        m_states.resize(newCapacity, State::Empty);
        m_size = 0;

        for (size_t i = 0; i < oldCapacity; ++i) {
            if (oldStates[i] == State::Occupied) {
                Insert(oldKeys[i], std::move(oldValues[i]));
            }
        }
    }

    [[nodiscard]] size_t FindSlot(const Key& key) const {
        size_t idx = m_hasher(key) & m_mask;

        for (size_t probe = 0; probe < m_capacity; ++probe) {
            if (m_states[idx] == State::Empty) {
                return INVALID_SLOT;
            }
            if (m_states[idx] == State::Occupied && m_keyEqual(m_keys[idx], key)) {
                return idx;
            }
            idx = (idx + 1) & m_mask;
        }

        return INVALID_SLOT;
    }

    [[nodiscard]] size_t FindOrInsertSlot(const Key& key) {
        size_t idx = m_hasher(key) & m_mask;
        size_t firstTombstone = INVALID_SLOT;

        for (size_t probe = 0; probe < m_capacity; ++probe) {
            if (m_states[idx] == State::Empty) {
                return (firstTombstone != INVALID_SLOT) ? firstTombstone : idx;
            }
            if (m_states[idx] == State::Tombstone && firstTombstone == INVALID_SLOT) {
                firstTombstone = idx;
            }
            if (m_states[idx] == State::Occupied && m_keyEqual(m_keys[idx], key)) {
                return idx;
            }
            idx = (idx + 1) & m_mask;
        }

        return (firstTombstone != INVALID_SLOT) ? firstTombstone : idx;
    }

    [[nodiscard]] static size_t RoundUpPow2(size_t n) {
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }

    std::vector<Key> m_keys;
    std::vector<Value> m_values;
    std::vector<State> m_states;
    size_t m_capacity = 0;
    size_t m_mask = 0;
    size_t m_size = 0;
    Hash m_hasher;
    KeyEqual m_keyEqual;
};

/**
 * @brief Compile-time FNV-1a hash for strings
 */
constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
constexpr uint64_t FNV_PRIME = 1099511628211ull;

constexpr uint64_t HashString(std::string_view str) {
    uint64_t hash = FNV_OFFSET_BASIS;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= FNV_PRIME;
    }
    return hash;
}

/**
 * @brief String literal hash operator for compile-time hashing
 */
constexpr uint64_t operator""_hash(const char* str, size_t len) {
    return HashString(std::string_view(str, len));
}

/**
 * @brief Pre-hashed string for O(1) map lookups
 */
struct HashedString {
    std::string str;
    uint64_t hash;

    HashedString() : hash(0) {}
    HashedString(const std::string& s) : str(s), hash(HashString(s)) {}
    HashedString(std::string_view s) : str(s), hash(HashString(s)) {}
    HashedString(const char* s) : str(s), hash(HashString(s)) {}

    bool operator==(const HashedString& other) const {
        return hash == other.hash && str == other.str;
    }

    bool operator!=(const HashedString& other) const {
        return !(*this == other);
    }
};

struct HashedStringHash {
    size_t operator()(const HashedString& hs) const {
        return hs.hash;
    }
};

/**
 * @brief String-keyed map with pre-computed hashes
 */
template<typename Value>
using StringHashMap = FlatHashMap<HashedString, Value, HashedStringHash>;

/**
 * @brief LRU Cache with O(1) operations
 *
 * Useful for caching expensive computations (config lookups, resource loading).
 */
template<typename Key, typename Value,
         typename Hash = std::hash<Key>>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : m_capacity(capacity) {}

    /**
     * @brief Get value from cache
     * @return Pointer to value or nullptr if not cached
     */
    [[nodiscard]] Value* Get(const Key& key) {
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return nullptr;
        }

        // Move to front (most recently used)
        MoveToFront(it->second);
        return &it->second->value;
    }

    /**
     * @brief Get or compute value
     * @param key The key to look up
     * @param compute Function to compute value if not cached
     * @return Reference to cached value
     */
    template<typename Func>
    Value& GetOrCompute(const Key& key, Func&& compute) {
        Value* cached = Get(key);
        if (cached) return *cached;

        return Put(key, compute());
    }

    /**
     * @brief Insert value into cache
     */
    Value& Put(const Key& key, Value value) {
        auto it = m_map.find(key);

        if (it != m_map.end()) {
            // Update existing
            it->second->value = std::move(value);
            MoveToFront(it->second);
            return it->second->value;
        }

        // Evict if at capacity
        while (m_list.size() >= m_capacity) {
            EvictOldest();
        }

        // Insert new
        m_list.push_front({key, std::move(value)});
        m_map[key] = m_list.begin();
        return m_list.front().value;
    }

    /**
     * @brief Check if key exists
     */
    [[nodiscard]] bool Contains(const Key& key) const {
        return m_map.find(key) != m_map.end();
    }

    /**
     * @brief Remove a key from cache
     */
    void Remove(const Key& key) {
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            m_list.erase(it->second);
            m_map.erase(it);
        }
    }

    /**
     * @brief Clear the cache
     */
    void Clear() {
        m_list.clear();
        m_map.clear();
    }

    [[nodiscard]] size_t Size() const { return m_map.size(); }
    [[nodiscard]] size_t Capacity() const { return m_capacity; }

private:
    struct Node {
        Key key;
        Value value;
    };

    using List = std::list<Node>;
    using ListIterator = typename List::iterator;

    void MoveToFront(ListIterator it) {
        if (it != m_list.begin()) {
            m_list.splice(m_list.begin(), m_list, it);
        }
    }

    void EvictOldest() {
        if (!m_list.empty()) {
            m_map.erase(m_list.back().key);
            m_list.pop_back();
        }
    }

    List m_list;
    std::unordered_map<Key, ListIterator, Hash> m_map;
    size_t m_capacity;
};

/**
 * @brief Slot map - stable indices with dense iteration
 *
 * Provides stable handles that survive element removal.
 */
template<typename T>
class SlotMap {
public:
    using Index = uint32_t;
    using Generation = uint32_t;

    struct Handle {
        Index index = INVALID_INDEX;
        Generation generation = 0;

        [[nodiscard]] bool IsValid() const { return index != INVALID_INDEX; }
        bool operator==(const Handle& other) const = default;
    };

    static constexpr Index INVALID_INDEX = ~Index{0};
    static constexpr Handle INVALID_HANDLE{INVALID_INDEX, 0};

    SlotMap() = default;
    explicit SlotMap(size_t reserveCapacity) {
        m_data.reserve(reserveCapacity);
        m_slots.reserve(reserveCapacity);
        m_erase.reserve(reserveCapacity);
    }

    /**
     * @brief Insert a value and get a stable handle
     */
    template<typename... Args>
    Handle Insert(Args&&... args) {
        Index dataIdx = static_cast<Index>(m_data.size());
        m_data.emplace_back(std::forward<Args>(args)...);

        Index slotIdx;
        if (m_freeHead != INVALID_INDEX) {
            slotIdx = m_freeHead;
            m_freeHead = m_slots[slotIdx].index;  // Next free
            m_slots[slotIdx].index = dataIdx;
        } else {
            slotIdx = static_cast<Index>(m_slots.size());
            m_slots.push_back({dataIdx, 0});
        }

        m_erase.push_back(slotIdx);

        return {slotIdx, m_slots[slotIdx].generation};
    }

    /**
     * @brief Remove element by handle
     */
    void Remove(Handle handle) {
        if (!IsValid(handle)) return;

        Slot& slot = m_slots[handle.index];
        Index dataIdx = slot.index;

        // Swap with last in data array
        if (dataIdx != m_data.size() - 1) {
            m_data[dataIdx] = std::move(m_data.back());
            Index movedSlotIdx = m_erase.back();
            m_slots[movedSlotIdx].index = dataIdx;
            m_erase[dataIdx] = movedSlotIdx;
        }

        m_data.pop_back();
        m_erase.pop_back();

        // Mark slot as free, increment generation
        slot.generation++;
        slot.index = m_freeHead;
        m_freeHead = handle.index;
    }

    /**
     * @brief Check if handle is valid
     */
    [[nodiscard]] bool IsValid(Handle handle) const {
        return handle.index < m_slots.size() &&
               m_slots[handle.index].generation == handle.generation;
    }

    /**
     * @brief Get element by handle
     */
    [[nodiscard]] T* Get(Handle handle) {
        if (!IsValid(handle)) return nullptr;
        return &m_data[m_slots[handle.index].index];
    }

    [[nodiscard]] const T* Get(Handle handle) const {
        if (!IsValid(handle)) return nullptr;
        return &m_data[m_slots[handle.index].index];
    }

    /**
     * @brief Direct access to dense data (for iteration)
     */
    [[nodiscard]] std::vector<T>& GetDense() { return m_data; }
    [[nodiscard]] const std::vector<T>& GetDense() const { return m_data; }

    /**
     * @brief Iterate over all elements
     */
    template<typename Func>
    void ForEach(Func&& func) {
        for (T& item : m_data) {
            func(item);
        }
    }

    [[nodiscard]] size_t Size() const { return m_data.size(); }
    [[nodiscard]] bool Empty() const { return m_data.empty(); }

    void Clear() {
        m_data.clear();
        m_slots.clear();
        m_erase.clear();
        m_freeHead = INVALID_INDEX;
    }

private:
    struct Slot {
        Index index;
        Generation generation;
    };

    std::vector<T> m_data;       // Dense array of values
    std::vector<Slot> m_slots;   // Handle -> data index mapping
    std::vector<Index> m_erase;  // Data index -> slot index (for removal)
    Index m_freeHead = INVALID_INDEX;
};

/**
 * @brief Ring buffer for streaming data
 */
template<typename T, size_t Capacity>
class RingBuffer {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    [[nodiscard]] bool Push(const T& value) {
        if (Full()) return false;
        m_data[m_tail & MASK] = value;
        ++m_tail;
        return true;
    }

    [[nodiscard]] bool Push(T&& value) {
        if (Full()) return false;
        m_data[m_tail & MASK] = std::move(value);
        ++m_tail;
        return true;
    }

    [[nodiscard]] bool Pop(T& value) {
        if (Empty()) return false;
        value = std::move(m_data[m_head & MASK]);
        ++m_head;
        return true;
    }

    [[nodiscard]] T* Front() {
        return Empty() ? nullptr : &m_data[m_head & MASK];
    }

    [[nodiscard]] size_t Size() const { return m_tail - m_head; }
    [[nodiscard]] bool Empty() const { return m_head == m_tail; }
    [[nodiscard]] bool Full() const { return Size() == Capacity; }
    [[nodiscard]] static constexpr size_t GetCapacity() { return Capacity; }

    void Clear() { m_head = m_tail = 0; }

private:
    static constexpr size_t MASK = Capacity - 1;

    std::array<T, Capacity> m_data;
    size_t m_head = 0;
    size_t m_tail = 0;
};

/**
 * @brief Batch processor for grouping similar operations
 */
template<typename T, size_t BatchSize = 64>
class BatchProcessor {
public:
    using ProcessFunc = std::function<void(T*, size_t)>;

    explicit BatchProcessor(ProcessFunc processor)
        : m_processor(std::move(processor)) {}

    void Add(const T& item) {
        m_batch[m_count++] = item;
        if (m_count >= BatchSize) {
            Flush();
        }
    }

    void Add(T&& item) {
        m_batch[m_count++] = std::move(item);
        if (m_count >= BatchSize) {
            Flush();
        }
    }

    void Flush() {
        if (m_count > 0) {
            m_processor(m_batch.data(), m_count);
            m_count = 0;
        }
    }

    ~BatchProcessor() {
        Flush();
    }

private:
    std::array<T, BatchSize> m_batch;
    size_t m_count = 0;
    ProcessFunc m_processor;
};

} // namespace Nova
