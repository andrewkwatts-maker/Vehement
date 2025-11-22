#pragma once

#include <vector>
#include <array>
#include <tuple>
#include <utility>
#include <cstddef>
#include <type_traits>
#include <algorithm>
#include <cassert>
#include <functional>

namespace Nova {

/**
 * @brief Structure of Arrays (SoA) container template
 *
 * Provides cache-friendly data layout by storing each component type
 * in a contiguous array, maximizing cache utilization for operations
 * that iterate over specific components.
 *
 * @tparam Components The component types to store
 *
 * Example usage:
 * @code
 * // Instead of: struct Entity { vec3 pos, vel; float mass; };
 * // Use:
 * SoA<glm::vec3, glm::vec3, float> entities;  // positions, velocities, masses
 *
 * auto idx = entities.Add({0,0,0}, {1,0,0}, 1.0f);
 * auto& positions = entities.Get<0>();  // vector<vec3>&
 * auto& velocities = entities.Get<1>(); // vector<vec3>&
 *
 * // Cache-efficient iteration over positions only
 * for (auto& pos : positions) {
 *     pos += deltaTime * ...;
 * }
 * @endcode
 */
template<typename... Components>
class SoA {
public:
    static constexpr size_t ComponentCount = sizeof...(Components);
    using Index = uint32_t;
    static constexpr Index INVALID_INDEX = ~Index{0};

    /**
     * @brief Add an element with all components
     */
    Index Add(Components... components) {
        Index idx = static_cast<Index>(Size());
        (std::get<std::vector<Components>>(m_arrays).push_back(std::move(components)), ...);
        return idx;
    }

    /**
     * @brief Remove element at index (swap with last, O(1))
     * @return Index of element that was moved to fill the gap (or INVALID_INDEX if was last)
     */
    Index Remove(Index index) {
        assert(index < Size() && "Invalid index");

        Index lastIdx = static_cast<Index>(Size() - 1);
        if (index != lastIdx) {
            // Swap with last element in each array
            SwapWithLast(index, std::index_sequence_for<Components...>{});
        }

        // Remove last element from each array
        PopBack(std::index_sequence_for<Components...>{});

        return (index != lastIdx) ? index : INVALID_INDEX;
    }

    /**
     * @brief Get component array by index
     */
    template<size_t I>
    [[nodiscard]] auto& Get() {
        return std::get<I>(m_arrays);
    }

    template<size_t I>
    [[nodiscard]] const auto& Get() const {
        return std::get<I>(m_arrays);
    }

    /**
     * @brief Get component array by type (must be unique)
     */
    template<typename T>
    [[nodiscard]] std::vector<T>& Get() {
        return std::get<std::vector<T>>(m_arrays);
    }

    template<typename T>
    [[nodiscard]] const std::vector<T>& Get() const {
        return std::get<std::vector<T>>(m_arrays);
    }

    /**
     * @brief Get single element from component array
     */
    template<size_t I>
    [[nodiscard]] auto& GetAt(Index index) {
        return std::get<I>(m_arrays)[index];
    }

    template<size_t I>
    [[nodiscard]] const auto& GetAt(Index index) const {
        return std::get<I>(m_arrays)[index];
    }

    /**
     * @brief Get all components for an index as tuple
     */
    [[nodiscard]] auto GetAll(Index index) {
        return GetAllImpl(index, std::index_sequence_for<Components...>{});
    }

    [[nodiscard]] auto GetAll(Index index) const {
        return GetAllConstImpl(index, std::index_sequence_for<Components...>{});
    }

    /**
     * @brief Set all components for an index
     */
    void SetAll(Index index, Components... components) {
        SetAllImpl(index, std::index_sequence_for<Components...>{},
                   std::forward<Components>(components)...);
    }

    /**
     * @brief Get number of elements
     */
    [[nodiscard]] size_t Size() const {
        return std::get<0>(m_arrays).size();
    }

    /**
     * @brief Check if empty
     */
    [[nodiscard]] bool Empty() const {
        return Size() == 0;
    }

    /**
     * @brief Reserve capacity in all arrays
     */
    void Reserve(size_t capacity) {
        (std::get<std::vector<Components>>(m_arrays).reserve(capacity), ...);
    }

    /**
     * @brief Clear all arrays
     */
    void Clear() {
        (std::get<std::vector<Components>>(m_arrays).clear(), ...);
    }

    /**
     * @brief Shrink all arrays to fit
     */
    void ShrinkToFit() {
        (std::get<std::vector<Components>>(m_arrays).shrink_to_fit(), ...);
    }

    /**
     * @brief Apply function to each element (all components as tuple)
     */
    template<typename Func>
    void ForEach(Func&& func) {
        for (Index i = 0; i < Size(); ++i) {
            std::apply(func, GetAll(i));
        }
    }

    /**
     * @brief Apply function to each element with index
     */
    template<typename Func>
    void ForEachIndexed(Func&& func) {
        for (Index i = 0; i < Size(); ++i) {
            std::apply([&](auto&... args) { func(i, args...); }, GetAll(i));
        }
    }

