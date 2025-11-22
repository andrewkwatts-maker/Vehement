#pragma once

/**
 * @file TechNode.hpp
 * @brief Individual technology node definition
 *
 * A TechNode represents a single researchable technology in the tech tree.
 * Nodes contain:
 * - Identity (ID, name, description, icon)
 * - Prerequisites (required techs)
 * - Costs (resources, time)
 * - Unlocks (abilities, units, buildings, upgrades, spells)
 * - Modifiers (stat changes)
 * - Age/era requirements
 * - Script hooks (Create/Tick/Destroy)
 * - JSON serialization support
 */

#include "TechModifier.hpp"
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
namespace TechTree {

// Forward declarations
class TechTree;
class TechManager;

// ============================================================================
// Age/Era System
// ============================================================================

/**
 * @brief Ages/eras of civilization progression
 */
enum class TechAge : uint8_t {
    Primitive = 0,  ///< Very early game, basic survival
    Stone,          ///< Stone Age - primitive tools, gathering
    Bronze,         ///< Bronze Age - metal working, early agriculture
    Iron,           ///< Iron Age - advanced metallurgy, fortifications
    Classical,      ///< Classical Age - philosophy, organized states
    Medieval,       ///< Medieval Age - castles, siege weapons
    Renaissance,    ///< Renaissance - early science, gunpowder
    Industrial,     ///< Industrial Age - machines, factories
    Modern,         ///< Modern Age - electricity, vehicles
    Atomic,         ///< Atomic Age - nuclear power, computers
    Information,    ///< Information Age - internet, automation
    Future,         ///< Future Age - advanced tech, AI

    COUNT
};

/**
 * @brief Convert TechAge to display string
 */
[[nodiscard]] inline const char* TechAgeToString(TechAge age) {
    switch (age) {
        case TechAge::Primitive:   return "Primitive Age";
        case TechAge::Stone:       return "Stone Age";
        case TechAge::Bronze:      return "Bronze Age";
        case TechAge::Iron:        return "Iron Age";
        case TechAge::Classical:   return "Classical Age";
        case TechAge::Medieval:    return "Medieval Age";
        case TechAge::Renaissance: return "Renaissance";
        case TechAge::Industrial:  return "Industrial Age";
        case TechAge::Modern:      return "Modern Age";
        case TechAge::Atomic:      return "Atomic Age";
        case TechAge::Information: return "Information Age";
        case TechAge::Future:      return "Future Age";
        default:                   return "Unknown Age";
    }
}

/**
 * @brief Get short age identifier
 */
[[nodiscard]] inline const char* TechAgeToShortString(TechAge age) {
    switch (age) {
        case TechAge::Primitive:   return "primitive";
        case TechAge::Stone:       return "stone";
        case TechAge::Bronze:      return "bronze";
        case TechAge::Iron:        return "iron";
        case TechAge::Classical:   return "classical";
        case TechAge::Medieval:    return "medieval";
        case TechAge::Renaissance: return "renaissance";
        case TechAge::Industrial:  return "industrial";
        case TechAge::Modern:      return "modern";
        case TechAge::Atomic:      return "atomic";
        case TechAge::Information: return "information";
        case TechAge::Future:      return "future";
        default:                   return "unknown";
    }
}

/**
 * @brief Parse TechAge from string
 */
[[nodiscard]] inline TechAge StringToTechAge(const std::string& str) {
    if (str == "primitive") return TechAge::Primitive;
    if (str == "stone")     return TechAge::Stone;
    if (str == "bronze")    return TechAge::Bronze;
    if (str == "iron")      return TechAge::Iron;
    if (str == "classical") return TechAge::Classical;
    if (str == "medieval")  return TechAge::Medieval;
    if (str == "renaissance") return TechAge::Renaissance;
    if (str == "industrial") return TechAge::Industrial;
    if (str == "modern")    return TechAge::Modern;
    if (str == "atomic")    return TechAge::Atomic;
    if (str == "information") return TechAge::Information;
    if (str == "future")    return TechAge::Future;
    return TechAge::Stone;
}

// ============================================================================
// Technology Category
// ============================================================================

/**
 * @brief Category for organizing technologies
 */
enum class TechCategory : uint8_t {
    Military,       ///< Combat units, weapons, tactics
    Economy,        ///< Resource gathering, production, trade
    Defense,        ///< Walls, towers, fortifications
    Infrastructure, ///< Buildings, construction, logistics
    Science,        ///< Research speed, special techs
    Magic,          ///< Spells, enchantments, magical abilities
    Culture,        ///< Unique culture-specific technologies
    Special,        ///< Unique or one-time technologies

