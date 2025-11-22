#pragma once

#include "Resource.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace Vehement {
namespace RTS {

// Forward declarations
class Gatherer;

// ============================================================================
// Resource Node Types
// ============================================================================

/**
 * @brief Types of resource nodes found in the world
 */
enum class NodeType : uint8_t {
    Tree,           ///< Forest trees - yields Wood
    RockDeposit,    ///< Mountain rocks - yields Stone
    ScrapPile,      ///< Ruins/wreckage - yields Metal
    AbandonedCache, ///< Buildings/containers - yields various
    CropField,      ///< Farm fields - yields Food
    FuelTank,       ///< Abandoned vehicles/stations - yields Fuel
    MedicalSupply,  ///< Hospitals/pharmacies - yields Medicine
    AmmoCache,      ///< Military sites - yields Ammunition

    COUNT
};

/**
 * @brief Get the primary resource type for a node type
 */
[[nodiscard]] ResourceType GetNodeResourceType(NodeType type);

/**
 * @brief Get display name for a node type
 */
[[nodiscard]] const char* GetNodeTypeName(NodeType type);

// ============================================================================
// Resource Node
// ============================================================================

/**
 * @brief A resource node in the game world that can be harvested
 *
 * Resource nodes spawn in the world and can be gathered by workers.
 * They deplete over time and may respawn after a delay.
 */
struct ResourceNode {
    /// Unique identifier for this node
    uint32_t id = 0;

    /// World position of the node
    glm::vec2 position{0.0f};

    /// Type of node (determines resource and visuals)
    NodeType nodeType = NodeType::Tree;

    /// Resource type this node yields
    ResourceType resourceType = ResourceType::Wood;

    /// Remaining resources in this node
    int remaining = 100;

    /// Maximum resources this node can hold
    int maxAmount = 100;

    /// How many resources per second a single gatherer can extract
    float gatherRate = 2.0f;

    /// Collision radius for interaction
    float radius = 1.5f;

    /// Time until this node respawns after depletion (seconds)
    float respawnTime = 120.0f;

    /// Current respawn timer (counts down when depleted)
    float respawnTimer = 0.0f;

    /// Whether the node is currently active (not depleted)
    bool active = true;

    /// How many gatherers are currently assigned to this node
    int assignedGatherers = 0;

    /// Maximum gatherers that can work on this node simultaneously
    int maxGatherers = 3;

    /**
     * @brief Check if node is depleted
     */
    [[nodiscard]] bool IsDepleted() const { return remaining <= 0; }

    /**
     * @brief Check if more gatherers can be assigned
     */
    [[nodiscard]] bool CanAssignGatherer() const {
        return active && assignedGatherers < maxGatherers;
    }

    /**
     * @brief Get percentage of resources remaining
     */
    [[nodiscard]] float GetPercentageRemaining() const {
        return maxAmount > 0 ? static_cast<float>(remaining) / maxAmount : 0.0f;
    }

    /**
     * @brief Extract resources from this node
     * @param amount Amount to extract
     * @return Actual amount extracted (may be less if not enough)
     */
    int Extract(int amount);

    /**
     * @brief Update respawn timer
     * @param deltaTime Time since last update
     */
    void UpdateRespawn(float deltaTime);

    /**
     * @brief Respawn the node to full capacity
     */
    void Respawn();

    /**
     * @brief Create a node with default settings for a type
     */
    static ResourceNode CreateDefault(NodeType type, const glm::vec2& pos, uint32_t nodeId);
};

// ============================================================================
// Gatherer
// ============================================================================

/**
 * @brief State of a gatherer entity
 */
enum class GathererState : uint8_t {
    Idle,               ///< Not assigned to any task
    MovingToNode,       ///< Traveling to resource node
    Gathering,          ///< Actively gathering resources
    MovingToStorage,    ///< Returning with resources
    Depositing,         ///< Depositing resources at storage
    Waiting             ///< Waiting (node full of gatherers, etc.)
};

/**
 * @brief A worker entity that gathers resources
 */
struct Gatherer {
    /// Unique identifier
    uint32_t id = 0;

    /// Current world position
    glm::vec2 position{0.0f};

    /// Current state
    GathererState state = GathererState::Idle;

    /// Target node ID (-1 if none)
    int32_t targetNodeId = -1;

    /// Storage location to return to
    glm::vec2 storagePosition{0.0f};

    /// Resource type currently carrying
    ResourceType carryingType = ResourceType::Wood;

    /// Amount of resources currently carrying
    int carryingAmount = 0;

    /// Maximum carry capacity
    int carryCapacity = 20;

