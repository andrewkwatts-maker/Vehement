#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>
#include <span>
#include <concepts>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Graph;
struct PathNode;

/**
 * @brief Concept for types that can be used as graph node identifiers
 */
template<typename T>
concept NodeIdentifier = std::integral<T> || std::is_enum_v<T>;

/**
 * @brief Concept for heuristic functions
 */
template<typename F>
concept HeuristicFunction = std::invocable<F, const glm::vec3&, const glm::vec3&> &&
    std::convertible_to<std::invoke_result_t<F, const glm::vec3&, const glm::vec3&>, float>;

/**
 * @brief Concept for cost functions
 */
template<typename F>
concept CostFunction = std::invocable<F, int, int, const Graph&> &&
    std::convertible_to<std::invoke_result_t<F, int, int, const Graph&>, float>;

/**
 * @brief Graph node for pathfinding - contains only static node data
 *
 * Pathfinding state (gCost, hCost, parent) is stored separately in
 * PathfindingContext to allow thread-safe concurrent pathfinding operations.
 */
struct PathNode {
    int id = -1;
    glm::vec3 position{0.0f};
    std::vector<int> neighbors;

    // Optional node properties
    float traversalCost = 1.0f;  // Cost multiplier for traversing this node
    bool walkable = true;         // Whether this node can be traversed

    /**
     * @brief Check if this node has a specific neighbor
     */
    [[nodiscard]] bool HasNeighbor(int neighborId) const noexcept;

    /**
     * @brief Get neighbors as a span (non-owning view)
     */
    [[nodiscard]] std::span<const int> GetNeighbors() const noexcept {
        return neighbors;
    }
};

/**
 * @brief Graph edge with weight and optional metadata
 */
struct PathEdge {
    int from = -1;
    int to = -1;
    float weight = 1.0f;
    bool bidirectional = false;

    [[nodiscard]] bool IsValid() const noexcept {
        return from >= 0 && to >= 0 && weight >= 0.0f;
    }
};

/**
 * @brief Spatial hash cell for accelerated nearest-neighbor queries
 */
struct SpatialCell {
    std::vector<int> nodeIds;
};

/**
 * @brief Configuration for spatial hashing
 */
struct SpatialHashConfig {
    float cellSize = 10.0f;
    bool enabled = false;
};

/**
 * @brief Pathfinding graph data structure
 *
 * Supports weighted directed graphs with optional spatial indexing
 * for accelerated nearest-neighbor queries.
 */
class Graph {
public:
    Graph() = default;
    explicit Graph(const SpatialHashConfig& spatialConfig);

    // Prevent accidental copies (graphs can be large)
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;
    Graph(Graph&&) noexcept = default;
    Graph& operator=(Graph&&) noexcept = default;

    /**
     * @brief Add a node to the graph
     * @param position World position of the node
     * @return ID of the newly created node
     */
    [[nodiscard]] int AddNode(const glm::vec3& position);

    /**
     * @brief Add a node with custom traversal cost
     * @param position World position of the node
     * @param traversalCost Cost multiplier for traversing this node
     * @return ID of the newly created node
     */
    [[nodiscard]] int AddNode(const glm::vec3& position, float traversalCost);

    /**
     * @brief Remove a node and all its connections
     * @param nodeId ID of the node to remove
     */
    void RemoveNode(int nodeId);

    /**
     * @brief Add a directed edge between nodes
     * @param from Source node ID
     * @param to Target node ID
     * @param weight Edge weight (negative = auto-calculate from distance)
     */
    void AddEdge(int from, int to, float weight = -1.0f);

    /**
     * @brief Add a bidirectional edge between nodes
     * @param nodeA First node ID
     * @param nodeB Second node ID
     * @param weight Edge weight (negative = auto-calculate from distance)
     */
    void AddBidirectionalEdge(int nodeA, int nodeB, float weight = -1.0f);

    /**
     * @brief Remove a directed edge
     */
    void RemoveEdge(int from, int to);

    /**
     * @brief Remove bidirectional edges between nodes
     */
    void RemoveBidirectionalEdge(int nodeA, int nodeB);

    /**
     * @brief Get a node by ID
     * @return Pointer to node or nullptr if not found
     */
    [[nodiscard]] PathNode* GetNode(int id);
    [[nodiscard]] const PathNode* GetNode(int id) const;

    /**
     * @brief Get the nearest node to a position
     * @param position World position to search from
     * @return Node ID or -1 if graph is empty
     */
    [[nodiscard]] int GetNearestNode(const glm::vec3& position) const;