    COUNT
};

[[nodiscard]] inline const char* TechCategoryToString(TechCategory cat) {
    switch (cat) {
        case TechCategory::Military:       return "Military";
        case TechCategory::Economy:        return "Economy";
        case TechCategory::Defense:        return "Defense";
        case TechCategory::Infrastructure: return "Infrastructure";
        case TechCategory::Science:        return "Science";
        case TechCategory::Magic:          return "Magic";
        case TechCategory::Culture:        return "Culture";
        case TechCategory::Special:        return "Special";
        default:                           return "Unknown";
    }
}

[[nodiscard]] inline TechCategory StringToTechCategory(const std::string& str) {
    if (str == "military")       return TechCategory::Military;
    if (str == "economy")        return TechCategory::Economy;
    if (str == "defense")        return TechCategory::Defense;
    if (str == "infrastructure") return TechCategory::Infrastructure;
    if (str == "science")        return TechCategory::Science;
    if (str == "magic")          return TechCategory::Magic;
    if (str == "culture")        return TechCategory::Culture;
    if (str == "special")        return TechCategory::Special;
    return TechCategory::Military;
}

// ============================================================================
// Resource Cost
// ============================================================================

/**
 * @brief Resource cost for researching a technology
 */
struct TechCost {
    std::map<std::string, int> resources;   ///< Resource type -> amount
    float time = 30.0f;                     ///< Research time in seconds
    int researchPoints = 0;                 ///< Special research points required

    [[nodiscard]] bool IsEmpty() const {
        return resources.empty() && researchPoints == 0;
    }

    [[nodiscard]] int GetResourceCost(const std::string& resource) const {
        auto it = resources.find(resource);
        return it != resources.end() ? it->second : 0;
    }

    void SetResourceCost(const std::string& resource, int amount) {
        if (amount > 0) {
            resources[resource] = amount;
        } else {
            resources.erase(resource);
        }
    }