    /// Movement speed in units per second
    float moveSpeed = 4.0f;

    /// Gathering efficiency multiplier
    float gatherEfficiency = 1.0f;

    /// Time spent in current state
    float stateTimer = 0.0f;

    /**
     * @brief Check if gatherer is carrying resources
     */
    [[nodiscard]] bool IsCarrying() const { return carryingAmount > 0; }

    /**
     * @brief Check if gatherer is at full capacity
     */
    [[nodiscard]] bool IsFullyLoaded() const { return carryingAmount >= carryCapacity; }

    /**
     * @brief Check if gatherer is available for assignment
     */
    [[nodiscard]] bool IsAvailable() const {
        return state == GathererState::Idle;
    }

    /**
     * @brief Get free carry space
     */
    [[nodiscard]] int GetFreeSpace() const {
        return carryCapacity - carryingAmount;
    }

    /**
     * @brief Add resources to carry
     * @return Amount actually added
     */
    int AddToCarry(ResourceType type, int amount);

    /**
     * @brief Deposit all carried resources
     * @return Amount deposited
     */
    int DepositAll();

    /**
     * @brief Reset gatherer to idle state
     */
    void Reset();
};

// ============================================================================
// Gathering System
// ============================================================================

/**
 * @brief Configuration for the gathering system
 */
struct GatheringConfig {
    float baseGatherRate = 2.0f;            ///< Base resources per second
    float gathererMoveSpeed = 4.0f;         ///< Base gatherer speed
    int defaultCarryCapacity = 20;          ///< Default carry capacity
    float nodeRespawnTime = 120.0f;         ///< Default respawn time
    float depositTime = 0.5f;               ///< Time to deposit at storage
    float nodeDetectionRadius = 50.0f;      ///< Range to find nearby nodes
};

/**
 * @brief Manages resource gathering in the game world
 *
 * This system handles:
 * - Spawning and managing resource nodes
 * - Assigning gatherers to nodes
 * - Transporting resources to storage
 * - Node depletion and respawning
 */
class GatheringSystem {
public:
    GatheringSystem();
    ~GatheringSystem();

    // Delete copy, allow move
    GatheringSystem(const GatheringSystem&) = delete;
    GatheringSystem& operator=(const GatheringSystem&) = delete;
    GatheringSystem(GatheringSystem&&) noexcept = default;
    GatheringSystem& operator=(GatheringSystem&&) noexcept = default;

    /**
     * @brief Initialize the gathering system
     * @param config Configuration settings
     */
    void Initialize(const GatheringConfig& config = GatheringConfig{});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update all gathering operations
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // -------------------------------------------------------------------------
    // Node Management
    // -------------------------------------------------------------------------

    /**
     * @brief Spawn a resource node
     * @param type Type of node
     * @param position World position
     * @param amount Initial resource amount
     * @return Pointer to created node, or nullptr on failure
     */
    ResourceNode* SpawnNode(NodeType type, const glm::vec2& position, int amount = -1);

    /**
     * @brief Remove a resource node
     * @param nodeId ID of node to remove
     */
    void RemoveNode(uint32_t nodeId);

    /**
     * @brief Get a node by ID
     */
    [[nodiscard]] ResourceNode* GetNode(uint32_t nodeId);
    [[nodiscard]] const ResourceNode* GetNode(uint32_t nodeId) const;

    /**
     * @brief Get all resource nodes
     */
    [[nodiscard]] const std::vector<ResourceNode>& GetNodes() const { return m_nodes; }

    /**
     * @brief Find nodes near a position
     * @param position Center of search
     * @param radius Search radius
     * @param type Optional filter by node type (-1 for any)
     */
    [[nodiscard]] std::vector<ResourceNode*> FindNodesNear(
        const glm::vec2& position,
        float radius,
        int type = -1
    );

    /**
     * @brief Find the nearest node of a specific type
     */
    [[nodiscard]] ResourceNode* FindNearestNode(
        const glm::vec2& position,
        ResourceType resourceType
    );

    /**
     * @brief Spawn nodes randomly in an area
     * @param center Center of spawn area
     * @param radius Spawn radius
     * @param count Number of nodes to spawn
     * @param type Type of nodes (or -1 for random)
     */
    void SpawnNodesInArea(
        const glm::vec2& center,
        float radius,
        int count,
        int type = -1
    );

    // -------------------------------------------------------------------------
    // Gatherer Management
    // -------------------------------------------------------------------------

    /**
     * @brief Create a new gatherer
     * @param position Starting position
     * @return Pointer to created gatherer
     */
    Gatherer* CreateGatherer(const glm::vec2& position);

