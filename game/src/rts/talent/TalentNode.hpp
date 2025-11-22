#pragma once

/**
 * @file TalentNode.hpp
 * @brief Individual talent node definitions for talent trees
 */

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Talent {

// ============================================================================
// Talent Categories
// ============================================================================

enum class TalentCategory : uint8_t {
    Military = 0,
    Economy,
    Magic,
    Technology,
    Special,

    COUNT
};

[[nodiscard]] inline const char* TalentCategoryToString(TalentCategory c) {
    switch (c) {
        case TalentCategory::Military: return "Military";
        case TalentCategory::Economy: return "Economy";
        case TalentCategory::Magic: return "Magic";
        case TalentCategory::Technology: return "Technology";
        case TalentCategory::Special: return "Special";
        default: return "Unknown";
    }
}

// ============================================================================
// Unlock Effect
// ============================================================================

struct TalentUnlock {
    std::string type;               ///< "unit", "building", "upgrade", "spell", "ability"
    std::string targetId;           ///< ID of unlocked content
    std::string description;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TalentUnlock FromJson(const nlohmann::json& j);
};

// ============================================================================
// Stat Modifier
// ============================================================================

struct TalentModifier {
    std::string stat;               ///< Stat to modify
    float value = 0.0f;             ///< Modifier value
    bool isPercentage = true;       ///< true = percentage, false = flat
    std::string targetType;         ///< "all", "infantry", "cavalry", etc.

    [[nodiscard]] nlohmann::json ToJson() const;
    static TalentModifier FromJson(const nlohmann::json& j);
};

// ============================================================================
// Talent Node
// ============================================================================

struct TalentNode {
    // Identity
    std::string id;
    std::string name;
    std::string description;
    std::string iconPath;

    // Classification
    TalentCategory category = TalentCategory::Military;
    int tier = 1;                   ///< Tier within tree (1-5 typically)

    // Cost
    int pointCost = 1;              ///< Talent points required
    int requiredAge = 0;            ///< Minimum age to unlock

    // Prerequisites
    std::vector<std::string> prerequisites;     ///< Required talent IDs
    int prerequisiteCount = 0;                  ///< How many prereqs needed (0 = all)

    // Unlocks
    std::vector<TalentUnlock> unlocks;          ///< Things this talent unlocks
    std::vector<TalentModifier> modifiers;      ///< Stat modifiers

    // Visual position in tree
    int positionX = 0;              ///< Column in tree view
    int positionY = 0;              ///< Row in tree view
    std::string connectedFrom;      ///< Parent node for visual connection

    // Synergies
    std::vector<std::string> synergyWith;       ///< Enhanced if these also owned
    float synergyBonus = 0.1f;                  ///< Bonus per synergy (10%)

    // Flags
    bool isKeystone = false;        ///< Major talent at branch end
    bool isPassive = true;          ///< Always active vs activated
    int maxRank = 1;                ///< For multi-rank talents

    // Balance
    float powerRating = 1.0f;

    // Tags
    std::vector<std::string> tags;

    // =========================================================================
    // Methods
    // =========================================================================

    [[nodiscard]] bool CanUnlock(const std::vector<std::string>& ownedTalents, int currentAge, int availablePoints) const;
    [[nodiscard]] float CalculateSynergyBonus(const std::vector<std::string>& ownedTalents) const;
    [[nodiscard]] bool Validate() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TalentNode FromJson(const nlohmann::json& j);

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Talent Node Registry
// ============================================================================

class TalentNodeRegistry {
public:
    [[nodiscard]] static TalentNodeRegistry& Instance();

    bool Initialize();
    void Shutdown();

    bool RegisterNode(const TalentNode& node);
    [[nodiscard]] const TalentNode* GetNode(const std::string& id) const;
    [[nodiscard]] std::vector<const TalentNode*> GetAllNodes() const;
    [[nodiscard]] std::vector<const TalentNode*> GetByCategory(TalentCategory cat) const;
    [[nodiscard]] std::vector<const TalentNode*> GetByTier(int tier) const;

    int LoadFromDirectory(const std::string& directory);

private:
    TalentNodeRegistry() = default;
    bool m_initialized = false;
    std::map<std::string, TalentNode> m_nodes;
    void InitializeBuiltInNodes();
};

} // namespace Talent
} // namespace RTS
} // namespace Vehement
