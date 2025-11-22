#pragma once

/**
 * @file TechTree.hpp
 * @brief Technology tree structure and research management
 *
 * Manages:
 * - Multiple trees per culture/faction
 * - Branches and tiers
 * - Research queue
 * - Progress tracking
 * - Event callbacks (on_research_start, on_research_complete, on_unlock)
 */

#include "TechNode.hpp"
#include "TechRequirement.hpp"
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
class TechManager;

// ============================================================================
// Research Status
// ============================================================================

/**
 * @brief Status of a technology for a player
 */
enum class ResearchStatus : uint8_t {
    Locked,         ///< Prerequisites not met, cannot research
    Available,      ///< Can be researched (all requirements met)
    Queued,         ///< In research queue
    InProgress,     ///< Currently being researched
    Completed,      ///< Research finished, effects active
    Lost,           ///< Was completed but lost (can re-research)

    COUNT
};

[[nodiscard]] inline const char* ResearchStatusToString(ResearchStatus status) {
    switch (status) {
        case ResearchStatus::Locked:     return "locked";
        case ResearchStatus::Available:  return "available";
        case ResearchStatus::Queued:     return "queued";
        case ResearchStatus::InProgress: return "in_progress";
        case ResearchStatus::Completed:  return "completed";
        case ResearchStatus::Lost:       return "lost";
        default:                         return "unknown";
    }
}

// ============================================================================
// Research Progress
// ============================================================================

/**
 * @brief Tracks research progress for a single technology
 */
struct ResearchProgress {
    std::string techId;
    ResearchStatus status = ResearchStatus::Locked;
    float progress = 0.0f;              ///< Current progress (0.0-1.0)
    float totalTime = 0.0f;             ///< Total time required
    float elapsedTime = 0.0f;           ///< Time spent researching
    int64_t startTimestamp = 0;         ///< When research started
    int64_t completedTimestamp = 0;     ///< When research completed
    int timesResearched = 0;            ///< Times researched (for repeatable)
    int timesLost = 0;                  ///< Times lost

    [[nodiscard]] float GetRemainingTime() const {
        return std::max(0.0f, totalTime - elapsedTime);
    }

    [[nodiscard]] float GetProgressPercent() const {
        return totalTime > 0.0f ? std::min(1.0f, elapsedTime / totalTime) : 0.0f;
    }

