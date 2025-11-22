#pragma once

/**
 * @file PointAllocation.hpp
 * @brief Point-based balance system for race creation
 *
 * Implements a comprehensive point allocation system that ensures
 * balanced race creation with configurable weights for different
 * gameplay aspects like military, economy, magic, and technology.
 */

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// Point Categories
// ============================================================================

/**
 * @brief Main allocation categories
 */
enum class PointCategory : uint8_t {
    Military = 0,       ///< Unit strength and combat effectiveness
    Economy,            ///< Resource gathering and production
    Magic,              ///< Spell power and magical abilities
    Technology,         ///< Research speed and tech advancement

    COUNT
};

/**
 * @brief Convert category to string
 */
[[nodiscard]] inline const char* PointCategoryToString(PointCategory cat) {
    switch (cat) {
        case PointCategory::Military:   return "Military";
        case PointCategory::Economy:    return "Economy";
        case PointCategory::Magic:      return "Magic";
        case PointCategory::Technology: return "Technology";
        default:                        return "Unknown";
    }
}

/**
 * @brief Parse category from string
 */
[[nodiscard]] inline PointCategory StringToPointCategory(const std::string& str) {
    if (str == "Military" || str == "military") return PointCategory::Military;
    if (str == "Economy" || str == "economy") return PointCategory::Economy;
    if (str == "Magic" || str == "magic") return PointCategory::Magic;
    if (str == "Technology" || str == "technology") return PointCategory::Technology;
    return PointCategory::Military;
}

// ============================================================================
// Military Sub-Categories
// ============================================================================

/**
 * @brief Military point distribution sub-categories
 */
struct MilitaryAllocation {
    int infantry = 25;          ///< Melee infantry strength
    int ranged = 25;            ///< Ranged unit effectiveness
    int cavalry = 25;           ///< Mounted unit power
    int siege = 25;             ///< Siege weapon damage

    // Computed bonuses
    float infantryDamageBonus = 0.0f;
    float infantryArmorBonus = 0.0f;
    float rangedDamageBonus = 0.0f;
    float rangedRangeBonus = 0.0f;
    float cavalrySpeedBonus = 0.0f;
    float cavalryChargeBonus = 0.0f;
    float siegeDamageBonus = 0.0f;
    float siegeRangeBonus = 0.0f;

    [[nodiscard]] int GetTotal() const { return infantry + ranged + cavalry + siege; }
    [[nodiscard]] bool IsValid() const { return GetTotal() == 100; }

    void ComputeBonuses();

    [[nodiscard]] nlohmann::json ToJson() const;
    static MilitaryAllocation FromJson(const nlohmann::json& j);
};

// ============================================================================
// Economy Sub-Categories
// ============================================================================

/**
 * @brief Economy point distribution sub-categories
 */
struct EconomyAllocation {
    int harvestSpeed = 30;      ///< Resource gathering rate
    int buildSpeed = 25;        ///< Construction speed
    int carryCapacity = 25;     ///< Worker carry capacity
    int tradeProfits = 20;      ///< Trade bonus percentage

    // Computed bonuses
    float harvestSpeedBonus = 0.0f;
    float buildSpeedBonus = 0.0f;
    float carryCapacityBonus = 0.0f;
    float tradeProfitBonus = 0.0f;
    float workerCostReduction = 0.0f;
    float storageBonus = 0.0f;

    [[nodiscard]] int GetTotal() const { return harvestSpeed + buildSpeed + carryCapacity + tradeProfits; }
    [[nodiscard]] bool IsValid() const { return GetTotal() == 100; }

    void ComputeBonuses();

    [[nodiscard]] nlohmann::json ToJson() const;
    static EconomyAllocation FromJson(const nlohmann::json& j);
};

// ============================================================================
// Magic Sub-Categories
// ============================================================================

/**
 * @brief Magic point distribution sub-categories
 */
struct MagicAllocation {
    int spellDamage = 30;       ///< Spell damage output
    int spellRange = 25;        ///< Casting range
    int manaCost = 25;          ///< Mana efficiency
    int cooldownReduction = 20; ///< Spell cooldown reduction

    // Computed bonuses
    float spellDamageBonus = 0.0f;
    float spellRangeBonus = 0.0f;
    float manaCostReduction = 0.0f;
    float cooldownReductionBonus = 0.0f;
    float manaRegenBonus = 0.0f;
    float maxManaBonus = 0.0f;

    [[nodiscard]] int GetTotal() const { return spellDamage + spellRange + manaCost + cooldownReduction; }
    [[nodiscard]] bool IsValid() const { return GetTotal() == 100; }

    void ComputeBonuses();

