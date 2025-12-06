#include "pathfinding/Graph.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>

namespace Nova {

/**
 * @brief Node utility functions for pathfinding operations
 *
 * This file contains utility functions for working with PathNode objects,
 * including distance calculations, neighbor analysis, and spatial queries.
 */

namespace NodeUtils {

/**
 * @brief Calculate the degree (number of connections) of a node
 */
[[nodiscard]] size_t GetDegree(const PathNode& node) {
    return node.neighbors.size();
}

/**
 * @brief Check if a node is a dead end (only one connection)
 */
[[nodiscard]] bool IsDeadEnd(const PathNode& node) {
    return node.neighbors.size() == 1;
}

/**
 * @brief Check if a node is isolated (no connections)
 */
[[nodiscard]] bool IsIsolated(const PathNode& node) {
    return node.neighbors.empty();
}

/**
 * @brief Check if two nodes are adjacent (directly connected)
 */
[[nodiscard]] bool AreAdjacent(const Graph& graph, int nodeA, int nodeB) {
    const PathNode* node = graph.GetNode(nodeA);
    return node && node->HasNeighbor(nodeB);
}

/**
 * @brief Get the common neighbors between two nodes
 */
[[nodiscard]] std::vector<int> GetCommonNeighbors(
    const Graph& graph,
    int nodeA,
    int nodeB) {

    std::vector<int> common;
    const PathNode* a = graph.GetNode(nodeA);
    const PathNode* b = graph.GetNode(nodeB);

    if (!a || !b) return common;

    // Use the smaller neighbor list for iteration
    const auto& smaller = a->neighbors.size() < b->neighbors.size() ? a->neighbors : b->neighbors;
    const auto& larger = a->neighbors.size() < b->neighbors.size() ? b->neighbors : a->neighbors;

    for (int id : smaller) {
        if (std::find(larger.begin(), larger.end(), id) != larger.end()) {
            common.push_back(id);
        }
    }

    return common;
}

/**
 * @brief Calculate the centroid of a set of nodes
 */
[[nodiscard]] glm::vec3 CalculateCentroid(const Graph& graph, const std::vector<int>& nodeIds) {
    if (nodeIds.empty()) {
        return glm::vec3(0.0f);
    }

    glm::vec3 sum(0.0f);
    int count = 0;

    for (int id : nodeIds) {
        const PathNode* node = graph.GetNode(id);
        if (node) {
            sum += node->position;
            ++count;
        }
    }

    return count > 0 ? sum / static_cast<float>(count) : glm::vec3(0.0f);
}

/**
 * @brief Find the node closest to a target position from a set of candidates
 */
[[nodiscard]] int FindClosestNode(
    const Graph& graph,
    const std::vector<int>& candidates,
    const glm::vec3& targetPosition) {

    int closest = -1;
    float minDistSq = std::numeric_limits<float>::max();

    for (int id : candidates) {
        const PathNode* node = graph.GetNode(id);
        if (node) {
            float distSq = glm::distance2(node->position, targetPosition);
            if (distSq < minDistSq) {
                minDistSq = distSq;
                closest = id;
            }
        }
    }

    return closest;
}

/**
 * @brief Calculate the bounding box of a set of nodes
 */
struct BoundingBox {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{std::numeric_limits<float>::lowest()};

    [[nodiscard]] glm::vec3 GetCenter() const {
        return (min + max) * 0.5f;
    }

    [[nodiscard]] glm::vec3 GetSize() const {
        return max - min;
    }

    [[nodiscard]] bool IsValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
};

[[nodiscard]] BoundingBox CalculateBoundingBox(const Graph& graph) {
    BoundingBox box;

    for (const auto& [id, node] : graph.GetNodes()) {
        box.min = glm::min(box.min, node.position);
        box.max = glm::max(box.max, node.position);
    }

    return box;
}

[[nodiscard]] BoundingBox CalculateBoundingBox(
    const Graph& graph,
    const std::vector<int>& nodeIds) {

    BoundingBox box;

    for (int id : nodeIds) {
        const PathNode* node = graph.GetNode(id);
        if (node) {
            box.min = glm::min(box.min, node->position);
            box.max = glm::max(box.max, node->position);
        }
    }

    return box;
}

/**
 * @brief Get all leaf nodes (nodes with only one connection)
 */
[[nodiscard]] std::vector<int> GetLeafNodes(const Graph& graph) {
    std::vector<int> leaves;

    for (const auto& [id, node] : graph.GetNodes()) {
        if (node.neighbors.size() == 1) {
            leaves.push_back(id);
        }
    }

    return leaves;
}

/**
 * @brief Get all hub nodes (nodes with many connections)
 */
[[nodiscard]] std::vector<int> GetHubNodes(const Graph& graph, size_t minConnections) {
    std::vector<int> hubs;

    for (const auto& [id, node] : graph.GetNodes()) {
        if (node.neighbors.size() >= minConnections) {
            hubs.push_back(id);
        }
    }

    return hubs;
}

/**
 * @brief Calculate local clustering coefficient for a node
 * Measures how interconnected a node's neighbors are
 */
[[nodiscard]] float CalculateClusteringCoefficient(const Graph& graph, int nodeId) {
    const PathNode* node = graph.GetNode(nodeId);
    if (!node || node->neighbors.size() < 2) {
        return 0.0f;
    }

    int connections = 0;
    int possibleConnections = 0;

    const auto& neighbors = node->neighbors;
    for (size_t i = 0; i < neighbors.size(); ++i) {
        for (size_t j = i + 1; j < neighbors.size(); ++j) {
            ++possibleConnections;
            if (graph.HasEdge(neighbors[i], neighbors[j]) ||
                graph.HasEdge(neighbors[j], neighbors[i])) {
                ++connections;
            }
        }
    }

    return possibleConnections > 0
        ? static_cast<float>(connections) / static_cast<float>(possibleConnections)
        : 0.0f;
}

} // namespace NodeUtils

} // namespace Nova