    /**
     * @brief Remove a gatherer
     * @param gathererId ID of gatherer to remove
     */
    void RemoveGatherer(uint32_t gathererId);

    /**
     * @brief Get a gatherer by ID
     */
    [[nodiscard]] Gatherer* GetGatherer(uint32_t gathererId);
    [[nodiscard]] const Gatherer* GetGatherer(uint32_t gathererId) const;

    /**
     * @brief Get all gatherers
     */
    [[nodiscard]] const std::vector<Gatherer>& GetGatherers() const { return m_gatherers; }

    /**
     * @brief Assign a gatherer to a node
     * @param gathererId ID of gatherer
     * @param nodeId ID of target node
     * @return true if assignment succeeded
     */
    bool AssignGathererToNode(uint32_t gathererId, uint32_t nodeId);

    /**
     * @brief Unassign a gatherer from their current node
     * @param gathererId ID of gatherer
     */
    void UnassignGatherer(uint32_t gathererId);

    /**
     * @brief Auto-assign idle gatherers to nearby nodes
     */
    void AutoAssignIdleGatherers();

    /**
     * @brief Get number of idle gatherers
     */
    [[nodiscard]] int GetIdleGathererCount() const;

    // -------------------------------------------------------------------------
    // Storage
    // -------------------------------------------------------------------------

    /**
     * @brief Set the main storage location
     */
    void SetStorageLocation(const glm::vec2& position);

    /**
     * @brief Get the storage location
     */
    [[nodiscard]] const glm::vec2& GetStorageLocation() const { return m_storageLocation; }

    /**
     * @brief Set the resource stock to deposit into
     */
    void SetResourceStock(ResourceStock* stock) { m_resourceStock = stock; }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Apply scarcity settings
     */
    void ApplyScarcitySettings(const ScarcitySettings& settings);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const GatheringConfig& GetConfig() const { return m_config; }

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    /**
     * @brief Get total resources gathered (lifetime)
     */
    [[nodiscard]] int GetTotalGathered(ResourceType type) const;

    /**
     * @brief Get current gathering rate per second
     */
    [[nodiscard]] float GetCurrentGatherRate(ResourceType type) const;

    /**
     * @brief Get number of active nodes
     */
    [[nodiscard]] int GetActiveNodeCount() const;

    /**
     * @brief Get number of depleted nodes
     */
    [[nodiscard]] int GetDepletedNodeCount() const;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using NodeDepletedCallback = std::function<void(const ResourceNode&)>;
    using NodeRespawnedCallback = std::function<void(const ResourceNode&)>;
    using ResourceGatheredCallback = std::function<void(ResourceType, int amount)>;

    void SetOnNodeDepleted(NodeDepletedCallback callback) { m_onNodeDepleted = std::move(callback); }
    void SetOnNodeRespawned(NodeRespawnedCallback callback) { m_onNodeRespawned = std::move(callback); }
    void SetOnResourceGathered(ResourceGatheredCallback callback) { m_onResourceGathered = std::move(callback); }

private:
    void UpdateNodes(float deltaTime);
    void UpdateGatherers(float deltaTime);
    void UpdateGatherer(Gatherer& gatherer, float deltaTime);

    void ProcessGatheringState(Gatherer& gatherer, float deltaTime);
    void ProcessMovementState(Gatherer& gatherer, float deltaTime);
    void ProcessDepositState(Gatherer& gatherer, float deltaTime);

    void MoveTowards(Gatherer& gatherer, const glm::vec2& target, float deltaTime);
    bool IsAtPosition(const Gatherer& gatherer, const glm::vec2& target, float threshold = 0.5f) const;

    void DepositResources(Gatherer& gatherer);

    uint32_t GenerateNodeId();
    uint32_t GenerateGathererId();

    GatheringConfig m_config;
    ScarcitySettings m_scarcitySettings;

    std::vector<ResourceNode> m_nodes;
    std::vector<Gatherer> m_gatherers;

    glm::vec2 m_storageLocation{0.0f};
    ResourceStock* m_resourceStock = nullptr;

    // Statistics
    std::unordered_map<ResourceType, int> m_totalGathered;
    std::unordered_map<ResourceType, float> m_recentGatherRates;

    // ID generators
    uint32_t m_nextNodeId = 1;
    uint32_t m_nextGathererId = 1;

    // Callbacks
    NodeDepletedCallback m_onNodeDepleted;
    NodeRespawnedCallback m_onNodeRespawned;
    ResourceGatheredCallback m_onResourceGathered;

    bool m_initialized = false;
};

} // namespace RTS
} // namespace Vehement
