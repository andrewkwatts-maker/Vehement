#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Graph node for pathfinding
 */
struct PathNode {
    int id = -1;
    glm::vec3 position{0};
    std::vector<int> neighbors;

    // Pathfinding data
    float gCost = 0;      // Cost from start
    float hCost = 0;      // Heuristic cost to goal
    float fCost() const { return gCost + hCost; }
    int parent = -1;
    bool visited = false;
    bool inOpenSet = false;
};

/**
 * @brief Graph edge with weight
 */
struct PathEdge {
    int from;
    int to;
    float weight;
};

/**
 * @brief Pathfinding graph data structure
 */
class Graph {
public:
    Graph() = default;

    /**
     * @brief Add a node to the graph
     */
    int AddNode(const glm::vec3& position);

    /**
     * @brief Remove a node from the graph
     */
    void RemoveNode(int nodeId);

    /**
     * @brief Add a connection between nodes
     */
    void AddEdge(int from, int to, float weight = -1.0f);

    /**
     * @brief Remove a connection
     */
    void RemoveEdge(int from, int to);

    /**
     * @brief Get a node by ID
     */
    PathNode* GetNode(int id);
    const PathNode* GetNode(int id) const;

    /**
     * @brief Get the nearest node to a position
     */
    int GetNearestNode(const glm::vec3& position) const;

    /**
     * @brief Get all nodes
     */
    const std::unordered_map<int, PathNode>& GetNodes() const { return m_nodes; }

    /**
     * @brief Get edge weight between nodes
     */
    float GetEdgeWeight(int from, int to) const;

    /**
     * @brief Reset pathfinding state
     */
    void ResetPathfindingState();

    /**
     * @brief Build a grid graph
     */
    void BuildGrid(int width, int height, float spacing = 1.0f);

    /**
     * @brief Build a random graph
     */
    void BuildRandom(int nodeCount, float connectionRadius, float areaSize = 100.0f);

    /**
     * @brief Clear all nodes and edges
     */
    void Clear();

    /**
     * @brief Get node count
     */
    size_t GetNodeCount() const { return m_nodes.size(); }

private:
    std::unordered_map<int, PathNode> m_nodes;
    std::unordered_map<int, std::unordered_map<int, float>> m_weights;
    int m_nextId = 0;
};

} // namespace Nova
