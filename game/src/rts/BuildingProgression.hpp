#pragma once

/**
 * @file BuildingProgression.hpp
 * @brief Building unlock and progression system
 *
 * Buildings unlock based on:
 * - Player's current Age (Stone, Bronze, Iron, etc.)
 * - Researched technologies
 * - Culture-specific bonuses
 * - Building prerequisites (need X before Y)
 */

#include "Culture.hpp"
#include "CultureTech.hpp"
#include "Building.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>

namespace Vehement {
namespace RTS {

// Forward declarations
class TechTree;
class PlayerResearch;

// ============================================================================
// Age/Era System
// ============================================================================

/**
 * @brief Civilization ages that unlock content
 */
enum class Age : uint8_t {
    Stone,          ///< Basic survival - wood, basic shelters
    Bronze,         ///< Early civilization - basic metal, farms
    Iron,           ///< Advanced metals - stronger buildings
    Medieval,       ///< Castles, fortifications
    Renaissance,    ///< Advanced architecture
    Industrial,     ///< Factories, mass production
    Modern,         ///< Contemporary buildings
    Future,         ///< Sci-fi structures
    COUNT
};

/**
 * @brief Get age name
 */
inline const char* AgeToString(Age age) {
    switch (age) {
        case Age::Stone:        return "Stone Age";
        case Age::Bronze:       return "Bronze Age";
        case Age::Iron:         return "Iron Age";
        case Age::Medieval:     return "Medieval Age";
        case Age::Renaissance:  return "Renaissance";
        case Age::Industrial:   return "Industrial Age";
        case Age::Modern:       return "Modern Age";
        case Age::Future:       return "Future Age";
        default:                return "Unknown Age";
    }
}

/**
 * @brief Requirements to advance to an age
 */
struct AgeRequirement {
    Age age;
    int buildingsRequired;          ///< Total buildings needed
    int populationRequired;         ///< Population needed
    std::vector<std::string> techsRequired; ///< Technologies needed
    ResourceCost advanceCost;       ///< Resources to advance

    [[nodiscard]] bool IsMet(int buildings, int population,
                             const std::unordered_set<std::string>& completedTechs) const;
};

// ============================================================================
// Building Unlock Definition
// ============================================================================

/**
 * @brief Definition of when a building type becomes available
 */
struct BuildingUnlock {
    BuildingType type;
    std::string internalName;

    // Requirements
    Age requiredAge = Age::Stone;
    std::vector<std::string> requiredTechs;
    std::vector<BuildingType> requiredBuildings; ///< Must have these first

    // Culture restrictions
    std::optional<CultureType> cultureOnly;      ///< Only this culture can build
    std::vector<CultureType> culturesExcluded;   ///< These cultures cannot build

    // Limits
    int maxCount = -1;              ///< -1 = unlimited
    int maxPerTerritory = -1;       ///< Limit per territory
    bool isUnique = false;          ///< Only one in the world

    // Description for UI
    std::string unlockDescription;

    /**
     * @brief Check if building is available to a culture at given age
     */
    [[nodiscard]] bool IsAvailableTo(CultureType culture, Age currentAge,
                                      const std::unordered_set<std::string>& completedTechs,
                                      const std::vector<BuildingType>& existingBuildings) const;

    /**
     * @brief Get human-readable unlock requirements
     */
    [[nodiscard]] std::string GetRequirementsString() const;
};

// ============================================================================
// Building Upgrade Path
// ============================================================================

/**
 * @brief Defines upgrade path for a building
 */
struct BuildingUpgradePath {
    BuildingType baseType;
    int maxLevel = 3;

    struct LevelData {
        int level;
        std::string name;           ///< "House" -> "Manor" -> "Estate"
        ResourceCost upgradeCost;
        float upgradeTime;          ///< Seconds to upgrade

        // Stat improvements
        float hpMultiplier = 1.0f;
        float productionMultiplier = 1.0f;
        float capacityMultiplier = 1.0f;
        float visionMultiplier = 1.0f;

