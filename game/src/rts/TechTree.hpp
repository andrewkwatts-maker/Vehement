#pragma once

/**
 * @file TechTree.hpp
 * @brief Age of Empires-style tech tree and age progression system
 *
 * Features:
 * - Seven ages of progression from Stone Age to Future Age
 * - Technology research with prerequisites and costs
 * - Tech loss on death/defeat mechanics
 * - Unlocks for buildings, units, and abilities
 * - Firebase persistence for multiplayer sync
 */

#include "Culture.hpp"
#include "Resource.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {

// Forward declarations
class TechTree;
class TechLoss;

// ============================================================================
// Age System
// ============================================================================

/**
 * @brief Ages of civilization progression
 *
 * Players advance through ages to unlock new technologies, buildings, and units.
 * Each age represents a major leap in technological capability.
 */
enum class Age : uint8_t {
    Stone = 0,      ///< Basic survival, primitive tools, gathering
    Bronze,         ///< Metal working, better weapons, early agriculture
    Iron,           ///< Advanced metallurgy, fortifications, organized military
    Medieval,       ///< Castles, siege weapons, feudal systems
    Industrial,     ///< Machines, factories, firearms, mass production
    Modern,         ///< Electricity, vehicles, advanced communications
    Future,         ///< Advanced tech, special abilities, ultimate power

    COUNT
};

/**
 * @brief Convert Age to display string
 */
[[nodiscard]] inline const char* AgeToString(Age age) {
    switch (age) {
        case Age::Stone:      return "Stone Age";
        case Age::Bronze:     return "Bronze Age";
        case Age::Iron:       return "Iron Age";
        case Age::Medieval:   return "Medieval Age";
        case Age::Industrial: return "Industrial Age";
        case Age::Modern:     return "Modern Age";
        case Age::Future:     return "Future Age";
        default:              return "Unknown Age";
    }
}

/**
 * @brief Get short name for Age
 */
[[nodiscard]] inline const char* AgeToShortString(Age age) {
    switch (age) {
        case Age::Stone:      return "Stone";
        case Age::Bronze:     return "Bronze";
        case Age::Iron:       return "Iron";
        case Age::Medieval:   return "Medieval";
        case Age::Industrial: return "Industrial";
        case Age::Modern:     return "Modern";
        case Age::Future:     return "Future";
        default:              return "Unknown";
    }
}

/**
 * @brief Parse Age from string
 */
[[nodiscard]] inline Age StringToAge(const std::string& str) {
    if (str == "Stone" || str == "Stone Age") return Age::Stone;
    if (str == "Bronze" || str == "Bronze Age") return Age::Bronze;
    if (str == "Iron" || str == "Iron Age") return Age::Iron;
    if (str == "Medieval" || str == "Medieval Age") return Age::Medieval;
    if (str == "Industrial" || str == "Industrial Age") return Age::Industrial;
    if (str == "Modern" || str == "Modern Age") return Age::Modern;
    if (str == "Future" || str == "Future Age") return Age::Future;
    return Age::Stone;
}

/**
 * @brief Get the numeric index of an age (0-6)
 */
[[nodiscard]] inline int AgeToIndex(Age age) {
    return static_cast<int>(age);
}

/**
 * @brief Get age from numeric index
 */
[[nodiscard]] inline Age IndexToAge(int index) {
    if (index < 0) return Age::Stone;
    if (index >= static_cast<int>(Age::COUNT)) return Age::Future;
    return static_cast<Age>(index);
}

// ============================================================================
// Age Requirements
// ============================================================================

/**
 * @brief Requirements to advance to a new age
 *
 * Players must meet resource costs, research required technologies,
 * and wait through a research time to advance ages.
 */
struct AgeRequirements {
    Age age = Age::Stone;                           ///< The age this unlocks
    std::map<ResourceType, int> resourceCost;       ///< Resources required
    std::vector<std::string> requiredTechs;         ///< Technologies needed
    float researchTime = 60.0f;                     ///< Time to advance (seconds)
    std::string description;                        ///< Flavor text description
    int requiredBuildings = 0;                      ///< Min buildings to advance
    int requiredPopulation = 0;                     ///< Min population to advance

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static AgeRequirements FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tech Effects
// ============================================================================

/**
 * @brief Types of effects a technology can provide
 */
enum class TechEffectType : uint8_t {
    // Stat modifiers
    StatMultiplier,         ///< Multiplies a stat (e.g., +20% damage)
    StatFlat,               ///< Adds a flat value (e.g., +50 HP)