    /**
     * @brief Get the nearest walkable node to a position
     * @param position World position to search from
     * @return Node ID or -1 if no walkable node found
     */
    [[nodiscard]] int GetNearestWalkableNode(const glm::vec3& position) const;

    /**
     * @brief Get all nodes within a radius
     * @param position Center position
     * @param radius Search radius
     * @return Vector of node IDs within radius
     */
    [[nodiscard]] std::vector<int> GetNodesInRadius(const glm::vec3& position, float radius) const;

    /**
     * @brief Get all nodes (read-only access)
     */
    [[nodiscard]] const std::unordered_map<int, PathNode>& GetNodes() const noexcept {
        return m_nodes;
    }

    /**
     * @brief Get edge weight between nodes
     * @return Edge weight or infinity if no edge exists
     */
    [[nodiscard]] float GetEdgeWeight(int from, int to) const;

    /**
     * @brief Check if an edge exists
     */
    [[nodiscard]] bool HasEdge(int from, int to) const;

    /**
     * @brief Build a grid graph with 8-directional connectivity
     * @param width Grid width in cells
     * @param height Grid height in cells
     * @param spacing Distance between adjacent nodes
     * @param diagonals Include diagonal connections
     */
    void BuildGrid(int width, int height, float spacing = 1.0f, bool diagonals = true);

    /**
     * @brief Build a hexagonal grid graph
     * @param width Grid width
     * @param height Grid height
     * @param radius Hex cell radius
     */
    void BuildHexGrid(int width, int height, float radius = 1.0f);

    /**
     * @brief Build a random graph with proximity-based connections
     * @param nodeCount Number of nodes to create
     * @param connectionRadius Maximum distance for edge creation
     * @param areaSize Size of the square area to place nodes in
     */
    void BuildRandom(int nodeCount, float connectionRadius, float areaSize = 100.0f);

    /**
     * @brief Clear all nodes and edges
     */
    void Clear();

    /**
     * @brief Get the number of nodes
     */
    [[nodiscard]] size_t GetNodeCount() const noexcept { return m_nodes.size(); }

    /**
     * @brief Check if graph is empty
     */
    [[nodiscard]] bool IsEmpty() const noexcept { return m_nodes.empty(); }

    /**
     * @brief Set a node's walkable state
     */
    void SetNodeWalkable(int nodeId, bool walkable);

    /**
     * @brief Rebuild spatial index (call after bulk node additions)
     */
    void RebuildSpatialIndex();

    /**
     * @brief Enable or disable spatial indexing
     */
    void SetSpatialIndexEnabled(bool enabled);

private:
    std::unordered_map<int, PathNode> m_nodes;
    std::unordered_map<int, std::unordered_map<int, float>> m_weights;
    int m_nextId = 0;

    // Spatial hashing for accelerated queries
    SpatialHashConfig m_spatialConfig;
    std::unordered_map<int64_t, SpatialCell> m_spatialHash;

    [[nodiscard]] int64_t GetSpatialKey(const glm::vec3& position) const;
    void AddToSpatialHash(int nodeId, const glm::vec3& position);
    void RemoveFromSpatialHash(int nodeId, const glm::vec3& position);
};

/**
 * @brief Context for a single pathfinding operation
 *
 * Stores per-node pathfinding state separately from the graph,
 * allowing multiple concurrent pathfinding operations on the same graph.
 */
class PathfindingContext {
public:
    explicit PathfindingContext(const Graph& graph);

    /**
     * @brief Reset all pathfinding state
     */
    void Reset();

    /**
     * @brief Get/set node costs and state
     */
    [[nodiscard]] float GetGCost(int nodeId) const;
    [[nodiscard]] float GetHCost(int nodeId) const;
    [[nodiscard]] float GetFCost(int nodeId) const;
    [[nodiscard]] int GetParent(int nodeId) const;
    [[nodiscard]] bool IsVisited(int nodeId) const;
    [[nodiscard]] bool IsInOpenSet(int nodeId) const;

    void SetGCost(int nodeId, float cost);
    void SetHCost(int nodeId, float cost);
    void SetParent(int nodeId, int parentId);
    void SetVisited(int nodeId, bool visited);
    void SetInOpenSet(int nodeId, bool inOpenSet);

private:
    struct NodeState {
        float gCost = std::numeric_limits<float>::infinity();
        float hCost = 0.0f;
        int parent = -1;
        bool visited = false;
        bool inOpenSet = false;
    };

    std::unordered_map<int, NodeState> m_states;
    const Graph* m_graph;
};

} // namespace Nova