        // Requirements
        Age requiredAge = Age::Stone;
        std::vector<std::string> requiredTechs;
    };

    std::vector<LevelData> levels;

    [[nodiscard]] const LevelData* GetLevel(int level) const;
    [[nodiscard]] bool CanUpgradeTo(int targetLevel, Age currentAge,
                                     const std::unordered_set<std::string>& techs) const;
};

// ============================================================================
// Building Progression System
// ============================================================================

/**
 * @brief Manages building unlocks and progression
 */
class BuildingProgression {
public:
    BuildingProgression();
    ~BuildingProgression();

    // Non-copyable
    BuildingProgression(const BuildingProgression&) = delete;
    BuildingProgression& operator=(const BuildingProgression&) = delete;

    /**
     * @brief Initialize with default unlock data
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    // =========================================================================
    // Age Management
    // =========================================================================

    /**
     * @brief Get requirements for an age
     */
    [[nodiscard]] const AgeRequirement* GetAgeRequirement(Age age) const;

    /**
     * @brief Check if can advance to next age
     */
    [[nodiscard]] bool CanAdvanceAge(Age currentAge, int buildings, int population,
                                      const std::unordered_set<std::string>& techs) const;

    /**
     * @brief Get next age after current
     */
    [[nodiscard]] Age GetNextAge(Age current) const;

    // =========================================================================
    // Building Availability
    // =========================================================================

    /**
     * @brief Get all buildings available to player
     */
    [[nodiscard]] std::vector<BuildingType> GetAvailableBuildings(
        CultureType culture, Age currentAge,
        const std::unordered_set<std::string>& completedTechs,
        const std::vector<BuildingType>& existingBuildings) const;

    /**
     * @brief Check if specific building can be built
     */
    [[nodiscard]] bool CanBuild(BuildingType type, CultureType culture, Age currentAge,
                                 const std::unordered_set<std::string>& completedTechs,
                                 const std::vector<BuildingType>& existingBuildings) const;

    /**
     * @brief Check if building count limit reached
     */
    [[nodiscard]] bool IsAtBuildLimit(BuildingType type,
                                       const std::vector<BuildingType>& existingBuildings) const;

    /**
     * @brief Get unlock requirements as string
     */
    [[nodiscard]] std::string GetUnlockRequirements(BuildingType type) const;

    /**
     * @brief Get all buildings that would be unlocked by a tech
     */
    [[nodiscard]] std::vector<BuildingType> GetBuildingsUnlockedByTech(
        const std::string& techId) const;

    // =========================================================================
    // Building Upgrades
    // =========================================================================

    /**
     * @brief Get max level for building type
     */
    [[nodiscard]] int GetMaxBuildingLevel(BuildingType type) const;

    /**
     * @brief Get max level achievable with current tech
     */
    [[nodiscard]] int GetMaxBuildingLevel(BuildingType type, Age currentAge,
                                           const std::unordered_set<std::string>& techs) const;

    /**
     * @brief Get upgrade path for building
     */
    [[nodiscard]] const BuildingUpgradePath* GetUpgradePath(BuildingType type) const;

    /**
     * @brief Check if building can be upgraded
     */
    [[nodiscard]] bool CanUpgrade(BuildingType type, int currentLevel, Age age,
                                   const std::unordered_set<std::string>& techs) const;

    /**
     * @brief Get upgrade cost
     */
    [[nodiscard]] ResourceCost GetUpgradeCost(BuildingType type, int targetLevel) const;

    /**
     * @brief Get upgrade time
     */
    [[nodiscard]] float GetUpgradeTime(BuildingType type, int targetLevel) const;

    /**
     * @brief Get level name
     */
    [[nodiscard]] std::string GetLevelName(BuildingType type, int level) const;

    // =========================================================================
    // Culture-Specific Buildings
    // =========================================================================

    /**
     * @brief Get unique buildings for culture
     */
    [[nodiscard]] std::vector<BuildingType> GetCultureUniqueBuildings(CultureType culture) const;