    [[nodiscard]] bool IsComplete() const {
        return status == ResearchStatus::Completed;
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static ResearchProgress FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tree Connection
// ============================================================================

/**
 * @brief A connection between two tech nodes in the tree
 */
struct TreeConnection {
    std::string fromTech;           ///< Source tech ID
    std::string toTech;             ///< Target tech ID
    bool isRequired = true;         ///< If true, 'from' is required for 'to'
    std::string label;              ///< Optional label for the connection

    [[nodiscard]] nlohmann::json ToJson() const;
    static TreeConnection FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tree Branch
// ============================================================================

/**
 * @brief A branch/category within a tech tree
 */
struct TreeBranch {
    std::string id;
    std::string name;
    std::string description;
    std::string icon;
    TechCategory category = TechCategory::Military;
    std::vector<std::string> techIds;   ///< Techs in this branch
    int displayOrder = 0;               ///< Order in UI

    [[nodiscard]] nlohmann::json ToJson() const;
    static TreeBranch FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tech Tree Definition
// ============================================================================

/**
 * @brief Definition of a complete technology tree
 *
 * A TechTreeDef contains the structure and nodes of a tech tree,
 * separate from any player's research state.
 */
class TechTreeDef {
public:
    TechTreeDef() = default;
    explicit TechTreeDef(const std::string& id);

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

    // =========================================================================
    // Culture/Faction
    // =========================================================================

    [[nodiscard]] const std::string& GetCulture() const { return m_culture; }
    void SetCulture(const std::string& culture) { m_culture = culture; }

    [[nodiscard]] bool IsUniversal() const { return m_isUniversal; }
    void SetUniversal(bool universal) { m_isUniversal = universal; }

    // =========================================================================
    // Nodes
    // =========================================================================

    void AddNode(const TechNode& node);
    void RemoveNode(const std::string& techId);

    [[nodiscard]] const TechNode* GetNode(const std::string& techId) const;
    [[nodiscard]] TechNode* GetNodeMutable(const std::string& techId);

    [[nodiscard]] const std::unordered_map<std::string, TechNode>& GetAllNodes() const {
        return m_nodes;
    }

    [[nodiscard]] std::vector<const TechNode*> GetNodesInTier(int tier) const;
    [[nodiscard]] std::vector<const TechNode*> GetNodesInCategory(TechCategory category) const;
    [[nodiscard]] std::vector<const TechNode*> GetNodesForAge(TechAge age) const;
    [[nodiscard]] std::vector<const TechNode*> GetRootNodes() const;

    // =========================================================================
    // Connections
    // =========================================================================

    void AddConnection(const TreeConnection& connection);
    void RemoveConnection(const std::string& fromTech, const std::string& toTech);

    [[nodiscard]] const std::vector<TreeConnection>& GetConnections() const {
        return m_connections;
    }

    [[nodiscard]] std::vector<std::string> GetDependents(const std::string& techId) const;
    [[nodiscard]] std::vector<std::string> GetDependencies(const std::string& techId) const;

    // =========================================================================
    // Branches
    // =========================================================================

    void AddBranch(const TreeBranch& branch);
    [[nodiscard]] const TreeBranch* GetBranch(const std::string& branchId) const;
    [[nodiscard]] const std::vector<TreeBranch>& GetBranches() const { return m_branches; }

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate the tech tree for errors
     * @return Vector of error messages (empty = valid)
     */
    [[nodiscard]] std::vector<std::string> Validate() const;

    /**
     * @brief Check for circular dependencies
     */
    [[nodiscard]] bool HasCircularDependencies() const;

    /**
     * @brief Get all unreachable nodes (no path from root)
     */
    [[nodiscard]] std::vector<std::string> GetUnreachableNodes() const;

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechTreeDef FromJson(const nlohmann::json& j);

    [[nodiscard]] bool LoadFromFile(const std::string& filePath);
    [[nodiscard]] bool SaveToFile(const std::string& filePath) const;

private:
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::string m_icon;
    std::string m_culture;
    bool m_isUniversal = false;

    std::unordered_map<std::string, TechNode> m_nodes;
    std::vector<TreeConnection> m_connections;
    std::vector<TreeBranch> m_branches;

    // Cached dependency info
    mutable bool m_dependenciesDirty = true;
    mutable std::unordered_map<std::string, std::vector<std::string>> m_dependents;
    mutable std::unordered_map<std::string, std::vector<std::string>> m_dependencies;

    void RebuildDependencyCache() const;
};

// ============================================================================
// Research Event Types
// ============================================================================

/**
 * @brief Types of research events for callbacks
 */
enum class ResearchEventType : uint8_t {
    ResearchStarted,
    ResearchProgress,
    ResearchCompleted,
    ResearchCancelled,
    ResearchQueued,
    ResearchDequeued,
    TechUnlocked,
    TechLocked,
    TechLost,

    COUNT
};

/**
 * @brief Data for research events
 */
struct ResearchEvent {
    ResearchEventType type;
    std::string techId;
    std::string treeId;
    float progress = 0.0f;
    std::string message;
};

// ============================================================================
// Player Tech Tree
// ============================================================================

/**
 * @brief Player's research state for a tech tree
 *
 * Tracks which technologies a player has researched, current research,
 * and the research queue.
 */
class PlayerTechTree {
public:
    using ResearchCallback = std::function<void(const ResearchEvent& event)>;

    PlayerTechTree() = default;
    PlayerTechTree(const TechTreeDef* treeDef, const std::string& playerId);

    // =========================================================================
    // Initialization
    // =========================================================================

    void Initialize(const TechTreeDef* treeDef, const std::string& playerId);
    void Reset();

    [[nodiscard]] const TechTreeDef* GetTreeDef() const { return m_treeDef; }
    [[nodiscard]] const std::string& GetPlayerId() const { return m_playerId; }

    // =========================================================================
    // Research Status
    // =========================================================================

    [[nodiscard]] ResearchStatus GetTechStatus(const std::string& techId) const;
    [[nodiscard]] bool HasTech(const std::string& techId) const;
    [[nodiscard]] bool CanResearch(const std::string& techId, const IRequirementContext& context) const;

    [[nodiscard]] const ResearchProgress* GetProgress(const std::string& techId) const;
    [[nodiscard]] const std::unordered_set<std::string>& GetCompletedTechs() const {
        return m_completedTechs;
    }

    // =========================================================================
    // Current Research
    // =========================================================================

    [[nodiscard]] bool IsResearching() const { return !m_currentResearch.empty(); }
    [[nodiscard]] const std::string& GetCurrentResearch() const { return m_currentResearch; }
    [[nodiscard]] float GetCurrentProgress() const;
    [[nodiscard]] float GetCurrentRemainingTime() const;

    // =========================================================================
    // Research Actions
    // =========================================================================

    /**
     * @brief Start researching a technology
     * @return true if research started successfully
     */
    bool StartResearch(const std::string& techId, const IRequirementContext& context);

    /**
     * @brief Update research progress
     * @param deltaTime Time since last update in seconds
     * @param speedMultiplier Research speed multiplier (from bonuses)
     */
    void UpdateResearch(float deltaTime, float speedMultiplier = 1.0f);

    /**
     * @brief Complete current research immediately
     */
    void CompleteCurrentResearch();

    /**
     * @brief Cancel current research
     * @param refundPercent Percentage of resources to refund
     * @return Map of refunded resources
     */
    std::map<std::string, int> CancelResearch(float refundPercent = 0.5f);

    /**
     * @brief Grant a technology instantly (cheat/debug/scenario)
     */
    void GrantTech(const std::string& techId);

    /**
     * @brief Remove a researched technology
     */
    bool LoseTech(const std::string& techId);

    // =========================================================================
    // Research Queue
    // =========================================================================

    bool QueueResearch(const std::string& techId);
    bool DequeueResearch(const std::string& techId);
    void ClearQueue();

    [[nodiscard]] const std::vector<std::string>& GetQueue() const { return m_researchQueue; }
    [[nodiscard]] bool IsQueued(const std::string& techId) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnResearchEvent(ResearchCallback callback) {
        m_onResearchEvent = std::move(callback);
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] int GetTotalTechsResearched() const { return m_totalTechsResearched; }
    [[nodiscard]] int GetTotalTechsLost() const { return m_totalTechsLost; }
    [[nodiscard]] float GetTotalResearchTime() const { return m_totalResearchTime; }

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);

private:
    void OnResearchComplete(const std::string& techId);
    void ProcessQueue();
    void UpdateTechStatuses();
    void EmitEvent(ResearchEventType type, const std::string& techId,
                   float progress = 0.0f, const std::string& message = "");

    const TechTreeDef* m_treeDef = nullptr;
    std::string m_playerId;

    // Research state
    std::unordered_set<std::string> m_completedTechs;
    std::unordered_map<std::string, ResearchProgress> m_techProgress;
    std::string m_currentResearch;
    std::vector<std::string> m_researchQueue;

    // Statistics
    int m_totalTechsResearched = 0;
    int m_totalTechsLost = 0;
    float m_totalResearchTime = 0.0f;

    // Callback
    ResearchCallback m_onResearchEvent;
};

// ============================================================================
// Research Queue Manager
// ============================================================================

/**
 * @brief Manages research queue logic and auto-queuing
 */
class ResearchQueueManager {
public:
    /**
     * @brief Auto-queue prerequisites for a tech
     * @param targetTech Tech to research
     * @param tree Player's tech tree
     * @param context Requirement context
     * @return List of techs added to queue (in order)
     */
    [[nodiscard]] static std::vector<std::string> AutoQueuePrerequisites(
        const std::string& targetTech,
        PlayerTechTree& tree,
        const IRequirementContext& context);

    /**
     * @brief Get optimal research path to a tech
     * @param targetTech Target technology
     * @param tree Player's tech tree
     * @return Ordered list of techs to research
     */
    [[nodiscard]] static std::vector<std::string> GetResearchPath(
        const std::string& targetTech,
        const PlayerTechTree& tree);

    /**
     * @brief Estimate total time to research a tech (including prerequisites)
     */
    [[nodiscard]] static float EstimateTotalTime(
        const std::string& targetTech,
        const PlayerTechTree& tree,
        float speedMultiplier = 1.0f);
};

} // namespace TechTree
} // namespace Vehement