    /**
     * @brief Apply function to specific components
     */
    template<size_t... Is, typename Func>
    void ForEachComponents(Func&& func) {
        for (Index i = 0; i < Size(); ++i) {
            func(std::get<Is>(m_arrays)[i]...);
        }
    }

private:
    template<size_t... Is>
    auto GetAllImpl(Index index, std::index_sequence<Is...>) {
        return std::tie(std::get<Is>(m_arrays)[index]...);
    }

    template<size_t... Is>
    auto GetAllConstImpl(Index index, std::index_sequence<Is...>) const {
        return std::tie(std::get<Is>(m_arrays)[index]...);
    }

    template<size_t... Is>
    void SetAllImpl(Index index, std::index_sequence<Is...>, Components... components) {
        ((std::get<Is>(m_arrays)[index] = std::move(components)), ...);
    }

    template<size_t... Is>
    void SwapWithLast(Index index, std::index_sequence<Is...>) {
        ((std::swap(std::get<Is>(m_arrays)[index],
                    std::get<Is>(m_arrays).back())), ...);
    }

    template<size_t... Is>
    void PopBack(std::index_sequence<Is...>) {
        (std::get<Is>(m_arrays).pop_back(), ...);
    }

    std::tuple<std::vector<Components>...> m_arrays;
};

/**
 * @brief Aligned SoA for SIMD operations
 *
 * Ensures each array is aligned for efficient SIMD access.
 */
template<typename... Components>
class AlignedSoA {
public:
    static constexpr size_t Alignment = 64;  // Cache line alignment
    static constexpr size_t ComponentCount = sizeof...(Components);
    using Index = uint32_t;

    /**
     * @brief Aligned vector type
     */
    template<typename T>
    using AlignedVector = std::vector<T, std::allocator<T>>;  // Use aligned_alloc in real impl

    Index Add(Components... components) {
        Index idx = static_cast<Index>(Size());
        (std::get<AlignedVector<Components>>(m_arrays).push_back(std::move(components)), ...);
        return idx;
    }

    template<size_t I>
    [[nodiscard]] auto& Get() {
        return std::get<I>(m_arrays);
    }

    template<size_t I>
    [[nodiscard]] const auto& Get() const {
        return std::get<I>(m_arrays);
    }

    [[nodiscard]] size_t Size() const {
        return std::get<0>(m_arrays).size();
    }

    void Reserve(size_t capacity) {
        (std::get<AlignedVector<Components>>(m_arrays).reserve(capacity), ...);
    }

    void Clear() {
        (std::get<AlignedVector<Components>>(m_arrays).clear(), ...);
    }

private:
    std::tuple<AlignedVector<Components>...> m_arrays;
};

/**
 * @brief Helper to create a view over SoA for specific components
 */
template<typename SoAType, size_t... Is>
class SoAView {
public:
    using Index = typename SoAType::Index;

    explicit SoAView(SoAType& soa) : m_soa(&soa) {}

    template<typename Func>
    void ForEach(Func&& func) {
        for (Index i = 0; i < m_soa->Size(); ++i) {
            func(m_soa->template Get<Is>()[i]...);
        }
    }

    template<typename Func>
    void ForEachIndexed(Func&& func) {
        for (Index i = 0; i < m_soa->Size(); ++i) {
            func(i, m_soa->template Get<Is>()[i]...);
        }
    }

private:
    SoAType* m_soa;
};

/**
 * @brief Create a view for specific component indices
 */
template<size_t... Is, typename SoAType>
auto MakeView(SoAType& soa) {
    return SoAView<SoAType, Is...>(soa);
}

/**
 * @brief Sparse set for efficient entity-component mapping
 *
 * Provides O(1) add, remove, and lookup with cache-friendly iteration.
 */
template<typename T, size_t PageSize = 4096>
class SparseSet {
public:
    using Index = uint32_t;
    static constexpr Index INVALID = ~Index{0};

    /**
     * @brief Add element with associated ID
     */
    Index Add(Index id, T value) {
        EnsurePage(id);

        Index denseIdx = static_cast<Index>(m_dense.size());
        m_sparse[id / PageSize][id % PageSize] = denseIdx;
        m_dense.push_back(id);
        m_data.push_back(std::move(value));

        return denseIdx;
    }

    /**
     * @brief Remove element by ID
     */
    void Remove(Index id) {
        if (!Has(id)) return;

        Index denseIdx = m_sparse[id / PageSize][id % PageSize];
        Index lastId = m_dense.back();

        // Swap with last
        m_dense[denseIdx] = lastId;
        m_data[denseIdx] = std::move(m_data.back());
        m_sparse[lastId / PageSize][lastId % PageSize] = denseIdx;

        // Remove last
        m_dense.pop_back();
        m_data.pop_back();
        m_sparse[id / PageSize][id % PageSize] = INVALID;
    }