    /**
     * @brief Get buildings excluded from culture
     */
    [[nodiscard]] std::vector<BuildingType> GetCultureExcludedBuildings(CultureType culture) const;

    /**
     * @brief Check if building is culture-specific
     */
    [[nodiscard]] bool IsCultureSpecific(BuildingType type) const;

    /**
     * @brief Get which culture a building is specific to
     */
    [[nodiscard]] std::optional<CultureType> GetCultureForBuilding(BuildingType type) const;

    // =========================================================================
    // Building Categories
    // =========================================================================

    /**
     * @brief Get buildings by category
     */
    [[nodiscard]] std::vector<BuildingType> GetBuildingsByCategory(
        BuildingCategory category) const;

    /**
     * @brief Get buildings available at an age
     */
    [[nodiscard]] std::vector<BuildingType> GetBuildingsForAge(Age age) const;

    // =========================================================================
    // UI Helpers
    // =========================================================================

    /**
     * @brief Get building info for UI display
     */
    struct BuildingInfo {
        BuildingType type;
        std::string name;
        std::string description;
        BuildingCategory category;
        Age requiredAge;
        bool isAvailable;
        bool isLocked;
        std::string lockReason;
        ResourceCost buildCost;
        int currentCount;
        int maxCount;
    };

    [[nodiscard]] BuildingInfo GetBuildingInfo(BuildingType type, CultureType culture,
                                                Age currentAge,
                                                const std::unordered_set<std::string>& techs,
                                                const std::vector<BuildingType>& existing) const;

    /**
     * @brief Get all building info for menu
     */
    [[nodiscard]] std::vector<BuildingInfo> GetAllBuildingInfo(CultureType culture,
                                                                Age currentAge,
                                                                const std::unordered_set<std::string>& techs,
                                                                const std::vector<BuildingType>& existing) const;

private:
    void InitializeAgeRequirements();
    void InitializeBuildingUnlocks();
    void InitializeUpgradePaths();

    void AddUnlock(const BuildingUnlock& unlock);
    void AddUpgradePath(const BuildingUpgradePath& path);

    std::map<Age, AgeRequirement> m_ageRequirements;
    std::map<BuildingType, BuildingUnlock> m_unlocks;
    std::map<BuildingType, BuildingUpgradePath> m_upgradePaths;
};

// ============================================================================
// Age Advancement Manager
// ============================================================================

/**
 * @brief Tracks player's age progression
 */
class AgeProgression {
public:
    AgeProgression();

    /**
     * @brief Initialize for a player
     */
    void Initialize(CultureType culture);

    /**
     * @brief Get current age
     */
    [[nodiscard]] Age GetCurrentAge() const { return m_currentAge; }

    /**
     * @brief Check if can advance
     */
    [[nodiscard]] bool CanAdvance(const BuildingProgression& progression,
                                   int buildings, int population,
                                   const std::unordered_set<std::string>& techs) const;

    /**
     * @brief Advance to next age
     */
    bool Advance(const BuildingProgression& progression,
                 int buildings, int population,
                 const std::unordered_set<std::string>& techs);

    /**
     * @brief Get progress to next age (0.0 - 1.0)
     */
    [[nodiscard]] float GetProgressToNextAge(int buildings, int population) const;

    /**
     * @brief Get time in current age
     */
    [[nodiscard]] float GetTimeInCurrentAge() const { return m_timeInAge; }

    /**
     * @brief Update time tracking
     */
    void Update(float deltaTime);

    // Callbacks
    using AgeAdvanceCallback = std::function<void(Age oldAge, Age newAge)>;
    void SetOnAgeAdvance(AgeAdvanceCallback cb) { m_onAdvance = std::move(cb); }

private:
    CultureType m_culture = CultureType::Fortress;
    Age m_currentAge = Age::Stone;
    float m_timeInAge = 0.0f;
    AgeAdvanceCallback m_onAdvance;
};

} // namespace RTS
} // namespace Vehement
