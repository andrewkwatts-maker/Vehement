#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>

namespace Vehement {
namespace RTS {

// ============================================================================
// Resource Types
// ============================================================================

/**
 * @brief Types of resources in the RTS economy
 *
 * Resources are categorized into:
 * - Basic: Essential for survival and construction
 * - Currency: For trading and upgrades
 * - Special: Consumed by specific systems
 */
enum class ResourceType : uint8_t {
    // Basic resources
    Food,           ///< Required for workers - consumed over time
    Wood,           ///< Construction material - from trees
    Stone,          ///< Construction material - from rock deposits
    Metal,          ///< Advanced construction - from scrap/mines

    // Currency
    Coins,          ///< Trading, upgrades - earned from combat/trading

    // Special resources
    Fuel,           ///< Powers generators and vehicles
    Medicine,       ///< Heals workers and cures infection
    Ammunition,     ///< For defense structures and weapons

    COUNT           ///< Number of resource types
};

/**
 * @brief Get the display name for a resource type
 */
[[nodiscard]] const char* GetResourceName(ResourceType type);

/**
 * @brief Get the icon path for a resource type
 */
[[nodiscard]] const char* GetResourceIcon(ResourceType type);

/**
 * @brief Get the color associated with a resource type (for UI)
 * @return RGBA color as packed uint32_t (0xRRGGBBAA)
 */
[[nodiscard]] uint32_t GetResourceColor(ResourceType type);

/**
 * @brief Check if a resource type is a basic resource
 */
[[nodiscard]] inline bool IsBasicResource(ResourceType type) {
    return type == ResourceType::Food ||
           type == ResourceType::Wood ||
           type == ResourceType::Stone ||
           type == ResourceType::Metal;
}

/**
 * @brief Check if a resource type is a special resource
 */
[[nodiscard]] inline bool IsSpecialResource(ResourceType type) {
    return type == ResourceType::Fuel ||
           type == ResourceType::Medicine ||
           type == ResourceType::Ammunition;
}

// ============================================================================
// Resource Cost
// ============================================================================

/**
 * @brief Represents a cost in multiple resource types
 * Used for building costs, recipe inputs, etc.
 */
struct ResourceCost {
    std::vector<std::pair<ResourceType, int>> costs;

    ResourceCost() = default;

    /**
     * @brief Create a cost with a single resource type
     */
    ResourceCost(ResourceType type, int amount) {
        costs.emplace_back(type, amount);
    }

    /**
     * @brief Add a resource requirement to the cost
     */
    ResourceCost& Add(ResourceType type, int amount) {
        costs.emplace_back(type, amount);
        return *this;
    }

    /**
     * @brief Get the amount required of a specific type
     */
    [[nodiscard]] int GetAmount(ResourceType type) const {
        for (const auto& [t, amt] : costs) {
            if (t == type) return amt;
        }
        return 0;
    }

    /**
     * @brief Check if cost is empty (free)
     */
    [[nodiscard]] bool IsEmpty() const { return costs.empty(); }

    /**
     * @brief Multiply all costs by a factor
     */
    ResourceCost operator*(float factor) const {
        ResourceCost result;
        for (const auto& [type, amount] : costs) {
            result.costs.emplace_back(type, static_cast<int>(amount * factor));
        }
        return result;
    }

    /**
     * @brief Get string representation for UI
     */
    [[nodiscard]] std::string ToString() const;
};

// ============================================================================
// Resource Stock
// ============================================================================

/**
 * @brief Manages a stockpile of resources with storage limits
 *
 * This is the main container for player/settlement resources.
 * Supports capacity limits, income/expense tracking, and callbacks.
 */
struct ResourceStock {
    /// Current resource amounts
    std::unordered_map<ResourceType, int> amounts;

    /// Maximum storage capacity per resource type
    std::unordered_map<ResourceType, int> capacity;

    /// Income rate per second (from production)
    std::unordered_map<ResourceType, float> incomeRate;

    /// Expense rate per second (from upkeep)
    std::unordered_map<ResourceType, float> expenseRate;

    /**
     * @brief Initialize with default values
     */
    ResourceStock();

    // -------------------------------------------------------------------------
    // Query Functions
    // -------------------------------------------------------------------------

    /**
     * @brief Get current amount of a resource
     */
    [[nodiscard]] int GetAmount(ResourceType type) const;

    /**
     * @brief Get maximum capacity for a resource
     */
    [[nodiscard]] int GetCapacity(ResourceType type) const;

    /**
     * @brief Get available space for a resource
     */
    [[nodiscard]] int GetFreeSpace(ResourceType type) const;

    /**
     * @brief Check if storage is full for a resource type
     */
    [[nodiscard]] bool IsFull(ResourceType type) const;

    /**
     * @brief Get fill percentage (0.0 - 1.0)
     */
    [[nodiscard]] float GetFillPercentage(ResourceType type) const;

    /**
     * @brief Get net income rate (income - expense)
     */
    [[nodiscard]] float GetNetRate(ResourceType type) const;

    // -------------------------------------------------------------------------
    // Affordability Checks
    // -------------------------------------------------------------------------

    /**
     * @brief Check if can afford a single resource amount
     */
    [[nodiscard]] bool CanAfford(ResourceType type, int amount) const;