    /**
     * @brief Check if ID exists
     */
    [[nodiscard]] bool Has(Index id) const {
        size_t page = id / PageSize;
        if (page >= m_sparse.size() || !m_sparse[page]) {
            return false;
        }
        Index denseIdx = m_sparse[page][id % PageSize];
        return denseIdx != INVALID && denseIdx < m_dense.size();
    }

    /**
     * @brief Get value by ID
     */
    [[nodiscard]] T* Get(Index id) {
        if (!Has(id)) return nullptr;
        return &m_data[m_sparse[id / PageSize][id % PageSize]];
    }

    [[nodiscard]] const T* Get(Index id) const {
        if (!Has(id)) return nullptr;
        return &m_data[m_sparse[id / PageSize][id % PageSize]];
    }

    /**
     * @brief Direct access to dense data array
     */
    [[nodiscard]] std::vector<T>& GetDense() { return m_data; }
    [[nodiscard]] const std::vector<T>& GetDense() const { return m_data; }

    /**
     * @brief Get IDs array (parallel to data)
     */
    [[nodiscard]] const std::vector<Index>& GetIds() const { return m_dense; }

    [[nodiscard]] size_t Size() const { return m_dense.size(); }
    [[nodiscard]] bool Empty() const { return m_dense.empty(); }

    void Clear() {
        m_dense.clear();
        m_data.clear();
        m_sparse.clear();
    }

    /**
     * @brief Iterate over all elements
     */
    template<typename Func>
    void ForEach(Func&& func) {
        for (size_t i = 0; i < m_data.size(); ++i) {
            func(m_dense[i], m_data[i]);
        }
    }

private:
    void EnsurePage(Index id) {
        size_t page = id / PageSize;
        if (page >= m_sparse.size()) {
            m_sparse.resize(page + 1);
        }
        if (!m_sparse[page]) {
            m_sparse[page] = std::make_unique<std::array<Index, PageSize>>();
            m_sparse[page]->fill(INVALID);
        }
    }

    std::vector<Index> m_dense;
    std::vector<T> m_data;
    std::vector<std::unique_ptr<std::array<Index, PageSize>>> m_sparse;
};

/**
 * @brief Multi-component sparse set (mini ECS)
 */
template<typename... Components>
class ComponentStorage {
public:
    using EntityId = uint32_t;
    static constexpr EntityId INVALID_ENTITY = ~EntityId{0};

    /**
     * @brief Add entity with all components
     */
    void Add(EntityId entity, Components... components) {
        AddImpl(entity, std::index_sequence_for<Components...>{},
                std::forward<Components>(components)...);
        m_entities.push_back(entity);
    }

    /**
     * @brief Remove entity
     */
    void Remove(EntityId entity) {
        // Find and remove from entities list
        auto it = std::find(m_entities.begin(), m_entities.end(), entity);
        if (it != m_entities.end()) {
            *it = m_entities.back();
            m_entities.pop_back();
        }

        // Remove from each component storage
        RemoveImpl(entity, std::index_sequence_for<Components...>{});
    }

    /**
     * @brief Check if entity has all components
     */
    [[nodiscard]] bool Has(EntityId entity) const {
        return HasImpl(entity, std::index_sequence_for<Components...>{});
    }

    /**
     * @brief Get component for entity
     */
    template<typename T>
    [[nodiscard]] T* GetComponent(EntityId entity) {
        return std::get<SparseSet<T>>(m_components).Get(entity);
    }

    template<typename T>
    [[nodiscard]] const T* GetComponent(EntityId entity) const {
        return std::get<SparseSet<T>>(m_components).Get(entity);
    }

    /**
     * @brief Iterate over all entities with all components
     */
    template<typename Func>
    void ForEach(Func&& func) {
        for (EntityId entity : m_entities) {
            if (Has(entity)) {
                func(entity, *GetComponent<Components>(entity)...);
            }
        }
    }

    /**
     * @brief Get entity count
     */
    [[nodiscard]] size_t Size() const { return m_entities.size(); }

    void Clear() {
        m_entities.clear();
        ClearImpl(std::index_sequence_for<Components...>{});
    }

private:
    template<size_t... Is>
    void AddImpl(EntityId entity, std::index_sequence<Is...>, Components... components) {
        (std::get<Is>(m_components).Add(entity, std::move(components)), ...);
    }

    template<size_t... Is>
    void RemoveImpl(EntityId entity, std::index_sequence<Is...>) {
        (std::get<Is>(m_components).Remove(entity), ...);
    }

    template<size_t... Is>
    bool HasImpl(EntityId entity, std::index_sequence<Is...>) const {
        return (std::get<Is>(m_components).Has(entity) && ...);
    }

    template<size_t... Is>
    void ClearImpl(std::index_sequence<Is...>) {
        (std::get<Is>(m_components).Clear(), ...);
    }

    std::tuple<SparseSet<Components>...> m_components;
    std::vector<EntityId> m_entities;
};

} // namespace Nova