    [[nodiscard]] nlohmann::json ToJson() const;
    static MagicAllocation FromJson(const nlohmann::json& j);
};

// ============================================================================
// Technology Sub-Categories
// ============================================================================

/**
 * @brief Technology point distribution sub-categories
 */
struct TechnologyAllocation {
    int researchSpeed = 35;     ///< Research completion speed
    int ageUpCost = 35;         ///< Age advancement cost reduction
    int uniqueTechs = 30;       ///< Unique technology power

    // Computed bonuses
    float researchSpeedBonus = 0.0f;
    float ageUpCostReduction = 0.0f;
    float uniqueTechBonus = 0.0f;
    float techProtectionBonus = 0.0f;
    int bonusStartingTechs = 0;

    [[nodiscard]] int GetTotal() const { return researchSpeed + ageUpCost + uniqueTechs; }
    [[nodiscard]] bool IsValid() const { return GetTotal() == 100; }

    void ComputeBonuses();

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechnologyAllocation FromJson(const nlohmann::json& j);
};

// ============================================================================
// Balance Metrics
// ============================================================================

/**
 * @brief Balance warning types
 */
enum class BalanceWarningType : uint8_t {
    None = 0,
    MinorImbalance,     ///< Slight imbalance, may be intentional
    MajorImbalance,     ///< Significant imbalance
    Critical,           ///< Severe imbalance that may break gameplay

    COUNT
};

/**
 * @brief Single balance warning
 */
struct BalanceWarning {
    BalanceWarningType severity = BalanceWarningType::None;
    std::string category;       ///< Category causing the warning
    std::string message;        ///< Human-readable warning
    float deviation = 0.0f;     ///< How far from balanced

    [[nodiscard]] nlohmann::json ToJson() const;
    static BalanceWarning FromJson(const nlohmann::json& j);
};

/**
 * @brief Overall balance score and metrics
 */
struct BalanceScore {
    float overallScore = 100.0f;    ///< 0-100 balance score
    float militaryBalance = 1.0f;   ///< Military strength relative to average
    float economyBalance = 1.0f;    ///< Economy strength relative to average
    float magicBalance = 1.0f;      ///< Magic power relative to average
    float techBalance = 1.0f;       ///< Tech speed relative to average

    std::vector<BalanceWarning> warnings;

    [[nodiscard]] bool IsBalanced() const { return overallScore >= 80.0f; }
    [[nodiscard]] bool HasCriticalWarnings() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static BalanceScore FromJson(const nlohmann::json& j);
};

// ============================================================================
// Main Point Allocation Structure
// ============================================================================

/**
 * @brief Complete point allocation for a race
 *
 * Manages the distribution of points across all categories and sub-categories.
 * Ensures balance through validation and scoring mechanisms.
 *
 * Example usage:
 * @code
 * PointAllocation allocation;
 * allocation.SetTotalPoints(100);
 *
 * // Set main category allocation
 * allocation.SetCategoryPoints(PointCategory::Military, 30);
 * allocation.SetCategoryPoints(PointCategory::Economy, 25);
 * allocation.SetCategoryPoints(PointCategory::Magic, 20);
 * allocation.SetCategoryPoints(PointCategory::Technology, 25);
 *
 * // Validate and compute bonuses
 * if (allocation.Validate()) {
 *     allocation.ComputeAllBonuses();
 *
 *     // Check balance
 *     auto score = allocation.CalculateBalanceScore();
 *     if (!score.IsBalanced()) {
 *         for (const auto& warning : score.warnings) {
 *             std::cout << warning.message << std::endl;
 *         }
 *     }
 * }
 * @endcode
 */
struct PointAllocation {
    // Total budget
    int totalPoints = 100;          ///< Total points to allocate
    int remainingPoints = 0;        ///< Points not yet allocated

    // Main category allocation (should sum to 100%)
    int military = 25;              ///< Military strength points
    int economy = 25;               ///< Economy power points
    int magic = 25;                 ///< Magic power points
    int technology = 25;            ///< Technology advancement points

    // Sub-allocations (each category has 100 internal points)
    MilitaryAllocation militaryAlloc;
    EconomyAllocation economyAlloc;
    MagicAllocation magicAlloc;
    TechnologyAllocation techAlloc;

    // =========================================================================
    // Point Management
    // =========================================================================

    /**
     * @brief Set total points available
     */
    void SetTotalPoints(int points);

    /**
     * @brief Get points allocated to a category
     */
    [[nodiscard]] int GetCategoryPoints(PointCategory category) const;

    /**
     * @brief Set points for a category
     * @return true if allocation is valid
     */
    bool SetCategoryPoints(PointCategory category, int points);

    /**
     * @brief Get percentage allocation for a category
     */
    [[nodiscard]] float GetCategoryPercentage(PointCategory category) const;