    // Unlocks
    UnlockBuilding,         ///< Allows construction of a building
    UnlockUnit,             ///< Allows training of a unit type
    UnlockAbility,          ///< Grants a special ability
    UnlockResource,         ///< Unlocks a new resource type

    // Mechanics
    EnableFeature,          ///< Enables a gameplay feature
    ModifyMechanic,         ///< Changes how something works
    ReduceCost,             ///< Reduces costs of something

    // Age-specific
    UpgradeExisting,        ///< Upgrades existing units/buildings
    RevealMap,              ///< Reveals portion of the map
};

/**
 * @brief Single effect provided by a technology
 */
struct TechEffect {
    TechEffectType type = TechEffectType::StatMultiplier;
    std::string target;             ///< What this effect applies to (stat name, building ID, etc.)
    float value = 0.0f;             ///< Numeric value for multipliers/bonuses
    std::string stringValue;        ///< String value for unlocks
    std::string description;        ///< Human-readable description

    // Factory methods for easy creation
    [[nodiscard]] static TechEffect Multiplier(const std::string& target, float mult, const std::string& desc);
    [[nodiscard]] static TechEffect FlatBonus(const std::string& target, float amount, const std::string& desc);
    [[nodiscard]] static TechEffect UnlockBuilding(const std::string& buildingId, const std::string& desc = "");
    [[nodiscard]] static TechEffect UnlockUnit(const std::string& unitId, const std::string& desc = "");
    [[nodiscard]] static TechEffect UnlockAbility(const std::string& abilityId, const std::string& desc);
    [[nodiscard]] static TechEffect EnableFeature(const std::string& feature, const std::string& desc);
    [[nodiscard]] static TechEffect ReduceCost(const std::string& target, float percent, const std::string& desc);

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechEffect FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tech Node
// ============================================================================

/**
 * @brief Category for organizing technologies in the UI
 */
enum class TechCategory : uint8_t {
    Military,           ///< Combat units, weapons, tactics
    Economy,            ///< Resource gathering, production, trade
    Defense,            ///< Walls, towers, fortifications
    Infrastructure,     ///< Buildings, construction, logistics
    Science,            ///< Research speed, special techs
    Special,            ///< Unique culture-specific technologies

    COUNT
};

[[nodiscard]] inline const char* TechCategoryToString(TechCategory cat) {
    switch (cat) {
        case TechCategory::Military:        return "Military";
        case TechCategory::Economy:         return "Economy";
        case TechCategory::Defense:         return "Defense";
        case TechCategory::Infrastructure:  return "Infrastructure";
        case TechCategory::Science:         return "Science";
        case TechCategory::Special:         return "Special";
        default:                            return "Unknown";
    }
}

/**
 * @brief Single node in the technology tree
 *
 * Represents one researchable technology with its costs, prerequisites,
 * and effects when researched. Technologies can be lost on death.
 */
struct TechNode {
    // Identity
    std::string id;                     ///< Unique identifier (e.g., "tech_bronze_weapons")
    std::string name;                   ///< Display name (e.g., "Bronze Weapons")
    std::string description;            ///< Full description text
    std::string iconPath;               ///< Path to icon texture

    // Classification
    TechCategory category = TechCategory::Military;
    Age requiredAge = Age::Stone;       ///< Minimum age to research this tech

    // Requirements
    std::vector<std::string> prerequisites;         ///< IDs of techs that must be researched first
    std::map<ResourceType, int> cost;               ///< Resource costs to research
    float researchTime = 30.0f;                     ///< Time in seconds to complete research

    // Effects when researched
    std::vector<TechEffect> effects;
    std::vector<std::string> unlocksBuildings;      ///< Building IDs unlocked
    std::vector<std::string> unlocksUnits;          ///< Unit types unlocked
    std::vector<std::string> unlocksAbilities;      ///< Ability IDs unlocked

