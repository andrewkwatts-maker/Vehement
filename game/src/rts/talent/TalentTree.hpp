#pragma once

/**
 * @file TalentTree.hpp
 * @brief Visual talent tree management for races
 */

#include "TalentNode.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Talent {

// ============================================================================
// Talent Branch
// ============================================================================

struct TalentBranch {
    std::string id;
    std::string name;
    std::string description;
    TalentCategory category = TalentCategory::Military;
    std::string iconPath;
    std::string colorHex;           ///< Branch color in tree view

    std::vector<std::string> nodeIds;   ///< Nodes in this branch (ordered)
    std::string keystoneId;             ///< Final keystone talent

    [[nodiscard]] nlohmann::json ToJson() const;
    static TalentBranch FromJson(const nlohmann::json& j);
};

// ============================================================================
// Age Gate
// ============================================================================

struct AgeGate {
    int age = 1;
    std::vector<std::string> unlockedNodes;     ///< Nodes available at this age
    int bonusTalentPoints = 0;                  ///< Extra points at this age

    [[nodiscard]] nlohmann::json ToJson() const;
    static AgeGate FromJson(const nlohmann::json& j);
};

// ============================================================================
// Player Talent Progress
// ============================================================================

struct TalentProgress {
    std::set<std::string> unlockedTalents;
    std::map<std::string, int> talentRanks;     ///< For multi-rank talents
    int totalPointsSpent = 0;
    int availablePoints = 0;

    [[nodiscard]] bool HasTalent(const std::string& id) const {
        return unlockedTalents.find(id) != unlockedTalents.end();
    }

    [[nodiscard]] int GetTalentRank(const std::string& id) const {
        auto it = talentRanks.find(id);
        return (it != talentRanks.end()) ? it->second : 0;
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static TalentProgress FromJson(const nlohmann::json& j);
};

// ============================================================================
// Talent Tree Definition
// ============================================================================

struct TalentTreeDefinition {
    // Identity
    std::string id;
    std::string name;
    std::string description;
    std::string raceId;             ///< Associated race (empty = universal)

    // Structure
    std::vector<TalentBranch> branches;
    std::map<std::string, TalentNode> nodes;    ///< All nodes
    std::vector<AgeGate> ageGates;

    // Configuration
    int totalTalentPoints = 30;
    int pointsPerAge = 5;
    int startingPoints = 0;

    // Visual
    int treeWidth = 5;              ///< Columns
    int treeHeight = 7;             ///< Rows (one per age)

    [[nodiscard]] nlohmann::json ToJson() const;
    static TalentTreeDefinition FromJson(const nlohmann::json& j);

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Talent Tree Manager
// ============================================================================

class TalentTree {
public:
    using TalentUnlockCallback = std::function<void(const std::string& talentId, const TalentNode& node)>;
    using TalentResetCallback = std::function<void()>;

    TalentTree();
    ~TalentTree();

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize(const TalentTreeDefinition& definition);
    bool InitializeForRace(const std::string& raceId);
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Point Management
    // =========================================================================

    void SetTotalPoints(int points);
    void AddPoints(int points);
    [[nodiscard]] int GetAvailablePoints() const { return m_progress.availablePoints; }
    [[nodiscard]] int GetTotalPointsSpent() const { return m_progress.totalPointsSpent; }
    [[nodiscard]] int GetTotalPoints() const { return m_definition.totalTalentPoints; }

    // =========================================================================
    // Talent Operations
    // =========================================================================

    [[nodiscard]] bool CanUnlockTalent(const std::string& talentId) const;
    bool UnlockTalent(const std::string& talentId);
    bool RefundTalent(const std::string& talentId);
    void ResetAllTalents();

    [[nodiscard]] bool HasTalent(const std::string& talentId) const;
    [[nodiscard]] int GetTalentRank(const std::string& talentId) const;
    [[nodiscard]] std::vector<std::string> GetUnlockedTalents() const;

    // =========================================================================
    // Node Access
    // =========================================================================

    [[nodiscard]] const TalentNode* GetNode(const std::string& nodeId) const;
    [[nodiscard]] std::vector<const TalentNode*> GetAllNodes() const;
    [[nodiscard]] std::vector<const TalentNode*> GetAvailableNodes() const;
    [[nodiscard]] std::vector<const TalentNode*> GetNodesByCategory(TalentCategory category) const;
    [[nodiscard]] std::vector<const TalentNode*> GetNodesByTier(int tier) const;

    // =========================================================================
    // Branch Access
    // =========================================================================

    [[nodiscard]] const TalentBranch* GetBranch(const std::string& branchId) const;
    [[nodiscard]] std::vector<const TalentBranch*> GetAllBranches() const;
    [[nodiscard]] float GetBranchProgress(const std::string& branchId) const;

    // =========================================================================
    // Modifier Calculations
    // =========================================================================

    [[nodiscard]] float GetStatModifier(const std::string& stat, const std::string& targetType = "all") const;
    [[nodiscard]] std::map<std::string, float> GetAllModifiers() const;
    [[nodiscard]] std::vector<std::string> GetUnlockedContent(const std::string& type) const;

    // =========================================================================
    // Age Integration
    // =========================================================================

    void OnAgeAdvance(int newAge);
    [[nodiscard]] int GetCurrentAge() const { return m_currentAge; }
    [[nodiscard]] std::vector<std::string> GetNodesAvailableAtAge(int age) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnTalentUnlock(TalentUnlockCallback callback) { m_onTalentUnlock = std::move(callback); }
    void SetOnTalentReset(TalentResetCallback callback) { m_onTalentReset = std::move(callback); }

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);

    bool SaveProgress(const std::string& filepath) const;
    bool LoadProgress(const std::string& filepath);

private:
    bool m_initialized = false;
    TalentTreeDefinition m_definition;
    TalentProgress m_progress;
    int m_currentAge = 0;

    TalentUnlockCallback m_onTalentUnlock;
    TalentResetCallback m_onTalentReset;

    void RecalculateModifiers();
    std::map<std::string, float> m_cachedModifiers;
};

// ============================================================================
// Talent Tree Registry
// ============================================================================

class TalentTreeRegistry {
public:
    [[nodiscard]] static TalentTreeRegistry& Instance();

    bool Initialize();
    void Shutdown();

    bool RegisterTree(const TalentTreeDefinition& tree);
    [[nodiscard]] const TalentTreeDefinition* GetTree(const std::string& id) const;
    [[nodiscard]] const TalentTreeDefinition* GetTreeForRace(const std::string& raceId) const;
    [[nodiscard]] std::vector<const TalentTreeDefinition*> GetAllTrees() const;

    int LoadFromDirectory(const std::string& directory);

private:
    TalentTreeRegistry() = default;
    bool m_initialized = false;
    std::map<std::string, TalentTreeDefinition> m_trees;
    void InitializeBuiltInTrees();
};

} // namespace Talent
} // namespace RTS
} // namespace Vehement