    /**
     * @brief Get total allocated points
     */
    [[nodiscard]] int GetAllocatedPoints() const {
        return military + economy + magic + technology;
    }

    /**
     * @brief Check if all points are allocated
     */
    [[nodiscard]] bool IsFullyAllocated() const {
        return GetAllocatedPoints() == totalPoints;
    }

    /**
     * @brief Reset to default allocation
     */
    void ResetToDefault();

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate the allocation
     * @return true if valid
     */
    [[nodiscard]] bool Validate() const;

    /**
     * @brief Get validation error message if invalid
     */
    [[nodiscard]] std::string GetValidationError() const;

    /**
     * @brief Check if a specific category allocation is valid
     */
    [[nodiscard]] bool ValidateCategory(PointCategory category) const;

    // =========================================================================
    // Bonus Computation
    // =========================================================================

    /**
     * @brief Compute all bonuses from current allocation
     */
    void ComputeAllBonuses();

    /**
     * @brief Get a specific bonus value
     * @param bonusName Name of the bonus (e.g., "infantryDamageBonus")
     * @return Bonus value or 0.0f if not found
     */
    [[nodiscard]] float GetBonus(const std::string& bonusName) const;

    /**
     * @brief Get all bonuses as a map
     */
    [[nodiscard]] std::map<std::string, float> GetAllBonuses() const;

    // =========================================================================
    // Balance Scoring
    // =========================================================================

    /**
     * @brief Calculate balance score
     */
    [[nodiscard]] BalanceScore CalculateBalanceScore() const;

    /**
     * @brief Get recommended adjustments to improve balance
     */
    [[nodiscard]] std::map<PointCategory, int> GetRecommendedAdjustments() const;

    /**
     * @brief Auto-balance the allocation
     * @param preserveCategory Category to preserve during balancing
     */
    void AutoBalance(PointCategory preserveCategory = PointCategory::Military);

    // =========================================================================
    // Presets
    // =========================================================================

    /**
     * @brief Apply a preset allocation
     */
    void ApplyPreset(const std::string& presetName);

    /**
     * @brief Get list of available presets
     */
    [[nodiscard]] static std::vector<std::string> GetAvailablePresets();

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] nlohmann::json ToJson() const;
    static PointAllocation FromJson(const nlohmann::json& j);

    /**
     * @brief Save to file
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * @brief Load from file
     */
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Preset Allocations
// ============================================================================

/**
 * @brief Balanced preset - equal distribution
 */
[[nodiscard]] PointAllocation CreateBalancedPreset();

/**
 * @brief Military focus preset
 */
[[nodiscard]] PointAllocation CreateMilitaryPreset();

/**
 * @brief Economy focus preset
 */
[[nodiscard]] PointAllocation CreateEconomyPreset();

/**
 * @brief Magic focus preset
 */
[[nodiscard]] PointAllocation CreateMagicPreset();

/**
 * @brief Technology focus preset
 */
[[nodiscard]] PointAllocation CreateTechPreset();

/**
 * @brief Rush strategy preset
 */
[[nodiscard]] PointAllocation CreateRushPreset();

/**
 * @brief Turtle defense preset
 */
[[nodiscard]] PointAllocation CreateTurtlePreset();

// ============================================================================
// Balance Calculator
// ============================================================================

/**
 * @brief Calculator for balance metrics
 */
class BalanceCalculator {
public:
    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static BalanceCalculator& Instance();

    /**
     * @brief Calculate power level from allocation
     * @return Power level (100 = average)
     */
    [[nodiscard]] float CalculatePowerLevel(const PointAllocation& allocation) const;

    /**
     * @brief Compare two allocations
     * @return Positive if a is stronger, negative if b is stronger
     */
    [[nodiscard]] float CompareAllocations(const PointAllocation& a,
                                            const PointAllocation& b) const;

    /**
     * @brief Get win probability vs balanced allocation
     */
    [[nodiscard]] float GetWinProbability(const PointAllocation& allocation) const;

    /**
     * @brief Set balance weights for scoring
     */
    void SetBalanceWeights(float militaryWeight, float economyWeight,
                          float magicWeight, float techWeight);

    /**
     * @brief Get effective stat bonus for a stat type
     */
    [[nodiscard]] float GetEffectiveBonus(const PointAllocation& allocation,
                                          const std::string& statType) const;

private:
    BalanceCalculator() = default;

    float m_militaryWeight = 1.0f;
    float m_economyWeight = 1.0f;
    float m_magicWeight = 1.0f;
    float m_techWeight = 1.0f;
};

} // namespace Race
} // namespace RTS
} // namespace Vehement