    // Tech loss settings (for death mechanics)
    float lossChanceOnDeath = 0.3f;     ///< 30% chance to lose on hero death
    bool canBeLost = true;              ///< Some techs are permanent (can't be lost)
    Age minimumAgeLoss = Age::Stone;    ///< Can't lose if below this age's techs

    // Culture restrictions
    std::vector<CultureType> availableToCultures;   ///< Empty = all cultures
    bool isUniversal = false;                       ///< Available to all cultures

    // UI positioning
    int treeRow = 0;                    ///< Row in tech tree visualization
    int treeColumn = 0;                 ///< Column in tech tree visualization
    int tier = 1;                       ///< Tier within age (1-3 typically)

    // Gameplay
    bool isKeyTech = false;             ///< Key techs have reduced loss chance
    float protectionBonus = 0.0f;       ///< Bonus protection against tech loss

    /**
     * @brief Check if tech is available to a specific culture
     */
    [[nodiscard]] bool IsAvailableTo(CultureType culture) const;

    /**
     * @brief Get total resource cost value
     */
    [[nodiscard]] int GetTotalCostValue() const;

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static TechNode FromJson(const nlohmann::json& j);
};

// ============================================================================
// Research Status
// ============================================================================

/**
 * @brief Current status of a technology for a player
 */
enum class TechStatus : uint8_t {
    Locked,             ///< Prerequisites not met
    Available,          ///< Can be researched (prereqs met, age ok)
    InProgress,         ///< Currently being researched
    Completed,          ///< Research complete, effects active
    Lost                ///< Was researched but lost (can re-research)
};

[[nodiscard]] inline const char* TechStatusToString(TechStatus status) {
    switch (status) {
        case TechStatus::Locked:        return "Locked";
        case TechStatus::Available:     return "Available";
        case TechStatus::InProgress:    return "In Progress";
        case TechStatus::Completed:     return "Completed";
        case TechStatus::Lost:          return "Lost";
        default:                        return "Unknown";
    }
}

/**
 * @brief Research progress tracking for a single technology
 */
struct ResearchProgress {
    std::string techId;
    TechStatus status = TechStatus::Locked;
    float progressTime = 0.0f;          ///< Time spent researching
    float totalTime = 0.0f;             ///< Total time required
    int64_t startedTimestamp = 0;       ///< When research started
    int64_t completedTimestamp = 0;     ///< When research completed
    int timesResearched = 0;            ///< How many times researched (for re-research)
    int timesLost = 0;                  ///< How many times lost

    [[nodiscard]] float GetProgressPercent() const {
        return (totalTime > 0.0f) ? std::min(1.0f, progressTime / totalTime) : 0.0f;
    }