    /**
     * @brief Check if can afford a complete cost
     */
    [[nodiscard]] bool CanAfford(const ResourceCost& cost) const;

    /**
     * @brief Get missing resources from a cost
     * @return ResourceCost containing only the missing amounts
     */
    [[nodiscard]] ResourceCost GetMissing(const ResourceCost& cost) const;

    // -------------------------------------------------------------------------
    // Modification Functions
    // -------------------------------------------------------------------------

    /**
     * @brief Add resources (respects capacity limits)
     * @param type Resource type to add
     * @param amount Amount to add
     * @return Actual amount added (may be less if capped)
     */
    int Add(ResourceType type, int amount);

    /**
     * @brief Remove resources
     * @param type Resource type to remove
     * @param amount Amount to remove
     * @return true if had enough resources
     */
    bool Remove(ResourceType type, int amount);

    /**
     * @brief Spend resources according to a cost
     * @param cost The cost to spend
     * @return true if transaction succeeded
     */
    bool Spend(const ResourceCost& cost);

    /**
     * @brief Set resource amount directly (ignores capacity)
     */
    void Set(ResourceType type, int amount);

    /**
     * @brief Set capacity for a resource type
     */
    void SetCapacity(ResourceType type, int cap);

    /**
     * @brief Increase capacity for a resource type
     */
    void AddCapacity(ResourceType type, int additionalCap);

    // -------------------------------------------------------------------------
    // Rate Management
    // -------------------------------------------------------------------------

    /**
     * @brief Set income rate for a resource
     */
    void SetIncomeRate(ResourceType type, float rate);

    /**
     * @brief Add to income rate (from a new source)
     */
    void AddIncomeRate(ResourceType type, float rate);

    /**
     * @brief Set expense rate for a resource
     */
    void SetExpenseRate(ResourceType type, float rate);

    /**
     * @brief Add to expense rate (from a new consumer)
     */
    void AddExpenseRate(ResourceType type, float rate);

    /**
     * @brief Apply rates over time (call each frame/tick)
     * @param deltaTime Time since last update
     */
    void ApplyRates(float deltaTime);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using ResourceCallback = std::function<void(ResourceType, int oldAmount, int newAmount)>;
    using LowResourceCallback = std::function<void(ResourceType, int amount, int threshold)>;

    /**
     * @brief Set callback for when resource amounts change
     */
    void SetOnResourceChanged(ResourceCallback callback) { m_onResourceChanged = std::move(callback); }

    /**
     * @brief Set callback for when resource drops below threshold
     */
    void SetOnLowResource(LowResourceCallback callback) { m_onLowResource = std::move(callback); }

    /**
     * @brief Set low resource threshold for a type
     */
    void SetLowThreshold(ResourceType type, int threshold);

    // -------------------------------------------------------------------------
    // Utility
    // -------------------------------------------------------------------------

    /**
     * @brief Reset all resources to zero
     */
    void Clear();

    /**
     * @brief Initialize with starting resources
     */
    void InitializeDefaults();

    /**
     * @brief Get total value in coins (for scoring)
     */
    [[nodiscard]] int GetTotalValue() const;

private:
    ResourceCallback m_onResourceChanged;
    LowResourceCallback m_onLowResource;
    std::unordered_map<ResourceType, int> m_lowThresholds;

    // Accumulator for fractional resource changes
    std::unordered_map<ResourceType, float> m_fractionalAccumulator;

    void NotifyChange(ResourceType type, int oldAmount, int newAmount);
    void CheckLowResource(ResourceType type);
};

// ============================================================================
// Resource Value Table
// ============================================================================

/**
 * @brief Value of each resource in coins (for trading calculations)
 */
struct ResourceValueTable {
    std::unordered_map<ResourceType, float> baseValues;

    ResourceValueTable();

    /**
     * @brief Get base value of a resource in coins
     */
    [[nodiscard]] float GetBaseValue(ResourceType type) const;

    /**
     * @brief Calculate total value of a cost
     */
    [[nodiscard]] int CalculateValue(const ResourceCost& cost) const;
};

/**
 * @brief Global resource value table
 */
[[nodiscard]] const ResourceValueTable& GetResourceValues();

// ============================================================================
// Resource Scarcity Settings
// ============================================================================

/**
 * @brief Configuration for resource scarcity (difficulty settings)
 */
struct ScarcitySettings {
    float gatherRateMultiplier = 1.0f;      ///< How fast resources are gathered
    float consumptionMultiplier = 1.0f;     ///< How fast resources are consumed
    float respawnTimeMultiplier = 1.0f;     ///< How long for nodes to respawn
    float startingResourceMultiplier = 1.0f; ///< Starting resource amounts

    /**
     * @brief Preset for easy difficulty
     */
    static ScarcitySettings Easy() {
        return {1.5f, 0.75f, 0.5f, 1.5f};
    }

    /**
     * @brief Preset for normal difficulty
     */
    static ScarcitySettings Normal() {
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }

    /**
     * @brief Preset for hard difficulty
     */
    static ScarcitySettings Hard() {
        return {0.75f, 1.25f, 1.5f, 0.75f};
    }

    /**
     * @brief Preset for survival mode (very scarce)
     */
    static ScarcitySettings Survival() {
        return {0.5f, 1.5f, 2.0f, 0.5f};
    }
};

} // namespace RTS
} // namespace Vehement