    [[nodiscard]] int GetTotalResourceValue() const {
        int total = 0;
        for (const auto& [res, amount] : resources) {
            total += amount;
        }
        return total;
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechCost FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tech Unlocks
// ============================================================================

/**
 * @brief What a technology unlocks when researched
 */
struct TechUnlocks {
    std::vector<std::string> buildings;     ///< Building IDs unlocked
    std::vector<std::string> units;         ///< Unit type IDs unlocked
    std::vector<std::string> abilities;     ///< Ability IDs unlocked
    std::vector<std::string> upgrades;      ///< Upgrade IDs unlocked
    std::vector<std::string> spells;        ///< Spell IDs unlocked
    std::vector<std::string> features;      ///< Game features enabled
    std::vector<std::string> resources;     ///< New resource types available

    [[nodiscard]] bool IsEmpty() const {
        return buildings.empty() && units.empty() && abilities.empty() &&
               upgrades.empty() && spells.empty() && features.empty() &&
               resources.empty();
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechUnlocks FromJson(const nlohmann::json& j);
};

// ============================================================================
// Script Events
// ============================================================================

/**
 * @brief Script hooks for technology lifecycle events
 */
struct TechScriptEvents {
    std::string onResearchStart;        ///< Script called when research begins
    std::string onResearchComplete;     ///< Script called when research completes
    std::string onResearchCancel;       ///< Script called when research is cancelled
    std::string onCreate;               ///< Script called when tech is granted
    std::string onTick;                 ///< Script called each game tick (if tech is active)
    std::string onDestroy;              ///< Script called when tech is lost
    std::string onApply;                ///< Script called when effects are applied
    std::string onRemove;               ///< Script called when effects are removed

    [[nodiscard]] bool HasAnyScripts() const {
        return !onResearchStart.empty() || !onResearchComplete.empty() ||
               !onResearchCancel.empty() || !onCreate.empty() ||
               !onTick.empty() || !onDestroy.empty() ||
               !onApply.empty() || !onRemove.empty();
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechScriptEvents FromJson(const nlohmann::json& j);
};

// ============================================================================
// Visual Position
// ============================================================================

/**
 * @brief Visual position for UI layout
 */
struct TechPosition {
    float x = 0.0f;     ///< X position in tech tree UI
    float y = 0.0f;     ///< Y position in tech tree UI
    int row = 0;        ///< Row in grid layout
    int column = 0;     ///< Column in grid layout

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechPosition FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tech Node
// ============================================================================

/**
 * @brief Single node in the technology tree
 *
 * A TechNode represents one researchable technology with all its
 * requirements, costs, effects, and unlocks.
 *
 * Example JSON:
 * @code
 * {
 *   "id": "tech_iron_weapons",
 *   "name": "Iron Weapons",
 *   "description": "Unlocks iron weapons for improved combat.",
 *   "icon": "icons/tech/iron_weapons.png",
 *   "tier": 2,
 *   "age_requirement": "iron",
 *   "category": "military",
 *   "cost": {
 *     "metal": 200,
 *     "time": 120
 *   },
 *   "prerequisites": ["tech_bronze_working"],
 *   "unlocks": {
 *     "units": ["swordsman", "pikeman"],
 *     "upgrades": ["iron_sword", "iron_armor"]
 *   },
 *   "modifiers": [
 *     {"stat": "damage", "type": "flat", "value": 5, "scope": {"unit_type": "melee"}}
 *   ],
 *   "events": {
 *     "on_research_complete": "scripts/tech/iron_weapons_complete.py"
 *   },
 *   "position": {"x": 100, "y": 200}
 * }
 * @endcode
 */
class TechNode {
public:
    TechNode() = default;
    explicit TechNode(const std::string& id);
    ~TechNode() = default;

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] const std::string& GetId() const { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& GetDescription() const { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    [[nodiscard]] const std::string& GetIcon() const { return m_icon; }
    void SetIcon(const std::string& icon) { m_icon = icon; }

    [[nodiscard]] const std::string& GetFlavorText() const { return m_flavorText; }
    void SetFlavorText(const std::string& text) { m_flavorText = text; }

    // =========================================================================
    // Classification
    // =========================================================================

    [[nodiscard]] TechCategory GetCategory() const { return m_category; }
    void SetCategory(TechCategory cat) { m_category = cat; }

    [[nodiscard]] TechAge GetAgeRequirement() const { return m_ageRequirement; }
    void SetAgeRequirement(TechAge age) { m_ageRequirement = age; }

    [[nodiscard]] int GetTier() const { return m_tier; }
    void SetTier(int tier) { m_tier = tier; }

    [[nodiscard]] const std::vector<std::string>& GetTags() const { return m_tags; }
    void SetTags(const std::vector<std::string>& tags) { m_tags = tags; }
    void AddTag(const std::string& tag) { m_tags.push_back(tag); }
    [[nodiscard]] bool HasTag(const std::string& tag) const;

    // =========================================================================
    // Prerequisites
    // =========================================================================

    [[nodiscard]] const std::vector<std::string>& GetPrerequisites() const { return m_prerequisites; }
    void SetPrerequisites(const std::vector<std::string>& prereqs) { m_prerequisites = prereqs; }
    void AddPrerequisite(const std::string& techId) { m_prerequisites.push_back(techId); }

    [[nodiscard]] const std::vector<std::string>& GetOptionalPrereqs() const { return m_optionalPrereqs; }
    void SetOptionalPrereqs(const std::vector<std::string>& prereqs) { m_optionalPrereqs = prereqs; }
    [[nodiscard]] int GetOptionalRequiredCount() const { return m_optionalRequiredCount; }
    void SetOptionalRequiredCount(int count) { m_optionalRequiredCount = count; }

    [[nodiscard]] const std::vector<std::string>& GetExclusiveWith() const { return m_exclusiveWith; }
    void SetExclusiveWith(const std::vector<std::string>& techs) { m_exclusiveWith = techs; }
    [[nodiscard]] bool IsExclusiveWith(const std::string& techId) const;

    // =========================================================================
    // Cost
    // =========================================================================

    [[nodiscard]] const TechCost& GetCost() const { return m_cost; }
    void SetCost(const TechCost& cost) { m_cost = cost; }

    [[nodiscard]] float GetResearchTime() const { return m_cost.time; }
    void SetResearchTime(float time) { m_cost.time = time; }

    // =========================================================================
    // Unlocks
    // =========================================================================

    [[nodiscard]] const TechUnlocks& GetUnlocks() const { return m_unlocks; }
    void SetUnlocks(const TechUnlocks& unlocks) { m_unlocks = unlocks; }

    [[nodiscard]] bool UnlocksBuilding(const std::string& buildingId) const;
    [[nodiscard]] bool UnlocksUnit(const std::string& unitId) const;
    [[nodiscard]] bool UnlocksAbility(const std::string& abilityId) const;

    // =========================================================================
    // Modifiers
    // =========================================================================

    [[nodiscard]] const std::vector<TechModifier>& GetModifiers() const { return m_modifiers; }
    void SetModifiers(const std::vector<TechModifier>& mods) { m_modifiers = mods; }
    void AddModifier(const TechModifier& mod) { m_modifiers.push_back(mod); }

    // =========================================================================
    // Scripts
    // =========================================================================

    [[nodiscard]] const TechScriptEvents& GetScriptEvents() const { return m_scriptEvents; }
    void SetScriptEvents(const TechScriptEvents& events) { m_scriptEvents = events; }

    // =========================================================================
    // UI Position
    // =========================================================================

    [[nodiscard]] const TechPosition& GetPosition() const { return m_position; }
    void SetPosition(const TechPosition& pos) { m_position = pos; }

    // =========================================================================
    // Culture/Faction Restrictions
    // =========================================================================

    [[nodiscard]] const std::vector<std::string>& GetAvailableToCultures() const { return m_availableToCultures; }
    void SetAvailableToCultures(const std::vector<std::string>& cultures) { m_availableToCultures = cultures; }

    [[nodiscard]] bool IsUniversal() const { return m_isUniversal; }
    void SetUniversal(bool universal) { m_isUniversal = universal; }

    [[nodiscard]] bool IsAvailableToCulture(const std::string& culture) const;

    // =========================================================================
    // Special Properties
    // =========================================================================

    [[nodiscard]] bool IsRepeatable() const { return m_repeatable; }
    void SetRepeatable(bool repeatable) { m_repeatable = repeatable; }
    [[nodiscard]] int GetMaxResearchCount() const { return m_maxResearchCount; }
    void SetMaxResearchCount(int count) { m_maxResearchCount = count; }

    [[nodiscard]] bool IsHidden() const { return m_hidden; }
    void SetHidden(bool hidden) { m_hidden = hidden; }

    [[nodiscard]] bool CanBeLost() const { return m_canBeLost; }
    void SetCanBeLost(bool canBeLost) { m_canBeLost = canBeLost; }
    [[nodiscard]] float GetLossChance() const { return m_lossChance; }
    void SetLossChance(float chance) { m_lossChance = chance; }

    [[nodiscard]] bool IsKeyTech() const { return m_isKeyTech; }
    void SetKeyTech(bool keyTech) { m_isKeyTech = keyTech; }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static TechNode FromJson(const nlohmann::json& j);

    /**
     * @brief Load from JSON file
     */
    [[nodiscard]] bool LoadFromFile(const std::string& filePath);

    /**
     * @brief Save to JSON file
     */
    [[nodiscard]] bool SaveToFile(const std::string& filePath) const;

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate the tech node configuration
     * @return Vector of error/warning messages (empty = valid)
     */
    [[nodiscard]] std::vector<std::string> Validate() const;

    // =========================================================================
    // Lifecycle Hooks (for TechManager to call)
    // =========================================================================

    /**
     * @brief Called when tech is first created/loaded
     */
    void OnCreate();

    /**
     * @brief Called each game tick while tech is active
     * @param deltaTime Time since last tick
     */
    void OnTick(float deltaTime);

    /**
     * @brief Called when tech is removed/lost
     */
    void OnDestroy();

private:
    // Identity
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::string m_icon;
    std::string m_flavorText;

    // Classification
    TechCategory m_category = TechCategory::Military;
    TechAge m_ageRequirement = TechAge::Stone;
    int m_tier = 1;
    std::vector<std::string> m_tags;

    // Prerequisites
    std::vector<std::string> m_prerequisites;
    std::vector<std::string> m_optionalPrereqs;     ///< Need N of these
    int m_optionalRequiredCount = 1;                ///< How many optional prereqs needed
    std::vector<std::string> m_exclusiveWith;       ///< Cannot have these techs

    // Cost
    TechCost m_cost;

    // Unlocks
    TechUnlocks m_unlocks;

    // Modifiers
    std::vector<TechModifier> m_modifiers;

    // Scripts
    TechScriptEvents m_scriptEvents;

    // UI
    TechPosition m_position;

    // Culture restrictions
    std::vector<std::string> m_availableToCultures;
    bool m_isUniversal = false;

    // Special properties
    bool m_repeatable = false;
    int m_maxResearchCount = 1;
    bool m_hidden = false;
    bool m_canBeLost = true;
    float m_lossChance = 0.3f;
    bool m_isKeyTech = false;
};

// ============================================================================
// Tech Node Builder
// ============================================================================

/**
 * @brief Fluent builder for creating TechNodes
 */
class TechNodeBuilder {
public:
    TechNodeBuilder(const std::string& id);

    TechNodeBuilder& Name(const std::string& name);
    TechNodeBuilder& Description(const std::string& desc);
    TechNodeBuilder& Icon(const std::string& icon);
    TechNodeBuilder& FlavorText(const std::string& text);

    TechNodeBuilder& Category(TechCategory cat);
    TechNodeBuilder& Age(TechAge age);
    TechNodeBuilder& Tier(int tier);
    TechNodeBuilder& Tag(const std::string& tag);

    TechNodeBuilder& Prerequisite(const std::string& techId);
    TechNodeBuilder& Prerequisites(const std::vector<std::string>& prereqs);
    TechNodeBuilder& ExclusiveWith(const std::string& techId);

    TechNodeBuilder& Cost(const std::string& resource, int amount);
    TechNodeBuilder& ResearchTime(float time);

    TechNodeBuilder& UnlockBuilding(const std::string& buildingId);
    TechNodeBuilder& UnlockUnit(const std::string& unitId);
    TechNodeBuilder& UnlockAbility(const std::string& abilityId);
    TechNodeBuilder& UnlockUpgrade(const std::string& upgradeId);
    TechNodeBuilder& UnlockSpell(const std::string& spellId);

    TechNodeBuilder& Modifier(const TechModifier& mod);
    TechNodeBuilder& FlatBonus(const std::string& stat, float value, TargetScope scope = TargetScope::Global());
    TechNodeBuilder& PercentBonus(const std::string& stat, float percent, TargetScope scope = TargetScope::Global());

    TechNodeBuilder& OnResearchComplete(const std::string& script);
    TechNodeBuilder& OnCreate(const std::string& script);
    TechNodeBuilder& OnTick(const std::string& script);
    TechNodeBuilder& OnDestroy(const std::string& script);

    TechNodeBuilder& Position(float x, float y);
    TechNodeBuilder& GridPosition(int row, int column);

    TechNodeBuilder& Culture(const std::string& culture);
    TechNodeBuilder& Universal();

    TechNodeBuilder& Repeatable(int maxCount = 0);
    TechNodeBuilder& Hidden();
    TechNodeBuilder& CannotBeLost();
    TechNodeBuilder& KeyTech();

    [[nodiscard]] TechNode Build() const;

private:
    TechNode m_node;
};

} // namespace TechTree
} // namespace Vehement