    [[nodiscard]] float GetRemainingTime() const {
        return std::max(0.0f, totalTime - progressTime);
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static ResearchProgress FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tech Tree Class
// ============================================================================

/**
 * @brief Complete technology tree manager for a player
 *
 * Manages:
 * - All available technologies and their definitions
 * - Player's researched techs and current research
 * - Age advancement and requirements
 * - Research queue processing
 * - Firebase synchronization
 *
 * Example usage:
 * @code
 * TechTree tree;
 * tree.Initialize(CultureType::Fortress);
 *
 * // Start researching
 * if (tree.CanResearch("tech_bronze_weapons")) {
 *     tree.StartResearch("tech_bronze_weapons");
 * }
 *
 * // Update each frame
 * tree.UpdateResearch(deltaTime);
 *
 * // Check unlocks
 * if (tree.HasTech("tech_stone_walls")) {
 *     // Can build stone walls
 * }
 *
 * // Advance age
 * if (tree.CanAdvanceAge()) {
 *     tree.AdvanceAge();
 * }
 * @endcode
 */
class TechTree {
public:
    // Callback types
    using ResearchCompleteCallback = std::function<void(const std::string& techId, const TechNode& tech)>;
    using AgeAdvanceCallback = std::function<void(Age newAge, Age previousAge)>;
    using TechLostCallback = std::function<void(const std::string& techId, const TechNode& tech)>;

    TechTree();
    ~TechTree();

    // Non-copyable but movable
    TechTree(const TechTree&) = delete;
    TechTree& operator=(const TechTree&) = delete;
    TechTree(TechTree&&) noexcept;
    TechTree& operator=(TechTree&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize tech tree for a culture
     * @param culture Player's selected culture
     * @param playerId Player's unique ID for Firebase sync
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(CultureType culture, const std::string& playerId = "");

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Set player culture (reinitializes available techs)
     */
    void SetCulture(CultureType culture);

    /**
     * @brief Get player's culture
     */
    [[nodiscard]] CultureType GetCulture() const noexcept { return m_culture; }

    // =========================================================================
    // Tech Node Access
    // =========================================================================

    /**
     * @brief Get a technology definition by ID
     * @return Pointer to TechNode or nullptr if not found
     */
    [[nodiscard]] const TechNode* GetTech(const std::string& techId) const;

    /**
     * @brief Get all technology definitions
     */
    [[nodiscard]] const std::unordered_map<std::string, TechNode>& GetAllTechs() const {
        return m_allTechs;
    }

    /**
     * @brief Get technologies available for current age and culture
     */
    [[nodiscard]] std::vector<const TechNode*> GetAvailableTechs() const;

    /**
     * @brief Get technologies available for a specific age
     */
    [[nodiscard]] std::vector<const TechNode*> GetTechsForAge(Age age) const;

    /**
     * @brief Get technologies in a category
     */
    [[nodiscard]] std::vector<const TechNode*> GetTechsByCategory(TechCategory category) const;

    /**
     * @brief Get technologies that unlock a building
     */
    [[nodiscard]] std::vector<const TechNode*> GetTechsUnlockingBuilding(const std::string& buildingId) const;

    // =========================================================================
    // Research Status
    // =========================================================================

    /**
     * @brief Check if a technology has been researched
     */
    [[nodiscard]] bool HasTech(const std::string& techId) const;

    /**
     * @brief Check if a technology can be researched now
     */
    [[nodiscard]] bool CanResearch(const std::string& techId) const;

    /**
     * @brief Get status of a technology
     */
    [[nodiscard]] TechStatus GetTechStatus(const std::string& techId) const;

    /**
     * @brief Get research progress for a technology
     */
    [[nodiscard]] std::optional<ResearchProgress> GetResearchProgress(const std::string& techId) const;

    /**
     * @brief Get all completed technologies
     */
    [[nodiscard]] const std::set<std::string>& GetResearchedTechs() const {
        return m_researchedTechs;
    }

    /**
     * @brief Get missing prerequisites for a tech
     */
    [[nodiscard]] std::vector<std::string> GetMissingPrerequisites(const std::string& techId) const;

    // =========================================================================
    // Research Actions
    // =========================================================================

    /**
     * @brief Start researching a technology
     * @return true if research started successfully
     */
    bool StartResearch(const std::string& techId);

    /**
     * @brief Update research progress (call each frame)
     * @param deltaTime Time since last update in seconds
     */
    void UpdateResearch(float deltaTime);

    /**
     * @brief Complete the current research immediately
     */
    void CompleteResearch();

    /**
     * @brief Cancel current research
     * @param refundPercent Percentage of resources to refund (0.0-1.0)
     * @return Map of refunded resources
     */
    std::map<ResourceType, int> CancelResearch(float refundPercent = 0.5f);

    /**
     * @brief Get currently researching technology ID
     * @return Tech ID or empty string if not researching
     */
    [[nodiscard]] const std::string& GetCurrentResearch() const {
        return m_currentResearch;
    }

    /**
     * @brief Get current research progress (0.0-1.0)
     */
    [[nodiscard]] float GetResearchProgress() const {
        return m_researchProgress;
    }

    /**
     * @brief Check if currently researching anything
     */
    [[nodiscard]] bool IsResearching() const {
        return !m_currentResearch.empty();
    }

    /**
     * @brief Grant a technology immediately (cheat/debug/scenario)
     */
    void GrantTech(const std::string& techId);

    /**
     * @brief Remove a researched technology (for tech loss)
     * @return true if tech was removed
     */
    bool LoseTech(const std::string& techId);

    // =========================================================================
    // Research Queue
    // =========================================================================

    /**
     * @brief Add technology to research queue
     * @return true if added successfully
     */
    bool QueueResearch(const std::string& techId);

    /**
     * @brief Remove technology from queue
     */
    bool DequeueResearch(const std::string& techId);

    /**
     * @brief Get research queue
     */
    [[nodiscard]] const std::vector<std::string>& GetResearchQueue() const {
        return m_researchQueue;
    }

    /**
     * @brief Clear research queue
     */
    void ClearResearchQueue();

    // =========================================================================
    // Age System
    // =========================================================================

    /**
     * @brief Get current age
     */
    [[nodiscard]] Age GetCurrentAge() const noexcept { return m_currentAge; }

    /**
     * @brief Check if can advance to next age
     */
    [[nodiscard]] bool CanAdvanceAge() const;

    /**
     * @brief Get requirements for next age
     * @return Requirements or nullopt if already at max age
     */
    [[nodiscard]] std::optional<AgeRequirements> GetNextAgeRequirements() const;

    /**
     * @brief Start advancing to next age
     * @return true if advancement started
     */
    bool StartAgeAdvancement();

    /**
     * @brief Complete age advancement
     */
    void AdvanceAge();

    /**
     * @brief Regress to a lower age (from tech loss)
     */
    void RegressToAge(Age age);

    /**
     * @brief Check if advancing age
     */
    [[nodiscard]] bool IsAdvancingAge() const noexcept { return m_isAdvancingAge; }

    /**
     * @brief Get age advancement progress
     */
    [[nodiscard]] float GetAgeAdvancementProgress() const {
        return m_ageAdvancementProgress;
    }

    /**
     * @brief Get requirements for a specific age
     */
    [[nodiscard]] const AgeRequirements& GetAgeRequirements(Age age) const;

    // =========================================================================
    // Effect Calculations
    // =========================================================================

    /**
     * @brief Get total multiplier bonus for a stat from all techs
     * @param statName Name of the stat (e.g., "attack_damage", "gather_speed")
     * @return Multiplier (1.0 = no bonus, 1.2 = +20%)
     */
    [[nodiscard]] float GetStatMultiplier(const std::string& statName) const;

    /**
     * @brief Get total flat bonus for a stat from all techs
     */
    [[nodiscard]] float GetStatFlatBonus(const std::string& statName) const;

    /**
     * @brief Check if a building is unlocked by research
     */
    [[nodiscard]] bool IsBuildingUnlocked(const std::string& buildingId) const;

    /**
     * @brief Check if a unit is unlocked by research
     */
    [[nodiscard]] bool IsUnitUnlocked(const std::string& unitId) const;

    /**
     * @brief Check if an ability is unlocked by research
     */
    [[nodiscard]] bool IsAbilityUnlocked(const std::string& abilityId) const;

    /**
     * @brief Get all unlocked buildings
     */
    [[nodiscard]] std::vector<std::string> GetUnlockedBuildings() const;

    /**
     * @brief Get all unlocked units
     */
    [[nodiscard]] std::vector<std::string> GetUnlockedUnits() const;

    /**
     * @brief Get all unlocked abilities
     */
    [[nodiscard]] std::vector<std::string> GetUnlockedAbilities() const;

    // =========================================================================
    // Tech Protection
    // =========================================================================

    /**
     * @brief Get protection level against tech loss (0.0-1.0)
     */
    [[nodiscard]] float GetTechProtectionLevel() const;

    /**
     * @brief Check if a specific tech is protected from loss
     */
    [[nodiscard]] bool IsTechProtected(const std::string& techId) const;

    /**
     * @brief Add temporary tech protection
     * @param bonus Protection bonus (0.0-1.0)
     * @param duration Duration in seconds
     */
    void AddTechProtection(float bonus, float duration);

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnResearchComplete(ResearchCompleteCallback callback) {
        m_onResearchComplete = std::move(callback);
    }

    void SetOnAgeAdvance(AgeAdvanceCallback callback) {
        m_onAgeAdvance = std::move(callback);
    }

    void SetOnTechLost(TechLostCallback callback) {
        m_onTechLost = std::move(callback);
    }

    // =========================================================================
    // Persistence
    // =========================================================================

    /**
     * @brief Serialize tech tree state to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Load tech tree state from JSON
     */
    void FromJson(const nlohmann::json& j);

    /**
     * @brief Save to Firebase
     */
    void SaveToFirebase();

    /**
     * @brief Load from Firebase
     * @param callback Called when load completes
     */
    void LoadFromFirebase(std::function<void(bool success)> callback);

    /**
     * @brief Enable real-time sync with Firebase
     */
    void EnableFirebaseSync();

    /**
     * @brief Disable Firebase sync
     */
    void DisableFirebaseSync();

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of techs researched (all time)
     */
    [[nodiscard]] int GetTotalTechsResearched() const { return m_totalTechsResearched; }

    /**
     * @brief Get total number of techs lost (all time)
     */
    [[nodiscard]] int GetTotalTechsLost() const { return m_totalTechsLost; }

    /**
     * @brief Get highest age ever achieved
     */
    [[nodiscard]] Age GetHighestAgeAchieved() const { return m_highestAgeAchieved; }

    /**
     * @brief Get total research time spent
     */
    [[nodiscard]] float GetTotalResearchTime() const { return m_totalResearchTime; }

private:
    // Initialization helpers
    void InitializeAgeRequirements();
    void InitializeUniversalTechs();
    void InitializeCultureTechs(CultureType culture);

    // Tech tree building
    void AddTech(TechNode tech);
    void BuildTechDependencies();

    // Research helpers
    void ProcessResearchCompletion();
    void ProcessResearchQueue();
    void RecalculateBonuses();

    // Firebase helpers
    std::string GetFirebasePath() const;

    // State
    bool m_initialized = false;
    CultureType m_culture = CultureType::Fortress;
    std::string m_playerId;

    // All available techs (definitions)
    std::unordered_map<std::string, TechNode> m_allTechs;

    // Player's researched techs
    std::set<std::string> m_researchedTechs;
    std::unordered_map<std::string, ResearchProgress> m_techProgress;

    // Current research
    std::string m_currentResearch;
    float m_researchProgress = 0.0f;
    std::vector<std::string> m_researchQueue;

    // Age system
    Age m_currentAge = Age::Stone;
    bool m_isAdvancingAge = false;
    float m_ageAdvancementProgress = 0.0f;
    float m_ageAdvancementTime = 0.0f;
    std::vector<AgeRequirements> m_ageRequirements;

    // Cached bonuses
    std::unordered_map<std::string, float> m_statMultipliers;
    std::unordered_map<std::string, float> m_statFlatBonuses;
    std::unordered_set<std::string> m_unlockedBuildings;
    std::unordered_set<std::string> m_unlockedUnits;
    std::unordered_set<std::string> m_unlockedAbilities;

    // Tech protection
    float m_baseTechProtection = 0.0f;
    float m_temporaryProtection = 0.0f;
    float m_protectionDuration = 0.0f;

    // Statistics
    int m_totalTechsResearched = 0;
    int m_totalTechsLost = 0;
    Age m_highestAgeAchieved = Age::Stone;
    float m_totalResearchTime = 0.0f;

    // Firebase
    std::string m_firebaseListenerId;
    bool m_firebaseSyncEnabled = false;

    // Callbacks
    ResearchCompleteCallback m_onResearchComplete;
    AgeAdvanceCallback m_onAgeAdvance;
    TechLostCallback m_onTechLost;
};

// ============================================================================
// Default Tech IDs
// ============================================================================

/**
 * @brief Universal technology IDs (available to all cultures)
 */
namespace UniversalTechs {
    // Stone Age
    constexpr const char* PRIMITIVE_TOOLS = "tech_primitive_tools";
    constexpr const char* BASIC_SHELTER = "tech_basic_shelter";
    constexpr const char* FORAGING = "tech_foraging";
    constexpr const char* STONE_WEAPONS = "tech_stone_weapons";

    // Bronze Age
    constexpr const char* BRONZE_WORKING = "tech_bronze_working";
    constexpr const char* BRONZE_WEAPONS = "tech_bronze_weapons";
    constexpr const char* AGRICULTURE = "tech_agriculture";
    constexpr const char* POTTERY = "tech_pottery";
    constexpr const char* BASIC_WALLS = "tech_basic_walls";

    // Iron Age
    constexpr const char* IRON_WORKING = "tech_iron_working";
    constexpr const char* IRON_WEAPONS = "tech_iron_weapons";
    constexpr const char* STONE_FORTIFICATIONS = "tech_stone_fortifications";
    constexpr const char* IRON_ARMOR = "tech_iron_armor";
    constexpr const char* ADVANCED_FARMING = "tech_advanced_farming";

    // Medieval Age
    constexpr const char* CASTLE_BUILDING = "tech_castle_building";
    constexpr const char* SIEGE_WEAPONS = "tech_siege_weapons";
    constexpr const char* HEAVY_CAVALRY = "tech_heavy_cavalry";
    constexpr const char* CROSSBOWS = "tech_crossbows";
    constexpr const char* GUILDS = "tech_guilds";

    // Industrial Age
    constexpr const char* STEAM_POWER = "tech_steam_power";
    constexpr const char* FIREARMS = "tech_firearms";
    constexpr const char* FACTORIES = "tech_factories";
    constexpr const char* RAILROADS = "tech_railroads";
    constexpr const char* ARTILLERY = "tech_artillery";

    // Modern Age
    constexpr const char* ELECTRICITY = "tech_electricity";
    constexpr const char* COMBUSTION_ENGINE = "tech_combustion_engine";
    constexpr const char* RADIO_COMM = "tech_radio_communication";
    constexpr const char* AUTOMATIC_WEAPONS = "tech_automatic_weapons";
    constexpr const char* TANKS = "tech_tanks";

    // Future Age
    constexpr const char* FUSION_POWER = "tech_fusion_power";
    constexpr const char* ENERGY_SHIELDS = "tech_energy_shields";
    constexpr const char* PLASMA_WEAPONS = "tech_plasma_weapons";
    constexpr const char* AI_SYSTEMS = "tech_ai_systems";
    constexpr const char* NANOTECH = "tech_nanotech";
}

/**
 * @brief Fortress culture specific techs
 */
namespace FortressTechs {
    constexpr const char* STONE_MASONRY = "tech_fortress_stone_masonry";
    constexpr const char* THICK_WALLS = "tech_fortress_thick_walls";
    constexpr const char* CASTLE_KEEP = "tech_fortress_castle_keep";
    constexpr const char* SIEGE_DEFENSE = "tech_fortress_siege_defense";
    constexpr const char* GARRISON_TRAINING = "tech_fortress_garrison";
    constexpr const char* IMPENETRABLE = "tech_fortress_impenetrable";
}

/**
 * @brief Nomad culture specific techs
 */
namespace NomadTechs {
    constexpr const char* MOBILE_CAMPS = "tech_nomad_mobile_camps";
    constexpr const char* SWIFT_PACK = "tech_nomad_swift_pack";
    constexpr const char* HIT_AND_RUN = "tech_nomad_hit_and_run";
    constexpr const char* CARAVAN_MASTERS = "tech_nomad_caravan_masters";
    constexpr const char* GUERRILLA_TACTICS = "tech_nomad_guerrilla";
    constexpr const char* WIND_RIDERS = "tech_nomad_wind_riders";
}

/**
 * @brief Merchant culture specific techs
 */
namespace MerchantTechs {
    constexpr const char* TRADE_ROUTES = "tech_merchant_trade_routes";
    constexpr const char* BAZAAR_NETWORK = "tech_merchant_bazaar";
    constexpr const char* MERCENARIES = "tech_merchant_mercenaries";
    constexpr const char* DIPLOMACY = "tech_merchant_diplomacy";
    constexpr const char* GOLD_RESERVES = "tech_merchant_gold_reserves";
    constexpr const char* TRADE_EMPIRE = "tech_merchant_trade_empire";
}

/**
 * @brief Industrial culture specific techs
 */
namespace IndustrialTechs {
    constexpr const char* ASSEMBLY_LINE = "tech_industrial_assembly_line";
    constexpr const char* MASS_PRODUCTION = "tech_industrial_mass_production";
    constexpr const char* AUTOMATION = "tech_industrial_automation";
    constexpr const char* LOGISTICS = "tech_industrial_logistics";
    constexpr const char* FACTORY_UPGRADE = "tech_industrial_factory_upgrade";
    constexpr const char* REVOLUTION = "tech_industrial_revolution";
}

} // namespace RTS
} // namespace Vehement
